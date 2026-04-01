#ifndef GUITAR_ENGINE_TUNER_H
#define GUITAR_ENGINE_TUNER_H

#include "effect.h"
#include <pffft/pffft.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>

/**
 * Chromatic guitar tuner using the McLeod Pitch Method (MPM).
 *
 * This is NOT an audio effect -- it analyzes the input signal to detect
 * the fundamental frequency but passes audio through UNCHANGED.
 *
 * MPM algorithm (McLeod & Wyvill, 2005):
 *   1. Compute autocorrelation r(tau) via FFT (O(N log N))
 *   2. Type-II normalization: NSDF(tau) = 2*r(tau) / m(tau)
 *      where m(tau) = sum of x[j]^2 + x[j+tau]^2
 *   3. Find "key maxima" — peaks between positive zero crossings
 *   4. Threshold selection: k * highest_peak, take first qualifying max
 *   5. Parabolic interpolation for sub-sample accuracy
 *   6. Convert period (tau) to frequency: f = sampleRate / tau
 *
 * Why MPM over YIN for guitar:
 *   YIN's CMND produces DIPS (minima near 0). The "first dip below
 *   threshold" strategy finds harmonics before the fundamental on low
 *   guitar strings (E2, A2, D3) where the 2nd/3rd harmonics are
 *   physically stronger than the fundamental during attack.
 *
 *   MPM's NSDF produces PEAKS (maxima near +1). The fundamental's peak
 *   is typically the tallest due to Type-II normalization. The threshold
 *   is RELATIVE (k * max_peak), not absolute, so it adapts to signal
 *   strength automatically. The "first key maximum above threshold"
 *   strategy naturally selects the fundamental period.
 *
 * Stabilization pipeline (post-MPM):
 *   1. 3-sample median filter: reject single-frame frequency spikes
 *   2. Note hysteresis: require 3 consecutive same-note detections
 *   3. Cents computed relative to CONFIRMED note (not nearest raw note)
 *   4. EMA smoothing: alpha=0.15 for cents, alpha=0.15 for frequency
 *   5. Deadband: suppress updates when |delta| < 0.1 cents
 *
 * Parameter IDs for JNI access (READ-ONLY via getParameter):
 *   0 = detectedFrequency (Hz)
 *   1 = detectedNote (0-11: C, Db, D, Eb, E, F, Gb, G, Ab, A, Bb, B)
 *   2 = centsDeviation (-50 to +50 cents from nearest note)
 *   3 = confidence (0.0-1.0, where 1.0 = very confident detection)
 */
class Tuner : public Effect {
public:
    Tuner() = default;

    ~Tuner() {
        cleanupFft();
    }

    // Non-copyable (PFFFT resources are not copyable)
    Tuner(const Tuner&) = delete;
    Tuner& operator=(const Tuner&) = delete;

    /**
     * Pass audio through unchanged while accumulating samples for analysis.
     * Audio is NOT modified by the tuner.
     */
    void process(float* buffer, int numFrames) override {
        // Guard: analysis buffers not yet allocated (setSampleRate not called)
        if (analysisBuffer_.empty()) {
            return;
        }

        // Copy input samples to analysis buffer (does NOT modify buffer)
        const int space = kWindowSize - analysisWritePos_;
        const int toCopy = std::min(numFrames, space);
        if (toCopy > 0) {
            std::memcpy(&analysisBuffer_[analysisWritePos_], buffer,
                        toCopy * sizeof(float));
            analysisWritePos_ += toCopy;
        }

        // Run analysis when we have enough samples
        if (analysisWritePos_ >= kWindowSize) {
            // Skip the first few analysis windows after engine start.
            // The ADPF CPU governor needs ~500ms to ramp up frequency.
            // Running 8192-point FFTs before the governor has ramped causes
            // cache-cold FFT work at low CPU frequency → underruns.
            if (startupSkipCount_ > 0) {
                --startupSkipCount_;
            } else {
                detectPitch(analysisBuffer_.data(), kWindowSize);
            }

            // Shift for next analysis window (50% overlap) using memmove
            const int overlap = kWindowSize / 2;
            std::memmove(analysisBuffer_.data(),
                         analysisBuffer_.data() + overlap,
                         overlap * sizeof(float));
            analysisWritePos_ = overlap;
        }
    }

    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;
        analysisBuffer_.assign(kWindowSize, 0.0f);

        // Start the write position at 1/4 window to phase-offset analysis
        // relative to the spectrum analyzer (which also fires every 2048 samples).
        // Without this, both the tuner's 8192-pt FFT and the spectrum analyzer's
        // 2048-pt FFT fire on the SAME audio callback, creating a CPU spike.
        // The 1024-sample offset delays the tuner's first window completion by
        // ~21ms at 48kHz, breaking the phase lock permanently.
        analysisWritePos_ = kWindowSize / 4;

        // Skip the first 8 analysis windows (~680ms at 48kHz with 50% overlap).
        // This grace period allows the ADPF CPU governor to ramp up frequency
        // before we start running expensive 8192-point FFTs on 128KB+ of data.
        startupSkipCount_ = kStartupSkipWindows;

        // NSDF buffer (only need up to maxTau, but allocate halfWindow for safety)
        nsdf_.assign(kWindowSize / 2, 0.0f);

        // FFT setup for autocorrelation
        // Zero-pad to next power of 2 >= 2 * kWindowSize for linear
        // (non-circular) autocorrelation via FFT.
        cleanupFft();

        fftSize_ = 1;
        while (fftSize_ < 2 * kWindowSize) fftSize_ *= 2; // 8192

        pffftSetup_ = pffft_new_setup(fftSize_, PFFFT_REAL);
        fftIn_ = static_cast<float*>(
            pffft_aligned_malloc(static_cast<size_t>(fftSize_) * sizeof(float)));
        fftOut_ = static_cast<float*>(
            pffft_aligned_malloc(static_cast<size_t>(fftSize_) * sizeof(float)));
        fftWork_ = static_cast<float*>(
            pffft_aligned_malloc(static_cast<size_t>(fftSize_) * sizeof(float)));

        std::memset(fftIn_, 0, static_cast<size_t>(fftSize_) * sizeof(float));
        std::memset(fftOut_, 0, static_cast<size_t>(fftSize_) * sizeof(float));
        std::memset(fftWork_, 0, static_cast<size_t>(fftSize_) * sizeof(float));

        // Pre-warm PFFFT buffers: run a dummy forward+inverse transform to
        // pull the FFT twiddle factors and working memory into L1/L2 cache.
        // Without this, the first real FFT on the audio thread hits cold cache
        // lines (128KB+ of data), causing a ~2-5ms stall on the first callback
        // that uses the tuner. This runs on the setup thread (not audio thread),
        // so the memory stays warm by the time the first audio callback fires.
        if (pffftSetup_) {
            pffft_transform_ordered(pffftSetup_, fftIn_, fftOut_,
                                    fftWork_, PFFFT_FORWARD);
            pffft_transform_ordered(pffftSetup_, fftOut_, fftIn_,
                                    fftWork_, PFFFT_BACKWARD);
        }
    }

    /**
     * Prepare the tuner for re-activation after being inactive.
     *
     * Called from the JNI thread BEFORE tunerActive_ is set to true,
     * so no data race with the audio thread (which only calls process()
     * when tunerActive_ is true).
     *
     * Resets:
     *  - Skip counter: gives 340ms grace period for LatencyTuner to
     *    absorb the buffer boost before heavy FFT work begins
     *  - Analysis buffer: clears stale samples from last activation
     *  - Write position: restores 1/4 window phase offset with the
     *    spectrum analyzer to prevent simultaneous FFT spikes
     *
     * Does NOT reset EMA/median/hysteresis state — these converge
     * naturally after 3-4 analysis windows (~130ms).
     */
    void prepareForActivation() {
        // Shorter skip than cold-start: ADPF is already running and CPU is warm.
        // 2 windows (~84ms) is enough for the buffer boost to take effect.
        startupSkipCount_ = 2;
        analysisWritePos_ = kWindowSize / 4;
        std::fill(analysisBuffer_.begin(), analysisBuffer_.end(), 0.0f);
    }

    void reset() override {
        std::fill(analysisBuffer_.begin(), analysisBuffer_.end(), 0.0f);
        analysisWritePos_ = 0;
        startupSkipCount_ = kStartupSkipWindows;
        smoothedFreq_ = 0.0f;
        smoothedCents_ = 0.0f;
        smoothedConfidence_ = 0.0f;
        lastPublishedCents_ = 0.0f;
        confirmedNote_ = -1;
        confirmedMidiNote_ = -1;
        pendingNote_ = -1;
        pendingNoteCount_ = 0;
        octaveHoldCount_ = 0;
        medianIdx_ = 0;
        for (int i = 0; i < kMedianSize; ++i) medianBuf_[i] = 0.0f;
        detectedFreq_.store(0.0f, std::memory_order_relaxed);
        detectedNote_.store(0.0f, std::memory_order_relaxed);
        centsDeviation_.store(0.0f, std::memory_order_relaxed);
        confidence_.store(0.0f, std::memory_order_relaxed);
    }

    void setParameter(int /*paramId*/, float /*value*/) override {
        // Tuner has no writable parameters
    }

    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return detectedFreq_.load(std::memory_order_relaxed);
            case 1: return detectedNote_.load(std::memory_order_relaxed);
            case 2: return centsDeviation_.load(std::memory_order_relaxed);
            case 3: return confidence_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    // Analysis window size: 4096 samples gives 6+ cycles of Eb2 (77Hz)
    // for robust autocorrelation. Analysis latency ~85ms at 48kHz.
    static constexpr int kWindowSize = 4096;

    // MPM threshold constant (k): McLeod's recommended value.
    // The first key maximum of the NSDF that exceeds k * highest_peak
    // is selected as the pitch period. Higher k → more conservative
    // (fewer detections, higher accuracy). Lower k → more sensitive
    // (faster response, more octave risk). 0.93 is the paper's default.
    static constexpr float kMpmThreshold = 0.93f;

    // Minimum/maximum detectable frequencies
    static constexpr float kMinFreq = 60.0f;   // Below low E (82Hz) for drop tunings
    static constexpr float kMaxFreq = 1400.0f;  // Above high E 24th fret (~1319Hz)

    // EMA smoothing constants
    static constexpr float kCentsAlpha = 0.15f;
    static constexpr float kFreqAlpha = 0.15f;
    static constexpr float kConfAlpha = 0.30f;

    // Note hysteresis: require N consecutive same-note detections to switch.
    static constexpr int kNoteHoldCount = 3;

    // Confidence decay rate per analysis window when no pitch detected.
    static constexpr float kConfidenceDecay = 0.85f;

    // Deadband: suppress cents/freq atomic updates when change is below this.
    static constexpr float kCentsDeadband = 0.1f;

    // Median filter size for spike rejection
    static constexpr int kMedianSize = 3;

    // Maximum number of key maxima to track
    static constexpr int kMaxKeyMaxima = 32;

    // Number of analysis windows to skip after engine start.
    // Each window fires every ~42ms at 48kHz (2048 hop / 48000).
    // 8 windows = ~340ms grace period for ADPF CPU ramp-up.
    static constexpr int kStartupSkipWindows = 8;

    /**
     * Return the median of three floats (6 comparisons, no allocation).
     */
    static float median3(float a, float b, float c) {
        if (a > b) {
            if (b > c) return b;
            else if (a > c) return c;
            else return a;
        } else {
            if (a > c) return a;
            else if (b > c) return c;
            else return b;
        }
    }

    /**
     * Free PFFFT resources. Safe to call multiple times.
     */
    void cleanupFft() {
        if (pffftSetup_) { pffft_destroy_setup(pffftSetup_); pffftSetup_ = nullptr; }
        if (fftIn_) { pffft_aligned_free(fftIn_); fftIn_ = nullptr; }
        if (fftOut_) { pffft_aligned_free(fftOut_); fftOut_ = nullptr; }
        if (fftWork_) { pffft_aligned_free(fftWork_); fftWork_ = nullptr; }
    }

    /**
     * MPM pitch detection with FFT-accelerated autocorrelation
     * and multi-stage stabilization pipeline.
     *
     * The McLeod Pitch Method computes the Normalized Square Difference
     * Function (NSDF), which produces positive PEAKS at integer multiples
     * of the fundamental period. Unlike YIN's CMND (which produces DIPS),
     * the NSDF's Type-II normalization ensures the fundamental peak is
     * typically the TALLEST, and the relative threshold (k * max_peak)
     * naturally rejects harmonic peaks without needing heuristic guards.
     */
    void detectPitch(const float* buffer, int windowSize) {
        // Guard: FFT not initialized
        if (!pffftSetup_) return;

        const int minTau = static_cast<int>(
            static_cast<float>(sampleRate_) / kMaxFreq);
        const int maxTau = std::min(
            windowSize / 2 - 1,
            static_cast<int>(static_cast<float>(sampleRate_) / kMinFreq));

        if (maxTau <= minTau) return;

        // =====================================================================
        // Step 1: Autocorrelation via FFT (O(N log N))
        //
        // r(tau) = sum_{j=0}^{W-tau-1} x[j] * x[j+tau]
        //
        // Computed as: IFFT(|FFT(x_padded)|^2)
        // Zero-padding to fftSize >= 2*W ensures the circular autocorrelation
        // equals the linear autocorrelation for lags 0..W-1.
        // =====================================================================

        // Copy signal to FFT input buffer, zero-pad the rest
        std::memcpy(fftIn_, buffer, windowSize * sizeof(float));
        std::memset(fftIn_ + windowSize, 0,
                    (fftSize_ - windowSize) * sizeof(float));

        // Forward FFT
        pffft_transform_ordered(pffftSetup_, fftIn_, fftOut_,
                                fftWork_, PFFFT_FORWARD);

        // Power spectrum: |X[k]|^2
        // PFFFT ordered real format: [DC, Nyquist, Re1, Im1, Re2, Im2, ...]
        fftOut_[0] = fftOut_[0] * fftOut_[0]; // DC (real only)
        fftOut_[1] = fftOut_[1] * fftOut_[1]; // Nyquist (real only)
        for (int k = 1; k < fftSize_ / 2; ++k) {
            float re = fftOut_[2 * k];
            float im = fftOut_[2 * k + 1];
            fftOut_[2 * k] = re * re + im * im;
            fftOut_[2 * k + 1] = 0.0f;
        }

        // Inverse FFT → unnormalized autocorrelation
        // fftIn_[tau] = fftSize * r(tau) after this call
        pffft_transform_ordered(pffftSetup_, fftOut_, fftIn_,
                                fftWork_, PFFFT_BACKWARD);

        // =====================================================================
        // Step 2: Type-II normalization → NSDF
        //
        // NSDF(tau) = 2 * r(tau) / m(tau)
        //
        // where m(tau) = sum_{j=0}^{W-tau-1} (x[j]^2 + x[j+tau]^2)
        //
        // m(tau) is computed incrementally:
        //   m(0) = 2 * sum(x[j]^2)
        //   m(tau) = m(tau-1) - x[tau-1]^2 - x[W-tau]^2
        //
        // This normalization is why MPM is better than YIN for guitar:
        // as tau increases, m(tau) decreases (fewer terms), which BOOSTS
        // the NSDF value at the fundamental period relative to shorter-
        // period harmonics. The fundamental peak tends to be the tallest.
        // =====================================================================

        // Check for silence
        float sumSq = 0.0f;
        for (int j = 0; j < windowSize; ++j) {
            sumSq += buffer[j] * buffer[j];
        }
        if (sumSq < 1e-10f) {
            smoothedConfidence_ *= kConfidenceDecay;
            confidence_.store(smoothedConfidence_, std::memory_order_relaxed);
            return;
        }

        float mTau = 2.0f * sumSq;
        const float invN = 1.0f / static_cast<float>(fftSize_);

        nsdf_[0] = 1.0f;
        for (int tau = 1; tau <= maxTau; ++tau) {
            mTau -= buffer[tau - 1] * buffer[tau - 1]
                  + buffer[windowSize - tau] * buffer[windowSize - tau];

            if (mTau > 1e-10f) {
                // NSDF = 2 * r(tau) / m(tau)
                // fftIn_[tau] = fftSize * r(tau), so r(tau) = fftIn_[tau] / fftSize
                nsdf_[tau] = 2.0f * fftIn_[tau] * invN / mTau;
            } else {
                nsdf_[tau] = 0.0f;
            }
        }

        // =====================================================================
        // Step 3: Find key maxima
        //
        // A "key maximum" is the highest NSDF peak between consecutive
        // positive-going zero crossings. These correspond to candidate
        // pitch periods (fundamental and its harmonics).
        // =====================================================================

        struct KeyMax { int tau; float value; };
        KeyMax keyMaxima[kMaxKeyMaxima];
        int numKeyMax = 0;

        bool inPositiveRegion = false;
        float curMax = -1.0f;
        int curMaxTau = -1;

        for (int tau = minTau; tau <= maxTau; ++tau) {
            if (nsdf_[tau] > 0.0f) {
                if (!inPositiveRegion) {
                    // Entered a new positive lobe
                    inPositiveRegion = true;
                    curMax = nsdf_[tau];
                    curMaxTau = tau;
                } else if (nsdf_[tau] > curMax) {
                    curMax = nsdf_[tau];
                    curMaxTau = tau;
                }
            } else {
                if (inPositiveRegion && numKeyMax < kMaxKeyMaxima) {
                    // Left a positive lobe — record the peak
                    keyMaxima[numKeyMax++] = {curMaxTau, curMax};
                    inPositiveRegion = false;
                    curMax = -1.0f;
                    curMaxTau = -1;
                }
            }
        }
        // Capture the last positive region if it extends to maxTau
        if (inPositiveRegion && curMaxTau >= 0 && numKeyMax < kMaxKeyMaxima) {
            keyMaxima[numKeyMax++] = {curMaxTau, curMax};
        }

        if (numKeyMax == 0) {
            smoothedConfidence_ *= kConfidenceDecay;
            confidence_.store(smoothedConfidence_, std::memory_order_relaxed);
            return;
        }

        // =====================================================================
        // Step 4: Threshold selection (core of MPM)
        //
        // Find the highest key maximum (strongest periodicity signal).
        // Set threshold = k * highest_peak. Select the FIRST key maximum
        // that meets or exceeds this threshold.
        //
        // Why this works for guitar low strings:
        //   - The fundamental's NSDF peak is typically the tallest due to
        //     Type-II normalization boosting longer periods.
        //   - Even if a harmonic peak is slightly taller (rare), the
        //     threshold (93% of max) means the fundamental peak usually
        //     still qualifies. And harmonics at shorter periods (lower
        //     indices in keyMaxima) are checked first — but we want the
        //     FIRST qualifying peak, which for MPM corresponds to the
        //     shortest period (highest frequency) that qualifies.
        //
        // Wait — MPM's "first" key maximum is the one at the SHORTEST
        // period (lowest tau). For guitar, this might be a harmonic.
        // But the threshold gates it: harmonic peaks are typically
        // 60-85% of the fundamental peak height, and k=0.93 requires
        // 93% of the highest. So harmonics are rejected.
        // =====================================================================

        float highestPeak = 0.0f;
        for (int i = 0; i < numKeyMax; ++i) {
            if (keyMaxima[i].value > highestPeak) {
                highestPeak = keyMaxima[i].value;
            }
        }

        float threshold = kMpmThreshold * highestPeak;

        int tauEstimate = -1;
        float bestNsdf = 0.0f;
        for (int i = 0; i < numKeyMax; ++i) {
            if (keyMaxima[i].value >= threshold) {
                tauEstimate = keyMaxima[i].tau;
                bestNsdf = keyMaxima[i].value;
                break;
            }
        }

        if (tauEstimate < 0) {
            smoothedConfidence_ *= kConfidenceDecay;
            confidence_.store(smoothedConfidence_, std::memory_order_relaxed);
            return;
        }

        // Confidence from NSDF peak value (0.0 to 1.0)
        float rawConf = std::max(0.0f, std::min(1.0f, bestNsdf));

        // =====================================================================
        // Step 5: Parabolic interpolation for sub-sample accuracy
        //
        // Fit a parabola through NSDF[tau-1], NSDF[tau], NSDF[tau+1] to
        // find the true peak location between integer samples.
        //
        // IMPORTANT: Do NOT use std::isfinite() here. The -ffast-math flag
        // makes the compiler assume all values are finite.
        // =====================================================================

        float betterTau = static_cast<float>(tauEstimate);
        if (tauEstimate > minTau && tauEstimate < maxTau) {
            float s0 = nsdf_[tauEstimate - 1];
            float s1 = nsdf_[tauEstimate];
            float s2 = nsdf_[tauEstimate + 1];
            float denom = s0 - 2.0f * s1 + s2;
            // For a peak (maximum), denom < 0 (concave down).
            // Guard against near-zero denominator (flat region).
            if (denom < -1e-9f || denom > 1e-9f) {
                float adjustment = (s0 - s2) / (2.0f * denom);
                if (adjustment > -1.0f && adjustment < 1.0f) {
                    betterTau += adjustment;
                }
            }
        }

        // Step 6: Convert period to frequency
        if (betterTau <= 0.0f) return;
        float rawFreq = static_cast<float>(sampleRate_) / betterTau;

        // =================================================================
        // Stabilization pipeline (unchanged from previous implementation)
        // =================================================================

        // --- 3-sample median filter ---
        if (medianIdx_ == 0) {
            medianBuf_[0] = medianBuf_[1] = medianBuf_[2] = rawFreq;
            medianIdx_ = kMedianSize;
        } else {
            medianBuf_[medianIdx_ % kMedianSize] = rawFreq;
            ++medianIdx_;
            if (medianIdx_ > 1000000) {
                medianIdx_ = kMedianSize + (medianIdx_ % kMedianSize);
            }
        }
        float filteredFreq = median3(
            medianBuf_[0], medianBuf_[1], medianBuf_[2]);

        // --- MIDI note from filtered frequency ---
        float noteNum = 12.0f * std::log2(filteredFreq / 440.0f) + 69.0f;
        int nearestMidi = static_cast<int>(std::round(noteNum));
        int noteName = nearestMidi % 12;
        if (noteName < 0) noteName += 12;

        // --- Note hysteresis ---
        if (noteName == pendingNote_) {
            ++pendingNoteCount_;
        } else {
            pendingNote_ = noteName;
            pendingNoteCount_ = 1;
        }

        bool noteChanged = false;
        if (confirmedNote_ < 0) {
            confirmedNote_ = noteName;
            confirmedMidiNote_ = nearestMidi;
            noteChanged = true;
        } else if (pendingNoteCount_ >= kNoteHoldCount &&
                   noteName != confirmedNote_) {
            confirmedNote_ = noteName;
            confirmedMidiNote_ = nearestMidi;
            noteChanged = true;
        }

        // --- Octave tracking with hysteresis ---
        bool octaveMismatch = false;
        if (!noteChanged && noteName == confirmedNote_) {
            if (nearestMidi != confirmedMidiNote_) {
                ++octaveHoldCount_;
                if (octaveHoldCount_ >= kNoteHoldCount) {
                    confirmedMidiNote_ = nearestMidi;
                    octaveHoldCount_ = 0;
                } else {
                    octaveMismatch = true;
                }
            } else {
                octaveHoldCount_ = 0;
            }
        }

        // --- Cents relative to CONFIRMED note ---
        float centsFromConfirmed =
            (noteNum - static_cast<float>(confirmedMidiNote_)) * 100.0f;
        centsFromConfirmed = std::max(-50.0f, std::min(50.0f, centsFromConfirmed));

        // --- EMA smoothing ---
        if (noteChanged || smoothedFreq_ <= 0.0f) {
            smoothedFreq_ = filteredFreq;
            smoothedCents_ = centsFromConfirmed;
            lastPublishedCents_ = centsFromConfirmed;
        } else if (!octaveMismatch) {
            smoothedFreq_ = kFreqAlpha * filteredFreq
                          + (1.0f - kFreqAlpha) * smoothedFreq_;
            smoothedCents_ = kCentsAlpha * centsFromConfirmed
                           + (1.0f - kCentsAlpha) * smoothedCents_;
        } else {
            // Octave mismatch: update frequency only, skip cents
            smoothedFreq_ = kFreqAlpha * filteredFreq
                          + (1.0f - kFreqAlpha) * smoothedFreq_;
        }

        // Smooth confidence
        smoothedConfidence_ = kConfAlpha * rawConf
                            + (1.0f - kConfAlpha) * smoothedConfidence_;

        // --- Deadband ---
        float centsToPublish = lastPublishedCents_;
        if (std::abs(smoothedCents_ - lastPublishedCents_) > kCentsDeadband) {
            centsToPublish = smoothedCents_;
            lastPublishedCents_ = smoothedCents_;
        }

        // Publish results to atomic outputs
        detectedFreq_.store(smoothedFreq_, std::memory_order_relaxed);
        detectedNote_.store(static_cast<float>(confirmedNote_),
                            std::memory_order_relaxed);
        centsDeviation_.store(centsToPublish, std::memory_order_relaxed);
        confidence_.store(smoothedConfidence_, std::memory_order_relaxed);
    }

    int32_t sampleRate_ = 44100;

    // Analysis buffers (pre-allocated in setSampleRate)
    std::vector<float> analysisBuffer_;
    std::vector<float> nsdf_;
    int analysisWritePos_ = 0;
    int startupSkipCount_ = kStartupSkipWindows;

    // FFT resources for autocorrelation (PFFFT aligned memory)
    int fftSize_ = 0;
    PFFFT_Setup* pffftSetup_ = nullptr;
    float* fftIn_ = nullptr;
    float* fftOut_ = nullptr;
    float* fftWork_ = nullptr;

    // 3-sample median filter buffer (audio thread only)
    float medianBuf_[kMedianSize] = {};
    int medianIdx_ = 0;

    // EMA smoothing state (audio thread only, not shared)
    float smoothedFreq_ = 0.0f;
    float smoothedCents_ = 0.0f;
    float smoothedConfidence_ = 0.0f;
    float lastPublishedCents_ = 0.0f;

    // Note hysteresis state (audio thread only)
    int confirmedNote_ = -1;
    int confirmedMidiNote_ = -1;
    int pendingNote_ = -1;
    int pendingNoteCount_ = 0;
    int octaveHoldCount_ = 0;

    // Results (written by audio thread, read by UI via getParameter)
    std::atomic<float> detectedFreq_{0.0f};
    std::atomic<float> detectedNote_{0.0f};
    std::atomic<float> centsDeviation_{0.0f};
    std::atomic<float> confidence_{0.0f};
};

#endif // GUITAR_ENGINE_TUNER_H
