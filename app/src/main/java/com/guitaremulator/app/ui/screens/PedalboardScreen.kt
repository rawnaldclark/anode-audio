package com.guitaremulator.app.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.zIndex
import com.guitaremulator.app.audio.InputSourceManager
import com.guitaremulator.app.data.Preset
import com.guitaremulator.app.data.PresetCategory
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.foundation.layout.Spacer
import androidx.compose.runtime.mutableStateMapOf
import com.guitaremulator.app.ui.components.ABComparisonBar
import com.guitaremulator.app.ui.components.BypassStrip
import com.guitaremulator.app.ui.components.EngineErrorBanner
import com.guitaremulator.app.ui.components.GutterRailHeader
import com.guitaremulator.app.ui.components.MeteringStrip
import com.guitaremulator.app.ui.components.PresetBrowserOverlay
import com.guitaremulator.app.ui.components.RackModule
import com.guitaremulator.app.ui.components.SavePresetDialog
import com.guitaremulator.app.ui.components.LooperOverlay
import com.guitaremulator.app.ui.components.LooperTrimEditor
import com.guitaremulator.app.ui.components.SessionLibraryOverlay
import com.guitaremulator.app.ui.components.TopBar
import com.guitaremulator.app.ui.components.TunerOverlay
import com.guitaremulator.app.ui.theme.DesignSystem
import com.guitaremulator.app.viewmodel.EffectUiState
import com.guitaremulator.app.viewmodel.TunerData
import sh.calvin.reorderable.ReorderableItem
import sh.calvin.reorderable.rememberReorderableLazyListState

/**
 * Main pedalboard screen with a vertical rack layout.
 *
 * Layout (top to bottom):
 *   1. TopBar (56dp, fixed): preset name, engine power, tuner, A/B, settings
 *   2. Engine error banner (conditional)
 *   3. A/B comparison bar (conditional, when abMode is active)
 *   4. Vertical LazyColumn of RackModules (weight 1f, scrollable, drag-reorderable)
 *   5. MeteringStrip (52dp collapsed, expandable with spectrum + master controls)
 *
 * Overlays (rendered on top of the main layout):
 *   - PresetBrowserOverlay: full-screen preset browser, triggered from TopBar preset name
 *   - TunerOverlay: semi-transparent tuner, triggered from TopBar tuner icon
 *   - LooperOverlay: semi-transparent looper panel, triggered from TopBar looper icon
 *
 * The background features a subtle tolex noise texture overlay and the rack area
 * includes chrome rail lines at left/right edges with gradient scroll shadows at
 * top and bottom when content extends beyond the visible region.
 *
 * All state is provided as parameters (unidirectional data flow) so this
 * composable is stateless and easy to preview/test.
 *
 * @param isRunning Whether the audio engine is currently active.
 * @param inputLevel Current input RMS level in [0.0, 1.0].
 * @param outputLevel Current output RMS level in [0.0, 1.0].
 * @param inputSource The currently detected input source (retained for backward compat).
 * @param currentPresetName Name of the currently loaded preset.
 * @param presets List of all available presets for the picker.
 * @param effectStates UI state for all 21 effects in the chain.
 * @param tunerData Current tuner readings (retained for backward compat).
 * @param engineError Current engine error message, or empty string if no error.
 * @param onStartClick Callback when the user taps Start (checks permissions first).
 * @param onStopClick Callback when the user taps Stop.
 * @param onPresetSelected Callback when a preset is selected from the dropdown.
 * @param onToggleEffect Callback to toggle an effect on/off (by index).
 * @param onEditEffect Callback when user taps a rack module detail button (by index).
 * @param onParameterChange Callback when a parameter value changes (effectIndex, paramId, value).
 * @param onWetDryMixChange Callback when wet/dry mix changes (effectIndex, mix).
 * @param onSettingsClick Callback to navigate to the settings screen.
 * @param modifier Modifier for the root layout.
 */
@Composable
fun PedalboardScreen(
    isRunning: Boolean,
    inputLevel: Float,
    outputLevel: Float,
    inputSource: InputSourceManager.InputSource,
    currentPresetName: String,
    presets: List<Preset>,
    effectStates: List<EffectUiState>,
    tunerData: TunerData,
    engineError: String = "",
    onStartClick: () -> Unit,
    onStopClick: () -> Unit,
    onPresetSelected: (String) -> Unit,
    onToggleEffect: (Int) -> Unit,
    onEditEffect: (Int) -> Unit,
    currentPresetId: String = "",
    onSavePreset: (String, PresetCategory) -> Unit = { _, _ -> },
    onOverwritePreset: (String) -> Unit = {},
    onRenamePreset: (String, String) -> Unit = { _, _ -> },
    onDeletePreset: (String) -> Unit = {},
    onDeleteFactoryPreset: (String) -> Unit = {},
    onDuplicatePreset: (String) -> Unit = {},
    onExportPreset: () -> Unit = {},
    onExportPresetById: (String) -> Unit = {},
    onImportPreset: () -> Unit = {},
    onToggleFavorite: (String) -> Unit = {},
    onDismissError: () -> Unit = {},
    inputGain: Float = 1.0f,
    outputGain: Float = 1.0f,
    globalBypass: Boolean = false,
    onInputGainChange: (Float) -> Unit = {},
    onOutputGainChange: (Float) -> Unit = {},
    onToggleBypass: () -> Unit = {},
    effectOrder: List<Int> = emptyList(),
    onReorderEffect: (Int, Int) -> Unit = { _, _ -> },
    spectrumData: FloatArray = FloatArray(64) { -90f },
    abMode: Boolean = false,
    activeSlot: Char = 'A',
    onEnterAbMode: () -> Unit = {},
    onToggleAB: () -> Unit = {},
    onExitAbMode: (Char) -> Unit = {},
    onParameterChange: (effectIndex: Int, paramId: Int, value: Float) -> Unit = { _, _, _ -> },
    onWetDryMixChange: (effectIndex: Int, mix: Float) -> Unit = { _, _ -> },
    onSettingsClick: () -> Unit = {},
    onDrumsClick: () -> Unit = {},
    onTunerActiveChanged: (Boolean) -> Unit = {},
    // -- Looper state --
    isLooperVisible: Boolean = false,
    looperState: Int = 0,
    looperPlaybackPosition: Int = 0,
    looperLoopLength: Int = 0,
    looperCurrentBeat: Int = 0,
    looperBpm: Float = 120f,
    looperBeatsPerBar: Int = 4,
    looperBarCount: Int = 4,
    looperIsMetronomeActive: Boolean = false,
    looperIsQuantizedMode: Boolean = false,
    looperIsCountInEnabled: Boolean = true,
    looperVolume: Float = 1.0f,
    looperLayerCount: Int = 0,
    looperLayerVolumes: List<Float> = emptyList(),
    looperMetronomeTone: Int = 0,
    looperSampleRate: Int = 48000,
    onLooperToggle: () -> Unit = {},
    onLooperStop: () -> Unit = {},
    onLooperUndo: () -> Unit = {},
    onLooperPlayPause: () -> Unit = {},
    onLooperClear: () -> Unit = {},
    onLooperTapTempo: () -> Unit = {},
    onLooperBpmChange: (Float) -> Unit = {},
    onLooperBarCountChange: (Int) -> Unit = {},
    onLooperBeatsPerBarChange: (Int) -> Unit = {},
    onLooperMetronomeToggle: (Boolean) -> Unit = {},
    onLooperQuantizedToggle: (Boolean) -> Unit = {},
    onLooperCountInToggle: (Boolean) -> Unit = {},
    onLooperMetronomeToneChange: (Int) -> Unit = {},
    onLooperVolumeChange: (Float) -> Unit = {},
    onLooperLayerVolumeChange: (Int, Float) -> Unit = { _, _ -> },
    onLooperSaveSession: (String) -> Unit = {},
    onLooperShow: () -> Unit = {},
    onLooperHide: () -> Unit = {},
    // -- Session Library --
    isSessionLibraryVisible: Boolean = false,
    savedSessions: List<com.guitaremulator.app.data.LooperSession> = emptyList(),
    isReamping: Boolean = false,
    onShowSessionLibrary: () -> Unit = {},
    onHideSessionLibrary: () -> Unit = {},
    onReampSession: (com.guitaremulator.app.data.LooperSession) -> Unit = {},
    onStopReamp: () -> Unit = {},
    onDeleteSession: (String) -> Unit = {},
    // -- Trim editor --
    isTrimEditorVisible: Boolean = false,
    trimWaveformData: FloatArray? = null,
    trimStart: Int = 0,
    trimEnd: Int = 0,
    isCropUndoAvailable: Boolean = false,
    trimSnapMode: com.guitaremulator.app.viewmodel.LooperDelegate.SnapMode = com.guitaremulator.app.viewmodel.LooperDelegate.SnapMode.FREE,
    onShowTrimEditor: () -> Unit = {},
    onHideTrimEditor: () -> Unit = {},
    onTrimStartChange: (Int) -> Unit = {},
    onTrimEndChange: (Int) -> Unit = {},
    onTrimSnapModeChange: (com.guitaremulator.app.viewmodel.LooperDelegate.SnapMode) -> Unit = {},
    onCommitTrim: () -> Unit = {},
    onUndoCrop: () -> Unit = {},
    modifier: Modifier = Modifier
) {
    // -- Local UI state --
    var showSaveDialog by remember { mutableStateOf(false) }
    var deleteConfirmPresetId by remember { mutableStateOf<String?>(null) }
    var showPresetBrowser by remember { mutableStateOf(false) }
    var showTuner by remember { mutableStateOf(false) }

    // Accordion state: only one RackModule can be expanded at a time.
    // A value of -1 means all modules are collapsed.
    var expandedIndex by remember { mutableIntStateOf(-1) }

    // Activate/deactivate the tuner FFT when the overlay is shown/hidden
    LaunchedEffect(showTuner) {
        onTunerActiveChanged(showTuner)
    }

    // -- Ordered effect states based on current effect order --
    val orderedStates = remember(effectStates, effectOrder) {
        if (effectOrder.isNotEmpty() && effectOrder.size == effectStates.size) {
            effectOrder.mapNotNull { physIdx ->
                effectStates.getOrNull(physIdx)
            }
        } else {
            effectStates
        }
    }

    // -- Focus Mode state --
    // Focus Mode uses "first-in-run visual expansion": consecutive bypassed
    // effects are visually grouped with the FIRST in each run rendering a
    // 44dp GutterRailHeader ("N BYPASSED" card) and subsequent items at 0dp.
    // Every effect is ALWAYS an individual item in the LazyColumn -- no
    // items appearing/disappearing. This guarantees stable item count and
    // indices during drag-and-drop reordering. Clicking the header toggles
    // expansion (all items in the run become 44dp BypassStrip rows).
    //
    // Chain Mode shows all effects as full RackModules (the original behavior).
    var focusMode by remember { mutableStateOf(true) }

    // In Focus Mode, filter out the Tuner (passthrough analyzer, index 14).
    val displayStates = remember(orderedStates, focusMode) {
        if (focusMode) {
            orderedStates.filter { it.index != TUNER_EFFECT_INDEX }
        } else {
            orderedStates
        }
    }

    // -- Bypass run membership for first-in-run visual expansion --
    // Each bypassed effect gets a BypassRunInfo telling it whether it is the
    // first item in a consecutive bypassed run, its run's start display index,
    // size, and the list of effects in the run.
    val bypassRunMap = remember(displayStates) {
        val map = mutableMapOf<Int, BypassRunInfo>()
        var i = 0
        while (i < displayStates.size) {
            if (!displayStates[i].enabled) {
                val runStart = i
                val runEffects = mutableListOf<EffectUiState>()
                while (i < displayStates.size && !displayStates[i].enabled) {
                    runEffects.add(displayStates[i])
                    i++
                }
                // Mark first item in run
                map[runStart] = BypassRunInfo(
                    isFirstInRun = true,
                    runStartDisplayIndex = runStart,
                    runSize = runEffects.size,
                    runEffects = runEffects
                )
                // Mark remaining items in run
                for (j in (runStart + 1) until (runStart + runEffects.size)) {
                    map[j] = BypassRunInfo(
                        isFirstInRun = false,
                        runStartDisplayIndex = runStart,
                        runSize = runEffects.size,
                        runEffects = runEffects
                    )
                }
            } else {
                i++
            }
        }
        map
    }

    // Pre-compute a display-index lookup map to avoid O(n) indexOf inside items().
    // Keyed by effectState.index (chain index) -> display index.
    val displayIndexMap = remember(displayStates) {
        val map = HashMap<Int, Int>(displayStates.size)
        displayStates.forEachIndexed { displayIdx, state ->
            map[state.index] = displayIdx
        }
        map
    }

    // Track which gutter groups are expanded (keyed by run-start display index)
    val expandedGutters = remember { mutableStateMapOf<Int, Boolean>() }

    // -- Reorderable list state for drag-and-drop --
    // Always active in both Focus and Chain modes so users can drag to reorder.
    //
    // from.index / to.index are positional indices in the LazyColumn, which
    // correspond to positions in displayStates (NOT the full effectOrder list).
    // In Focus Mode the Tuner (index 14) is filtered out, so display index N
    // may not equal order position N. We convert via displayStates to get the
    // actual effect chain indices, then find their positions in effectOrder.
    val lazyListState = rememberLazyListState()
    val reorderableLazyListState = rememberReorderableLazyListState(lazyListState) { from, to ->
        // Convert LazyColumn positional indices to effectOrder positions.
        val fromEffect = displayStates.getOrNull(from.index)
        val toEffect = displayStates.getOrNull(to.index)
        if (fromEffect != null && toEffect != null && effectOrder.isNotEmpty()) {
            val fromOrderPos = effectOrder.indexOf(fromEffect.index)
            val toOrderPos = effectOrder.indexOf(toEffect.index)
            if (fromOrderPos >= 0 && toOrderPos >= 0) {
                onReorderEffect(fromOrderPos, toOrderPos)
            }
        } else {
            // Fallback: direct index pass-through (Chain Mode, no Tuner filter)
            onReorderEffect(from.index, to.index)
        }
    }

    // -- Scroll position tracking for scroll shadow visibility --
    val canScrollUp by remember { derivedStateOf { lazyListState.canScrollBackward } }
    val canScrollDown by remember { derivedStateOf { lazyListState.canScrollForward } }

    // -- Pre-computed colors and brushes for rack rail and scroll shadow drawing --
    // Wrapped in remember to avoid recreating Color/Brush objects on each recomposition.
    val rackRailColor = remember { DesignSystem.ChromeHighlight.copy(alpha = 0.15f) }
    val topScrollShadow = remember {
        Brush.verticalGradient(
            colors = listOf(
                DesignSystem.Background.copy(alpha = 0.7f),
                Color.Transparent
            )
        )
    }
    val bottomScrollShadow = remember {
        Brush.verticalGradient(
            colors = listOf(
                Color.Transparent,
                DesignSystem.Background.copy(alpha = 0.7f)
            )
        )
    }

    // -- Root layout --
    Box(
        modifier = modifier
            .fillMaxSize()
            .background(DesignSystem.Background)
    ) {
        // Tolex noise texture overlay on the background
        DesignSystem.TolexNoiseOverlay()

        // Main content column
        Column(modifier = Modifier.fillMaxSize()) {
            // 1. Fixed TopBar (56dp)
            TopBar(
                presetName = currentPresetName,
                isRunning = isRunning,
                engineError = engineError,
                abMode = abMode,
                activeSlot = activeSlot,
                onPresetClick = { showPresetBrowser = true },
                onToggleEngine = if (isRunning) onStopClick else onStartClick,
                onTunerClick = { showTuner = !showTuner },
                onLooperClick = {
                    if (isLooperVisible) onLooperHide() else onLooperShow()
                },
                isLooperActive = looperState != 0,
                onToggleAB = onToggleAB,
                onEnterAbMode = onEnterAbMode,
                onExitAbMode = onExitAbMode,
                onSettingsClick = onSettingsClick,
                onDrumsClick = onDrumsClick,
                focusMode = focusMode,
                onToggleViewMode = { focusMode = !focusMode }
            )

            // 2. Engine error banner (conditional, above rack)
            if (engineError.isNotEmpty()) {
                EngineErrorBanner(
                    message = engineError,
                    onDismiss = onDismissError,
                    onRetry = onStartClick
                )
            }

            // 3. A/B comparison bar (conditional, when in A/B mode)
            if (abMode) {
                ABComparisonBar(
                    activeSlot = activeSlot,
                    onToggle = onToggleAB,
                    onKeepA = { onExitAbMode('A') },
                    onKeepB = { onExitAbMode('B') },
                    onCancel = { onExitAbMode('A') },
                    modifier = Modifier.padding(horizontal = 8.dp, vertical = 4.dp)
                )
            }

            // 4. Vertical rack of RackModules (scrollable, drag-reorderable)
            //    Wrapped in a Box to draw chrome rack rails and scroll shadows.
            Box(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxWidth()
            ) {
                // LazyColumn with rack rail lines drawn behind
                LazyColumn(
                    state = lazyListState,
                    modifier = Modifier
                        .fillMaxSize()
                        .drawBehind {
                            // Chrome rack rail lines at left and right edges
                            val railWidth = 1.5.dp.toPx()
                            val railOffsetLeft = 2.dp.toPx()
                            val railOffsetRight = size.width - 2.dp.toPx()

                            // Left rack rail
                            drawLine(
                                color = rackRailColor,
                                start = Offset(railOffsetLeft, 0f),
                                end = Offset(railOffsetLeft, size.height),
                                strokeWidth = railWidth
                            )

                            // Right rack rail
                            drawLine(
                                color = rackRailColor,
                                start = Offset(railOffsetRight, 0f),
                                end = Offset(railOffsetRight, size.height),
                                strokeWidth = railWidth
                            )
                        },
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 4.dp)
                ) {
                    // Every effect is an individual ReorderableItem with a
                    // stable key (its chain index). Item count is ALWAYS
                    // constant (26 in Focus Mode, 27 in Chain Mode).
                    //
                    // In Focus Mode, bypassed effects use "first-in-run
                    // visual expansion": the first bypassed effect in each
                    // consecutive run renders as a 44dp GutterRailHeader
                    // ("N BYPASSED" card), while the rest render at 0dp
                    // height (invisible spacers). Clicking the header
                    // toggles expansion, showing all run items as 44dp
                    // BypassStrip rows. Item count never changes; heights
                    // are stable during drag.
                    items(
                        displayStates,
                        key = { it.index },
                        contentType = { state ->
                            val dIdx = displayIndexMap[state.index] ?: 0
                            val rInfo = bypassRunMap[dIdx]
                            when {
                                !focusMode || state.enabled -> "rack_module"
                                rInfo != null && rInfo.isFirstInRun -> "gutter_header"
                                rInfo != null -> "bypass_spacer"
                                else -> "bypass_strip"
                            }
                        }
                    ) { state ->
                        val displayIndex = displayIndexMap[state.index] ?: 0
                        val runInfo = bypassRunMap[displayIndex]

                        ReorderableItem(reorderableLazyListState, key = state.index) { isDragging ->
                            if (!focusMode || state.enabled) {
                                // Enabled effect (both modes) or any effect
                                // in Chain Mode: full RackModule.
                                RackModule(
                                    effectState = state,
                                    expanded = expandedIndex == state.index,
                                    onToggleExpand = {
                                        expandedIndex = if (expandedIndex == state.index) -1 else state.index
                                    },
                                    onToggleEnabled = { onToggleEffect(state.index) },
                                    onParameterChange = { paramId, value ->
                                        onParameterChange(state.index, paramId, value)
                                    },
                                    onWetDryMixChange = { mix ->
                                        onWetDryMixChange(state.index, mix)
                                    },
                                    onEditDetail = { onEditEffect(state.index) },
                                    modifier = Modifier
                                        .longPressDraggableHandle()
                                        .zIndex(if (isDragging) 1f else 0f)
                                        .animateItem()
                                )
                            } else if (runInfo != null && runInfo.isFirstInRun) {
                                // First bypassed effect in a run: show the
                                // GutterRailHeader card (44dp) or expanded
                                // BypassStrip (44dp).
                                val runStart = runInfo.runStartDisplayIndex
                                val isExpanded = expandedGutters[runStart] == true

                                if (isExpanded) {
                                    BypassStrip(
                                        effectState = state,
                                        onToggleEnabled = { onToggleEffect(state.index) },
                                        modifier = Modifier
                                            .longPressDraggableHandle()
                                            .zIndex(if (isDragging) 1f else 0f)
                                            .animateItem()
                                    )
                                } else {
                                    GutterRailHeader(
                                        runSize = runInfo.runSize,
                                        runEffects = runInfo.runEffects,
                                        onExpandToggle = {
                                            expandedGutters[runStart] =
                                                expandedGutters[runStart] != true
                                        },
                                        modifier = Modifier
                                            .longPressDraggableHandle()
                                            .zIndex(if (isDragging) 1f else 0f)
                                            .animateItem()
                                    )
                                }
                            } else if (runInfo != null) {
                                // Non-first bypassed in a run: 0dp spacer
                                // when collapsed, 44dp BypassStrip when
                                // expanded. Always present for stable
                                // indices.
                                val runStart = runInfo.runStartDisplayIndex
                                val isExpanded = expandedGutters[runStart] == true

                                if (isExpanded) {
                                    BypassStrip(
                                        effectState = state,
                                        onToggleEnabled = { onToggleEffect(state.index) },
                                        modifier = Modifier
                                            .longPressDraggableHandle()
                                            .zIndex(if (isDragging) 1f else 0f)
                                            .animateItem()
                                    )
                                } else {
                                    Spacer(
                                        modifier = Modifier
                                            .height(0.dp)
                                            .fillMaxWidth()
                                            .longPressDraggableHandle()
                                            .animateItem()
                                    )
                                }
                            } else {
                                // Safety fallback: bypassed effect not in
                                // any run (shouldn't happen, but defensive).
                                BypassStrip(
                                    effectState = state,
                                    onToggleEnabled = { onToggleEffect(state.index) },
                                    modifier = Modifier
                                        .longPressDraggableHandle()
                                        .zIndex(if (isDragging) 1f else 0f)
                                        .animateItem()
                                )
                            }
                        }
                    }
                }

                // Top scroll shadow overlay (visible when content extends above)
                if (canScrollUp) {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(8.dp)
                            .align(Alignment.TopCenter)
                            .background(topScrollShadow)
                    )
                }

                // Bottom scroll shadow overlay (visible when content extends below)
                if (canScrollDown) {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(8.dp)
                            .align(Alignment.BottomCenter)
                            .background(bottomScrollShadow)
                    )
                }
            }

            // 5. Bottom MeteringStrip (52dp collapsed, expandable)
            MeteringStrip(
                inputLevel = inputLevel,
                outputLevel = outputLevel,
                spectrumData = spectrumData,
                inputGain = inputGain,
                outputGain = outputGain,
                globalBypass = globalBypass,
                onInputGainChange = onInputGainChange,
                onOutputGainChange = onOutputGainChange,
                onToggleBypass = onToggleBypass
            )
        }

        // -- Overlays (rendered on top of the main content) --

        // Full-screen preset browser overlay
        PresetBrowserOverlay(
            visible = showPresetBrowser,
            currentPresetId = currentPresetId,
            presets = presets,
            onPresetSelected = onPresetSelected,
            onSavePreset = { showSaveDialog = true },
            onOverwritePreset = onOverwritePreset,
            onRenamePreset = onRenamePreset,
            onDeletePreset = { presetId -> deleteConfirmPresetId = presetId },
            onDeleteFactoryPreset = onDeleteFactoryPreset,
            onDuplicatePreset = onDuplicatePreset,
            onExportPreset = onExportPreset,
            onExportPresetById = onExportPresetById,
            onImportPreset = onImportPreset,
            onToggleFavorite = onToggleFavorite,
            onDismiss = { showPresetBrowser = false }
        )

        // Semi-transparent tuner overlay (slides down from top)
        TunerOverlay(
            visible = showTuner,
            tunerData = tunerData,
            onDismiss = { showTuner = false }
        )

        // Semi-transparent looper overlay (slides down from top)
        LooperOverlay(
            visible = isLooperVisible,
            looperState = looperState,
            playbackPosition = looperPlaybackPosition,
            loopLength = looperLoopLength,
            currentBeat = looperCurrentBeat,
            bpm = looperBpm,
            beatsPerBar = looperBeatsPerBar,
            barCount = looperBarCount,
            isMetronomeActive = looperIsMetronomeActive,
            isQuantizedMode = looperIsQuantizedMode,
            isCountInEnabled = looperIsCountInEnabled,
            loopVolume = looperVolume,
            layerCount = looperLayerCount,
            layerVolumes = looperLayerVolumes,
            metronomeTone = looperMetronomeTone,
            sampleRate = looperSampleRate,
            onToggle = onLooperToggle,
            onStop = onLooperStop,
            onUndo = onLooperUndo,
            onPlayPause = onLooperPlayPause,
            onClear = onLooperClear,
            onTapTempo = onLooperTapTempo,
            onMetronomeToneChange = onLooperMetronomeToneChange,
            onBpmChange = onLooperBpmChange,
            onBarCountChange = onLooperBarCountChange,
            onBeatsPerBarChange = onLooperBeatsPerBarChange,
            onMetronomeToggle = onLooperMetronomeToggle,
            onQuantizedToggle = onLooperQuantizedToggle,
            onCountInToggle = onLooperCountInToggle,
            onVolumeChange = onLooperVolumeChange,
            onLayerVolumeChange = onLooperLayerVolumeChange,
            onSaveSession = onLooperSaveSession,
            onShowSessions = onShowSessionLibrary,
            onShowTrimEditor = onShowTrimEditor,
            onClose = onLooperHide
        )

        // Session library overlay
        SessionLibraryOverlay(
            visible = isSessionLibraryVisible,
            sessions = savedSessions,
            isReamping = isReamping,
            onReamp = onReampSession,
            onStopReamp = onStopReamp,
            onDelete = onDeleteSession,
            onClose = onHideSessionLibrary
        )

        // Trim editor overlay
        LooperTrimEditor(
            visible = isTrimEditorVisible,
            waveformData = trimWaveformData,
            trimStart = trimStart,
            trimEnd = trimEnd,
            loopLength = looperLoopLength,
            playbackPosition = looperPlaybackPosition,
            sampleRate = looperSampleRate,
            isCropUndoAvailable = isCropUndoAvailable,
            snapMode = trimSnapMode,
            onTrimStartChange = onTrimStartChange,
            onTrimEndChange = onTrimEndChange,
            onSnapModeChange = onTrimSnapModeChange,
            onCommit = onCommitTrim,
            onUndoCrop = onUndoCrop,
            onClose = onHideTrimEditor
        )
    }

    // -- Dialogs (rendered outside the main Column layout tree) --

    // Save preset dialog
    if (showSaveDialog) {
        SavePresetDialog(
            initialName = currentPresetName,
            onDismiss = { showSaveDialog = false },
            onSave = { name, category ->
                showSaveDialog = false
                onSavePreset(name, category)
            }
        )
    }

    // Delete confirmation dialog
    deleteConfirmPresetId?.let { presetId ->
        val presetName = presets.find { it.id == presetId }?.name ?: "this preset"
        AlertDialog(
            onDismissRequest = { deleteConfirmPresetId = null },
            containerColor = DesignSystem.ModuleSurface,
            title = {
                Text(
                    "Delete Preset",
                    color = DesignSystem.TextPrimary,
                    fontWeight = FontWeight.Bold
                )
            },
            text = {
                Text(
                    "Are you sure you want to delete \"$presetName\"? This cannot be undone.",
                    color = DesignSystem.TextPrimary
                )
            },
            confirmButton = {
                Button(
                    onClick = {
                        deleteConfirmPresetId = null
                        onDeletePreset(presetId)
                    },
                    colors = ButtonDefaults.buttonColors(
                        containerColor = DesignSystem.ClipRed,
                        contentColor = Color.White
                    )
                ) {
                    Text("Delete", fontWeight = FontWeight.Bold)
                }
            },
            dismissButton = {
                TextButton(onClick = { deleteConfirmPresetId = null }) {
                    Text("Cancel", color = DesignSystem.TextSecondary)
                }
            }
        )
    }
}

// -- Constants --

/** Signal chain index of the Tuner effect, used for the TopBar tuner shortcut. */
private const val TUNER_EFFECT_INDEX = 14

/**
 * Describes a bypassed effect's membership in a consecutive bypass run.
 *
 * Used by the "first-in-run visual expansion" pattern: the first item in
 * each run renders a 44dp [GutterRailHeader], while subsequent items render
 * as 0dp spacers (or 44dp [BypassStrip] when expanded). This keeps the
 * LazyColumn item count constant for drag-and-drop stability.
 *
 * @param isFirstInRun True if this is the first bypassed effect in the run.
 * @param runStartDisplayIndex The display index of the first item in this run.
 * @param runSize Number of consecutive bypassed effects in this run.
 * @param runEffects All [EffectUiState] items in this run.
 */
private data class BypassRunInfo(
    val isFirstInRun: Boolean,
    val runStartDisplayIndex: Int,
    val runSize: Int,
    val runEffects: List<EffectUiState>
)

