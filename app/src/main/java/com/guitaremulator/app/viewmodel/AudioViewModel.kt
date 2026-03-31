package com.guitaremulator.app.viewmodel

import android.app.Application
import android.content.Intent
import android.media.AudioManager
import android.net.Uri
import android.os.Build
import android.os.PowerManager
import android.provider.Settings
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.guitaremulator.app.audio.AudioEngine
import com.guitaremulator.app.audio.AudioFocusManager
import com.guitaremulator.app.audio.IAudioEngine
import com.guitaremulator.app.audio.AudioProcessingService
import com.guitaremulator.app.audio.InputSourceManager
import com.guitaremulator.app.data.EffectRegistry
import com.guitaremulator.app.data.EffectState
import com.guitaremulator.app.data.Preset
import com.guitaremulator.app.data.PresetCategory
import com.guitaremulator.app.data.LooperSessionManager
import com.guitaremulator.app.data.PresetManager
import com.guitaremulator.app.data.UserPreferencesRepository
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch

/**
 * Tuner data read from the native Tuner effect at index 14.
 *
 * All fields are read-only and updated via polling. The tuner runs
 * continuously in the signal chain regardless of other effects.
 *
 * @property frequency Detected fundamental frequency in Hz (0 if no signal).
 * @property noteName Musical note name string (C, C#, D, ..., B).
 * @property centsDeviation Pitch deviation from nearest semitone (-50..+50 cents).
 * @property confidence Detection confidence in [0.0, 1.0]. Below 0.3 indicates
 *     unreliable detection (noise, no signal, or polyphonic input).
 */
data class TunerData(
    val frequency: Float = 0f,
    val noteName: String = "--",
    val centsDeviation: Float = 0f,
    val confidence: Float = 0f
)

/**
 * UI state for a single effect in the signal chain.
 *
 * Mirrors the native effect state and is kept in sync by the ViewModel
 * when the user makes changes or a preset is applied.
 *
 * @property index Position in the signal chain (0..20).
 * @property name Display name (from [EffectRegistry]).
 * @property type C++ type string (from [EffectRegistry]).
 * @property enabled Whether the effect is active.
 * @property wetDryMix Current wet/dry mix in [0.0, 1.0].
 * @property parameters Current parameter values keyed by parameter ID.
 * @property isReadOnly True for the Tuner effect.
 */
@androidx.compose.runtime.Immutable
data class EffectUiState(
    val index: Int,
    val name: String,
    val type: String,
    val enabled: Boolean,
    val wetDryMix: Float = 1.0f,
    val parameters: Map<Int, Float> = emptyMap(),
    val isReadOnly: Boolean = false
)

/**
 * ViewModel managing the audio engine state, preset system, and effect
 * parameters, bridging between the UI layer and the native audio pipeline.
 *
 * Responsibilities are split across delegate classes for maintainability:
 *   - [ABComparisonDelegate]: A/B comparison mode
 *   - [NeuralModelDelegate]: Neural amp model (.nam) loading
 *   - [CabinetIRDelegate]: User cabinet IR (WAV) loading
 *   - [PresetDelegate]: Preset save/load/import/export
 *
 * This class retains core responsibilities:
 *   - Owns the [AudioEngine] JNI bridge instance
 *   - Manages audio focus via [AudioFocusManager]
 *   - Monitors input source changes via [InputSourceManager]
 *   - Maintains UI state for all effects in the signal chain
 *   - Polls tuner data from the native Tuner effect (index 14)
 *   - Exposes observable state via [StateFlow] for Compose consumption
 *   - Polls native level meters and publishes updates to the UI
 *   - Manages the foreground service lifecycle
 *
 * Uses [AndroidViewModel] to access the application context needed for
 * system services (AudioManager, starting services, file storage).
 */
class AudioViewModel @JvmOverloads constructor(
    application: Application,
    private val audioEngine: IAudioEngine = AudioEngine(),
    private val presetManager: PresetManager = PresetManager(application),
    private val userPreferencesRepository: UserPreferencesRepository = UserPreferencesRepository(application),
    audioFocusManagerOverride: AudioFocusManager? = null
) : AndroidViewModel(application) {

    // =========================================================================
    // Engine State
    // =========================================================================

    /** Whether the audio engine is currently running. */
    private val _isRunning = MutableStateFlow(false)
    val isRunning: StateFlow<Boolean> = _isRunning.asStateFlow()

    /** Engine error message shown to user (empty = no error). */
    private val _engineError = MutableStateFlow("")
    val engineError: StateFlow<String> = _engineError.asStateFlow()

    /** The currently active input source. */
    private val _currentInputSource = MutableStateFlow(InputSourceManager.InputSource.PHONE_MIC)
    val currentInputSource: StateFlow<InputSourceManager.InputSource> =
        _currentInputSource.asStateFlow()

    /** Current RMS input level in [0.0, 1.0] for UI metering display. */
    private val _inputLevel = MutableStateFlow(0f)
    val inputLevel: StateFlow<Float> = _inputLevel.asStateFlow()

    /** Current RMS output level in [0.0, 1.0] for UI metering display. */
    private val _outputLevel = MutableStateFlow(0f)
    val outputLevel: StateFlow<Float> = _outputLevel.asStateFlow()

    /** Current spectrum data: 64 dB-scaled bins in [-90, 0] for visualization. */
    private val _spectrumData = MutableStateFlow(FloatArray(64) { -90f })
    val spectrumData: StateFlow<FloatArray> = _spectrumData.asStateFlow()

    /** Device native sample rate in Hz (0 if engine not running). */
    private val _sampleRate = MutableStateFlow(0)
    val sampleRate: StateFlow<Int> = _sampleRate.asStateFlow()

    /** Audio buffer size in frames (0 if engine not running). */
    private val _framesPerBuffer = MutableStateFlow(0)
    val framesPerBuffer: StateFlow<Int> = _framesPerBuffer.asStateFlow()

    /** Estimated round-trip latency in milliseconds. */
    private val _estimatedLatencyMs = MutableStateFlow(0f)
    val estimatedLatencyMs: StateFlow<Float> = _estimatedLatencyMs.asStateFlow()

    /** Audio buffer size multiplier (1..8). Higher = more latency, fewer glitches. */
    private val _bufferMultiplier = MutableStateFlow(2)
    val bufferMultiplier: StateFlow<Int> = _bufferMultiplier.asStateFlow()

    /** Master input gain applied before effects (0.0..4.0, 1.0 = unity). */
    private val _inputGain = MutableStateFlow(1.0f)
    val inputGain: StateFlow<Float> = _inputGain.asStateFlow()

    /** Master output gain applied after effects (0.0..4.0, 1.0 = unity). */
    private val _outputGain = MutableStateFlow(1.0f)
    val outputGain: StateFlow<Float> = _outputGain.asStateFlow()

    /** Global bypass: when true, audio passes through without processing. */
    private val _globalBypass = MutableStateFlow(false)
    val globalBypass: StateFlow<Boolean> = _globalBypass.asStateFlow()

    /** Whether the Activity is in the foreground (STARTED or above). */
    private val _isInForeground = MutableStateFlow(true)

    /** Whether the Settings screen is currently visible (polling not needed). */
    private val _isSettingsVisible = MutableStateFlow(false)

    // =========================================================================
    // Effect State
    // =========================================================================

    /** UI state for all effects in the signal chain. */
    private val _effectStates = MutableStateFlow<List<EffectUiState>>(emptyList())
    val effectStates: StateFlow<List<EffectUiState>> = _effectStates.asStateFlow()

    /**
     * Current processing order of effects (list of physical effect indices).
     * Identity order [0,1,2,...,20] is the default. The UI should display
     * effects in this order for the pedal chain.
     */
    // Default order places TubeScreamer (31) between Boost (3) and Overdrive (4)
    private val _effectOrder = MutableStateFlow(
        listOf(0, 1, 2, 3, 31, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
               16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30)
    )
    val effectOrder: StateFlow<List<Int>> = _effectOrder.asStateFlow()

    // =========================================================================
    // Tuner State
    // =========================================================================

    /** Current tuner readings polled from the native Tuner effect. */
    private val _tunerData = MutableStateFlow(TunerData())
    val tunerData: StateFlow<TunerData> = _tunerData.asStateFlow()

    // =========================================================================
    // Delegates
    // =========================================================================

    private val abDelegate = ABComparisonDelegate(
        captureState = ::captureCurrentSnapshot,
        applyState = ::applySnapshot
    )

    private val neuralModelDelegate = NeuralModelDelegate(
        audioEngine = audioEngine,
        coroutineScope = viewModelScope,
        getApplication = { getApplication() },
        enableEffectIfDisabled = ::enableEffectIfDisabled
    )

    private val cabinetIRDelegate = CabinetIRDelegate(
        audioEngine = audioEngine,
        coroutineScope = viewModelScope,
        getApplication = { getApplication() },
        setEffectParameter = ::setEffectParameter,
        enableEffectIfDisabled = ::enableEffectIfDisabled
    )

    private val presetDelegate = PresetDelegate(
        presetManager = presetManager,
        userPreferencesRepository = userPreferencesRepository,
        audioEngine = audioEngine,
        coroutineScope = viewModelScope,
        getApplication = { getApplication() },
        isRunning = { _isRunning.value },
        onPresetLoaded = ::updateEffectStatesFromPreset,
        getCurrentEffectStates = { _effectStates.value },
        getInputGain = { _inputGain.value },
        getOutputGain = { _outputGain.value },
        setInputGain = { _inputGain.value = it },
        setOutputGain = { _outputGain.value = it },
        getCurrentEffectOrder = { _effectOrder.value }
    )

    private val looperSessionManager = LooperSessionManager(application)

    /**
     * Delegate managing all looper state, transport commands, settings,
     * and session persistence. Exposed as a public val so the UI layer
     * can observe its StateFlows directly.
     */
    val looperDelegate = LooperDelegate(
        audioEngine = audioEngine,
        sessionManager = looperSessionManager,
        onLoadPreset = { presetId ->
            // Synchronous preset load + apply so the chain is fully set
            // before re-amp audio starts (avoids async race condition).
            val preset = presetManager.loadPreset(presetId)
            if (preset != null) {
                presetManager.applyPreset(preset, audioEngine)
            }
        }
    )

    // =========================================================================
    // Delegated A/B Comparison State & API
    // =========================================================================

    val abMode: StateFlow<Boolean> get() = abDelegate.abMode
    val activeSlot: StateFlow<Char> get() = abDelegate.activeSlot
    fun enterAbMode() = abDelegate.enterAbMode()
    fun toggleAB() = abDelegate.toggleAB()
    fun exitAbMode(keep: Char) = abDelegate.exitAbMode(keep)

    // =========================================================================
    // Delegated Neural Amp Model State & API
    // =========================================================================

    val neuralModelName: StateFlow<String> get() = neuralModelDelegate.neuralModelName
    val isLoadingNeuralModel: StateFlow<Boolean> get() = neuralModelDelegate.isLoadingNeuralModel
    val neuralModelError: StateFlow<String> get() = neuralModelDelegate.neuralModelError
    val isNeuralModelLoaded: StateFlow<Boolean> get() = neuralModelDelegate.isNeuralModelLoaded
    fun loadNeuralModel(uri: Uri, fileName: String = "Neural Model") =
        neuralModelDelegate.loadNeuralModel(uri, fileName)
    fun clearNeuralModel() = neuralModelDelegate.clearNeuralModel()
    fun clearNeuralModelError() = neuralModelDelegate.clearNeuralModelError()

    // =========================================================================
    // Delegated Cabinet IR State & API
    // =========================================================================

    val userIRName: StateFlow<String> get() = cabinetIRDelegate.userIRName
    val isLoadingIR: StateFlow<Boolean> get() = cabinetIRDelegate.isLoadingIR
    val irLoadError: StateFlow<String> get() = cabinetIRDelegate.irLoadError
    fun loadUserCabinetIR(uri: Uri, fileName: String = "Custom IR") =
        cabinetIRDelegate.loadUserCabinetIR(uri, fileName)
    fun clearIRLoadError() = cabinetIRDelegate.clearIRLoadError()

    // =========================================================================
    // Delegated Preset State & API
    // =========================================================================

    val currentPresetName: StateFlow<String> get() = presetDelegate.currentPresetName
    val currentPresetId: StateFlow<String> get() = presetDelegate.currentPresetId
    val presets: StateFlow<List<Preset>> get() = presetDelegate.presets
    val presetIOStatus: StateFlow<String> get() = presetDelegate.presetIOStatus
    fun loadPreset(presetId: String) = presetDelegate.loadPreset(presetId)
    fun saveCurrentAsPreset(name: String, category: PresetCategory = PresetCategory.CUSTOM) =
        presetDelegate.saveCurrentAsPreset(name, category)
    fun renamePreset(presetId: String, newName: String) = presetDelegate.renamePreset(presetId, newName)
    fun overwritePreset(presetId: String) = presetDelegate.overwritePreset(presetId)
    fun deletePreset(presetId: String) = presetDelegate.deletePreset(presetId)
    fun refreshPresetList() = presetDelegate.refreshPresetList()
    fun exportCurrentPreset(uri: Uri) = presetDelegate.exportCurrentPreset(uri)
    fun importPreset(uri: Uri) = presetDelegate.importPreset(uri)
    fun getExportFileName(): String = presetDelegate.getExportFileName()
    fun clearPresetIOStatus() = presetDelegate.clearPresetIOStatus()
    fun togglePresetFavorite(presetId: String) = presetDelegate.togglePresetFavorite(presetId)

    // =========================================================================
    // Private Members
    // =========================================================================

    private val audioFocusManager: AudioFocusManager
    private val inputSourceManager: InputSourceManager

    /**
     * Note name lookup table. Index corresponds to the note value returned
     * by the native Tuner effect (0=C, 1=Db, ..., 11=B).
     * Uses flat notation (Db, Eb, Gb, Ab, Bb) instead of sharps because
     * guitarists are more familiar with flats in common tunings (Eb, Ab).
     */
    private val noteNames = arrayOf(
        "C", "Db", "D", "Eb", "E", "F",
        "Gb", "G", "Ab", "A", "Bb", "B"
    )

    /** True when user has manually overridden the input source. */
    private var manualInputOverride = false

    /** Timestamp when the engine was last started (for timeout detection). */
    private var engineStartTimeMs = 0L

    init {
        val audioManager =
            application.getSystemService(android.content.Context.AUDIO_SERVICE) as AudioManager

        audioFocusManager = audioFocusManagerOverride ?: AudioFocusManager(audioManager)
        audioFocusManager.setListener(object : AudioFocusManager.Listener {
            override fun onAudioFocusGained() {
                // Resume if we were running before transient loss
                if (_isRunning.value && !audioEngine.isRunning()) {
                    audioEngine.startEngine()
                }
            }

            override fun onAudioFocusLost() {
                // Permanent loss: stop everything
                stopEngine()
            }

            override fun onAudioFocusLostTransient() {
                // Temporary loss: stop the native engine but keep our state
                // so we can resume when focus returns
                audioEngine.stopEngine()
            }

            override fun onAudioFocusLostTransientCanDuck() {
                // Could reduce output volume here; for now treat as transient loss
                audioEngine.stopEngine()
            }
        })

        inputSourceManager = InputSourceManager(application)
        inputSourceManager.setListener(object : InputSourceManager.Listener {
            override fun onInputSourceChanged(source: InputSourceManager.InputSource) {
                // Skip auto-detection if user has manually overridden
                if (manualInputOverride) return
                _currentInputSource.value = source

                // Restart engine using ViewModel methods which properly
                // re-apply ALL state (effects, gains, buffer multiplier,
                // device IDs, effect order) to the new native engine instance.
                // Previously used raw JNI calls which destroyed the engine
                // and created a fresh one with default settings, losing every
                // user setting and causing "underwater" sound.
                if (_isRunning.value) {
                    stopEngine()
                    startEngine()
                }
            }
        })
        inputSourceManager.startMonitoring()

        // Load factory presets and restore user preferences
        viewModelScope.launch {
            presetManager.loadFactoryPresets()
            refreshPresetList()

            // Initialize effect UI states from registry defaults
            initializeEffectStates()

            // Restore persisted user preferences
            try {
                val prefs = userPreferencesRepository.userPreferences.first()

                // Restore input source
                val savedSource = InputSourceManager.InputSource.entries
                    .find { it.nativeType == prefs.inputSourceType }
                if (savedSource != null && savedSource != InputSourceManager.InputSource.PHONE_MIC) {
                    manualInputOverride = true
                    _currentInputSource.value = savedSource
                }

                // Restore buffer multiplier
                _bufferMultiplier.value = prefs.bufferMultiplier

                // Restore master gains
                _inputGain.value = prefs.inputGain
                _outputGain.value = prefs.outputGain

                // Don't restore last preset on startup — start with clean passthrough.
                // All effects default to disabled (Effect base class enabled_{false}),
                // giving the user a clean signal until they choose a preset.
            } catch (_: Exception) {
                // DataStore read failure: continue with defaults
            }

            // Initialize looper delegate (loads saved session list from disk)
            looperDelegate.initialize()
        }
    }

    // =========================================================================
    // Engine Lifecycle
    // =========================================================================

    /**
     * Start the audio engine and foreground service.
     *
     * Requests audio focus, starts the native engine, begins the foreground
     * service, and launches coroutines to poll level meters and tuner data.
     */
    fun startEngine() {
        if (_isRunning.value) return

        // Request audio focus before starting
        if (!audioFocusManager.requestFocus()) {
            return // Focus denied -- another app has priority
        }

        _engineError.value = "" // Clear any previous error

        // Set input source and device IDs BEFORE starting the engine.
        // These are used by start() to configure the correct audio streams.
        // They write to atomics on the existing gEngine (kept alive after stop)
        // or silently no-op on cold start (acceptable -- start() uses defaults).
        audioEngine.setInputSource(_currentInputSource.value.nativeType)
        audioEngine.setBufferMultiplier(_bufferMultiplier.value)
        applyDeviceIds()

        val started = audioEngine.startEngine()
        if (!started) {
            audioFocusManager.abandonFocus()
            _engineError.value = "Failed to start audio engine"
            return
        }

        // Apply ALL state AFTER startEngine() so gEngine is guaranteed to exist.
        // Previously these calls were made before startEngine(), but gEngine
        // was null on cold start (destroyed by stopEngine()), so every JNI
        // setter silently no-oped. This was a latent bug masked by the old
        // enabled_{true} default -- exposed when Sprint 18 P5 changed to
        // enabled_{false}, causing all 7 core effects to stay disabled.
        audioEngine.setInputGain(_inputGain.value)
        audioEngine.setOutputGain(_outputGain.value)
        audioEngine.setGlobalBypass(_globalBypass.value)
        audioEngine.setBufferMultiplier(_bufferMultiplier.value)
        if (_effectStates.value.isNotEmpty()) {
            applyCurrentEffectStatesToEngine()
        }

        _isRunning.value = true
        engineStartTimeMs = System.currentTimeMillis()

        // Start the foreground service to keep the engine alive in background
        val context = getApplication<android.app.Application>()
        val serviceIntent = Intent(context, AudioProcessingService::class.java)
        context.startForegroundService(serviceIntent)

        // Query device info once after engine starts
        _sampleRate.value = audioEngine.getSampleRate()
        _framesPerBuffer.value = audioEngine.getFramesPerBuffer()
        _estimatedLatencyMs.value = audioEngine.getEstimatedLatencyMs()

        // Poll native level meters and tuner data, publish to UI StateFlows.
        // This runs at ~30 FPS which is sufficient for visual metering.
        // Also detects unexpected native engine stops (USB disconnect, stream error)
        // and surfaces them to the UI via _engineError.
        viewModelScope.launch {
            while (isActive && _isRunning.value) {
                // Engine health check always runs (lightweight atomic read)
                // to detect USB disconnect or stream errors regardless of UI state.
                if (!audioEngine.isRunning()) {
                    _isRunning.value = false
                    val uptimeMs = System.currentTimeMillis() - engineStartTimeMs
                    _engineError.value = getEngineStopReason(uptimeMs)
                    _inputLevel.value = 0f
                    _outputLevel.value = 0f
                    break
                }
                // Only poll meters, spectrum, and tuner when the UI needs them.
                // Skip when Settings screen is visible or app is in background:
                // eliminates ~6 JNI calls + FloatArray(64) allocation per frame,
                // reducing CPU/GC pressure that causes audio thread starvation.
                if (_isInForeground.value && !_isSettingsVisible.value) {
                    _inputLevel.value = audioEngine.getInputLevel()
                    _outputLevel.value = audioEngine.getOutputLevel()
                    _spectrumData.value = audioEngine.getSpectrumData()
                    // Only poll tuner data when the tuner is active (user has
                    // the tuner editor open). Saves 4 JNI calls per frame.
                    if (audioEngine.isTunerActive()) {
                        pollTunerData()
                    }
                    // Poll looper state when visible or when a loop is active
                    // (to track state transitions even during background playback).
                    looperDelegate.pollState()
                } else if (_inputLevel.value != 0f) {
                    // Zero stale meter data so UI doesn't show frozen readings
                    _inputLevel.value = 0f
                    _outputLevel.value = 0f
                }
                delay(33) // ~30 FPS meter refresh
            }
        }
    }

    /**
     * Stop the audio engine and foreground service.
     *
     * Stops the native engine, abandons audio focus, stops the service,
     * and resets level meters to zero.
     */
    fun stopEngine() {
        if (!_isRunning.value) return

        _isRunning.value = false
        audioEngine.stopEngine()
        audioFocusManager.abandonFocus()

        // Stop the foreground service
        val context = getApplication<android.app.Application>()
        val serviceIntent = Intent(context, AudioProcessingService::class.java)
        context.stopService(serviceIntent)

        // Reset meters, tuner, device info, and neural model state.
        // The native engine is destroyed on stop, so any loaded neural model
        // is gone. Reset Kotlin-side state to match.
        _inputLevel.value = 0f
        _outputLevel.value = 0f
        _spectrumData.value = FloatArray(64) { -90f }
        _tunerData.value = TunerData()
        _sampleRate.value = 0
        _framesPerBuffer.value = 0
        _estimatedLatencyMs.value = 0f
        neuralModelDelegate.resetOnEngineStop()
    }

    /** Dismiss the engine error message. */
    fun clearEngineError() {
        _engineError.value = ""
    }

    /**
     * Manually set the input source, overriding auto-detection.
     * Auto-detection callbacks are suppressed until the engine is restarted.
     *
     * @param source The input source to use.
     */
    fun setInputSource(source: InputSourceManager.InputSource) {
        manualInputOverride = true
        _currentInputSource.value = source

        // If the engine is running, restart using the ViewModel methods which
        // properly re-apply ALL state (gains, effects, buffer multiplier,
        // device IDs, effect order) to the new native engine instance.
        // The raw JNI stopEngine() destroys gEngine entirely, so a raw
        // startEngine() would create a fresh engine with default state,
        // losing every user setting.
        if (_isRunning.value) {
            stopEngine()
            startEngine()
        } else {
            // Engine not running -- just stage the source for next start.
            audioEngine.setInputSource(source.nativeType)
            applyDeviceIds()
        }

        // Persist input source selection
        viewModelScope.launch {
            userPreferencesRepository.setInputSourceType(source.nativeType)
        }
    }

    /**
     * Set the audio buffer size multiplier (1..8).
     *
     * Higher values increase latency but reduce the chance of audio glitches.
     * Persists the setting and applies it to the running engine if active.
     *
     * @param multiplier Buffer size multiplier (clamped to 1..8).
     */
    fun setBufferMultiplier(multiplier: Int) {
        val clamped = multiplier.coerceIn(1, 8)
        _bufferMultiplier.value = clamped
        audioEngine.setBufferMultiplier(clamped)
        viewModelScope.launch {
            userPreferencesRepository.setBufferMultiplier(clamped)
        }
    }

    /**
     * Called by the Activity when lifecycle transitions between foreground/background.
     * Pauses the JNI polling loop in background to reduce CPU/GC overhead.
     */
    fun setInForeground(foreground: Boolean) {
        _isInForeground.value = foreground
    }

    /**
     * Called by the Activity when navigating to/from the Settings screen.
     * Pauses the JNI polling loop since Settings doesn't need level meters,
     * spectrum analyzer, or tuner data.
     */
    fun setSettingsVisible(visible: Boolean) {
        _isSettingsVisible.value = visible
    }

    /**
     * Activate or deactivate the tuner's pitch analysis.
     *
     * The tuner is off by default to save CPU — its 8192-point FFT runs on
     * every audio callback when active. Call with true when the user opens
     * the tuner effect editor, false when they close it.
     *
     * @param active true to start pitch analysis, false to stop.
     */
    fun setTunerActive(active: Boolean) {
        audioEngine.setTunerActive(active)
    }

    // =========================================================================
    // Master Gain & Bypass
    // =========================================================================

    /**
     * Set the master input gain applied before effects processing.
     *
     * @param gain Linear gain (0.0 = silence, 1.0 = unity, 4.0 = +12dB max).
     */
    fun setInputGain(gain: Float) {
        val clamped = gain.coerceIn(0.0f, 4.0f)
        _inputGain.value = clamped
        audioEngine.setInputGain(clamped)
        viewModelScope.launch {
            userPreferencesRepository.setInputGain(clamped)
        }
    }

    /**
     * Set the master output gain applied after effects processing.
     *
     * @param gain Linear gain (0.0 = silence, 1.0 = unity, 4.0 = +12dB max).
     */
    fun setOutputGain(gain: Float) {
        val clamped = gain.coerceIn(0.0f, 4.0f)
        _outputGain.value = clamped
        audioEngine.setOutputGain(clamped)
        viewModelScope.launch {
            userPreferencesRepository.setOutputGain(clamped)
        }
    }

    /**
     * Toggle global bypass: when active, audio passes through without
     * any effects processing for latency-free monitoring or A/B comparison.
     */
    fun toggleGlobalBypass() {
        val newState = !_globalBypass.value
        _globalBypass.value = newState
        audioEngine.setGlobalBypass(newState)
    }

    // =========================================================================
    // Effect Control
    // =========================================================================

    /**
     * Reorder an effect in the processing chain by moving it from one
     * logical position to another.
     *
     * @param fromPosition Logical position to move from (0-based).
     * @param toPosition Logical position to move to (0-based).
     */
    fun reorderEffect(fromPosition: Int, toPosition: Int) {
        val order = _effectOrder.value.toMutableList()
        if (fromPosition !in order.indices || toPosition !in order.indices) return
        if (fromPosition == toPosition) return

        val item = order.removeAt(fromPosition)
        order.add(toPosition, item)

        if (audioEngine.setEffectOrder(order.toIntArray())) {
            _effectOrder.value = order
        }
    }

    /**
     * Toggle an effect's enabled state.
     *
     * @param effectIndex Zero-based index of the effect (0..20).
     */
    fun toggleEffect(effectIndex: Int) {
        val states = _effectStates.value.toMutableList()
        if (effectIndex !in states.indices) return

        val current = states[effectIndex]
        val newEnabled = !current.enabled
        states[effectIndex] = current.copy(enabled = newEnabled)
        _effectStates.value = states

        // Apply to native engine if running
        if (_isRunning.value) {
            audioEngine.setEffectEnabled(effectIndex, newEnabled)
        }
    }

    /**
     * Set a parameter value on an effect.
     *
     * @param effectIndex Zero-based effect index.
     * @param paramId Effect-specific parameter ID.
     * @param value New parameter value.
     */
    fun setEffectParameter(effectIndex: Int, paramId: Int, value: Float) {
        val states = _effectStates.value.toMutableList()
        if (effectIndex !in states.indices) return

        val current = states[effectIndex]
        val newParams = current.parameters.toMutableMap()
        newParams[paramId] = value
        states[effectIndex] = current.copy(parameters = newParams)
        _effectStates.value = states

        // Apply to native engine if running
        if (_isRunning.value) {
            audioEngine.setEffectParameter(effectIndex, paramId, value)
        }
    }

    /**
     * Set the wet/dry mix for an effect.
     *
     * @param effectIndex Zero-based effect index.
     * @param mix New mix value in [0.0, 1.0].
     */
    fun setEffectWetDryMix(effectIndex: Int, mix: Float) {
        val states = _effectStates.value.toMutableList()
        if (effectIndex !in states.indices) return

        states[effectIndex] = states[effectIndex].copy(wetDryMix = mix.coerceIn(0f, 1f))
        _effectStates.value = states

        if (_isRunning.value) {
            audioEngine.setEffectWetDryMix(effectIndex, mix.coerceIn(0f, 1f))
        }
    }

    // =========================================================================
    // Battery Optimization
    // =========================================================================

    /**
     * Check if the app is exempt from battery optimization (Doze mode).
     *
     * @return true if already exempt from battery optimization.
     */
    fun isBatteryOptimizationExempt(): Boolean {
        val context = getApplication<Application>()
        val powerManager = context.getSystemService(android.content.Context.POWER_SERVICE)
            as PowerManager
        return powerManager.isIgnoringBatteryOptimizations(context.packageName)
    }

    /**
     * Create an intent to request battery optimization exemption.
     *
     * @return Intent to launch the battery optimization exemption dialog.
     */
    fun createBatteryOptimizationIntent(): Intent {
        val context = getApplication<Application>()
        return Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS).apply {
            data = Uri.parse("package:${context.packageName}")
        }
    }

    override fun onCleared() {
        super.onCleared()
        stopEngine()
        inputSourceManager.stopMonitoring()
    }

    // =========================================================================
    // Private Helpers
    // =========================================================================

    /**
     * Set input and output device IDs on the native engine based on the
     * current input source. When a USB audio interface is detected, both
     * streams are explicitly routed to the USB device. When using the
     * phone mic or analog adapter, device IDs are reset to 0 (system default).
     *
     * This ensures processed audio routes back through the USB interface
     * (e.g., to iRig headphone/amp output) instead of the phone speaker.
     */
    private fun applyDeviceIds() {
        if (_currentInputSource.value == InputSourceManager.InputSource.USB_AUDIO) {
            val inputId = inputSourceManager.getUsbInputDeviceId()
            val outputId = inputSourceManager.getUsbOutputDeviceId()
            audioEngine.setInputDeviceId(inputId)
            audioEngine.setOutputDeviceId(outputId)
        } else {
            // Non-USB: let Android pick the default devices
            audioEngine.setInputDeviceId(0)
            audioEngine.setOutputDeviceId(0)
        }
    }

    /**
     * Enable an effect only if it is currently disabled.
     * Used by delegates that need to auto-enable an effect after loading
     * a resource (neural model, cabinet IR).
     *
     * @param effectIndex Zero-based index of the effect (0..20).
     */
    private fun enableEffectIfDisabled(effectIndex: Int) {
        val states = _effectStates.value
        if (effectIndex in states.indices && !states[effectIndex].enabled) {
            toggleEffect(effectIndex)
        }
    }

    /**
     * Capture the current audio configuration as an immutable snapshot.
     * Reads from ViewModel state (not engine) for instant capture.
     */
    private fun captureCurrentSnapshot(): AudioSnapshot {
        val effects = _effectStates.value.map { uiState ->
            EffectState(
                effectType = uiState.type,
                enabled = uiState.enabled,
                wetDryMix = uiState.wetDryMix,
                parameters = uiState.parameters
            )
        }
        val currentOrder = _effectOrder.value
        val identityOrder = (0 until EffectRegistry.EFFECT_COUNT).toList()
        return AudioSnapshot(
            effectStates = effects,
            effectOrder = if (currentOrder != identityOrder) currentOrder else null,
            inputGain = _inputGain.value,
            outputGain = _outputGain.value,
            globalBypass = _globalBypass.value
        )
    }

    /**
     * Apply a snapshot to the engine and update all ViewModel state.
     * Pushes effect parameters, order, and gains via JNI.
     */
    private fun applySnapshot(snapshot: AudioSnapshot) {
        // Update gains
        _inputGain.value = snapshot.inputGain
        _outputGain.value = snapshot.outputGain
        _globalBypass.value = snapshot.globalBypass
        audioEngine.setInputGain(snapshot.inputGain)
        audioEngine.setOutputGain(snapshot.outputGain)
        audioEngine.setGlobalBypass(snapshot.globalBypass)

        // Update effect states in ViewModel
        val newStates = EffectRegistry.effects.map { info ->
            val effectState = snapshot.effectStates.find { it.effectType == info.type }
            if (effectState != null) {
                EffectUiState(
                    index = info.index,
                    name = info.displayName,
                    type = effectState.effectType,
                    enabled = effectState.enabled,
                    wetDryMix = effectState.wetDryMix,
                    parameters = effectState.parameters,
                    isReadOnly = info.isReadOnly
                )
            } else {
                _effectStates.value.getOrNull(info.index) ?: EffectUiState(
                    index = info.index,
                    name = info.displayName,
                    type = info.type,
                    enabled = info.enabledByDefault,
                    wetDryMix = 1.0f,
                    parameters = emptyMap(),
                    isReadOnly = info.isReadOnly
                )
            }
        }
        _effectStates.value = newStates

        // Update effect order
        _effectOrder.value = snapshot.effectOrder
            ?: (0 until EffectRegistry.EFFECT_COUNT).toList()

        // Apply to native engine if running
        if (_isRunning.value) {
            applyCurrentEffectStatesToEngine()
        }
    }

    /**
     * Initialize effect UI states from the [EffectRegistry] defaults.
     *
     * Called once during ViewModel initialization to populate the state
     * list before any preset is loaded.
     */
    private fun initializeEffectStates() {
        val states = EffectRegistry.effects.map { info ->
            val defaultParams = mutableMapOf<Int, Float>()
            for (param in info.params) {
                defaultParams[param.id] = param.defaultValue
            }

            EffectUiState(
                index = info.index,
                name = info.displayName,
                type = info.type,
                enabled = info.enabledByDefault,
                wetDryMix = 1.0f,
                parameters = defaultParams,
                isReadOnly = info.isReadOnly
            )
        }
        _effectStates.value = states
    }

    /**
     * Update the UI effect states from a loaded preset.
     *
     * Merges preset data with registry metadata (names, read-only flags)
     * to produce complete [EffectUiState] entries.
     */
    private fun updateEffectStatesFromPreset(preset: Preset) {
        val states = EffectRegistry.effects.map { info ->
            val effectState = preset.effects.find { it.effectType == info.type }
            if (effectState != null) {
                EffectUiState(
                    index = info.index,
                    name = info.displayName,
                    type = effectState.effectType,
                    enabled = effectState.enabled,
                    wetDryMix = effectState.wetDryMix,
                    parameters = effectState.parameters,
                    isReadOnly = info.isReadOnly
                )
            } else {
                // Effect not in preset -- use registry defaults
                val defaultParams = mutableMapOf<Int, Float>()
                for (param in info.params) {
                    defaultParams[param.id] = param.defaultValue
                }
                EffectUiState(
                    index = info.index,
                    name = info.displayName,
                    type = info.type,
                    enabled = info.enabledByDefault,
                    wetDryMix = 1.0f,
                    parameters = defaultParams,
                    isReadOnly = info.isReadOnly
                )
            }
        }
        _effectStates.value = states

        // Restore effect processing order from preset (or identity default)
        _effectOrder.value = preset.effectOrder
            ?: (0 until EffectRegistry.EFFECT_COUNT).toList()
    }

    /**
     * Apply current UI effect states to the native engine.
     *
     * Called when the engine starts to sync the native state with
     * whatever the UI is showing (from default init or a loaded preset).
     */
    private fun applyCurrentEffectStatesToEngine() {
        for (state in _effectStates.value) {
            audioEngine.setEffectEnabled(state.index, state.enabled)
            audioEngine.setEffectWetDryMix(state.index, state.wetDryMix)

            if (!state.isReadOnly) {
                for ((paramId, value) in state.parameters) {
                    audioEngine.setEffectParameter(state.index, paramId, value)
                }
            }
        }

        // Apply the current processing order to the engine
        audioEngine.setEffectOrder(_effectOrder.value.toIntArray())
    }

    /**
     * Poll the native Tuner effect (index 14) for current readings.
     */
    private fun pollTunerData() {
        val frequency = audioEngine.getEffectParameter(14, 0)
        val noteIndex = audioEngine.getEffectParameter(14, 1).toInt()
        val cents = audioEngine.getEffectParameter(14, 2)
        val confidence = audioEngine.getEffectParameter(14, 3)

        val noteName = if (noteIndex in noteNames.indices && confidence > 0.2f) {
            noteNames[noteIndex]
        } else {
            "--"
        }

        _tunerData.value = TunerData(
            frequency = frequency,
            noteName = noteName,
            centsDeviation = cents,
            confidence = confidence
        )

        // Update effectStates so the EffectEditorScreen shows live tuner readings.
        // The Tuner is read-only (no user-settable params), so its parameters in
        // _effectStates are otherwise stuck at EffectRegistry defaults (all 0.0f).
        val states = _effectStates.value
        if (states.size > 14) {
            val liveParams = mapOf(
                0 to frequency,
                1 to noteIndex.toFloat(),
                2 to cents,
                3 to confidence
            )
            val updated = states.toMutableList()
            updated[14] = states[14].copy(parameters = liveParams)
            _effectStates.value = updated
        }
    }

    // =========================================================================
    // Engine Error Diagnostics
    // =========================================================================

    /**
     * Determine a user-friendly error message based on engine uptime.
     *
     * @param uptimeMs How long the engine ran before stopping.
     * @return Context-specific error message for the UI.
     */
    private fun getEngineStopReason(uptimeMs: Long): String {
        val fiveAndHalfHoursMs = 5L * 60 * 60 * 1000 + 30 * 60 * 1000

        return when {
            Build.VERSION.SDK_INT >= 35 && uptimeMs >= fiveAndHalfHoursMs ->
                "Android suspended audio after extended use. Tap retry to resume."
            uptimeMs < 5_000 ->
                "Audio stream failed to start. Check your audio device."
            else ->
                "Audio device disconnected. Tap retry to reconnect."
        }
    }
}
