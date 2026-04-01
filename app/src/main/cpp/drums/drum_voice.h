#ifndef GUITAR_ENGINE_DRUM_VOICE_H
#define GUITAR_ENGINE_DRUM_VOICE_H

#include <cstdint>

/**
 * Abstract base class for all drum synthesis voices.
 *
 * Each drum track owns one DrumVoice instance that generates audio
 * sample-by-sample. Subclasses implement the actual synthesis algorithm
 * (FM, subtractive, metallic, noise, physical model, multi-layer).
 *
 * Lifecycle:
 *   1. setSampleRate() called once during stream setup.
 *   2. trigger(velocity) called by the sequencer when a step fires.
 *   3. process() called per-sample to generate output.
 *   4. Voice sets active_ = false when its envelope decays to silence.
 *   5. fadeOut() called for choke group behavior (2ms exponential fade).
 *
 * Thread safety:
 *   - trigger() may be called from the audio thread (sequencer step)
 *     or from the UI thread (pad tap via triggerPad). Since both paths
 *     converge through DrumEngine::renderBlock() or DrumEngine::triggerPad(),
 *     and triggerPad() writes to an atomic command slot consumed by the
 *     audio thread, there is no concurrent access to voice internals.
 *   - process() is called exclusively from the audio thread.
 *   - setParam()/getParam() use atomic storage for cross-thread safety.
 *
 * All methods called from the audio thread must be real-time safe:
 * no allocations, no locks, no system calls, no logging.
 */

namespace drums {

class DrumVoice {
public:
    virtual ~DrumVoice() = default;

    /**
     * Trigger the voice with a given velocity.
     * Resets envelopes and oscillator phases as appropriate for the voice type.
     *
     * @param velocity Note velocity in [0.0, 1.0] range (0 = silent, 1 = max).
     */
    virtual void trigger(float velocity) = 0;

    /**
     * Generate one output sample from this voice.
     * Called per-sample from DrumTrack::processBlock().
     * Returns 0.0f when the voice is inactive.
     *
     * @return Audio sample in approximately [-1.0, 1.0] range.
     */
    virtual float process() = 0;

    /**
     * Set a synthesis parameter by ID.
     * Parameter IDs are voice-type specific (see each voice header).
     * Called from UI thread -- implementations must use atomic storage.
     *
     * @param paramId Voice-specific parameter identifier (0-15).
     * @param value   Parameter value (range depends on parameter).
     */
    virtual void setParam(int paramId, float value) = 0;

    /**
     * Get a synthesis parameter by ID. Thread-safe.
     *
     * @param paramId Voice-specific parameter identifier (0-15).
     * @return Current parameter value, or 0.0f if paramId is unknown.
     */
    virtual float getParam(int paramId) const = 0;

    /**
     * Set the audio sample rate. Called once during stream setup.
     * Subclasses should recalculate any rate-dependent coefficients
     * (envelope decay rates, filter coefficients, etc.).
     *
     * @param sr Device native sample rate in Hz.
     */
    virtual void setSampleRate(int32_t sr) = 0;

    /**
     * Reset all internal state (oscillator phases, envelope values,
     * filter history). Called when the audio stream restarts.
     */
    virtual void reset() = 0;

    /**
     * Check if this voice is currently producing audio.
     * Returns false when the amplitude envelope has decayed below
     * the silence threshold. Used by DrumEngine::hasActiveVoices()
     * to determine if drum tails are still audible after transport stop.
     */
    bool isActive() const { return active_; }

    /**
     * Initiate a fast fade-out for choke group behavior.
     *
     * When a choke group member triggers, all other members in the
     * same group receive a fadeOut() call. The fade is a ~2ms exponential
     * decay (96 samples at 48kHz) to avoid clicks.
     *
     * @param fadeSamples Number of samples for the fade (default 96 = ~2ms at 48kHz).
     */
    void fadeOut(int fadeSamples = 96) {
        if (active_) {
            fadeCounter_ = fadeSamples;
            fadeGain_ = 1.0f;
        }
    }

protected:
    bool  active_ = false;      ///< True while voice is producing audio
    int   fadeCounter_ = 0;     ///< Remaining fade-out samples (0 = no fade)
    float fadeGain_ = 1.0f;     ///< Current fade gain multiplier

    /**
     * Apply choke-group fade to an output sample.
     * Call this at the end of process() to handle fadeOut() requests.
     * When the fade completes, the voice is deactivated.
     *
     * The fade uses an exponential curve for a natural decay:
     *   fadeGain_ *= decayCoeff (where decayCoeff drives ~60dB attenuation
     *   over fadeSamples). Simplified here as a linear ramp for efficiency
     *   since 2ms is short enough that the difference is inaudible.
     *
     * @param sample The voice's output sample before fade.
     * @return The sample with fade applied, or 0.0f if voice deactivated.
     */
    float applyFade(float sample) {
        if (fadeCounter_ <= 0) {
            return sample;
        }

        // Exponential-ish fade: multiply by decreasing gain
        // Using a per-sample coefficient that reaches ~-60dB over fadeCounter_ samples
        // Approximation: gain *= 0.95 gives ~60dB fade in ~90 samples (~2ms at 48kHz)
        fadeGain_ *= 0.95f;
        fadeCounter_--;

        if (fadeCounter_ <= 0) {
            active_ = false;
            fadeGain_ = 1.0f;
            return 0.0f;
        }

        return sample * fadeGain_;
    }
};

} // namespace drums

#endif // GUITAR_ENGINE_DRUM_VOICE_H
