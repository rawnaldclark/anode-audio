#ifndef GUITAR_ENGINE_NOISE_VOICE_H
#define GUITAR_ENGINE_NOISE_VOICE_H

#include <atomic>
#include <cmath>
#include <cstdint>

#include "../drum_voice.h"
#include "../../effects/fast_math.h"

/**
 * Filtered noise drum voice for shakers, brushes, and hi-hat textures.
 *
 * Signal flow:
 *   [White/Pink Noise] -> [SVF (LP/HP/BP)] -> [Amp Envelope] -> out
 *
 * Noise types:
 *   - White: xorshift32 PRNG, flat spectrum.
 *   - Pink: 3-stage Voss-McCartney algorithm. Maintains 3 random values
 *     updated at different rates (every 1, 2, 4 samples) and sums them.
 *     Produces approximately -3 dB/octave rolloff.
 *
 * SVF filter uses topology-preserving transform (TPT) form for stability
 * at high cutoff frequencies. Switchable LP/HP/BP output.
 *
 * Filter sweep: on trigger, the filter cutoff starts at
 * filterFreq * 2^(sweep*4) octaves above the base frequency and
 * decays exponentially to the base filterFreq, creating a characteristic
 * "closing" sound used in shakers and brushed snares.
 *
 * Parameters:
 *   0: noiseType   - 0 = white, 1 = pink (3-stage Voss-McCartney)
 *   1: filterMode  - 0 = lowpass, 1 = highpass, 2 = bandpass
 *   2: filterFreq  - SVF cutoff frequency (100-16000 Hz)
 *   3: filterRes   - SVF resonance (0-1)
 *   4: decay       - Amplitude envelope decay time (5-2000 ms)
 *   5: sweep       - Filter envelope depth (0-1, maps to 0-4 octaves)
 *
 * Thread safety: All parameters are std::atomic<float> for UI thread writes.
 * process() is called exclusively from the audio thread.
 */

namespace drums {

class NoiseVoice : public DrumVoice {
public:
    NoiseVoice() {
        params_[0].store(0.0f, std::memory_order_relaxed);  // white noise
        params_[1].store(0.0f, std::memory_order_relaxed);  // LP mode
        params_[2].store(4000.0f, std::memory_order_relaxed); // 4kHz cutoff
        params_[3].store(0.3f, std::memory_order_relaxed);  // moderate res
        params_[4].store(200.0f, std::memory_order_relaxed); // 200ms decay
        params_[5].store(0.5f, std::memory_order_relaxed);  // half sweep
    }

    void trigger(float velocity) override {
        velocity_ = velocity;
        ampEnv_ = 1.0f;
        active_ = true;
        fadeCounter_ = 0;
        fadeGain_ = 1.0f;

        // Reset filter state to avoid transient clicks from stale history
        svfIc1eq_ = 0.0;
        svfIc2eq_ = 0.0;

        // Initialize filter sweep envelope to full depth
        filterEnv_ = 1.0f;

        // Reset pink noise state on trigger for consistent attack
        pinkState_[0] = 0.0f;
        pinkState_[1] = 0.0f;
        pinkState_[2] = 0.0f;
        pinkCounter_ = 0;

        // Pre-compute amp decay coefficient
        updateAmpDecayCoeff();

        // Pre-compute filter sweep decay (fixed ~50ms sweep time)
        updateFilterSweepCoeff();
    }

    float process() override {
        if (!active_) return 0.0f;

        // --- Read params (relaxed loads, audio thread only consumer) ---
        const float noiseType  = params_[0].load(std::memory_order_relaxed);
        const int   filterMode = static_cast<int>(params_[1].load(std::memory_order_relaxed));
        const float filterFreq = params_[2].load(std::memory_order_relaxed);
        const float filterRes  = params_[3].load(std::memory_order_relaxed);
        const float sweep      = params_[5].load(std::memory_order_relaxed);

        // --- Generate noise sample ---
        float noise;
        if (noiseType < 0.5f) {
            noise = generateWhiteNoise();
        } else {
            noise = generatePinkNoise();
        }

        // --- Compute swept filter cutoff ---
        // Sweep moves cutoff from filterFreq * 2^(sweep*4) down to filterFreq
        const float sweepOctaves = sweep * 4.0f;
        const float envFreq = filterFreq * fast_math::exp2_approx(sweepOctaves * filterEnv_);
        // Clamp to Nyquist safety margin (0.49 * sampleRate)
        const float maxFreq = sampleRate_ * 0.49f;
        const float clampedFreq = (envFreq < maxFreq) ? envFreq : maxFreq;

        // --- SVF filter (TPT form, Vadim Zavalishin) ---
        // g = tan(pi * fc / fs), using fast tan approximation
        const double g = fast_math::tan_approx(M_PI * static_cast<double>(clampedFreq) / sampleRate_);
        // Q from resonance: Q ranges from 0.5 (no res) to 20 (high res)
        const double Q = 0.5 + filterRes * 19.5;
        const double k = 1.0 / Q;

        // Pre-compute: a1 = 1 / (1 + g*(g + k))
        const double a1 = 1.0 / (1.0 + g * (g + k));

        // TPT SVF update equations
        const double v3 = static_cast<double>(noise) - svfIc2eq_;
        const double v1 = a1 * svfIc1eq_ + (g * a1) * v3;
        const double v2 = svfIc2eq_ + g * v1;

        svfIc1eq_ = 2.0 * v1 - svfIc1eq_;
        svfIc2eq_ = 2.0 * v2 - svfIc2eq_;

        // Denormal protection on filter state
        svfIc1eq_ = fast_math::denormal_guard(svfIc1eq_);
        svfIc2eq_ = fast_math::denormal_guard(svfIc2eq_);

        // Select filter output based on mode
        float filtered;
        switch (filterMode) {
            case 1:  // HP: input - LP - k*BP
                filtered = static_cast<float>(static_cast<double>(noise) - k * v1 - v2);
                break;
            case 2:  // BP
                filtered = static_cast<float>(v1);
                break;
            default: // LP (mode 0)
                filtered = static_cast<float>(v2);
                break;
        }

        // NaN/Inf safety: if the filter produces a non-finite value
        // (e.g., from corrupted state or extreme coefficients), reset
        // filter state and return silence to prevent audio corruption.
        if (!std::isfinite(filtered)) {
            svfIc1eq_ = 0.0;
            svfIc2eq_ = 0.0;
            filtered = 0.0f;
        }

        // --- Apply amplitude envelope ---
        float out = filtered * ampEnv_ * velocity_;

        // Decay envelopes
        ampEnv_ *= ampDecayCoeff_;
        filterEnv_ *= filterSweepCoeff_;

        // Denormal guard on envelopes
        ampEnv_ = fast_math::denormal_guard(ampEnv_);
        filterEnv_ = fast_math::denormal_guard(filterEnv_);

        // Deactivate when envelope is below silence threshold (-80 dB)
        if (ampEnv_ < 0.0001f) {
            active_ = false;
            return 0.0f;
        }

        return applyFade(out);
    }

    void setParam(int paramId, float value) override {
        if (paramId < 0 || paramId > 5) return;

        // Clamp values to valid ranges
        switch (paramId) {
            case 0: value = (value < 0.5f) ? 0.0f : 1.0f; break; // snap to 0 or 1
            case 1: value = clamp(value, 0.0f, 2.0f); break;
            case 2: value = clamp(value, 100.0f, 16000.0f); break;
            case 3: value = clamp(value, 0.0f, 1.0f); break;
            case 4: value = clamp(value, 5.0f, 2000.0f); break;
            case 5: value = clamp(value, 0.0f, 1.0f); break;
        }
        params_[paramId].store(value, std::memory_order_relaxed);

        // Recompute rate-dependent coefficients when decay changes
        if (paramId == 4 && sampleRate_ > 0) {
            updateAmpDecayCoeff();
        }
    }

    float getParam(int paramId) const override {
        if (paramId < 0 || paramId > 5) return 0.0f;
        return params_[paramId].load(std::memory_order_relaxed);
    }

    void setSampleRate(int32_t sr) override {
        sampleRate_ = static_cast<float>(sr);
        updateAmpDecayCoeff();
        updateFilterSweepCoeff();
    }

    void reset() override {
        active_ = false;
        ampEnv_ = 0.0f;
        filterEnv_ = 0.0f;
        svfIc1eq_ = 0.0;
        svfIc2eq_ = 0.0;
        prngState_ = 0x12345678u;
        pinkState_[0] = 0.0f;
        pinkState_[1] = 0.0f;
        pinkState_[2] = 0.0f;
        pinkCounter_ = 0;
        fadeCounter_ = 0;
        fadeGain_ = 1.0f;
    }

private:
    // --- Parameters (atomic for UI thread writes) ---
    std::atomic<float> params_[6];

    // --- State (audio thread only) ---
    float sampleRate_ = 48000.0f;
    float velocity_ = 0.0f;
    float ampEnv_ = 0.0f;         ///< Amplitude envelope level
    float ampDecayCoeff_ = 0.999f; ///< Per-sample decay multiplier
    float filterEnv_ = 0.0f;      ///< Filter sweep envelope (1 -> 0)
    float filterSweepCoeff_ = 0.999f; ///< Per-sample filter sweep decay

    // SVF filter state (double precision for stability in feedback)
    double svfIc1eq_ = 0.0;
    double svfIc2eq_ = 0.0;

    // xorshift32 PRNG state
    uint32_t prngState_ = 0x12345678u;

    // Voss-McCartney pink noise state
    float pinkState_[3] = {0.0f, 0.0f, 0.0f};
    uint32_t pinkCounter_ = 0;

    /**
     * xorshift32 PRNG: generates white noise in [-1, 1].
     * Period 2^32-1, extremely fast, no branching.
     */
    float generateWhiteNoise() {
        prngState_ ^= prngState_ << 13;
        prngState_ ^= prngState_ >> 17;
        prngState_ ^= prngState_ << 5;
        // Convert to float in [-1, 1] range
        // Cast to signed int first for symmetric range
        return static_cast<float>(static_cast<int32_t>(prngState_)) * (1.0f / 2147483648.0f);
    }

    /**
     * 3-stage Voss-McCartney pink noise generator.
     *
     * Maintains 3 random values updated at rates of every 1, 2, and 4
     * samples respectively. Summing these produces an approximate -3 dB/oct
     * spectrum (pink noise). Simple and efficient for real-time use.
     *
     * Each stage is updated when its corresponding bit in the counter flips.
     * The output is normalized by dividing by 3 (number of stages).
     */
    float generatePinkNoise() {
        pinkCounter_++;

        // Stage 0: update every sample
        pinkState_[0] = generateWhiteNoise();

        // Stage 1: update every 2 samples (bit 0 flips)
        if ((pinkCounter_ & 1u) == 0) {
            pinkState_[1] = generateWhiteNoise();
        }

        // Stage 2: update every 4 samples (bit 1 flips)
        if ((pinkCounter_ & 3u) == 0) {
            pinkState_[2] = generateWhiteNoise();
        }

        // Sum and normalize (3 stages)
        return (pinkState_[0] + pinkState_[1] + pinkState_[2]) * (1.0f / 3.0f);
    }

    /**
     * Pre-compute amplitude decay coefficient from decay time in ms.
     * coeff = exp(-1 / (decaySeconds * sampleRate))
     */
    void updateAmpDecayCoeff() {
        const float decayMs = params_[4].load(std::memory_order_relaxed);
        const float decaySec = decayMs * 0.001f;
        if (decaySec > 0.0f && sampleRate_ > 0.0f) {
            ampDecayCoeff_ = std::exp(-1.0f / (decaySec * sampleRate_));
        }
    }

    /**
     * Pre-compute filter sweep decay coefficient.
     * Uses a fixed ~50ms sweep time for a snappy transient character.
     */
    void updateFilterSweepCoeff() {
        const float sweepSec = 0.05f; // 50ms sweep time
        if (sampleRate_ > 0.0f) {
            filterSweepCoeff_ = std::exp(-1.0f / (sweepSec * sampleRate_));
        }
    }

    /** Clamp a value to [lo, hi]. */
    static float clamp(float x, float lo, float hi) {
        return (x < lo) ? lo : ((x > hi) ? hi : x);
    }
};

} // namespace drums

#endif // GUITAR_ENGINE_NOISE_VOICE_H
