#ifndef GUITAR_ENGINE_REVERB_H
#define GUITAR_ENGINE_REVERB_H

#include "effect.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * Multi-mode reverb effect: Plate (Dattorro) + Spring (chirp allpass dispersion).
 *
 * MODE 0 (Plate): Jon Dattorro's 1997 plate reverb algorithm with figure-8
 * topology, modulated delay lines, and cascaded input diffusion. Controlled by
 * params 0-3 (decay, damping, preDelay, size).
 *
 * MODE 1 (Spring 2): 2-spring tank emulation using Algorithm B from the
 * research report -- chirp allpass dispersion chains with graduated coefficients,
 * mutually prime delay lines, input/output transducer EQ, and per-spring
 * feedback damping. Produces the classic Fender "drip" and "surf" character.
 *
 * MODE 2 (Spring 3): 3-spring tank emulation. Smoother, denser tail with
 * less pronounced drip -- the three chirps partially mask each other.
 *
 * Spring signal flow (per spring):
 *   Input -> Dwell drive (tanh saturation) -> Input transducer LPF (4.5kHz)
 *         -> [Allpass dispersion chain: 12 cascaded 1st-order allpass filters
 *             with graduated coefficients 0.30-0.84]
 *         -> Delay line (mutually prime lengths per spring)
 *         -> Damping LPF + HPF (64-bit state) -> Feedback scaling
 *         -> Output -> Pickup resonance EQ -> DC blocker -> Sum springs
 *
 * Parameter IDs for JNI access (Hall of Fame 2 layout):
 *   0 = decay (0.0 to 1.0) - normalized reverb tail length, maps to 0.1-10s
 *       In Digital mode, scales the Type preset's base decay time.
 *   1 = damping (0.0 to 1.0) - internal HF absorption (set by Type, hidden)
 *   2 = preDelay (0 to 100 ms) - internal gap before reverb (set by Type, hidden)
 *   3 = size (0.0 to 1.0) - internal room size (set by Type, hidden)
 *   4 = mode (0=Digital, 1=Spring2, 2=Spring3)
 *   5 = dwell (0.0 to 1.0) - input drive to spring tank (Spring modes)
 *   6 = tone (0.0 to 1.0) - brightness control (both Digital and Spring modes)
 *       Digital: modulates damping (0=dark, 1=bright). Spring: LPF cutoff.
 *   7 = springDecay (0.0 to 1.0) - spring reverb time (Spring modes)
 *   8 = digitalType (0=Room, 1=Hall, 2=Church, 3=Plate, 4=Lo-Fi, 5=Mod)
 *       When changed, auto-configures ALL internal Dattorro parameters.
 *       Shimmer (6) reserved for future (needs pitch shifter).
 */
class Reverb : public Effect {
public:
    Reverb() {
        // Pre-compute default decay coefficient from the default 2.0s decay time
        updateDecayCoeff(2.0f);
    }

    // =========================================================================
    // Effect interface
    // =========================================================================

    /**
     * Process audio through the selected reverb mode. Real-time safe.
     * Routes to processDattorro() or processSpring() based on mode parameter.
     */
    void process(float* buffer, int numFrames) override {
        // Guard against being called before setSampleRate()
        if (preDelayBuffer_.empty()) return;

        const int mode = static_cast<int>(mode_.load(std::memory_order_relaxed));

        if (mode == kModeSpring2 || mode == kModeSpring3) {
            processSpring(buffer, numFrames, mode);
        } else {
            processDattorro(buffer, numFrames);
        }
    }

    /**
     * Configure sample rate and pre-allocate all delay line buffers.
     * This is the ONLY allocation point -- never on the audio thread.
     */
    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;

        // Scale factor from Dattorro's 29761Hz reference rate to device rate
        rateScale_ = static_cast<float>(sampleRate) / 29761.0f;

        // ---- Dattorro plate allocations (unchanged) ----
        allocateDattorroBuffers(sampleRate);

        // ---- Spring reverb allocations ----
        allocateSpringBuffers(sampleRate);

        // Recompute decay coefficient (loop time depends on sample rate and size)
        updateDecayCoeff(decayTime_.load(std::memory_order_relaxed),
                         0.5f + size_.load(std::memory_order_relaxed));

        // Recompute spring damping filters for new sample rate
        updateSpringDamping();

        // Recompute transducer EQ biquad coefficients
        computeTransducerEQ(sampleRate);

        resetState();
    }

    void reset() override {
        resetState();
    }

    // =========================================================================
    // Parameter access (called from UI thread via JNI)
    // =========================================================================

    /**
     * Set a parameter by ID. Thread-safe.
     * @param paramId  0=decay, 1=damping, 2=preDelay, 3=size,
     *                 4=mode, 5=dwell, 6=tone, 7=springDecay
     */
    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0: {
                // Decay: normalized 0-1 input, maps to 0.1-10.0s internally.
                // In Digital mode, the Type preset sets a base decay; this knob
                // scales from 10% to 200% of that base for user control.
                float normalized = clamp(value, 0.0f, 1.0f);
                decayNorm_.store(normalized, std::memory_order_relaxed);
                // Map: 0->0.1s, 0.5->~2.0s, 1.0->10.0s (exponential curve)
                float decayTimeSec = 0.1f * std::pow(100.0f, normalized);
                decayTime_.store(decayTimeSec, std::memory_order_relaxed);
                updateDecayCoeff(decayTimeSec,
                                 0.5f + size_.load(std::memory_order_relaxed));
                break;
            }
            case 1:
                damping_.store(clamp(value, 0.0f, 1.0f),
                               std::memory_order_relaxed);
                break;
            case 2:
                preDelay_.store(clamp(value, 0.0f, 100.0f),
                                std::memory_order_relaxed);
                break;
            case 3: {
                float s = clamp(value, 0.0f, 1.0f);
                size_.store(s, std::memory_order_relaxed);
                updateDecayCoeff(decayTime_.load(std::memory_order_relaxed),
                                 0.5f + s);
                break;
            }
            case 4: {
                // Mode: 0=Digital, 1=Spring2, 2=Spring3
                int newMode = static_cast<int>(clamp(value, 0.0f, 2.0f) + 0.5f);
                int oldMode = static_cast<int>(mode_.load(std::memory_order_relaxed));
                mode_.store(static_cast<float>(newMode), std::memory_order_relaxed);
                // Reset state on mode change to prevent cross-mode artifacts
                if (newMode != oldMode) {
                    resetState();
                }
                break;
            }
            case 5:
                // Dwell: input drive to spring tank (0-1)
                springDwell_.store(clamp(value, 0.0f, 1.0f),
                                   std::memory_order_relaxed);
                break;
            case 6:
                // Tone: brightness control for BOTH Digital and Spring modes.
                // Digital: modulates damping (0=very dark, 1=very bright).
                // Spring: LPF cutoff scaling.
                springTone_.store(clamp(value, 0.0f, 1.0f),
                                  std::memory_order_relaxed);
                updateSpringDamping();
                // In Digital mode, Tone overrides damping for user control
                {
                    float t = clamp(value, 0.0f, 1.0f);
                    // Tone 0 = very dark (damping=0.9), Tone 1 = bright (damping=0.05)
                    float digitalDamping = 0.9f - t * 0.85f;
                    damping_.store(digitalDamping, std::memory_order_relaxed);
                }
                break;
            case 7:
                // Spring Decay: reverb time (0-1, maps to feedback 0.7-0.95)
                springDecay_.store(clamp(value, 0.0f, 1.0f),
                                   std::memory_order_relaxed);
                break;
            case 8: {
                // Digital Type: HOF2 preset voicing that auto-configures Dattorro.
                // 0=Room, 1=Hall, 2=Church, 3=Plate, 4=Lo-Fi, 5=Mod
                int type = static_cast<int>(clamp(value, 0.0f,
                    static_cast<float>(kDigitalTypeMax)) + 0.5f);
                digitalType_.store(static_cast<float>(type), std::memory_order_relaxed);
                applyDigitalTypePreset(type);
                break;
            }
            default:
                break;
        }
    }

    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return decayNorm_.load(std::memory_order_relaxed);
            case 1: return damping_.load(std::memory_order_relaxed);
            case 2: return preDelay_.load(std::memory_order_relaxed);
            case 3: return size_.load(std::memory_order_relaxed);
            case 4: return mode_.load(std::memory_order_relaxed);
            case 5: return springDwell_.load(std::memory_order_relaxed);
            case 6: return springTone_.load(std::memory_order_relaxed);
            case 7: return springDecay_.load(std::memory_order_relaxed);
            case 8: return digitalType_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    // =========================================================================
    // Mode constants
    // =========================================================================
    static constexpr int kModeDigital = 0;
    static constexpr int kModeSpring2 = 1;
    static constexpr int kModeSpring3 = 2;

    // =========================================================================
    // Digital type preset constants (Hall of Fame 2 modes)
    // =========================================================================
    static constexpr int kDigitalRoom    = 0;
    static constexpr int kDigitalHall    = 1;
    static constexpr int kDigitalChurch  = 2;
    static constexpr int kDigitalPlate   = 3;
    static constexpr int kDigitalLoFi    = 4;
    static constexpr int kDigitalMod     = 5;
    static constexpr int kDigitalTypeMax = 5;

    // =========================================================================
    // Dattorro plate constants (unchanged from original)
    // =========================================================================
    static constexpr float kInputDiffCoeff1 = 0.75f;
    static constexpr float kInputDiffCoeff2 = 0.625f;
    static constexpr float kDecayDiffCoeff1 = 0.7f;
    static constexpr float kDecayDiffCoeff2 = 0.5f;
    static constexpr float kTankADelay = 4453.0f;
    static constexpr float kTankADelay2 = 3720.0f;
    static constexpr float kTankBDelay = 4217.0f;
    static constexpr float kTankBDelay2 = 3163.0f;
    static constexpr float kAvgLoopTime =
        (kTankADelay + kTankADelay2 + kTankBDelay + kTankBDelay2) / 29761.0f;
    static constexpr float kLfoRateA = 0.5f;
    static constexpr float kLfoRateB = 0.31f;
    static constexpr float kModDepth = 16.0f;

    // =========================================================================
    // Spring reverb constants
    // =========================================================================

    /** Maximum number of springs supported (3-spring tank). */
    static constexpr int kMaxSprings = 3;

    /** Number of allpass dispersion stages per spring (from research report). */
    static constexpr int kSpringAllpassCount = 12;

    /**
     * Base delay line lengths at 48kHz (mutually prime) from research report.
     * Spring 1: shortest/brightest, Spring 2: medium, Spring 3: longest/darkest.
     */
    static constexpr int kSpringBaseDelay[kMaxSprings] = { 1109, 1543, 1831 };

    /**
     * Graduated allpass dispersion coefficients per spring.
     * These create frequency-dependent group delay that models the spring's
     * dispersive wave propagation -- high frequencies arrive first (chirp).
     * Values from research report Section 3.2.
     */
    static constexpr float kSpringAllpassCoeffs[kMaxSprings][kSpringAllpassCount] = {
        // Spring 1 (shortest, brightest): coeffs 0.30-0.80
        { 0.30f, 0.34f, 0.38f, 0.43f, 0.48f, 0.53f, 0.59f, 0.64f, 0.69f, 0.74f, 0.79f, 0.80f },
        // Spring 2 (medium): coeffs 0.32-0.82
        { 0.32f, 0.36f, 0.41f, 0.46f, 0.51f, 0.56f, 0.61f, 0.66f, 0.71f, 0.76f, 0.80f, 0.82f },
        // Spring 3 (longest, darkest): coeffs 0.34-0.84
        { 0.34f, 0.39f, 0.44f, 0.49f, 0.54f, 0.59f, 0.64f, 0.69f, 0.73f, 0.77f, 0.81f, 0.84f }
    };

    /**
     * Base damping LPF cutoff frequencies per spring (Hz).
     * Shorter spring = brighter. Values from research report Section 3.2.
     */
    static constexpr float kSpringDampLPF_fc[kMaxSprings] = { 5500.0f, 4800.0f, 4200.0f };

    /**
     * Base damping HPF cutoff frequencies per spring (Hz).
     * Prevents DC buildup in feedback loop.
     */
    static constexpr float kSpringDampHPF_fc[kMaxSprings] = { 25.0f, 30.0f, 35.0f };

    // =========================================================================
    // Dattorro plate processing (original, unchanged)
    // =========================================================================

    void processDattorro(float* buffer, int numFrames) {
        const float decayCoeff = decayCoeff_.load(std::memory_order_relaxed);
        const float damping = damping_.load(std::memory_order_relaxed);
        const float preDelayMs = preDelay_.load(std::memory_order_relaxed);
        const float size = size_.load(std::memory_order_relaxed);
        const bool isLoFi = loFiEnabled_.load(std::memory_order_relaxed);
        const bool isMod = modEnabled_.load(std::memory_order_relaxed);
        const float modDepthScale = modDepthScale_.load(std::memory_order_relaxed);

        const float preDelaySamples = (preDelayMs / 1000.0f) *
                                       static_cast<float>(sampleRate_);
        const float sizeScale = 0.5f + size;
        const float combinedScale = rateScale_ * sizeScale;

        const int tankADelayLen = clampTap(
            static_cast<int>(kTankADelay * combinedScale), tankADelaySize_);
        const int tankADelay2Len = clampTap(
            static_cast<int>(kTankADelay2 * combinedScale), tankADelay2Size_);
        const int tankBDelayLen = clampTap(
            static_cast<int>(kTankBDelay * combinedScale), tankBDelaySize_);
        const int tankBDelay2Len = clampTap(
            static_cast<int>(kTankBDelay2 * combinedScale), tankBDelay2Size_);

        const int tapA1 = clampTap(static_cast<int>(266.0f * combinedScale), tankADelaySize_);
        const int tapA2 = clampTap(static_cast<int>(2974.0f * combinedScale), tankADelaySize_);
        const int tapA3 = clampTap(static_cast<int>(1913.0f * combinedScale), tankADelay2Size_);
        const int tapB1 = clampTap(static_cast<int>(1996.0f * combinedScale), tankBDelaySize_);
        const int tapB2 = clampTap(static_cast<int>(1990.0f * combinedScale), tankBDelaySize_);
        const int tapB3 = clampTap(static_cast<int>(187.0f * combinedScale), tankBDelay2Size_);

        for (int i = 0; i < numFrames; ++i) {
            const float inputSample = buffer[i];

            const float prevTankAFeedback = tankAFeedback_;
            const float prevTankBFeedback = tankBFeedback_;

            // Pre-delay
            preDelayBuffer_[preDelayWritePos_] = inputSample;
            float preDelayed = readDelay(preDelayBuffer_.data(), preDelayMask_,
                                          preDelayWritePos_, preDelaySamples);
            preDelayWritePos_ = (preDelayWritePos_ + 1) & preDelayMask_;

            // Input diffusion (4 cascaded allpass)
            // Use type-specific diffusion coefficients for dramatically different character
            float diffused = preDelayed;
            diffused = processAllpass(inputDiffusion1_, inputDiff1Mask_,
                                       inputDiff1WritePos_, inputDiff1Delay_,
                                       diffused, inputDiffCoeff1_);
            diffused = processAllpass(inputDiffusion2_, inputDiff2Mask_,
                                       inputDiff2WritePos_, inputDiff2Delay_,
                                       diffused, inputDiffCoeff1_);
            diffused = processAllpass(inputDiffusion3_, inputDiff3Mask_,
                                       inputDiff3WritePos_, inputDiff3Delay_,
                                       diffused, inputDiffCoeff2_);
            diffused = processAllpass(inputDiffusion4_, inputDiff4Mask_,
                                       inputDiff4WritePos_, inputDiff4Delay_,
                                       diffused, inputDiffCoeff2_);

            // Lo-Fi mode: bandwidth-limit the input (LP at ~3kHz)
            if (isLoFi) {
                loFiLPState_ = loFiLPState_ * loFiLPCoeff_
                             + static_cast<double>(diffused) * (1.0 - loFiLPCoeff_);
                loFiLPState_ = fast_math::denormal_guard(loFiLPState_);
                diffused = static_cast<float>(loFiLPState_);
                // Bit-crush: reduce to ~10-bit equivalent
                diffused = std::round(diffused * 1024.0f) / 1024.0f;
            }

            // Tank A
            float tankAInput = diffused + prevTankBFeedback * decayCoeff;
            float tankA = processAllpass(tankAAllpass_, tankAAP_Mask_,
                                          tankAAP_WritePos_, tankAAP_Delay_,
                                          tankAInput, decayDiffCoeff1_);

            // LFO modulation: Mod mode uses slow (0.5Hz) deep (±20 samples) chorus
            float effectiveLfoRateA = isMod ? 0.5f : kLfoRateA;
            float effectiveModDepth = kModDepth * modDepthScale;
            float modA = fast_math::sin2pi(lfoPhaseA_);
            lfoPhaseA_ += effectiveLfoRateA / static_cast<float>(sampleRate_);
            if (lfoPhaseA_ >= 1.0f) lfoPhaseA_ -= 1.0f;

            float tankAModDelay = static_cast<float>(tankADelayLen) + modA * effectiveModDepth * rateScale_;
            tankADelay_[tankADelayWritePos_] = tankA;
            tankA = readDelayInterp(tankADelay_.data(), tankADelayMask_,
                                     tankADelayWritePos_, tankAModDelay);
            tankADelayWritePos_ = (tankADelayWritePos_ + 1) & tankADelayMask_;

            // Bandwidth filter: pre-filter input into damping (type-configurable)
            float bwFilteredA = static_cast<float>(
                static_cast<double>(tankA) * (1.0 - bandwidth_) + tankABWState_ * bandwidth_);
            tankABWState_ = fast_math::denormal_guard(static_cast<double>(bwFilteredA));

            tankADampState_ = static_cast<double>(bwFilteredA) * (1.0 - damping)
                            + tankADampState_ * damping;
            tankADampState_ = fast_math::denormal_guard(tankADampState_);
            tankA = static_cast<float>(tankADampState_) * decayCoeff;

            // Lo-Fi: additional bandwidth limiting in the feedback path
            if (isLoFi) {
                loFiFeedbackLPStateA_ = loFiFeedbackLPStateA_ * loFiFeedbackLPCoeff_
                    + static_cast<double>(tankA) * (1.0 - loFiFeedbackLPCoeff_);
                loFiFeedbackLPStateA_ = fast_math::denormal_guard(loFiFeedbackLPStateA_);
                tankA = static_cast<float>(loFiFeedbackLPStateA_);
            }

            tankA = processAllpass(tankAAllpass2_, tankAAP2_Mask_,
                                    tankAAP2_WritePos_, tankAAP2_Delay_,
                                    tankA, decayCoeff * decayDiffCoeff2_);

            tankADelay2_[tankADelay2WritePos_] = tankA;
            tankA = readDelay(tankADelay2_.data(), tankADelay2Mask_,
                               tankADelay2WritePos_,
                               static_cast<float>(tankADelay2Len));
            tankADelay2WritePos_ = (tankADelay2WritePos_ + 1) & tankADelay2Mask_;
            tankAFeedback_ = fast_math::denormal_guard(tankA);

            // Tank B
            float tankBInput = diffused + prevTankAFeedback * decayCoeff;
            float tankB = processAllpass(tankBAllpass_, tankBAP_Mask_,
                                          tankBAP_WritePos_, tankBAP_Delay_,
                                          tankBInput, decayDiffCoeff1_);

            float effectiveLfoRateB = isMod ? 0.37f : kLfoRateB;
            float modB = fast_math::sin2pi(lfoPhaseB_);
            lfoPhaseB_ += effectiveLfoRateB / static_cast<float>(sampleRate_);
            if (lfoPhaseB_ >= 1.0f) lfoPhaseB_ -= 1.0f;

            float tankBModDelay = static_cast<float>(tankBDelayLen) + modB * effectiveModDepth * rateScale_;
            tankBDelay_[tankBDelayWritePos_] = tankB;
            tankB = readDelayInterp(tankBDelay_.data(), tankBDelayMask_,
                                     tankBDelayWritePos_, tankBModDelay);
            tankBDelayWritePos_ = (tankBDelayWritePos_ + 1) & tankBDelayMask_;

            float bwFilteredB = static_cast<float>(
                static_cast<double>(tankB) * (1.0 - bandwidth_) + tankBBWState_ * bandwidth_);
            tankBBWState_ = fast_math::denormal_guard(static_cast<double>(bwFilteredB));

            tankBDampState_ = static_cast<double>(bwFilteredB) * (1.0 - damping)
                            + tankBDampState_ * damping;
            tankBDampState_ = fast_math::denormal_guard(tankBDampState_);
            tankB = static_cast<float>(tankBDampState_) * decayCoeff;

            if (isLoFi) {
                loFiFeedbackLPStateB_ = loFiFeedbackLPStateB_ * loFiFeedbackLPCoeff_
                    + static_cast<double>(tankB) * (1.0 - loFiFeedbackLPCoeff_);
                loFiFeedbackLPStateB_ = fast_math::denormal_guard(loFiFeedbackLPStateB_);
                tankB = static_cast<float>(loFiFeedbackLPStateB_);
            }

            tankB = processAllpass(tankBAllpass2_, tankBAP2_Mask_,
                                    tankBAP2_WritePos_, tankBAP2_Delay_,
                                    tankB, decayCoeff * decayDiffCoeff2_);

            tankBDelay2_[tankBDelay2WritePos_] = tankB;
            tankB = readDelay(tankBDelay2_.data(), tankBDelay2Mask_,
                               tankBDelay2WritePos_,
                               static_cast<float>(tankBDelay2Len));
            tankBDelay2WritePos_ = (tankBDelay2WritePos_ + 1) & tankBDelay2Mask_;
            tankBFeedback_ = fast_math::denormal_guard(tankB);

            // Output: multi-tap sum
            float output = 0.0f;
            output += readTap(tankADelay_.data(), tankADelayMask_,
                               tankADelayWritePos_, tapA1);
            output += readTap(tankADelay_.data(), tankADelayMask_,
                               tankADelayWritePos_, tapA2);
            output -= readTap(tankADelay2_.data(), tankADelay2Mask_,
                               tankADelay2WritePos_, tapA3);
            output += readTap(tankBDelay_.data(), tankBDelayMask_,
                               tankBDelayWritePos_, tapB1);
            output -= readTap(tankBDelay_.data(), tankBDelayMask_,
                               tankBDelayWritePos_, tapB2);
            output -= readTap(tankBDelay2_.data(), tankBDelay2Mask_,
                               tankBDelay2WritePos_, tapB3);

            output *= 0.2f;
            buffer[i] = clamp(output, -1.0f, 1.0f);
        }
    }

    // =========================================================================
    // Spring reverb processing
    // =========================================================================

    /**
     * Process audio through the spring reverb emulation.
     *
     * Signal flow:
     *   Input -> Dwell drive (tanh saturation)
     *         -> Input transducer bandpass (biquad LPF ~4.5kHz)
     *         -> Split to N parallel springs
     *         -> Per-spring: allpass dispersion chain -> delay -> damping -> feedback
     *         -> Sum springs / N
     *         -> Output pickup resonance biquad
     *         -> DC blocker
     *         -> Clamp output
     *
     * @param buffer    Mono audio buffer, modified in-place.
     * @param numFrames Number of frames to process.
     * @param mode      kModeSpring2 (2 springs) or kModeSpring3 (3 springs).
     */
    void processSpring(float* buffer, int numFrames, int mode) {
        // Guard against uninitialized spring buffers
        if (springDelay_[0].empty()) return;

        const int numSprings = (mode == kModeSpring3) ? 3 : 2;
        const float dwell = springDwell_.load(std::memory_order_relaxed);
        const float decayParam = springDecay_.load(std::memory_order_relaxed);
        const float invSprings = 1.0f / static_cast<float>(numSprings);

        // Dwell maps to input drive gain: 1.0 to 8.0
        const float dwellGain = 1.0f + dwell * 7.0f;

        // Spring decay maps to feedback coefficient: 0.70 to 0.95
        // Each spring gets its own coefficient based on its loop time
        float springFeedback[kMaxSprings];
        const float rt60 = 0.5f + decayParam * 4.0f; // 0.5 to 4.5 seconds
        for (int s = 0; s < numSprings; ++s) {
            float loopTimeSec = static_cast<float>(springDelayLen_[s]) /
                                static_cast<float>(sampleRate_);
            // Add average allpass group delay to loop time estimate
            loopTimeSec += 0.005f; // ~5ms allpass group delay estimate
            float fb = std::exp(-6.908f * loopTimeSec / std::max(0.1f, rt60));
            springFeedback[s] = clamp(fb, 0.0f, 0.97f);
        }

        for (int i = 0; i < numFrames; ++i) {
            float input = buffer[i];

            // ---- Dwell drive (tanh saturation, mimics tube driver) ----
            float driven = input * dwellGain;
            driven = fast_math::fast_tanh(driven * 0.7f) * 1.4f;

            // ---- Input transducer LPF (bandwidth-limited, ~4.5kHz) ----
            // Biquad direct form II transposed
            float transducerOut = txBiquadB0_ * driven
                                + txBiquadState1_;
            txBiquadState1_ = txBiquadB1_ * driven
                            - txBiquadA1_ * transducerOut
                            + txBiquadState2_;
            txBiquadState2_ = txBiquadB2_ * driven
                            - txBiquadA2_ * transducerOut;

            // ---- Process each spring in parallel ----
            float springSum = 0.0f;

            for (int s = 0; s < numSprings; ++s) {
                // Add feedback from previous iteration into the input
                float springInput = transducerOut + springFeedbackSample_[s] * springFeedback[s];

                // ---- Chirp allpass dispersion chain (12 cascaded 1st-order) ----
                // Each stage: y[n] = a * x[n] + x[n-1] - a * y[n-1]
                // The graduated coefficients create the frequency-dependent
                // group delay that produces the characteristic spring "chirp".
                float sig = springInput;
                for (int k = 0; k < kSpringAllpassCount; ++k) {
                    float a = kSpringAllpassCoeffs[s][k];
                    float prev = springAPState_[s][k];
                    float out = a * sig + prev - a * springAPPrev_[s][k];
                    // Store state for next sample:
                    // x[n-1] for next iteration = current input
                    springAPState_[s][k] = sig;
                    // y[n-1] for next iteration = current output
                    springAPPrev_[s][k] = fast_math::denormal_guard(out);
                    sig = out;
                }

                // ---- Delay line (bulk propagation time) ----
                springDelay_[s][springDelayWritePos_[s]] = sig;
                int readPos = springDelayWritePos_[s] - springDelayLen_[s];
                if (readPos < 0) readPos += (springDelayMask_[s] + 1);
                float delayed = springDelay_[s][readPos & springDelayMask_[s]];
                springDelayWritePos_[s] = (springDelayWritePos_[s] + 1) & springDelayMask_[s];

                // ---- Damping LPF (one-pole, 64-bit state for feedback stability) ----
                springDampLPState_[s] = static_cast<double>(delayed) * (1.0 - springDampLPCoeff_[s])
                                      + springDampLPState_[s] * springDampLPCoeff_[s];
                springDampLPState_[s] = fast_math::denormal_guard(springDampLPState_[s]);
                float dampedLP = static_cast<float>(springDampLPState_[s]);

                // ---- Damping HPF (one-pole, prevents DC buildup) ----
                float hpInput = dampedLP;
                springDampHPState_[s] = springDampHPCoeff_[s]
                    * (springDampHPState_[s] + hpInput - springDampHPPrev_[s]);
                springDampHPPrev_[s] = hpInput;
                springDampHPState_[s] = fast_math::denormal_guard(springDampHPState_[s]);
                float dampedOut = static_cast<float>(springDampHPState_[s]);

                // Store feedback sample for next iteration
                springFeedbackSample_[s] = fast_math::denormal_guard(dampedOut);

                springSum += dampedOut;
            }

            // Average the springs
            float output = springSum * invSprings;

            // ---- Output pickup resonance biquad (peak at ~2.5kHz) ----
            float pickupOut = pkBiquadB0_ * output
                            + pkBiquadState1_;
            pkBiquadState1_ = pkBiquadB1_ * output
                            - pkBiquadA1_ * pickupOut
                            + pkBiquadState2_;
            pkBiquadState2_ = pkBiquadB2_ * output
                            - pkBiquadA2_ * pickupOut;

            // ---- DC blocker (HP at 10Hz, adaptive to sample rate) ----
            double dcIn = static_cast<double>(pickupOut);
            double dcOut = dcIn - dcPrevIn_ + dcCoeff_ * dcPrevOut_;
            dcPrevIn_ = dcIn;
            dcPrevOut_ = fast_math::denormal_guard(dcOut);

            buffer[i] = clamp(static_cast<float>(dcOut), -1.0f, 1.0f);
        }
    }

    // =========================================================================
    // Digital type preset configurations
    // =========================================================================

    /**
     * Apply a digital type preset by configuring ALL Dattorro plate parameters.
     *
     * Hall of Fame 2 mode mapping: each type configures decay, damping, preDelay,
     * size, input diffusion, decay diffusion, bandwidth, and special mode flags
     * (Lo-Fi bandwidth limiting, Mod chorus modulation depth).
     *
     * This is why the presets sound dramatically different from each other --
     * they configure every internal parameter, not just decay and damping.
     *
     * @param type  0=Room, 1=Hall, 2=Church, 3=Plate, 4=Lo-Fi, 5=Mod
     */
    void applyDigitalTypePreset(int type) {
        float decay, damping, preDelay, size;
        float inDiff1, inDiff2, decDiff1, decDiff2, bw;
        bool lofi = false, mod = false;
        float modScale = 1.0f;  // Mod depth multiplier (1.0 = default kModDepth)

        switch (type) {
            case kDigitalRoom:
                // Very short decay, high damping (dark), small size,
                // prominent early reflections. Intimate practice room.
                decay    = 0.8f;    // 0.8s (very short tail)
                damping  = 0.7f;    // dark -- absorptive walls
                preDelay = 3.0f;    // 3ms (close walls)
                size     = 0.15f;   // very small
                inDiff1  = 0.80f;   // high input diffusion for early reflections
                inDiff2  = 0.70f;
                decDiff1 = 0.50f;   // low decay diffusion (more discrete echoes)
                decDiff2 = 0.30f;
                bw       = 0.85f;   // moderate bandwidth (slightly filtered input)
                break;

            case kDigitalHall:
                // Long decay, moderate damping, large size, diffuse.
                // Grand concert hall with warm, enveloping tail.
                decay    = 3.5f;    // 3.5s (long, luxurious tail)
                damping  = 0.35f;   // moderately bright
                preDelay = 18.0f;   // 18ms (spacious separation)
                size     = 0.80f;   // large space
                inDiff1  = 0.75f;   // standard Dattorro diffusion
                inDiff2  = 0.625f;
                decDiff1 = 0.70f;   // standard decay diffusion
                decDiff2 = 0.50f;
                bw       = 0.95f;   // wide bandwidth (full spectrum)
                break;

            case kDigitalChurch:
                // Very long decay, low damping (bright), huge size,
                // sparse early reflections. Cathedral with stone walls.
                decay    = 6.0f;    // 6.0s (very long, cathedral echo)
                damping  = 0.15f;   // very bright -- stone reflects everything
                preDelay = 35.0f;   // 35ms (distant walls)
                size     = 0.95f;   // massive space
                inDiff1  = 0.60f;   // lower input diffusion (sparser reflections)
                inDiff2  = 0.45f;
                decDiff1 = 0.80f;   // high decay diffusion (smooth sustain)
                decDiff2 = 0.65f;
                bw       = 0.99f;   // maximum bandwidth (bright stone)
                break;

            case kDigitalPlate:
                // Medium decay, very low damping (bright/sparkly), no early
                // reflections, dense. Classic studio plate reverb.
                decay    = 2.0f;    // 2.0s (medium)
                damping  = 0.10f;   // very bright, sparkly
                preDelay = 0.0f;    // zero predelay (immediate density, no room)
                size     = 0.50f;   // medium (plate dimensions)
                inDiff1  = 0.85f;   // very high diffusion (dense from the start)
                inDiff2  = 0.80f;
                decDiff1 = 0.75f;   // high decay diffusion (smooth, continuous)
                decDiff2 = 0.60f;
                bw       = 0.99f;   // full bandwidth (metallic plate resonance)
                break;

            case kDigitalLoFi:
                // Medium decay with heavy bandwidth limitation and bitcrushing.
                // Degraded vintage character -- old tape, lo-fi ambient.
                decay    = 2.5f;    // 2.5s
                damping  = 0.65f;   // fairly dark
                preDelay = 10.0f;   // 10ms
                size     = 0.45f;   // medium-small
                inDiff1  = 0.70f;
                inDiff2  = 0.55f;
                decDiff1 = 0.60f;
                decDiff2 = 0.40f;
                bw       = 0.60f;   // narrow bandwidth (muffled)
                lofi     = true;    // enables input LP + bitcrush + feedback LP
                break;

            case kDigitalMod:
            default:
                // Medium-long decay with deep chorus modulation in the tail.
                // Shimmer/movement effect -- delay lines wobble with slow LFO.
                decay    = 3.0f;    // 3.0s (medium-long)
                damping  = 0.25f;   // bright
                preDelay = 12.0f;   // 12ms
                size     = 0.65f;   // medium-large
                inDiff1  = 0.75f;
                inDiff2  = 0.625f;
                decDiff1 = 0.70f;
                decDiff2 = 0.50f;
                bw       = 0.97f;   // wide bandwidth
                mod      = true;    // enables deep LFO modulation
                modScale = 2.5f;    // 2.5x deeper modulation (~40 samples)
                break;
        }

        // Store type-specific diffusion coefficients (read by audio thread)
        inputDiffCoeff1_ = inDiff1;
        inputDiffCoeff2_ = inDiff2;
        decayDiffCoeff1_ = decDiff1;
        decayDiffCoeff2_ = decDiff2;
        // Bandwidth: higher bw value -> less one-pole filtering (0.99 = almost no filter)
        bandwidth_ = 1.0 - static_cast<double>(bw);
        loFiEnabled_.store(lofi, std::memory_order_relaxed);
        modEnabled_.store(mod, std::memory_order_relaxed);
        modDepthScale_.store(modScale, std::memory_order_relaxed);

        // Compute Lo-Fi LP filter coefficients (fc ~3kHz input, ~2.5kHz feedback)
        if (sampleRate_ > 0) {
            const double pi2 = 2.0 * 3.14159265358979;
            const double fs = static_cast<double>(sampleRate_);
            loFiLPCoeff_ = std::exp(-pi2 * 3000.0 / fs);
            loFiFeedbackLPCoeff_ = std::exp(-pi2 * 2500.0 / fs);
        }

        // Apply base decay/damping/preDelay/size via setParameter
        // for proper clamping and coefficient updates.
        setParameter(1, damping);
        setParameter(2, preDelay);
        setParameter(3, size);
        // Set decay last (depends on size). Convert decay time to normalized 0-1:
        // Inverse of mapping: decayTimeSec = 0.1 * 100^norm
        // => norm = log10(decayTimeSec / 0.1) / 2.0
        float decayNorm = std::log10(std::max(0.1f, decay) / 0.1f) / 2.0f;
        decayNorm = clamp(decayNorm, 0.0f, 1.0f);
        setParameter(0, decayNorm);
    }

    // =========================================================================
    // Allocation helpers
    // =========================================================================

    /** Allocate all Dattorro plate delay buffers (factored out of setSampleRate). */
    void allocateDattorroBuffers(int32_t sampleRate) {
        // Pre-delay: up to 100ms
        allocateDelay(preDelayBuffer_, preDelaySize_, preDelayMask_,
                      static_cast<int>(0.1f * sampleRate) + 1);
        preDelayWritePos_ = 0;

        // Input diffusion allpass buffers
        inputDiff1Delay_ = std::max(1, static_cast<int>(142.0f * rateScale_));
        allocateDelay(inputDiffusion1_, inputDiff1Size_, inputDiff1Mask_,
                      inputDiff1Delay_ + 1);
        inputDiff1WritePos_ = 0;

        inputDiff2Delay_ = std::max(1, static_cast<int>(107.0f * rateScale_));
        allocateDelay(inputDiffusion2_, inputDiff2Size_, inputDiff2Mask_,
                      inputDiff2Delay_ + 1);
        inputDiff2WritePos_ = 0;

        inputDiff3Delay_ = std::max(1, static_cast<int>(379.0f * rateScale_));
        allocateDelay(inputDiffusion3_, inputDiff3Size_, inputDiff3Mask_,
                      inputDiff3Delay_ + 1);
        inputDiff3WritePos_ = 0;

        inputDiff4Delay_ = std::max(1, static_cast<int>(277.0f * rateScale_));
        allocateDelay(inputDiffusion4_, inputDiff4Size_, inputDiff4Mask_,
                      inputDiff4Delay_ + 1);
        inputDiff4WritePos_ = 0;

        // Tank A
        tankAAP_Delay_ = std::max(1, static_cast<int>(672.0f * rateScale_));
        allocateDelay(tankAAllpass_, tankAAP_Size_, tankAAP_Mask_,
                      tankAAP_Delay_ + 1);
        tankAAP_WritePos_ = 0;

        allocateDelay(tankADelay_, tankADelaySize_, tankADelayMask_,
                      static_cast<int>(kTankADelay * 2.0f * rateScale_) + 64);
        tankADelayWritePos_ = 0;

        tankAAP2_Delay_ = std::max(1, static_cast<int>(1800.0f * rateScale_));
        allocateDelay(tankAAllpass2_, tankAAP2_Size_, tankAAP2_Mask_,
                      tankAAP2_Delay_ + 1);
        tankAAP2_WritePos_ = 0;

        allocateDelay(tankADelay2_, tankADelay2Size_, tankADelay2Mask_,
                      static_cast<int>(kTankADelay2 * 2.0f * rateScale_) + 64);
        tankADelay2WritePos_ = 0;

        // Tank B
        tankBAP_Delay_ = std::max(1, static_cast<int>(908.0f * rateScale_));
        allocateDelay(tankBAllpass_, tankBAP_Size_, tankBAP_Mask_,
                      tankBAP_Delay_ + 1);
        tankBAP_WritePos_ = 0;

        allocateDelay(tankBDelay_, tankBDelaySize_, tankBDelayMask_,
                      static_cast<int>(kTankBDelay * 2.0f * rateScale_) + 64);
        tankBDelayWritePos_ = 0;

        tankBAP2_Delay_ = std::max(1, static_cast<int>(2656.0f * rateScale_));
        allocateDelay(tankBAllpass2_, tankBAP2_Size_, tankBAP2_Mask_,
                      tankBAP2_Delay_ + 1);
        tankBAP2_WritePos_ = 0;

        allocateDelay(tankBDelay2_, tankBDelay2Size_, tankBDelay2Mask_,
                      static_cast<int>(kTankBDelay2 * 2.0f * rateScale_) + 64);
        tankBDelay2WritePos_ = 0;
    }

    /** Allocate all spring reverb delay buffers. Pre-allocates for all 3 springs. */
    void allocateSpringBuffers(int32_t sampleRate) {
        float springRateScale = static_cast<float>(sampleRate) / 48000.0f;

        for (int s = 0; s < kMaxSprings; ++s) {
            // Scale delay line length for sample rate
            springDelayLen_[s] = std::max(1, static_cast<int>(
                static_cast<float>(kSpringBaseDelay[s]) * springRateScale));

            // Allocate power-of-two buffer with headroom
            int minSize = springDelayLen_[s] + 1;
            allocateDelay(springDelay_[s], springDelaySize_[s],
                          springDelayMask_[s], minSize);
            springDelayWritePos_[s] = 0;
        }

        // DC blocker coefficient: R = exp(-2*pi*10/fs) for 10Hz cutoff
        dcCoeff_ = std::exp(-2.0 * 3.14159265358979 * 10.0 / static_cast<double>(sampleRate));
    }

    /**
     * Compute input transducer LPF and output pickup resonance biquad coefficients.
     *
     * Input transducer: 2nd-order LPF at ~4500Hz, Q=0.707 (Butterworth).
     * Simulates the mechanical bandwidth limitation of the electromagnetic driver.
     *
     * Output pickup: peaking EQ at ~2500Hz, Q=2.0, +6dB.
     * Simulates the pickup armature mechanical resonance that gives spring reverb
     * its characteristic "ting" on transients.
     */
    void computeTransducerEQ(int32_t sampleRate) {
        const double pi = 3.14159265358979;
        const double fs = static_cast<double>(sampleRate);

        // ---- Input transducer LPF: fc=4500Hz, Q=0.707 ----
        {
            double omega = 2.0 * pi * 4500.0 / fs;
            double sinW = std::sin(omega);
            double cosW = std::cos(omega);
            double alpha = sinW / (2.0 * 0.707);

            double b0 = (1.0 - cosW) / 2.0;
            double b1 = 1.0 - cosW;
            double b2 = (1.0 - cosW) / 2.0;
            double a0 = 1.0 + alpha;
            double a1 = -2.0 * cosW;
            double a2 = 1.0 - alpha;

            // Normalize by a0
            txBiquadB0_ = static_cast<float>(b0 / a0);
            txBiquadB1_ = static_cast<float>(b1 / a0);
            txBiquadB2_ = static_cast<float>(b2 / a0);
            txBiquadA1_ = static_cast<float>(a1 / a0);
            txBiquadA2_ = static_cast<float>(a2 / a0);
        }

        // ---- Output pickup peaking EQ: fc=2500Hz, Q=2.0, gain=+6dB ----
        {
            double omega = 2.0 * pi * 2500.0 / fs;
            double sinW = std::sin(omega);
            double cosW = std::cos(omega);
            // +6dB = 10^(6/40) linear amplitude = ~1.995
            double A = std::pow(10.0, 6.0 / 40.0);
            double alpha = sinW / (2.0 * 2.0); // Q = 2.0

            double b0 = 1.0 + alpha * A;
            double b1 = -2.0 * cosW;
            double b2 = 1.0 - alpha * A;
            double a0 = 1.0 + alpha / A;
            double a1 = -2.0 * cosW;
            double a2 = 1.0 - alpha / A;

            pkBiquadB0_ = static_cast<float>(b0 / a0);
            pkBiquadB1_ = static_cast<float>(b1 / a0);
            pkBiquadB2_ = static_cast<float>(b2 / a0);
            pkBiquadA1_ = static_cast<float>(a1 / a0);
            pkBiquadA2_ = static_cast<float>(a2 / a0);
        }
    }

    /**
     * Recompute spring damping filter coefficients from the Tone parameter.
     *
     * Tone (0-1) scales the LPF cutoff: fc = 2000 + tone * 6000 Hz.
     * At tone=0: dark (2kHz LPF). At tone=1: bright (8kHz LPF).
     * The base cutoffs per spring provide slight differentiation.
     *
     * HPF coefficients are fixed per spring (prevent DC, not user-adjustable).
     */
    void updateSpringDamping() {
        if (sampleRate_ <= 0) return;

        const float tone = springTone_.load(std::memory_order_relaxed);
        const double pi2 = 2.0 * 3.14159265358979;
        const double fs = static_cast<double>(sampleRate_);

        for (int s = 0; s < kMaxSprings; ++s) {
            // Tone scales the per-spring LPF cutoff
            // Base fc from research report, then blend with tone parameter
            float baseFc = kSpringDampLPF_fc[s];
            float fc = 2000.0f + tone * (baseFc + 1500.0f);
            // One-pole LPF: coeff = exp(-2*pi*fc/fs)
            springDampLPCoeff_[s] = std::exp(-pi2 * static_cast<double>(fc) / fs);

            // HPF: fixed per-spring cutoff, R = exp(-2*pi*fc/fs)
            springDampHPCoeff_[s] = std::exp(-pi2 * static_cast<double>(kSpringDampHPF_fc[s]) / fs);
        }
    }

    // =========================================================================
    // Shared internal helpers (all real-time safe)
    // =========================================================================

    static void allocateDelay(std::vector<float>& buffer, int& size,
                               int& mask, int minSize) {
        size = nextPowerOfTwo(minSize);
        mask = size - 1;
        buffer.assign(size, 0.0f);
    }

    static float readDelay(const float* buffer, int mask,
                            int writePos, float delaySamples) {
        int readPos = writePos - static_cast<int>(delaySamples);
        if (readPos < 0) readPos += (mask + 1);
        return buffer[readPos & mask];
    }

    static float readDelayInterp(const float* buffer, int mask,
                                   int writePos, float delaySamples) {
        float readPosF = static_cast<float>(writePos) - delaySamples;
        if (readPosF < 0.0f) readPosF += static_cast<float>(mask + 1);
        int readPosInt = static_cast<int>(readPosF);
        float frac = readPosF - static_cast<float>(readPosInt);
        int idx0 = readPosInt & mask;
        int idx1 = (readPosInt + 1) & mask;
        return buffer[idx0] * (1.0f - frac) + buffer[idx1] * frac;
    }

    static float readTap(const float* buffer, int mask,
                          int writePos, int delaySamples) {
        int readPos = writePos - delaySamples;
        if (readPos < 0) readPos += (mask + 1);
        return buffer[readPos & mask];
    }

    static int clampTap(int tap, int bufferSize) {
        return std::max(1, std::min(tap, bufferSize - 1));
    }

    float processAllpass(std::vector<float>& buffer, int mask,
                          int& writePos, int delayLength,
                          float input, float coeff) {
        int readPos = writePos - delayLength;
        if (readPos < 0) readPos += (mask + 1);
        float delayed = buffer[readPos & mask];
        float temp = input + coeff * delayed;
        buffer[writePos] = temp;
        writePos = (writePos + 1) & mask;
        return delayed - coeff * temp;
    }

    void updateDecayCoeff(float decayTimeSec, float sizeScale = 1.0f) {
        float effectiveLoopTime = kAvgLoopTime * sizeScale;
        float coeff = std::exp(-6.908f * effectiveLoopTime /
                               std::max(0.1f, decayTimeSec));
        coeff = std::min(0.99f, std::max(0.0f, coeff));
        decayCoeff_.store(coeff, std::memory_order_relaxed);
    }

    void resetState() {
        // ---- Dattorro state ----
        std::fill(preDelayBuffer_.begin(), preDelayBuffer_.end(), 0.0f);
        std::fill(inputDiffusion1_.begin(), inputDiffusion1_.end(), 0.0f);
        std::fill(inputDiffusion2_.begin(), inputDiffusion2_.end(), 0.0f);
        std::fill(inputDiffusion3_.begin(), inputDiffusion3_.end(), 0.0f);
        std::fill(inputDiffusion4_.begin(), inputDiffusion4_.end(), 0.0f);
        std::fill(tankAAllpass_.begin(), tankAAllpass_.end(), 0.0f);
        std::fill(tankADelay_.begin(), tankADelay_.end(), 0.0f);
        std::fill(tankAAllpass2_.begin(), tankAAllpass2_.end(), 0.0f);
        std::fill(tankADelay2_.begin(), tankADelay2_.end(), 0.0f);
        std::fill(tankBAllpass_.begin(), tankBAllpass_.end(), 0.0f);
        std::fill(tankBDelay_.begin(), tankBDelay_.end(), 0.0f);
        std::fill(tankBAllpass2_.begin(), tankBAllpass2_.end(), 0.0f);
        std::fill(tankBDelay2_.begin(), tankBDelay2_.end(), 0.0f);

        tankADampState_ = 0.0;
        tankBDampState_ = 0.0;
        tankABWState_ = 0.0;
        tankBBWState_ = 0.0;
        tankAFeedback_ = 0.0f;
        tankBFeedback_ = 0.0f;
        lfoPhaseA_ = 0.0f;
        lfoPhaseB_ = 0.0f;

        // Lo-Fi / Mod state
        loFiLPState_ = 0.0;
        loFiFeedbackLPStateA_ = 0.0;
        loFiFeedbackLPStateB_ = 0.0;

        // ---- Spring state ----
        for (int s = 0; s < kMaxSprings; ++s) {
            std::fill(springDelay_[s].begin(), springDelay_[s].end(), 0.0f);
            springDelayWritePos_[s] = 0;
            springFeedbackSample_[s] = 0.0f;
            springDampLPState_[s] = 0.0;
            springDampHPState_[s] = 0.0;
            springDampHPPrev_[s] = 0.0;
            for (int k = 0; k < kSpringAllpassCount; ++k) {
                springAPState_[s][k] = 0.0f;
                springAPPrev_[s][k] = 0.0f;
            }
        }

        // ---- Transducer EQ biquad state ----
        txBiquadState1_ = 0.0f;
        txBiquadState2_ = 0.0f;
        pkBiquadState1_ = 0.0f;
        pkBiquadState2_ = 0.0f;

        // ---- DC blocker state ----
        dcPrevIn_ = 0.0;
        dcPrevOut_ = 0.0;
    }

    static int nextPowerOfTwo(int n) {
        if (n <= 0) return 1;
        if (n > (1 << 24)) return (1 << 24);
        int power = 1;
        while (power < n) power <<= 1;
        return power;
    }

    static float clamp(float value, float minVal, float maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    // =========================================================================
    // Parameters (written from UI thread, read from audio thread)
    // =========================================================================

    // Plate parameters (params 0-3)
    std::atomic<float> decayTime_{2.0f};
    std::atomic<float> decayNorm_{0.5f};      // Normalized decay knob position [0, 1]
    std::atomic<float> decayCoeff_{0.5f};
    std::atomic<float> damping_{0.5f};
    std::atomic<float> preDelay_{10.0f};
    std::atomic<float> size_{0.5f};

    // Mode parameter (param 4): 0=Digital, 1=Spring2, 2=Spring3
    std::atomic<float> mode_{0.0f};

    // Digital type parameter (param 8): 0=Room, 1=Hall, 2=Church, 3=Plate, 4=Lo-Fi, 5=Mod
    std::atomic<float> digitalType_{1.0f};  // Default to Hall

    // Digital sub-mode flags (Lo-Fi, Mod, etc.)
    std::atomic<bool>  loFiEnabled_{false};   // Lo-Fi mode: bit-crush + jitter
    std::atomic<bool>  modEnabled_{false};    // Mod mode: chorus-modulated tails
    std::atomic<float> modDepthScale_{1.0f};  // Modulation depth multiplier

    // Spring parameters (params 5-7)
    std::atomic<float> springDwell_{0.5f};    // [0, 1] input drive
    std::atomic<float> springTone_{0.5f};     // [0, 1] damping brightness
    std::atomic<float> springDecay_{0.5f};    // [0, 1] spring reverb time

    // =========================================================================
    // Dattorro plate audio state
    // =========================================================================

    int32_t sampleRate_ = 44100;
    float rateScale_ = 1.483f;

    std::vector<float> preDelayBuffer_;
    int preDelaySize_ = 0, preDelayMask_ = 0, preDelayWritePos_ = 0;

    std::vector<float> inputDiffusion1_;
    int inputDiff1Size_ = 0, inputDiff1Mask_ = 0, inputDiff1WritePos_ = 0;
    int inputDiff1Delay_ = 0;

    std::vector<float> inputDiffusion2_;
    int inputDiff2Size_ = 0, inputDiff2Mask_ = 0, inputDiff2WritePos_ = 0;
    int inputDiff2Delay_ = 0;

    std::vector<float> inputDiffusion3_;
    int inputDiff3Size_ = 0, inputDiff3Mask_ = 0, inputDiff3WritePos_ = 0;
    int inputDiff3Delay_ = 0;

    std::vector<float> inputDiffusion4_;
    int inputDiff4Size_ = 0, inputDiff4Mask_ = 0, inputDiff4WritePos_ = 0;
    int inputDiff4Delay_ = 0;

    std::vector<float> tankAAllpass_;
    int tankAAP_Size_ = 0, tankAAP_Mask_ = 0, tankAAP_WritePos_ = 0;
    int tankAAP_Delay_ = 0;

    std::vector<float> tankADelay_;
    int tankADelaySize_ = 0, tankADelayMask_ = 0, tankADelayWritePos_ = 0;

    std::vector<float> tankAAllpass2_;
    int tankAAP2_Size_ = 0, tankAAP2_Mask_ = 0, tankAAP2_WritePos_ = 0;
    int tankAAP2_Delay_ = 0;

    std::vector<float> tankADelay2_;
    int tankADelay2Size_ = 0, tankADelay2Mask_ = 0, tankADelay2WritePos_ = 0;

    double tankADampState_ = 0.0;
    float tankAFeedback_ = 0.0f;

    std::vector<float> tankBAllpass_;
    int tankBAP_Size_ = 0, tankBAP_Mask_ = 0, tankBAP_WritePos_ = 0;
    int tankBAP_Delay_ = 0;

    std::vector<float> tankBDelay_;
    int tankBDelaySize_ = 0, tankBDelayMask_ = 0, tankBDelayWritePos_ = 0;

    std::vector<float> tankBAllpass2_;
    int tankBAP2_Size_ = 0, tankBAP2_Mask_ = 0, tankBAP2_WritePos_ = 0;
    int tankBAP2_Delay_ = 0;

    std::vector<float> tankBDelay2_;
    int tankBDelay2Size_ = 0, tankBDelay2Mask_ = 0, tankBDelay2WritePos_ = 0;

    double tankBDampState_ = 0.0;
    float tankBFeedback_ = 0.0f;

    float lfoPhaseA_ = 0.0f;
    float lfoPhaseB_ = 0.0f;

    // Type-specific diffusion and bandwidth coefficients (set by applyDigitalTypePreset)
    float inputDiffCoeff1_ = kInputDiffCoeff1;   // Input diffusion stage 1-2
    float inputDiffCoeff2_ = kInputDiffCoeff2;   // Input diffusion stage 3-4
    float decayDiffCoeff1_ = kDecayDiffCoeff1;   // Tank decay diffusion 1
    float decayDiffCoeff2_ = kDecayDiffCoeff2;   // Tank decay diffusion 2
    double bandwidth_ = 0.05;   // Input bandwidth filter (0 = full BW, 1 = heavy LP)
    double tankABWState_ = 0.0; // Bandwidth filter state, tank A
    double tankBBWState_ = 0.0; // Bandwidth filter state, tank B

    // Lo-Fi mode audio state
    double loFiLPCoeff_ = 0.0;            // One-pole LP coeff for ~3kHz input filter
    double loFiLPState_ = 0.0;            // Input LP filter state
    double loFiFeedbackLPCoeff_ = 0.0;    // One-pole LP coeff for ~2.5kHz feedback filter
    double loFiFeedbackLPStateA_ = 0.0;   // Feedback LP filter state, tank A
    double loFiFeedbackLPStateB_ = 0.0;   // Feedback LP filter state, tank B

    // =========================================================================
    // Spring reverb audio state
    // =========================================================================

    // Per-spring delay lines
    std::vector<float> springDelay_[kMaxSprings];
    int springDelaySize_[kMaxSprings] = {};
    int springDelayMask_[kMaxSprings] = {};
    int springDelayWritePos_[kMaxSprings] = {};
    int springDelayLen_[kMaxSprings] = {};

    // Per-spring allpass dispersion chain state
    // springAPState_[spring][stage] = x[n-1] (previous input)
    // springAPPrev_[spring][stage] = y[n-1] (previous output)
    float springAPState_[kMaxSprings][kSpringAllpassCount] = {};
    float springAPPrev_[kMaxSprings][kSpringAllpassCount] = {};

    // Per-spring feedback sample (output fed back to input)
    float springFeedbackSample_[kMaxSprings] = {};

    // Per-spring damping filter state (64-bit for feedback path stability)
    double springDampLPState_[kMaxSprings] = {};
    double springDampLPCoeff_[kMaxSprings] = {};  // Computed from tone param + sample rate
    double springDampHPState_[kMaxSprings] = {};
    double springDampHPCoeff_[kMaxSprings] = {};  // Computed from fixed HPF fc + sample rate
    double springDampHPPrev_[kMaxSprings] = {};   // Previous input for HPF

    // Input transducer biquad (LPF ~4.5kHz, Q=0.707)
    float txBiquadB0_ = 0.0f, txBiquadB1_ = 0.0f, txBiquadB2_ = 0.0f;
    float txBiquadA1_ = 0.0f, txBiquadA2_ = 0.0f;
    float txBiquadState1_ = 0.0f, txBiquadState2_ = 0.0f;

    // Output pickup resonance biquad (peaking EQ ~2.5kHz, Q=2.0, +6dB)
    float pkBiquadB0_ = 0.0f, pkBiquadB1_ = 0.0f, pkBiquadB2_ = 0.0f;
    float pkBiquadA1_ = 0.0f, pkBiquadA2_ = 0.0f;
    float pkBiquadState1_ = 0.0f, pkBiquadState2_ = 0.0f;

    // DC blocker (HP at 10Hz, sample-rate adaptive)
    double dcCoeff_ = 0.9987;  // exp(-2*pi*10/48000)
    double dcPrevIn_ = 0.0;
    double dcPrevOut_ = 0.0;
};

#endif // GUITAR_ENGINE_REVERB_H
