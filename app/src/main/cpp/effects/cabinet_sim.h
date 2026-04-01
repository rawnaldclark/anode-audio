#ifndef GUITAR_ENGINE_CABINET_SIM_H
#define GUITAR_ENGINE_CABINET_SIM_H

#include "effect.h"
#include "fft_convolver.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <array>
#include <mutex>
#include <vector>

/**
 * Cabinet impulse response (IR) simulator using direct convolution.
 *
 * A guitar speaker cabinet is the final stage of a guitar amplifier's signal
 * path, and it has an enormous impact on the overall tone. The speaker and
 * cabinet enclosure act as a complex filter that rolls off harsh high
 * frequencies, adds resonant peaks, and shapes the midrange character.
 * Without cabinet simulation, a distorted guitar signal sounds thin, harsh,
 * and "fizzy" -- nothing like the warm, full sound of a real amp.
 *
 * This effect applies a cabinet's frequency response to the audio signal
 * using convolution with a short impulse response (IR). Convolution in the
 * time domain computes:
 *
 *   output[n] = sum(input[n-k] * ir[k]) for k = 0..irLength-1
 *
 * The impulse response captures the complete frequency and phase behavior
 * of the cabinet in a single, short recording. When convolved with the
 * input signal, the result sounds as if the signal passed through the
 * actual cabinet.
 *
 * Implementation approach:
 *   Hybrid convolution engine with two paths selected automatically:
 *
 *   1. Direct (time-domain) convolution for very short IRs (< 128 samples).
 *      O(N*M) per buffer, efficient for M < 128 on NEON/SSE hardware.
 *      Uses a circular buffer to avoid shifting the input history array.
 *
 *   2. FFT-based convolution via FFTConvolver (uniformly partitioned
 *      overlap-add) for longer IRs (>= 128 samples). O(N*log(N)) per
 *      block, uses PFFFT for SIMD-optimized (NEON/SSE) transforms.
 *      Enables high-quality IRs of 256-8192 samples without excessive CPU.
 *
 * Built-in cabinet profiles (9 built-in + 1 user slot = 10 types):
 *   Nine cabinet IRs are generated algorithmically in the constructor using
 *   a windowed frequency-sampling FIR design method. Each profile is defined
 *   by a target frequency response curve that approximates a known cabinet
 *   type. The IR is synthesized by:
 *     1. Defining the desired magnitude response at evenly-spaced frequencies
 *     2. Computing the inverse DFT to get the time-domain impulse response
 *     3. Applying a Blackman window to reduce spectral leakage and truncation
 *        artifacts (Gibbs phenomenon)
 *     4. Normalizing the IR so its peak absolute value is 1.0
 *
 *   Cabinet types:
 *     0 = 1x12 Combo (Fender-style, Jensen speaker, bright/articulate)
 *     1 = 4x12 British (Marshall 1960A stock, warm/mid-forward)
 *     2 = 2x12 Open Back (Vox AC30-style, chimey/balanced)
 *     3 = User IR (loaded from .wav file)
 *     4 = Greenback 4x12 (Celestion G12M-25, vintage warm/creamy)
 *     5 = G12H-30 4x12 (Celestion G12H-30, tight/aggressive/cutting)
 *     6 = G12-65 4x12 (Celestion G12-65, modern/transparent/flat)
 *     7 = Fane Crescendo (WEM Starfinder, hi-fi/detailed/articulate)
 *     8 = Alnico Blue 2x12 (Vox AC30 Alnico, bright/chimey/jangly)
 *     9 = Jensen C10Q 1x10 (Silvertone 1485, tight/nasal/lo-fi)
 *
 * User IR loading:
 *   loadUserIR() accepts float sample data from a decoded .wav file,
 *   stores it internally, and initializes the FFTConvolver for real-time
 *   processing. Must be called from a non-audio thread.
 *
 * Parameter IDs for JNI access:
 *   0 = cabinetType (0-2 = original built-in, 3 = user IR, 4-9 = new built-in)
 *   1 = mix (0.0 to 1.0) - blend between dry and cabinet-processed signal
 */
class CabinetSim : public Effect {
public:
    /**
     * Construct the cabinet simulator and generate all built-in IR profiles.
     *
     * The IRs are generated at the default sample rate (44100 Hz).
     * They will be regenerated if setSampleRate() provides a different rate.
     */
    CabinetSim() {
        generateAllIRs(44100);
        initBuiltInConvolvers();
        selectIR(0);
    }

    // =========================================================================
    // Effect interface
    // =========================================================================

    /**
     * Process audio through the cabinet simulator. Real-time safe.
     *
     * For each input sample:
     *   1. Write the sample into the circular buffer at the current write position
     *   2. Compute the convolution sum over the active IR
     *   3. Blend the convolved signal with the dry signal based on the mix parameter
     *   4. Advance the circular buffer write pointer
     *
     * The convolution uses the standard time-domain formulation:
     *   y[n] = sum_{k=0}^{M-1} h[k] * x[n-k]
     * where h[] is the IR and x[] is the input history in the circular buffer.
     *
     * @param buffer   Mono audio samples in [-1.0, 1.0].
     * @param numFrames Number of frames in the buffer.
     */
    void process(float* buffer, int numFrames) override {
        // Snapshot parameters
        const int cabType = static_cast<int>(
            cabinetType_.load(std::memory_order_relaxed));
        const float mix = mix_.load(std::memory_order_relaxed);

        // Early exit if fully dry (no cabinet processing needed)
        if (mix <= 0.0f) return;

        // Try to acquire the IR swap mutex. If loadUserIR() is currently
        // running on another thread, try_lock() fails and we skip processing
        // for this one callback (~1-10ms). This prevents use-after-free if
        // the user IR vector is being reallocated. Since IR loads are rare
        // (user-initiated), contention is negligible in practice.
        std::unique_lock<std::mutex> lock(irMutex_, std::try_to_lock);
        if (!lock.owns_lock()) {
            // loadUserIR() is running -- pass audio through unchanged.
            // One callback of unprocessed audio is inaudible.
            return;
        }

        // Check if cabinet type has changed since last process call.
        // We track this with a non-atomic int because only the audio thread
        // reads/writes it, and the parameter change is picked up on the next
        // process() call (acceptable latency: one audio buffer, ~1-10ms).
        if (cabType != currentCabType_) {
            selectIR(cabType);
            currentCabType_ = cabType;
        }

        if (useFFTConvolution_) {
            processFFT(buffer, numFrames, mix);
        } else {
            processDirect(buffer, numFrames, mix);
        }
    }

    /**
     * Configure sample rate and regenerate IRs.
     *
     * The cabinet IRs are frequency-response-based, so they must be
     * regenerated when the sample rate changes to maintain the correct
     * frequency characteristics. A 120Hz resonance peak in a 44100Hz IR
     * would shift to a different frequency if played back at 48000Hz
     * without regeneration.
     *
     * @param sampleRate Device sample rate in Hz.
     */
    void setSampleRate(int32_t sampleRate) override {
        if (sampleRate_ != sampleRate) {
            sampleRate_ = sampleRate;
            generateAllIRs(sampleRate);
            initBuiltInConvolvers();
            // Re-select current cabinet to update the active convolver pointer
            selectIR(currentCabType_);
        }
    }

    /**
     * Reset the convolution state.
     *
     * Clears both the direct convolution circular buffer and the FFT
     * convolver state. This prevents stale audio data from "leaking"
     * into the output when the effect is re-enabled or the stream restarts.
     */
    void reset() override {
        std::fill(circularBuffer_.begin(), circularBuffer_.end(), 0.0f);
        writePos_ = 0;
        for (auto& conv : builtInConvolvers_) {
            conv.reset();
        }
        userConvolver_.reset();
    }

    // =========================================================================
    // Parameter access (called from UI thread via JNI)
    // =========================================================================

    /**
     * Set a parameter by ID. Thread-safe via atomic stores.
     *
     * @param paramId  0=cabinetType, 1=mix
     * @param value    Parameter value.
     */
    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0:
                cabinetType_.store(clamp(std::round(value), 0.0f, 9.0f),
                                   std::memory_order_relaxed);
                break;
            case 1:
                mix_.store(clamp(value, 0.0f, 1.0f),
                           std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    /**
     * Get a parameter by ID. Thread-safe via atomic loads.
     *
     * @param paramId  0=cabinetType, 1=mix
     * @return Current parameter value.
     */
    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return cabinetType_.load(std::memory_order_relaxed);
            case 1: return mix_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

    // =========================================================================
    // Future API: User-loaded IRs
    // =========================================================================

    /**
     * Load a user-provided impulse response.
     *
     * Copies the IR data into internal storage, resamples it to the device
     * sample rate if needed (using linear interpolation), normalizes it,
     * and initializes the FFTConvolver for real-time processing. The IR is
     * used until another user IR is loaded or a built-in cabinet is selected.
     *
     * If irSampleRate differs from the device sample rate, the IR is
     * resampled so that its frequency content plays back correctly.
     * Without resampling, a 96kHz IR played on a 48kHz device would have
     * all frequencies shifted down by an octave.
     *
     * NOT real-time safe (allocates memory). Must be called from the main
     * thread. Ideally call with audio processing paused to avoid glitches
     * during the brief FFTConvolver re-initialization.
     *
     * TODO: Double-buffered FFTConvolver swap for glitch-free IR switching.
     *
     * @param irData       Pointer to float IR samples.
     * @param irLength     Number of samples in the IR (1 to kMaxIRLength).
     * @param irSampleRate Sample rate the IR was recorded at.
     *                     If <= 0, the IR is assumed to match the device rate.
     * @return true if the IR was loaded successfully.
     */
    bool loadUserIR(const float* irData, int irLength, int irSampleRate) {
        if (irData == nullptr || irLength < 1 || irLength > kMaxIRLength) {
            return false;
        }

        // Lock the IR swap mutex to prevent the audio thread from reading
        // activeIR_ or activeConvolver_ while we reallocate and reinitialize.
        // The audio thread uses try_lock() in process() and will skip one
        // callback if we hold the lock -- a single buffer of unprocessed
        // audio (~1-10ms) which is inaudible.
        std::lock_guard<std::mutex> lock(irMutex_);

        // Resample the IR if its sample rate differs from the device rate.
        // If irSampleRate is invalid (<= 0), assume it matches the device
        // rate (no resampling). This is a safe default that preserves the
        // original IR data.
        if (irSampleRate > 0 && irSampleRate != sampleRate_) {
            std::vector<float> resampled = resampleIR(
                irData, irLength, irSampleRate, sampleRate_, kMaxIRLength);
            if (resampled.empty()) {
                return false;
            }
            userIR_ = std::move(resampled);
            userIRLength_ = static_cast<int>(userIR_.size());
        } else {
            // No resampling needed -- copy IR data directly
            userIR_.assign(irData, irData + irLength);
            userIRLength_ = irLength;
        }

        // Normalize to unit peak amplitude.
        // Use userIRLength_ (not irLength) because resampling may have
        // changed the number of samples.
        float peak = 0.0f;
        for (int i = 0; i < userIRLength_; ++i) {
            float absVal = std::abs(userIR_[i]);
            if (absVal > peak) peak = absVal;
        }
        if (peak > 0.0f) {
            float invPeak = 1.0f / peak;
            for (int i = 0; i < userIRLength_; ++i) {
                userIR_[i] *= invPeak;
            }
        }

        // Initialize the user convolver (FFT for long IRs)
        if (userIRLength_ >= kDirectConvolutionThreshold) {
            userConvolver_.init(userIR_.data(), userIRLength_, kFFTBlockSize);
        }

        // Activate the user IR
        activeIR_ = userIR_.data();
        activeIRLength_ = userIRLength_;
        if (userConvolver_.isInitialized() &&
            userIRLength_ >= kDirectConvolutionThreshold) {
            activeConvolver_ = &userConvolver_;
            useFFTConvolution_ = true;
        } else {
            activeConvolver_ = nullptr;
            useFFTConvolution_ = false;
        }
        currentCabType_ = kUserIRIndex;

        return true;
    }

private:
    // =========================================================================
    // Constants
    // =========================================================================

    /** Maximum IR length in samples. 8192 at 48kHz = ~170ms (long room IRs). */
    static constexpr int kMaxIRLength = 8192;

    /**
     * IR length threshold for switching from direct to FFT convolution.
     * Below this, direct convolution is faster (no FFT overhead).
     * Above this, FFT convolution's O(N*log(N)) beats direct's O(N*M).
     * 128 is conservative; on NEON, FFT may win as early as 64 samples.
     */
    static constexpr int kDirectConvolutionThreshold = 128;

    /** Internal block size for the FFT convolver. Must be power of 2. */
    static constexpr int kFFTBlockSize = 256;

    /**
     * Maximum audio frames per callback for dry buffer storage.
     * Oboe typically delivers 64-960 frames. 4096 is a generous upper bound.
     * If a callback exceeds this, excess frames get full-wet processing only.
     */
    static constexpr int kMaxBufferSize = 4096;

    /**
     * IR length used for built-in cabinet profiles.
     * 256 samples at 44.1kHz = ~5.8ms, sufficient for capturing the
     * essential frequency response of a guitar cabinet without excessive
     * CPU cost from the convolution loop.
     */
    static constexpr int kBuiltInIRLength = 256;

    /**
     * Number of built-in cabinet profiles (9 total).
     * These occupy indices 0-2 and 4-9 in the user-facing parameter space.
     * Index 3 is reserved for user IR.
     */
    static constexpr int kNumCabinets = 9;

    /** Parameter index for the user-loaded IR. */
    static constexpr int kUserIRIndex = 3;

    /**
     * Circular buffer size for direct convolution. Sized to accommodate
     * the longest IR that would use the direct path (< kDirectConvolutionThreshold).
     * In practice, only IRs shorter than 128 samples use direct convolution.
     */
    static constexpr int kCircularBufferSize = 512;

    /** Pi constant. */
    static constexpr double kPi = 3.14159265358979323846;

    // =========================================================================
    // IR generation
    // =========================================================================

    /**
     * Generate all nine built-in cabinet IRs at the given sample rate.
     *
     * Each IR is designed using the windowed frequency-sampling method:
     *   1. Define a target magnitude response at N/2+1 frequency bins
     *   2. Construct a complex spectrum with the target magnitude and
     *      linear phase (to center the IR in the window)
     *   3. Inverse DFT to produce the time-domain IR
     *   4. Apply a Blackman window to reduce truncation artifacts
     *   5. Normalize to unit peak amplitude
     *
     * This approach is standard FIR filter design and produces IRs that
     * accurately reproduce the specified frequency response curves.
     *
     * @param sampleRate Target sample rate for the IRs.
     */
    void generateAllIRs(int sampleRate) {
        generateComboIR(sampleRate);
        generateBritishIR(sampleRate);
        generateOpenBackIR(sampleRate);
        generateGreenbackIR(sampleRate);
        generateG12H30IR(sampleRate);
        generateG12_65IR(sampleRate);
        generateFaneCrescendoIR(sampleRate);
        generateAlnicoBlueIR(sampleRate);
        generateJensenC10qIR(sampleRate);
    }

    /**
     * Cabinet 0: 1x12 Combo (American-style, Fender-like).
     *
     * Frequency response characteristics:
     *   - Tight, focused low end with rolloff below 80Hz
     *   - Prominent upper-midrange peak around 2-3kHz (the "presence" peak)
     *   - Sharp rolloff above 5kHz (speaker cone breakup region)
     *   - Overall bright and articulate character
     *
     * This models a single 12" speaker in a closed-back combo amplifier,
     * typical of Fender Deluxe Reverb or Princeton Reverb amps.
     *
     * @param sampleRate Target sample rate.
     */
    void generateComboIR(int sampleRate) {
        const int N = kBuiltInIRLength;
        const double sr = static_cast<double>(sampleRate);

        // Define magnitude response at frequency bins
        // Bin k corresponds to frequency k * sr / N
        std::array<double, kBuiltInIRLength> magResponse{};
        for (int k = 0; k <= N / 2; ++k) {
            double freq = static_cast<double>(k) * sr / static_cast<double>(N);
            magResponse[k] = comboMagnitudeResponse(freq);
        }

        // Generate IR from magnitude response using inverse DFT with
        // linear phase (type I FIR, symmetric)
        synthesizeIR(magResponse.data(), comboIR_.data(), N);
    }

    /**
     * Cabinet 1: 4x12 British (Marshall-style).
     *
     * Frequency response characteristics:
     *   - Extended low end with slight boost around 100-150Hz (cabinet resonance)
     *   - Forward midrange with peak around 1-2kHz ("bark" character)
     *   - Moderate high-frequency rolloff starting at 4kHz
     *   - Overall warm and mid-focused character
     *
     * This models four 12" speakers (Celestion Greenback style) in a closed-back
     * cabinet, typical of Marshall 1960A/B cabs.
     *
     * @param sampleRate Target sample rate.
     */
    void generateBritishIR(int sampleRate) {
        const int N = kBuiltInIRLength;
        const double sr = static_cast<double>(sampleRate);

        std::array<double, kBuiltInIRLength> magResponse{};
        for (int k = 0; k <= N / 2; ++k) {
            double freq = static_cast<double>(k) * sr / static_cast<double>(N);
            magResponse[k] = britishMagnitudeResponse(freq);
        }

        synthesizeIR(magResponse.data(), britishIR_.data(), N);
    }

    /**
     * Cabinet 2: 2x12 Open Back (Vox-style).
     *
     * Frequency response characteristics:
     *   - Loose, open low end (open-back cabinet reduces bass coupling)
     *   - Scooped midrange (characteristic of Vox AC30)
     *   - Bright, chimey highs with extended top end
     *   - Smooth, balanced overall character
     *
     * This models two 12" speakers (Celestion Blue/Alnico style) in an
     * open-back cabinet, typical of Vox AC30 amps.
     *
     * @param sampleRate Target sample rate.
     */
    void generateOpenBackIR(int sampleRate) {
        const int N = kBuiltInIRLength;
        const double sr = static_cast<double>(sampleRate);

        std::array<double, kBuiltInIRLength> magResponse{};
        for (int k = 0; k <= N / 2; ++k) {
            double freq = static_cast<double>(k) * sr / static_cast<double>(N);
            magResponse[k] = openBackMagnitudeResponse(freq);
        }

        synthesizeIR(magResponse.data(), openBackIR_.data(), N);
    }

    /**
     * Cabinet type 4: Celestion G12M-25 "Greenback" (in Marshall 1960A/B).
     *
     * THE classic Marshall speaker. Warm, vintage character with a focused
     * midrange and earlier HF rolloff than the stock British profile.
     *
     * Frequency response characteristics:
     *   - HP below 75Hz (closed-back, tight coupling)
     *   - Resonance peak at ~100Hz from cabinet coupling
     *   - Focused mid presence peak at ~2kHz (narrower than British, more "creamy")
     *   - Sharp treble rolloff above ~4kHz (paper cone breakup, 25W limited HF)
     *   - Overall: warm, slightly dark, compressed -- Hendrix, Clapton, EVH era
     *
     * Key difference from existing "british" (index 1): more mid-focused at
     * 2kHz instead of 1.5kHz, earlier and steeper HF rolloff, tighter Q on
     * the presence peak for a more "vocal" midrange character.
     *
     * @param sampleRate Target sample rate.
     */
    void generateGreenbackIR(int sampleRate) {
        const int N = kBuiltInIRLength;
        const double sr = static_cast<double>(sampleRate);

        std::array<double, kBuiltInIRLength> magResponse{};
        for (int k = 0; k <= N / 2; ++k) {
            double freq = static_cast<double>(k) * sr / static_cast<double>(N);
            magResponse[k] = greenbackMagnitudeResponse(freq);
        }

        synthesizeIR(magResponse.data(), greenbackIR_.data(), N);
    }

    /**
     * Cabinet type 5: Celestion G12H-30 (in Marshall 1960A/B).
     *
     * Heavier magnet than Greenback = tighter low end, more prominent
     * upper-mids. The "cutting" Marshall tone -- Hendrix live, Angus Young.
     *
     * Frequency response characteristics:
     *   - HP below 70Hz (tighter than Greenback)
     *   - Bass resonance at ~110Hz (slightly higher than Greenback)
     *   - Strong upper-mid peak at ~2.5kHz (brighter attack)
     *   - Moderate rolloff above 5kHz (better HF extension than Greenback)
     *   - Overall: tighter, more aggressive, "cutting" tone
     *
     * @param sampleRate Target sample rate.
     */
    void generateG12H30IR(int sampleRate) {
        const int N = kBuiltInIRLength;
        const double sr = static_cast<double>(sampleRate);

        std::array<double, kBuiltInIRLength> magResponse{};
        for (int k = 0; k <= N / 2; ++k) {
            double freq = static_cast<double>(k) * sr / static_cast<double>(N);
            magResponse[k] = g12h30MagnitudeResponse(freq);
        }

        synthesizeIR(magResponse.data(), g12h30IR_.data(), N);
    }

    /**
     * Cabinet type 6: Celestion G12-65 (modern Marshall).
     *
     * Modern Celestion, handles high power, flatter response. The EVH
     * "brown sound" era speaker -- transparent, handles high gain without
     * mushiness.
     *
     * Frequency response characteristics:
     *   - HP below 65Hz (most bass extension of closed-back cabs)
     *   - Flat bass region (no pronounced resonance bump)
     *   - Broad, moderate mid peak at ~1.5-3kHz
     *   - Extended rolloff starting at ~5.5kHz (more modern highs)
     *   - Overall: flatter, more "transparent"
     *
     * @param sampleRate Target sample rate.
     */
    void generateG12_65IR(int sampleRate) {
        const int N = kBuiltInIRLength;
        const double sr = static_cast<double>(sampleRate);

        std::array<double, kBuiltInIRLength> magResponse{};
        for (int k = 0; k <= N / 2; ++k) {
            double freq = static_cast<double>(k) * sr / static_cast<double>(N);
            magResponse[k] = g12_65MagnitudeResponse(freq);
        }

        synthesizeIR(magResponse.data(), g12_65IR_.data(), N);
    }

    /**
     * Cabinet type 7: Fane Crescendo 12" (in WEM Starfinder).
     *
     * David Gilmour's speaker. Hi-fi, almost flat, extended HF response.
     * Pink Floyd's expansive clean and lead tones.
     *
     * Frequency response characteristics:
     *   - HP below 60Hz (WEM cab = more bass extension)
     *   - Very flat midrange (200Hz-2kHz nearly ruler-flat)
     *   - Gentle presence peak at ~3.5kHz (subtle, not aggressive)
     *   - Gradual rolloff above 7kHz (much more HF than any Celestion)
     *   - Overall: hi-fi, detailed, articulate, open
     *
     * @param sampleRate Target sample rate.
     */
    void generateFaneCrescendoIR(int sampleRate) {
        const int N = kBuiltInIRLength;
        const double sr = static_cast<double>(sampleRate);

        std::array<double, kBuiltInIRLength> magResponse{};
        for (int k = 0; k <= N / 2; ++k) {
            double freq = static_cast<double>(k) * sr / static_cast<double>(N);
            magResponse[k] = faneCrescendoMagnitudeResponse(freq);
        }

        synthesizeIR(magResponse.data(), faneCrescendoIR_.data(), N);
    }

    /**
     * Cabinet type 8: Celestion Alnico Blue (in Vox AC30).
     *
     * THE Vox speaker. Chimey, jangly, bright shimmer. Beatles, Brian May,
     * Tom Petty. Similar to the existing openBack (index 2) but with MORE
     * pronounced chime peak, different scoop character, and true open-back
     * bass cancellation modeling.
     *
     * Frequency response characteristics:
     *   - HP below 100Hz (open-back = reduced bass coupling)
     *   - Mid scoop at ~400-600Hz (open-back cancellation, deeper than openBack)
     *   - Prominent "chime" peak at ~3kHz (Alnico magnet character, higher Q)
     *   - Extended top end, gradual rolloff above 6kHz
     *   - Overall: bright, chimey, "jangly"
     *
     * Key difference from openBack (index 2): deeper mid scoop (0.4 vs 0.3),
     * sharper chime peak with higher Q (more "glassy"), slightly less bass.
     *
     * @param sampleRate Target sample rate.
     */
    void generateAlnicoBlueIR(int sampleRate) {
        const int N = kBuiltInIRLength;
        const double sr = static_cast<double>(sampleRate);

        std::array<double, kBuiltInIRLength> magResponse{};
        for (int k = 0; k <= N / 2; ++k) {
            double freq = static_cast<double>(k) * sr / static_cast<double>(N);
            magResponse[k] = alnicoBlueMagnitudeResponse(freq);
        }

        synthesizeIR(magResponse.data(), alnicoBlueIR_.data(), N);
    }

    /**
     * Cabinet type 9: Jensen C10Q 10" (in Silvertone 1485).
     *
     * Jack White's speaker. Small, focused, quick transient response.
     * Tight, focused, "nasal," lo-fi -- White Stripes character.
     *
     * Frequency response characteristics:
     *   - HP below 120Hz (10" speaker = limited bass extension)
     *   - Strong bass rolloff (10" cannot reproduce deep bass)
     *   - Focused midrange peak at ~1kHz (smaller cone = more mid concentration)
     *   - Quick, sharp rolloff above 3.5kHz (early cone breakup, small diameter)
     *   - Overall: tight, focused, "nasal," lo-fi
     *
     * @param sampleRate Target sample rate.
     */
    void generateJensenC10qIR(int sampleRate) {
        const int N = kBuiltInIRLength;
        const double sr = static_cast<double>(sampleRate);

        std::array<double, kBuiltInIRLength> magResponse{};
        for (int k = 0; k <= N / 2; ++k) {
            double freq = static_cast<double>(k) * sr / static_cast<double>(N);
            magResponse[k] = jensenC10qMagnitudeResponse(freq);
        }

        synthesizeIR(magResponse.data(), jensenC10qIR_.data(), N);
    }

    // =========================================================================
    // Convolution processing paths
    // =========================================================================

    /**
     * Process audio through the FFT convolver. Real-time safe.
     *
     * Saves the dry signal before processing (for wet/dry mixing),
     * runs the FFTConvolver, then blends the result with the dry signal.
     *
     * @param buffer    Audio samples, modified in-place.
     * @param numFrames Number of frames in the buffer.
     * @param mix       Wet/dry mix [0.0, 1.0]. Caller guarantees mix > 0.
     */
    void processFFT(float* buffer, int numFrames, float mix) {
        if (activeConvolver_ == nullptr || !activeConvolver_->isInitialized()) {
            return;
        }

        // Clamp to dry buffer capacity. Oboe typically delivers 64-960
        // frames, so this should never activate in practice.
        const int frames = std::min(numFrames, kMaxBufferSize);

        if (mix >= 1.0f) {
            // Fully wet: process in-place, no dry buffer needed
            activeConvolver_->process(buffer, frames);
        } else {
            // Partial mix: save dry signal, process, then blend
            std::memcpy(dryBuffer_.data(), buffer,
                static_cast<size_t>(frames) * sizeof(float));

            activeConvolver_->process(buffer, frames);

            // Blend: output = dry * (1 - mix) + wet * mix
            const float dryGain = 1.0f - mix;
            for (int i = 0; i < frames; ++i) {
                buffer[i] = clamp(
                    dryBuffer_[i] * dryGain + buffer[i] * mix,
                    -1.0f, 1.0f);
            }
        }
    }

    /**
     * Process audio through direct (time-domain) convolution. Real-time safe.
     *
     * Uses the circular buffer and split inner loop for efficient
     * per-sample convolution with short IRs.
     *
     * @param buffer    Audio samples, modified in-place.
     * @param numFrames Number of frames in the buffer.
     * @param mix       Wet/dry mix [0.0, 1.0]. Caller guarantees mix > 0.
     */
    void processDirect(float* buffer, int numFrames, float mix) {
        const float* ir = activeIR_;
        const int irLen = activeIRLength_;

        // Safety check: if no IR is loaded, pass through
        if (ir == nullptr || irLen == 0) return;

        for (int i = 0; i < numFrames; ++i) {
            const float dry = buffer[i];

            // Write current sample into circular buffer
            circularBuffer_[writePos_] = dry;

            // Compute: y[n] = sum_{k=0}^{irLen-1} ir[k] * x[n-k]
            // Split loop to avoid modulo per iteration
            float wet = 0.0f;

            const int firstSegLen = std::min(irLen, writePos_ + 1);

            // Part 1: from writePos_ backward (no wrap needed)
            int bufIdx = writePos_;
            for (int k = 0; k < firstSegLen; ++k) {
                wet += ir[k] * circularBuffer_[bufIdx];
                --bufIdx;
            }

            // Part 2: wrap around and continue from end of buffer
            if (firstSegLen < irLen) {
                bufIdx = kCircularBufferSize - 1;
                for (int k = firstSegLen; k < irLen; ++k) {
                    wet += ir[k] * circularBuffer_[bufIdx];
                    --bufIdx;
                }
            }

            // Blend wet (cabinet) and dry signals
            buffer[i] = clamp(dry * (1.0f - mix) + wet * mix, -1.0f, 1.0f);

            // Advance write position with wraparound
            writePos_ = (writePos_ + 1) % kCircularBufferSize;
        }
    }

    // =========================================================================
    // Cabinet frequency response curves
    // =========================================================================

    /**
     * 1x12 Combo magnitude response curve.
     *
     * Designed to approximate the frequency response of a Fender-style
     * 1x12 combo cabinet with a Jensen-type speaker.
     *
     * Key features:
     *   - 12dB/oct high-pass below 80Hz (cabinet + speaker rolloff)
     *   - Flat through 200-800Hz
     *   - +3dB presence peak at 2.5kHz (speaker cone resonance)
     *   - Steep rolloff above 5kHz (-18dB/oct, speaker cone breakup)
     *
     * @param freq Frequency in Hz.
     * @return Linear magnitude at this frequency.
     */
    static double comboMagnitudeResponse(double freq) {
        if (freq < 1.0) return 0.0;

        // High-pass: 2nd order Butterworth shape below 80Hz
        double hpNorm = freq / 80.0;
        double hp = hpNorm * hpNorm / (1.0 + hpNorm * hpNorm);

        // Presence peak: resonant bump at 2.5kHz, Q ~= 2
        double pNorm = freq / 2500.0;
        double presence = 1.0 + 1.5 / (1.0 + 4.0 * (pNorm - 1.0 / pNorm) *
                                                     (pNorm - 1.0 / pNorm));

        // Low-pass: 3rd order rolloff above 5kHz (speaker HF limit)
        double lpNorm = freq / 5000.0;
        double lp = 1.0 / (1.0 + lpNorm * lpNorm * lpNorm);

        return hp * presence * lp;
    }

    /**
     * 4x12 British magnitude response curve.
     *
     * Designed to approximate a Marshall 1960A closed-back cabinet with
     * Celestion Greenback speakers.
     *
     * Key features:
     *   - Bass resonance bump at 120Hz (closed-back cabinet coupling)
     *   - Forward midrange peak at 1.5kHz (Greenback character)
     *   - Gradual rolloff above 4kHz
     *   - Overall warm, mid-forward character
     *
     * @param freq Frequency in Hz.
     * @return Linear magnitude at this frequency.
     */
    static double britishMagnitudeResponse(double freq) {
        if (freq < 1.0) return 0.0;

        // High-pass: gentler rolloff below 70Hz (4x12 has more bass extension)
        double hpNorm = freq / 70.0;
        double hp = hpNorm * hpNorm / (1.0 + hpNorm * hpNorm);

        // Bass resonance: bump at 120Hz from closed-back cabinet
        double bassNorm = freq / 120.0;
        double bassRes = 1.0 + 0.8 / (1.0 + 6.0 * (bassNorm - 1.0 / bassNorm) *
                                                     (bassNorm - 1.0 / bassNorm));

        // Mid presence: forward peak at 1.5kHz (Greenback character)
        double midNorm = freq / 1500.0;
        double midPeak = 1.0 + 1.2 / (1.0 + 5.0 * (midNorm - 1.0 / midNorm) *
                                                     (midNorm - 1.0 / midNorm));

        // Low-pass: moderate rolloff above 4kHz (warmer than combo)
        double lpNorm = freq / 4000.0;
        double lp = 1.0 / (1.0 + lpNorm * lpNorm * lpNorm);

        return hp * bassRes * midPeak * lp;
    }

    /**
     * 2x12 Open Back magnitude response curve.
     *
     * Designed to approximate a Vox AC30-style open-back cabinet with
     * Celestion Blue (Alnico) speakers.
     *
     * Key features:
     *   - Reduced bass due to open-back design (acoustic cancellation)
     *   - Scooped lower-midrange (characteristic Vox "chimey" tone)
     *   - Extended highs compared to closed-back designs
     *   - Overall balanced and articulate
     *
     * @param freq Frequency in Hz.
     * @return Linear magnitude at this frequency.
     */
    static double openBackMagnitudeResponse(double freq) {
        if (freq < 1.0) return 0.0;

        // High-pass: steeper rolloff below 100Hz (open-back loses bass)
        double hpNorm = freq / 100.0;
        double hp = hpNorm * hpNorm / (1.0 + hpNorm * hpNorm);

        // Mid scoop: dip around 400-600Hz (open-back acoustic cancellation)
        double scoopNorm = freq / 500.0;
        double scoop = 1.0 - 0.3 / (1.0 + 3.0 * (scoopNorm - 1.0 / scoopNorm) *
                                                   (scoopNorm - 1.0 / scoopNorm));

        // Presence: chimey peak at 3kHz (Alnico speaker character)
        double presNorm = freq / 3000.0;
        double presence = 1.0 + 1.0 / (1.0 + 4.0 * (presNorm - 1.0 / presNorm) *
                                                     (presNorm - 1.0 / presNorm));

        // Low-pass: extended top end, gentler rolloff above 6kHz
        double lpNorm = freq / 6000.0;
        double lp = 1.0 / (1.0 + lpNorm * lpNorm * lpNorm);

        return hp * scoop * presence * lp;
    }

    /**
     * Celestion G12M-25 "Greenback" magnitude response curve.
     *
     * Models the classic Greenback in a Marshall 1960A/B closed-back 4x12.
     * Warm, vintage character with a focused mid presence and earlier HF
     * rolloff than the stock British profile. The definitive classic rock
     * speaker: Hendrix, Clapton, early Van Halen.
     *
     * Key features:
     *   - HP below 75Hz (closed-back, tight bass coupling)
     *   - Resonance bump at 100Hz (cabinet coupling)
     *   - Focused mid presence at 2kHz, higher Q than British (more "vocal")
     *   - Sharp treble rolloff above 4kHz (25W paper cone breakup)
     *   - Overall warmer and darker than British, more mid-focused
     *
     * @param freq Frequency in Hz.
     * @return Linear magnitude at this frequency.
     */
    static double greenbackMagnitudeResponse(double freq) {
        if (freq < 1.0) return 0.0;

        // High-pass: 2nd order rolloff below 75Hz (closed-back cabinet)
        double hpNorm = freq / 75.0;
        double hp = hpNorm * hpNorm / (1.0 + hpNorm * hpNorm);

        // Cabinet resonance: bump at 100Hz from closed-back coupling
        double bassNorm = freq / 100.0;
        double bassRes = 1.0 + 0.7 / (1.0 + 5.0 * (bassNorm - 1.0 / bassNorm) *
                                                     (bassNorm - 1.0 / bassNorm));

        // Focused mid presence: peak at 2kHz, narrower Q than British (Q ~= 3)
        // This gives the "vocal" midrange that defines the Greenback tone
        double midNorm = freq / 2000.0;
        double midPeak = 1.0 + 1.8 / (1.0 + 8.0 * (midNorm - 1.0 / midNorm) *
                                                     (midNorm - 1.0 / midNorm));

        // Low-pass: sharp rolloff above 4kHz (25W cone = early HF breakup)
        // Steeper than British (4th order equivalent shape)
        double lpNorm = freq / 4000.0;
        double lp4 = lpNorm * lpNorm;
        double lp = 1.0 / (1.0 + lp4 * lp4);

        return hp * bassRes * midPeak * lp;
    }

    /**
     * Celestion G12H-30 magnitude response curve.
     *
     * Models the heavier-magnet G12H-30 in a Marshall closed-back 4x12.
     * Tighter low end and more prominent upper-mids than Greenback.
     * The "cutting" tone: Hendrix live, Angus Young, early AC/DC.
     *
     * Key features:
     *   - HP below 70Hz (tighter than Greenback due to heavier magnet)
     *   - Bass resonance at 110Hz (slightly higher than Greenback)
     *   - Strong upper-mid peak at 2.5kHz (brighter attack, more "bite")
     *   - Moderate rolloff above 5kHz (better HF extension than Greenback)
     *   - Overall: tighter, more aggressive, "cutting"
     *
     * @param freq Frequency in Hz.
     * @return Linear magnitude at this frequency.
     */
    static double g12h30MagnitudeResponse(double freq) {
        if (freq < 1.0) return 0.0;

        // High-pass: 2nd order rolloff below 70Hz (tight, controlled bass)
        double hpNorm = freq / 70.0;
        double hp = hpNorm * hpNorm / (1.0 + hpNorm * hpNorm);

        // Bass resonance: bump at 110Hz (heavier magnet = slightly higher)
        double bassNorm = freq / 110.0;
        double bassRes = 1.0 + 0.6 / (1.0 + 5.0 * (bassNorm - 1.0 / bassNorm) *
                                                     (bassNorm - 1.0 / bassNorm));

        // Upper-mid aggression: strong peak at 2.5kHz (brighter "bite")
        // Higher amplitude boost than Greenback for more cutting attack
        double midNorm = freq / 2500.0;
        double midPeak = 1.0 + 2.0 / (1.0 + 6.0 * (midNorm - 1.0 / midNorm) *
                                                     (midNorm - 1.0 / midNorm));

        // Low-pass: moderate rolloff above 5kHz (better HF than Greenback)
        double lpNorm = freq / 5000.0;
        double lp = 1.0 / (1.0 + lpNorm * lpNorm * lpNorm);

        return hp * bassRes * midPeak * lp;
    }

    /**
     * Celestion G12-65 magnitude response curve.
     *
     * Models the modern G12-65 in a Marshall closed-back 4x12.
     * Flatter, more "transparent" response that handles high power and
     * high gain without mushiness. EVH brown sound era.
     *
     * Key features:
     *   - HP below 65Hz (most bass extension of closed-back cabs)
     *   - Flat bass region (no pronounced resonance bump)
     *   - Broad, moderate mid peak at ~2kHz (gentle, wide presence)
     *   - Extended rolloff starting at ~5.5kHz (more modern highs)
     *   - Overall: flatter, more "transparent," studio-quality response
     *
     * @param freq Frequency in Hz.
     * @return Linear magnitude at this frequency.
     */
    static double g12_65MagnitudeResponse(double freq) {
        if (freq < 1.0) return 0.0;

        // High-pass: 2nd order rolloff below 65Hz (good bass extension)
        double hpNorm = freq / 65.0;
        double hp = hpNorm * hpNorm / (1.0 + hpNorm * hpNorm);

        // Minimal bass resonance: very slight bump at 90Hz (flat bottom end)
        double bassNorm = freq / 90.0;
        double bassRes = 1.0 + 0.2 / (1.0 + 4.0 * (bassNorm - 1.0 / bassNorm) *
                                                     (bassNorm - 1.0 / bassNorm));

        // Broad, gentle mid presence: wide peak at 2kHz, low Q (Q ~= 1)
        // This gives a subtle lift without coloring the tone aggressively
        double midNorm = freq / 2000.0;
        double midPeak = 1.0 + 0.8 / (1.0 + 2.0 * (midNorm - 1.0 / midNorm) *
                                                     (midNorm - 1.0 / midNorm));

        // Low-pass: extended rolloff above 5.5kHz (more modern, open top end)
        double lpNorm = freq / 5500.0;
        double lp = 1.0 / (1.0 + lpNorm * lpNorm * lpNorm);

        return hp * bassRes * midPeak * lp;
    }

    /**
     * Fane Crescendo 12" magnitude response curve (in WEM Starfinder cabinet).
     *
     * Models David Gilmour's speaker. Hi-fi, almost flat, extended HF
     * response. The most "transparent" cabinet profile -- Pink Floyd's
     * expansive clean tones and singing lead work.
     *
     * Key features:
     *   - HP below 60Hz (WEM cab = excellent bass extension)
     *   - Very flat midrange (200Hz-2kHz nearly ruler-flat)
     *   - Gentle, subtle presence peak at ~3.5kHz (not aggressive)
     *   - Gradual rolloff above 7kHz (much more HF than any Celestion)
     *   - Overall: hi-fi, detailed, articulate, open, "studio monitor-like"
     *
     * @param freq Frequency in Hz.
     * @return Linear magnitude at this frequency.
     */
    static double faneCrescendoMagnitudeResponse(double freq) {
        if (freq < 1.0) return 0.0;

        // High-pass: 2nd order rolloff below 60Hz (excellent bass extension)
        double hpNorm = freq / 60.0;
        double hp = hpNorm * hpNorm / (1.0 + hpNorm * hpNorm);

        // Flat midrange: very minimal resonance, near-unity through mids
        // Slight, subtle bump at 800Hz to avoid "scooped" character
        double midNorm = freq / 800.0;
        double midFlat = 1.0 + 0.1 / (1.0 + 2.0 * (midNorm - 1.0 / midNorm) *
                                                     (midNorm - 1.0 / midNorm));

        // Gentle presence: subtle peak at 3.5kHz (articulate, not harsh)
        double presNorm = freq / 3500.0;
        double presence = 1.0 + 0.5 / (1.0 + 3.0 * (presNorm - 1.0 / presNorm) *
                                                     (presNorm - 1.0 / presNorm));

        // Low-pass: very extended top end, gentle rolloff above 7kHz
        // 2nd order instead of 3rd to preserve more treble
        double lpNorm = freq / 7000.0;
        double lp = 1.0 / (1.0 + lpNorm * lpNorm);

        return hp * midFlat * presence * lp;
    }

    /**
     * Celestion Alnico Blue magnitude response curve (in Vox AC30).
     *
     * THE Vox speaker. Chimey, jangly, bright shimmer. More pronounced
     * chime than the generic openBack profile. Beatles, Brian May, Tom Petty.
     *
     * Key features:
     *   - HP below 100Hz (open-back = reduced bass coupling)
     *   - Deeper mid scoop at ~500Hz (open-back cancellation, more pronounced)
     *   - Prominent "chime" peak at 3kHz (Alnico magnet, higher Q = "glassy")
     *   - Extended top end, gradual rolloff above 6kHz
     *   - Overall: bright, chimey, "jangly," more defined than generic openBack
     *
     * Key difference from openBack (index 2): deeper scoop (-0.4 vs -0.3),
     * sharper chime peak (Q ~= 3 vs ~= 2), higher presence amplitude (+1.4
     * vs +1.0), giving a more pronounced "glassy" Alnico character.
     *
     * @param freq Frequency in Hz.
     * @return Linear magnitude at this frequency.
     */
    static double alnicoBlueMagnitudeResponse(double freq) {
        if (freq < 1.0) return 0.0;

        // High-pass: steeper rolloff below 100Hz (open-back reduces bass)
        double hpNorm = freq / 100.0;
        double hp = hpNorm * hpNorm / (1.0 + hpNorm * hpNorm);

        // Deeper mid scoop: pronounced dip at 500Hz (open-back cancellation)
        // More aggressive than openBack profile (0.4 vs 0.3 depth)
        double scoopNorm = freq / 500.0;
        double scoop = 1.0 - 0.4 / (1.0 + 4.0 * (scoopNorm - 1.0 / scoopNorm) *
                                                   (scoopNorm - 1.0 / scoopNorm));

        // Pronounced chime: sharp peak at 3kHz, higher Q for "glassy" tone
        // Higher amplitude (+1.4) and tighter Q (6.0) than openBack (+1.0, 4.0)
        double presNorm = freq / 3000.0;
        double chime = 1.0 + 1.4 / (1.0 + 6.0 * (presNorm - 1.0 / presNorm) *
                                                   (presNorm - 1.0 / presNorm));

        // Low-pass: extended top end, gentler rolloff above 6kHz
        double lpNorm = freq / 6000.0;
        double lp = 1.0 / (1.0 + lpNorm * lpNorm * lpNorm);

        return hp * scoop * chime * lp;
    }

    /**
     * Jensen C10Q 10" magnitude response curve (in Silvertone 1485).
     *
     * Jack White's speaker. Small 10" cone = limited bass, focused mids,
     * early HF breakup. The quintessential lo-fi, garage rock tone.
     * White Stripes, early blues recordings character.
     *
     * Key features:
     *   - HP below 120Hz (10" speaker = very limited bass extension)
     *   - No bass resonance (10" cone cannot develop low-freq coupling)
     *   - Focused midrange peak at ~1kHz (smaller cone = mid concentration)
     *   - Sharp rolloff above 3.5kHz (early cone breakup, small diameter)
     *   - Overall: tight, focused, "nasal," lo-fi, telephone-like character
     *
     * @param freq Frequency in Hz.
     * @return Linear magnitude at this frequency.
     */
    static double jensenC10qMagnitudeResponse(double freq) {
        if (freq < 1.0) return 0.0;

        // High-pass: steep rolloff below 120Hz (10" speaker, limited bass)
        // 3rd order shape for aggressive bass cutoff
        double hpNorm = freq / 120.0;
        double hp3 = hpNorm * hpNorm * hpNorm;
        double hp = hp3 / (1.0 + hp3);

        // Focused midrange: strong peak at 1kHz (small cone = mid concentration)
        // Higher Q (tight, nasal character) and strong amplitude
        double midNorm = freq / 1000.0;
        double midPeak = 1.0 + 2.2 / (1.0 + 7.0 * (midNorm - 1.0 / midNorm) *
                                                     (midNorm - 1.0 / midNorm));

        // Low-pass: very early, sharp rolloff above 3.5kHz
        // 4th order equivalent for aggressive HF cutoff (small cone breakup)
        double lpNorm = freq / 3500.0;
        double lp4 = lpNorm * lpNorm;
        double lp = 1.0 / (1.0 + lp4 * lp4);

        return hp * midPeak * lp;
    }

    // =========================================================================
    // FIR synthesis (windowed frequency sampling)
    // =========================================================================

    /**
     * Synthesize a time-domain IR from a magnitude response using inverse DFT.
     *
     * This implements the windowed frequency-sampling method for FIR filter design:
     *
     * 1. The input magnitude response defines |H(k)| at N frequency bins.
     *    We assume a zero-phase (linear-phase) response, so the phase is
     *    set to exp(-j * pi * k * (N-1) / N) which centers the impulse
     *    response in the window.
     *
     * 2. Inverse DFT: For a real, symmetric (type I linear phase) FIR,
     *    the IDFT simplifies to a cosine transform:
     *      h[n] = (1/N) * (H[0] + 2*sum_{k=1}^{N/2-1} H[k]*cos(2*pi*k*(n-c)/N)
     *                     + H[N/2]*cos(pi*(n-c)))
     *    where c = (N-1)/2 is the center of the filter.
     *
     * 3. Apply a Blackman window to reduce sidelobes and Gibbs ringing:
     *      w[n] = 0.42 - 0.5*cos(2*pi*n/(N-1)) + 0.08*cos(4*pi*n/(N-1))
     *    The Blackman window provides excellent sidelobe attenuation
     *    (-58dB) at the cost of slightly wider main lobe.
     *
     * 4. Normalize the IR so its peak absolute value is 1.0, preventing
     *    the convolution from amplifying or attenuating the signal overall.
     *
     * @param magResponse Array of magnitude values at N/2+1 frequency bins.
     * @param irOutput    Output array for the synthesized IR (length N).
     * @param N           IR length (must be even).
     */
    static void synthesizeIR(const double* magResponse, float* irOutput, int N) {
        const double center = static_cast<double>(N - 1) / 2.0;
        const int halfN = N / 2;

        // Step 1: Inverse DFT with assumed linear phase
        // For each time-domain sample n, sum the cosine contributions
        // from each frequency bin k, weighted by the magnitude response.
        for (int n = 0; n < N; ++n) {
            double sum = magResponse[0]; // DC component (k=0)

            // Frequency bins k=1 to N/2-1 (positive frequencies)
            for (int k = 1; k < halfN; ++k) {
                double angle = 2.0 * kPi * static_cast<double>(k) *
                               (static_cast<double>(n) - center) /
                               static_cast<double>(N);
                sum += 2.0 * magResponse[k] * std::cos(angle);
            }

            // Nyquist bin (k = N/2)
            double nyquistAngle = kPi * (static_cast<double>(n) - center);
            sum += magResponse[halfN] * std::cos(nyquistAngle);

            // Scale by 1/N (DFT normalization)
            irOutput[n] = static_cast<float>(sum / static_cast<double>(N));
        }

        // Step 2: Apply Blackman window
        // w[n] = 0.42 - 0.5*cos(2*pi*n/(N-1)) + 0.08*cos(4*pi*n/(N-1))
        for (int n = 0; n < N; ++n) {
            double nNorm = static_cast<double>(n) / static_cast<double>(N - 1);
            double window = 0.42 - 0.5 * std::cos(2.0 * kPi * nNorm)
                                 + 0.08 * std::cos(4.0 * kPi * nNorm);
            irOutput[n] *= static_cast<float>(window);
        }

        // Step 3: Normalize to unit peak amplitude
        float peak = 0.0f;
        for (int n = 0; n < N; ++n) {
            float absVal = std::abs(irOutput[n]);
            if (absVal > peak) peak = absVal;
        }
        if (peak > 0.0f) {
            float invPeak = 1.0f / peak;
            for (int n = 0; n < N; ++n) {
                irOutput[n] *= invPeak;
            }
        }
    }

    // =========================================================================
    // IR selection
    // =========================================================================

    /**
     * Select the active IR profile by cabinet type index.
     *
     * REAL-TIME SAFE: This method performs only pointer assignments and
     * a memset of the direct convolution buffer. All FFTConvolvers are
     * pre-initialized by initBuiltInConvolvers() (called from the
     * constructor and setSampleRate, both on non-audio threads).
     * No heap allocations occur here.
     *
     * The user-facing parameter indices are:
     *   0=combo, 1=british, 2=openBack, 3=user,
     *   4=greenback, 5=g12h30, 6=g12_65, 7=faneCrescendo,
     *   8=alnicoBlue, 9=jensenC10q
     *
     * The builtInConvolvers_ array is indexed 0-8 (9 built-in profiles).
     * Indices 0-2 map directly; index 3 is user IR (separate convolver);
     * indices 4-9 map to builtInConvolvers_[3-8].
     *
     * @param cabType User-facing cabinet type index (0-9).
     */
    void selectIR(int cabType) {
        const float* ir = nullptr;
        int irLen = 0;
        FFTConvolver* convolver = nullptr;

        switch (cabType) {
            case 0:  // 1x12 Combo
                ir = comboIR_.data();
                irLen = kBuiltInIRLength;
                convolver = &builtInConvolvers_[0];
                break;
            case 1:  // 4x12 British
                ir = britishIR_.data();
                irLen = kBuiltInIRLength;
                convolver = &builtInConvolvers_[1];
                break;
            case 2:  // 2x12 Open Back
                ir = openBackIR_.data();
                irLen = kBuiltInIRLength;
                convolver = &builtInConvolvers_[2];
                break;
            case 3:  // User IR
                // User IR (already loaded + initialized via loadUserIR)
                if (!userIR_.empty()) {
                    ir = userIR_.data();
                    irLen = userIRLength_;
                    if (userConvolver_.isInitialized()) {
                        convolver = &userConvolver_;
                    }
                }
                break;
            case 4:  // Greenback G12M-25
                ir = greenbackIR_.data();
                irLen = kBuiltInIRLength;
                convolver = &builtInConvolvers_[3];
                break;
            case 5:  // G12H-30
                ir = g12h30IR_.data();
                irLen = kBuiltInIRLength;
                convolver = &builtInConvolvers_[4];
                break;
            case 6:  // G12-65
                ir = g12_65IR_.data();
                irLen = kBuiltInIRLength;
                convolver = &builtInConvolvers_[5];
                break;
            case 7:  // Fane Crescendo
                ir = faneCrescendoIR_.data();
                irLen = kBuiltInIRLength;
                convolver = &builtInConvolvers_[6];
                break;
            case 8:  // Alnico Blue
                ir = alnicoBlueIR_.data();
                irLen = kBuiltInIRLength;
                convolver = &builtInConvolvers_[7];
                break;
            case 9:  // Jensen C10Q
                ir = jensenC10qIR_.data();
                irLen = kBuiltInIRLength;
                convolver = &builtInConvolvers_[8];
                break;
            default:
                break;
        }

        // Fall back to combo if no valid IR selected
        if (ir == nullptr) {
            ir = comboIR_.data();
            irLen = kBuiltInIRLength;
            convolver = &builtInConvolvers_[0];
        }

        activeIR_ = ir;
        activeIRLength_ = irLen;

        // Route to FFT or direct based on IR length and convolver availability
        if (convolver != nullptr && irLen >= kDirectConvolutionThreshold) {
            activeConvolver_ = convolver;
            useFFTConvolution_ = true;
        } else {
            activeConvolver_ = nullptr;
            useFFTConvolution_ = false;
        }

        // Reset the direct convolution circular buffer to prevent
        // stale data from leaking when switching between cabinets
        std::fill(circularBuffer_.begin(), circularBuffer_.end(), 0.0f);
        writePos_ = 0;
    }

    /**
     * Pre-initialize FFTConvolvers for all built-in cabinet IRs.
     *
     * Called from the constructor and setSampleRate() (both on non-audio
     * threads). After this, selectIR() can switch between cabinets with
     * just a pointer swap -- zero allocations on the audio thread.
     *
     * NOT real-time safe (allocates memory via FFTConvolver::init).
     */
    void initBuiltInConvolvers() {
        builtInConvolvers_[0].init(comboIR_.data(), kBuiltInIRLength,
                                   kFFTBlockSize);
        builtInConvolvers_[1].init(britishIR_.data(), kBuiltInIRLength,
                                   kFFTBlockSize);
        builtInConvolvers_[2].init(openBackIR_.data(), kBuiltInIRLength,
                                   kFFTBlockSize);
        builtInConvolvers_[3].init(greenbackIR_.data(), kBuiltInIRLength,
                                   kFFTBlockSize);
        builtInConvolvers_[4].init(g12h30IR_.data(), kBuiltInIRLength,
                                   kFFTBlockSize);
        builtInConvolvers_[5].init(g12_65IR_.data(), kBuiltInIRLength,
                                   kFFTBlockSize);
        builtInConvolvers_[6].init(faneCrescendoIR_.data(), kBuiltInIRLength,
                                   kFFTBlockSize);
        builtInConvolvers_[7].init(alnicoBlueIR_.data(), kBuiltInIRLength,
                                   kFFTBlockSize);
        builtInConvolvers_[8].init(jensenC10qIR_.data(), kBuiltInIRLength,
                                   kFFTBlockSize);
    }

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * Resample an IR from one sample rate to another using linear interpolation.
     *
     * For each output sample at position i, the corresponding position in the
     * source is computed as srcPos = i * srcRate / dstRate. The output value
     * is linearly interpolated between the two nearest source samples.
     *
     * Linear interpolation is sufficient for guitar cabinet IRs because:
     *   - Cabinet IRs are already heavily band-limited (steep rolloff above 5-6kHz)
     *   - The perceptual difference vs. sinc interpolation is negligible for
     *     short, band-limited signals
     *   - This runs on a non-audio thread, but we still want it to be fast for
     *     responsive UI
     *
     * The output length is capped at maxOutputLength to prevent excessively
     * long IRs when upsampling (e.g., 44.1kHz IR on a 96kHz device would
     * nearly double the length).
     *
     * @param input           Source IR samples.
     * @param inputLength     Number of samples in the source IR.
     * @param srcRate         Sample rate of the source IR in Hz.
     * @param dstRate         Target sample rate in Hz.
     * @param maxOutputLength Maximum allowed output length in samples.
     * @return Resampled IR as a vector. Empty if inputs are invalid.
     */
    static std::vector<float> resampleIR(const float* input, int inputLength,
                                         int srcRate, int dstRate,
                                         int maxOutputLength) {
        // Validate inputs
        if (input == nullptr || inputLength < 1 ||
            srcRate <= 0 || dstRate <= 0 || maxOutputLength < 1) {
            return {};
        }

        // No-op: rates match, just copy
        if (srcRate == dstRate) {
            int len = std::min(inputLength, maxOutputLength);
            return std::vector<float>(input, input + len);
        }

        // Compute output length: inputLength * (dstRate / srcRate), rounded.
        // Use double to avoid integer overflow for large IR lengths * rates.
        double ratio = static_cast<double>(dstRate) / static_cast<double>(srcRate);
        int outputLength = static_cast<int>(
            static_cast<double>(inputLength) * ratio + 0.5);

        // Clamp to valid range
        if (outputLength < 1) outputLength = 1;
        if (outputLength > maxOutputLength) outputLength = maxOutputLength;

        std::vector<float> output(outputLength);

        // The inverse ratio maps output positions back to source positions.
        // srcPos = i * srcRate / dstRate
        // Using double for accumulation to avoid floating-point drift over
        // thousands of samples.
        const double invRatio = static_cast<double>(srcRate) /
                                static_cast<double>(dstRate);
        const int lastSrcIdx = inputLength - 1;

        for (int i = 0; i < outputLength; ++i) {
            double srcPos = static_cast<double>(i) * invRatio;

            // Decompose into integer index and fractional part
            int idx = static_cast<int>(srcPos);
            float frac = static_cast<float>(srcPos - static_cast<double>(idx));

            // Clamp index to valid source range
            if (idx >= lastSrcIdx) {
                // At or beyond the last source sample -- use the last sample
                output[i] = input[lastSrcIdx];
            } else {
                // Linear interpolation: output = src[idx]*(1-frac) + src[idx+1]*frac
                output[i] = input[idx] * (1.0f - frac) + input[idx + 1] * frac;
            }
        }

        return output;
    }

    /** Clamp a float value to [minVal, maxVal]. */
    static float clamp(float value, float minVal, float maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    // =========================================================================
    // Parameters (written from UI thread, read from audio thread)
    // =========================================================================

    /** Cabinet type: 0-2 = original built-in, 3 = user IR, 4-9 = new built-in. */
    std::atomic<float> cabinetType_{0.0f};

    /** Wet/dry blend [0.0, 1.0]. */
    std::atomic<float> mix_{1.0f};

    /**
     * Mutex protecting user IR state during loadUserIR().
     *
     * Guards: userIR_, userIRLength_, activeIR_, activeIRLength_,
     * activeConvolver_, useFFTConvolution_, and userConvolver_.
     *
     * The audio thread (process()) uses try_lock(). If loadUserIR() holds
     * the lock, the audio thread skips cabinet processing for one callback
     * (~1-10ms) rather than blocking. This prevents use-after-free when
     * the user IR vector is reallocated.
     *
     * Built-in cabinet switching (selectIR for types 0-2, 4-9) does NOT need
     * this mutex because it only swaps pointers to pre-initialized arrays
     * that are never reallocated during processing.
     */
    std::mutex irMutex_;

    // =========================================================================
    // Audio thread state
    // =========================================================================

    int32_t sampleRate_ = 44100;

    /**
     * Circular buffer for direct convolution input history.
     *
     * Only used when useFFTConvolution_ is false (short IRs).
     * The FFT convolver has its own internal ring buffers.
     */
    std::array<float, kCircularBufferSize> circularBuffer_{};

    /** Current write position in the circular buffer. */
    int writePos_ = 0;

    /**
     * Pre-generated IR storage for each built-in cabinet type.
     * These are filled by generateAllIRs() during construction and
     * whenever the sample rate changes.
     */
    std::array<float, kBuiltInIRLength> comboIR_{};          // param 0: 1x12 Combo
    std::array<float, kBuiltInIRLength> britishIR_{};        // param 1: 4x12 British
    std::array<float, kBuiltInIRLength> openBackIR_{};       // param 2: 2x12 Open Back
    // param 3 = User IR (stored in userIR_ vector, not here)
    std::array<float, kBuiltInIRLength> greenbackIR_{};      // param 4: Greenback G12M-25
    std::array<float, kBuiltInIRLength> g12h30IR_{};         // param 5: G12H-30
    std::array<float, kBuiltInIRLength> g12_65IR_{};         // param 6: G12-65
    std::array<float, kBuiltInIRLength> faneCrescendoIR_{};  // param 7: Fane Crescendo
    std::array<float, kBuiltInIRLength> alnicoBlueIR_{};     // param 8: Alnico Blue
    std::array<float, kBuiltInIRLength> jensenC10qIR_{};     // param 9: Jensen C10Q

    /**
     * Pointer to the currently active IR data.
     * Points to one of the built-in IR arrays or a user-loaded IR.
     * Read by the audio thread in process(); updated by selectIR() which
     * is called from process() when the cabinetType parameter changes.
     */
    const float* activeIR_ = nullptr;

    /** Length of the currently active IR in samples. */
    int activeIRLength_ = 0;

    /** Tracks which cabinet type is currently loaded to detect changes. */
    int currentCabType_ = -1;

    // =========================================================================
    // FFT convolution state
    // =========================================================================

    /**
     * Pre-initialized FFT convolvers for each built-in cabinet type.
     * Initialized by initBuiltInConvolvers() during construction and
     * setSampleRate(). selectIR() switches between them via pointer swap.
     */
    FFTConvolver builtInConvolvers_[kNumCabinets];

    /** Separate convolver for user-loaded IRs. Initialized by loadUserIR(). */
    FFTConvolver userConvolver_;

    /**
     * Pointer to the currently active FFT convolver.
     * Points to one of builtInConvolvers_ or userConvolver_.
     * nullptr when using direct convolution (short user IRs).
     */
    FFTConvolver* activeConvolver_ = nullptr;

    /** True when the current IR uses FFT convolution, false for direct. */
    bool useFFTConvolution_ = false;

    /**
     * Temporary buffer for saving the dry signal before FFT processing.
     * Required because FFTConvolver::process() modifies the buffer in-place,
     * and we need the original dry signal for wet/dry mixing.
     */
    std::array<float, kMaxBufferSize> dryBuffer_{};

    // =========================================================================
    // User IR storage
    // =========================================================================

    /** User-loaded IR sample data. Populated by loadUserIR(). */
    std::vector<float> userIR_;

    /** Length of the user IR in samples. */
    int userIRLength_ = 0;
};

#endif // GUITAR_ENGINE_CABINET_SIM_H
