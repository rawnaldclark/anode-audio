#ifndef GUITAR_ENGINE_PASSTHROUGH_H
#define GUITAR_ENGINE_PASSTHROUGH_H

#include "effect.h"

/**
 * Passthrough effect that leaves the audio buffer unchanged.
 *
 * Used for testing the audio pipeline end-to-end without any signal
 * modification. Validates that the full-duplex input-to-output path
 * works correctly with minimal latency before adding real effects.
 */
class Passthrough : public Effect {
public:
    /**
     * No-op processing: the buffer passes through unchanged.
     * This is intentionally empty -- the audio data remains as-is.
     */
    void process(float* /* buffer */, int /* numFrames */) override {
        // Intentionally empty: passthrough leaves audio unmodified
    }

    void setSampleRate(int32_t /* sampleRate */) override {
        // Passthrough has no sample-rate-dependent state
    }

    void reset() override {
        // Passthrough has no internal state to reset
    }
};

#endif // GUITAR_ENGINE_PASSTHROUGH_H
