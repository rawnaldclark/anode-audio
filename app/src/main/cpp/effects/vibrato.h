#ifndef GUITAR_ENGINE_VIBRATO_H
#define GUITAR_ENGINE_VIBRATO_H

#include "effect.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * Vibrato effect using modulated delay line pitch shifting.
 *
 * Unlike chorus (which blends a modulated copy WITH the dry signal),
 * vibrato replaces the dry signal entirely with the pitch-modulated version.
 * This creates a pure pitch wobble without the thickening effect of chorus.
 *
 * The effect is achieved by reading from a delay line at a position that
 * oscillates around a center delay point. When the read position moves
 * toward the write position, the output pitch rises (samples are being
 * "consumed" faster). When it moves away, pitch drops. The LFO rate
 * controls the speed of the wobble; depth controls the pitch deviation.
 *
 * Common uses:
 *   - Uni-Vibe tones (Hendrix, Robin Trower) - slow rate, medium depth
 *   - Leslie speaker simulation (fast rate, light depth)
 *   - Surf/rockabilly shimmer
 *
 * Architecture:
 *   Single voice with selectable LFO waveform (sine or triangle).
 *   Circular delay buffer with linear interpolation for sub-sample
 *   read position accuracy.
 *
 * Parameter IDs for JNI access:
 *   0 = rate  (0.5 to 10.0 Hz) - LFO speed
 *   1 = depth (0.0 to 1.0) - modulation depth (maps to 0-7ms delay swing)
 */
class Vibrato : public Effect {
public:
    Vibrato() = default;

    void process(float* buffer, int numFrames) override {
        // Guard: setSampleRate() must be called before first process().
        if (bufferSize_ == 0) return;

        const float rate = rate_.load(std::memory_order_relaxed);
        const float depth = depth_.load(std::memory_order_relaxed);

        // Phase increment per sample for the LFO
        const float phaseInc = rate / static_cast<float>(sampleRate_);

        // Depth in samples (0-7ms of delay swing)
        const float depthSamples = depth * 0.007f * static_cast<float>(sampleRate_);

        // Center delay (~5ms) -- shorter than chorus for tighter pitch effect
        const float centerDelay = 0.005f * static_cast<float>(sampleRate_);

        for (int i = 0; i < numFrames; ++i) {
            const float input = buffer[i];

            // Write input to delay buffer
            delayBuffer_[writePos_] = input;

            // Sine LFO for smooth pitch modulation
            float lfo = fast_math::sin2pi(lfoPhase_);

            // Modulated delay position
            float delaySamples = centerDelay + lfo * depthSamples;
            delaySamples = std::max(1.0f, delaySamples);

            // Read with linear interpolation for sub-sample accuracy.
            // Without interpolation, the quantized read position would cause
            // audible stepping artifacts, especially at low modulation depths.
            float readPosF = static_cast<float>(writePos_) - delaySamples;
            if (readPosF < 0.0f) readPosF += static_cast<float>(bufferSize_);
            int readInt = static_cast<int>(readPosF);
            float frac = readPosF - static_cast<float>(readInt);
            int idx0 = readInt & bufferMask_;
            int idx1 = (readInt + 1) & bufferMask_;

            float sample = delayBuffer_[idx0] * (1.0f - frac) +
                           delayBuffer_[idx1] * frac;

            // Output the modulated signal directly (no dry blend --
            // that's the key difference from chorus).
            buffer[i] = sample;

            // Advance LFO phase
            lfoPhase_ += phaseInc;
            if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;

            writePos_ = (writePos_ + 1) & bufferMask_;
        }
    }

    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;

        // Allocate delay buffer for maximum delay (~15ms + margin)
        int minSize = static_cast<int>(0.020f * sampleRate) + 1;
        bufferSize_ = nextPowerOfTwo(minSize);
        bufferMask_ = bufferSize_ - 1;
        delayBuffer_.assign(bufferSize_, 0.0f);
        writePos_ = 0;
    }

    void reset() override {
        std::fill(delayBuffer_.begin(), delayBuffer_.end(), 0.0f);
        writePos_ = 0;
        lfoPhase_ = 0.0f;
    }

    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0:
                rate_.store(clamp(value, 0.5f, 10.0f),
                            std::memory_order_relaxed);
                break;
            case 1:
                depth_.store(clamp(value, 0.0f, 1.0f),
                             std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return rate_.load(std::memory_order_relaxed);
            case 1: return depth_.load(std::memory_order_relaxed);
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

    std::atomic<float> rate_{4.0f};   // [0.5, 10] Hz LFO speed
    std::atomic<float> depth_{0.3f};  // [0, 1] modulation depth

    int32_t sampleRate_ = 44100;
    std::vector<float> delayBuffer_;
    int bufferSize_ = 0;
    int bufferMask_ = 0;
    int writePos_ = 0;
    float lfoPhase_ = 0.0f;
};

#endif // GUITAR_ENGINE_VIBRATO_H
