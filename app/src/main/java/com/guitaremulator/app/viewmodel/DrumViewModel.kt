package com.guitaremulator.app.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.guitaremulator.app.audio.DrumEngine
import com.guitaremulator.app.data.DrumKit
import com.guitaremulator.app.data.DrumPatternData
import com.guitaremulator.app.data.DrumRepository
import com.guitaremulator.app.data.DrumStep
import com.guitaremulator.app.data.DrumTrackData
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

/**
 * ViewModel managing the drum machine state, bridging between the Compose UI
 * layer and the native drum engine via [DrumEngine] JNI calls.
 *
 * Responsibilities:
 *   - Exposes observable [StateFlow]s for all drum machine UI state
 *   - Polls the native step cursor at ~60fps for real-time display
 *   - Manages pattern load/save through [DrumRepository]
 *   - Proxies all transport, track, and editing operations to the JNI layer
 *   - Maintains a Kotlin-side copy of the active pattern for UI rendering
 *
 * Thread safety: All JNI calls are safe from any thread (C++ uses atomics).
 * StateFlows are updated from viewModelScope coroutines (Main dispatcher).
 *
 * @param application Application context for DrumRepository file access.
 */
class DrumViewModel @JvmOverloads constructor(
    application: Application,
    private val drumRepository: DrumRepository = DrumRepository(application)
) : AndroidViewModel(application) {

    // =========================================================================
    // Transport State
    // =========================================================================

    /** Whether the drum sequencer is currently playing. */
    private val _isPlaying = MutableStateFlow(false)
    val isPlaying: StateFlow<Boolean> = _isPlaying.asStateFlow()

    /** Current tempo in BPM. */
    private val _tempo = MutableStateFlow(120f)
    val tempo: StateFlow<Float> = _tempo.asStateFlow()

    /** Current swing percentage (50=straight, 75=heavy). */
    private val _swing = MutableStateFlow(50f)
    val swing: StateFlow<Float> = _swing.asStateFlow()

    /** Current step cursor position for UI display (0 to length-1). */
    private val _currentStep = MutableStateFlow(0)
    val currentStep: StateFlow<Int> = _currentStep.asStateFlow()

    /** Current tempo multiplier (0.5=half, 1.0=normal, 2.0=double). */
    private val _tempoMultiplier = MutableStateFlow(1.0f)
    val tempoMultiplier: StateFlow<Float> = _tempoMultiplier.asStateFlow()

    /** Drum mix level relative to guitar (0.0-2.0, default 0.8). */
    private val _mixLevel = MutableStateFlow(0.8f)
    val mixLevel: StateFlow<Float> = _mixLevel.asStateFlow()

    // =========================================================================
    // Pattern State
    // =========================================================================

    /** The currently active drum pattern (Kotlin-side copy for UI rendering). */
    private val _currentPattern = MutableStateFlow(DrumPatternData())
    val currentPattern: StateFlow<DrumPatternData> = _currentPattern.asStateFlow()

    /** List of all available patterns for the pattern selector. */
    private val _patternList = MutableStateFlow<List<DrumPatternData>>(emptyList())
    val patternList: StateFlow<List<DrumPatternData>> = _patternList.asStateFlow()

    // =========================================================================
    // Track State
    // =========================================================================

    /** Currently selected track index for editing (0-7). */
    private val _selectedTrack = MutableStateFlow(0)
    val selectedTrack: StateFlow<Int> = _selectedTrack.asStateFlow()

    /** Per-track mute states for quick UI toggle display. */
    private val _trackMutes = MutableStateFlow(BooleanArray(8) { false })
    val trackMutes: StateFlow<BooleanArray> = _trackMutes.asStateFlow()

    /** Per-track solo states (implemented via mute toggling). */
    private val _trackSolos = MutableStateFlow(BooleanArray(8) { false })
    val trackSolos: StateFlow<BooleanArray> = _trackSolos.asStateFlow()

    /**
     * Set of track indices that fired on the current sequencer step.
     *
     * Updated at ~60fps alongside the step cursor. The pad grid observes
     * this to flash LEDs on pads whose tracks triggered a hit. The set
     * is cleared when the step advances, creating a brief pulse effect.
     */
    private val _activeTracksOnStep = MutableStateFlow<Set<Int>>(emptySet())
    val activeTracksOnStep: StateFlow<Set<Int>> = _activeTracksOnStep.asStateFlow()

    // =========================================================================
    // Initialization
    // =========================================================================

    init {
        // Load factory patterns on first launch
        viewModelScope.launch {
            drumRepository.loadFactoryPresets()
            refreshPatternList()

            // Load the first pattern if available
            val patterns = _patternList.value
            if (patterns.isNotEmpty()) {
                loadPattern(patterns.first().id)
            }
        }

        // Start step cursor polling (~60fps for smooth step indicator animation)
        // Also computes which tracks fired on the current step for pad LED flash.
        viewModelScope.launch {
            var previousStep = -1
            while (isActive) {
                if (_isPlaying.value) {
                    val step = DrumEngine.nativeGetCurrentStep()
                    _currentStep.value = step

                    // When the step advances, compute which tracks have a trigger
                    // on the new step. This drives the pad LED flash animation.
                    if (step != previousStep) {
                        previousStep = step
                        val pattern = _currentPattern.value
                        val active = mutableSetOf<Int>()
                        for (t in pattern.tracks.indices.take(8)) {
                            val trackSteps = pattern.tracks[t].steps
                            if (step < trackSteps.size && trackSteps[step].trigger) {
                                active.add(t)
                            }
                        }
                        _activeTracksOnStep.value = active
                    }
                } else {
                    // Clear active tracks when stopped
                    if (_activeTracksOnStep.value.isNotEmpty()) {
                        _activeTracksOnStep.value = emptySet()
                    }
                }
                delay(16L) // ~60fps
            }
        }
    }

    // =========================================================================
    // Transport Controls
    // =========================================================================

    /** Start the drum sequencer. */
    fun play() {
        DrumEngine.nativeSetPlaying(true)
        _isPlaying.value = true
    }

    /** Stop the drum sequencer. */
    fun stop() {
        DrumEngine.nativeSetPlaying(false)
        _isPlaying.value = false
        _currentStep.value = 0
    }

    /** Toggle play/stop. */
    fun togglePlayback() {
        if (_isPlaying.value) stop() else play()
    }

    /**
     * Set the tempo in BPM.
     * @param bpm Tempo (clamped to 20-300 by the native layer).
     */
    fun setTempo(bpm: Float) {
        DrumEngine.nativeSetTempo(bpm)
        _tempo.value = bpm.coerceIn(20f, 300f)
    }

    /**
     * Record a tap for tap tempo calculation.
     * Uses System.nanoTime() for sub-millisecond accuracy.
     */
    fun tapTempo() {
        val timestampNs = System.nanoTime()
        DrumEngine.nativeTapTempo(timestampNs)
        // Read back the tempo that the native median filter computed
        _tempo.value = DrumEngine.nativeGetTempo()
    }

    /**
     * Set swing percentage.
     * @param percent 50 (straight) to 75 (heavy shuffle).
     */
    fun setSwing(percent: Float) {
        val clamped = percent.coerceIn(50f, 75f)
        DrumEngine.nativeSetSwing(clamped)
        _swing.value = clamped
    }

    /**
     * Set the drum mix level.
     * @param level Linear gain (0.0=silent, 0.8=default, 1.0+=boosted).
     */
    fun setMixLevel(level: Float) {
        val clamped = level.coerceIn(0f, 2f)
        DrumEngine.nativeSetMixLevel(clamped)
        _mixLevel.value = clamped
    }

    /**
     * Activate fill mode (held down by user).
     * FILL-conditioned steps will fire; NOT_FILL steps will be skipped.
     */
    fun triggerFill() {
        DrumEngine.nativeSetFillActive(true)
    }

    /**
     * Deactivate fill mode (user released the fill button).
     */
    fun releaseFill() {
        DrumEngine.nativeSetFillActive(false)
    }

    /**
     * Set half-time mode.
     * @param enabled True for half-time (0.5x tempo multiplier).
     */
    fun setHalfTime(enabled: Boolean) {
        val mult = if (enabled) 0.5f else 1.0f
        DrumEngine.nativeSetTempoMultiplier(mult)
        _tempoMultiplier.value = mult
    }

    /**
     * Set double-time mode.
     * @param enabled True for double-time (2.0x tempo multiplier).
     */
    fun setDoubleTime(enabled: Boolean) {
        val mult = if (enabled) 2.0f else 1.0f
        DrumEngine.nativeSetTempoMultiplier(mult)
        _tempoMultiplier.value = mult
    }

    // =========================================================================
    // Track Controls
    // =========================================================================

    /**
     * Select a track for editing in the step editor / parameter page.
     * @param track Track index (0-7).
     */
    fun selectTrack(track: Int) {
        _selectedTrack.value = track.coerceIn(0, 7)
    }

    /**
     * Toggle a track's mute state.
     * @param track Track index (0-7).
     */
    fun muteTrack(track: Int) {
        val idx = track.coerceIn(0, 7)
        val mutes = _trackMutes.value.copyOf()
        mutes[idx] = !mutes[idx]

        DrumEngine.nativeSetTrackMute(idx, mutes[idx])
        _trackMutes.value = mutes

        // Update the Kotlin-side pattern copy
        updateTrackInPattern(idx) { it.copy(muted = mutes[idx]) }
    }

    /**
     * Toggle solo for a track. Solo is implemented by muting all other tracks.
     * Toggling solo off restores all tracks to unmuted.
     *
     * @param track Track index (0-7).
     */
    fun soloTrack(track: Int) {
        val idx = track.coerceIn(0, 7)
        val solos = _trackSolos.value.copyOf()
        solos[idx] = !solos[idx]

        // If any track is soloed, mute all non-soloed tracks
        val anySoloed = solos.any { it }
        val mutes = BooleanArray(8) { i ->
            if (anySoloed) !solos[i] else false
        }

        for (i in 0..7) {
            DrumEngine.nativeSetTrackMute(i, mutes[i])
        }

        _trackSolos.value = solos
        _trackMutes.value = mutes
    }

    /**
     * Set a track's volume.
     * @param track  Track index (0-7).
     * @param volume Volume in [0.0, 1.0].
     */
    fun setTrackVolume(track: Int, volume: Float) {
        val idx = track.coerceIn(0, 7)
        DrumEngine.nativeSetTrackVolume(idx, volume.coerceIn(0f, 1f))
        updateTrackInPattern(idx) { it.copy(volume = volume.coerceIn(0f, 1f)) }
    }

    /**
     * Set a track's pan position.
     * @param track Track index (0-7).
     * @param pan   Pan in [-1.0 (L), 1.0 (R)].
     */
    fun setTrackPan(track: Int, pan: Float) {
        val idx = track.coerceIn(0, 7)
        DrumEngine.nativeSetTrackPan(idx, pan.coerceIn(-1f, 1f))
        updateTrackInPattern(idx) { it.copy(pan = pan.coerceIn(-1f, 1f)) }
    }

    /**
     * Set a voice synthesis parameter on a track.
     * @param track   Track index (0-7).
     * @param paramId Voice-specific parameter ID (0-15).
     * @param value   Parameter value.
     */
    fun setVoiceParam(track: Int, paramId: Int, value: Float) {
        val idx = track.coerceIn(0, 7)
        DrumEngine.nativeSetVoiceParam(idx, paramId, value)

        // Update Kotlin-side pattern copy
        updateTrackInPattern(idx) { trackData ->
            val params = trackData.voiceParams.toMutableList()
            if (paramId in params.indices) {
                params[paramId] = value
            }
            trackData.copy(voiceParams = params)
        }
    }

    /**
     * Trigger a drum pad (finger drumming).
     * @param track    Track index (0-7).
     * @param velocity Velocity in [0.0, 1.0].
     */
    fun triggerPad(track: Int, velocity: Float = 1.0f) {
        DrumEngine.nativeTriggerPad(track.coerceIn(0, 7), velocity.coerceIn(0f, 1f))
    }

    // =========================================================================
    // Step Editing
    // =========================================================================

    /**
     * Toggle a step on/off. If the step is currently triggered, turns it off.
     * If off, turns it on with default velocity (100).
     *
     * @param track Track index (0-7).
     * @param step  Step index (0-63).
     */
    fun toggleStep(track: Int, step: Int) {
        val pattern = _currentPattern.value
        val trackIdx = track.coerceIn(0, 7)
        if (trackIdx >= pattern.tracks.size) return
        val trackData = pattern.tracks[trackIdx]
        if (step < 0 || step >= trackData.steps.size) return

        val currentStep = trackData.steps[step]
        val newVelocity = if (currentStep.trigger) 0 else 100

        // Update native engine (double-buffered atomic swap)
        DrumEngine.nativeSetStep(trackIdx, step, newVelocity)

        // Update Kotlin-side copy
        updateStepInPattern(trackIdx, step) {
            it.copy(trigger = newVelocity > 0, velocity = if (newVelocity > 0) 100 else 0)
        }
    }

    /**
     * Set a step's velocity.
     * @param track    Track index (0-7).
     * @param step     Step index (0-63).
     * @param velocity Velocity 0-127 (0 = step off).
     */
    fun setStepVelocity(track: Int, step: Int, velocity: Int) {
        val v = velocity.coerceIn(0, 127)
        DrumEngine.nativeSetStep(track.coerceIn(0, 7), step, v)
        updateStepInPattern(track.coerceIn(0, 7), step) {
            it.copy(trigger = v > 0, velocity = v)
        }
    }

    /**
     * Set a step's probability.
     * @param track   Track index (0-7).
     * @param step    Step index (0-63).
     * @param percent Probability 1-100.
     */
    fun setStepProbability(track: Int, step: Int, percent: Int) {
        val p = percent.coerceIn(1, 100)
        DrumEngine.nativeSetStepProb(track.coerceIn(0, 7), step, p)
        updateStepInPattern(track.coerceIn(0, 7), step) {
            it.copy(probability = p)
        }
    }

    /**
     * Set a step's retrig count.
     * @param track Track index (0-7).
     * @param step  Step index (0-63).
     * @param count Sub-triggers (0=off, 2, 3, 4, 6, 8).
     */
    fun setStepRetrig(track: Int, step: Int, count: Int) {
        val c = count.coerceIn(0, 8)
        DrumEngine.nativeSetStepRetrig(track.coerceIn(0, 7), step, c)
        updateStepInPattern(track.coerceIn(0, 7), step) {
            it.copy(retrigCount = c)
        }
    }

    /**
     * Set the pattern length.
     * @param length Length in steps (1-64).
     */
    fun setPatternLength(length: Int) {
        val l = length.coerceIn(1, 64)
        DrumEngine.nativeSetPatternLength(l)
        _currentPattern.value = _currentPattern.value.copy(length = l)
    }

    // =========================================================================
    // Pattern Management
    // =========================================================================

    /**
     * Load a drum pattern from storage and apply it to the native engine.
     *
     * Pushes all step data, track configs, and transport settings to C++
     * via individual JNI calls (the C++ setStep/setPattern API handles
     * the double-buffered swap atomically).
     *
     * @param patternId UUID of the pattern to load.
     */
    fun loadPattern(patternId: String) {
        viewModelScope.launch {
            val pattern = drumRepository.loadPattern(patternId) ?: return@launch

            // ================================================================
            // ORDERING IS CRITICAL:
            //
            // nativeSetStep / nativeSetStepProb / nativeSetStepRetrig /
            // nativeSetPatternLength all internally call C++ setPattern()
            // which overwrites DrumTrack volume/pan/mute/chokeGroup AND
            // resets clock tempo/swing from the C++ Pattern struct.
            //
            // Therefore we MUST:
            //   Phase 1: Set engine types + voice params (creates voices)
            //   Phase 2: Set all steps (builds C++ Pattern struct via
            //            copy-modify-swap; setPattern() side effects here
            //            are harmless because we re-apply everything after)
            //   Phase 3: Set pattern length (also calls setPattern())
            //   Phase 4: Re-apply track config (volume/pan/mute/choke/
            //            drive/filter) AFTER all steps, so the final
            //            setPattern() call in phase 2-3 doesn't clobber them
            //   Phase 5: Set tempo + swing LAST (setPattern() in phase 2-3
            //            overwrites clock tempo/swing from stale Pattern data)
            // ================================================================

            // Phase 1: Create voices and set synthesis parameters
            // Engine type must be set first -- it creates a new voice with
            // constructor defaults. Voice params then overwrite those defaults.
            for (t in pattern.tracks.indices.take(8)) {
                val track = pattern.tracks[t]
                DrumEngine.nativeSetTrackEngineType(t, track.engineType)
                for (p in track.voiceParams.indices.take(16)) {
                    DrumEngine.nativeSetVoiceParam(t, p, track.voiceParams[p])
                }
            }

            // Phase 2: Load all step data into the C++ Pattern struct
            // Each nativeSetStep call does copy-modify-swap and calls
            // setPattern(), which has side effects on DrumTrack mix params
            // and clock tempo/swing. We accept those side effects here and
            // correct them in phases 3-5.
            for (t in pattern.tracks.indices.take(8)) {
                val track = pattern.tracks[t]
                for (s in track.steps.indices.take(64)) {
                    val step = track.steps[s]
                    DrumEngine.nativeSetStep(t, s, if (step.trigger) step.velocity else 0)
                    if (step.probability != 100) {
                        DrumEngine.nativeSetStepProb(t, s, step.probability)
                    }
                    if (step.retrigCount > 0) {
                        DrumEngine.nativeSetStepRetrig(t, s, step.retrigCount)
                    }
                }
            }

            // Phase 3: Set pattern length (also calls setPattern() internally)
            DrumEngine.nativeSetPatternLength(pattern.length)

            // Phase 4: Apply track mix params AFTER all steps are loaded.
            // This is the authoritative source for volume/pan/mute/choke/
            // drive/filter -- any values that setPattern() clobbered above
            // are now corrected.
            for (t in pattern.tracks.indices.take(8)) {
                val track = pattern.tracks[t]
                DrumEngine.nativeSetTrackVolume(t, track.volume)
                DrumEngine.nativeSetTrackPan(t, track.pan)
                DrumEngine.nativeSetTrackMute(t, track.muted)
                DrumEngine.nativeSetTrackChokeGroup(t, track.chokeGroup)
                DrumEngine.nativeSetTrackDrive(t, track.drive)
                DrumEngine.nativeSetTrackFilterCutoff(t, track.filterCutoff)
                DrumEngine.nativeSetTrackFilterRes(t, track.filterRes)
            }

            // Phase 5: Set tempo and swing LAST so they aren't overwritten
            // by setPattern() calls in phases 2-3
            DrumEngine.nativeSetTempo(pattern.bpm)
            DrumEngine.nativeSetSwing(pattern.swing)

            // Update Kotlin-side state
            _currentPattern.value = pattern
            _tempo.value = pattern.bpm
            _swing.value = pattern.swing
            _trackMutes.value = BooleanArray(8) { i ->
                if (i < pattern.tracks.size) pattern.tracks[i].muted else false
            }
            _trackSolos.value = BooleanArray(8) { false }
        }
    }

    /**
     * Save the current pattern to storage.
     *
     * @param name Optional new name. If null, keeps the existing name.
     * @return True if saved successfully.
     */
    fun savePattern(name: String? = null): Boolean {
        val pattern = _currentPattern.value
        val toSave = if (name != null) {
            pattern.copy(name = name.take(100), isFactory = false)
        } else {
            pattern.copy(isFactory = false)
        }

        val saved = drumRepository.savePattern(toSave)
        if (saved) {
            _currentPattern.value = toSave
            viewModelScope.launch { refreshPatternList() }
        }
        return saved
    }

    /**
     * Save the current pattern as a new copy with a new ID.
     * @param name Name for the new pattern.
     * @return True if saved successfully.
     */
    fun savePatternAs(name: String): Boolean {
        val pattern = _currentPattern.value.copy(
            id = java.util.UUID.randomUUID().toString(),
            name = name.take(100),
            isFactory = false
        )

        val saved = drumRepository.savePattern(pattern)
        if (saved) {
            _currentPattern.value = pattern
            viewModelScope.launch { refreshPatternList() }
        }
        return saved
    }

    /**
     * Delete a pattern from storage.
     * @param patternId UUID of the pattern to delete.
     */
    fun deletePattern(patternId: String) {
        viewModelScope.launch {
            if (drumRepository.deletePattern(patternId)) {
                refreshPatternList()
                // If we deleted the active pattern, load the first available
                if (_currentPattern.value.id == patternId) {
                    val patterns = _patternList.value
                    if (patterns.isNotEmpty()) {
                        loadPattern(patterns.first().id)
                    } else {
                        _currentPattern.value = DrumPatternData()
                    }
                }
            }
        }
    }

    /**
     * Refresh the pattern list from storage.
     */
    private fun refreshPatternList() {
        _patternList.value = drumRepository.getPatternList()
    }

    // =========================================================================
    // Internal helpers
    // =========================================================================

    /**
     * Update a single track in the Kotlin-side pattern copy.
     * Creates a new immutable copy with the modified track.
     */
    private inline fun updateTrackInPattern(
        trackIndex: Int,
        transform: (DrumTrackData) -> DrumTrackData
    ) {
        val pattern = _currentPattern.value
        if (trackIndex >= pattern.tracks.size) return
        val tracks = pattern.tracks.toMutableList()
        tracks[trackIndex] = transform(tracks[trackIndex])
        _currentPattern.value = pattern.copy(tracks = tracks)
    }

    /**
     * Update a single step in the Kotlin-side pattern copy.
     * Creates new immutable copies of the track and step lists.
     */
    private inline fun updateStepInPattern(
        trackIndex: Int,
        stepIndex: Int,
        transform: (DrumStep) -> DrumStep
    ) {
        val pattern = _currentPattern.value
        if (trackIndex >= pattern.tracks.size) return
        val trackData = pattern.tracks[trackIndex]
        if (stepIndex < 0 || stepIndex >= trackData.steps.size) return

        val steps = trackData.steps.toMutableList()
        steps[stepIndex] = transform(steps[stepIndex])

        val tracks = pattern.tracks.toMutableList()
        tracks[trackIndex] = trackData.copy(steps = steps)
        _currentPattern.value = pattern.copy(tracks = tracks)
    }
}
