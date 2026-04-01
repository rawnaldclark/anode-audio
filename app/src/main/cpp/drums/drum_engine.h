#ifndef GUITAR_ENGINE_DRUM_ENGINE_H
#define GUITAR_ENGINE_DRUM_ENGINE_H

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>

#include "drum_types.h"
#include "sequencer_clock.h"
#include "drum_track.h"
#include "../effects/fast_math.h"
#include "drum_bus.h"
#include "voices/fm_voice.h"
#include "voices/subtractive_voice.h"
#include "voices/metallic_voice.h"
#include "voices/noise_voice.h"
#include "voices/physical_voice.h"
#include "voices/multilayer_voice.h"

/**
 * Top-level drum machine engine.
 *
 * Owns the SequencerClock, 8 DrumTracks, and a double-buffered Pattern
 * for lock-free pattern swapping. Renders stereo audio blocks that are
 * summed with the guitar signal in AudioEngine::onAudioReady().
 *
 * Architecture:
 *   DrumEngine is a peer of SignalChain, NOT inside it. Drums are mixed
 *   AFTER the looper so that loop recordings contain only guitar audio.
 *   The engine outputs stereo (L/R) because per-voice pan is fundamental
 *   to drum mixing.
 *
 * renderBlock() flow:
 *   1. Clear outL/outR to zero.
 *   2. If not playing and no active voices, return early.
 *   3. For each sample: advance clock, check for step triggers.
 *   4. When a step triggers: evaluate choke groups, trigger affected tracks.
 *   5. Each track processes its block (voice + drive + filter + pan).
 *   6. [Future: DrumBus processes the summed stereo output.]
 *
 * Thread safety:
 *   - Pattern data is double-buffered with atomic pointer swap.
 *   - Transport controls (play, tempo, swing) use atomic fields.
 *   - Tap tempo uses a rolling window with median calculation.
 *   - renderBlock() is called exclusively from the audio thread.
 *   - All public setters are safe to call from the UI thread.
 *
 * All audio-thread code is real-time safe: no allocations, no locks,
 * no system calls, no logging.
 */

namespace drums {

/** Maximum number of drum tracks. */
static constexpr int kMaxTracks = 8;

/** Maximum number of tap tempo samples in the rolling window. */
static constexpr int kMaxTapSamples = 8;

/** Tap tempo timeout in nanoseconds (2 seconds). */
static constexpr int64_t kTapTimeoutNs = 2000000000LL;

class DrumEngine {
public:
    DrumEngine() {
        patternA_.clear();
        patternB_.clear();
        activePattern_.store(&patternA_, std::memory_order_relaxed);
        std::memset(scratchL_, 0, sizeof(scratchL_));
        std::memset(scratchR_, 0, sizeof(scratchR_));

        // Create default voices for all 8 tracks so they can produce sound
        // immediately. Without this, tracks have null voice pointers and
        // processBlock() / triggerStep() / triggerImmediate() all return silence.
        createDefaultVoices();
    }

    ~DrumEngine() = default;

    // Non-copyable, non-movable
    DrumEngine(const DrumEngine&) = delete;
    DrumEngine& operator=(const DrumEngine&) = delete;

    // -----------------------------------------------------------------------
    // Initialization
    // -----------------------------------------------------------------------

    /**
     * Set the audio sample rate. Must be called before renderBlock().
     * Propagates to the clock and all tracks.
     *
     * @param sr Device native sample rate in Hz.
     */
    void setSampleRate(int32_t sr) {
        sampleRate_ = sr;
        clock_.setSampleRate(sr);
        for (int i = 0; i < kMaxTracks; ++i) {
            tracks_[i].setSampleRate(sr);
        }
        drumBus_.setSampleRate(sr);
    }

    /**
     * Set the voice engine type for a specific track.
     *
     * Creates a new DrumVoice instance matching the requested engine type
     * and assigns it to the track. The new voice is initialized with the
     * current sample rate.
     *
     * Must only be called when the audio engine is stopped or from a safe
     * context (e.g., during pattern load before playback starts).
     *
     * @param trackIndex Track index (0-7).
     * @param type       Engine type (0=FM, 1=Subtractive, 2=Metallic, 3=Noise, 4=Physical, 5=MultiLayer).
     */
    void setTrackEngineType(int trackIndex, int type) {
        if (trackIndex < 0 || trackIndex >= kMaxTracks) return;

        auto voice = createVoiceForType(static_cast<EngineType>(type));
        if (voice && sampleRate_ > 0) {
            voice->setSampleRate(sampleRate_);
        }
        tracks_[trackIndex].setVoice(std::move(voice));
    }

    /** Reset all engine state (clock, tracks, transport). */
    void reset() {
        clock_.reset();
        for (int i = 0; i < kMaxTracks; ++i) {
            tracks_[i].reset();
        }
        drumBus_.reset();
        playing_.store(false, std::memory_order_relaxed);
        fillActive_.store(false, std::memory_order_relaxed);
    }

    // -----------------------------------------------------------------------
    // Transport controls (UI thread)
    // -----------------------------------------------------------------------

    /** Start or stop playback. */
    void setPlaying(bool play) {
        if (play && !playing_.load(std::memory_order_relaxed)) {
            clock_.reset();
        }
        playing_.store(play, std::memory_order_release);
    }

    /** Check if the sequencer is currently playing. Thread-safe. */
    bool isPlaying() const {
        return playing_.load(std::memory_order_acquire);
    }

    /**
     * Set the tempo in BPM. Thread-safe.
     * @param bpm Tempo 20-300 BPM.
     */
    void setTempo(float bpm) {
        clock_.setTempo(bpm);
    }

    /** Get the current tempo. Thread-safe. */
    float getTempo() const {
        return clock_.getTempo();
    }

    /**
     * Set swing percentage. Thread-safe.
     * @param percent 50 (straight) to 75 (heavy shuffle).
     */
    void setSwing(float percent) {
        clock_.setSwing(percent);
    }

    /** Get the current swing percentage. Thread-safe. */
    float getSwing() const {
        return clock_.getSwing();
    }

    /**
     * Set tempo multiplier (half-time / double-time).
     * @param mult 0.5 = half-time, 1.0 = normal, 2.0 = double-time.
     */
    void setTempoMultiplier(float mult) {
        clock_.setTempoMultiplier(mult);
    }

    /** Get the current step position for UI display. */
    int getCurrentStep() const {
        return clock_.getCurrentStep();
    }

    /** Activate or deactivate fill mode. */
    void setFillActive(bool active) {
        fillActive_.store(active, std::memory_order_relaxed);
    }

    /** Check if fill mode is active. */
    bool isFillActive() const {
        return fillActive_.load(std::memory_order_relaxed);
    }

    // -----------------------------------------------------------------------
    // Pattern management (UI thread)
    // -----------------------------------------------------------------------

    /**
     * Load a new pattern via double-buffered atomic swap.
     *
     * Copies the pattern data into the inactive buffer, then atomically
     * swaps the active pointer. The audio thread will pick up the new
     * pattern on the next read with no lock contention.
     *
     * Also applies the pattern's track configuration to the DrumTrack
     * instances (volume, pan, mute, choke group). Voice engine type
     * changes require separate setTrackVoice() calls since they involve
     * allocation.
     *
     * @param pattern The new pattern to load.
     */
    void setPattern(const Pattern& pattern) {
        Pattern* active = activePattern_.load(std::memory_order_acquire);
        Pattern* inactive = (active == &patternA_) ? &patternB_ : &patternA_;

        // Copy pattern data into inactive buffer
        std::memcpy(inactive, &pattern, sizeof(Pattern));

        // Apply track mix parameters
        for (int i = 0; i < kMaxTracks; ++i) {
            tracks_[i].setVolume(pattern.tracks[i].volume);
            tracks_[i].setPan(pattern.tracks[i].pan);
            tracks_[i].setMuted(pattern.tracks[i].muted);
            tracks_[i].setChokeGroup(pattern.tracks[i].chokeGroup);
        }

        // Atomic pointer swap (release ensures all writes above are visible)
        activePattern_.store(inactive, std::memory_order_release);

        // Apply tempo and swing from pattern
        clock_.setTempo(pattern.bpm);
        clock_.setSwing(pattern.swing);
    }

    /**
     * Queue a pattern index to switch on the next downbeat.
     * The actual switch happens inside renderBlock() when the clock
     * reaches step 0. Set to -1 to cancel the queue.
     *
     * @param patternIndex Index of the pattern to queue, or -1 to cancel.
     */
    void queuePattern(int patternIndex) {
        queuedPattern_.store(patternIndex, std::memory_order_relaxed);
    }

    /** Get the queued pattern index (-1 if none). */
    int getQueuedPattern() const {
        return queuedPattern_.load(std::memory_order_relaxed);
    }

    // -----------------------------------------------------------------------
    // Track controls (UI thread)
    // -----------------------------------------------------------------------

    /** Get a mutable reference to a track (for setting voice, params, etc.). */
    DrumTrack& getTrack(int index) {
        return tracks_[index & (kMaxTracks - 1)];
    }

    /** Get a const reference to a track. */
    const DrumTrack& getTrack(int index) const {
        return tracks_[index & (kMaxTracks - 1)];
    }

    /**
     * Trigger a drum pad from the UI (finger drumming).
     * Sets an atomic trigger command that the audio thread consumes
     * on the next renderBlock() call.
     *
     * @param trackIndex Track to trigger (0-7).
     * @param velocity   Velocity in [0.0, 1.0].
     */
    void triggerPad(int trackIndex, float velocity) {
        if (trackIndex < 0 || trackIndex >= kMaxTracks) return;

        // Pack track index and velocity into a single 64-bit atomic.
        // High 32 bits: track index, low 32 bits: velocity as uint32_t bit pattern.
        // Value of 0 means "no pending trigger" (track -1 is invalid).
        uint32_t velBits;
        std::memcpy(&velBits, &velocity, sizeof(uint32_t));
        int64_t packed = (static_cast<int64_t>(trackIndex + 1) << 32) | velBits;
        padTrigger_.store(packed, std::memory_order_release);
    }

    // -----------------------------------------------------------------------
    // Tap tempo (UI thread)
    // -----------------------------------------------------------------------

    /**
     * Record a tap for tap tempo calculation.
     *
     * Uses a rolling window of up to 8 taps. If more than 2 seconds
     * elapse between taps, the window resets. The tempo is calculated
     * from the median inter-tap interval (rejects outliers).
     *
     * @param timestampNs Current timestamp in nanoseconds (monotonic clock).
     */
    void tapTempo(int64_t timestampNs) {
        // Reset if timeout exceeded
        if (tapCount_ > 0 && (timestampNs - tapTimestamps_[tapCount_ - 1]) > kTapTimeoutNs) {
            tapCount_ = 0;
        }

        // Add tap to window
        if (tapCount_ < kMaxTapSamples) {
            tapTimestamps_[tapCount_] = timestampNs;
            tapCount_++;
        } else {
            // Shift window: drop oldest, add newest
            for (int i = 0; i < kMaxTapSamples - 1; ++i) {
                tapTimestamps_[i] = tapTimestamps_[i + 1];
            }
            tapTimestamps_[kMaxTapSamples - 1] = timestampNs;
        }

        // Need at least 2 taps to calculate tempo
        if (tapCount_ < 2) return;

        // Calculate inter-tap intervals
        int numIntervals = tapCount_ - 1;
        int64_t intervals[kMaxTapSamples - 1];
        for (int i = 0; i < numIntervals; ++i) {
            intervals[i] = tapTimestamps_[i + 1] - tapTimestamps_[i];
        }

        // Find median interval (insertion sort for small N)
        for (int i = 1; i < numIntervals; ++i) {
            int64_t key = intervals[i];
            int j = i - 1;
            while (j >= 0 && intervals[j] > key) {
                intervals[j + 1] = intervals[j];
                j--;
            }
            intervals[j + 1] = key;
        }
        int64_t medianNs = intervals[numIntervals / 2];

        // Convert median interval to BPM: BPM = 60e9 / medianNs
        if (medianNs > 0) {
            float bpm = static_cast<float>(60000000000.0 / static_cast<double>(medianNs));
            bpm = (bpm < 20.0f) ? 20.0f : (bpm > 300.0f) ? 300.0f : bpm;
            clock_.setTempo(bpm);
        }
    }

    // -----------------------------------------------------------------------
    // Audio rendering (audio thread only)
    // -----------------------------------------------------------------------

    /**
     * Render one block of stereo drum audio.
     *
     * Called from AudioEngine::onAudioReady() after the looper.
     * The output buffers are filled with the summed stereo drum mix.
     * The caller is responsible for mixing these into the final output.
     *
     * @param outL      Left channel output buffer (filled, not summed into).
     * @param outR      Right channel output buffer (filled, not summed into).
     * @param numFrames Number of audio frames to render.
     */
    void renderBlock(float* outL, float* outR, int numFrames) {
        // Clear output buffers
        std::memset(outL, 0, numFrames * sizeof(float));
        std::memset(outR, 0, numFrames * sizeof(float));

        // Read active pattern pointer (acquire pairs with release in setPattern)
        const Pattern* pattern = activePattern_.load(std::memory_order_acquire);
        const bool isPlaying = playing_.load(std::memory_order_acquire);

        // Consume any pending pad trigger
        int64_t padCmd = padTrigger_.exchange(0, std::memory_order_acquire);
        if (padCmd != 0) {
            int trackIdx = static_cast<int>((padCmd >> 32) & 0xFFFFFFFF) - 1;
            uint32_t velBits = static_cast<uint32_t>(padCmd & 0xFFFFFFFF);
            float velocity;
            std::memcpy(&velocity, &velBits, sizeof(float));

            if (trackIdx >= 0 && trackIdx < kMaxTracks) {
                // Evaluate choke groups for pad trigger
                int8_t chokeGroup = tracks_[trackIdx].getChokeGroup();
                if (chokeGroup >= 0) {
                    for (int j = 0; j < kMaxTracks; ++j) {
                        if (j != trackIdx && tracks_[j].getChokeGroup() == chokeGroup) {
                            tracks_[j].choke();
                        }
                    }
                }
                tracks_[trackIdx].triggerImmediate(velocity);
            }
        }

        // Early out if not playing and no voices are still ringing
        if (!isPlaying && !hasActiveVoices()) {
            return;
        }

        // Process sample-by-sample for sequencer timing, then block for tracks
        if (isPlaying) {
            int patternLength = pattern->length;
            if (patternLength < 1) patternLength = 1;
            if (patternLength > 64) patternLength = 64;

            // Read samplesPerStep for retrig spacing
            // (approximate: use current tempo, not accounting for swing on this step)
            double sps = (static_cast<double>(sampleRate_) * 60.0)
                       / (static_cast<double>(clock_.getTempo())
                          * static_cast<double>(clock_.getTempoMultiplier()) * 4.0);

            for (int i = 0; i < numFrames; ++i) {
                int stepTriggered = clock_.advanceAndCheck(patternLength);

                if (stepTriggered >= 0) {
                    // Check for pattern queue on downbeat
                    if (stepTriggered == 0) {
                        int queued = queuedPattern_.load(std::memory_order_relaxed);
                        if (queued >= 0) {
                            // Pattern switch is handled externally via callback
                            // For now, just clear the queue flag
                            queuedPattern_.store(-1, std::memory_order_relaxed);
                            onDownbeat_ = true;
                        }
                    }

                    // Trigger all tracks for this step
                    const bool fillMode = fillActive_.load(std::memory_order_relaxed);

                    for (int t = 0; t < kMaxTracks; ++t) {
                        int trackStep = stepTriggered % pattern->tracks[t].length;
                        const Step& step = pattern->tracks[t].steps[trackStep];

                        // Skip if step doesn't pass fill-mode check
                        if (step.condition == TrigCondition::FILL && !fillMode) continue;
                        if (step.condition == TrigCondition::NOT_FILL && fillMode) continue;

                        if (step.trigger) {
                            // Evaluate choke groups before triggering
                            int8_t chokeGroup = tracks_[t].getChokeGroup();
                            if (chokeGroup >= 0) {
                                for (int j = 0; j < kMaxTracks; ++j) {
                                    if (j != t && tracks_[j].getChokeGroup() == chokeGroup) {
                                        tracks_[j].choke();
                                    }
                                }
                            }
                            tracks_[t].triggerStep(step, sps);
                        }
                    }
                }
            }
        }

        // Process each track's audio block (tracks sum into outL/outR)
        for (int t = 0; t < kMaxTracks; ++t) {
            tracks_[t].processBlock(outL, outR, numFrames);
        }

        // Drum bus: compressor + EQ + reverb on the summed stereo mix
        drumBus_.processStereo(outL, outR, numFrames);
    }

    /** Get a mutable reference to the drum bus for parameter control. */
    DrumBus& getDrumBus() { return drumBus_; }

    // -----------------------------------------------------------------------
    // Step editing (UI thread -- copies active pattern, modifies, swaps back)
    // -----------------------------------------------------------------------

    /**
     * Set a step's trigger and velocity on the active pattern.
     * Copies the active pattern, modifies the step, and atomically swaps.
     *
     * @param track   Track index (0-7).
     * @param step    Step index (0-63).
     * @param velocity Velocity 0-127 (0 = step off, >0 = step on).
     */
    void setStep(int track, int step, int velocity) {
        if (track < 0 || track >= kMaxTracks) return;
        if (step < 0 || step >= 64) return;

        Pattern copy;
        const Pattern* active = activePattern_.load(std::memory_order_acquire);
        std::memcpy(&copy, active, sizeof(Pattern));

        copy.tracks[track].steps[step].trigger = (velocity > 0);
        copy.tracks[track].steps[step].velocity = static_cast<uint8_t>(
            (velocity < 0) ? 0 : (velocity > 127) ? 127 : velocity);

        setPattern(copy);
    }

    /**
     * Set a step's probability on the active pattern.
     *
     * @param track   Track index (0-7).
     * @param step    Step index (0-63).
     * @param percent Probability 1-100.
     */
    void setStepProbability(int track, int step, int percent) {
        if (track < 0 || track >= kMaxTracks) return;
        if (step < 0 || step >= 64) return;

        Pattern copy;
        const Pattern* active = activePattern_.load(std::memory_order_acquire);
        std::memcpy(&copy, active, sizeof(Pattern));

        copy.tracks[track].steps[step].probability = static_cast<uint8_t>(
            (percent < 1) ? 1 : (percent > 100) ? 100 : percent);

        setPattern(copy);
    }

    /**
     * Set a step's retrig count on the active pattern.
     *
     * @param track Track index (0-7).
     * @param step  Step index (0-63).
     * @param count Retrig count (0=off, 2, 3, 4, 6, 8).
     */
    void setStepRetrig(int track, int step, int count) {
        if (track < 0 || track >= kMaxTracks) return;
        if (step < 0 || step >= 64) return;

        Pattern copy;
        const Pattern* active = activePattern_.load(std::memory_order_acquire);
        std::memcpy(&copy, active, sizeof(Pattern));

        copy.tracks[track].steps[step].retrigCount = static_cast<uint8_t>(
            (count < 0) ? 0 : (count > 8) ? 8 : count);

        setPattern(copy);
    }

    /**
     * Set the global pattern length on the active pattern.
     *
     * @param length Pattern length in steps (1-64).
     */
    void setPatternLength(int length) {
        if (length < 1) length = 1;
        if (length > 64) length = 64;

        Pattern copy;
        const Pattern* active = activePattern_.load(std::memory_order_acquire);
        std::memcpy(&copy, active, sizeof(Pattern));

        copy.length = static_cast<uint8_t>(length);

        setPattern(copy);
    }

    /**
     * Get a copy of the active pattern for serialization.
     * NOT real-time safe -- called from UI thread only.
     */
    Pattern getPatternCopy() const {
        Pattern copy;
        const Pattern* active = activePattern_.load(std::memory_order_acquire);
        std::memcpy(&copy, active, sizeof(Pattern));
        return copy;
    }

    /**
     * Check if any track has an active (audible) voice.
     * Used to continue rendering drum tails after transport stop,
     * and for AudioEngine to know if it can skip drum mixing.
     *
     * @return true if any voice is currently producing audio.
     */
    bool hasActiveVoices() const {
        for (int i = 0; i < kMaxTracks; ++i) {
            if (tracks_[i].isActive()) return true;
        }
        return false;
    }

private:
    // --- Drum bus (compressor + EQ + reverb on stereo drum sum) ---
    DrumBus drumBus_;

    // --- Clock ---
    SequencerClock clock_;

    // --- Tracks ---
    DrumTrack tracks_[kMaxTracks];

    // --- Pattern double buffer ---
    Pattern patternA_;
    Pattern patternB_;
    std::atomic<Pattern*> activePattern_{nullptr};

    // --- Transport state ---
    std::atomic<bool>  playing_{false};
    std::atomic<bool>  fillActive_{false};
    std::atomic<int>   queuedPattern_{-1};

    // --- Pad trigger command slot (atomic exchange for consume-once) ---
    std::atomic<int64_t> padTrigger_{0};

    // --- Scratch buffers for internal processing ---
    static constexpr int kMaxBlockSize = 1024;
    float scratchL_[kMaxBlockSize];
    float scratchR_[kMaxBlockSize];

    // --- Tap tempo state (UI thread only, not accessed from audio thread) ---
    int64_t tapTimestamps_[kMaxTapSamples] = {};
    int     tapCount_ = 0;

    // --- Misc state ---
    int32_t sampleRate_ = 48000;
    bool    onDownbeat_ = false;

    /**
     * Create default voices for all 8 tracks with distinct parameters.
     *
     * Track assignment follows standard drum machine convention:
     *   0: FM (kick)                4: Subtractive (clap)
     *   1: Subtractive (snare)      5: Physical/KS (rim)
     *   2: Metallic (closed hi-hat) 6: Noise (shaker)
     *   3: Metallic (open hi-hat)   7: Physical/Modal (cowbell)
     *
     * Each voice has its parameters explicitly configured to produce a
     * distinct, genre-appropriate sound. Without this, most voices would
     * default to similar timbres (e.g., all FM voices sound like the
     * same 80Hz kick).
     *
     * Called from the constructor so tracks are never left with null voices.
     */
    void createDefaultVoices() {
        // ----------------------------------------------------------
        // Track 0: KICK (FM Voice — 808 reference)
        //
        // The TR-808 kick uses a bridged-T bandpass filter at ~49Hz (G1)
        // with a 6ms pitch sweep from ~300Hz down to 49Hz for the
        // characteristic attack click. Decay range 50-800ms; we use
        // 400ms for a medium boom that sits well in a mix.
        //
        // FM params: 0=freq(40-400Hz), 1=modRatio(0.5-8), 2=modDepth(0-10),
        //   3=pitchEnvDepth(0-8 oct), 4=pitchDecay(5-500ms),
        //   5=ampDecay(20-2000ms), 6=filterCutoff(100-20kHz),
        //   7=filterRes(0-1), 8=drive(0-5)
        // ----------------------------------------------------------
        {
            auto v = std::make_unique<FmVoice>();
            v->setParam(0, 49.0f);    // freq: 808 resonant fundamental (G1)
            v->setParam(1, 1.0f);     // modRatio: self-modulation for harmonic richness
            v->setParam(2, 1.5f);     // modDepth: subtle FM adds body, no metallic overtones
            v->setParam(3, 3.0f);     // pitchEnvDepth: ~3 octaves sweep (300Hz -> 49Hz)
            v->setParam(4, 6.0f);     // pitchDecay: 6ms snap (808 bridged-T attack)
            v->setParam(5, 400.0f);   // ampDecay: 400ms medium 808 boom
            v->setParam(6, 3000.0f);  // filterCutoff: let the click transient through
            v->setParam(7, 0.1f);     // filterRes: minimal resonance
            v->setParam(8, 1.0f);     // drive: slight saturation for analog warmth
            tracks_[0].setVoice(std::move(v));
        }

        // ----------------------------------------------------------
        // Track 1: SNARE (Subtractive Voice — 909 reference)
        //
        // The TR-909 snare has a 200Hz sine body + broadband noise at
        // ~60% mix. Body decays in ~80ms, noise tail in ~200ms. Filter
        // at 5kHz gives the characteristic brightness without harshness.
        //
        // Subtractive params: 0=toneFreq(80-800Hz), 1=toneDecay(10-500ms),
        //   2=noiseDecay(10-500ms), 3=noiseMix(0-1), 4=filterFreq(200-16kHz),
        //   5=filterRes(0-1), 6=filterDecay(5-500ms),
        //   7=filterEnvDepth(0-8 oct), 8=drive(0-5)
        // ----------------------------------------------------------
        {
            auto v = std::make_unique<SubtractiveVoice>();
            v->setParam(0, 200.0f);   // toneFreq: 909 snare body fundamental
            v->setParam(1, 80.0f);    // toneDecay: 80ms body thump
            v->setParam(2, 200.0f);   // noiseDecay: 200ms noise rattle tail
            v->setParam(3, 0.6f);     // noiseMix: 60% noise (909 character)
            v->setParam(4, 5000.0f);  // filterFreq: 5kHz bright snare
            v->setParam(5, 0.3f);     // filterRes: moderate presence peak
            v->setParam(6, 60.0f);    // filterDecay: 60ms filter snap
            v->setParam(7, 3.0f);     // filterEnvDepth: 3 octaves for snappy attack
            v->setParam(8, 0.8f);     // drive: 909-style mild grit
            tracks_[1].setVoice(std::move(v));
        }

        // ----------------------------------------------------------
        // Track 2: CLOSED HI-HAT (Metallic Voice — 808 reference)
        //
        // 808 closed hat: 6 detuned square oscillators through BP
        // filters + HP at 8kHz. Very short 40ms decay for tight tick.
        // Subtle detuning (color=0.2) from nominal 808 frequencies.
        //
        // Metallic params: 0=color(0-1), 1=tone(0-1), 2=decay(5-2000ms),
        //   3=hpCutoff(2000-12000Hz)
        // ----------------------------------------------------------
        {
            auto v = std::make_unique<MetallicVoice>();
            v->setParam(0, 0.2f);     // color: subtle 808-style detuning
            v->setParam(1, 0.7f);     // tone: favor BP2 (7100Hz) for brightness
            v->setParam(2, 40.0f);    // decay: 40ms tight closed hat
            v->setParam(3, 8000.0f);  // hpCutoff: 8kHz removes all low-end
            tracks_[2].setVoice(std::move(v));
            tracks_[2].setChokeGroup(0);  // Hat choke group
        }

        // ----------------------------------------------------------
        // Track 3: OPEN HI-HAT (Metallic Voice — 808 reference)
        //
        // Same 808 metallic character as closed hat but with 300ms
        // decay for sustained shimmer. Slightly lower HP (6kHz)
        // allows more body. Choke group 0 pairs with closed hat.
        //
        // Metallic params: 0=color(0-1), 1=tone(0-1), 2=decay(5-2000ms),
        //   3=hpCutoff(2000-12000Hz)
        // ----------------------------------------------------------
        {
            auto v = std::make_unique<MetallicVoice>();
            v->setParam(0, 0.2f);     // color: match closed hat detuning
            v->setParam(1, 0.6f);     // tone: slightly darker than closed
            v->setParam(2, 300.0f);   // decay: 300ms open hat sustain
            v->setParam(3, 6000.0f);  // hpCutoff: 6kHz allows more low-mid body
            tracks_[3].setVoice(std::move(v));
            tracks_[3].setChokeGroup(0);  // Same choke group: closed hat kills open
        }

        // ----------------------------------------------------------
        // Track 4: CLAP (Subtractive Voice — 909 reference)
        //
        // 909 clap: multiple noise bursts, heavy noise mix (85%),
        // bandpass-ish filter at 3.5kHz with moderate resonance.
        // Very short body (15ms), longer noise tail (180ms).
        //
        // Subtractive params: 0=toneFreq(80-800Hz), 1=toneDecay(10-500ms),
        //   2=noiseDecay(10-500ms), 3=noiseMix(0-1), 4=filterFreq(200-16kHz),
        //   5=filterRes(0-1), 6=filterDecay(5-500ms),
        //   7=filterEnvDepth(0-8 oct), 8=drive(0-5)
        // ----------------------------------------------------------
        {
            auto v = std::make_unique<SubtractiveVoice>();
            v->setParam(0, 150.0f);   // toneFreq: low body thump
            v->setParam(1, 15.0f);    // toneDecay: 15ms very short body
            v->setParam(2, 180.0f);   // noiseDecay: 180ms noise tail
            v->setParam(3, 0.85f);    // noiseMix: 85% noise (almost all noise)
            v->setParam(4, 3500.0f);  // filterFreq: 3.5kHz bandpass-ish focus
            v->setParam(5, 0.5f);     // filterRes: moderate resonance for character
            v->setParam(6, 100.0f);   // filterDecay: 100ms filter envelope
            v->setParam(7, 2.0f);     // filterEnvDepth: 2 octaves sweep
            v->setParam(8, 1.2f);     // drive: slight crunch for 909 edge
            tracks_[4].setVoice(std::move(v));
        }

        // ----------------------------------------------------------
        // Track 5: RIMSHOT (Physical Voice — Karplus-Strong)
        //
        // Short, sharp attack with high pitch and metallic ring.
        // MIDI 76 (~659Hz, high E) for the characteristic rim crack.
        // Very short decay (0.15) with bright attack and quick
        // high-frequency damping.
        //
        // Physical params: 0=modelType(0/1), 1=pitch(20-120 MIDI),
        //   2=decay(0-1), 3=brightness(0-1), 4=material(0-1),
        //   5=damping(0-1)
        // ----------------------------------------------------------
        {
            auto v = std::make_unique<PhysicalVoice>();
            v->setParam(0, 0.0f);     // modelType: Karplus-Strong
            v->setParam(1, 76.0f);    // pitch: MIDI 76 (~659Hz, high E)
            v->setParam(2, 0.15f);    // decay: very short ring
            v->setParam(3, 0.9f);     // brightness: bright attack transient
            v->setParam(4, 0.1f);     // material: mostly tonal (string-like)
            v->setParam(5, 0.7f);     // damping: quick high-frequency decay
            tracks_[5].setVoice(std::move(v));
        }

        // ----------------------------------------------------------
        // Track 6: SHAKER (Noise Voice)
        //
        // Organic-sounding shaker: pink noise through LP filter at
        // 6kHz with short 60ms decay. Moderate filter sweep (0.4)
        // creates the characteristic "shh" attack.
        //
        // Noise params: 0=noiseType(0/1), 1=filterMode(0-2),
        //   2=filterFreq(100-16kHz), 3=filterRes(0-1),
        //   4=decay(5-2000ms), 5=sweep(0-1)
        // ----------------------------------------------------------
        {
            auto v = std::make_unique<NoiseVoice>();
            v->setParam(0, 1.0f);     // noiseType: pink noise (organic)
            v->setParam(1, 0.0f);     // filterMode: lowpass
            v->setParam(2, 6000.0f);  // filterFreq: 6kHz natural shaker range
            v->setParam(3, 0.15f);    // filterRes: minimal resonance
            v->setParam(4, 60.0f);    // decay: 60ms short shaker rattle
            v->setParam(5, 0.4f);     // sweep: moderate filter sweep for "shh" attack
            tracks_[6].setVoice(std::move(v));
        }

        // ----------------------------------------------------------
        // Track 7: COWBELL (Physical Voice — Modal, 808 reference)
        //
        // 808 cowbell uses two tones at 587Hz and 845Hz (ratio 1:1.44).
        // MIDI 74 (~587Hz) matches the 808's fundamental. Modal synthesis
        // with Bessel ratios approximates the dual-tone character. Pure
        // tonal (material=0), moderate decay, moderate HF damping.
        //
        // Physical params: 0=modelType(0/1), 1=pitch(20-120 MIDI),
        //   2=decay(0-1), 3=brightness(0-1), 4=material(0-1),
        //   5=damping(0-1)
        // ----------------------------------------------------------
        {
            auto v = std::make_unique<PhysicalVoice>();
            v->setParam(0, 1.0f);     // modelType: Modal synthesis
            v->setParam(1, 74.0f);    // pitch: MIDI 74 (~587Hz, 808 cowbell fundamental)
            v->setParam(2, 0.3f);     // decay: moderate ring
            v->setParam(3, 0.7f);     // brightness: bright metallic character
            v->setParam(4, 0.0f);     // material: pure tonal (no noise)
            v->setParam(5, 0.4f);     // damping: moderate high-frequency decay
            tracks_[7].setVoice(std::move(v));
        }
    }

    /**
     * Factory function to create a DrumVoice by engine type.
     *
     * @param type The engine type enum value.
     * @return New voice instance, or FM voice as fallback for unknown types.
     */
    static std::unique_ptr<DrumVoice> createVoiceForType(EngineType type) {
        switch (type) {
            case EngineType::FM:
                return std::make_unique<FmVoice>();
            case EngineType::SUBTRACTIVE:
                return std::make_unique<SubtractiveVoice>();
            case EngineType::METALLIC:
                return std::make_unique<MetallicVoice>();
            case EngineType::NOISE:
                return std::make_unique<NoiseVoice>();
            case EngineType::PHYSICAL:
                return std::make_unique<PhysicalVoice>();
            case EngineType::MULTI_LAYER:
                return std::make_unique<MultiLayerVoice>();
            default:
                return std::make_unique<FmVoice>(); // safe fallback
        }
    }
};

} // namespace drums

#endif // GUITAR_ENGINE_DRUM_ENGINE_H
