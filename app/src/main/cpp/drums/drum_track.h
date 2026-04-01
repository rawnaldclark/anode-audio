#ifndef GUITAR_ENGINE_DRUM_TRACK_H
#define GUITAR_ENGINE_DRUM_TRACK_H

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>

#include "drum_types.h"
#include "drum_voice.h"
#include "../effects/fast_math.h"

/**
 * Owns one DrumVoice and manages step triggering, retrig subdivision,
 * choke group response, and per-track post-processing (drive + SVF filter).
 *
 * Signal flow per track:
 *   [DrumVoice::process()] -> [Drive (fast_tanh)] -> [SVF Filter] -> [Amp * Volume] -> [Pan] -> L/R bus
 *
 * The SVF filter uses topology-preserving transform (TPT) form for stability
 * at high cutoff frequencies, matching the approach used throughout the
 * guitar effects chain.
 *
 * Retrig support:
 *   When a step has retrigCount > 0, the step duration is subdivided into
 *   N equally-spaced sub-triggers. Each sub-trigger fires with velocity
 *   modified by the retrigDecay curve (flat, ramp-up, ramp-down).
 *
 * Thread safety:
 *   - volume_, pan_, muted_, drive_, filterCutoff_, filterRes_ are atomics
 *     (set from UI thread, read from audio thread).
 *   - Voice pointer swap (setVoice) must only be called when the engine
 *     is stopped or via a double-buffered mechanism.
 *   - processBlock() and triggerStep() are called exclusively from audio thread.
 */

namespace drums {

class DrumTrack {
public:
    DrumTrack() = default;

    /**
     * Set the synthesis voice for this track.
     * Must only be called when the audio engine is stopped or from a
     * thread-safe swap mechanism. The old voice is returned for cleanup
     * outside the audio thread.
     *
     * @param v New voice instance (takes ownership).
     * @return Previous voice instance (caller must delete outside audio thread).
     */
    std::unique_ptr<DrumVoice> setVoice(std::unique_ptr<DrumVoice> v) {
        auto old = std::move(voice_);
        voice_ = std::move(v);
        if (voice_ && sampleRate_ > 0) {
            voice_->setSampleRate(sampleRate_);
        }
        return old;
    }

    /** Get a raw pointer to the current voice (for parameter access). */
    DrumVoice* getVoice() { return voice_.get(); }
    const DrumVoice* getVoice() const { return voice_.get(); }

    /**
     * Trigger the voice from a sequencer step.
     *
     * Evaluates the step's probability field using a fast PRNG.
     * If the step passes the probability check, the voice is triggered
     * with the step's velocity (normalized to [0, 1]).
     *
     * For retrig steps, sets up internal retrig state so that subsequent
     * calls to processBlock() will fire sub-triggers at evenly spaced
     * intervals within the step duration.
     *
     * @param step       The step data to evaluate and potentially trigger.
     * @param stepsPerStep Samples per step (from SequencerClock), used for retrig spacing.
     */
    void triggerStep(const Step& step, double samplesPerStep) {
        if (!step.trigger || !voice_) return;
        if (muted_.load(std::memory_order_relaxed)) return;

        // Probability check: random value [1, 100] compared to step probability
        if (step.probability < 100) {
            uint32_t r = xorshift32() % 100 + 1;
            if (r > step.probability) return;
        }

        // Normalize velocity from [0, 127] to [0.0, 1.0]
        float vel = static_cast<float>(step.velocity) / 127.0f;

        // Set up retrig if count > 0
        if (step.retrigCount >= 2) {
            retrigRemaining_ = step.retrigCount - 1; // first trigger fires now
            retrigSpacing_ = static_cast<int>(samplesPerStep / step.retrigCount);
            retrigCounter_ = retrigSpacing_;
            retrigBaseVelocity_ = vel;
            retrigDecayMode_ = step.retrigDecay;
            retrigIndex_ = 1;
            retrigTotal_ = step.retrigCount;
        } else {
            retrigRemaining_ = 0;
        }

        // Fire the initial trigger
        voice_->trigger(vel);
    }

    /**
     * Trigger the voice immediately with a given velocity (for pad tap).
     *
     * @param velocity Velocity in [0.0, 1.0].
     */
    void triggerImmediate(float velocity) {
        if (voice_) {
            retrigRemaining_ = 0;
            voice_->trigger(velocity);
        }
    }

    /**
     * Initiate a choke fade-out on this track's voice.
     * Called when another track in the same choke group triggers.
     */
    void choke() {
        if (voice_ && voice_->isActive()) {
            voice_->fadeOut();
        }
    }

    /**
     * Process one block of audio from this track, summing into L/R output buffers.
     *
     * For each sample:
     *   1. Check for pending retrig sub-triggers.
     *   2. Get sample from voice->process().
     *   3. Apply drive saturation (fast_tanh).
     *   4. Apply SVF filter (TPT form, lowpass).
     *   5. Apply volume scaling.
     *   6. Apply pan law (constant-power) and sum into L/R buffers.
     *
     * @param outL   Left output buffer to sum into (NOT cleared by this method).
     * @param outR   Right output buffer to sum into (NOT cleared by this method).
     * @param numFrames Number of audio frames to process.
     */
    void processBlock(float* outL, float* outR, int numFrames) {
        if (!voice_) return;

        // Read mix parameters (relaxed: eventual consistency is fine)
        const float vol = volume_.load(std::memory_order_relaxed);
        const float panVal = pan_.load(std::memory_order_relaxed);
        const float driveAmt = drive_.load(std::memory_order_relaxed);
        const float cutoff = filterCutoff_.load(std::memory_order_relaxed);
        const float res = filterRes_.load(std::memory_order_relaxed);
        const bool isMuted = muted_.load(std::memory_order_relaxed);

        if (isMuted && !voice_->isActive()) return;

        // Update SVF coefficients if cutoff/res changed
        updateSVFCoefficients(cutoff, res);

        // Constant-power pan law: L = cos(angle), R = sin(angle)
        // where angle = (pan + 1) * pi/4 maps [-1,1] to [0, pi/2]
        const float angle = (panVal + 1.0f) * 0.25f * 3.14159265f;
        const float panL = std::cos(angle);
        const float panR = std::sin(angle);

        for (int i = 0; i < numFrames; ++i) {
            // Handle retrig sub-triggers
            if (retrigRemaining_ > 0) {
                retrigCounter_--;
                if (retrigCounter_ <= 0) {
                    float vel = computeRetrigVelocity();
                    voice_->trigger(vel);
                    retrigRemaining_--;
                    retrigCounter_ = retrigSpacing_;
                    retrigIndex_++;
                }
            }

            // Generate voice sample
            float sample = voice_->process();
            if (sample == 0.0f && !voice_->isActive()) continue;

            // Drive saturation (skip if drive is zero for efficiency)
            if (driveAmt > 0.01f) {
                sample = fast_math::fast_tanh(sample * (1.0f + driveAmt * 4.0f));
            }

            // SVF lowpass filter (TPT form)
            sample = processSVF(sample);

            // Apply volume (muted tracks still fade out active voices)
            float gain = isMuted ? 0.0f : vol;
            sample *= gain;

            // Pan and sum into stereo output
            outL[i] += sample * panL;
            outR[i] += sample * panR;
        }
    }

    /**
     * Set the audio sample rate. Propagates to the owned voice
     * and recalculates SVF filter coefficients.
     *
     * @param sr Device native sample rate in Hz.
     */
    void setSampleRate(int32_t sr) {
        sampleRate_ = sr;
        if (voice_) {
            voice_->setSampleRate(sr);
        }
        // Reset SVF state
        svfIc1eq_ = 0.0f;
        svfIc2eq_ = 0.0f;
        prevCutoff_ = -1.0f; // Force coefficient recalc
    }

    /** Reset all track state (voice, filter, retrigs). */
    void reset() {
        if (voice_) {
            voice_->reset();
        }
        svfIc1eq_ = 0.0f;
        svfIc2eq_ = 0.0f;
        retrigRemaining_ = 0;
        retrigCounter_ = 0;
    }

    /** Check if this track's voice is currently producing audio. */
    bool isActive() const {
        return voice_ && voice_->isActive();
    }

    // --- Atomic parameter setters (called from UI thread) ---

    void setVolume(float v) { volume_.store(v, std::memory_order_relaxed); }
    float getVolume() const { return volume_.load(std::memory_order_relaxed); }

    void setPan(float p) { pan_.store(p, std::memory_order_relaxed); }
    float getPan() const { return pan_.load(std::memory_order_relaxed); }

    void setMuted(bool m) { muted_.store(m, std::memory_order_relaxed); }
    bool isMuted() const { return muted_.load(std::memory_order_relaxed); }

    void setDrive(float d) { drive_.store(d, std::memory_order_relaxed); }
    float getDrive() const { return drive_.load(std::memory_order_relaxed); }

    void setFilterCutoff(float fc) { filterCutoff_.store(fc, std::memory_order_relaxed); }
    float getFilterCutoff() const { return filterCutoff_.load(std::memory_order_relaxed); }

    void setFilterRes(float r) { filterRes_.store(r, std::memory_order_relaxed); }
    float getFilterRes() const { return filterRes_.load(std::memory_order_relaxed); }

    void setChokeGroup(int8_t group) { chokeGroup_ = group; }
    int8_t getChokeGroup() const { return chokeGroup_; }

private:
    /**
     * Compute velocity for the current retrig sub-trigger based on decay mode.
     * 0=flat (same velocity), 1=ramp-up, 2=ramp-down.
     */
    float computeRetrigVelocity() const {
        if (retrigTotal_ <= 1) return retrigBaseVelocity_;

        float t = static_cast<float>(retrigIndex_) / static_cast<float>(retrigTotal_ - 1);
        switch (retrigDecayMode_) {
            case 0: // Flat
                return retrigBaseVelocity_;
            case 1: // Ramp-up: start at 25% velocity, increase to full
                return retrigBaseVelocity_ * (0.25f + 0.75f * t);
            case 2: // Ramp-down: start at full velocity, decrease to 25%
                return retrigBaseVelocity_ * (1.0f - 0.75f * t);
            default:
                return retrigBaseVelocity_;
        }
    }

    /**
     * Update SVF filter coefficients when cutoff or resonance changes.
     * Uses topology-preserving transform (TPT) for stability at high cutoff.
     *
     * @param cutoff Filter cutoff frequency in Hz.
     * @param res    Resonance amount in [0, 1].
     */
    void updateSVFCoefficients(float cutoff, float res) {
        // Skip recalc if parameters haven't changed (avoid trig in audio path)
        if (cutoff == prevCutoff_ && res == prevRes_) return;
        prevCutoff_ = cutoff;
        prevRes_ = res;

        // TPT coefficient: g = tan(pi * fc / fs)
        // Use fast_math::tan_approx for the audio-thread path
        float fc = cutoff;
        if (fc < 20.0f) fc = 20.0f;
        float fs = static_cast<float>(sampleRate_);
        if (fc > fs * 0.49f) fc = fs * 0.49f;

        double g = fast_math::tan_approx(3.14159265358979 * static_cast<double>(fc) / static_cast<double>(fs));
        svfG_ = static_cast<float>(g);

        // Damping: k = 2 - 2*res (res=0 => k=2 (no resonance), res=1 => k=0 (self-oscillation))
        svfK_ = 2.0f * (1.0f - res);

        // Pre-compute: a1 = 1 / (1 + g*(g + k))
        svfA1_ = 1.0f / (1.0f + svfG_ * (svfG_ + svfK_));
        // a2 = g * a1
        svfA2_ = svfG_ * svfA1_;
        // a3 = g * a2
        svfA3_ = svfG_ * svfA2_;
    }

    /**
     * Process one sample through the SVF lowpass filter (TPT form).
     *
     * @param input Input sample.
     * @return Lowpass filtered output.
     */
    float processSVF(float input) {
        float v3 = input - svfIc2eq_;
        float v1 = svfA1_ * svfIc1eq_ + svfA2_ * v3;
        float v2 = svfIc2eq_ + svfA2_ * svfIc1eq_ + svfA3_ * v3;
        svfIc1eq_ = 2.0f * v1 - svfIc1eq_;
        svfIc2eq_ = 2.0f * v2 - svfIc2eq_;

        // Denormal guard on filter state
        svfIc1eq_ = fast_math::denormal_guard(svfIc1eq_);
        svfIc2eq_ = fast_math::denormal_guard(svfIc2eq_);

        return v2; // Lowpass output
    }

    /**
     * Fast 32-bit xorshift PRNG for probability evaluation.
     * Not cryptographic, but sufficient for musical randomization.
     * Period: 2^32 - 1.
     */
    uint32_t xorshift32() {
        rngState_ ^= rngState_ << 13;
        rngState_ ^= rngState_ >> 17;
        rngState_ ^= rngState_ << 5;
        return rngState_;
    }

    // --- Voice ---
    std::unique_ptr<DrumVoice> voice_;

    // --- Mix parameters (UI thread writes, audio thread reads) ---
    std::atomic<float> volume_{0.8f};
    std::atomic<float> pan_{0.0f};
    std::atomic<bool>  muted_{false};
    std::atomic<float> drive_{0.0f};           ///< Drive saturation amount 0-5
    std::atomic<float> filterCutoff_{16000.0f}; ///< SVF cutoff in Hz
    std::atomic<float> filterRes_{0.0f};       ///< SVF resonance 0-1

    int8_t chokeGroup_ = -1;    ///< -1=none, 0=hat group, 1-3=user groups
    int32_t sampleRate_ = 48000;

    // --- SVF filter state (audio thread only) ---
    float svfG_ = 0.0f;        ///< TPT coefficient g = tan(pi*fc/fs)
    float svfK_ = 2.0f;        ///< Damping 2*(1-res)
    float svfA1_ = 0.0f;       ///< Pre-computed: 1/(1+g*(g+k))
    float svfA2_ = 0.0f;       ///< Pre-computed: g*a1
    float svfA3_ = 0.0f;       ///< Pre-computed: g*a2
    float svfIc1eq_ = 0.0f;    ///< SVF integrator state 1
    float svfIc2eq_ = 0.0f;    ///< SVF integrator state 2
    float prevCutoff_ = -1.0f; ///< Last cutoff for change detection
    float prevRes_ = -1.0f;    ///< Last resonance for change detection

    // --- Retrig state (audio thread only) ---
    int   retrigRemaining_ = 0;     ///< Sub-triggers still to fire
    int   retrigSpacing_ = 0;       ///< Samples between sub-triggers
    int   retrigCounter_ = 0;       ///< Countdown to next sub-trigger
    float retrigBaseVelocity_ = 0;  ///< Base velocity for decay computation
    int   retrigDecayMode_ = 0;     ///< 0=flat, 1=ramp-up, 2=ramp-down
    int   retrigIndex_ = 0;         ///< Current sub-trigger index
    int   retrigTotal_ = 0;         ///< Total sub-triggers in this step

    // --- PRNG state ---
    uint32_t rngState_ = 0x12345678; ///< xorshift32 state (non-zero seed)
};

} // namespace drums

#endif // GUITAR_ENGINE_DRUM_TRACK_H
