#ifndef GUITAR_ENGINE_PHYSICAL_VOICE_H
#define GUITAR_ENGINE_PHYSICAL_VOICE_H

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "../drum_voice.h"
#include "../../effects/fast_math.h"

/**
 * Physical modeling drum voice using Karplus-Strong and Modal synthesis.
 *
 * Provides two sub-modes for tuned percussion sounds:
 *
 * Karplus-Strong (modelType=0):
 *   Delay-line based plucked string / struck surface model. A noise burst
 *   excites a delay line whose length sets the fundamental pitch. The
 *   feedback path contains a one-pole LP averaging filter controlled by
 *   brightness, and a probabilistic sign flip controlled by material
 *   (0 = string, 1 = drum-like with more sign flips).
 *
 *   Signal flow:
 *     [Noise burst (p samples)] -> [Delay line] -> [LP filter] -> [Sign flip] -> feedback
 *
 * Modal (modelType=1):
 *   Impulse-excited bank of 8 biquad bandpass resonators tuned to
 *   circular membrane Bessel function zeros. Each mode decays independently
 *   with higher modes decaying faster, controlled by the damping parameter.
 *   Suitable for clave, woodblock, bell, and pitched metallic sounds.
 *
 *   Signal flow:
 *     [Impulse] -> [8x Biquad BP resonators] -> [Sum] -> out
 *
 * Parameters:
 *   0: modelType  - 0 = Karplus-Strong, 1 = Modal synthesis
 *   1: pitch      - MIDI note number (20-120), converted to Hz
 *   2: decay      - 0-1, maps to feedback coefficient (KS) or global damping (Modal)
 *   3: brightness - 0-1, LP filter cutoff in KS feedback path
 *   4: material   - 0-1, probability of sign flip on KS feedback sample
 *   5: damping    - 0-1, high-frequency damping (higher modes decay faster)
 *
 * Thread safety: All parameters are std::atomic<float> for UI thread writes.
 * process() is called exclusively from the audio thread.
 */

namespace drums {

class PhysicalVoice : public DrumVoice {
public:
    PhysicalVoice() {
        params_[0].store(0.0f, std::memory_order_relaxed);   // Karplus-Strong
        params_[1].store(60.0f, std::memory_order_relaxed);  // Middle C
        params_[2].store(0.7f, std::memory_order_relaxed);   // moderate decay
        params_[3].store(0.8f, std::memory_order_relaxed);   // bright
        params_[4].store(0.0f, std::memory_order_relaxed);   // string-like
        params_[5].store(0.3f, std::memory_order_relaxed);   // moderate damping

        std::memset(delayLine_, 0, sizeof(delayLine_));
        resetModalState();
    }

    void trigger(float velocity) override {
        velocity_ = velocity;
        active_ = true;
        fadeCounter_ = 0;
        fadeGain_ = 1.0f;

        const int modelType = static_cast<int>(params_[0].load(std::memory_order_relaxed));

        if (modelType == 0) {
            triggerKarplusStrong();
        } else {
            triggerModal();
        }
    }

    float process() override {
        if (!active_) return 0.0f;

        const int modelType = static_cast<int>(params_[0].load(std::memory_order_relaxed));

        float out;
        if (modelType == 0) {
            out = processKarplusStrong();
        } else {
            out = processModal();
        }

        out *= velocity_;

        // Check for silence (-80 dB threshold)
        samplesSinceTrigger_++;
        if (samplesSinceTrigger_ > minActiveSamples_) {
            // Only start checking after minimum active period to avoid
            // premature deactivation during the attack transient
            if (std::abs(out) < 0.0001f && peakTracker_ < 0.0001f) {
                active_ = false;
                return 0.0f;
            }
        }

        // Simple peak tracker for silence detection
        const float absOut = std::abs(out);
        peakTracker_ = (absOut > peakTracker_) ? absOut : peakTracker_ * 0.9999f;

        return applyFade(out);
    }

    void setParam(int paramId, float value) override {
        if (paramId < 0 || paramId > 5) return;

        switch (paramId) {
            case 0: value = (value < 0.5f) ? 0.0f : 1.0f; break;
            case 1: value = clamp(value, 20.0f, 120.0f); break;
            case 2: value = clamp(value, 0.0f, 1.0f); break;
            case 3: value = clamp(value, 0.0f, 1.0f); break;
            case 4: value = clamp(value, 0.0f, 1.0f); break;
            case 5: value = clamp(value, 0.0f, 1.0f); break;
        }
        params_[paramId].store(value, std::memory_order_relaxed);
    }

    float getParam(int paramId) const override {
        if (paramId < 0 || paramId > 5) return 0.0f;
        return params_[paramId].load(std::memory_order_relaxed);
    }

    void setSampleRate(int32_t sr) override {
        sampleRate_ = static_cast<float>(sr);
        // Recompute modal coefficients for new sample rate
        computeModalCoefficients();
    }

    void reset() override {
        active_ = false;
        std::memset(delayLine_, 0, sizeof(delayLine_));
        delayWriteIdx_ = 0;
        ksFilterState_ = 0.0f;
        ksPeriodSamples_ = 0;
        ksExcitationCounter_ = 0;
        prngState_ = 0x12345678u;
        samplesSinceTrigger_ = 0;
        peakTracker_ = 0.0f;
        resetModalState();
        fadeCounter_ = 0;
        fadeGain_ = 1.0f;
    }

private:
    // --- Constants ---

    /** Maximum delay line length: supports ~10Hz at 48kHz. */
    static constexpr int kMaxDelayLength = 4800;

    /** Number of modal resonator modes. */
    static constexpr int kNumModes = 8;

    /**
     * Bessel function zeros for a circular membrane (drum head).
     * These are the frequency ratios of the first 8 modes relative
     * to the fundamental. Unlike a string (integer harmonics), a
     * membrane has inharmonic partials which give drums their
     * characteristic timbre.
     */
    static constexpr float kBesselRatios[kNumModes] = {
        1.000f, 1.593f, 2.136f, 2.296f, 2.653f, 2.917f, 3.156f, 3.500f
    };

    /** Minimum samples before silence detection activates. */
    static constexpr int minActiveSamples_ = 256;

    // --- Parameters ---
    std::atomic<float> params_[6];

    // --- Common state ---
    float sampleRate_ = 48000.0f;
    float velocity_ = 0.0f;
    int   samplesSinceTrigger_ = 0;
    float peakTracker_ = 0.0f;

    // --- Karplus-Strong state ---
    float delayLine_[kMaxDelayLength];
    int   delayWriteIdx_ = 0;
    float ksFilterState_ = 0.0f;   ///< One-pole LP filter state in feedback
    int   ksPeriodSamples_ = 0;    ///< Delay length in samples (= sampleRate / freq)
    int   ksExcitationCounter_ = 0; ///< Remaining noise burst samples
    uint32_t prngState_ = 0x12345678u;

    // --- Modal synthesis state ---
    struct ModalResonator {
        // Biquad state (Direct Form II Transposed)
        double b0 = 0.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;
        double z1 = 0.0, z2 = 0.0;
        float  amplitude = 0.0f; ///< Weight for this mode (1/modeIndex)
    };
    ModalResonator modes_[kNumModes];
    bool modalImpulseFired_ = false;

    // ------------------------------------------------------------------
    // Karplus-Strong implementation
    // ------------------------------------------------------------------

    void triggerKarplusStrong() {
        const float midiNote = params_[1].load(std::memory_order_relaxed);
        const float freq = midiToHz(midiNote);

        // Period in samples, clamped to delay line size
        ksPeriodSamples_ = static_cast<int>(sampleRate_ / freq);
        if (ksPeriodSamples_ < 2) ksPeriodSamples_ = 2;
        if (ksPeriodSamples_ > kMaxDelayLength - 1) ksPeriodSamples_ = kMaxDelayLength - 1;

        // Fill delay line with noise burst for excitation
        ksExcitationCounter_ = ksPeriodSamples_;
        ksFilterState_ = 0.0f;

        // Clear the delay line
        std::memset(delayLine_, 0, sizeof(delayLine_));
        delayWriteIdx_ = 0;

        // Fill initial noise burst into the delay line
        for (int i = 0; i < ksPeriodSamples_; ++i) {
            delayLine_[i] = generateWhiteNoise() * velocity_;
        }
        delayWriteIdx_ = ksPeriodSamples_ % kMaxDelayLength;

        samplesSinceTrigger_ = 0;
        peakTracker_ = 1.0f;
    }

    float processKarplusStrong() {
        const float decay      = params_[2].load(std::memory_order_relaxed);
        const float brightness = params_[3].load(std::memory_order_relaxed);
        const float material   = params_[4].load(std::memory_order_relaxed);

        // Read from delay line at position (writeIdx - period)
        int readIdx = delayWriteIdx_ - ksPeriodSamples_;
        if (readIdx < 0) readIdx += kMaxDelayLength;

        int readIdx1 = readIdx + 1;
        if (readIdx1 >= kMaxDelayLength) readIdx1 -= kMaxDelayLength;

        // Averaging filter: y = 0.5 * (x[n-p] + x[n-p-1])
        float avg = 0.5f * (delayLine_[readIdx] + delayLine_[readIdx1]);

        // One-pole LP filter controlled by brightness
        // brightness=1: fully bright (no filtering), brightness=0: heavy LP
        // LP coefficient: higher = more damping
        const float lpCoeff = 1.0f - brightness * 0.9f; // 0.1 (bright) to 1.0 (dark)
        ksFilterState_ = ksFilterState_ + lpCoeff * (avg - ksFilterState_);
        ksFilterState_ = fast_math::denormal_guard(ksFilterState_);

        float feedback = ksFilterState_;

        // Material: probability of sign flip (0 = string, 1 = percussive)
        // Uses the PRNG to decide per-sample whether to flip the sign.
        // This creates a more drum-like, quickly-decaying timbre.
        if (material > 0.01f) {
            // Generate a random number [0, 1) for comparison
            uint32_t r = prngState_;
            r ^= r << 13; r ^= r >> 17; r ^= r << 5;
            prngState_ = r;
            float randVal = static_cast<float>(r) * (1.0f / 4294967296.0f);
            if (randVal < material * 0.5f) {
                feedback = -feedback;
            }
        }

        // Feedback gain from decay parameter (0.9 to 0.9999)
        const float fbGain = 0.9f + decay * 0.0999f;
        feedback *= fbGain;

        // Write feedback sample into delay line
        delayLine_[delayWriteIdx_] = feedback;
        delayWriteIdx_ = (delayWriteIdx_ + 1) % kMaxDelayLength;

        return feedback;
    }

    // ------------------------------------------------------------------
    // Modal synthesis implementation
    // ------------------------------------------------------------------

    void triggerModal() {
        const float midiNote = params_[1].load(std::memory_order_relaxed);
        const float decay    = params_[2].load(std::memory_order_relaxed);
        const float damping  = params_[5].load(std::memory_order_relaxed);

        const float baseFreq = midiToHz(midiNote);

        // Compute biquad coefficients for each modal resonator
        computeModalCoefficientsForTrigger(baseFreq, decay, damping);

        // Reset resonator state
        for (int i = 0; i < kNumModes; ++i) {
            modes_[i].z1 = 0.0;
            modes_[i].z2 = 0.0;
        }
        modalImpulseFired_ = false;

        samplesSinceTrigger_ = 0;
        peakTracker_ = 1.0f;
    }

    /**
     * Compute biquad bandpass coefficients for all modal resonators.
     *
     * Each mode is a 2-pole bandpass biquad tuned to baseFreq * besselRatio[i].
     * Higher modes get narrower bandwidth (higher Q) scaled by decay, and
     * faster decay scaled by the damping parameter.
     *
     * @param baseFreq  Fundamental frequency in Hz.
     * @param decay     Global decay factor (0-1, higher = longer ring).
     * @param damping   High-frequency damping (0-1, higher = faster HF decay).
     */
    void computeModalCoefficientsForTrigger(float baseFreq, float decay, float damping) {
        for (int i = 0; i < kNumModes; ++i) {
            float freq = baseFreq * kBesselRatios[i];
            // Clamp frequency to below Nyquist
            if (freq >= sampleRate_ * 0.49f) {
                freq = sampleRate_ * 0.49f;
            }

            // Q increases with decay, providing longer ring
            // Base Q: 20-200 mapped from decay 0-1
            float Q = 20.0f + decay * 180.0f;

            // Higher modes decay faster: reduce Q by damping factor
            // mode 0: no extra damping, mode 7: up to 8x faster decay
            Q /= (1.0f + static_cast<float>(i) * damping);

            // Amplitude weighting: 1 / (modeIndex + 1)
            modes_[i].amplitude = 1.0f / static_cast<float>(i + 1);

            // 2-pole bandpass biquad coefficients (peaking EQ at unity gain)
            // Using standard RBJ audio EQ cookbook formulas
            const double w0 = 2.0 * M_PI * static_cast<double>(freq) / static_cast<double>(sampleRate_);
            const double sinW0 = std::sin(w0);
            const double cosW0 = std::cos(w0);
            const double alpha = sinW0 / (2.0 * static_cast<double>(Q));

            const double a0 = 1.0 + alpha;
            const double a0inv = 1.0 / a0;

            modes_[i].b0 = (alpha) * a0inv;
            modes_[i].b1 = 0.0;
            modes_[i].b2 = (-alpha) * a0inv;
            modes_[i].a1 = (-2.0 * cosW0) * a0inv;
            modes_[i].a2 = (1.0 - alpha) * a0inv;
        }
    }

    /**
     * Recompute modal coefficients when sample rate changes (for existing params).
     */
    void computeModalCoefficients() {
        const float midiNote = params_[1].load(std::memory_order_relaxed);
        const float decay    = params_[2].load(std::memory_order_relaxed);
        const float damping  = params_[5].load(std::memory_order_relaxed);
        computeModalCoefficientsForTrigger(midiToHz(midiNote), decay, damping);
    }

    float processModal() {
        // On the first sample after trigger, feed an impulse (1.0) to all resonators.
        // On all subsequent samples, input is 0.0.
        const float input = modalImpulseFired_ ? 0.0f : 1.0f;
        modalImpulseFired_ = true;

        float sum = 0.0f;

        for (int i = 0; i < kNumModes; ++i) {
            // Direct Form II Transposed biquad
            double in = static_cast<double>(input);
            double out = modes_[i].b0 * in + modes_[i].z1;
            modes_[i].z1 = modes_[i].b1 * in - modes_[i].a1 * out + modes_[i].z2;
            modes_[i].z2 = modes_[i].b2 * in - modes_[i].a2 * out;

            // Denormal protection on biquad state
            modes_[i].z1 = fast_math::denormal_guard(modes_[i].z1);
            modes_[i].z2 = fast_math::denormal_guard(modes_[i].z2);

            sum += static_cast<float>(out) * modes_[i].amplitude;
        }

        return sum;
    }

    void resetModalState() {
        for (int i = 0; i < kNumModes; ++i) {
            modes_[i] = ModalResonator{};
        }
        modalImpulseFired_ = false;
    }

    // ------------------------------------------------------------------
    // Utilities
    // ------------------------------------------------------------------

    /** Convert MIDI note number to frequency in Hz. */
    static float midiToHz(float note) {
        return 440.0f * fast_math::exp2_approx((note - 69.0f) / 12.0f);
    }

    /** xorshift32 PRNG: generates white noise in [-1, 1]. */
    float generateWhiteNoise() {
        prngState_ ^= prngState_ << 13;
        prngState_ ^= prngState_ >> 17;
        prngState_ ^= prngState_ << 5;
        return static_cast<float>(static_cast<int32_t>(prngState_)) * (1.0f / 2147483648.0f);
    }

    /** Clamp a value to [lo, hi]. */
    static float clamp(float x, float lo, float hi) {
        return (x < lo) ? lo : ((x > hi) ? hi : x);
    }
};

} // namespace drums

#endif // GUITAR_ENGINE_PHYSICAL_VOICE_H
