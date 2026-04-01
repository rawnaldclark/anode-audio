#ifndef GUITAR_ENGINE_DRUM_BUS_H
#define GUITAR_ENGINE_DRUM_BUS_H

#include "../effects/fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>
#include <cstring>

/**
 * Lightweight stereo bus processing chain for the drum mix.
 *
 * Signal flow: Compressor -> 3-Band EQ -> Schroeder Reverb
 *
 * Each stage processes stereo audio in-place. The compressor uses linked
 * stereo detection (same gain applied to L and R). The EQ uses the same
 * biquad coefficients for both channels but maintains independent filter
 * state. The reverb is a simple Schroeder design (4 parallel comb filters
 * + 2 series allpass filters) with wet/dry mix.
 *
 * All parameters are std::atomic<float> for lock-free UI<->audio thread
 * communication. Biquad coefficients are pre-computed on parameter change
 * (in the setter, not per-sample). No heap allocation in processStereo().
 *
 * Namespace: drums::
 * Header-only, C++17, real-time safe.
 */
namespace drums {

class DrumBus {
public:
    DrumBus() = default;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * Set the sample rate and pre-compute all derived constants.
     * Must be called before processStereo(). Not real-time safe (allocates).
     *
     * @param sr Sample rate in Hz (e.g., 44100, 48000).
     */
    void setSampleRate(int32_t sr) {
        sampleRate_ = sr;
        invSampleRate_ = 1.0 / static_cast<double>(sr);

        // Recompute compressor envelope coefficients
        recomputeCompAttack(compAttackMs_.load(std::memory_order_relaxed));
        recomputeCompRelease(compReleaseMs_.load(std::memory_order_relaxed));

        // Recompute all EQ biquad coefficients
        recomputeLowShelf();
        recomputeMid();
        recomputeHighShelf();

        // Scale Schroeder comb/allpass delay lengths from 44100 Hz reference
        const double scale = static_cast<double>(sr) / 44100.0;

        // Comb filter delays (Schroeder standard values at 44100 Hz)
        static constexpr int kCombDelaysRef[kNumCombs] = { 1116, 1188, 1277, 1356 };
        for (int i = 0; i < kNumCombs; ++i) {
            combDelay_[i] = std::max(1, static_cast<int>(kCombDelaysRef[i] * scale));
            if (combDelay_[i] >= kMaxDelay) combDelay_[i] = kMaxDelay - 1;
        }

        // Allpass filter delays (Schroeder standard values at 44100 Hz)
        static constexpr int kAllpassDelaysRef[kNumAllpass] = { 225, 556 };
        for (int i = 0; i < kNumAllpass; ++i) {
            allpassDelay_[i] = std::max(1, static_cast<int>(kAllpassDelaysRef[i] * scale));
            if (allpassDelay_[i] >= kMaxDelay) allpassDelay_[i] = kMaxDelay - 1;
        }

        // Recompute reverb feedback from decay param
        recomputeReverbFeedback();

        reset();
    }

    /**
     * Reset all internal state (filter memories, delay lines, envelopes).
     * Call after setSampleRate() or when clearing audio state.
     */
    void reset() {
        // Compressor state
        rmsStateL_ = 0.0;
        rmsStateR_ = 0.0;
        envState_ = 0.0f;

        // EQ filter state (both channels, 3 bands)
        std::memset(eqStateL_, 0, sizeof(eqStateL_));
        std::memset(eqStateR_, 0, sizeof(eqStateR_));

        // Reverb delay lines
        for (int c = 0; c < kNumCombs; ++c) {
            std::memset(combBufL_[c], 0, sizeof(combBufL_[c]));
            std::memset(combBufR_[c], 0, sizeof(combBufR_[c]));
            combPosL_[c] = 0;
            combPosR_[c] = 0;
            combDampL_[c] = 0.0f;
            combDampR_[c] = 0.0f;
        }
        for (int a = 0; a < kNumAllpass; ++a) {
            std::memset(allpassBufL_[a], 0, sizeof(allpassBufL_[a]));
            std::memset(allpassBufR_[a], 0, sizeof(allpassBufR_[a]));
            allpassPosL_[a] = 0;
            allpassPosR_[a] = 0;
        }
    }

    // =========================================================================
    // Main processing
    // =========================================================================

    /**
     * Process stereo drum mix in-place through the bus chain.
     * Real-time safe: no allocations, no locks, no syscalls.
     *
     * @param bufL   Left channel buffer (numFrames samples, modified in-place).
     * @param bufR   Right channel buffer (numFrames samples, modified in-place).
     * @param numFrames Number of audio frames to process.
     */
    void processStereo(float* bufL, float* bufR, int numFrames) {
        if (!enabled_.load(std::memory_order_relaxed)) return;

        processCompressor(bufL, bufR, numFrames);
        processEQ(bufL, bufR, numFrames);
        processReverb(bufL, bufR, numFrames);
    }

    // =========================================================================
    // Compressor parameters
    // =========================================================================

    /**
     * Set compressor threshold in dB (0 to -40 dB).
     * 0 dB = no compression, -40 dB = aggressive compression.
     */
    void setCompThreshold(float dB) {
        dB = clamp(dB, -40.0f, 0.0f);
        compThresholdDb_.store(dB, std::memory_order_relaxed);
        // Convert dB to linear amplitude for per-sample comparison
        compThresholdLin_.store(std::pow(10.0f, dB / 20.0f),
                                std::memory_order_relaxed);
    }

    /** Set compressor ratio (1:1 = no compression, 20:1 = hard limiting). */
    void setCompRatio(float ratio) {
        compRatio_.store(clamp(ratio, 1.0f, 20.0f),
                         std::memory_order_relaxed);
    }

    /** Set compressor attack time in milliseconds (0.1 - 100 ms). */
    void setCompAttack(float ms) {
        ms = clamp(ms, 0.1f, 100.0f);
        compAttackMs_.store(ms, std::memory_order_relaxed);
        recomputeCompAttack(ms);
    }

    /** Set compressor release time in milliseconds (10 - 500 ms). */
    void setCompRelease(float ms) {
        ms = clamp(ms, 10.0f, 500.0f);
        compReleaseMs_.store(ms, std::memory_order_relaxed);
        recomputeCompRelease(ms);
    }

    /** Set compressor makeup gain in dB (0 - 20 dB). */
    void setCompMakeup(float dB) {
        dB = clamp(dB, 0.0f, 20.0f);
        compMakeupLin_.store(std::pow(10.0f, dB / 20.0f),
                             std::memory_order_relaxed);
    }

    // =========================================================================
    // EQ parameters
    // =========================================================================

    /** Set low shelf gain in dB (-12 to +12 dB) at ~200 Hz. */
    void setEqLowGain(float dB) {
        eqLowGainDb_.store(clamp(dB, -12.0f, 12.0f),
                           std::memory_order_relaxed);
        recomputeLowShelf();
    }

    /** Set parametric mid gain in dB (-12 to +12 dB). */
    void setEqMidGain(float dB) {
        eqMidGainDb_.store(clamp(dB, -12.0f, 12.0f),
                           std::memory_order_relaxed);
        recomputeMid();
    }

    /** Set parametric mid center frequency in Hz (200 - 5000 Hz). */
    void setEqMidFreq(float hz) {
        eqMidFreq_.store(clamp(hz, 200.0f, 5000.0f),
                         std::memory_order_relaxed);
        recomputeMid();
    }

    /** Set parametric mid Q (0.5 - 4.0). */
    void setEqMidQ(float q) {
        eqMidQ_.store(clamp(q, 0.5f, 4.0f), std::memory_order_relaxed);
        recomputeMid();
    }

    /** Set high shelf gain in dB (-12 to +12 dB) at ~8 kHz. */
    void setEqHighGain(float dB) {
        eqHighGainDb_.store(clamp(dB, -12.0f, 12.0f),
                            std::memory_order_relaxed);
        recomputeHighShelf();
    }

    // =========================================================================
    // Reverb parameters
    // =========================================================================

    /** Set reverb wet/dry mix (0.0 = fully dry, 1.0 = fully wet). */
    void setReverbMix(float wet) {
        reverbMix_.store(clamp(wet, 0.0f, 1.0f),
                         std::memory_order_relaxed);
    }

    /** Set reverb decay (0.0 - 1.0, controls comb filter feedback). */
    void setReverbDecay(float decay) {
        reverbDecay_.store(clamp(decay, 0.0f, 1.0f),
                           std::memory_order_relaxed);
        recomputeReverbFeedback();
    }

    /**
     * Set reverb damping (0.0 - 1.0).
     * Higher values = more high-frequency absorption in comb feedback.
     */
    void setReverbDamping(float damp) {
        reverbDamping_.store(clamp(damp, 0.0f, 1.0f),
                             std::memory_order_relaxed);
    }

    // =========================================================================
    // Global enable/disable
    // =========================================================================

    void setEnabled(bool enabled) {
        enabled_.store(enabled, std::memory_order_relaxed);
    }

    bool isEnabled() const {
        return enabled_.load(std::memory_order_relaxed);
    }

private:
    // =========================================================================
    // Constants
    // =========================================================================

    static constexpr int kNumCombs = 4;
    static constexpr int kNumAllpass = 2;
    static constexpr int kMaxDelay = 4096;    ///< Max delay line length (samples)
    static constexpr float kAllpassG = 0.5f;  ///< Allpass coefficient (Schroeder)

    // EQ band indices
    static constexpr int kBandLow = 0;
    static constexpr int kBandMid = 1;
    static constexpr int kBandHigh = 2;
    static constexpr int kNumBands = 3;

    // =========================================================================
    // Biquad coefficient storage (5 coefficients per band: b0, b1, b2, a1, a2)
    // Normalized form: a0 is always 1.0 (pre-divided).
    // =========================================================================

    struct BiquadCoeffs {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
    };

    // Per-channel biquad state: two delay elements (Direct Form II Transposed)
    struct BiquadState {
        double z1 = 0.0, z2 = 0.0;
    };

    // =========================================================================
    // Compressor implementation
    // =========================================================================

    /**
     * RMS-based linked stereo compressor.
     *
     * Detection: RMS level computed from max(|L|, |R|) using IIR smoothing.
     * The attack/release coefficients control how fast the detector responds.
     * Gain reduction is computed in dB domain for correct ratio behavior.
     * The same gain is applied to both channels (linked stereo).
     */
    void processCompressor(float* bufL, float* bufR, int numFrames) {
        const float threshold = compThresholdLin_.load(std::memory_order_relaxed);
        const float thresholdDb = compThresholdDb_.load(std::memory_order_relaxed);
        const float ratio = compRatio_.load(std::memory_order_relaxed);
        const float attackCoeff = compAttackCoeff_.load(std::memory_order_relaxed);
        const float releaseCoeff = compReleaseCoeff_.load(std::memory_order_relaxed);
        const float makeupLin = compMakeupLin_.load(std::memory_order_relaxed);

        // Skip processing if no compression would occur (threshold at 0 dB, ratio 1:1)
        if (thresholdDb >= 0.0f && ratio <= 1.01f) return;

        for (int i = 0; i < numFrames; ++i) {
            // Linked stereo: use max of L/R absolute values for detection
            const float absL = std::abs(bufL[i]);
            const float absR = std::abs(bufR[i]);
            const float peak = std::max(absL, absR);

            // IIR RMS detector: rms^2 = alpha * sample^2 + (1-alpha) * prevRms^2
            // Use attack coeff when signal rising, release when falling
            const double squared = static_cast<double>(peak) * peak;
            const float alpha = (squared > rmsStateL_) ? attackCoeff : releaseCoeff;
            rmsStateL_ = alpha * squared + (1.0f - alpha) * rmsStateL_;
            rmsStateL_ = fast_math::denormal_guard(rmsStateL_);

            const float rms = static_cast<float>(std::sqrt(rmsStateL_));

            // Compute gain reduction in dB domain
            float gainLin = 1.0f;
            if (rms > threshold && rms > 1e-10f) {
                // Convert to dB
                const float rmsDb = 20.0f * fast_math::fastLog10(rms);
                // How far above threshold (positive value)
                const float overDb = rmsDb - thresholdDb;
                // Compressed level = threshold + overshoot / ratio
                const float compressedDb = thresholdDb + overDb / ratio;
                // Gain reduction = desired level - actual level
                const float reductionDb = compressedDb - rmsDb;
                // Convert back to linear
                gainLin = fast_math::exp2_approx(reductionDb * 0.16609640474f);
                // 1/20 * 1/log10(2) = 0.16609640474 (dB to log2 scale)
            }

            // Smooth the gain envelope to avoid zipper noise
            const float envAlpha = (gainLin < envState_) ? attackCoeff : releaseCoeff;
            envState_ = envAlpha * gainLin + (1.0f - envAlpha) * envState_;
            envState_ = fast_math::denormal_guard(envState_);

            // Apply gain + makeup
            const float finalGain = envState_ * makeupLin;
            bufL[i] *= finalGain;
            bufR[i] *= finalGain;
        }
    }

    // =========================================================================
    // 3-Band EQ implementation
    // =========================================================================

    /**
     * Process stereo audio through 3 cascaded biquad filters.
     * Same coefficients for both channels, independent state per channel.
     */
    void processEQ(float* bufL, float* bufR, int numFrames) {
        // Snapshot coefficients (atomic-safe: each BiquadCoeffs is small
        // and written atomically from the setters via recompute functions)
        BiquadCoeffs bands[kNumBands];
        bands[kBandLow] = eqCoeffs_[kBandLow];
        bands[kBandMid] = eqCoeffs_[kBandMid];
        bands[kBandHigh] = eqCoeffs_[kBandHigh];

        // Check if EQ is effectively flat (all gains near 0 dB)
        const bool lowFlat = std::abs(eqLowGainDb_.load(std::memory_order_relaxed)) < 0.1f;
        const bool midFlat = std::abs(eqMidGainDb_.load(std::memory_order_relaxed)) < 0.1f;
        const bool highFlat = std::abs(eqHighGainDb_.load(std::memory_order_relaxed)) < 0.1f;
        if (lowFlat && midFlat && highFlat) return;

        for (int i = 0; i < numFrames; ++i) {
            float sL = bufL[i];
            float sR = bufR[i];

            // Process each band in series
            for (int b = 0; b < kNumBands; ++b) {
                sL = processBiquad(bands[b], eqStateL_[b], sL);
                sR = processBiquad(bands[b], eqStateR_[b], sR);
            }

            bufL[i] = sL;
            bufR[i] = sR;
        }
    }

    /**
     * Process one sample through a biquad filter (Direct Form II Transposed).
     * Uses double-precision state for stability in feedback paths.
     */
    static inline float processBiquad(const BiquadCoeffs& c, BiquadState& s,
                                       float input) {
        const double in = static_cast<double>(input);
        const double out = c.b0 * in + s.z1;
        s.z1 = c.b1 * in - c.a1 * out + s.z2;
        s.z2 = c.b2 * in - c.a2 * out;
        s.z1 = fast_math::denormal_guard(s.z1);
        s.z2 = fast_math::denormal_guard(s.z2);
        return static_cast<float>(out);
    }

    // =========================================================================
    // Biquad coefficient computation (called from setters, NOT per-sample)
    // =========================================================================

    /**
     * Compute low shelf biquad coefficients at 200 Hz.
     *
     * Using Robert Bristow-Johnson's Audio EQ Cookbook formulas.
     * Low shelf: boosts/cuts frequencies below the shelf frequency.
     */
    void recomputeLowShelf() {
        const float gainDb = eqLowGainDb_.load(std::memory_order_relaxed);
        constexpr double freq = 200.0;
        computeShelfCoeffs(eqCoeffs_[kBandLow], freq, gainDb, true);
    }

    /**
     * Compute parametric mid biquad coefficients.
     * Peaking EQ with adjustable frequency and Q.
     */
    void recomputeMid() {
        const float gainDb = eqMidGainDb_.load(std::memory_order_relaxed);
        const float freq = eqMidFreq_.load(std::memory_order_relaxed);
        const float Q = eqMidQ_.load(std::memory_order_relaxed);
        computePeakingCoeffs(eqCoeffs_[kBandMid], static_cast<double>(freq),
                             gainDb, static_cast<double>(Q));
    }

    /**
     * Compute high shelf biquad coefficients at 8000 Hz.
     */
    void recomputeHighShelf() {
        const float gainDb = eqHighGainDb_.load(std::memory_order_relaxed);
        constexpr double freq = 8000.0;
        computeShelfCoeffs(eqCoeffs_[kBandHigh], freq, gainDb, false);
    }

    /**
     * Compute shelf filter coefficients (low or high shelf).
     *
     * Based on RBJ Audio EQ Cookbook:
     *   A  = sqrt(10^(dBgain/20)) = 10^(dBgain/40)
     *   w0 = 2*pi*f0/Fs
     *   alpha = sin(w0)/2 * sqrt((A + 1/A)*(1/S - 1) + 2), S=1 (slope)
     *
     * @param c       Output coefficient struct.
     * @param freq    Shelf frequency in Hz.
     * @param gainDb  Gain in dB (-12 to +12).
     * @param isLow   true = low shelf, false = high shelf.
     */
    void computeShelfCoeffs(BiquadCoeffs& c, double freq, float gainDb,
                            bool isLow) {
        if (sampleRate_ <= 0) return;

        const double A = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
        const double w0 = 2.0 * M_PI * freq * invSampleRate_;
        const double cosw0 = std::cos(w0);
        const double sinw0 = std::sin(w0);
        // S = 1.0 (shelf slope), simplified alpha formula
        const double alpha = sinw0 * 0.5 *
            std::sqrt((A + 1.0 / A) * (1.0 / 1.0 - 1.0) + 2.0);
        const double sqrtA2alpha = 2.0 * std::sqrt(A) * alpha;

        double b0, b1, b2, a0, a1, a2;

        if (isLow) {
            // Low shelf
            b0 =    A * ((A + 1.0) - (A - 1.0) * cosw0 + sqrtA2alpha);
            b1 =  2.0 * A * ((A - 1.0) - (A + 1.0) * cosw0);
            b2 =    A * ((A + 1.0) - (A - 1.0) * cosw0 - sqrtA2alpha);
            a0 =        (A + 1.0) + (A - 1.0) * cosw0 + sqrtA2alpha;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosw0);
            a2 =        (A + 1.0) + (A - 1.0) * cosw0 - sqrtA2alpha;
        } else {
            // High shelf
            b0 =    A * ((A + 1.0) + (A - 1.0) * cosw0 + sqrtA2alpha);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosw0);
            b2 =    A * ((A + 1.0) + (A - 1.0) * cosw0 - sqrtA2alpha);
            a0 =        (A + 1.0) - (A - 1.0) * cosw0 + sqrtA2alpha;
            a1 =  2.0 * ((A - 1.0) - (A + 1.0) * cosw0);
            a2 =        (A + 1.0) - (A - 1.0) * cosw0 - sqrtA2alpha;
        }

        // Normalize by a0
        const double invA0 = 1.0 / a0;
        c.b0 = static_cast<float>(b0 * invA0);
        c.b1 = static_cast<float>(b1 * invA0);
        c.b2 = static_cast<float>(b2 * invA0);
        c.a1 = static_cast<float>(a1 * invA0);
        c.a2 = static_cast<float>(a2 * invA0);
    }

    /**
     * Compute peaking EQ biquad coefficients.
     *
     * RBJ Audio EQ Cookbook:
     *   A  = 10^(dBgain/40)
     *   w0 = 2*pi*f0/Fs
     *   alpha = sin(w0) / (2*Q)
     */
    void computePeakingCoeffs(BiquadCoeffs& c, double freq, float gainDb,
                              double Q) {
        if (sampleRate_ <= 0) return;

        const double A = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
        const double w0 = 2.0 * M_PI * freq * invSampleRate_;
        const double cosw0 = std::cos(w0);
        const double sinw0 = std::sin(w0);
        const double alpha = sinw0 / (2.0 * Q);

        const double b0 =  1.0 + alpha * A;
        const double b1 = -2.0 * cosw0;
        const double b2 =  1.0 - alpha * A;
        const double a0 =  1.0 + alpha / A;
        const double a1 = -2.0 * cosw0;
        const double a2 =  1.0 - alpha / A;

        const double invA0 = 1.0 / a0;
        c.b0 = static_cast<float>(b0 * invA0);
        c.b1 = static_cast<float>(b1 * invA0);
        c.b2 = static_cast<float>(b2 * invA0);
        c.a1 = static_cast<float>(a1 * invA0);
        c.a2 = static_cast<float>(a2 * invA0);
    }

    // =========================================================================
    // Schroeder reverb implementation
    // =========================================================================

    /**
     * Schroeder reverb: 4 parallel comb filters summed, then through
     * 2 series allpass filters. Lighter than full Dattorro plate.
     *
     * Each comb filter: y[n] = x[n-d] + feedback * filtered_y[n-d]
     *   where filtered_y uses a one-pole LP for damping.
     * Each allpass: y[n] = -g*x[n] + x[n-d] + g*y[n-d]
     *
     * Processes stereo: same structure for L and R with independent state.
     */
    void processReverb(float* bufL, float* bufR, int numFrames) {
        const float mix = reverbMix_.load(std::memory_order_relaxed);

        // Skip if fully dry
        if (mix < 0.001f) return;

        const float feedback = reverbFeedback_.load(std::memory_order_relaxed);
        const float damping = reverbDamping_.load(std::memory_order_relaxed);
        const float dry = 1.0f - mix;

        for (int i = 0; i < numFrames; ++i) {
            const float inL = bufL[i];
            const float inR = bufR[i];

            // Sum of 4 parallel comb filters
            float combSumL = 0.0f;
            float combSumR = 0.0f;

            for (int c = 0; c < kNumCombs; ++c) {
                combSumL += processComb(combBufL_[c], combPosL_[c],
                                        combDampL_[c], combDelay_[c],
                                        feedback, damping, inL);
                combSumR += processComb(combBufR_[c], combPosR_[c],
                                        combDampR_[c], combDelay_[c],
                                        feedback, damping, inR);
            }

            // Scale comb sum to avoid clipping (4 combs summed)
            combSumL *= 0.25f;
            combSumR *= 0.25f;

            // 2 series allpass filters
            float wetL = combSumL;
            float wetR = combSumR;

            for (int a = 0; a < kNumAllpass; ++a) {
                wetL = processAllpass(allpassBufL_[a], allpassPosL_[a],
                                      allpassDelay_[a], wetL);
                wetR = processAllpass(allpassBufR_[a], allpassPosR_[a],
                                      allpassDelay_[a], wetR);
            }

            // Wet/dry mix
            bufL[i] = dry * inL + mix * wetL;
            bufR[i] = dry * inR + mix * wetR;
        }
    }

    /**
     * Process one sample through a Schroeder comb filter with damping.
     *
     * Comb filter with one-pole lowpass in the feedback path:
     *   delayed = buffer[readPos]
     *   filtered = delayed * (1 - damp) + prevFiltered * damp
     *   buffer[writePos] = input + feedback * filtered
     *   output = delayed
     */
    static inline float processComb(float* buffer, int& pos, float& dampState,
                                     int delay, float feedback, float damping,
                                     float input) {
        const int readPos = (pos - delay + kMaxDelay) % kMaxDelay;
        const float delayed = buffer[readPos];

        // One-pole LP damping in feedback path
        dampState = delayed * (1.0f - damping) + dampState * damping;
        dampState = fast_math::denormal_guard(dampState);

        // Write to delay line
        buffer[pos] = input + feedback * dampState;

        // Advance write position
        pos = (pos + 1) % kMaxDelay;

        return delayed;
    }

    /**
     * Process one sample through a Schroeder allpass filter.
     *
     * y[n] = -g*x[n] + x[n-d] + g*y[n-d]
     * Implemented as: buffer stores full output history.
     */
    static inline float processAllpass(float* buffer, int& pos,
                                        int delay, float input) {
        const int readPos = (pos - delay + kMaxDelay) % kMaxDelay;
        const float delayed = buffer[readPos];

        const float output = -kAllpassG * input + delayed;
        buffer[pos] = input + kAllpassG * output;
        buffer[pos] = fast_math::denormal_guard(buffer[pos]);

        pos = (pos + 1) % kMaxDelay;

        return output;
    }

    /**
     * Recompute comb filter feedback from decay parameter.
     * Maps decay [0,1] to feedback [0.7, 0.95] (musical range).
     */
    void recomputeReverbFeedback() {
        const float decay = reverbDecay_.load(std::memory_order_relaxed);
        const float feedback = 0.7f + decay * 0.25f;
        reverbFeedback_.store(feedback, std::memory_order_relaxed);
    }

    // =========================================================================
    // Compressor coefficient helpers
    // =========================================================================

    /** Compute IIR attack coefficient from time constant in ms. */
    void recomputeCompAttack(float ms) {
        if (sampleRate_ <= 0) return;
        // alpha = 1 - exp(-2.2 / (attackTime * sampleRate))
        // 2.2 is the time constant for ~90% convergence
        const float alpha = 1.0f - std::exp(-2.2f /
            (ms * 0.001f * static_cast<float>(sampleRate_)));
        compAttackCoeff_.store(alpha, std::memory_order_relaxed);
    }

    /** Compute IIR release coefficient from time constant in ms. */
    void recomputeCompRelease(float ms) {
        if (sampleRate_ <= 0) return;
        const float alpha = 1.0f - std::exp(-2.2f /
            (ms * 0.001f * static_cast<float>(sampleRate_)));
        compReleaseCoeff_.store(alpha, std::memory_order_relaxed);
    }

    // =========================================================================
    // Utility
    // =========================================================================

    static float clamp(float val, float lo, float hi) {
        return std::max(lo, std::min(hi, val));
    }

    // =========================================================================
    // Parameters (atomic: written from UI thread, read from audio thread)
    // =========================================================================

    std::atomic<bool> enabled_{true};
    int32_t sampleRate_ = 44100;
    double invSampleRate_ = 1.0 / 44100.0;

    // -- Compressor params --
    std::atomic<float> compThresholdDb_{-10.0f};
    std::atomic<float> compThresholdLin_{0.316f};   // 10^(-10/20)
    std::atomic<float> compRatio_{4.0f};
    std::atomic<float> compAttackMs_{5.0f};
    std::atomic<float> compReleaseMs_{100.0f};
    std::atomic<float> compAttackCoeff_{0.01f};
    std::atomic<float> compReleaseCoeff_{0.001f};
    std::atomic<float> compMakeupLin_{1.0f};        // 0 dB

    // -- Compressor state (audio thread only) --
    double rmsStateL_ = 0.0;
    double rmsStateR_ = 0.0;    // Reserved for future per-channel detection
    float envState_ = 0.0f;

    // -- EQ params --
    std::atomic<float> eqLowGainDb_{0.0f};
    std::atomic<float> eqMidGainDb_{0.0f};
    std::atomic<float> eqMidFreq_{1000.0f};
    std::atomic<float> eqMidQ_{1.0f};
    std::atomic<float> eqHighGainDb_{0.0f};

    // -- EQ coefficients (written from setter thread, read from audio thread) --
    // These are small structs (20 bytes each); on ARM64 aligned stores are atomic
    // for our purposes (we tolerate a single frame of stale coefficients).
    BiquadCoeffs eqCoeffs_[kNumBands] = {};

    // -- EQ state (audio thread only) --
    BiquadState eqStateL_[kNumBands] = {};
    BiquadState eqStateR_[kNumBands] = {};

    // -- Reverb params --
    std::atomic<float> reverbMix_{0.15f};
    std::atomic<float> reverbDecay_{0.5f};
    std::atomic<float> reverbDamping_{0.5f};
    std::atomic<float> reverbFeedback_{0.825f};     // Computed from decay

    // -- Reverb state (audio thread only) --
    int combDelay_[kNumCombs] = { 1116, 1188, 1277, 1356 };
    float combBufL_[kNumCombs][kMaxDelay] = {};
    float combBufR_[kNumCombs][kMaxDelay] = {};
    int combPosL_[kNumCombs] = {};
    int combPosR_[kNumCombs] = {};
    float combDampL_[kNumCombs] = {};
    float combDampR_[kNumCombs] = {};

    int allpassDelay_[kNumAllpass] = { 225, 556 };
    float allpassBufL_[kNumAllpass][kMaxDelay] = {};
    float allpassBufR_[kNumAllpass][kMaxDelay] = {};
    int allpassPosL_[kNumAllpass] = {};
    int allpassPosR_[kNumAllpass] = {};
};

} // namespace drums

#endif // GUITAR_ENGINE_DRUM_BUS_H
