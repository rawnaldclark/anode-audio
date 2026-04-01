#ifndef GUITAR_ENGINE_DELAY_H
#define GUITAR_ENGINE_DELAY_H

#include "effect.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * Delay effect with feedback and linear interpolation.
 *
 * Delay is one of the most fundamental guitar effects. It records incoming
 * audio into a circular buffer and plays it back after a configurable time,
 * creating echoes. With feedback, each echo is fed back into the delay line
 * to create repeating, decaying echoes.
 *
 * Implementation details:
 *
 * Circular buffer:
 *   The delay line uses a power-of-two sized circular buffer with a
 *   bitmask for wrap-around (instead of modulo, which is slower).
 *   The buffer is pre-allocated in setSampleRate() for the maximum
 *   delay time (2 seconds) to ensure no allocations during processing.
 *
 * Linear interpolation:
 *   When the delay time doesn't align exactly to an integer number of
 *   samples, we interpolate between the two nearest samples. This is
 *   critical for smooth delay time changes (avoids zipper noise) and
 *   for sub-sample accuracy with musical delay times.
 *
 *   For example, 375ms at 44100Hz = 16537.5 samples.
 *   We read sample[16537] and sample[16538], then blend:
 *     output = sample[16537] * 0.5 + sample[16538] * 0.5
 *
 * Feedback:
 *   The delay output is mixed back into the input:
 *     delayLine[writePos] = input + feedback * delayOutput
 *
 *   Feedback is capped at 0.95 to prevent infinite buildup. Values
 *   above ~0.9 create very long decay times; exactly 1.0 would create
 *   an infinite loop that never decays.
 *
 * Wet/dry mixing is handled by the base Effect class, so the delay's
 * process() outputs the fully wet (delayed) signal, and the signal chain
 * blends it with the dry signal according to the wetDryMix parameter.
 *
 * Parameter IDs for JNI access:
 *   0 = delayTimeMs (1 to 2000 ms)
 *   1 = feedback (0.0 to 0.95)
 */
class Delay : public Effect {
public:
    Delay() = default;

    // =========================================================================
    // Effect interface
    // =========================================================================

    /**
     * Process audio through the delay line. Real-time safe.
     *
     * @param buffer   Mono audio samples in [-1.0, 1.0].
     * @param numFrames Number of frames in the buffer.
     */
    void process(float* buffer, int numFrames) override {
        // Snapshot parameters
        const float delayMs = delayTimeMs_.load(std::memory_order_relaxed);
        const float feedback = feedback_.load(std::memory_order_relaxed);

        // Convert delay time to fractional sample position.
        // delaySamples can be fractional for sub-sample accuracy.
        const float delaySamples = (delayMs / 1000.0f) *
                                    static_cast<float>(sampleRate_);

        for (int i = 0; i < numFrames; ++i) {
            const float inputSample = buffer[i];

            // ---- Read from delay line with linear interpolation ----
            // Calculate the read position as a float for fractional delay.
            float readPos = static_cast<float>(writePos_) - delaySamples;

            // Wrap negative positions into the valid buffer range.
            // We add bufferSize_ to ensure positive values before masking.
            if (readPos < 0.0f) {
                readPos += static_cast<float>(bufferSize_);
            }

            // Split into integer and fractional parts for interpolation
            const int readPosInt = static_cast<int>(readPos);
            const float frac = readPos - static_cast<float>(readPosInt);

            // Read the two nearest samples, wrapping with bitmask
            const int idx0 = readPosInt & bufferMask_;
            const int idx1 = (readPosInt + 1) & bufferMask_;

            // Linear interpolation between adjacent samples.
            // When frac=0, we use exactly sample[idx0].
            // When frac=0.5, we blend equally between both.
            const float delayedSample = delayBuffer_[idx0] * (1.0f - frac) +
                                         delayBuffer_[idx1] * frac;

            // ---- Write to delay line ----
            // Mix the input with the fed-back delayed signal.
            // This creates the repeating echo effect.
            // Guard against denormals in the feedback path: as echoes decay
            // toward zero, the feedback accumulation can produce denormalized
            // floats that cause 10-100x CPU slowdowns on some ARM processors.
            delayBuffer_[writePos_] = inputSample
                + fast_math::denormal_guard(feedback * delayedSample);

            // Advance write position with wrap-around via bitmask
            writePos_ = (writePos_ + 1) & bufferMask_;

            // ---- Output ----
            // Output the delayed signal (wet only).
            // The base class handles wet/dry blending via applyWetDryMix().
            buffer[i] = delayedSample;
        }
    }

    /**
     * Configure sample rate and pre-allocate the delay buffer.
     *
     * The buffer is sized to the next power of two that can hold
     * the maximum delay time (2 seconds). Power-of-two sizing enables
     * using a bitmask for wrapping instead of the modulo operator,
     * which is significantly faster on ARM processors.
     *
     * @param sampleRate Device sample rate in Hz.
     */
    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;

        // Calculate buffer size for maximum delay time (2 seconds)
        const int requiredSize = static_cast<int>(
            kMaxDelayMs / 1000.0f * static_cast<float>(sampleRate)) + 1;

        // Round up to next power of two for efficient bitmask wrapping
        bufferSize_ = nextPowerOfTwo(requiredSize);
        bufferMask_ = bufferSize_ - 1;

        // Allocate and zero the delay buffer.
        // This is the ONLY allocation -- it happens during stream setup,
        // never on the audio thread.
        delayBuffer_.assign(bufferSize_, 0.0f);

        writePos_ = 0;
    }

    /**
     * Reset the delay line to silence.
     * Clears the circular buffer and resets the write position.
     * Called when the stream restarts to prevent old echoes from
     * playing back after a pause.
     */
    void reset() override {
        std::fill(delayBuffer_.begin(), delayBuffer_.end(), 0.0f);
        writePos_ = 0;
    }

    // =========================================================================
    // Parameter access (called from UI thread via JNI)
    // =========================================================================

    /**
     * Set a parameter by ID. Thread-safe.
     * @param paramId  0=delayTimeMs, 1=feedback
     * @param value    Parameter value (see ranges in class doc).
     */
    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0:
                delayTimeMs_.store(clamp(value, 1.0f, kMaxDelayMs),
                                   std::memory_order_relaxed);
                break;
            case 1:
                feedback_.store(clamp(value, 0.0f, kMaxFeedback),
                                std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    /**
     * Get a parameter by ID. Thread-safe.
     * @param paramId  0=delayTimeMs, 1=feedback
     * @return Current parameter value.
     */
    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return delayTimeMs_.load(std::memory_order_relaxed);
            case 1: return feedback_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    // =========================================================================
    // Constants
    // =========================================================================

    /** Maximum delay time in milliseconds (2 seconds). */
    static constexpr float kMaxDelayMs = 2000.0f;

    /**
     * Maximum feedback coefficient.
     * Capped below 1.0 to prevent infinite feedback loops that would
     * cause the signal to grow without bound, eventually hitting digital
     * full-scale and producing harsh distortion.
     */
    static constexpr float kMaxFeedback = 0.95f;

    // =========================================================================
    // Internal helpers
    // =========================================================================

    /**
     * Round up to the next power of two.
     * Used to size the circular buffer for efficient bitmask wrapping.
     *
     * @param n Minimum required size.
     * @return Smallest power of two >= n.
     */
    static int nextPowerOfTwo(int n) {
        int power = 1;
        while (power < n) {
            power <<= 1;
        }
        return power;
    }

    /**
     * Clamp a value to a range.
     */
    static float clamp(float value, float minVal, float maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    // =========================================================================
    // Parameters (written from UI thread, read from audio thread)
    // =========================================================================

    std::atomic<float> delayTimeMs_{350.0f};  // ms, default 350ms (dotted eighth at 120bpm approx)
    std::atomic<float> feedback_{0.4f};       // [0, 0.95], default moderate feedback

    // =========================================================================
    // Audio thread state
    // =========================================================================

    int32_t sampleRate_ = 44100;

    /** Circular delay buffer. Power-of-two sized for bitmask wrapping. */
    std::vector<float> delayBuffer_;

    /** Size of delay buffer (always a power of two). */
    int bufferSize_ = 0;

    /** Bitmask for wrap-around: bufferSize_ - 1. */
    int bufferMask_ = 0;

    /** Current write position in the circular buffer. */
    int writePos_ = 0;
};

#endif // GUITAR_ENGINE_DELAY_H
