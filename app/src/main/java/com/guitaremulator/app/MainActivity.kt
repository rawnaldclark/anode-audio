package com.guitaremulator.app

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.database.Cursor
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.OpenableColumns
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding



import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Snackbar
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text




import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.flow.filter
import androidx.compose.ui.Modifier
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.compose.ui.graphics.Color

import androidx.core.content.ContextCompat
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import com.guitaremulator.app.ui.drums.DrumMachineScreen
import com.guitaremulator.app.ui.screens.EffectEditorSheet
import com.guitaremulator.app.ui.screens.PedalboardScreen
import com.guitaremulator.app.ui.screens.SettingsScreen
import com.guitaremulator.app.ui.theme.GuitarEmulatorTheme
import com.guitaremulator.app.data.PresetCategory
import com.guitaremulator.app.ui.theme.DesignSystem
import com.guitaremulator.app.viewmodel.AudioViewModel

/** Effect index for the tuner, used to route to TunerOverlay instead of EffectEditorSheet. */
private const val TUNER_INDEX = 14

/**
 * Single activity for the GuitarEmulator app.
 *
 * Uses Jetpack Compose for the entire UI. The activity is configured with
 * configChanges in the manifest to prevent recreation on orientation and
 * keyboard changes, which would interrupt audio processing.
 *
 * Permission handling:
 *   - RECORD_AUDIO: requested at runtime before starting the audio engine
 *   - POST_NOTIFICATIONS: requested on Android 13+ for the foreground service notification
 *
 * The activity delegates all audio state management to [AudioViewModel],
 * which survives configuration changes and manages the native engine lifecycle,
 * preset system, and effect parameter state.
 */
class MainActivity : ComponentActivity() {

    /** Tracks whether we've already requested RECORD_AUDIO once in this session.
     *  Used to distinguish first-time request from permanent denial. */
    private var permissionRequestedOnce = false

    /**
     * Permission launcher for RECORD_AUDIO. When granted, the user can
     * tap Start to begin audio processing. The result is handled but we
     * don't force the user to grant -- they can still browse the UI.
     */
    private val recordAudioPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { granted ->
        permissionRequestedOnce = true
        // If granted after the initial request, nothing else to do --
        // the user will tap Start again and hasRecordAudioPermission() will pass.
    }

    /**
     * Permission launcher for POST_NOTIFICATIONS (Android 13+).
     * The foreground service notification works without this permission,
     * but the user won't see it in the notification shade.
     */
    private val notificationPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { /* Notification permission is not critical -- service works without it */ }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        requestPermissionsIfNeeded()

        setContent {
            GuitarEmulatorTheme {
                val audioViewModel: AudioViewModel = viewModel()

                // Pause JNI polling when app goes to background.
                // The audio engine continues running via the foreground service,
                // but level meters, spectrum analyzer, and tuner updates are
                // unnecessary without a visible UI. This eliminates ~6 JNI calls
                // + FloatArray allocation every 33ms, reducing CPU/GC pressure
                // that causes audio callback deadline misses (crackling).
                val lifecycleOwner = LocalLifecycleOwner.current
                DisposableEffect(lifecycleOwner) {
                    val observer = LifecycleEventObserver { _, event ->
                        when (event) {
                            Lifecycle.Event.ON_START -> {
                                audioViewModel.setInForeground(true)
                            }
                            Lifecycle.Event.ON_STOP -> {
                                audioViewModel.setInForeground(false)
                            }
                            else -> {}
                        }
                    }
                    lifecycleOwner.lifecycle.addObserver(observer)
                    onDispose {
                        lifecycleOwner.lifecycle.removeObserver(observer)
                    }
                }

                // Collect ViewModel state as Compose state with lifecycle awareness.
                // collectAsStateWithLifecycle automatically stops collection when the
                // activity is in the background, saving battery.
                val isRunning by audioViewModel.isRunning.collectAsStateWithLifecycle()
                val inputLevel by audioViewModel.inputLevel.collectAsStateWithLifecycle()
                val outputLevel by audioViewModel.outputLevel.collectAsStateWithLifecycle()
                val inputSource by audioViewModel.currentInputSource.collectAsStateWithLifecycle()
                val currentPresetName by audioViewModel.currentPresetName.collectAsStateWithLifecycle()
                val presets by audioViewModel.presets.collectAsStateWithLifecycle()
                val effectStates by audioViewModel.effectStates.collectAsStateWithLifecycle()
                val effectOrder by audioViewModel.effectOrder.collectAsStateWithLifecycle()
                val tunerData by audioViewModel.tunerData.collectAsStateWithLifecycle()
                val currentPresetId by audioViewModel.currentPresetId.collectAsStateWithLifecycle()
                val engineError by audioViewModel.engineError.collectAsStateWithLifecycle()

                // User cabinet IR state
                val userIRName by audioViewModel.userIRName.collectAsStateWithLifecycle()
                val isLoadingIR by audioViewModel.isLoadingIR.collectAsStateWithLifecycle()
                val irLoadError by audioViewModel.irLoadError.collectAsStateWithLifecycle()

                // Neural amp model state
                val neuralModelName by audioViewModel.neuralModelName.collectAsStateWithLifecycle()
                val isLoadingNeuralModel by audioViewModel.isLoadingNeuralModel.collectAsStateWithLifecycle()
                val neuralModelError by audioViewModel.neuralModelError.collectAsStateWithLifecycle()
                val isNeuralModelLoaded by audioViewModel.isNeuralModelLoaded.collectAsStateWithLifecycle()

                // Preset I/O status snackbar
                val presetIOStatus by audioViewModel.presetIOStatus.collectAsStateWithLifecycle()
                val snackbarHostState = remember { SnackbarHostState() }
                LaunchedEffect(Unit) {
                    snapshotFlow { presetIOStatus }
                        .filter { it.isNotEmpty() }
                        .collectLatest { message ->
                            snackbarHostState.showSnackbar(message)
                            audioViewModel.clearPresetIOStatus()
                        }
                }

                // Track which effect is being edited (-1 = none)
                var editingEffectIndex by remember { mutableIntStateOf(-1) }
                var showSettings by remember { mutableStateOf(false) }
                var showDrums by remember { mutableStateOf(false) }
                var tunerOverlayActive by remember { mutableStateOf(false) }

                // Pause JNI polling on Settings screen: it doesn't show level
                // meters, spectrum, or tuner, so the 30fps JNI calls are wasted
                // CPU that competes with audio processing and causes crackling.
                LaunchedEffect(showSettings) {
                    audioViewModel.setSettingsVisible(showSettings)
                }

                // File picker for loading custom cabinet IR (.wav files)
                val irPickerLauncher = rememberLauncherForActivityResult(
                    ActivityResultContracts.OpenDocument()
                ) { uri: Uri? ->
                    if (uri != null) {
                        val fileName = getFileDisplayName(uri) ?: "Custom IR"
                        audioViewModel.loadUserCabinetIR(uri, fileName)
                    }
                }

                // File picker for loading neural amp model (.nam files)
                // .nam files are JSON text with no standard MIME type, so we
                // accept all file types and let the user select.
                val namPickerLauncher = rememberLauncherForActivityResult(
                    ActivityResultContracts.OpenDocument()
                ) { uri: Uri? ->
                    if (uri != null) {
                        val fileName = getFileDisplayName(uri) ?: "Neural Model"
                        audioViewModel.loadNeuralModel(uri, fileName)
                    }
                }

                // SAF launcher for exporting preset as JSON
                val presetExportLauncher = rememberLauncherForActivityResult(
                    ActivityResultContracts.CreateDocument("application/json")
                ) { uri: Uri? ->
                    if (uri != null) {
                        audioViewModel.exportCurrentPreset(uri)
                    }
                }

                // SAF launcher for importing a preset JSON file
                val presetImportLauncher = rememberLauncherForActivityResult(
                    ActivityResultContracts.OpenDocument()
                ) { uri: Uri? ->
                    if (uri != null) {
                        audioViewModel.importPreset(uri)
                    }
                }

                // Settings data
                val sampleRate by audioViewModel.sampleRate.collectAsStateWithLifecycle()
                val framesPerBuffer by audioViewModel.framesPerBuffer.collectAsStateWithLifecycle()
                val estimatedLatencyMs by audioViewModel.estimatedLatencyMs.collectAsStateWithLifecycle()
                val bufferMultiplier by audioViewModel.bufferMultiplier.collectAsStateWithLifecycle()

                // Spectrum analyzer data
                val spectrumData by audioViewModel.spectrumData.collectAsStateWithLifecycle()

                // A/B comparison state
                val abMode by audioViewModel.abMode.collectAsStateWithLifecycle()
                val activeSlot by audioViewModel.activeSlot.collectAsStateWithLifecycle()

                // Master controls state
                val inputGain by audioViewModel.inputGain.collectAsStateWithLifecycle()
                val outputGain by audioViewModel.outputGain.collectAsStateWithLifecycle()
                val globalBypass by audioViewModel.globalBypass.collectAsStateWithLifecycle()

                // Looper state (collected from LooperDelegate StateFlows)
                val isLooperVisible by audioViewModel.looperDelegate.isLooperVisible.collectAsStateWithLifecycle()
                val looperState by audioViewModel.looperDelegate.looperState.collectAsStateWithLifecycle()
                val looperPlaybackPosition by audioViewModel.looperDelegate.playbackPosition.collectAsStateWithLifecycle()
                val looperLoopLength by audioViewModel.looperDelegate.loopLength.collectAsStateWithLifecycle()
                val looperCurrentBeat by audioViewModel.looperDelegate.currentBeat.collectAsStateWithLifecycle()
                val looperBpm by audioViewModel.looperDelegate.bpm.collectAsStateWithLifecycle()
                val looperBeatsPerBar by audioViewModel.looperDelegate.beatsPerBar.collectAsStateWithLifecycle()
                val looperBarCount by audioViewModel.looperDelegate.barCount.collectAsStateWithLifecycle()
                val looperIsMetronomeActive by audioViewModel.looperDelegate.isMetronomeActive.collectAsStateWithLifecycle()
                val looperIsQuantizedMode by audioViewModel.looperDelegate.isQuantizedMode.collectAsStateWithLifecycle()
                val looperIsCountInEnabled by audioViewModel.looperDelegate.isCountInEnabled.collectAsStateWithLifecycle()
                val looperVolume by audioViewModel.looperDelegate.loopVolume.collectAsStateWithLifecycle()
                val looperLayerCount by audioViewModel.looperDelegate.layerCount.collectAsStateWithLifecycle()
                val looperLayerVolumes by audioViewModel.looperDelegate.layerVolumes.collectAsStateWithLifecycle()
                val looperMetronomeTone by audioViewModel.looperDelegate.metronomeTone.collectAsStateWithLifecycle()
                val isSessionLibraryVisible by audioViewModel.looperDelegate.isSessionLibraryVisible.collectAsStateWithLifecycle()
                val savedSessions by audioViewModel.looperDelegate.savedSessions.collectAsStateWithLifecycle()
                val isReamping by audioViewModel.looperDelegate.isReamping.collectAsStateWithLifecycle()

                // Trim editor state (collected from LooperDelegate StateFlows)
                val isTrimEditorVisible by audioViewModel.looperDelegate.isTrimEditorVisible.collectAsStateWithLifecycle()
                val trimWaveformData by audioViewModel.looperDelegate.waveformData.collectAsStateWithLifecycle()
                val trimStart by audioViewModel.looperDelegate.trimStart.collectAsStateWithLifecycle()
                val trimEnd by audioViewModel.looperDelegate.trimEnd.collectAsStateWithLifecycle()
                val isCropUndoAvailable by audioViewModel.looperDelegate.isCropUndoAvailable.collectAsStateWithLifecycle()
                val trimSnapMode by audioViewModel.looperDelegate.snapMode.collectAsStateWithLifecycle()

                if (showDrums) {
                    DrumMachineScreen(
                        onNavigateBack = { showDrums = false }
                    )
                } else if (showSettings) {
                    SettingsScreen(
                        sampleRate = sampleRate,
                        framesPerBuffer = framesPerBuffer,
                        estimatedLatencyMs = estimatedLatencyMs,
                        isEngineRunning = isRunning,
                        isBatteryOptimized = audioViewModel.isBatteryOptimizationExempt(),
                        currentInputSource = inputSource,
                        onInputSourceSelected = { source ->
                            audioViewModel.setInputSource(source)
                        },
                        bufferMultiplier = bufferMultiplier,
                        onBufferMultiplierChanged = { mult ->
                            audioViewModel.setBufferMultiplier(mult)
                        },
                        onRequestBatteryExemption = {
                            startActivity(audioViewModel.createBatteryOptimizationIntent())
                        },
                        onBack = { showSettings = false }
                    )
                } else {
                    Scaffold(
                        modifier = Modifier.fillMaxSize(),
                        containerColor = DesignSystem.Background,
                        snackbarHost = {
                            SnackbarHost(snackbarHostState) { data ->
                                Snackbar(
                                    snackbarData = data,
                                    containerColor = Color(0xFF2A2A2A),
                                    contentColor = Color.White
                                )
                            }
                        }
                    ) { innerPadding ->
                        PedalboardScreen(
                            isRunning = isRunning,
                            inputLevel = inputLevel,
                            outputLevel = outputLevel,
                            inputSource = inputSource,
                            currentPresetName = currentPresetName,
                            currentPresetId = currentPresetId,
                            presets = presets,
                            effectStates = effectStates,
                            tunerData = tunerData,
                            engineError = engineError,
                            onSavePreset = { name, category ->
                                audioViewModel.saveCurrentAsPreset(name, category)
                            },
                            onOverwritePreset = { presetId ->
                                audioViewModel.overwritePreset(presetId)
                            },
                            onRenamePreset = { presetId, newName ->
                                audioViewModel.renamePreset(presetId, newName)
                            },
                            onDeletePreset = { presetId ->
                                audioViewModel.deletePreset(presetId)
                            },
                            onDuplicatePreset = { presetId ->
                                audioViewModel.duplicatePreset(presetId)
                            },
                            onStartClick = {
                                if (hasRecordAudioPermission()) {
                                    audioViewModel.startEngine()
                                } else if (shouldShowRequestPermissionRationale(
                                        Manifest.permission.RECORD_AUDIO)) {
                                    // User denied once, show rationale before asking again
                                    recordAudioPermissionLauncher.launch(
                                        Manifest.permission.RECORD_AUDIO
                                    )
                                } else {
                                    // First request, or permanent denial ("Don't ask again").
                                    // Try launching the request. If permanently denied, the
                                    // system ignores the request and nothing happens, so we
                                    // direct the user to app settings.
                                    if (permissionRequestedOnce) {
                                        // Permanently denied: open app settings
                                        startActivity(Intent(
                                            android.provider.Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                                            Uri.fromParts("package", packageName, null)
                                        ))
                                    } else {
                                        permissionRequestedOnce = true
                                        recordAudioPermissionLauncher.launch(
                                            Manifest.permission.RECORD_AUDIO
                                        )
                                    }
                                }
                            },
                            onStopClick = { audioViewModel.stopEngine() },
                            onPresetSelected = { presetId ->
                                audioViewModel.loadPreset(presetId)
                            },
                            onToggleEffect = { index ->
                                audioViewModel.toggleEffect(index)
                            },
                            onEditEffect = { index ->
                                if (index == TUNER_INDEX) {
                                    tunerOverlayActive = true
                                } else {
                                    editingEffectIndex = index
                                }
                            },
                            onExportPreset = {
                                presetExportLauncher.launch(audioViewModel.getExportFileName())
                            },
                            onImportPreset = {
                                presetImportLauncher.launch(arrayOf("application/json"))
                            },
                            onToggleFavorite = audioViewModel::togglePresetFavorite,
                            onDismissError = { audioViewModel.clearEngineError() },
                            inputGain = inputGain,
                            outputGain = outputGain,
                            globalBypass = globalBypass,
                            onInputGainChange = { audioViewModel.setInputGain(it) },
                            onOutputGainChange = { audioViewModel.setOutputGain(it) },
                            onToggleBypass = { audioViewModel.toggleGlobalBypass() },
                            effectOrder = effectOrder,
                            onReorderEffect = { from, to ->
                                audioViewModel.reorderEffect(from, to)
                            },
                            spectrumData = spectrumData,
                            abMode = abMode,
                            activeSlot = activeSlot,
                            onEnterAbMode = { audioViewModel.enterAbMode() },
                            onToggleAB = { audioViewModel.toggleAB() },
                            onExitAbMode = { slot -> audioViewModel.exitAbMode(slot) },
                            onParameterChange = { effectIndex, paramId, value ->
                                audioViewModel.setEffectParameter(effectIndex, paramId, value)
                            },
                            onWetDryMixChange = { effectIndex, mix ->
                                audioViewModel.setEffectWetDryMix(effectIndex, mix)
                            },
                            onSettingsClick = { showSettings = true },
                            onDrumsClick = { showDrums = true },
                            onTunerActiveChanged = { active ->
                                tunerOverlayActive = active
                            },
                            // Looper state
                            isLooperVisible = isLooperVisible,
                            looperState = looperState,
                            looperPlaybackPosition = looperPlaybackPosition,
                            looperLoopLength = looperLoopLength,
                            looperCurrentBeat = looperCurrentBeat,
                            looperBpm = looperBpm,
                            looperBeatsPerBar = looperBeatsPerBar,
                            looperBarCount = looperBarCount,
                            looperIsMetronomeActive = looperIsMetronomeActive,
                            looperIsQuantizedMode = looperIsQuantizedMode,
                            looperIsCountInEnabled = looperIsCountInEnabled,
                            looperVolume = looperVolume,
                            looperLayerCount = looperLayerCount,
                            looperLayerVolumes = looperLayerVolumes,
                            looperMetronomeTone = looperMetronomeTone,
                            looperSampleRate = sampleRate,
                            // Looper callbacks
                            onLooperToggle = { audioViewModel.looperDelegate.toggle() },
                            onLooperStop = { audioViewModel.looperDelegate.stop() },
                            onLooperUndo = { audioViewModel.looperDelegate.undo() },
                            onLooperPlayPause = {
                                // Play/Pause: if playing or overdubbing, stop; if stopped with loop, toggle to play
                                val state = audioViewModel.looperDelegate.looperState.value
                                when (state) {
                                    2, 3 -> audioViewModel.looperDelegate.stop()  // Playing/Overdubbing -> Stop
                                    else -> audioViewModel.looperDelegate.toggle() // Idle/Stopped -> Play
                                }
                            },
                            onLooperClear = { audioViewModel.looperDelegate.clear() },
                            onLooperTapTempo = { audioViewModel.looperDelegate.tapTempo() },
                            onLooperBpmChange = { audioViewModel.looperDelegate.setBPM(it) },
                            onLooperBarCountChange = { audioViewModel.looperDelegate.setBarCount(it) },
                            onLooperBeatsPerBarChange = { bpb ->
                                audioViewModel.looperDelegate.setTimeSignature(bpb, 4)
                            },
                            onLooperMetronomeToggle = { audioViewModel.looperDelegate.setMetronomeActive(it) },
                            onLooperQuantizedToggle = { audioViewModel.looperDelegate.setQuantizedMode(it) },
                            onLooperCountInToggle = { audioViewModel.looperDelegate.setCountInEnabled(it) },
                            onLooperMetronomeToneChange = { audioViewModel.looperDelegate.setMetronomeTone(it) },
                            onLooperVolumeChange = { audioViewModel.looperDelegate.setLoopVolume(it) },
                            onLooperLayerVolumeChange = { layer, vol ->
                                audioViewModel.looperDelegate.setLayerVolume(layer, vol)
                            },
                            onLooperSaveSession = { name ->
                                audioViewModel.looperDelegate.exportAndSaveSession(
                                    name = name,
                                    presetId = audioViewModel.currentPresetId.value.ifEmpty { null },
                                    presetName = audioViewModel.currentPresetName.value.ifEmpty { null }
                                )
                            },
                            onLooperShow = { audioViewModel.looperDelegate.showLooper() },
                            onLooperHide = { audioViewModel.looperDelegate.hideLooper() },
                            // Session library
                            isSessionLibraryVisible = isSessionLibraryVisible,
                            savedSessions = savedSessions,
                            isReamping = isReamping,
                            onShowSessionLibrary = { audioViewModel.looperDelegate.showSessionLibrary() },
                            onHideSessionLibrary = { audioViewModel.looperDelegate.hideSessionLibrary() },
                            onReampSession = { session -> audioViewModel.looperDelegate.startReamp(session) },
                            onStopReamp = { audioViewModel.looperDelegate.stopReamp() },
                            onDeleteSession = { id -> audioViewModel.looperDelegate.deleteSession(id) },
                            // Trim editor
                            isTrimEditorVisible = isTrimEditorVisible,
                            trimWaveformData = trimWaveformData,
                            trimStart = trimStart,
                            trimEnd = trimEnd,
                            isCropUndoAvailable = isCropUndoAvailable,
                            trimSnapMode = trimSnapMode,
                            onShowTrimEditor = { audioViewModel.looperDelegate.showTrimEditor() },
                            onHideTrimEditor = { audioViewModel.looperDelegate.hideTrimEditor() },
                            onTrimStartChange = { audioViewModel.looperDelegate.setTrimStart(it) },
                            onTrimEndChange = { audioViewModel.looperDelegate.setTrimEnd(it) },
                            onTrimSnapModeChange = { audioViewModel.looperDelegate.setSnapMode(it) },
                            onCommitTrim = { audioViewModel.looperDelegate.commitTrim() },
                            onUndoCrop = { audioViewModel.looperDelegate.undoCrop() },
                            modifier = Modifier.padding(innerPadding)
                        )

                        // Activate the tuner's pitch analysis when the tuner
                        // overlay is open. This eliminates the tuner's 8192-point
                        // FFT from every audio callback when the user isn't tuning.
                        LaunchedEffect(tunerOverlayActive) {
                            audioViewModel.setTunerActive(tunerOverlayActive)
                        }

                        // Effect editor bottom sheet
                        if (editingEffectIndex >= 0 && editingEffectIndex < effectStates.size) {
                            val editingState = effectStates[editingEffectIndex]
                            EffectEditorSheet(
                                effectState = editingState,
                                onDismiss = {
                                    editingEffectIndex = -1
                                    audioViewModel.clearIRLoadError()
                                    audioViewModel.clearNeuralModelError()
                                },
                                onParameterChange = { paramId, value ->
                                    audioViewModel.setEffectParameter(
                                        editingEffectIndex, paramId, value
                                    )
                                },
                                onLoadCustomIR = {
                                    irPickerLauncher.launch(
                                        arrayOf("audio/wav", "audio/x-wav", "audio/vnd.wave")
                                    )
                                },
                                userIRName = userIRName,
                                isLoadingIR = isLoadingIR,
                                irLoadError = irLoadError,
                                onLoadNeuralModel = {
                                    namPickerLauncher.launch(arrayOf("*/*"))
                                },
                                onClearNeuralModel = {
                                    audioViewModel.clearNeuralModel()
                                },
                                neuralModelName = neuralModelName,
                                isLoadingNeuralModel = isLoadingNeuralModel,
                                neuralModelError = neuralModelError,
                                isNeuralModelLoaded = isNeuralModelLoaded
                            )
                        }
                    }
                }
            }
        }
    }

    /**
     * Request runtime permissions that are needed before the user can start
     * the audio engine. We request early so the permission dialog appears
     * promptly when the user first opens the app.
     */
    private fun requestPermissionsIfNeeded() {
        // RECORD_AUDIO is always a runtime permission
        if (!hasRecordAudioPermission()) {
            recordAudioPermissionLauncher.launch(Manifest.permission.RECORD_AUDIO)
        }

        // POST_NOTIFICATIONS is required starting Android 13 (API 33)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(
                    this, Manifest.permission.POST_NOTIFICATIONS
                ) != PackageManager.PERMISSION_GRANTED
            ) {
                notificationPermissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
            }
        }
    }

    private fun hasRecordAudioPermission(): Boolean {
        return ContextCompat.checkSelfPermission(
            this, Manifest.permission.RECORD_AUDIO
        ) == PackageManager.PERMISSION_GRANTED
    }

    /**
     * Extract the display name of a file from a content URI.
     *
     * Uses the ContentResolver to query the OpenableColumns metadata.
     * Falls back to the URI's last path segment if the query fails.
     *
     * @param uri Content URI from the file picker.
     * @return Display name or null if not resolvable.
     */
    private fun getFileDisplayName(uri: Uri): String? {
        var name: String? = null
        val cursor: Cursor? = contentResolver.query(
            uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null
        )
        cursor?.use {
            if (it.moveToFirst()) {
                val nameIndex = it.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                if (nameIndex >= 0) {
                    name = it.getString(nameIndex)
                }
            }
        }
        return name ?: uri.lastPathSegment
    }
}
