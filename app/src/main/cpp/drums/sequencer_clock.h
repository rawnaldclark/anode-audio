#ifndef GUITAR_ENGINE_SEQUENCER_CLOCK_H
#define GUITAR_ENGINE_SEQUENCER_CLOCK_H

#include <atomic>
#include <cmath>
#include <cstdint>

/**
 * Sample-accurate sequencer clock for the drum machine.
 *
 * Generates step triggers at precise sample offsets within audio buffers,
 * providing sub-millisecond timing accuracy (~0.02ms at 48kHz). The clock
 * uses a double-precision accumulator that is subtracted (never reset) to
 * preserve fractional remainders and prevent long-term drift.
 *
 * Timing model:
 *   samplesPerStep = (sampleRate * 60.0) / (bpm * 4.0)
 *   This gives 16th note resolution (4 steps per beat).
 *
 * Swing implementation (MPC-style):
 *   Even-numbered steps (0-indexed: 1, 3, 5, ...) are delayed by:
 *     offset = (swing - 50.0) / 50.0 * samplesPerStep
 *   50% = straight, 54-57% = house, 66% = triplet, 70%+ = jazz
 *
 * Thread safety:
 *   - Tempo, swing, and tempoMultiplier are set from the UI thread via atomics.
 *   - advanceAndCheck() is called exclusively from the audio thread.
 *   - All atomic loads in the audio path use relaxed ordering (eventual
 *     consistency is fine for musical parameters -- a 1-buffer delay
 *     in tempo change is imperceptible).
 */
namespace drums {

class SequencerClock {
public:
    SequencerClock() = default;

    /**
     * Set the audio sample rate. Must be called before any processing.
     * Recalculates samplesPerStep from the current BPM.
     *
     * @param sr Device native sample rate in Hz (e.g. 48000).
     */
    void setSampleRate(int32_t sr) {
        sampleRate_ = static_cast<double>(sr);
        recalcSamplesPerStep();
    }

    /**
     * Set the sequencer tempo in beats per minute.
     * Thread-safe (UI thread writer, audio thread reader).
     *
     * @param bpm Tempo in BPM, clamped to [20, 300].
     */
    void setTempo(float bpm) {
        bpm = (bpm < 20.0f) ? 20.0f : (bpm > 300.0f) ? 300.0f : bpm;
        bpm_.store(bpm, std::memory_order_relaxed);
        recalcSamplesPerStep();
    }

    /** Get the current tempo. Thread-safe. */
    float getTempo() const {
        return bpm_.load(std::memory_order_relaxed);
    }

    /**
     * Set swing amount (MPC-style even-step delay).
     *
     * @param percent Swing percentage, clamped to [50, 75].
     *                50 = straight, 66 = triplet shuffle.
     */
    void setSwing(float percent) {
        percent = (percent < 50.0f) ? 50.0f : (percent > 75.0f) ? 75.0f : percent;
        swing_.store(percent, std::memory_order_relaxed);
    }

    /** Get the current swing percentage. Thread-safe. */
    float getSwing() const {
        return swing_.load(std::memory_order_relaxed);
    }

    /**
     * Set tempo multiplier for half-time / double-time modes.
     *
     * @param mult 0.5 = half-time, 1.0 = normal, 2.0 = double-time.
     */
    void setTempoMultiplier(float mult) {
        mult = (mult < 0.25f) ? 0.25f : (mult > 4.0f) ? 4.0f : mult;
        tempoMultiplier_.store(mult, std::memory_order_relaxed);
        recalcSamplesPerStep();
    }

    /** Get the current tempo multiplier. Thread-safe. */
    float getTempoMultiplier() const {
        return tempoMultiplier_.load(std::memory_order_relaxed);
    }

    /**
     * Advance the clock by one sample and check for a step trigger.
     *
     * Called once per sample from the audio thread's render loop.
     * Returns the step number (0-based) if a step trigger occurs at
     * this sample, or -1 if no trigger. The step number wraps according
     * to the pattern length passed in.
     *
     * The accumulator uses subtraction (not reset) to preserve the
     * fractional remainder and prevent long-term drift.
     *
     * @param patternLength Total number of steps in the current pattern (1-64).
     * @return Step index [0, patternLength-1] if trigger fires, -1 otherwise.
     */
    int advanceAndCheck(int patternLength) {
        // Read current timing parameters (relaxed: eventual consistency OK)
        double sps = samplesPerStep_.load(std::memory_order_relaxed);
        float swingPct = swing_.load(std::memory_order_relaxed);

        // Calculate the threshold for this step, applying swing to even steps
        // (0-indexed odd steps = the "and" of each beat = swing targets)
        double threshold = sps;
        bool isSwungStep = (currentStep_ & 1) != 0;
        if (isSwungStep && swingPct > 50.0f) {
            double swingOffset = static_cast<double>(swingPct - 50.0f) / 50.0 * sps;
            threshold += swingOffset;
        }

        // Advance accumulator
        accumulator_ += 1.0;

        // Check if we've reached the next step threshold
        if (accumulator_ >= threshold) {
            // Subtract threshold (preserve fractional remainder for drift-free timing)
            accumulator_ -= threshold;

            int stepToTrigger = currentStep_;

            // Advance to next step, wrapping at pattern length
            currentStep_++;
            if (currentStep_ >= patternLength) {
                currentStep_ = 0;
                loopCount_++;
            }

            return stepToTrigger;
        }

        return -1;
    }

    /** Get the current step position (for UI display). Thread-safe. */
    int getCurrentStep() const {
        return currentStep_;
    }

    /** Get the number of complete pattern loops since last reset. */
    int getLoopCount() const {
        return loopCount_;
    }

    /** Check if current position is the downbeat (step 0). */
    bool isDownbeat() const {
        return currentStep_ == 0;
    }

    /**
     * Reset the clock to the beginning.
     * Called on transport start or pattern change.
     */
    void reset() {
        accumulator_ = 0.0;
        currentStep_ = 0;
        loopCount_ = 0;
    }

private:
    /**
     * Recalculate samplesPerStep from current BPM, sample rate, and multiplier.
     *
     * Formula: samplesPerStep = (sampleRate * 60.0) / (effectiveBPM * 4.0)
     * where effectiveBPM = bpm * tempoMultiplier.
     * The factor of 4 gives 16th note resolution (4 steps per beat).
     */
    void recalcSamplesPerStep() {
        double bpm = static_cast<double>(bpm_.load(std::memory_order_relaxed));
        double mult = static_cast<double>(tempoMultiplier_.load(std::memory_order_relaxed));
        double effectiveBpm = bpm * mult;
        if (effectiveBpm < 5.0) effectiveBpm = 5.0;  // Safety floor

        double sps = (sampleRate_ * 60.0) / (effectiveBpm * 4.0);
        samplesPerStep_.store(sps, std::memory_order_relaxed);
    }

    // --- Timing state (audio thread only, no atomics needed) ---
    double accumulator_ = 0.0;      ///< Sample accumulator (subtracted, never reset to 0)
    int    currentStep_ = 0;        ///< Current step position in pattern
    int    loopCount_ = 0;          ///< Number of complete pattern loops

    // --- Configuration (set from UI thread, read from audio thread) ---
    double sampleRate_ = 48000.0;   ///< Device sample rate in Hz

    std::atomic<float>  bpm_{120.0f};               ///< Tempo in BPM
    std::atomic<float>  swing_{50.0f};               ///< Swing percentage 50-75
    std::atomic<float>  tempoMultiplier_{1.0f};      ///< 0.5=half, 1.0=normal, 2.0=double
    std::atomic<double> samplesPerStep_{6000.0};     ///< Pre-computed step duration
};

} // namespace drums

#endif // GUITAR_ENGINE_SEQUENCER_CLOCK_H
