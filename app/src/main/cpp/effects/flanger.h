#ifndef GUITAR_ENGINE_FLANGER_H
#define GUITAR_ENGINE_FLANGER_H

#include "effect.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * Flanger effect using a short modulated delay with feedback.
 *
 * Similar to chorus but with much shorter delay times (0.1-10ms vs 5-25ms)
 * and feedback. The short delay creates a comb filter effect, and the LFO
 * sweep produces the characteristic jet/whoosh sound.
 *
 * Negative feedback inverts the comb filter peaks/nulls, creating a
 * more metallic, hollow sound (through-zero flanging character).
 *
 * Parameter IDs for JNI access:
 *   0 = rate (0.1 to 10.0 Hz) - LFO speed
 *   1 = depth (0.0 to 1.0) - modulation depth
 *   2 = feedback (-0.9 to 0.9) - comb filter resonance (negative = metallic)
 */
class Flanger : public Effect {
public:
    Flanger() = default;

    void process(float* buffer, int numFrames) override {
        const float rate = rate_.load(std::memory_order_relaxed);
        const float depth = depth_.load(std::memory_order_relaxed);
        const float feedback = feedback_.load(std::memory_order_relaxed);

        const float phaseInc = rate / static_cast<float>(sampleRate_);

        // Delay range: 0.1ms to 7ms (very short for flanging)
        const float minDelaySamples = 0.0001f * static_cast<float>(sampleRate_);
        const float maxDelaySamples = 0.007f * static_cast<float>(sampleRate_);
        const float delayRange = (maxDelaySamples - minDelaySamples) * depth;

        for (int i = 0; i < numFrames; ++i) {
            // Triangle LFO for more linear sweep than sine
            float lfo = 2.0f * std::abs(2.0f * lfoPhase_ - 1.0f) - 1.0f;
            lfoPhase_ += phaseInc;
            if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;

            // Modulated delay position
            float delaySamples = minDelaySamples + (0.5f + 0.5f * lfo) * delayRange;
            delaySamples = std::max(1.0f, delaySamples);

            // Write input + feedback to delay buffer
            float input = buffer[i] + feedbackSample_ * feedback;
            delayBuffer_[writePos_] = input;

            // Read with linear interpolation
            float readPosF = static_cast<float>(writePos_) - delaySamples;
            if (readPosF < 0.0f) readPosF += static_cast<float>(bufferSize_);
            int readInt = static_cast<int>(readPosF);
            float frac = readPosF - static_cast<float>(readInt);
            int idx0 = readInt & bufferMask_;
            int idx1 = (readInt + 1) & bufferMask_;
            float delayed = delayBuffer_[idx0] * (1.0f - frac) +
                            delayBuffer_[idx1] * frac;

            // Guard against denormals in the feedback path
            feedbackSample_ = fast_math::denormal_guard(delayed);

            // Mix dry + wet (50/50 for classic flanging)
            buffer[i] = clamp((buffer[i] + delayed) * 0.5f, -1.0f, 1.0f);

            writePos_ = (writePos_ + 1) & bufferMask_;
        }
    }

    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;
        int minSize = static_cast<int>(0.015f * sampleRate) + 1;
        bufferSize_ = nextPowerOfTwo(minSize);
        bufferMask_ = bufferSize_ - 1;
        delayBuffer_.assign(bufferSize_, 0.0f);
        writePos_ = 0;
        feedbackSample_ = 0.0f;
    }

    void reset() override {
        std::fill(delayBuffer_.begin(), delayBuffer_.end(), 0.0f);
        writePos_ = 0;
        feedbackSample_ = 0.0f;
        lfoPhase_ = 0.0f;
    }

    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0: rate_.store(clamp(value, 0.1f, 10.0f), std::memory_order_relaxed); break;
            case 1: depth_.store(clamp(value, 0.0f, 1.0f), std::memory_order_relaxed); break;
            case 2: feedback_.store(clamp(value, -0.9f, 0.9f), std::memory_order_relaxed); break;
            default: break;
        }
    }

    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return rate_.load(std::memory_order_relaxed);
            case 1: return depth_.load(std::memory_order_relaxed);
            case 2: return feedback_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    static int nextPowerOfTwo(int n) {
        int p = 1; while (p < n) p <<= 1; return p;
    }
    static float clamp(float v, float lo, float hi) {
        return std::max(lo, std::min(hi, v));
    }

    std::atomic<float> rate_{0.5f};
    std::atomic<float> depth_{0.7f};
    std::atomic<float> feedback_{0.5f};

    int32_t sampleRate_ = 44100;
    std::vector<float> delayBuffer_;
    int bufferSize_ = 0;
    int bufferMask_ = 0;
    int writePos_ = 0;
    float feedbackSample_ = 0.0f;
    float lfoPhase_ = 0.0f;
};

#endif // GUITAR_ENGINE_FLANGER_H
