#ifndef GUITAR_ENGINE_UNIVIBE_H
#define GUITAR_ENGINE_UNIVIBE_H

#include "effect.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>

/**
 * Shin-ei Uni-Vibe 1:1 digital recreation (1968, Fumio Mieda).
 *
 * The Uni-Vibe is NOT a standard phaser or chorus. Its unique character comes
 * from the interaction of three physical systems:
 *
 *   1. A tungsten INCANDESCENT BULB with thermal inertia (~15ms heating,
 *      ~50ms cooling) that cannot follow the LFO instantaneously.
 *
 *   2. Four CdS PHOTORESISTORS (LDRs) with their own response asymmetry
 *      (~5ms dark-to-light, ~120ms light-to-dark) that cascade with the
 *      bulb lag to create a lopsided, organic sweep envelope.
 *
 *   3. Four MISMATCHED ALLPASS stages with capacitors spanning nearly three
 *      decades (470pF to 220nF). The discrete BJT implementation creates
 *      NON-IDEAL allpass filters with 1-2 dB of frequency-dependent gain
 *      variation -- this is NOT a bug, it is THE defining characteristic.
 *      It creates a subtle modulated filter / frequency-dependent tremolo
 *      layered on top of the phase cancellation.
 *
 * The result: two moving notches (from 4 x 90deg = 360deg max phase shift)
 * with asymmetric sweep, non-uniform spacing, and amplitude modulation.
 * In chorus mode, dry+wet mixing creates the notches via phase cancellation.
 * In vibrato mode, only the phase-shifted signal is output.
 *
 * Circuit topology modeled (original Shin-ei/Univox schematic):
 *   1. Preamp (Q1-Q3, discrete BJT, gain ~4, soft saturation)
 *   2. Asymmetric LFO (phase-shift oscillator with harmonic skew)
 *   3. Incandescent bulb thermal model (28V/40mA grain-of-wheat)
 *   4. CdS LDR response model (cascaded with bulb, gamma=0.7 power law)
 *   5. Four non-ideal allpass stages (15nF, 220nF, 470pF, 4.7nF)
 *   6. Chorus (R35/R36 50/50 summing) or Vibrato (wet only) output
 *
 * References:
 *   - R.G. Keen, "The Technology of the Uni-Vibe" (geofex.com)
 *   - Electrosmash Uni-Vibe analysis
 *   - D. Yeh, CCRMA Stanford, photocell phase shifter modeling
 *   - Original Shin-ei/Univox traced schematics (freestompboxes.org)
 *
 * Parameter IDs for JNI access:
 *   0 = speed    (0.0 to 1.0) - LFO rate, maps to [0.5, 12] Hz
 *   1 = intensity (0.0 to 1.0) - depth of modulation
 *   2 = mode     (0.0 to 1.0) - Chorus (<0.5) or Vibrato (>=0.5)
 */
class Univibe : public Effect {
public:
    Univibe() {
        resetState();
    }

    /**
     * Process audio through the Uni-Vibe signal chain. Real-time safe.
     *
     * Per-sample algorithm:
     *   1. Soft-clip preamp (models Q1-Q3 discrete BJT gain stage)
     *   2. Advance LFO and generate asymmetric waveform
     *   3. Cascade through bulb thermal model -> LDR response model
     *   4. Convert brightness to LDR resistance per stage (with mismatch)
     *   5. Process through four non-ideal allpass filters
     *   6. Mix dry/wet per Chorus or Vibrato mode
     */
    void process(float* buffer, int numFrames) override {
        if (sampleRate_ <= 0) return;

        // Snapshot atomic parameters
        const float speed     = speed_.load(std::memory_order_relaxed);
        const float intensity = intensity_.load(std::memory_order_relaxed);
        const float mode      = mode_.load(std::memory_order_relaxed);

        // Map speed [0,1] -> LFO frequency [0.5, 12] Hz
        const double lfoFreq = 0.5 * std::pow(24.0, static_cast<double>(speed));
        const double phaseInc = lfoFreq / static_cast<double>(sampleRate_);

        const bool vibratoMode = (mode >= 0.5f);

        // Pre-compute constant for allpass coefficient calculation
        const double piOverSr = kPi / static_cast<double>(sampleRate_);
        const double maxFc = 0.45 * static_cast<double>(sampleRate_);

        // Intensity-dependent chorus mix: slightly favor wet to compensate
        // for ~1-3 dB gain loss through four BJT allpass stages.
        // At intensity=0: 50/50. At intensity=1: 46/54 (deeper notches).
        const double wetGain = 0.5 + 0.04 * static_cast<double>(intensity);
        const double dryGain = 1.0 - wetGain;

        for (int i = 0; i < numFrames; ++i) {
            // --- Step 1: Preamp with soft saturation ---
            // Original Uni-Vibe has a discrete BJT preamp (Q1-Q3) with
            // gain ~4. The BJTs soft-clip, adding warmth and odd harmonics.
            double dry = static_cast<double>(buffer[i]) * kPreampGain;
            dry = softSaturate(dry);

            // --- Step 2: Advance LFO ---
            const double lfoRaw = asymmetricLFO(lfoPhase_);
            lfoPhase_ += phaseInc;
            if (lfoPhase_ >= 1.0) lfoPhase_ -= 1.0;

            // --- Step 3: Cascaded bulb + LDR thermal model ---
            // Two-stage process: LFO -> bulb filament -> LDR photoconductor
            // Each has its own asymmetric response time, and they cascade
            // to create the Uni-Vibe's characteristic lopsided throb.
            const double brightness = lamp_.process(lfoRaw,
                bulbAttackCoeff_, bulbReleaseCoeff_,
                ldrFastCoeff_, ldrSlowCoeff_);

            // --- Step 4-5: Four non-ideal allpass stages ---
            double wet = dry;

            for (int s = 0; s < kNumStages; ++s) {
                // Per-stage LDR mismatch (real CdS cells are never matched)
                double stageBrightness = brightness * kLdrScales[s]
                                         + kLdrOffsets[s];
                stageBrightness = clampDouble(stageBrightness, 0.001, 0.999);

                // CdS photoresistor: brightness -> resistance (gamma=0.7)
                const double resistance = ldrResistance(stageBrightness);

                // Apply intensity: blend between fixed midpoint and modulated
                const double effectiveR = kMidpointResistance
                    + static_cast<double>(intensity)
                      * (resistance - kMidpointResistance);

                // Allpass cutoff from R*C
                double fc = 1.0 / (kTwoPi * effectiveR * stages_[s].capacitor);
                fc = clampDouble(fc, 1.0, maxFc);

                // Non-ideal allpass: phase shift + frequency-dependent gain
                // Models the discrete BJT (Darlington Q4/Q5 etc.) stages that
                // are NOT true unity-gain allpass. Each stage adds 1-2 dB of
                // gain variation that depends on the LDR state, creating a
                // subtle modulated filter / frequency-dependent tremolo.
                wet = stages_[s].process(wet, fc, piOverSr, stageBrightness);
            }

            // --- Step 6: Output mixing ---
            double output;
            if (vibratoMode) {
                // Vibrato: 100% wet (phase-shifted signal only)
                // Original uses 47K/220K divider = ~82% of wet
                output = wet * kVibratoOutputGain;
            } else {
                // Chorus: dry + wet summed through R35/R36 (100K each)
                // Compensated mix for deeper notch cancellation
                output = dryGain * dry + wetGain * wet;
            }

            buffer[i] = static_cast<float>(clampDouble(output, -1.0, 1.0));
        }
    }

    /**
     * Set sample rate and recompute all rate-dependent coefficients.
     *
     * Thermal model coefficients from physical time constants:
     *   Bulb:  heating ~15ms, cooling ~50ms (tungsten filament thermal mass)
     *   LDR:   dark-to-light ~5ms (fast photoconductor response)
     *          light-to-dark ~120ms (slow trap-state release in CdS crystal)
     */
    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;
        resetState();  // resetState() calls recomputeCoefficients() internally
    }

    void reset() override {
        resetState();
    }

    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0:
                speed_.store(clamp(value, 0.0f, 1.0f),
                             std::memory_order_relaxed);
                break;
            case 1:
                intensity_.store(clamp(value, 0.0f, 1.0f),
                                 std::memory_order_relaxed);
                break;
            case 2:
                mode_.store(clamp(value, 0.0f, 1.0f),
                            std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return speed_.load(std::memory_order_relaxed);
            case 1: return intensity_.load(std::memory_order_relaxed);
            case 2: return mode_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    // -----------------------------------------------------------------------
    // Constants
    // -----------------------------------------------------------------------

    static constexpr double kPi    = 3.14159265358979323846;
    static constexpr double kTwoPi = 6.28318530717958647692;

    /** Number of cascaded allpass stages. */
    static constexpr int kNumStages = 4;

    /**
     * Fixed capacitor values for each allpass stage (Farads).
     * Canonical Univox production values from traced schematics:
     *
     *   C7  = 15 nF   -> with R=100K: fc ~106 Hz  (low-mid body)
     *   C9  = 220 nF  -> with R=100K: fc ~7.2 Hz  (sub-audio throb)
     *   C13 = 470 pF  -> with R=100K: fc ~3386 Hz (high-freq shimmer)
     *   C16 = 4.7 nF  -> with R=100K: fc ~339 Hz  (midrange focus)
     *
     * The deliberate mismatch spanning ~3 decades creates the Uni-Vibe's
     * non-uniform notch spacing that sounds nothing like a regular phaser.
     */
    static constexpr double kCapacitors[kNumStages] = {
        15.0e-9, 220.0e-9, 470.0e-12, 4.7e-9
    };

    /**
     * Per-stage LDR mismatch scaling factors.
     * Real CdS photoresistors vary ~5-10% in sensitivity. This subtle
     * mismatch makes the sweep feel alive and organic.
     */
    static constexpr double kLdrScales[kNumStages]  = { 1.0, 0.92, 1.08, 0.96 };
    static constexpr double kLdrOffsets[kNumStages] = { 0.0, 0.02, -0.01, 0.03 };

    /**
     * CdS photoresistor resistance range (ohms).
     * Based on original MXY-7BX4 / NSL-7532 cells:
     *   - Fully illuminated: ~10K ohm (measured on original units)
     *   - Fully dark: ~1M ohm (datasheet maximum)
     *
     * R.G. Keen (geofex.com NeoVibe article) measured 50K-250K on
     * originals but recommended 10K-400K for better sweep.
     */
    static constexpr double kRmin = 10000.0;     // 10K ohm (bright)
    static constexpr double kRmax = 1000000.0;   // 1M ohm (dark)

    /** Pre-computed natural logs for log-space LDR interpolation. */
    static constexpr double kLogRmin = 9.210340371976184;    // ln(10000)
    static constexpr double kLogRmax = 13.815510557964274;   // ln(1000000)

    /**
     * Midpoint resistance when intensity = 0 (no modulation).
     * Geometric mean of Rmin and Rmax: sqrt(10K * 1M) = 100K ohm.
     */
    static constexpr double kMidpointResistance = 100000.0;

    /**
     * CdS power-law exponent (gamma).
     * Empirical relationship: R = k / L^gamma
     * For CdS cells, gamma is typically 0.7 (range 0.5-1.0).
     * This determines how time is distributed across the resistance range.
     */
    static constexpr double kCdsGamma = 0.7;

    /**
     * Preamp gain (~4x, matching Q1-Q3 discrete BJT stage).
     * Scaled down slightly because the signal is already normalized.
     */
    static constexpr double kPreampGain = 2.5;

    /**
     * Non-ideal allpass gain variation amount.
     * Models the ~1-2 dB frequency-dependent gain change in the
     * discrete BJT allpass stages. This creates the Uni-Vibe's
     * characteristic "frequency-dependent tremolo" that distinguishes
     * it from a clean phaser.
     */
    static constexpr double kGainVariation = 0.04;  // ~1.5 dB variation

    /**
     * Vibrato output attenuation: 47K/220K voltage divider in original circuit.
     * 220K / (47K + 220K) = 0.824, rounded to 0.82.
     */
    static constexpr double kVibratoOutputGain = 0.82;

    // --- Thermal time constants ---

    /** Bulb filament heating (fast): ~15ms */
    static constexpr double kBulbAttackTau = 0.015;
    /** Bulb filament cooling (slow): ~50ms */
    static constexpr double kBulbReleaseTau = 0.050;
    /** LDR dark-to-light (fast): ~5ms (photon excitation is near-instant) */
    static constexpr double kLdrFastTau = 0.005;
    /** LDR light-to-dark (slow): ~120ms (trap-state release in CdS crystal) */
    static constexpr double kLdrSlowTau = 0.120;

    // -----------------------------------------------------------------------
    // Internal types
    // -----------------------------------------------------------------------

    /**
     * Two-stage cascaded thermal model: bulb filament + CdS LDR.
     *
     * Stage 1 - Bulb: One-pole with split attack/release models the
     * tungsten filament thermal mass. Heating is faster than cooling
     * (Stefan-Boltzmann radiation law: P_radiated ~ T^4).
     *
     * Stage 2 - LDR: One-pole with split attack/release models the
     * CdS photoresistor response. Dark-to-light (resistance dropping)
     * is fast (~5ms, direct photon excitation). Light-to-dark
     * (resistance rising) is much slower (~120ms, carriers must
     * escape crystal trap states via thermal excitation).
     *
     * The cascade of these two asymmetric systems produces MUCH more
     * lopsided modulation than either alone: the sweep snaps quickly
     * to the bright position and oozes slowly back to dark.
     */
    struct LampModel {
        double bulbState = 0.0;  // Filament temperature [0, 1]
        double ldrState = 0.0;   // LDR brightness response [0, 1]

        double process(double lfoValue,
                        double bulbAttack, double bulbRelease,
                        double ldrFast, double ldrSlow) {
            // Rectify LFO to unipolar [0, 1] (lamp current -> brightness)
            // Clamp to physical domain: lamp current can't exceed supply rail,
            // LDR brightness can't go negative. Prevents thermal model from
            // tracking out-of-range targets when asymmetric LFO peaks > 1.0.
            const double target = std::max(0.0, std::min(1.0, lfoValue * 0.5 + 0.5));

            // Stage 1: Bulb filament thermal response
            const double bulbCoeff = (target > bulbState) ? bulbAttack : bulbRelease;
            bulbState += bulbCoeff * (target - bulbState);
            bulbState = fast_math::denormal_guard(bulbState);

            // Stage 2: CdS LDR photoconductor response
            // Fast when getting brighter (dark-to-light), slow when dimming
            const double ldrCoeff = (bulbState > ldrState) ? ldrFast : ldrSlow;
            ldrState += ldrCoeff * (bulbState - ldrState);
            ldrState = fast_math::denormal_guard(ldrState);

            return ldrState;
        }

        void reset() {
            bulbState = 0.0;
            ldrState = 0.0;
        }
    };

    /**
     * Non-ideal first-order allpass filter stage.
     *
     * Unlike a textbook allpass, the real Uni-Vibe's discrete BJT
     * (Darlington-connected) stages have frequency-dependent gain
     * variations of 1-2 dB. This creates a subtle amplitude modulation
     * layered on top of the phase shift -- almost like a frequency-
     * dependent tremolo. This is the single most important sonic
     * characteristic that separates a Uni-Vibe from a generic phaser.
     *
     * Transfer function (bilinear transform):
     *   H(z) = (a - z^-1) / (1 - a * z^-1)
     *   where a = (1 - w) / (1 + w), w = tan(pi * fc / sampleRate)
     */
    struct AllpassStage {
        double capacitor = 0.0;
        double x1 = 0.0;
        double y1 = 0.0;

        /**
         * Process one sample through the non-ideal allpass.
         *
         * @param input        Current input sample.
         * @param fc           Cutoff frequency in Hz.
         * @param piOverSr     Pre-computed pi / sampleRate.
         * @param brightness   Current LDR brightness [0,1] for gain modulation.
         * @return Processed output with phase shift + gain variation.
         */
        double process(double input, double fc, double piOverSr,
                        double brightness) {
            // Bilinear-transformed allpass coefficient
            const double w = fast_math::tan_approx(fc * piOverSr);
            const double a = (1.0 - w) / (1.0 + w);

            // Standard first-order allpass
            const double output = a * input + x1 - a * y1;

            x1 = input;
            y1 = fast_math::denormal_guard(output);

            // Non-ideal gain: BJT stages have ~1-2 dB gain variation
            // that modulates with the LDR state. When brightness is high
            // (low R, high fc), stages above fc are slightly attenuated.
            // When brightness is low (high R, low fc), they're boosted.
            // This creates the "throbbing" amplitude component.
            const double gain = 1.0 + kGainVariation * (2.0 * brightness - 1.0);
            return fast_math::denormal_guard(output * gain);
        }

        void reset() {
            x1 = 0.0;
            y1 = 0.0;
        }
    };

    // -----------------------------------------------------------------------
    // Asymmetric LFO
    // -----------------------------------------------------------------------

    /**
     * Generate the Uni-Vibe's asymmetric LFO waveform.
     *
     * The original phase-shift oscillator with back-to-back diodes produces
     * a rounded sawtooth: faster rise, slower fall. We model this with
     * harmonic content (2nd at 20%, 3rd at 5%) that interacts with the
     * cascaded bulb+LDR thermal lag to produce the signature wobbly sweep.
     */
    static double asymmetricLFO(double phase) {
        const double sine = static_cast<double>(
            fast_math::sin2pi(static_cast<float>(phase)));

        // 2nd harmonic (20%): creates the asymmetric rise/fall shape
        const double h2 = 0.20 * static_cast<double>(
            fast_math::sin2pi(static_cast<float>(phase * 2.0)));

        // 3rd harmonic (5%): adds the slight "pointed" character
        const double h3 = 0.05 * static_cast<double>(
            fast_math::sin2pi(static_cast<float>(phase * 3.0)));

        return sine + h2 + h3;
    }

    // -----------------------------------------------------------------------
    // CdS Photoresistor Model
    // -----------------------------------------------------------------------

    /**
     * Convert lamp brightness to CdS LDR resistance using the correct
     * gamma=0.7 power-law characteristic.
     *
     * CdS photoresistors follow: R = k / L^gamma where gamma ~ 0.7.
     * On a log-log scale this is linear with slope -gamma. The key
     * behavior: resistance changes are concentrated in the mid-range
     * (musically useful) rather than at the extremes.
     *
     * We interpolate in log-resistance space for the correct curve shape:
     *   log(R) = log(Rmax) + brightness^gamma * (log(Rmin) - log(Rmax))
     *
     * At brightness=0 (dark): R = Rmax = 1M ohm
     * At brightness=1 (bright): R = Rmin = 10K ohm
     */
    static double ldrResistance(double brightness) {
        const double b = std::max(brightness, 0.001);

        // Apply gamma curve: pow(b, 0.7) via exp2(0.7 * log2(b))
        // Uses fast_math approximations (~5-15x faster than std::pow).
        // 0.6% error is inaudible for LDR resistance mapping.
        const float t = fast_math::exp2_approx(
            static_cast<float>(kCdsGamma) *
            fast_math::fastLog2(static_cast<float>(b)));

        // Interpolate in log-resistance space (correct for power-law devices)
        // exp(x) = exp2(x * log2(e))
        constexpr double kLog2E = 1.4426950408889634;
        const double logR = kLogRmax + static_cast<double>(t) * (kLogRmin - kLogRmax);
        return static_cast<double>(
            fast_math::exp2_approx(static_cast<float>(logR * kLog2E)));
    }

    // -----------------------------------------------------------------------
    // Soft Saturation (Preamp Model)
    // -----------------------------------------------------------------------

    /**
     * Soft saturation modeling the Q1-Q3 discrete BJT preamp.
     * Uses rational function: x / (1 + |x|) for smooth, tube-like clipping.
     * This adds odd harmonics and warmth without harsh clipping artifacts.
     */
    static double softSaturate(double x) {
        return x / (1.0 + std::abs(x));
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    static float clamp(float v, float lo, float hi) {
        return std::max(lo, std::min(hi, v));
    }

    static double clampDouble(double v, double lo, double hi) {
        return std::max(lo, std::min(hi, v));
    }

    /**
     * Recompute per-sample thermal coefficients from time constants.
     * alpha = 1 - exp(-1 / (tau * sampleRate))
     */
    void recomputeCoefficients() {
        const double sr = static_cast<double>(sampleRate_);
        bulbAttackCoeff_  = 1.0 - std::exp(-1.0 / (kBulbAttackTau * sr));
        bulbReleaseCoeff_ = 1.0 - std::exp(-1.0 / (kBulbReleaseTau * sr));
        ldrFastCoeff_     = 1.0 - std::exp(-1.0 / (kLdrFastTau * sr));
        ldrSlowCoeff_     = 1.0 - std::exp(-1.0 / (kLdrSlowTau * sr));
    }

    void resetState() {
        lfoPhase_ = 0.0;
        lamp_.reset();

        for (int s = 0; s < kNumStages; ++s) {
            stages_[s].capacitor = kCapacitors[s];
            stages_[s].reset();
        }

        recomputeCoefficients();
    }

    // -----------------------------------------------------------------------
    // Parameters (atomic for lock-free UI <-> audio thread)
    // -----------------------------------------------------------------------

    std::atomic<float> speed_{0.3f};
    std::atomic<float> intensity_{0.7f};
    std::atomic<float> mode_{0.0f};

    // -----------------------------------------------------------------------
    // Audio-thread state
    // -----------------------------------------------------------------------

    int32_t sampleRate_ = 44100;
    double lfoPhase_ = 0.0;
    LampModel lamp_;
    AllpassStage stages_[kNumStages] = {};

    // Pre-computed thermal coefficients (sample-rate dependent)
    double bulbAttackCoeff_  = 0.0;
    double bulbReleaseCoeff_ = 0.0;
    double ldrFastCoeff_     = 0.0;
    double ldrSlowCoeff_     = 0.0;
};

#endif // GUITAR_ENGINE_UNIVIBE_H
