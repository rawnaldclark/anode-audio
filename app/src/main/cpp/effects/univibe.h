#ifndef GUITAR_ENGINE_UNIVIBE_H
#define GUITAR_ENGINE_UNIVIBE_H

#include "effect.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>

/**
 * Shin-ei Uni-Vibe emulation using the DAFx 2019 non-ideal allpass model.
 *
 * KEY INSIGHT: The Uni-Vibe's BJT stages are NOT true allpass filters.
 * Each stage has a ~10% gain imbalance between its inverting path (beta)
 * and non-inverting path (alpha). This creates AMPLITUDE MODULATION
 * (a frequency-dependent tremolo) across the entire guitar range,
 * not just phase-cancellation notches at specific frequencies.
 *
 * The "Univibe sound" is primarily this throbbing amplitude modulation,
 * with the phase cancellation notches as a secondary effect.
 *
 * Per-stage transfer function (from DAFx 2019, equations 4-5):
 *   H(w) = Hc(w) + He(w)
 *   Hc(w) = -beta * (jw) / (w0 + jw)     [highpass, inverting, gain beta]
 *   He(w) = alpha * (w0) / (w0 + jw)      [lowpass, non-inverting, gain alpha]
 *
 * When alpha = beta = 1: this reduces to a standard allpass.
 * When beta > alpha (real hardware): amplitude varies with frequency,
 * creating the throbbing modulation that defines the Univibe.
 *
 * Parameters:
 *   0 = Speed     (0.0-1.0) -> LFO rate [0.5, 12] Hz
 *   1 = Intensity (0.0-1.0) -> Modulation depth
 *   2 = Mode      (0.0-1.0) -> Chorus (<0.5) or Vibrato (>=0.5)
 */
class Univibe : public Effect {
public:
    Univibe() { resetState(); }

    void process(float* buffer, int numFrames) override {
        if (sampleRate_ <= 0) return;

        const float speed     = speed_.load(std::memory_order_relaxed);
        const float intensity = intensity_.load(std::memory_order_relaxed);
        const float mode      = mode_.load(std::memory_order_relaxed);

        const double lfoFreq = 0.5 * std::pow(24.0, static_cast<double>(speed));
        const double phaseInc = lfoFreq / static_cast<double>(sampleRate_);
        const bool vibratoMode = (mode >= 0.5f);
        const double depth = static_cast<double>(intensity);
        const double sr = static_cast<double>(sampleRate_);

        for (int i = 0; i < numFrames; ++i) {
            const double input = static_cast<double>(buffer[i]);

            // ---- LFO with thermal cascade ----
            const double lfoRaw = asymmetricLFO(lfoPhase_);
            lfoPhase_ += phaseInc;
            if (lfoPhase_ >= 1.0) lfoPhase_ -= 1.0;
            const double brightness = processLamp(lfoRaw);

            // ---- LDR resistance from brightness ----
            const double logR = kLogRmax + brightness * (kLogRmin - kLogRmax);
            const double resistance = std::exp(logR);

            // ---- 4 cascaded NON-IDEAL allpass stages ----
            // Each stage splits into lowpass (He, gain=alpha) + highpass (Hc, gain=beta).
            // beta > alpha by ~10%, creating frequency-dependent amplitude modulation.
            double wet = input;

            for (int s = 0; s < kNumStages; ++s) {
                const double stageR = resistance * kLdrScales[s];
                double fc = 1.0 / (kTwoPi * stageR * kCapacitors[s]);
                fc = std::max(20.0, std::min(fc, sr * 0.45));

                // One-pole lowpass coefficient for this stage's break frequency
                // LP: y[n] = (1-g)*x[n] + g*y[n-1], where g = exp(-2*pi*fc/sr)
                const double g = std::exp(-kTwoPi * fc / sr);

                // Lowpass path: He (non-inverting, gain = alpha)
                lpState_[s] = (1.0 - g) * wet + g * lpState_[s];

                // Highpass path: Hc (inverting, gain = -beta)
                // HP = input - LP
                const double hp = wet - lpState_[s];

                // Non-ideal allpass: alpha * LP - beta * HP
                // alpha ~1.0, beta ~1.10 (measured from DAFx paper Table 1)
                // The gain imbalance creates amplitude modulation:
                //   Below fc: output ≈ alpha * input (slightly attenuated)
                //   Above fc: output ≈ -beta * input (inverted, slightly boosted)
                //   At fc: transition region with maximum amplitude variation
                const double stageOut = kAlpha[s] * lpState_[s] - kBeta[s] * hp;

                wet = stageOut;
            }

            // ---- Output mixing ----
            double output;
            if (vibratoMode) {
                output = wet * 0.82;
            } else {
                // Additive sum: dry + depth * wet
                // The non-ideal allpass stages now produce BOTH:
                //   1. Phase shifts that create sweeping notches/peaks via interference
                //   2. Amplitude modulation from the alpha/beta imbalance
                // Together these create the classic Univibe throb.
                output = input + depth * wet;
                output *= 0.5;
            }

            buffer[i] = static_cast<float>(
                std::max(-1.0, std::min(1.0, output)));
        }
    }

    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;
        resetState();
    }

    void reset() override { resetState(); }

    void setParameter(int paramId, float value) override {
        value = std::max(0.0f, std::min(1.0f, value));
        switch (paramId) {
            case 0: speed_.store(value, std::memory_order_relaxed); break;
            case 1: intensity_.store(value, std::memory_order_relaxed); break;
            case 2: mode_.store(value, std::memory_order_relaxed); break;
            default: break;
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
    static constexpr double kPi    = 3.14159265358979323846;
    static constexpr double kTwoPi = 6.28318530717958647692;
    static constexpr int kNumStages = 4;

    // Uni-Vibe capacitor values (traced from original Shin-ei schematics)
    static constexpr double kCapacitors[kNumStages] = {
        15.0e-9, 220.0e-9, 470.0e-12, 4.7e-9
    };

    // Per-stage alpha/beta gains (DAFx 2019 Table 1 measurements)
    // alpha = non-inverting (lowpass) path gain
    // beta = inverting (highpass) path gain
    // The ~10% imbalance creates the Univibe's signature amplitude modulation
    static constexpr double kAlpha[kNumStages] = { 1.01, 0.98, 0.97, 0.95 };
    static constexpr double kBeta[kNumStages]  = { 1.11, 1.09, 1.10, 1.09 };

    // Per-stage LDR mismatch
    static constexpr double kLdrScales[kNumStages] = { 1.0, 0.93, 1.07, 0.96 };

    // LDR resistance range (wider than before — DAFx measured up to 4.2M)
    static constexpr double kRmin = 4000.0;
    static constexpr double kRmax = 500000.0;
    static constexpr double kLogRmin = 8.29404964;   // ln(4000)
    static constexpr double kLogRmax = 13.12236338;  // ln(500000)

    // Thermal time constants
    static constexpr double kBulbAttackTau  = 0.015;
    static constexpr double kBulbReleaseTau = 0.050;
    static constexpr double kLdrFastTau     = 0.005;
    static constexpr double kLdrSlowTau     = 0.120;

    // ---- LFO ----
    static double asymmetricLFO(double phase) {
        const double s  = std::sin(kTwoPi * phase);
        const double h2 = 0.20 * std::sin(kTwoPi * phase * 2.0);
        const double h3 = 0.05 * std::sin(kTwoPi * phase * 3.0);
        return s + h2 + h3;
    }

    // ---- Lamp/LDR thermal cascade ----
    double processLamp(double lfoValue) {
        const double target = std::max(0.0, std::min(1.0, lfoValue * 0.5 + 0.5));
        const double bulbCoeff = (target > bulbState_) ? bulbAttack_ : bulbRelease_;
        bulbState_ += bulbCoeff * (target - bulbState_);
        const double ldrCoeff = (bulbState_ > ldrState_) ? ldrFast_ : ldrSlow_;
        ldrState_ += ldrCoeff * (bulbState_ - ldrState_);
        return std::max(0.001, std::min(0.999, ldrState_));
    }

    void resetState() {
        lfoPhase_ = 0.0;
        bulbState_ = 0.5;
        ldrState_ = 0.5;
        for (int s = 0; s < kNumStages; ++s) {
            lpState_[s] = 0.0;
        }
        if (sampleRate_ > 0) {
            const double sr = static_cast<double>(sampleRate_);
            bulbAttack_  = 1.0 - std::exp(-1.0 / (kBulbAttackTau * sr));
            bulbRelease_ = 1.0 - std::exp(-1.0 / (kBulbReleaseTau * sr));
            ldrFast_     = 1.0 - std::exp(-1.0 / (kLdrFastTau * sr));
            ldrSlow_     = 1.0 - std::exp(-1.0 / (kLdrSlowTau * sr));
        }
    }

    // ---- Parameters ----
    std::atomic<float> speed_{0.3f};
    std::atomic<float> intensity_{0.7f};
    std::atomic<float> mode_{0.0f};

    // ---- Audio state ----
    int32_t sampleRate_ = 44100;
    double lfoPhase_ = 0.0;
    double bulbState_ = 0.5;
    double ldrState_ = 0.5;
    double lpState_[kNumStages] = {};  // One-pole LP state per stage

    // Thermal coefficients
    double bulbAttack_  = 0.0;
    double bulbRelease_ = 0.0;
    double ldrFast_     = 0.0;
    double ldrSlow_     = 0.0;
};

#endif // GUITAR_ENGINE_UNIVIBE_H
