#ifndef GUITAR_ENGINE_SPECTRUM_ANALYZER_H
#define GUITAR_ENGINE_SPECTRUM_ANALYZER_H

#include <pffft/pffft.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

/**
 * Real-time spectrum analyzer for guitar audio visualization.
 *
 * This is a PASSIVE analysis tap -- it does NOT modify audio and is NOT an
 * Effect subclass. It accepts raw audio samples from the audio callback,
 * performs FFT analysis, and exposes 64 logarithmically-spaced frequency
 * bins (20Hz-20kHz) for UI visualization.
 *
 * Architecture:
 *   - Audio thread calls pushSamples() every callback (real-time safe)
 *   - UI thread calls getSpectrum() at ~30fps (lock-free read)
 *   - Double-buffered output with atomic index swap (zero contention)
 *
 * FFT parameters:
 *   - FFT size: 2048 samples (~23Hz resolution at 48kHz)
 *   - Hop size: 2048 samples (no overlap, one FFT per buffer fill)
 *   - Window: Hann (pre-computed)
 *   - Library: PFFFT (NEON on ARM, SSE on x86)
 *
 * Output:
 *   - 64 bins, logarithmically spaced from 20Hz to 20kHz
 *   - Each bin = max magnitude in its frequency range, in dB
 *   - Range: [-90, 0] dB (clamped)
 *   - Exponential smoothing (alpha = 0.3) for visual stability
 *
 * Thread safety:
 *   - pushSamples(): audio thread only. No allocs, no locks, no syscalls.
 *   - getSpectrum(): UI thread only. Lock-free read of active double buffer.
 *   - setSampleRate(): setup thread only. Allocates all resources.
 *
 * Lifecycle:
 *   1. Construct (minimal init, no allocations)
 *   2. setSampleRate() -- allocates PFFFT setup, buffers, precomputes window
 *   3. pushSamples() -- called every audio callback
 *   4. getSpectrum() -- called ~30fps from UI polling
 *   5. Destructor frees all resources
 */
class SpectrumAnalyzer {
public:
    // =========================================================================
    // Constants
    // =========================================================================

    /** FFT size in samples. 2048 gives ~23Hz resolution at 48kHz. */
    static constexpr int kFftSize = 2048;

    /** Hop size in samples. 2048 = 100% of FFT size = no overlap.
     *  Reduced from 512 (75% overlap) to minimize CPU cost in the audio
     *  callback. At 48kHz this triggers an FFT every ~42ms, which is still
     *  faster than the UI's 30 FPS polling rate. The slight reduction in
     *  temporal smoothness is imperceptible in a bar-chart visualization. */
    static constexpr int kHopSize = 2048;

    /** Number of output frequency bins for UI visualization. */
    static constexpr int kNumBins = 64;

    /** Lower frequency bound of the analysis range (Hz). */
    static constexpr float kMinFreqHz = 20.0f;

    /** Upper frequency bound of the analysis range (Hz). */
    static constexpr float kMaxFreqHz = 20000.0f;

    /**
     * Logarithmic base for bin spacing.
     * Since maxFreq = minFreq * base, we have base = maxFreq / minFreq = 1000.
     * Bin i spans: minFreq * pow(base, i/63) to minFreq * pow(base, (i+1)/63).
     * Using 63 (not 64) so that bin 63's lower edge lands exactly at 20kHz,
     * and we get 65 edges total (0..64) for 64 bins.
     */
    static constexpr float kFreqRatio = kMaxFreqHz / kMinFreqHz; // 1000.0

    /** Smoothing factor for exponential moving average on bins.
     *  Higher alpha = faster response, lower = smoother.
     *  At kHopSize=2048 / 48kHz, FFT runs at ~23Hz. Alpha=0.6 gives
     *  a ~72ms time constant, fast enough for responsive visualization. */
    static constexpr float kSmoothingAlpha = 0.6f;

    /** Minimum dB value (floor). Magnitudes below this are clamped. */
    static constexpr float kMinDb = -90.0f;

    /** Maximum dB value (ceiling). Magnitudes above this are clamped. */
    static constexpr float kMaxDb = 0.0f;

    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    SpectrumAnalyzer()
        : fftSetup_(nullptr)
        , fftInputBuf_(nullptr)
        , fftOutputBuf_(nullptr)
        , workBuf_(nullptr)
        , sampleRate_(48000)
        , ringWritePos_(0)
        , samplesUntilNextFft_(kHopSize)
        , activeBufferIndex_(0)
        , initialized_{false}
    {
        /* Zero the ring buffer, window, bin edges, and output buffers.
         * These are all member arrays (stack/member allocated), so no
         * heap allocation occurs here. */
        ringBuffer_.fill(0.0f);
        hannWindow_.fill(0.0f);
        smoothedBins_.fill(kMinDb);
        outputBuffers_[0].fill(kMinDb);
        outputBuffers_[1].fill(kMinDb);

        /* Bin edges are initialized to zero; they will be properly computed
         * in setSampleRate() when the sample rate is known. */
        binEdges_.fill(0);
    }

    ~SpectrumAnalyzer() {
        cleanup();
    }

    /* Non-copyable, non-movable: owns raw PFFFT-allocated resources. */
    SpectrumAnalyzer(const SpectrumAnalyzer&) = delete;
    SpectrumAnalyzer& operator=(const SpectrumAnalyzer&) = delete;
    SpectrumAnalyzer(SpectrumAnalyzer&&) = delete;
    SpectrumAnalyzer& operator=(SpectrumAnalyzer&&) = delete;

    // =========================================================================
    // Setup (NOT real-time safe -- called from setup thread)
    // =========================================================================

    /**
     * Initialize or reinitialize the analyzer for a given sample rate.
     *
     * Allocates the PFFFT setup and all aligned FFT buffers, pre-computes
     * the Hann window, and calculates the FFT bin -> log frequency bin
     * mapping table. Must be called before pushSamples().
     *
     * NOT real-time safe (allocates memory via pffft_aligned_malloc).
     *
     * @param rate  Audio sample rate in Hz (e.g., 44100, 48000).
     */
    void setSampleRate(int32_t rate) {
        /* Clean up any previous initialization (re-entrant safe). */
        cleanup();

        sampleRate_ = rate;

        // --------------------------------------------------------------
        // Create PFFFT plan for real-valued forward FFT
        // --------------------------------------------------------------

        fftSetup_ = pffft_new_setup(kFftSize, PFFFT_REAL);
        if (fftSetup_ == nullptr) {
            return; // PFFFT setup failed; analyzer will be non-functional
        }

        // --------------------------------------------------------------
        // Allocate PFFFT-aligned buffers
        // --------------------------------------------------------------

        /* All buffers passed to pffft_transform() must be 16-byte aligned
         * for SIMD (NEON/SSE) correctness. */
        fftInputBuf_ = static_cast<float*>(
            pffft_aligned_malloc(static_cast<size_t>(kFftSize) * sizeof(float)));
        fftOutputBuf_ = static_cast<float*>(
            pffft_aligned_malloc(static_cast<size_t>(kFftSize) * sizeof(float)));
        workBuf_ = static_cast<float*>(
            pffft_aligned_malloc(static_cast<size_t>(kFftSize) * sizeof(float)));

        if (!fftInputBuf_ || !fftOutputBuf_ || !workBuf_) {
            cleanup();
            return; // Allocation failed
        }

        /* Zero the FFT buffers to prevent uninitialized data. */
        std::memset(fftInputBuf_, 0, static_cast<size_t>(kFftSize) * sizeof(float));
        std::memset(fftOutputBuf_, 0, static_cast<size_t>(kFftSize) * sizeof(float));
        std::memset(workBuf_, 0, static_cast<size_t>(kFftSize) * sizeof(float));

        // --------------------------------------------------------------
        // Pre-compute Hann window
        // --------------------------------------------------------------

        /* Hann window: w(n) = 0.5 * (1 - cos(2*pi*n / (N-1)))
         * This reduces spectral leakage compared to a rectangular window.
         * The window is applied sample-by-sample before the FFT. */
        const float piOverN = static_cast<float>(M_PI) * 2.0f
                              / static_cast<float>(kFftSize - 1);
        for (int i = 0; i < kFftSize; ++i) {
            hannWindow_[i] = 0.5f * (1.0f - std::cos(piOverN * static_cast<float>(i)));
        }

        // --------------------------------------------------------------
        // Pre-compute log-frequency bin edge mapping
        // --------------------------------------------------------------

        /* We map FFT magnitude bins to 64 logarithmically-spaced output bins.
         * Each output bin i covers the frequency range:
         *   lowFreq  = 20 * pow(1000, i / 63.0)
         *   highFreq = 20 * pow(1000, (i+1) / 63.0)
         *
         * We store the mapping as FFT bin indices (integers) so that
         * pushSamples() can iterate without any floating-point frequency
         * calculations on the audio thread.
         *
         * binEdges_[i] = first FFT bin index that falls within output bin i
         * binEdges_[i+1] = first FFT bin index PAST output bin i
         * So output bin i covers FFT bins [binEdges_[i], binEdges_[i+1]).
         */
        const float fftBinWidth = static_cast<float>(sampleRate_) / static_cast<float>(kFftSize);
        const int maxFftBin = kFftSize / 2; // Nyquist bin index

        for (int i = 0; i <= kNumBins; ++i) {
            /* Frequency at this bin edge. */
            float freq = kMinFreqHz * std::pow(kFreqRatio,
                static_cast<float>(i) / static_cast<float>(kNumBins - 1));

            /* Convert frequency to FFT bin index (round to nearest).
             * FFT bin k corresponds to frequency k * sampleRate / fftSize. */
            int fftBin = static_cast<int>(std::round(freq / fftBinWidth));

            /* Clamp to valid range [0, N/2]. Bin 0 = DC, bin N/2 = Nyquist. */
            if (fftBin < 0) fftBin = 0;
            if (fftBin > maxFftBin) fftBin = maxFftBin;

            binEdges_[i] = fftBin;
        }

        /* Ensure every output bin covers at least one FFT bin. If two
         * consecutive edges are equal (possible at low frequencies where
         * log spacing is very fine), bump the upper edge by 1. This
         * guarantees the inner loop in performFft() always has at least
         * one FFT bin to examine per output bin. */
        for (int i = 0; i < kNumBins; ++i) {
            if (binEdges_[i + 1] <= binEdges_[i]) {
                binEdges_[i + 1] = binEdges_[i] + 1;
                /* Clamp to Nyquist. */
                if (binEdges_[i + 1] > maxFftBin) {
                    binEdges_[i + 1] = maxFftBin;
                }
            }
        }

        // --------------------------------------------------------------
        // Reset processing state
        // --------------------------------------------------------------

        ringBuffer_.fill(0.0f);
        ringWritePos_ = 0;
        samplesUntilNextFft_ = kHopSize;
        smoothedBins_.fill(kMinDb);
        outputBuffers_[0].fill(kMinDb);
        outputBuffers_[1].fill(kMinDb);
        activeBufferIndex_.store(0, std::memory_order_relaxed);

        initialized_.store(true, std::memory_order_release);
    }

    // =========================================================================
    // Audio thread interface (REAL-TIME SAFE)
    // =========================================================================

    /**
     * Feed audio samples into the analyzer from the audio callback.
     *
     * Accumulates samples into a ring buffer. Every kHopSize (2048) new
     * samples, performs a windowed FFT on the latest kFftSize (2048) samples,
     * computes log-frequency magnitudes, applies smoothing, and publishes
     * the result via atomic double-buffer swap.
     *
     * REAL-TIME SAFE: No allocations, no locks, no logging, no syscalls.
     * All buffers are pre-allocated in setSampleRate().
     *
     * @param data   Pointer to mono float audio samples.
     * @param count  Number of samples in the buffer.
     */
    void pushSamples(const float* data, int count) {
        if (!initialized_.load(std::memory_order_acquire) || data == nullptr || count <= 0) {
            return;
        }

        for (int i = 0; i < count; ++i) {
            /* Write sample into the ring buffer at the current position.
             * The ring buffer is a fixed-size circular buffer of kFftSize
             * samples. When ringWritePos_ reaches kFftSize, it wraps to 0. */
            ringBuffer_[ringWritePos_] = data[i];
            ringWritePos_ = (ringWritePos_ + 1) % kFftSize;

            /* Decrement the hop counter. When it reaches zero, we have
             * accumulated kHopSize new samples since the last FFT, so
             * we perform a new analysis frame. */
            --samplesUntilNextFft_;
            if (samplesUntilNextFft_ <= 0) {
                performFft();
                samplesUntilNextFft_ = kHopSize;
            }
        }
    }

    // =========================================================================
    // UI thread interface (LOCK-FREE)
    // =========================================================================

    /**
     * Read the current spectrum data for UI visualization.
     *
     * Copies the active double buffer into the caller's array. Lock-free:
     * reads the atomic buffer index, then copies. The worst case is reading
     * a slightly stale frame (one frame behind), which is visually
     * imperceptible at 30fps UI updates.
     *
     * @param bins   Destination array for dB magnitude values [-90, 0].
     * @param count  Number of bins to read (should be kNumBins = 64).
     *               If count > kNumBins, extra elements are set to kMinDb.
     */
    void getSpectrum(float* bins, int count) const {
        if (bins == nullptr || count <= 0) {
            return;
        }

        /* Read the active buffer index. memory_order_acquire ensures we
         * see all the writes the audio thread made before the swap. */
        const int idx = activeBufferIndex_.load(std::memory_order_acquire);
        const auto& activeBuf = outputBuffers_[idx];

        /* Copy available bins. */
        const int toCopy = (count < kNumBins) ? count : kNumBins;
        std::memcpy(bins, activeBuf.data(), static_cast<size_t>(toCopy) * sizeof(float));

        /* Zero-fill any excess bins requested beyond kNumBins. */
        if (count > kNumBins) {
            for (int i = kNumBins; i < count; ++i) {
                bins[i] = kMinDb;
            }
        }
    }

    /**
     * Check whether the analyzer has been successfully initialized.
     * @return true if setSampleRate() completed successfully.
     */
    bool isInitialized() const {
        return initialized_;
    }

private:
    // =========================================================================
    // Core FFT analysis (called from pushSamples, real-time safe)
    // =========================================================================

    /**
     * Perform one FFT analysis frame and update the output double buffer.
     *
     * Steps:
     *   1. Copy kFftSize samples from the ring buffer (unwrapping the
     *      circular read) into the FFT input buffer
     *   2. Apply the pre-computed Hann window
     *   3. Forward FFT via PFFFT
     *   4. Compute magnitude for each FFT bin
     *   5. Map FFT bins to 64 log-frequency output bins (max magnitude)
     *   6. Convert to dB and clamp to [-90, 0]
     *   7. Apply exponential smoothing
     *   8. Write to inactive double buffer and atomically swap
     *
     * REAL-TIME SAFE: operates entirely on pre-allocated member buffers.
     */
    void performFft() {
        // ------------------------------------------------------------------
        // Step 1: Copy ring buffer contents into FFT input buffer
        // ------------------------------------------------------------------

        /* The ring buffer write position points to the NEXT sample to be
         * written, which is also the OLDEST sample in the buffer. So the
         * most recent kFftSize samples start at ringWritePos_ and wrap
         * around. We read them in chronological order. */
        const int readStart = ringWritePos_; // oldest sample position
        for (int i = 0; i < kFftSize; ++i) {
            fftInputBuf_[i] = ringBuffer_[(readStart + i) % kFftSize];
        }

        // ------------------------------------------------------------------
        // Step 2: Apply Hann window
        // ------------------------------------------------------------------

        /* Multiply each sample by its corresponding window coefficient.
         * This reduces spectral leakage at the frame boundaries. */
        for (int i = 0; i < kFftSize; ++i) {
            fftInputBuf_[i] *= hannWindow_[i];
        }

        // ------------------------------------------------------------------
        // Step 3: Forward FFT
        // ------------------------------------------------------------------

        pffft_transform_ordered(fftSetup_, fftInputBuf_, fftOutputBuf_,
                               workBuf_, PFFFT_FORWARD);

        // ------------------------------------------------------------------
        // Step 4 & 5: Compute magnitudes and map to log-frequency bins
        // ------------------------------------------------------------------

        /* PFFFT output format for REAL transform of size N:
         *   out[0]     = DC component (real only, imaginary = 0)
         *   out[1]     = Nyquist component (real only, imaginary = 0)
         *   For k = 1 to N/2-1:
         *     out[2*k]   = real part of bin k
         *     out[2*k+1] = imaginary part of bin k
         *
         * We compute magnitude = sqrt(re^2 + im^2) for each FFT bin on
         * demand within the log-frequency bin loop below. */

        /* Determine which double buffer to write to (the inactive one). */
        const int writeIdx = 1 - activeBufferIndex_.load(std::memory_order_relaxed);

        for (int bin = 0; bin < kNumBins; ++bin) {
            const int fftBinStart = binEdges_[bin];
            const int fftBinEnd = binEdges_[bin + 1];

            /* Find the maximum magnitude across all FFT bins in this
             * log-frequency range. Using max (rather than sum or average)
             * gives the best visual representation for guitar signals,
             * which tend to have sharp spectral peaks. */
            float maxMag = 0.0f;

            for (int k = fftBinStart; k < fftBinEnd; ++k) {
                float mag = 0.0f;

                if (k == 0) {
                    /* DC bin: real component only. */
                    mag = std::fabs(fftOutputBuf_[0]);
                } else if (k == kFftSize / 2) {
                    /* Nyquist bin: stored in out[1], real component only. */
                    mag = std::fabs(fftOutputBuf_[1]);
                } else {
                    /* General bin k: interleaved real/imag at [2k, 2k+1]. */
                    const float re = fftOutputBuf_[2 * k];
                    const float im = fftOutputBuf_[2 * k + 1];
                    mag = std::sqrt(re * re + im * im);
                }

                if (mag > maxMag) {
                    maxMag = mag;
                }
            }

            // ----------------------------------------------------------
            // Step 6: Convert to dB and clamp
            // ----------------------------------------------------------

            /* dB = 20 * log10(magnitude)
             * Guard against log10(0) by using a tiny floor value.
             * 1e-10 corresponds to -200dB, well below our -90dB floor. */
            float db = 20.0f * std::log10(maxMag + 1e-10f);

            /* Clamp to [-90, 0] dB range. */
            if (db < kMinDb) db = kMinDb;
            if (db > kMaxDb) db = kMaxDb;

            // ----------------------------------------------------------
            // Step 7: Exponential smoothing
            // ----------------------------------------------------------

            /* smoothed = alpha * new + (1 - alpha) * old
             * This provides temporal smoothing so the visualization doesn't
             * flicker. Alpha = 0.3 gives a good balance between
             * responsiveness and visual stability. */
            smoothedBins_[bin] = kSmoothingAlpha * db
                                 + (1.0f - kSmoothingAlpha) * smoothedBins_[bin];

            // ----------------------------------------------------------
            // Step 8: Write to inactive double buffer
            // ----------------------------------------------------------

            outputBuffers_[writeIdx][bin] = smoothedBins_[bin];
        }

        // ------------------------------------------------------------------
        // Atomically swap the active buffer index
        // ------------------------------------------------------------------

        /* memory_order_release ensures all writes to outputBuffers_[writeIdx]
         * are visible to any thread that subsequently reads activeBufferIndex_
         * with memory_order_acquire (i.e., the UI thread in getSpectrum()). */
        activeBufferIndex_.store(writeIdx, std::memory_order_release);
    }

    // =========================================================================
    // Resource cleanup
    // =========================================================================

    /**
     * Free all PFFFT resources. Safe to call multiple times (idempotent).
     * NOT real-time safe (frees memory).
     */
    void cleanup() {
        if (fftSetup_) {
            pffft_destroy_setup(fftSetup_);
            fftSetup_ = nullptr;
        }
        if (fftInputBuf_) {
            pffft_aligned_free(fftInputBuf_);
            fftInputBuf_ = nullptr;
        }
        if (fftOutputBuf_) {
            pffft_aligned_free(fftOutputBuf_);
            fftOutputBuf_ = nullptr;
        }
        if (workBuf_) {
            pffft_aligned_free(workBuf_);
            workBuf_ = nullptr;
        }
        initialized_.store(false, std::memory_order_release);
    }

    // =========================================================================
    // PFFFT resources (allocated in setSampleRate, freed in cleanup)
    // =========================================================================

    /** PFFFT plan for forward real FFT of size kFftSize. */
    PFFFT_Setup* fftSetup_;

    /** FFT input buffer: windowed time-domain samples. Size: kFftSize floats.
     *  PFFFT-aligned (16-byte) for SIMD correctness. */
    float* fftInputBuf_;

    /** FFT output buffer: frequency-domain data in PFFFT's interleaved format.
     *  Size: kFftSize floats. PFFFT-aligned. */
    float* fftOutputBuf_;

    /** PFFFT work buffer for transform calls. Size: kFftSize floats.
     *  Required by pffft_transform to avoid stack allocation. PFFFT-aligned. */
    float* workBuf_;

    // =========================================================================
    // Configuration
    // =========================================================================

    /** Audio sample rate in Hz. Set by setSampleRate(). */
    int32_t sampleRate_;

    // =========================================================================
    // Pre-computed tables (filled in setSampleRate, read-only during process)
    // =========================================================================

    /** Pre-computed Hann window coefficients. Applied to the time-domain
     *  signal before FFT to reduce spectral leakage. */
    std::array<float, kFftSize> hannWindow_;

    /**
     * FFT bin index boundaries for the 64 log-frequency output bins.
     * Size: kNumBins + 1 = 65 entries (one more than bins, for the upper edge).
     *
     * Output bin i covers FFT bins in the range [binEdges_[i], binEdges_[i+1]).
     * Pre-computed in setSampleRate() so the audio thread does no float math
     * for frequency mapping.
     */
    std::array<int, kNumBins + 1> binEdges_;

    // =========================================================================
    // Ring buffer for input accumulation (audio thread only)
    // =========================================================================

    /** Circular buffer holding the most recent kFftSize audio samples.
     *  New samples overwrite the oldest. When kHopSize new samples arrive,
     *  an FFT is triggered on the full buffer contents. */
    std::array<float, kFftSize> ringBuffer_;

    /** Current write position in the ring buffer [0, kFftSize). */
    int ringWritePos_;

    /** Countdown: number of new samples needed before the next FFT.
     *  Initialized to kHopSize; decremented per sample; reset after FFT. */
    int samplesUntilNextFft_;

    // =========================================================================
    // Smoothed bins (audio thread only, written during performFft)
    // =========================================================================

    /** Exponentially smoothed magnitude values (dB) for each output bin.
     *  Updated in performFft() using: new = alpha * raw + (1-alpha) * old.
     *  This array is written ONLY by the audio thread. */
    std::array<float, kNumBins> smoothedBins_;

    // =========================================================================
    // Double-buffered output (audio thread writes, UI thread reads)
    // =========================================================================

    /** Two output buffers holding 64 dB-scaled magnitude bins each.
     *  The audio thread writes to the inactive buffer, then atomically
     *  swaps activeBufferIndex_. The UI thread reads the active buffer. */
    std::array<float, kNumBins> outputBuffers_[2];

    /** Index of the buffer currently readable by the UI thread (0 or 1).
     *  The audio thread writes to (1 - activeBufferIndex_) then stores
     *  the new index with memory_order_release. The UI thread loads with
     *  memory_order_acquire to see all preceding writes. */
    std::atomic<int> activeBufferIndex_;

    /** True once setSampleRate() has completed successfully. Prevents
     *  pushSamples() from operating on uninitialized state. */
    std::atomic<bool> initialized_;
};

#endif // GUITAR_ENGINE_SPECTRUM_ANALYZER_H
