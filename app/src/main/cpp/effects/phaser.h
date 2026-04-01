#ifndef GUITAR_ENGINE_PHASER_H
#define GUITAR_ENGINE_PHASER_H

#include "effect.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>

/**
 * Phaser effect using cascaded first-order allpass filters swept by an LFO.
 *
 * The phaser creates its characteristic sweeping sound by passing the signal
 * through a series of allpass filters whose break frequencies are modulated
 * by an LFO. Each allpass stage creates a notch in the frequency response
 * when mixed with the dry signal; sweeping the break frequency moves the
 * notches up and down the spectrum.
 *
 * Allpass filter formula (first-order):
 *   a = (1 - tan(pi * f / sr)) / (1 + tan(pi * f / sr))
 *   y[n] = a * x[n] + x[n-1] - a * y[n-1]
 *
 * More stages = more notches = richer phaser effect:
 *   4 stages: 2 notches (subtle, classic)
 *   6 stages: 3 notches (moderate)
 *   8 stages: 4 notches (intense, jet-like)
 *
 * Modes:
 *   Standard (0): Sine LFO, user-controlled stages/feedback/rate/depth.
 *                  Sweep range 200-4000 Hz, 2-8 allpass stages.
 *
 *   Phase90 Script (1): Emulates MXR Phase 90 with Script logo (original).
 *                  Triangle LFO (Schmitt trigger-integrator circuit),
 *                  4 allpass stages (47nF matched caps + 2N5952 JFETs),
 *                  allpass break freq sweep 150-4500 Hz, NO feedback path.
 *                  Smooth, subtle phasing character.
 *
 *   Phase90 Block (2): Emulates MXR Phase 90 with Block logo (later revision).
 *                  Same as Script but adds a feedback resistor (0.4 gain)
 *                  for a more pronounced, dramatic sweep.
 *
 * Parameter IDs for JNI access:
 *   0 = rate (0.1 to 5.0 Hz) - LFO sweep speed
 *   1 = depth (0.0 to 1.0) - sweep range (Standard) or blend amount (Phase90)
 *   2 = feedback (0.0 to 0.9) - resonance at notch frequencies (Standard only)
 *   3 = stages (2 to 8) - number of allpass stages (Standard only)
 *   4 = mode (0=Standard, 1=Phase90 Script, 2=Phase90 Block)
 */
class Phaser : public Effect {
public:
    Phaser() {
        resetStages();
    }

    void process(float* buffer, int numFrames) override {
        // Load all parameters atomically at block start
        const float rate = rate_.load(std::memory_order_relaxed);
        const float depth = depth_.load(std::memory_order_relaxed);
        const float mode = mode_.load(std::memory_order_relaxed);
        const bool isPhase90 = (mode >= 0.5f);

        // Detect mode change and reset stale allpass state to prevent clicks.
        // When switching from 8-stage Standard to 4-stage Phase90, stages 4-7
        // retain stale data that would cause a transient on switching back.
        const int modeInt = static_cast<int>(mode + 0.5f);
        if (modeInt != prevMode_) {
            resetStages();
            prevMode_ = modeInt;
        }

        // Determine effective parameters based on mode
        int numStages;
        float effectiveFeedback;
        float minFreq;
        float maxFreq;

        if (isPhase90) {
            // Phase 90 modes: fixed 4 stages, specific frequency range
            numStages = kPhase90Stages;

            // Script (mode ~1) = no feedback, Block (mode ~2) = 0.4 feedback
            const bool isBlock = (mode >= 1.5f);
            effectiveFeedback = isBlock ? kPhase90BlockFeedback : 0.0f;

            // Phase 90 allpass break freq sweep: 150 Hz to 4500 Hz
            // fc = 1/(2*pi*(22K||rDS)*47nF), JFET rDS varies ~100 ohm to >100K ohm.
            // Depth controls how much of this range is actually swept.
            minFreq = kPhase90MinFreq;
            maxFreq = kPhase90MinFreq +
                      (kPhase90MaxFreq - kPhase90MinFreq) * depth;
        } else {
            // Standard mode: user-controlled stages and feedback
            const int stages = static_cast<int>(
                stages_.load(std::memory_order_relaxed));
            numStages = clampInt(stages, 2, kMaxStages);
            effectiveFeedback = feedback_.load(std::memory_order_relaxed);

            // Standard sweep range: 200 Hz to 4000 Hz, modulated by depth
            minFreq = 200.0f;
            maxFreq = 200.0f + 3800.0f * depth;
        }

        // Guard against degenerate frequency range (minFreq == maxFreq)
        if (maxFreq < minFreq + 1.0f) maxFreq = minFreq + 1.0f;

        const float phaseInc = rate / static_cast<float>(sampleRate_);

        // Pre-compute log2 ratio for fast exponential frequency mapping.
        // Both modes use exponential mapping for perceptually linear sweep.
        const float log2Ratio = std::log2(maxFreq / minFreq);
        const double piOverSr = static_cast<double>(kPi) /
                                 static_cast<double>(sampleRate_);

        for (int i = 0; i < numFrames; ++i) {
            // Generate LFO sweep position [0, 1]
            float lfo;
            if (isPhase90) {
                // Triangle LFO: the real Phase 90 uses a Schmitt trigger-
                // integrator circuit that generates a triangle wave, NOT a
                // sine. This is the key tonal difference -- the triangle
                // spends equal time at all sweep positions (linear ramp)
                // whereas a sine lingers at the extremes.
                // Triangle: rises 0->1 in first half, falls 1->0 in second.
                lfo = (lfoPhase_ < 0.5f)
                    ? (lfoPhase_ * 2.0f)
                    : (2.0f - lfoPhase_ * 2.0f);
            } else {
                // Standard mode: sine LFO mapped to [0, 1]
                lfo = 0.5f + 0.5f * fast_math::sin2pi(lfoPhase_);
            }

            // Advance LFO phase and wrap
            lfoPhase_ += phaseInc;
            if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;

            // Map LFO to frequency (exponential for perceptually linear sweep)
            float freq = minFreq * fast_math::exp2_approx(lfo * log2Ratio);

            // Calculate allpass coefficient using fast tangent
            float t = static_cast<float>(fast_math::tan_approx(
                static_cast<double>(freq) * piOverSr));
            float a = (1.0f - t) / (1.0f + t);

            // Input with feedback
            float input = buffer[i] + feedbackSample_ * effectiveFeedback;

            // Process through cascaded allpass stages
            float sample = input;
            for (int s = 0; s < numStages; ++s) {
                float y = a * sample + apX1_[s] - a * apY1_[s];
                apX1_[s] = sample;
                apY1_[s] = fast_math::denormal_guard(y);
                sample = y;
            }

            // Store for feedback (guard against denormals in the feedback path)
            feedbackSample_ = fast_math::denormal_guard(sample);

            // Output: mix of dry and allpass-filtered signal creates notches
            buffer[i] = clamp(buffer[i] + sample, -1.0f, 1.0f) * 0.5f;
        }
    }

    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;
        resetStages();
    }

    void reset() override {
        resetStages();
    }

    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0: rate_.store(clamp(value, 0.1f, 5.0f), std::memory_order_relaxed); break;
            case 1: depth_.store(clamp(value, 0.0f, 1.0f), std::memory_order_relaxed); break;
            case 2: feedback_.store(clamp(value, 0.0f, 0.9f), std::memory_order_relaxed); break;
            case 3: stages_.store(clamp(value, 2.0f, 8.0f), std::memory_order_relaxed); break;
            case 4: mode_.store(clamp(value, 0.0f, 2.0f), std::memory_order_relaxed); break;
            default: break;
        }
    }

    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return rate_.load(std::memory_order_relaxed);
            case 1: return depth_.load(std::memory_order_relaxed);
            case 2: return feedback_.load(std::memory_order_relaxed);
            case 3: return stages_.load(std::memory_order_relaxed);
            case 4: return mode_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    static constexpr int kMaxStages = 8;
    static constexpr float kPi = 3.14159265358979323846f;

    // Phase 90 circuit constants
    static constexpr int kPhase90Stages = 4;         // 4 matched allpass stages
    static constexpr float kPhase90MinFreq = 150.0f;  // Allpass break freq at JFET near-off (22K||100K=18K, 47nF)
    static constexpr float kPhase90MaxFreq = 4500.0f; // Allpass break freq at JFET mid-on (22K||1K=957ohm, 47nF)
    static constexpr float kPhase90BlockFeedback = 0.4f; // Block logo feedback gain

    static float clamp(float v, float lo, float hi) {
        return std::max(lo, std::min(hi, v));
    }
    static int clampInt(int v, int lo, int hi) {
        return std::max(lo, std::min(hi, v));
    }

    void resetStages() {
        for (int s = 0; s < kMaxStages; ++s) {
            apX1_[s] = 0.0f;
            apY1_[s] = 0.0f;
        }
        feedbackSample_ = 0.0f;
        lfoPhase_ = 0.0f;
    }

    std::atomic<float> rate_{0.5f};
    std::atomic<float> depth_{0.7f};
    std::atomic<float> feedback_{0.3f};
    std::atomic<float> stages_{6.0f};
    std::atomic<float> mode_{0.0f};    // 0=Standard, 1=Phase90 Script, 2=Phase90 Block

    int32_t sampleRate_ = 44100;
    int prevMode_ = 0;  // Track mode for change detection (reset stale state)
    float apX1_[kMaxStages] = {};
    float apY1_[kMaxStages] = {};
    float feedbackSample_ = 0.0f;
    float lfoPhase_ = 0.0f;
};

#endif // GUITAR_ENGINE_PHASER_H
