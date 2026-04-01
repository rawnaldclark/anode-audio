package com.guitaremulator.app.viewmodel

import android.util.Log
import com.guitaremulator.app.audio.IAudioEngine
import com.guitaremulator.app.data.LooperSession
import com.guitaremulator.app.data.LooperSessionManager
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * Delegate handling all looper state management and transport commands.
 *
 * This delegate manages the looper's observable state (transport state,
 * playback position, beat tracking), settings (BPM, metronome, quantization),
 * and session persistence (save/load/delete). It follows the same delegate
 * pattern as [ABComparisonDelegate] -- no coroutine launching; all async
 * work is driven by the caller (AudioViewModel).
 *
 * State polling is performed by [pollState], which is called from the
 * existing polling loop in [AudioViewModel] at ~30fps alongside level
 * meters and spectrum data. This avoids creating a separate coroutine.
 *
 * Transport commands map to the C++ looper command enum:
 *   0 = None, 1 = Record, 2 = Stop, 3 = Overdub, 4 = Clear, 5 = Undo, 6 = Toggle
 *
 * Looper states from native engine:
 *   0 = Idle, 1 = Recording, 2 = Playing, 3 = Overdubbing, 4 = Finishing, 5 = CountIn
 *
 * @property audioEngine JNI bridge for looper and metronome commands.
 * @property sessionManager Handles saving/loading looper sessions to disk.
 * @property onLoadPreset Optional callback to load a preset by ID before
 *     re-amp playback begins. Wired to [PresetDelegate.loadPreset] via
 *     [AudioViewModel] so the saved session's original tone is restored
 *     before the dry recording plays through the effects chain.
 */
class LooperDelegate(
    private val audioEngine: IAudioEngine,
    private val sessionManager: LooperSessionManager,
    private val onLoadPreset: ((String) -> Unit)? = null
) {

    companion object {
        private const val TAG = "LooperDelegate"

        // Transport command constants (must match C++ LooperCommand enum)
        private const val CMD_NONE = 0
        private const val CMD_RECORD = 1
        private const val CMD_STOP = 2
        private const val CMD_OVERDUB = 3
        private const val CMD_CLEAR = 4
        private const val CMD_UNDO = 5
        private const val CMD_TOGGLE = 6

        // Looper state constants (must match C++ LooperState enum)
        const val STATE_IDLE = 0
        const val STATE_RECORDING = 1
        const val STATE_PLAYING = 2
        const val STATE_OVERDUBBING = 3
        const val STATE_FINISHING = 4
        const val STATE_COUNT_IN = 5

        // Default looper buffer duration in seconds.
        // Allocates ~66MB (3 buffers x 120s x 48000 x 4 bytes + reamp + fade).
        // Halved to 60s on devices with < 512MB available RAM.
        private const val DEFAULT_MAX_DURATION_SECONDS = 120

        /** Maximum number of independently-controllable layers (must match C++ kMaxLayers). */
        const val MAX_LAYERS = 8

        /** Number of available metronome tone types (must match C++ kMetronomeToneCount). */
        const val METRONOME_TONE_COUNT = 6

        /** Human-readable names for each metronome tone type, indexed 0-5. */
        val METRONOME_TONE_NAMES = listOf(
            "Click", "Soft", "Rim", "Cowbell", "Hi-Hat", "Deep"
        )
    }

    // =========================================================================
    // Observable State -- Looper Transport
    // =========================================================================

    /**
     * Current looper transport state.
     * 0=Idle, 1=Recording, 2=Playing, 3=Overdubbing, 4=Finishing, 5=CountIn.
     */
    private val _looperState = MutableStateFlow(STATE_IDLE)
    val looperState: StateFlow<Int> = _looperState.asStateFlow()

    /** Current playback position in samples (for waveform cursor display). */
    private val _playbackPosition = MutableStateFlow(0)
    val playbackPosition: StateFlow<Int> = _playbackPosition.asStateFlow()

    /** Total loop length in samples (0 if no loop recorded). */
    private val _loopLength = MutableStateFlow(0)
    val loopLength: StateFlow<Int> = _loopLength.asStateFlow()

    /** Current beat index within the bar (0-based, for metronome indicator). */
    private val _currentBeat = MutableStateFlow(0)
    val currentBeat: StateFlow<Int> = _currentBeat.asStateFlow()

    // =========================================================================
    // Observable State -- Settings
    // =========================================================================

    /** Current tempo in BPM (30-300). */
    private val _bpm = MutableStateFlow(120f)
    val bpm: StateFlow<Float> = _bpm.asStateFlow()

    /** Whether the metronome click is active. */
    private val _isMetronomeActive = MutableStateFlow(false)
    val isMetronomeActive: StateFlow<Boolean> = _isMetronomeActive.asStateFlow()

    /** Whether quantized (bar-snapping) mode is enabled. */
    private val _isQuantizedMode = MutableStateFlow(false)
    val isQuantizedMode: StateFlow<Boolean> = _isQuantizedMode.asStateFlow()

    /** Number of bars for quantized loop length. */
    private val _barCount = MutableStateFlow(4)
    val barCount: StateFlow<Int> = _barCount.asStateFlow()

    /** Number of beats per bar (time signature numerator). */
    private val _beatsPerBar = MutableStateFlow(4)
    val beatsPerBar: StateFlow<Int> = _beatsPerBar.asStateFlow()

    /** Beat unit (time signature denominator: 2, 4, 8, or 16). */
    private val _beatUnit = MutableStateFlow(4)
    val beatUnit: StateFlow<Int> = _beatUnit.asStateFlow()

    /** Whether count-in (one bar of clicks before recording) is enabled. */
    private val _isCountInEnabled = MutableStateFlow(true)
    val isCountInEnabled: StateFlow<Boolean> = _isCountInEnabled.asStateFlow()

    /** Loop playback volume (0.0 = silent, 1.0 = unity). */
    private val _loopVolume = MutableStateFlow(1.0f)
    val loopVolume: StateFlow<Float> = _loopVolume.asStateFlow()

    /** Metronome click volume (0.0 = silent, 1.0 = full). */
    private val _metronomeVolume = MutableStateFlow(0.7f)
    val metronomeVolume: StateFlow<Float> = _metronomeVolume.asStateFlow()

    /** Metronome tone type (0-5). See [METRONOME_TONE_NAMES] for labels. */
    private val _metronomeTone = MutableStateFlow(0)
    val metronomeTone: StateFlow<Int> = _metronomeTone.asStateFlow()

    // =========================================================================
    // Observable State -- Per-layer Volume
    // =========================================================================

    /**
     * Number of recorded layers. 0 = no loop, 1 = base, 2+ = overdubs.
     * Updated by [pollState].
     */
    private val _layerCount = MutableStateFlow(0)
    val layerCount: StateFlow<Int> = _layerCount.asStateFlow()

    /**
     * Per-layer volumes as a list. Index 0 = base, 1+ = overdub layers.
     * Each value in [0.0, 1.5]. Updated when user adjusts a layer slider.
     */
    private val _layerVolumes = MutableStateFlow(listOf(1.0f))
    val layerVolumes: StateFlow<List<Float>> = _layerVolumes.asStateFlow()

    // =========================================================================
    // Observable State -- Visibility
    // =========================================================================

    /** Whether the looper UI panel is visible. */
    private val _isLooperVisible = MutableStateFlow(false)
    val isLooperVisible: StateFlow<Boolean> = _isLooperVisible.asStateFlow()

    // =========================================================================
    // Observable State -- Re-amping
    // =========================================================================

    /** Whether the re-amp feature is currently active (dry playback through effects). */
    private val _isReamping = MutableStateFlow(false)
    val isReamping: StateFlow<Boolean> = _isReamping.asStateFlow()

    // =========================================================================
    // Observable State -- Trim / Crop
    // =========================================================================

    /** Whether trim preview mode is currently active. */
    private val _isTrimPreviewActive = MutableStateFlow(false)
    val isTrimPreviewActive: StateFlow<Boolean> = _isTrimPreviewActive.asStateFlow()

    /** Current trim start position in samples. */
    private val _trimStart = MutableStateFlow(0)
    val trimStart: StateFlow<Int> = _trimStart.asStateFlow()

    /** Current trim end position in samples. */
    private val _trimEnd = MutableStateFlow(0)
    val trimEnd: StateFlow<Int> = _trimEnd.asStateFlow()

    /** Whether crop undo is available (one-level). */
    private val _isCropUndoAvailable = MutableStateFlow(false)
    val isCropUndoAvailable: StateFlow<Boolean> = _isCropUndoAvailable.asStateFlow()

    /** Waveform summary data: min/max pairs per display column. */
    private val _waveformData = MutableStateFlow<FloatArray?>(null)
    val waveformData: StateFlow<FloatArray?> = _waveformData.asStateFlow()

    /** Whether the trim editor panel is visible. */
    private val _isTrimEditorVisible = MutableStateFlow(false)
    val isTrimEditorVisible: StateFlow<Boolean> = _isTrimEditorVisible.asStateFlow()

    /** Current snap mode for trim handles. */
    enum class SnapMode { FREE, ZERO_CROSSING, BEAT }
    private val _snapMode = MutableStateFlow(SnapMode.FREE)
    val snapMode: StateFlow<SnapMode> = _snapMode.asStateFlow()

    // =========================================================================
    // Observable State -- Session Library
    // =========================================================================

    /** All saved looper sessions, sorted by creation date (newest first). */
    private val _savedSessions = MutableStateFlow<List<LooperSession>>(emptyList())
    val savedSessions: StateFlow<List<LooperSession>> = _savedSessions.asStateFlow()

    /** Whether the session library panel is visible. */
    private val _isSessionLibraryVisible = MutableStateFlow(false)
    val isSessionLibraryVisible: StateFlow<Boolean> = _isSessionLibraryVisible.asStateFlow()

    // =========================================================================
    // Internal State
    // =========================================================================

    /** Whether looperInit has been called on the native engine. */
    private var isInitialized = false

    // =========================================================================
    // Transport Commands
    // =========================================================================

    /**
     * Send the TOGGLE command to the looper (main transport button).
     *
     * Behavior depends on current state:
     *   - Idle: Start recording
     *   - Recording: Stop recording and begin playback
     *   - Playing: Start overdubbing
     *   - Overdubbing: Return to playback
     */
    fun toggle() {
        ensureInitialized()
        audioEngine.looperSendCommand(CMD_TOGGLE)
    }

    /**
     * Start recording a new loop.
     *
     * If a loop already exists, this clears it and starts fresh.
     * If count-in is enabled, plays one bar of metronome clicks first.
     */
    fun record() {
        ensureInitialized()
        audioEngine.looperSendCommand(CMD_RECORD)
    }

    /**
     * Stop the looper transport.
     *
     * Stops both playback and recording. The recorded loop remains in
     * memory and can be resumed by calling [toggle].
     */
    fun stop() {
        audioEngine.looperSendCommand(CMD_STOP)
    }

    /**
     * Enter overdub mode.
     *
     * Layers new audio on top of the existing loop. Only valid when
     * a loop is already recorded and playing.
     */
    fun overdub() {
        audioEngine.looperSendCommand(CMD_OVERDUB)
    }

    /**
     * Clear the loop buffer and return to idle state.
     *
     * Erases all recorded audio. Cannot be undone.
     */
    fun clear() {
        audioEngine.looperSendCommand(CMD_CLEAR)
    }

    /**
     * Undo the last overdub layer.
     *
     * Reverts to the loop state before the most recent overdub pass.
     * Only one level of undo is supported (matching hardware loopers).
     */
    fun undo() {
        audioEngine.looperSendCommand(CMD_UNDO)
    }

    // =========================================================================
    // Settings
    // =========================================================================

    /**
     * Set the looper/metronome tempo.
     *
     * Updates both the local StateFlow and the native engine BPM.
     * Value is clamped to [30, 300] BPM by the native engine.
     *
     * @param bpm Tempo in beats per minute.
     */
    fun setBPM(bpm: Float) {
        val clamped = bpm.coerceIn(30f, 300f)
        _bpm.value = clamped
        audioEngine.setMetronomeBPM(clamped)
    }

    /**
     * Register a tap for tap-tempo BPM detection.
     *
     * Sends the current timestamp to the native tap-tempo detector and
     * reads back the computed BPM. If the detection returns a valid BPM
     * (> 0), updates the local BPM state.
     */
    fun tapTempo() {
        audioEngine.onTapTempo(System.nanoTime() / 1_000_000)
        val detected = audioEngine.getTapTempoBPM()
        if (detected > 0f) {
            _bpm.value = detected
            audioEngine.setMetronomeBPM(detected)
        }
    }

    /**
     * Enable or disable the metronome click.
     *
     * @param active true to enable clicks, false to silence.
     */
    fun setMetronomeActive(active: Boolean) {
        _isMetronomeActive.value = active
        audioEngine.setMetronomeActive(active)
    }

    /**
     * Set the metronome tone type.
     *
     * Updates both the local StateFlow and the native engine tone.
     * Value is clamped to [0, METRONOME_TONE_COUNT - 1].
     *
     * @param tone Tone index (0-5).
     */
    fun setMetronomeTone(tone: Int) {
        val clamped = tone.coerceIn(0, METRONOME_TONE_COUNT - 1)
        _metronomeTone.value = clamped
        audioEngine.setMetronomeTone(clamped)
    }

    /**
     * Enable or disable quantized (bar-snapping) mode.
     *
     * When enabled, loop boundaries snap to bar lines for precise
     * musical timing. When disabled, recording is free-form.
     *
     * @param enabled true for quantized mode.
     */
    fun setQuantizedMode(enabled: Boolean) {
        _isQuantizedMode.value = enabled
        audioEngine.setQuantizedMode(enabled)
    }

    /**
     * Set the number of bars for quantized loop length.
     *
     * Only effective when quantized mode is enabled. The native engine
     * clamps to [1, 64] bars.
     *
     * @param bars Number of bars.
     */
    fun setBarCount(bars: Int) {
        val clamped = bars.coerceIn(1, 64)
        _barCount.value = clamped
        audioEngine.setQuantizedBars(clamped)
    }

    /**
     * Set the time signature for the metronome and quantized mode.
     *
     * Updates both the local state and the native engine.
     *
     * @param beatsPerBar Numerator of the time signature (1-16).
     * @param beatUnit Denominator of the time signature (2, 4, 8, or 16).
     */
    fun setTimeSignature(beatsPerBar: Int, beatUnit: Int) {
        _beatsPerBar.value = beatsPerBar.coerceIn(1, 16)
        _beatUnit.value = beatUnit
        audioEngine.setTimeSignature(beatsPerBar, beatUnit)
    }

    /**
     * Enable or disable count-in before recording.
     *
     * When enabled, the looper plays one bar of metronome clicks before
     * recording begins, giving the player time to prepare.
     *
     * @param enabled true to enable count-in.
     */
    fun setCountInEnabled(enabled: Boolean) {
        _isCountInEnabled.value = enabled
        audioEngine.setCountInEnabled(enabled)
    }

    /**
     * Set the loop playback volume.
     *
     * @param volume Linear gain (0.0 = silent, 1.0 = unity).
     */
    fun setLoopVolume(volume: Float) {
        val clamped = volume.coerceIn(0f, 1f)
        _loopVolume.value = clamped
        audioEngine.looperSetVolume(clamped)
    }

    /**
     * Set the metronome click volume.
     *
     * @param volume Linear gain (0.0 = silent, 1.0 = full).
     */
    fun setMetronomeVolume(volume: Float) {
        val clamped = volume.coerceIn(0f, 1f)
        _metronomeVolume.value = clamped
        audioEngine.setMetronomeVolume(clamped)
    }

    /**
     * Set the volume of an individual layer.
     *
     * @param layer Layer index (0 = base recording, 1+ = overdub passes).
     * @param volume Linear gain (0.0 = silent, 1.0 = unity, up to 1.5 = boost).
     */
    fun setLayerVolume(layer: Int, volume: Float) {
        if (layer < 0 || layer >= MAX_LAYERS) return
        val clamped = volume.coerceIn(0f, 1.5f)
        // Update local state
        val current = _layerVolumes.value.toMutableList()
        while (current.size <= layer) current.add(1.0f)
        current[layer] = clamped
        _layerVolumes.value = current
        // Push to native engine (triggers wet buffer recomposition)
        audioEngine.looperSetLayerVolume(layer, clamped)
    }

    // =========================================================================
    // Visibility
    // =========================================================================

    /**
     * Show the looper UI panel.
     *
     * Initializes native looper buffers if not already done. The buffers
     * are kept alive even when the panel is hidden so a playing loop
     * continues in the background.
     */
    fun showLooper() {
        ensureInitialized()
        _isLooperVisible.value = true
    }

    /**
     * Hide the looper UI panel.
     *
     * Does NOT release native buffers -- the loop may still be playing
     * in the background. Call [clear] + [looperRelease] explicitly if
     * you need to free memory.
     */
    fun hideLooper() {
        _isLooperVisible.value = false
    }

    // =========================================================================
    // Polling
    // =========================================================================

    /**
     * Poll the native looper and metronome state.
     *
     * Called from the AudioViewModel's polling coroutine at ~30fps alongside
     * level meters and spectrum data. Only polls when the looper is visible
     * or a loop is playing/recording (to track state transitions even when
     * the UI is hidden).
     */
    fun pollState() {
        // Always poll looper state to detect transitions (e.g., quantized
        // recording finishing while UI is hidden)
        if (!isInitialized) return

        _looperState.value = audioEngine.looperGetState()
        _playbackPosition.value = audioEngine.looperGetPlaybackPosition()
        _loopLength.value = audioEngine.looperGetLoopLength()
        _currentBeat.value = audioEngine.getCurrentBeat()

        // Update re-amp state
        val wasReamping = _isReamping.value
        val isNowReamping = audioEngine.isReamping()
        if (wasReamping != isNowReamping) {
            _isReamping.value = isNowReamping
        }

        // Update trim state (only when trim editor is visible)
        if (_isTrimEditorVisible.value) {
            _isCropUndoAvailable.value = audioEngine.isCropUndoAvailable()
        }

        // Update layer count (drives per-layer volume UI)
        val newLayerCount = audioEngine.looperGetLayerCount()
        if (newLayerCount != _layerCount.value) {
            _layerCount.value = newLayerCount
            // Expand the volumes list if new layers were added
            val currentVolumes = _layerVolumes.value.toMutableList()
            while (currentVolumes.size < newLayerCount) {
                currentVolumes.add(1.0f)
            }
            _layerVolumes.value = currentVolumes.take(newLayerCount.coerceAtLeast(1))
        }
    }

    // =========================================================================
    // Trim / Crop
    // =========================================================================

    /**
     * Show the trim editor for the current loop.
     *
     * Enters trim preview mode on the native engine (constraining playback
     * to the trim range), fetches the waveform summary for display, and
     * shows the trim editor panel.
     *
     * @param waveformColumns Number of horizontal pixels for waveform display.
     */
    fun showTrimEditor(waveformColumns: Int = 300) {
        ensureInitialized()
        val len = _loopLength.value
        if (len <= 0) {
            Log.w(TAG, "Cannot show trim editor: no loop recorded")
            return
        }

        audioEngine.startTrimPreview()
        _isTrimPreviewActive.value = true
        _trimStart.value = 0
        _trimEnd.value = len

        // Fetch waveform summary for the UI
        refreshWaveformData(waveformColumns)

        _isTrimEditorVisible.value = true
        Log.d(TAG, "Trim editor opened, loop length: $len samples")
    }

    /**
     * Hide the trim editor and cancel any active trim preview.
     *
     * Restores full loop playback without committing changes.
     */
    fun hideTrimEditor() {
        if (_isTrimPreviewActive.value) {
            audioEngine.cancelTrimPreview()
            _isTrimPreviewActive.value = false
        }
        _isTrimEditorVisible.value = false
        _waveformData.value = null
    }

    /**
     * Set the trim start position.
     *
     * Applies snap mode if not FREE. Updates the native engine and local state.
     *
     * @param startSample Desired start position in samples.
     */
    fun setTrimStart(startSample: Int) {
        if (!_isTrimPreviewActive.value) return
        val snapped = applySnap(startSample)
        audioEngine.setTrimStart(snapped)
        _trimStart.value = audioEngine.getTrimStart()
    }

    /**
     * Set the trim end position.
     *
     * Applies snap mode if not FREE. Updates the native engine and local state.
     *
     * @param endSample Desired end position in samples.
     */
    fun setTrimEnd(endSample: Int) {
        if (!_isTrimPreviewActive.value) return
        val snapped = applySnap(endSample)
        audioEngine.setTrimEnd(snapped)
        _trimEnd.value = audioEngine.getTrimEnd()
    }

    /**
     * Set the snap mode for trim handles.
     *
     * @param mode FREE (no snap), ZERO_CROSSING (snap to nearest zero), or BEAT.
     */
    fun setSnapMode(mode: SnapMode) {
        _snapMode.value = mode
    }

    /**
     * Commit the current trim: permanently crop the loop to [trimStart, trimEnd).
     *
     * After commit, the loop length is updated and crop undo becomes available.
     * The trim editor remains visible so the user can verify the result.
     *
     * @return true if the trim was committed successfully.
     */
    fun commitTrim(): Boolean {
        if (!_isTrimPreviewActive.value) return false

        val success = audioEngine.commitTrim()
        if (success) {
            _isTrimPreviewActive.value = false
            _isCropUndoAvailable.value = audioEngine.isCropUndoAvailable()
            // Update loop length from native
            _loopLength.value = audioEngine.looperGetLoopLength()
            _trimStart.value = 0
            _trimEnd.value = _loopLength.value
            refreshWaveformData()
            Log.d(TAG, "Trim committed, new loop length: ${_loopLength.value}")
        } else {
            Log.e(TAG, "Failed to commit trim")
        }
        return success
    }

    /**
     * Undo the last crop operation, restoring the pre-crop loop.
     *
     * @return true if the crop was undone.
     */
    fun undoCrop(): Boolean {
        val success = audioEngine.undoCrop()
        if (success) {
            _isCropUndoAvailable.value = false
            _loopLength.value = audioEngine.looperGetLoopLength()
            _trimEnd.value = _loopLength.value
            refreshWaveformData()
            Log.d(TAG, "Crop undone, restored loop length: ${_loopLength.value}")
        }
        return success
    }

    /**
     * Refresh the waveform summary data for the trim editor display.
     *
     * @param numColumns Number of display columns for the waveform.
     */
    fun refreshWaveformData(numColumns: Int = 300) {
        val data = audioEngine.getWaveformSummary(numColumns, 0, -1)
        _waveformData.value = data
    }

    /**
     * Apply the current snap mode to a sample position.
     *
     * @param sample Raw sample position.
     * @return Snapped sample position.
     */
    private fun applySnap(sample: Int): Int {
        return when (_snapMode.value) {
            SnapMode.FREE -> sample
            SnapMode.ZERO_CROSSING -> audioEngine.findNearestZeroCrossing(sample, -1)
            SnapMode.BEAT -> audioEngine.snapToBeat(sample)
        }
    }

    // =========================================================================
    // Session Management
    // =========================================================================

    /**
     * Export the current loop to WAV files and save as a named session.
     *
     * This is the primary save flow: exports the native loop buffers to
     * temporary WAV files via JNI, then saves them as a session with metadata.
     *
     * @param name User-facing display name for the session.
     * @param presetId Currently active preset ID (null if none).
     * @param presetName Currently active preset name (null if none).
     */
    fun exportAndSaveSession(
        name: String,
        presetId: String? = null,
        presetName: String? = null
    ) {
        val loopLengthSamples = _loopLength.value
        if (loopLengthSamples <= 0) {
            Log.w(TAG, "Cannot save session: no loop recorded")
            return
        }

        val sampleRate = audioEngine.getSampleRate().coerceAtLeast(1)
        val durationMs = (loopLengthSamples.toLong() * 1000L) / sampleRate

        // Create session to get its UUID for directory creation
        val session = LooperSession(
            name = name.take(100).ifEmpty { "Untitled Loop" },
            bpm = if (_isQuantizedMode.value) _bpm.value else null,
            timeSignature = "${_beatsPerBar.value}/${_beatUnit.value}",
            barCount = if (_isQuantizedMode.value) _barCount.value else null,
            presetId = presetId,
            presetName = presetName,
            dryFilePath = "",  // Temporary, updated after export
            wetFilePath = "",
            durationMs = durationMs,
            sampleRate = sampleRate
        )

        // Get the session directory and create temp WAV paths
        val sessionDir = sessionManager.getSessionDir(session.id)
        if (!sessionDir.exists()) sessionDir.mkdirs()

        val dryPath = java.io.File(sessionDir, "dry.wav").absolutePath
        val wetPath = java.io.File(sessionDir, "wet.wav").absolutePath

        // Export loop buffers to WAV files via JNI
        val exported = audioEngine.looperExportLoop(dryPath, wetPath)
        if (!exported) {
            Log.e(TAG, "Failed to export loop to WAV files")
            sessionDir.deleteRecursively()
            return
        }

        // Save metadata
        val savedSession = session.copy(
            dryFilePath = dryPath,
            wetFilePath = wetPath
        )

        // Write metadata directly (WAV files are already in place)
        val metadataFile = java.io.File(sessionDir, "metadata.json")
        metadataFile.writeText(savedSession.toJson().toString(2))

        Log.d(TAG, "Saved session: ${session.name} (${session.id})")
        refreshSessions()
    }

    /**
     * Start re-amping a saved session's dry recording.
     *
     * Loads the dry WAV file into the native re-amp buffer and starts
     * playback through the current effects chain. The user can tweak
     * effects in real-time while hearing their recorded performance.
     *
     * @param session The session containing the dry recording to re-amp.
     */
    fun startReamp(session: LooperSession) {
        ensureInitialized()

        // Load the preset that was active when this loop was recorded
        // so the user hears the original tone first, rather than whatever
        // preset happens to be active now (which could be jarring/loud).
        val presetId = session.presetId
        if (presetId != null) {
            onLoadPreset?.invoke(presetId)
            Log.d(TAG, "Restored saved preset '${session.presetName}' for re-amp")
        } else {
            Log.w(TAG, "No saved preset for session '${session.name}', using current preset")
        }

        val loaded = audioEngine.loadDryForReamp(session.dryFilePath)
        if (!loaded) {
            Log.e(TAG, "Failed to load dry recording for re-amp: ${session.dryFilePath}")
            return
        }

        audioEngine.startReamp()
        _isReamping.value = true
        Log.d(TAG, "Re-amp started: ${session.name}")
    }

    /**
     * Stop re-amp playback and return to live input.
     */
    fun stopReamp() {
        audioEngine.stopReamp()
        _isReamping.value = false
        Log.d(TAG, "Re-amp stopped")
    }

    /** Show the session library panel. */
    fun showSessionLibrary() {
        refreshSessions()
        _isSessionLibraryVisible.value = true
    }

    /** Hide the session library panel. */
    fun hideSessionLibrary() {
        _isSessionLibraryVisible.value = false
    }

    /**
     * Delete a saved session and refresh the session list.
     *
     * @param id UUID of the session to delete.
     */
    fun deleteSession(id: String) {
        sessionManager.deleteSession(id)
        refreshSessions()
    }

    /**
     * Toggle the favorite status of a saved session and refresh the list.
     *
     * @param id UUID of the session to toggle.
     */
    fun toggleSessionFavorite(id: String) {
        sessionManager.toggleFavorite(id)
        refreshSessions()
    }

    /**
     * Reload the session list from disk.
     *
     * Updates [savedSessions] with the latest data from the filesystem.
     */
    fun refreshSessions() {
        _savedSessions.value = sessionManager.getAllSessions()
    }

    /**
     * Get the session directory for WAV file access (export, share).
     *
     * @param sessionId UUID of the session.
     * @return Session directory [java.io.File].
     */
    fun getSessionDir(sessionId: String): java.io.File {
        return sessionManager.getSessionDir(sessionId)
    }

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * Initialize the looper delegate.
     *
     * Loads the saved session list from disk. Called once from
     * AudioViewModel's init block.
     */
    fun initialize() {
        refreshSessions()
    }

    // =========================================================================
    // Private Helpers
    // =========================================================================

    /**
     * Ensure the native looper buffers are initialized.
     *
     * Called lazily before any transport command or visibility change.
     * Safe to call multiple times -- the native engine handles re-init
     * gracefully.
     */
    private fun ensureInitialized() {
        if (!isInitialized) {
            val success = audioEngine.looperInit(DEFAULT_MAX_DURATION_SECONDS)
            if (success) {
                isInitialized = true
                // Push current settings to the native engine
                audioEngine.setMetronomeBPM(_bpm.value)
                audioEngine.setTimeSignature(_beatsPerBar.value, _beatUnit.value)
                audioEngine.setQuantizedMode(_isQuantizedMode.value)
                audioEngine.setQuantizedBars(_barCount.value)
                audioEngine.setCountInEnabled(_isCountInEnabled.value)
                audioEngine.looperSetVolume(_loopVolume.value)
                audioEngine.setMetronomeVolume(_metronomeVolume.value)
                audioEngine.setMetronomeActive(_isMetronomeActive.value)
                Log.d(TAG, "Looper initialized ($DEFAULT_MAX_DURATION_SECONDS s buffer)")
            } else {
                Log.e(TAG, "Failed to initialize looper buffers")
            }
        }
    }
}
