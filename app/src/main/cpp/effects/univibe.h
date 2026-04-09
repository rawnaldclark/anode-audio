#ifndef GUITAR_ENGINE_UNIVIBE_H
#define GUITAR_ENGINE_UNIVIBE_H

#include "effect.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>

/**
 * Uni-Vibe emulation — rebuilt from scratch using a proven phaser core.
 *
 * Architecture based on the Ross Bencina / MusicDSP phaser reference
 * (the most widely tested phaser implementation in existence), adapted
 * with Uni-Vibe-specific characteristics:
 *
 *   - 4 cascaded first-order allpass filters (standard phaser topology)
 *   - Uni-Vibe capacitor values (15nF, 220nF, 470pF, 4.7nF) for
 *     non-uniform notch spacing across ~3 decades
 *   - ADDITIVE wet/dry mixing: output = dry + depth * wet
 *     (NOT a crossfade — the interference creates deep notches)
 *   - Asymmetric LFO through thermal lamp/LDR cascade
 *   - Chorus mode (dry + wet) vs Vibrato mode (wet only)
 *
 * The key insight from weeks of debugging: the wet/dry mixing MUST be
 * additive (output = dry + wet) for the phase cancellation to create
 * audible notches. A crossfade (output = mix*dry + (1-mix)*wet) keeps
 * output level constant and destroys the peak-to-null contrast.
 *
 * Parameter IDs:
 *   0 = Speed     (0.0-1.0) -> LFO rate [0.5, 12] Hz
 *   1 = Intensity (0.0-1.0) -> Modulation depth
 *   2 = Mode      (0.0-1.0) -> Chorus (<0.5) or Vibrato (>=0.5)
 */
class Univibe : public Effect {
public:
    Univibe() {
        resetState();
    }

    void process(float* buffer, int numFrames) override {
        if (sampleRate_ <= 0) return;

        const float speed     = speed_.load(std::memory_order_relaxed);
        const float intensity = intensity_.load(std::memory_order_relaxed);
        const float mode      = mode_.load(std::memory_order_relaxed);

        // LFO frequency: speed [0,1] -> [0.5, 12] Hz
        const double lfoFreq = 0.5 * std::pow(24.0, static_cast<double>(speed));
        const double phaseInc = lfoFreq / static_cast<double>(sampleRate_);

        const bool vibratoMode = (mode >= 0.5f);
        const double depth = static_cast<double>(intensity);

        for (int i = 0; i < numFrames; ++i) {
            const double input = static_cast<double>(buffer[i]);

            // ---- LFO ----
            // Asymmetric waveform (faster rise, slower fall) through
            // thermal lamp/LDR cascade for organic Uni-Vibe character
            const double lfoRaw = asymmetricLFO(lfoPhase_);
            lfoPhase_ += phaseInc;
            if (lfoPhase_ >= 1.0) lfoPhase_ -= 1.0;

            // Thermal cascade: LFO -> bulb -> LDR
            const double brightness = processLamp(lfoRaw);

            // ---- Compute allpass frequencies from LDR resistance ----
            // Map brightness [0,1] -> resistance [Rmax, Rmin] (log-space)
            // Then fc = 1/(2*pi*R*C) per stage
            const double logR = kLogRmax + brightness * (kLogRmin - kLogRmax);
            const double resistance = std::exp(logR);

            // ---- 4 cascaded allpass stages ----
            double wet = input;

            for (int s = 0; s < kNumStages; ++s) {
                // Per-stage slight mismatch (real LDRs are never identical)
                const double stageR = resistance * kLdrScales[s];
                const double fc = 1.0 / (kTwoPi * stageR * kCapacitors[s]);

                // Bilinear-transformed allpass coefficient
                // a = (1 - tan(pi*fc/sr)) / (1 + tan(pi*fc/sr))
                const double w = std::tan(kPi * fc / static_cast<double>(sampleRate_));
                const double a = (1.0 - w) / (1.0 + w);

                // First-order allpass: y[n] = a*x[n] + x[n-1] - a*y[n-1]
                const double y = a * wet + apX1_[s] - a * apY1_[s];
                apX1_[s] = wet;
                apY1_[s] = y;

                wet = y;
            }

            // ---- Output mixing ----
            double output;
            if (vibratoMode) {
                // Vibrato: wet only (brain perceives phase modulation as pitch)
                output = wet * 0.82;  // 47K/220K divider from original
            } else {
                // Chorus: ADDITIVE sum — this is THE critical line.
                // dry + wet creates interference: when the allpass chain shifts
                // wet by 180° at some frequency, dry + wet = 0 (deep notch).
                // When in phase: dry + wet = 2*dry (+6dB peak).
                // This peak-to-null contrast IS the phaser/Univibe sound.
                // depth controls how much wet is mixed in.
                output = input + depth * wet;

                // Scale to prevent clipping from additive sum
                output *= 0.55;
            }

            // Soft limit to prevent hard clipping
            buffer[i] = static_cast<float>(
                std::max(-1.0, std::min(1.0, output)));
        }
    }

    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;
        resetState();
    }

    void reset() override {
        resetState();
    }

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

    // Uni-Vibe capacitor values (the defining component choices)
    static constexpr double kCapacitors[kNumStages] = {
        15.0e-9, 220.0e-9, 470.0e-12, 4.7e-9
    };

    // Per-stage LDR mismatch (real CdS cells vary ~5-10%)
    static constexpr double kLdrScales[kNumStages] = { 1.0, 0.93, 1.07, 0.96 };

    // LDR resistance range
    // DAFx 2019 paper measured 6K-4.2M on originals.
    // Using 4K-500K for strong sweep range.
    static constexpr double kRmin = 4000.0;
    static constexpr double kRmax = 500000.0;
    static constexpr double kLogRmin = 8.29404964;   // ln(4000)
    static constexpr double kLogRmax = 13.12236338;  // ln(500000)

    // Thermal time constants
    static constexpr double kBulbAttackTau  = 0.015;  // 15ms heating
    static constexpr double kBulbReleaseTau = 0.050;  // 50ms cooling
    static constexpr double kLdrFastTau     = 0.005;  // 5ms dark-to-light
    static constexpr double kLdrSlowTau     = 0.120;  // 120ms light-to-dark

    // ---- Asymmetric LFO ----
    static double asymmetricLFO(double phase) {
        // Sine + harmonics for the asymmetric Uni-Vibe oscillator shape
        const double s = std::sin(kTwoPi * phase);
        const double h2 = 0.20 * std::sin(kTwoPi * phase * 2.0);
        const double h3 = 0.05 * std::sin(kTwoPi * phase * 3.0);
        return s + h2 + h3;
    }

    // ---- Lamp/LDR thermal cascade ----
    double processLamp(double lfoValue) {
        // Rectify to [0, 1]
        const double target = std::max(0.0, std::min(1.0, lfoValue * 0.5 + 0.5));

        // Bulb filament
        const double bulbCoeff = (target > bulbState_) ? bulbAttack_ : bulbRelease_;
        bulbState_ += bulbCoeff * (target - bulbState_);

        // CdS LDR
        const double ldrCoeff = (bulbState_ > ldrState_) ? ldrFast_ : ldrSlow_;
        ldrState_ += ldrCoeff * (bulbState_ - ldrState_);

        return std::max(0.001, std::min(0.999, ldrState_));
    }

    void resetState() {
        lfoPhase_ = 0.0;
        bulbState_ = 0.5;  // Start at midpoint (not dark!)
        ldrState_ = 0.5;   // Start at midpoint
        for (int s = 0; s < kNumStages; ++s) {
            apX1_[s] = 0.0;
            apY1_[s] = 0.0;
        }
        // Thermal coefficients
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
    double apX1_[kNumStages] = {};
    double apY1_[kNumStages] = {};

    // Thermal coefficients
    double bulbAttack_  = 0.0;
    double bulbRelease_ = 0.0;
    double ldrFast_     = 0.0;
    double ldrSlow_     = 0.0;
};

#endif // GUITAR_ENGINE_UNIVIBE_H
