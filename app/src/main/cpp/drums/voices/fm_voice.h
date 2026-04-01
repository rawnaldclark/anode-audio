#ifndef GUITAR_ENGINE_FM_VOICE_H
#define GUITAR_ENGINE_FM_VOICE_H

#include "../drum_voice.h"
#include "../../effects/fast_math.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace drums {

// ---------------------------------------------------------------------------
// Static 1024-point sine wavetable, shared across all FM voice instances.
// Initialized once at program startup via a self-initializing lambda.
// ---------------------------------------------------------------------------
namespace detail {

/**
 * Runtime-initialized sine wavetable. Guaranteed single init via static local.
 * 1024 samples covering one full period [0, 2*pi).
 */
inline const float* getSineTable() {
    static float table[1024] = {};
    static bool initialized = false;
    if (!initialized) {
        constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
        for (int i = 0; i < 1024; ++i) {
            table[i] = static_cast<float>(std::sin(kTwoPi * static_cast<double>(i) / 1024.0));
        }
        initialized = true;
    }
    return table;
}

/**
 * Wavetable sine lookup with linear interpolation.
 *
 * @param phase Phase in radians [any value, wraps automatically via bitmask].
 * @return Sine approximation in [-1, 1]. Max error vs std::sin: < 0.001
 *         (61 dB below signal), inaudible for FM synthesis.
 */
inline float sineLookup(float phase) {
    constexpr float kOneOverTwoPi = 1.0f / (2.0f * 3.14159265358979323846f);
    const float* table = getSineTable();

    // Convert radians to normalized [0, 1024) index
    float idx = phase * kOneOverTwoPi * 1024.0f;

    // Floor to integer (handles negative phases correctly)
    int i0 = static_cast<int>(idx);
    if (idx < static_cast<float>(i0)) i0--; // floor for negatives
    float frac = idx - static_cast<float>(i0);

    // Bitmask wrap to [0, 1023]
    i0 = i0 & 1023;
    int i1 = (i0 + 1) & 1023;

    // Linear interpolation between adjacent samples
    return table[i0] + frac * (table[i1] - table[i0]);
}

} // namespace detail

// ---------------------------------------------------------------------------
// FmVoice: 2-operator FM synthesis drum voice
//
// Signal flow:
//   [Mod Osc (sine)] --FM--> [Carrier Osc (sine)] --> [SVF LP] --> [Drive] --> [Amp Env] --> out
//
// Ideal for: kicks, toms, zaps, laser effects, metallic percussion.
// The pitch envelope sweeping from high to low is the classic 808 kick character.
//
// Parameters (set via setParam):
//   0: freq          (40-400 Hz)    Base carrier frequency
//   1: modRatio      (0.5-8.0)     Modulator freq = carrier * ratio
//   2: modDepth      (0-10)        FM modulation index
//   3: pitchEnvDepth (0-8)         Octaves of pitch sweep on trigger
//   4: pitchDecay    (5-500 ms)    Pitch envelope decay time
//   5: ampDecay      (20-2000 ms)  Amplitude envelope decay time
//   6: filterCutoff  (100-20000 Hz) SVF lowpass cutoff
//   7: filterRes     (0-1)         SVF resonance (Q)
//   8: drive         (0-5)         fast_tanh saturation amount
// ---------------------------------------------------------------------------
class FmVoice : public DrumVoice {
public:
    FmVoice() {
        // Initialize params to sensible defaults (808-style kick)
        params_[0].store(80.0f,  std::memory_order_relaxed);  // freq
        params_[1].store(1.0f,   std::memory_order_relaxed);  // modRatio
        params_[2].store(3.0f,   std::memory_order_relaxed);  // modDepth
        params_[3].store(2.0f,   std::memory_order_relaxed);  // pitchEnvDepth
        params_[4].store(40.0f,  std::memory_order_relaxed);  // pitchDecay (ms)
        params_[5].store(300.0f, std::memory_order_relaxed);  // ampDecay (ms)
        params_[6].store(8000.0f,std::memory_order_relaxed);  // filterCutoff
        params_[7].store(0.0f,   std::memory_order_relaxed);  // filterRes
        params_[8].store(0.0f,   std::memory_order_relaxed);  // drive
    }

    // ----- DrumVoice interface -----

    /**
     * Trigger the FM voice with the given velocity.
     *
     * Resets carrier phase to pi/2 (start at sine peak for punchy 808 attack),
     * modulator phase to 0, and all envelopes to 1.0 (scaled by velocity).
     */
    void trigger(float velocity) override {
        constexpr float kPiOver2 = 3.14159265358979323846f * 0.5f;

        velocity_ = std::clamp(velocity, 0.0f, 1.0f);

        // Reset oscillator phases -- carrier at pi/2 for sine peak (808 character)
        carrierPhase_ = kPiOver2;
        modPhase_ = 0.0f;

        // Reset all envelopes to full, will be scaled by velocity in process()
        ampEnv_ = 1.0f;
        pitchEnv_ = 1.0f;
        modEnv_ = 1.0f;

        // Reset filter state to prevent click from stale state
        svfIc1eq_ = 0.0f;
        svfIc2eq_ = 0.0f;

        // Reset fade state
        fadeCounter_ = 0;
        fadeGain_ = 1.0f;

        // Snapshot current atomic params and recompute coefficients
        recomputeCoefficients();

        active_ = true;
    }

    /**
     * Generate one output sample.
     *
     * Per-sample signal flow:
     *   1. Apply pitch envelope to carrier frequency (sweep high -> base)
     *   2. Compute FM modulator signal
     *   3. Compute FM carrier with phase modulation
     *   4. Apply SVF lowpass filter (TPT/Trapezoidal form)
     *   5. Apply drive saturation (fast_tanh)
     *   6. Apply amplitude envelope (scaled by velocity)
     *   7. Decay all envelopes
     *   8. Deactivate when amplitude envelope drops below threshold
     */
    float process() override {
        if (!active_) return 0.0f;

        constexpr float kTwoPi = 2.0f * 3.14159265358979323846f;
        constexpr float kSilenceThreshold = 0.0001f;

        // --- Pitch envelope: sweep carrier freq from high to base ---
        // currentFreq = freq * (1 + pitchEnv * pitchEnvDepth * 2)
        // At trigger: pitchEnv=1 -> freq * (1 + depth*2) -> up to 17x base freq
        // Decays to: pitchEnv~0 -> freq * 1.0 -> base frequency
        float currentFreq = freq_ * (1.0f + pitchEnv_ * pitchEnvDepth_ * 2.0f);

        // --- FM modulator ---
        float modSignal = detail::sineLookup(modPhase_) * modDepth_ * modEnv_;

        // --- FM carrier (phase modulated by modulator) ---
        float carrier = detail::sineLookup(carrierPhase_ + modSignal);

        // --- Advance oscillator phases ---
        float phaseInc = kTwoPi * currentFreq * sampleRateInv_;
        carrierPhase_ += phaseInc;
        modPhase_ += kTwoPi * currentFreq * modRatio_ * sampleRateInv_;

        // Phase wrap to prevent float precision loss over long sustains
        // Using subtraction (not fmod) to avoid branch misprediction
        if (carrierPhase_ > kTwoPi) carrierPhase_ -= kTwoPi;
        if (carrierPhase_ < 0.0f)   carrierPhase_ += kTwoPi;
        if (modPhase_ > kTwoPi)     modPhase_ -= kTwoPi;
        if (modPhase_ < 0.0f)       modPhase_ += kTwoPi;

        // --- SVF Lowpass Filter (TPT / Trapezoidal Integrator form) ---
        // This is the standard Vadim Zavalishin TPT SVF.
        // g, k are pre-computed in recomputeCoefficients().
        float v3 = carrier - svfIc2eq_;
        float v1 = svfA1_ * svfIc1eq_ + svfA2_ * v3;
        float v2 = svfIc2eq_ + svfA2_ * svfIc1eq_ + svfA3_ * v3;
        svfIc1eq_ = 2.0f * v1 - svfIc1eq_;
        svfIc2eq_ = 2.0f * v2 - svfIc2eq_;

        // Denormal guard on filter state
        svfIc1eq_ = fast_math::denormal_guard(svfIc1eq_);
        svfIc2eq_ = fast_math::denormal_guard(svfIc2eq_);

        // LP output is v2
        float filtered = v2;

        // --- Drive saturation ---
        float out;
        if (drive_ > 0.01f) {
            out = fast_math::fast_tanh(drive_ * filtered);
        } else {
            out = filtered;
        }

        // --- Amplitude envelope (scaled by trigger velocity) ---
        out *= ampEnv_ * velocity_;

        // --- Decay all envelopes ---
        ampEnv_ *= ampCoeff_;
        pitchEnv_ *= pitchCoeff_;
        modEnv_ *= modCoeff_;

        // --- Deactivate when amplitude falls below silence threshold ---
        if (ampEnv_ < kSilenceThreshold) {
            active_ = false;
            return 0.0f;
        }

        // Apply choke-group fade if active
        return applyFade(out);
    }

    /**
     * Set a synthesis parameter by ID. Thread-safe (atomic store).
     * Also recomputes rate-dependent coefficients for time-based params.
     *
     * @param paramId 0-8, see class header for parameter map.
     * @param value   Raw parameter value (clamped to valid range).
     */
    void setParam(int paramId, float value) override {
        if (paramId < 0 || paramId > 8) return;

        // Clamp to valid ranges
        switch (paramId) {
            case 0: value = std::clamp(value, 40.0f,  400.0f);   break; // freq
            case 1: value = std::clamp(value, 0.5f,   8.0f);     break; // modRatio
            case 2: value = std::clamp(value, 0.0f,   10.0f);    break; // modDepth
            case 3: value = std::clamp(value, 0.0f,   8.0f);     break; // pitchEnvDepth
            case 4: value = std::clamp(value, 5.0f,   500.0f);   break; // pitchDecay (ms)
            case 5: value = std::clamp(value, 20.0f,  2000.0f);  break; // ampDecay (ms)
            case 6: value = std::clamp(value, 100.0f, 20000.0f); break; // filterCutoff
            case 7: value = std::clamp(value, 0.0f,   1.0f);     break; // filterRes
            case 8: value = std::clamp(value, 0.0f,   5.0f);     break; // drive
            default: break;
        }

        params_[paramId].store(value, std::memory_order_relaxed);
    }

    /**
     * Get current parameter value. Thread-safe (atomic load).
     */
    float getParam(int paramId) const override {
        if (paramId < 0 || paramId > 8) return 0.0f;
        return params_[paramId].load(std::memory_order_relaxed);
    }

    /**
     * Set sample rate and recompute all rate-dependent coefficients.
     */
    void setSampleRate(int32_t sr) override {
        sampleRate_ = static_cast<float>(sr);
        sampleRateInv_ = 1.0f / sampleRate_;
        recomputeCoefficients();
    }

    /**
     * Reset all internal state. Called when audio stream restarts.
     */
    void reset() override {
        active_ = false;
        carrierPhase_ = 0.0f;
        modPhase_ = 0.0f;
        ampEnv_ = 0.0f;
        pitchEnv_ = 0.0f;
        modEnv_ = 0.0f;
        svfIc1eq_ = 0.0f;
        svfIc2eq_ = 0.0f;
        fadeCounter_ = 0;
        fadeGain_ = 1.0f;
        velocity_ = 0.0f;
    }

private:
    // ----- Atomic parameters (written by UI thread, read by audio thread) -----
    std::atomic<float> params_[9] = {};

    // ----- Cached / pre-computed values (audio thread only) -----
    float sampleRate_ = 48000.0f;
    float sampleRateInv_ = 1.0f / 48000.0f;

    // Oscillator state
    float carrierPhase_ = 0.0f;   ///< Carrier phase in radians
    float modPhase_ = 0.0f;       ///< Modulator phase in radians

    // Envelope state (exponential decay, multiplied per-sample by coeff)
    float ampEnv_ = 0.0f;         ///< Amplitude envelope [0, 1]
    float pitchEnv_ = 0.0f;       ///< Pitch sweep envelope [0, 1]
    float modEnv_ = 0.0f;         ///< FM modulation depth envelope [0, 1]

    // Pre-computed envelope decay coefficients
    // coeff = exp(-1 / (decaySeconds * sampleRate))
    // Multiplied per-sample: env *= coeff
    float ampCoeff_ = 0.999f;
    float pitchCoeff_ = 0.999f;
    float modCoeff_ = 0.999f;     ///< Mod envelope decays at same rate as pitch

    // Cached parameter snapshots (avoid atomic reads per-sample)
    float freq_ = 80.0f;
    float modRatio_ = 1.0f;
    float modDepth_ = 3.0f;
    float pitchEnvDepth_ = 2.0f;
    float drive_ = 0.0f;
    float velocity_ = 0.0f;

    // SVF filter state (TPT / Trapezoidal form)
    float svfIc1eq_ = 0.0f;       ///< First integrator state
    float svfIc2eq_ = 0.0f;       ///< Second integrator state
    float svfA1_ = 0.0f;          ///< Pre-computed SVF coefficient
    float svfA2_ = 0.0f;          ///< Pre-computed SVF coefficient
    float svfA3_ = 0.0f;          ///< Pre-computed SVF coefficient

    /**
     * Recompute all rate-dependent and parameter-dependent coefficients.
     *
     * Called from trigger() (to snapshot atomic params) and setSampleRate().
     * NOT called per-sample -- all expensive math (exp, tan) happens here.
     */
    void recomputeCoefficients() {
        // Snapshot atomic params to local cache
        freq_          = params_[0].load(std::memory_order_relaxed);
        modRatio_      = params_[1].load(std::memory_order_relaxed);
        modDepth_      = params_[2].load(std::memory_order_relaxed);
        pitchEnvDepth_ = params_[3].load(std::memory_order_relaxed);
        float pitchDecayMs = params_[4].load(std::memory_order_relaxed);
        float ampDecayMs   = params_[5].load(std::memory_order_relaxed);
        float filterCutoff = params_[6].load(std::memory_order_relaxed);
        float filterRes    = params_[7].load(std::memory_order_relaxed);
        drive_             = params_[8].load(std::memory_order_relaxed);

        // --- Envelope decay coefficients ---
        // coeff = exp(-1 / (decaySeconds * sampleRate))
        // This gives ~60dB decay in about 6.9 * decaySeconds
        float ampDecaySec   = ampDecayMs * 0.001f;
        float pitchDecaySec = pitchDecayMs * 0.001f;

        if (ampDecaySec > 0.0001f && sampleRate_ > 0.0f) {
            ampCoeff_ = static_cast<float>(std::exp(-1.0 / (static_cast<double>(ampDecaySec) * sampleRate_)));
        } else {
            ampCoeff_ = 0.0f;
        }

        if (pitchDecaySec > 0.0001f && sampleRate_ > 0.0f) {
            pitchCoeff_ = static_cast<float>(std::exp(-1.0 / (static_cast<double>(pitchDecaySec) * sampleRate_)));
        } else {
            pitchCoeff_ = 0.0f;
        }

        // Mod envelope decays at same rate as pitch envelope
        // (modulator contribution fades as pitch settles to base freq)
        modCoeff_ = pitchCoeff_;

        // --- SVF filter coefficients (TPT / Trapezoidal form) ---
        // g = tan(pi * fc / fs)
        // k = 2 - 2 * resonance  (resonance 0-1 maps to k 2-0)
        // Standard Vadim Zavalishin SVF coefficients:
        //   a1 = 1 / (1 + g*(g+k))
        //   a2 = g * a1
        //   a3 = g * a2
        constexpr float kPi = 3.14159265358979323846f;

        // Clamp cutoff to prevent instability near Nyquist
        float maxCutoff = sampleRate_ * 0.49f;
        float fc = std::min(filterCutoff, maxCutoff);

        // Use fast_math::tan_approx for the filter coefficient
        // (accurate enough for fc < 0.4*fs which is our clamped range)
        float g = static_cast<float>(fast_math::tan_approx(
            static_cast<double>(kPi) * static_cast<double>(fc) * static_cast<double>(sampleRateInv_)));

        // k: resonance mapping. filterRes=0 -> k=2 (no resonance), filterRes=1 -> k=0 (self-oscillation)
        float k = 2.0f * (1.0f - filterRes);

        float denom = 1.0f / (1.0f + g * (g + k));
        svfA1_ = denom;
        svfA2_ = g * denom;
        svfA3_ = g * svfA2_;
    }
};

} // namespace drums

#endif // GUITAR_ENGINE_FM_VOICE_H
