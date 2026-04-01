package com.guitaremulator.app.audio

/**
 * JNI bridge to the native C++ drum engine (drums::DrumEngine).
 *
 * All functions route through the global AudioEngine instance in jni_bridge.cpp,
 * accessing the drum engine via gEngine->getDrumEngine(). The native library is
 * already loaded by AudioEngine's companion object, so no System.loadLibrary()
 * is needed here.
 *
 * Thread safety: All native setters use atomic fields in the C++ layer and are
 * safe to call from any thread. Getters return eventually-consistent values
 * (relaxed memory ordering) which is acceptable for UI display.
 *
 * Singleton object because there is exactly one drum engine instance in the
 * native audio engine.
 */
object DrumEngine {

    // =========================================================================
    // Transport controls
    // =========================================================================

    /** Start or stop the drum sequencer. */
    external fun nativeSetPlaying(playing: Boolean)

    /** Check if the sequencer is currently playing. */
    external fun nativeIsPlaying(): Boolean

    /** Set the tempo in BPM (20-300). */
    external fun nativeSetTempo(bpm: Float)

    /** Get the current tempo in BPM. */
    external fun nativeGetTempo(): Float

    /**
     * Record a tap for tap tempo calculation.
     * Uses a rolling window with median interval for outlier rejection.
     * @param timestampNs Monotonic timestamp in nanoseconds (System.nanoTime()).
     */
    external fun nativeTapTempo(timestampNs: Long)

    /** Set swing percentage (50=straight, 66=triplet, 75=heavy). */
    external fun nativeSetSwing(swing: Float)

    /** Get the current swing percentage. */
    external fun nativeGetSwing(): Float

    /**
     * Set the drum mix level relative to guitar signal.
     * @param level Linear gain (0.0=silent, 0.8=default, 1.0+=boosted).
     */
    external fun nativeSetMixLevel(level: Float)

    /**
     * Set fill mode active/inactive.
     * When active, steps with FILL condition trigger; NOT_FILL steps are skipped.
     */
    external fun nativeSetFillActive(active: Boolean)

    /** Get the current step position for UI cursor display (0 to length-1). */
    external fun nativeGetCurrentStep(): Int

    /**
     * Set the tempo multiplier for half-time / double-time modes.
     * @param multiplier 0.5=half-time, 1.0=normal, 2.0=double-time.
     */
    external fun nativeSetTempoMultiplier(multiplier: Float)

    /**
     * Queue a pattern index for switching on the next downbeat.
     * @param patternIndex Index to queue, or -1 to cancel.
     */
    external fun nativeQueuePattern(patternIndex: Int)

    // =========================================================================
    // Track controls
    // =========================================================================

    /** Set a track's mute state. @param track 0-7. */
    external fun nativeSetTrackMute(track: Int, muted: Boolean)

    /** Get a track's mute state. @param track 0-7. */
    external fun nativeGetTrackMute(track: Int): Boolean

    /** Set a track's volume. @param track 0-7 @param volume 0.0-1.0. */
    external fun nativeSetTrackVolume(track: Int, volume: Float)

    /** Set a track's pan position. @param track 0-7 @param pan -1.0 to 1.0. */
    external fun nativeSetTrackPan(track: Int, pan: Float)

    /**
     * Set a voice synthesis parameter on a track.
     * @param track   Track index (0-7).
     * @param paramId Voice-specific parameter ID (0-15).
     * @param value   Parameter value (range depends on voice type).
     */
    external fun nativeSetVoiceParam(track: Int, paramId: Int, value: Float)

    /**
     * Get a voice synthesis parameter from a track.
     * @param track   Track index (0-7).
     * @param paramId Voice-specific parameter ID (0-15).
     * @return Parameter value, or 0.0 if no voice assigned.
     */
    external fun nativeGetVoiceParam(track: Int, paramId: Int): Float

    /**
     * Trigger a drum pad (finger drumming).
     * The trigger is consumed on the next audio callback via atomic exchange.
     * @param track    Track index (0-7).
     * @param velocity Velocity in [0.0, 1.0].
     */
    external fun nativeTriggerPad(track: Int, velocity: Float)

    /** Set a track's drive saturation amount. @param track 0-7 @param drive 0.0-5.0. */
    external fun nativeSetTrackDrive(track: Int, drive: Float)

    /** Set a track's filter cutoff frequency. @param track 0-7 @param cutoff 20-16000 Hz. */
    external fun nativeSetTrackFilterCutoff(track: Int, cutoff: Float)

    /** Set a track's filter resonance. @param track 0-7 @param resonance 0.0-1.0. */
    external fun nativeSetTrackFilterRes(track: Int, resonance: Float)

    /**
     * Set a track's choke group.
     * Tracks in the same choke group (>= 0) mute each other on trigger.
     * @param track Track index (0-7).
     * @param group Choke group (-1=none, 0=hat group, 1-3=user groups).
     */
    external fun nativeSetTrackChokeGroup(track: Int, group: Int)

    /**
     * Set the voice engine type for a drum track.
     * Creates a new synthesis voice matching the requested type.
     * @param track Track index (0-7).
     * @param engineType Engine type (0=FM, 1=Subtractive, 2=Metallic, 3=Noise, 4=Physical, 5=MultiLayer).
     */
    external fun nativeSetTrackEngineType(track: Int, engineType: Int)

    // =========================================================================
    // Pattern editing
    // =========================================================================

    /**
     * Set a step's trigger and velocity on the active pattern.
     * Uses double-buffered atomic swap for lock-free editing.
     * @param track    Track index (0-7).
     * @param step     Step index (0-63).
     * @param velocity 0=off, 1-127=on with given velocity.
     */
    external fun nativeSetStep(track: Int, step: Int, velocity: Int)

    /**
     * Set a step's probability.
     * @param track   Track index (0-7).
     * @param step    Step index (0-63).
     * @param percent Probability 1-100 (100=always fire).
     */
    external fun nativeSetStepProb(track: Int, step: Int, percent: Int)

    /**
     * Set a step's retrig (ratchet) count.
     * @param track Track index (0-7).
     * @param step  Step index (0-63).
     * @param count Sub-triggers per step (0=off, 2, 3, 4, 6, 8).
     */
    external fun nativeSetStepRetrig(track: Int, step: Int, count: Int)

    /**
     * Set the global pattern length.
     * @param length Pattern length in steps (1-64).
     */
    external fun nativeSetPatternLength(length: Int)
}
