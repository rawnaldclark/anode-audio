#ifndef GUITAR_ENGINE_WAH_H
#define GUITAR_ENGINE_WAH_H

#include "effect.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>

/**
 * Auto-wah effect using an envelope follower driving a TPT bandpass filter.
 *
 * Why TPT (Topology-Preserving Transform) instead of biquad:
 *   Biquad coefficient changes cause discontinuities ("zipper noise") when
 *   the cutoff frequency is modulated rapidly. TPT filters maintain the
 *   analog circuit topology during discretization, so frequency sweeps
 *   are smooth and artifact-free -- essential for a wah effect where the
 *   filter frequency changes on every sample.
 *
 * The envelope follower rectifies the input signal and smooths it with
 * attack/release filtering. The resulting envelope is mapped exponentially
 * to the filter cutoff frequency: playing harder sweeps the wah up,
 * playing softly lets it sweep back down.
 *
 * TPT SVF formula (same as parametric EQ SVF but with per-sample coefficient updates):
 *   g = tan(pi * cutoff / sampleRate)
 *   k = 1/Q  (high Q = narrow, resonant sweep)
 *
 * Three wah type models:
 *   Auto (default): Generic auto-wah, base freq * 8 sweep, standard envelope
 *   CryBaby/Vox:    Inductor-style, narrow 450-1600Hz sweep, high Q (x1.3),
 *                    fast 3ms attack, warm mix (bp*0.7 + lp*0.3)
 *   WH-10/Ibanez:   Op-amp MFB, wide 200-5000Hz sweep, consistent gain Q (x0.8),
 *                    slow 150ms release, bassy mix (bp*0.6 + lp*0.4)
 *
 * Parameter IDs for JNI access:
 *   0 = sensitivity (0.0-1.0) - how much the envelope controls the sweep
 *   1 = frequency (200-4000 Hz) - base/center frequency
 *   2 = resonance (0.5-10.0) - Q factor of the bandpass
 *   3 = mode (0.0=auto-wah, 1.0=fixed) - auto follows envelope, fixed stays put
 *   4 = wahType (0.0=Auto, 1.0=CryBaby, 2.0=WH-10) - wah voicing model
 */
class Wah : public Effect {
public:
    Wah() = default;

    void process(float* buffer, int numFrames) override {
        // Load all parameters once per block (atomic relaxed reads)
        const float sensitivity = sensitivity_.load(std::memory_order_relaxed);
        const float baseFreq = frequency_.load(std::memory_order_relaxed);
        const float resonance = resonance_.load(std::memory_order_relaxed);
        const float mode = mode_.load(std::memory_order_relaxed);
        const float wahType = wahType_.load(std::memory_order_relaxed);
        const bool autoMode = (mode < 0.5f);
        const int wahTypeInt = static_cast<int>(wahType + 0.5f);

        const double sr = static_cast<double>(sampleRate_);

        // Select envelope coefficients and voicing parameters based on wah type.
        // All three coefficient sets are pre-computed in setSampleRate() to avoid
        // std::exp in the per-sample loop.
        double atkCoeff, relCoeff;
        double bpMix, lpMix;
        double qMultiplier;

        switch (wahTypeInt) {
            case 1: // CryBaby/Vox - inductor-style resonant bandpass
                atkCoeff = cryBabyAttackCoeff_;
                relCoeff = cryBabyReleaseCoeff_;
                bpMix = 0.7;   // Warmer: more body from lowpass
                lpMix = 0.3;
                qMultiplier = 2.0; // Higher Q for vocal resonant peak (real CryBaby Q=5-10)
                break;

            case 2: // WH-10/Ibanez - op-amp multiple-feedback bandpass
                atkCoeff = wh10AttackCoeff_;
                relCoeff = wh10ReleaseCoeff_;
                bpMix = 0.6;   // Bassier: even more lowpass content
                lpMix = 0.4;
                qMultiplier = 0.8; // Lower Q, consistent gain across sweep
                break;

            default: // Auto (standard) - generic auto-wah
                atkCoeff = autoAttackCoeff_;
                relCoeff = autoReleaseCoeff_;
                bpMix = 0.8;   // Standard bandpass-dominant mix
                lpMix = 0.2;
                qMultiplier = 1.0; // No Q modification
                break;
        }

        // Compute effective Q: user resonance scaled by wah type multiplier
        const double effectiveQ = static_cast<double>(resonance) * qMultiplier;
        // k = 1/Q for SVF damping coefficient
        const double k = 1.0 / std::max(effectiveQ, 0.1); // Floor at 0.1 to prevent div-by-zero

        // Pre-compute frequency sweep range outside per-sample loop (block-invariant).
        // These depend only on wahType and baseFreq, not per-sample envelope state.
        double minF, maxF;
        switch (wahTypeInt) {
            case 1: { // CryBaby - Fasel inductor sweep (350-2200Hz)
                minF = std::max(350.0, static_cast<double>(baseFreq) * 0.7);
                maxF = std::min(2200.0, minF * 6.3);
                break;
            }
            case 2: { // WH-10 - wide op-amp MFB sweep (200-4000Hz)
                minF = std::max(200.0, static_cast<double>(baseFreq) * 0.5);
                maxF = std::min(std::min(4000.0, sr * 0.45), minF * 20.0);
                break;
            }
            default: { // Auto - standard sweep (base freq * 8)
                minF = static_cast<double>(baseFreq);
                maxF = std::min(minF * 8.0, sr * 0.45);
                break;
            }
        }
        // Prevent degenerate ratio (minF >= maxF would cause log2 issues)
        minF = std::max(minF, 1.0);
        if (maxF <= minF) maxF = minF + 1.0;

        // Pre-compute log2 ratio for exponential frequency mapping via exp2_approx
        const double log2Ratio = std::log2(maxF / minF);

        for (int i = 0; i < numFrames; ++i) {
            double input = static_cast<double>(buffer[i]);

            // Determine cutoff frequency
            double cutoff;
            if (autoMode) {
                // Envelope follower: rectify and smooth
                double rectified = std::abs(input);
                if (rectified > envelope_) {
                    // Attack phase
                    envelope_ += atkCoeff * (rectified - envelope_);
                } else {
                    // Release phase
                    envelope_ += relCoeff * (rectified - envelope_);
                }
                // Prevent denormals in envelope decay path
                envelope_ = fast_math::denormal_guard(envelope_);

                // Map envelope to frequency (exponential mapping)
                // Sensitivity controls how far the envelope sweeps
                double envScaled = envelope_ * static_cast<double>(sensitivity) * 10.0;
                envScaled = std::min(envScaled, 1.0); // Clamp to [0,1]

                // Exponential frequency mapping using fast exp2 approximation
                // (replaces std::pow in per-sample loop)
                cutoff = minF * fast_math::exp2_approx(
                    static_cast<float>(envScaled * log2Ratio));
            } else {
                // Fixed mode: use the frequency parameter directly
                cutoff = static_cast<double>(baseFreq);
            }

            // Clamp cutoff to safe range (above 20Hz, below Nyquist * 0.45)
            cutoff = std::max(20.0, std::min(cutoff, sr * 0.45));

            // TPT SVF bandpass filter
            // Per-sample coefficient update is safe with TPT topology (no zipper noise)
            double g = fast_math::tan_approx(kPi * cutoff / sr);

            // Cytomic SVF
            double hp = (input - (k + g) * z1_ - z2_) / (1.0 + k * g + g * g);
            double bp = g * hp + z1_;
            double lp = g * bp + z2_;

            // State update
            z1_ = 2.0 * bp - z1_;
            z2_ = 2.0 * lp - z2_;

            // Output: blend bandpass (wah character) with lowpass (body).
            // Mix ratio varies by wah type to capture each model's tonal signature.
            double output = bp * bpMix + lp * lpMix;
            buffer[i] = fast_math::softLimit(static_cast<float>(output));
        }
    }

    /**
     * Pre-compute envelope follower coefficients for all three wah types.
     * This avoids std::exp calls in the per-sample loop.
     *
     * Envelope time constants:
     *   Auto:    attack ~5ms,  release ~100ms (standard)
     *   CryBaby: attack ~3ms,  release ~100ms (faster attack for responsive feel)
     *   WH-10:   attack ~5ms,  release ~150ms (slower release for vocal/quacky quality)
     */
    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;
        const double sr = static_cast<double>(sampleRate);

        // Auto (standard): 5ms attack, 100ms release
        autoAttackCoeff_  = 1.0 - std::exp(-1.0 / (0.005 * sr));
        autoReleaseCoeff_ = 1.0 - std::exp(-1.0 / (0.100 * sr));

        // CryBaby/Vox: 3ms attack (faster response), 100ms release
        cryBabyAttackCoeff_  = 1.0 - std::exp(-1.0 / (0.003 * sr));
        cryBabyReleaseCoeff_ = 1.0 - std::exp(-1.0 / (0.100 * sr));

        // WH-10/Ibanez: 5ms attack, 150ms release (slower decay = more vocal quality)
        wh10AttackCoeff_  = 1.0 - std::exp(-1.0 / (0.005 * sr));
        wh10ReleaseCoeff_ = 1.0 - std::exp(-1.0 / (0.150 * sr));

        resetState();
    }

    void reset() override {
        resetState();
    }

    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0: sensitivity_.store(clamp(value, 0.0f, 1.0f), std::memory_order_relaxed); break;
            case 1: frequency_.store(clamp(value, 200.0f, 4000.0f), std::memory_order_relaxed); break;
            case 2: resonance_.store(clamp(value, 0.5f, 10.0f), std::memory_order_relaxed); break;
            case 3: mode_.store(clamp(value, 0.0f, 1.0f), std::memory_order_relaxed); break;
            case 4: wahType_.store(clamp(value, 0.0f, 2.0f), std::memory_order_relaxed); break;
            default: break;
        }
    }

    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return sensitivity_.load(std::memory_order_relaxed);
            case 1: return frequency_.load(std::memory_order_relaxed);
            case 2: return resonance_.load(std::memory_order_relaxed);
            case 3: return mode_.load(std::memory_order_relaxed);
            case 4: return wahType_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    static constexpr double kPi = 3.14159265358979323846;

    static float clamp(float v, float lo, float hi) {
        return std::max(lo, std::min(hi, v));
    }

    void resetState() {
        z1_ = 0.0;
        z2_ = 0.0;
        envelope_ = 0.0;
    }

    // Parameters (atomics for lock-free access from audio + UI threads)
    std::atomic<float> sensitivity_{0.7f};
    std::atomic<float> frequency_{500.0f};
    std::atomic<float> resonance_{3.0f};
    std::atomic<float> mode_{0.0f};    // 0=auto, 1=fixed
    std::atomic<float> wahType_{0.0f}; // 0=Auto, 1=CryBaby, 2=WH-10

    int32_t sampleRate_ = 44100;

    // TPT SVF state (64-bit for stability during fast sweeps)
    double z1_ = 0.0;
    double z2_ = 0.0;

    // Envelope follower state
    double envelope_ = 0.0;

    // Pre-computed envelope coefficients for each wah type.
    // Computed in setSampleRate() to avoid std::exp in the per-sample loop.

    // Auto (standard): 5ms attack, 100ms release
    double autoAttackCoeff_ = 0.0;
    double autoReleaseCoeff_ = 0.0;

    // CryBaby/Vox: 3ms attack, 100ms release (faster attack for responsive pick dynamics)
    double cryBabyAttackCoeff_ = 0.0;
    double cryBabyReleaseCoeff_ = 0.0;

    // WH-10/Ibanez: 5ms attack, 150ms release (slower release for vocal/quacky quality)
    double wh10AttackCoeff_ = 0.0;
    double wh10ReleaseCoeff_ = 0.0;
};

#endif // GUITAR_ENGINE_WAH_H
