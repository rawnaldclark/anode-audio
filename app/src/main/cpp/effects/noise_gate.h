#ifndef GUITAR_ENGINE_NOISE_GATE_H
#define GUITAR_ENGINE_NOISE_GATE_H

#include "effect.h"
#include <atomic>
#include <cmath>
#include <algorithm>

/**
 * Noise gate effect that silences audio below a configurable threshold.
 *
 * Guitar signals always have some noise floor from pickups, cables, and
 * high-gain amplifiers. A noise gate at the front of the chain prevents
 * this noise from being amplified and processed by downstream effects
 * (especially high-gain overdrive which would make hiss very audible).
 *
 * State machine:
 *   CLOSED --(level > threshold + hysteresis)--> OPENING
 *   OPENING --(gain reaches 1.0)---------------> OPEN
 *   OPEN -----(level < threshold)--------------> CLOSING
 *   CLOSING --(gain reaches 0.0)--------------> CLOSED
 *
 * The hysteresis prevents rapid on/off cycling (chattering) when the
 * signal level hovers near the threshold. The signal must rise above
 * (threshold + hysteresis) to open, but only needs to drop below
 * threshold to begin closing.
 *
 * Attack and release use exponential smoothing for natural-sounding
 * envelope transitions. Exponential curves model how real analog gates
 * behave due to capacitor charge/discharge characteristics.
 *
 * Parameter IDs for JNI access:
 *   0 = threshold (-80 to 0 dB)
 *   1 = hysteresis (0 to 20 dB)
 *   2 = attack (0.1 to 50 ms)
 *   3 = release (5 to 500 ms)
 */
class NoiseGate : public Effect {
public:
    NoiseGate() = default;

    // =========================================================================
    // Effect interface
    // =========================================================================

    /**
     * Process audio through the noise gate.
     * Called on the audio thread -- fully real-time safe.
     *
     * Algorithm:
     * 1. For each sample, compute instantaneous level (absolute value)
     * 2. Smooth level with a simple envelope follower
     * 3. Update gate state machine based on smoothed level vs threshold
     * 4. Compute gain envelope (exponential ramp)
     * 5. Apply gain to sample
     *
     * @param buffer   Mono audio samples in [-1.0, 1.0].
     * @param numFrames Number of frames in the buffer.
     */
    void process(float* buffer, int numFrames) override {
        // Read parameters once per buffer (atomics are cheap but
        // reading once is cleaner and avoids mid-buffer parameter jumps
        // that could cause audible artifacts in the envelope)
        const float thresholdDb = threshold_.load(std::memory_order_relaxed);
        const float hysteresisDb = hysteresis_.load(std::memory_order_relaxed);
        const float attackMs = attack_.load(std::memory_order_relaxed);
        const float releaseMs = release_.load(std::memory_order_relaxed);

        // Convert dB thresholds to linear amplitude for comparison.
        // threshold of -60dB = 0.001 linear, -20dB = 0.1, 0dB = 1.0
        const float thresholdLin = dbToLinear(thresholdDb);
        const float openThresholdLin = dbToLinear(thresholdDb + hysteresisDb);

        // Compute smoothing coefficients from millisecond times.
        // coeff = exp(-1 / (time_seconds * sampleRate))
        // When coeff is close to 1.0, the envelope moves slowly (long time).
        // When coeff is close to 0.0, the envelope moves quickly (short time).
        const float attackCoeff = computeCoefficient(attackMs);
        const float releaseCoeff = computeCoefficient(releaseMs);

        // Level detector smoothing coefficient -- fast follower for
        // tracking the signal envelope. 5ms gives responsive tracking
        // without being too jumpy on individual sample peaks.
        const float levelCoeff = computeCoefficient(5.0f);

        for (int i = 0; i < numFrames; ++i) {
            // ---- Level detection ----
            // Use absolute value for instantaneous level (peak detection).
            // Smooth with a one-pole lowpass to get a stable envelope
            // that doesn't react to every individual sample.
            const float instantLevel = std::abs(buffer[i]);
            envelopeLevel_ = levelCoeff * envelopeLevel_ +
                             (1.0f - levelCoeff) * instantLevel;

            // ---- State machine transitions ----
            switch (gateState_) {
                case GateState::CLOSED:
                    // Gate is shut. Only open if level exceeds the hysteresis
                    // threshold (which is higher than the close threshold).
                    if (envelopeLevel_ > openThresholdLin) {
                        gateState_ = GateState::OPENING;
                    }
                    break;

                case GateState::OPENING:
                    // Gate is ramping open. Transition to fully OPEN when
                    // the gain envelope reaches (near) unity.
                    if (gainEnvelope_ >= 0.999f) {
                        gainEnvelope_ = 1.0f;
                        gateState_ = GateState::OPEN;
                    }
                    // If level drops below threshold during opening,
                    // immediately begin closing to avoid letting noise through.
                    if (envelopeLevel_ < thresholdLin) {
                        gateState_ = GateState::CLOSING;
                    }
                    break;

                case GateState::OPEN:
                    // Gate is fully open. Begin closing when the level
                    // drops below the threshold (no hysteresis on close
                    // to ensure responsive shutoff).
                    if (envelopeLevel_ < thresholdLin) {
                        gateState_ = GateState::CLOSING;
                    }
                    break;

                case GateState::CLOSING:
                    // Gate is ramping shut. Transition to CLOSED when
                    // the gain envelope reaches (near) zero.
                    if (gainEnvelope_ <= 0.001f) {
                        gainEnvelope_ = 0.0f;
                        gateState_ = GateState::CLOSED;
                    }
                    // If level rises above hysteresis threshold during closing,
                    // immediately reopen to avoid cutting off sustained notes.
                    if (envelopeLevel_ > openThresholdLin) {
                        gateState_ = GateState::OPENING;
                    }
                    break;
            }

            // ---- Gain envelope smoothing ----
            // Target is 1.0 when opening/open, 0.0 when closing/closed.
            // Use exponential smoothing with attack coeff for opening
            // and release coeff for closing.
            float targetGain = (gateState_ == GateState::OPENING ||
                                gateState_ == GateState::OPEN) ? 1.0f : 0.0f;

            float coeff = (targetGain > gainEnvelope_) ? attackCoeff : releaseCoeff;
            gainEnvelope_ = coeff * gainEnvelope_ + (1.0f - coeff) * targetGain;

            // ---- Apply gain ----
            buffer[i] *= gainEnvelope_;
        }
    }

    /**
     * Configure sample rate and recalculate internal coefficients.
     * @param sampleRate Device sample rate in Hz.
     */
    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;
    }

    /**
     * Reset gate to closed state with zero gain.
     * Called when the audio stream restarts to prevent the gate from
     * being stuck in an open state with stale envelope values.
     */
    void reset() override {
        gateState_ = GateState::CLOSED;
        gainEnvelope_ = 0.0f;
        envelopeLevel_ = 0.0f;
    }

    // =========================================================================
    // Parameter access (called from UI thread via JNI)
    // =========================================================================

    /**
     * Set a parameter by ID. Thread-safe (atomic store).
     * @param paramId  0=threshold, 1=hysteresis, 2=attack, 3=release
     * @param value    Parameter value (see ranges in class doc).
     */
    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0:
                threshold_.store(clamp(value, -80.0f, 0.0f),
                                 std::memory_order_relaxed);
                break;
            case 1:
                hysteresis_.store(clamp(value, 0.0f, 20.0f),
                                  std::memory_order_relaxed);
                break;
            case 2:
                attack_.store(clamp(value, 0.1f, 50.0f),
                              std::memory_order_relaxed);
                break;
            case 3:
                release_.store(clamp(value, 5.0f, 500.0f),
                               std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    /**
     * Get a parameter by ID. Thread-safe (atomic load).
     * @param paramId  0=threshold, 1=hysteresis, 2=attack, 3=release
     * @return Current parameter value.
     */
    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return threshold_.load(std::memory_order_relaxed);
            case 1: return hysteresis_.load(std::memory_order_relaxed);
            case 2: return attack_.load(std::memory_order_relaxed);
            case 3: return release_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    // =========================================================================
    // Gate state machine
    // =========================================================================

    enum class GateState {
        CLOSED,   // Gate fully shut, output is silence
        OPENING,  // Gate transitioning from closed to open (attack phase)
        OPEN,     // Gate fully open, signal passes through
        CLOSING   // Gate transitioning from open to closed (release phase)
    };

    // =========================================================================
    // Internal helpers (all real-time safe, no allocations)
    // =========================================================================

    /**
     * Convert decibels to linear amplitude.
     * @param db Decibel value.
     * @return Linear amplitude (always >= 0).
     */
    static float dbToLinear(float db) {
        return std::pow(10.0f, db / 20.0f);
    }

    /**
     * Compute exponential smoothing coefficient from time in milliseconds.
     * The coefficient determines how quickly the envelope follower
     * converges to the target value.
     *
     * For a time constant T (in seconds) at sample rate fs:
     *   coeff = exp(-1.0 / (T * fs))
     *
     * @param timeMs Time constant in milliseconds.
     * @return Smoothing coefficient in [0, 1).
     */
    float computeCoefficient(float timeMs) const {
        if (timeMs <= 0.0f || sampleRate_ <= 0) return 0.0f;
        const float timeSec = timeMs / 1000.0f;
        return std::exp(-1.0f / (timeSec * static_cast<float>(sampleRate_)));
    }

    /**
     * Clamp a value to a range. Real-time safe.
     */
    static float clamp(float value, float minVal, float maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    // =========================================================================
    // Parameters (written from UI thread, read from audio thread)
    // =========================================================================

    std::atomic<float> threshold_{-40.0f};   // dB, default -40dB
    std::atomic<float> hysteresis_{6.0f};    // dB, default 6dB hysteresis band
    std::atomic<float> attack_{1.0f};        // ms, default 1ms (fast open)
    std::atomic<float> release_{50.0f};      // ms, default 50ms (moderate close)

    // =========================================================================
    // Audio thread state (only accessed from the audio callback)
    // =========================================================================

    int32_t sampleRate_ = 44100;
    GateState gateState_ = GateState::CLOSED;
    float gainEnvelope_ = 0.0f;   // Current gain applied to signal [0, 1]
    float envelopeLevel_ = 0.0f;  // Smoothed signal level for detection
};

#endif // GUITAR_ENGINE_NOISE_GATE_H
