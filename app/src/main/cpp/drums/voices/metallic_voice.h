#ifndef GUITAR_ENGINE_METALLIC_VOICE_H
#define GUITAR_ENGINE_METALLIC_VOICE_H

#include "../drum_voice.h"
#include "../../effects/fast_math.h"

#include <atomic>
#include <cmath>
#include <cstdint>

/**
 * 808-style metallic cymbal/hi-hat voice.
 *
 * Synthesis architecture:
 *   6 detuned PolyBLEP square oscillators at the classic TR-808
 *   metallic frequencies (205.3, 304.4, 369.6, 522.7, 540.0, 800.0 Hz)
 *   are summed and fed through two parallel SVF bandpass filters
 *   (3440 Hz and 7100 Hz). The "tone" parameter crossfades between
 *   these two bands. The result passes through an SVF highpass filter
 *   and a simple exponential amplitude envelope.
 *
 * The deliberately inharmonic frequency ratios create the characteristic
 * clangorous, metallic timbre of the 808 hi-hat and cymbal. The "color"
 *  parameter detunes the oscillators away from their nominal 808 values
 * for timbral variation.
 *
 * Parameters:
 *   0: color    (0-1)  Detuning amount from nominal 808 frequencies.
 *                       0 = exact 808 ratios, 1 = fully detuned.
 *   1: tone     (0-1)  Blend between BP1 (3440 Hz, darker) and
 *                       BP2 (7100 Hz, brighter).
 *   2: decay    (5-2000 ms)  Amplitude envelope decay time.
 *   3: hpCutoff (2000-12000 Hz)  Highpass filter cutoff frequency.
 *
 * Thread safety:
 *   All four parameters use std::atomic<float> for lock-free
 *   cross-thread access. Filter coefficients are recalculated
 *   per-block (at trigger time and on sample rate change).
 *
 * Real-time safety:
 *   No allocations, no locks, no system calls. All state is
 *   stack or member variables. PolyBLEP uses only arithmetic.
 */

namespace drums {

class MetallicVoice : public DrumVoice {
public:
    MetallicVoice() {
        // Set default parameter values
        color_.store(0.0f, std::memory_order_relaxed);
        tone_.store(0.5f, std::memory_order_relaxed);
        decay_.store(200.0f, std::memory_order_relaxed);
        hpCutoff_.store(6000.0f, std::memory_order_relaxed);

        reset();
    }

    // ----------------------------------------------------------------
    // DrumVoice interface
    // ----------------------------------------------------------------

    void trigger(float velocity) override {
        velocity_ = velocity;
        active_ = true;
        fadeCounter_ = 0;
        fadeGain_ = 1.0f;

        // Reset oscillator phases to zero for consistent attack transient
        for (int i = 0; i < kNumOscs; ++i) {
            phase_[i] = 0.0f;
        }

        // Reset filter state variables to avoid transient pops from
        // stale state of a previous note
        bp1Low_ = bp1Band_ = 0.0f;
        bp2Low_ = bp2Band_ = 0.0f;
        hpLow_ = hpBand_ = 0.0f;

        // Reset amplitude envelope to full level
        envLevel_ = 1.0f;

        // Recalculate rate-dependent coefficients for current params
        recalcOscFreqs();
        recalcFilters();
        recalcEnvDecay();
    }

    float process() override {
        if (!active_) return 0.0f;

        // ----------------------------------------------------------
        // 1. Generate 6 PolyBLEP square oscillators and sum
        // ----------------------------------------------------------
        float oscSum = 0.0f;
        for (int i = 0; i < kNumOscs; ++i) {
            // Naive square: +1 for phase < 0.5, -1 otherwise
            float sample = (phase_[i] < 0.5f) ? 1.0f : -1.0f;

            // Apply PolyBLEP anti-aliasing correction at transitions
            // Corrects the two discontinuities per cycle (at phase 0 and 0.5)
            sample += polyBLEP(phase_[i], phaseInc_[i]);
            sample -= polyBLEP(fmodf(phase_[i] + 0.5f, 1.0f), phaseInc_[i]);

            oscSum += sample;

            // Advance phase with wraparound
            phase_[i] += phaseInc_[i];
            if (phase_[i] >= 1.0f) phase_[i] -= 1.0f;
        }

        // Normalize by number of oscillators to keep level in [-1, 1]
        oscSum *= kOscGain;

        // ----------------------------------------------------------
        // 2. Two parallel SVF bandpass filters
        // ----------------------------------------------------------

        // BP1 at 3440 Hz
        bp1Low_  += bp1G_ * bp1Band_;
        float bp1High = oscSum - bp1Low_ - bp1Q_ * bp1Band_;
        bp1Band_ += bp1G_ * bp1High;
        bp1Low_  = fast_math::denormal_guard(bp1Low_);
        bp1Band_ = fast_math::denormal_guard(bp1Band_);
        float bp1Out = bp1Band_;

        // BP2 at 7100 Hz
        bp2Low_  += bp2G_ * bp2Band_;
        float bp2High = oscSum - bp2Low_ - bp2Q_ * bp2Band_;
        bp2Band_ += bp2G_ * bp2High;
        bp2Low_  = fast_math::denormal_guard(bp2Low_);
        bp2Band_ = fast_math::denormal_guard(bp2Band_);
        float bp2Out = bp2Band_;

        // ----------------------------------------------------------
        // 3. Crossfade between BP1 and BP2 based on tone parameter
        // ----------------------------------------------------------
        float toneVal = tone_.load(std::memory_order_relaxed);
        float mixed = bp1Out * (1.0f - toneVal) + bp2Out * toneVal;

        // ----------------------------------------------------------
        // 4. SVF highpass filter
        // ----------------------------------------------------------
        hpLow_  += hpG_ * hpBand_;
        float hpHigh = mixed - hpLow_ - hpQ_ * hpBand_;
        hpBand_ += hpG_ * hpHigh;
        hpLow_  = fast_math::denormal_guard(hpLow_);
        hpBand_ = fast_math::denormal_guard(hpBand_);
        float hpOut = hpHigh;  // HP output is the high component

        // ----------------------------------------------------------
        // 5. Amplitude envelope (exponential decay)
        // ----------------------------------------------------------
        float output = hpOut * envLevel_ * velocity_;
        envLevel_ *= envDecayCoeff_;

        // Silence threshold: deactivate voice when envelope is inaudible
        // -80 dB ~ 0.0001
        if (envLevel_ < 0.0001f) {
            active_ = false;
            return 0.0f;
        }

        // ----------------------------------------------------------
        // 6. Apply choke-group fade if active
        // ----------------------------------------------------------
        return applyFade(output);
    }

    void setParam(int paramId, float value) override {
        switch (paramId) {
            case 0:
                color_.store(clamp(value, 0.0f, 1.0f), std::memory_order_relaxed);
                break;
            case 1:
                tone_.store(clamp(value, 0.0f, 1.0f), std::memory_order_relaxed);
                break;
            case 2:
                decay_.store(clamp(value, 5.0f, 2000.0f), std::memory_order_relaxed);
                break;
            case 3:
                hpCutoff_.store(clamp(value, 2000.0f, 12000.0f), std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    float getParam(int paramId) const override {
        switch (paramId) {
            case 0: return color_.load(std::memory_order_relaxed);
            case 1: return tone_.load(std::memory_order_relaxed);
            case 2: return decay_.load(std::memory_order_relaxed);
            case 3: return hpCutoff_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

    void setSampleRate(int32_t sr) override {
        sampleRate_ = static_cast<float>(sr);
        invSampleRate_ = 1.0f / sampleRate_;
        recalcOscFreqs();
        recalcFilters();
        recalcEnvDecay();
    }

    void reset() override {
        // Reset oscillator phases
        for (int i = 0; i < kNumOscs; ++i) {
            phase_[i] = 0.0f;
            phaseInc_[i] = 0.0f;
        }

        // Reset filter state
        bp1Low_ = bp1Band_ = 0.0f;
        bp2Low_ = bp2Band_ = 0.0f;
        hpLow_ = hpBand_ = 0.0f;

        // Reset envelope
        envLevel_ = 0.0f;
        velocity_ = 0.0f;
        active_ = false;
        fadeCounter_ = 0;
        fadeGain_ = 1.0f;
    }

private:
    // ----------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------

    static constexpr int kNumOscs = 6;

    /// Normalization gain: 1/6 to keep summed oscillators in [-1, 1]
    static constexpr float kOscGain = 1.0f / static_cast<float>(kNumOscs);

    /// Classic TR-808 metallic oscillator frequencies (Hz).
    /// These inharmonic ratios create the characteristic clangorous timbre.
    static constexpr float kBaseFreqs[kNumOscs] = {
        205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f
    };

    /// Per-oscillator detune factors applied when color > 0.
    /// Asymmetric positive/negative offsets create beating and widening.
    static constexpr float kDetuneFactors[kNumOscs] = {
        -0.10f, 0.05f, -0.08f, 0.12f, -0.03f, 0.07f
    };

    /// Fixed bandpass filter frequencies (Hz)
    static constexpr float kBp1Freq = 3440.0f;
    static constexpr float kBp2Freq = 7100.0f;

    /// Bandpass filter Q (resonance). ~2.0 gives a moderately narrow peak.
    static constexpr float kBpQ = 2.0f;

    /// Highpass filter Q. Slightly below Butterworth (0.707) for a
    /// smooth rolloff without resonant peak.
    static constexpr float kHpQ = 0.707f;

    // ----------------------------------------------------------------
    // PolyBLEP anti-aliasing
    // ----------------------------------------------------------------

    /**
     * PolyBLEP (Polynomal Band-Limited Step) correction.
     *
     * Smooths the discontinuities in a naive square wave by applying
     * a polynomial correction in the region immediately around each
     * transition (within one sample of the edge). This dramatically
     * reduces aliasing without the cost of oversampling.
     *
     * @param t Current oscillator phase in [0, 1).
     * @param dt Phase increment per sample (frequency / sampleRate).
     * @return Correction value to add to the naive waveform.
     */
    static float polyBLEP(float t, float dt) {
        // Rising edge: phase just crossed 0 (wrapped from ~1.0)
        if (t < dt) {
            float n = t / dt;  // normalized position within transition region
            return n + n - n * n - 1.0f;
        }
        // Falling edge: phase about to cross 1.0
        if (t > 1.0f - dt) {
            float n = (t - 1.0f) / dt;  // negative normalized position
            return n * n + n + n + 1.0f;
        }
        return 0.0f;
    }

    // ----------------------------------------------------------------
    // Coefficient recalculation
    // ----------------------------------------------------------------

    /**
     * Recalculate oscillator phase increments from base frequencies
     * and current color (detuning) parameter.
     */
    void recalcOscFreqs() {
        float color = color_.load(std::memory_order_relaxed);
        for (int i = 0; i < kNumOscs; ++i) {
            float freq = kBaseFreqs[i] * (1.0f + color * kDetuneFactors[i]);
            phaseInc_[i] = freq * invSampleRate_;
        }
    }

    /**
     * Recalculate SVF filter coefficients for both bandpass filters
     * and the highpass filter.
     *
     * Uses the TPT (Topology-Preserving Transform) SVF formulation:
     *   g = tan(pi * fc / fs)
     * For efficiency, we use fast_math::tan_approx for the tangent.
     *
     * SVF update equations (per sample):
     *   low  += g * band
     *   high  = input - low - q * band
     *   band += g * high
     *
     * Where q = 1/Q controls the damping (inverse of resonance).
     */
    void recalcFilters() {
        // BP1 at 3440 Hz
        double bp1Arg = M_PI * static_cast<double>(kBp1Freq) / static_cast<double>(sampleRate_);
        bp1G_ = static_cast<float>(fast_math::tan_approx(bp1Arg));
        bp1Q_ = 1.0f / kBpQ;

        // BP2 at 7100 Hz
        double bp2Arg = M_PI * static_cast<double>(kBp2Freq) / static_cast<double>(sampleRate_);
        bp2G_ = static_cast<float>(fast_math::tan_approx(bp2Arg));
        bp2Q_ = 1.0f / kBpQ;

        // HP filter at user-specified cutoff
        float hpFreq = hpCutoff_.load(std::memory_order_relaxed);
        // Clamp to below Nyquist to prevent tan() from blowing up
        float maxFreq = sampleRate_ * 0.45f;
        if (hpFreq > maxFreq) hpFreq = maxFreq;
        double hpArg = M_PI * static_cast<double>(hpFreq) / static_cast<double>(sampleRate_);
        hpG_ = static_cast<float>(fast_math::tan_approx(hpArg));
        hpQ_ = 1.0f / kHpQ;
    }

    /**
     * Recalculate the exponential decay coefficient from the decay
     * time parameter (in milliseconds).
     *
     * We want the envelope to reach -60 dB (0.001) in the specified
     * decay time. The per-sample coefficient is:
     *   coeff = exp(-6.908 / (decayMs * 0.001 * sampleRate))
     * where -6.908 = ln(0.001).
     */
    void recalcEnvDecay() {
        float decayMs = decay_.load(std::memory_order_relaxed);
        float decaySamples = decayMs * 0.001f * sampleRate_;
        if (decaySamples < 1.0f) decaySamples = 1.0f;
        // ln(0.001) = -6.907755
        envDecayCoeff_ = std::exp(-6.907755f / decaySamples);
    }

    // ----------------------------------------------------------------
    // Utility
    // ----------------------------------------------------------------

    static float clamp(float x, float lo, float hi) {
        if (x < lo) return lo;
        if (x > hi) return hi;
        return x;
    }

    // ----------------------------------------------------------------
    // State variables (audio thread only, except atomics)
    // ----------------------------------------------------------------

    float sampleRate_ = 48000.0f;
    float invSampleRate_ = 1.0f / 48000.0f;

    // Oscillator state
    float phase_[kNumOscs] = {};       ///< Current phase [0, 1) per oscillator
    float phaseInc_[kNumOscs] = {};    ///< Phase increment per sample

    // SVF filter state — BP1 (3440 Hz)
    float bp1Low_ = 0.0f;
    float bp1Band_ = 0.0f;
    float bp1G_ = 0.0f;               ///< tan(pi * fc / fs) coefficient
    float bp1Q_ = 0.5f;               ///< 1/Q damping factor

    // SVF filter state — BP2 (7100 Hz)
    float bp2Low_ = 0.0f;
    float bp2Band_ = 0.0f;
    float bp2G_ = 0.0f;
    float bp2Q_ = 0.5f;

    // SVF filter state — HP
    float hpLow_ = 0.0f;
    float hpBand_ = 0.0f;
    float hpG_ = 0.0f;
    float hpQ_ = 1.414f;              ///< 1/Q for HP

    // Amplitude envelope
    float envLevel_ = 0.0f;           ///< Current envelope level [0, 1]
    float envDecayCoeff_ = 0.999f;    ///< Per-sample exponential decay multiplier
    float velocity_ = 0.0f;           ///< Trigger velocity [0, 1]

    // Atomic parameters (set from UI thread, read from audio thread)
    std::atomic<float> color_{0.0f};      ///< Param 0: oscillator detuning
    std::atomic<float> tone_{0.5f};       ///< Param 1: BP1/BP2 crossfade
    std::atomic<float> decay_{200.0f};    ///< Param 2: envelope decay (ms)
    std::atomic<float> hpCutoff_{6000.0f}; ///< Param 3: HP filter cutoff (Hz)
};

} // namespace drums

#endif // GUITAR_ENGINE_METALLIC_VOICE_H
