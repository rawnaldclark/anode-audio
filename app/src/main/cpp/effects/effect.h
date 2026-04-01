#ifndef GUITAR_ENGINE_EFFECT_H
#define GUITAR_ENGINE_EFFECT_H

#include <atomic>
#include <cstdint>
#include <cstring>

/**
 * Base class for all audio effects in the signal chain.
 *
 * Subclasses implement process() to transform audio buffers in-place.
 * All methods called from the audio thread (process, isEnabled, getWetDryMix)
 * must be real-time safe: no allocations, no locks, no system calls.
 *
 * The wet/dry mix and enabled state use std::atomic for lock-free
 * communication between the UI thread (writer) and audio thread (reader).
 */
class Effect {
public:
    virtual ~Effect() = default;

    /**
     * Process audio samples in-place.
     * Called on the audio callback thread -- must be real-time safe.
     *
     * @param buffer  Interleaved float samples in [-1.0, 1.0] range.
     * @param numFrames Number of audio frames (not samples) in the buffer.
     */
    virtual void process(float* buffer, int numFrames) = 0;

    /**
     * Inform the effect of the current sample rate.
     * Called once during stream setup, before any process() calls.
     *
     * @param sampleRate Device native sample rate in Hz.
     */
    virtual void setSampleRate(int32_t sampleRate) = 0;

    /**
     * Reset any internal state (delay lines, filter history, etc.).
     * Called when the audio stream restarts or the effect is re-enabled.
     */
    virtual void reset() = 0;

    /**
     * Set a parameter by ID. Each effect subclass defines its own parameter IDs.
     * Called from the UI thread -- parameter storage uses atomics for thread safety.
     *
     * @param paramId Effect-specific parameter identifier.
     * @param value   New parameter value.
     */
    virtual void setParameter(int paramId, float value) { (void)paramId; (void)value; }

    /**
     * Get a parameter by ID. Each effect subclass defines its own parameter IDs.
     * Thread-safe (reads from atomics).
     *
     * @param paramId Effect-specific parameter identifier.
     * @return Current parameter value, or 0.0f if paramId is unknown.
     */
    virtual float getParameter(int paramId) const { (void)paramId; return 0.0f; }

    /** Enable or disable this effect. Disabled effects pass audio through unchanged. */
    void setEnabled(bool enabled) { enabled_.store(enabled, std::memory_order_release); }

    /** Check if this effect is currently enabled. Real-time safe. */
    bool isEnabled() const { return enabled_.load(std::memory_order_acquire); }

    /**
     * Set the wet/dry mix ratio.
     * @param mix 0.0 = fully dry (original signal), 1.0 = fully wet (processed signal).
     */
    void setWetDryMix(float mix) { wetDryMix_.store(mix, std::memory_order_release); }

    /** Get the current wet/dry mix. Real-time safe. */
    float getWetDryMix() const { return wetDryMix_.load(std::memory_order_acquire); }

    /**
     * Utility to apply wet/dry mixing. Blends the processed buffer with the
     * original dry signal. Caller must provide a separate dry buffer copy.
     *
     * @param wetBuffer   The processed (wet) samples, modified in-place to the blend.
     * @param dryBuffer   Copy of the original unprocessed samples.
     * @param numSamples  Total number of float samples (frames * channels).
     */
    void applyWetDryMix(float* wetBuffer, const float* dryBuffer, int numSamples) {
        const float mix = getWetDryMix();
        if (mix >= 1.0f) return; // Fully wet, nothing to blend
        const float dryGain = 1.0f - mix;
        for (int i = 0; i < numSamples; ++i) {
            wetBuffer[i] = wetBuffer[i] * mix + dryBuffer[i] * dryGain;
        }
    }

private:
    std::atomic<bool> enabled_{false};
    std::atomic<float> wetDryMix_{1.0f};
};

#endif // GUITAR_ENGINE_EFFECT_H
