package com.guitaremulator.app.ui.drums

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBars
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import com.guitaremulator.app.data.DrumStep
import com.guitaremulator.app.ui.theme.DesignSystem
import com.guitaremulator.app.viewmodel.DrumViewModel

// ── Jam Mode Pad Layout (2x4 = 8 pads, 1:1 with tracks) ─────────────────────

/**
 * 8-pad layout for Jam Mode: 2 rows of 4 pads.
 *
 * Each pad index maps directly to a sequencer track (no mapping table).
 * Pads are color-coded by drum category:
 * - Red (JewelRed): Kick
 * - Amber (VUAmber): Snare, Clap
 * - Green (JewelGreen): Hi-hats
 * - Blue (JewelBlue): Percussion (Rim, Shaker, Cowbell)
 */
val jamModePads: List<DrumPadInfo> = listOf(
    // Row 1: Core kit
    DrumPadInfo(index = 0, name = "KICK",    categoryColor = DesignSystem.JewelRed,   hasActiveSteps = false),
    DrumPadInfo(index = 1, name = "SNARE",   categoryColor = DesignSystem.VUAmber,    hasActiveSteps = false),
    DrumPadInfo(index = 2, name = "CL HAT",  categoryColor = DesignSystem.JewelGreen, hasActiveSteps = false),
    DrumPadInfo(index = 3, name = "OP HAT",  categoryColor = DesignSystem.JewelGreen, hasActiveSteps = false),
    // Row 2: Auxiliary
    DrumPadInfo(index = 4, name = "CLAP",    categoryColor = DesignSystem.VUAmber,    hasActiveSteps = false),
    DrumPadInfo(index = 5, name = "RIM",     categoryColor = JewelBlue,               hasActiveSteps = false),
    DrumPadInfo(index = 6, name = "SHAKER",  categoryColor = JewelBlue,               hasActiveSteps = false),
    DrumPadInfo(index = 7, name = "COWBELL", categoryColor = JewelBlue,               hasActiveSteps = false)
)

/**
 * Track metadata for edit mode's track strip selector.
 */
private val trackNames = listOf("KICK", "SNARE", "CL HAT", "OP HAT", "CLAP", "RIM", "SHAKER", "COWBELL")
private val trackColors = listOf(
    DesignSystem.JewelRed,   // KICK
    DesignSystem.VUAmber,    // SNARE
    DesignSystem.JewelGreen, // CL HAT
    DesignSystem.JewelGreen, // OP HAT
    DesignSystem.VUAmber,    // CLAP
    JewelBlue,               // RIM
    JewelBlue,               // SHAKER
    JewelBlue                // COWBELL
)

// ── DrumMachineScreen ────────────────────────────────────────────────────────

/**
 * Main drum machine screen with two modes:
 *
 * **JAM MODE** (default): Minimal interface for instant jamming. Shows only
 * the preset selector, a large play/stop button, BPM + tap tempo, 8 drum
 * pads (2x4), and swing/volume controls. A guitarist picks a preset, hits
 * play, and jams. Total UI items: ~15.
 *
 * **EDIT MODE**: Full editing interface with step sequencer (16 steps for
 * selected track), track selector, per-track volume/pan/mute, kit editor,
 * and pattern length control.
 *
 * The mode toggle is controlled by an EDIT button in the top bar (Jam->Edit)
 * and a BACK TO JAM button in Edit Mode.
 *
 * @param onNavigateBack Callback to return to the pedalboard screen.
 * @param viewModel      DrumViewModel instance (default from viewModel() factory).
 */
@Composable
fun DrumMachineScreen(
    onNavigateBack: () -> Unit,
    viewModel: DrumViewModel = viewModel()
) {
    // ── Mode state ───────────────────────────────────────────────────
    var editMode by rememberSaveable { mutableStateOf(false) }

    // ── Collect all state from ViewModel ─────────────────────────────
    val isPlaying by viewModel.isPlaying.collectAsStateWithLifecycle()
    val tempo by viewModel.tempo.collectAsStateWithLifecycle()
    val swing by viewModel.swing.collectAsStateWithLifecycle()
    val currentStep by viewModel.currentStep.collectAsStateWithLifecycle()
    val selectedTrack by viewModel.selectedTrack.collectAsStateWithLifecycle()
    val pattern by viewModel.currentPattern.collectAsStateWithLifecycle()
    val patternList by viewModel.patternList.collectAsStateWithLifecycle()
    val tempoMultiplier by viewModel.tempoMultiplier.collectAsStateWithLifecycle()
    val trackMutes by viewModel.trackMutes.collectAsStateWithLifecycle()
    val trackSolos by viewModel.trackSolos.collectAsStateWithLifecycle()
    val mixLevel by viewModel.mixLevel.collectAsStateWithLifecycle()
    val activeTracksOnStep by viewModel.activeTracksOnStep.collectAsStateWithLifecycle()

    // ── Screen layout ────────────────────────────────────────────────
    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(DesignSystem.Background)
            .windowInsetsPadding(WindowInsets.statusBars)
            .windowInsetsPadding(WindowInsets.navigationBars)
    ) {
        // Top Bar (shared between modes)
        DrumTopBar(
            onBack = onNavigateBack,
            kitName = pattern.name,
            editMode = editMode,
            onToggleMode = { editMode = !editMode }
        )

        if (editMode) {
            // ── EDIT MODE ────────────────────────────────────────────
            EditModeContent(
                viewModel = viewModel,
                isPlaying = isPlaying,
                tempo = tempo,
                swing = swing,
                currentStep = currentStep,
                selectedTrack = selectedTrack,
                pattern = pattern,
                patternList = patternList,
                tempoMultiplier = tempoMultiplier,
                trackMutes = trackMutes,
                trackSolos = trackSolos,
                onBackToJam = { editMode = false }
            )
        } else {
            // ── JAM MODE (default) ───────────────────────────────────
            JamModeContent(
                viewModel = viewModel,
                isPlaying = isPlaying,
                tempo = tempo,
                swing = swing,
                mixLevel = mixLevel,
                tempoMultiplier = tempoMultiplier,
                pattern = pattern,
                patternList = patternList,
                activeTracksOnStep = activeTracksOnStep
            )
        }
    }
}

// ── JAM MODE CONTENT ─────────────────────────────────────────────────────────

/**
 * Jam Mode layout: preset selector, big play button, BPM/tap, 8 pads,
 * swing/volume controls. Minimal, guitarist-focused.
 */
@Composable
private fun JamModeContent(
    viewModel: DrumViewModel,
    isPlaying: Boolean,
    tempo: Float,
    swing: Float,
    mixLevel: Float,
    tempoMultiplier: Float,
    pattern: com.guitaremulator.app.data.DrumPatternData,
    patternList: List<com.guitaremulator.app.data.DrumPatternData>,
    activeTracksOnStep: Set<Int>
) {
    // Derive pad info with active-step LED state
    val pads = remember(pattern) {
        jamModePads.mapIndexed { index, pad ->
            val hasSteps = if (index < pattern.tracks.size) {
                pattern.tracks[index].steps.any { it.trigger }
            } else false
            pad.copy(hasActiveSteps = hasSteps)
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(horizontal = 12.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        // 1. Preset Selector (prominent, horizontally scrollable with genre headers)
        PatternSelector(
            patterns = patternList,
            currentId = pattern.id,
            onSelectPattern = { viewModel.loadPattern(it) }
        )

        // 2. Big Play/Stop Button + BPM display
        JamTransportBar(
            isPlaying = isPlaying,
            tempo = tempo,
            tempoMultiplier = tempoMultiplier,
            onPlayStop = { viewModel.togglePlayback() },
            onTapTempo = { viewModel.tapTempo() }
        )

        // 3. Drum Pad Grid (2x4 = 8 pads) -- fills available vertical space
        DrumPadGrid(
            pads = pads,
            columns = 4,
            rows = 2,
            activeTracksOnStep = activeTracksOnStep,
            selectedPad = -1, // No selection highlight in Jam Mode
            onPadTap = { padIndex ->
                // Direct 1:1 mapping: pad index = track index
                viewModel.triggerPad(padIndex, 1.0f)
            },
            modifier = Modifier
                .weight(1f)
                .padding(vertical = 4.dp)
        )

        // 4. Swing + Volume sliders (compact row at bottom)
        JamControlsRow(
            swing = swing,
            mixLevel = mixLevel,
            onSwingChange = { viewModel.setSwing(it) },
            onMixLevelChange = { viewModel.setMixLevel(it) }
        )

        Spacer(modifier = Modifier.height(8.dp))
    }
}

// ── EDIT MODE CONTENT ────────────────────────────────────────────────────────

/**
 * Edit Mode layout: full transport, pattern selector, step sequencer,
 * track selector, mixer strip, and pattern length control.
 */
@Composable
private fun EditModeContent(
    viewModel: DrumViewModel,
    isPlaying: Boolean,
    tempo: Float,
    swing: Float,
    currentStep: Int,
    selectedTrack: Int,
    pattern: com.guitaremulator.app.data.DrumPatternData,
    patternList: List<com.guitaremulator.app.data.DrumPatternData>,
    tempoMultiplier: Float,
    trackMutes: BooleanArray,
    trackSolos: BooleanArray,
    onBackToJam: () -> Unit
) {
    val selectedTrackData = remember(pattern, selectedTrack) {
        if (selectedTrack < pattern.tracks.size) pattern.tracks[selectedTrack] else null
    }
    val selectedSteps: List<DrumStep> = selectedTrackData?.steps ?: List(16) { DrumStep() }
    val selectedTrackColor = if (selectedTrack in trackColors.indices) {
        trackColors[selectedTrack]
    } else {
        DesignSystem.TextMuted
    }

    Column(
        modifier = Modifier.fillMaxSize()
    ) {
        // Transport Bar (full controls for Edit Mode)
        DrumTransportBar(
            isPlaying = isPlaying,
            tempo = tempo,
            swing = swing,
            tempoMultiplier = tempoMultiplier,
            onPlayStop = { viewModel.togglePlayback() },
            onTempoChange = { viewModel.setTempo(it) },
            onSwingChange = { viewModel.setSwing(it) },
            onTapTempo = { viewModel.tapTempo() },
            onFillPress = { viewModel.triggerFill() },
            onFillRelease = { viewModel.releaseFill() }
        )

        // Pattern Selector
        PatternSelector(
            patterns = patternList,
            currentId = pattern.id,
            onSelectPattern = { viewModel.loadPattern(it) }
        )

        // Scrollable editing area
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .weight(1f)
                .verticalScroll(rememberScrollState())
        ) {
            // Track Select Strip (8 track chips)
            TrackSelectStrip(
                selectedTrack = selectedTrack,
                trackNames = trackNames,
                trackColors = trackColors,
                trackMutes = trackMutes,
                onSelect = { viewModel.selectTrack(it) }
            )

            Spacer(modifier = Modifier.height(4.dp))

            // Step Sequencer Row (16 steps for selected track)
            StepSequencerRow(
                steps = selectedSteps.take(pattern.length.coerceAtMost(16)),
                currentStep = if (isPlaying) currentStep else -1,
                patternLength = pattern.length.coerceAtMost(16),
                selectedTrackColor = selectedTrackColor,
                onToggleStep = { stepIndex ->
                    viewModel.toggleStep(selectedTrack, stepIndex)
                },
                onLongPressStep = { /* TODO: velocity/probability popup */ },
                modifier = Modifier.padding(horizontal = 8.dp)
            )

            Spacer(modifier = Modifier.height(8.dp))

            // Track Mixer Strip
            if (selectedTrackData != null) {
                DrumMixerStrip(
                    engineType = selectedTrackData.engineType,
                    volume = selectedTrackData.volume,
                    pan = selectedTrackData.pan,
                    muted = trackMutes.getOrElse(selectedTrack) { false },
                    soloed = trackSolos.getOrElse(selectedTrack) { false },
                    onVolumeChange = { viewModel.setTrackVolume(selectedTrack, it) },
                    onPanChange = { viewModel.setTrackPan(selectedTrack, it) },
                    onMuteToggle = { viewModel.muteTrack(selectedTrack) },
                    onSoloToggle = { viewModel.soloTrack(selectedTrack) },
                    onEditClick = { /* TODO: open DrumKitEditor overlay */ }
                )
            }

            Spacer(modifier = Modifier.height(8.dp))

            // Back to Jam button at bottom of edit area
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 12.dp, vertical = 8.dp)
                    .background(
                        color = DesignSystem.ModuleSurface,
                        shape = RoundedCornerShape(8.dp)
                    )
                    .border(
                        width = 1.dp,
                        color = DesignSystem.GoldPiping.copy(alpha = 0.5f),
                        shape = RoundedCornerShape(8.dp)
                    )
                    .clickable { onBackToJam() }
                    .padding(vertical = 12.dp),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = "\u25C0 BACK TO JAM",
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace,
                    color = DesignSystem.GoldPiping
                )
            }

            Spacer(modifier = Modifier.height(16.dp))
        }
    }
}

// ── DrumTopBar ───────────────────────────────────────────────────────────────

/**
 * Brushed metal top bar with back navigation, title, and mode toggle.
 *
 * In Jam Mode, shows an EDIT button on the right.
 * In Edit Mode, shows the kit name on the right (EDIT label changes to JAM).
 *
 * @param onBack       Callback for the back arrow button.
 * @param kitName      Name of the currently loaded drum kit / pattern.
 * @param editMode     Whether Edit Mode is active.
 * @param onToggleMode Callback to toggle between Jam and Edit modes.
 * @param modifier     Layout modifier.
 */
@Composable
fun DrumTopBar(
    onBack: () -> Unit,
    kitName: String,
    editMode: Boolean = false,
    onToggleMode: () -> Unit = {},
    modifier: Modifier = Modifier
) {
    val goldPipingColor = DesignSystem.GoldPiping.copy(alpha = 0.60f)
    val panelShadowColor = DesignSystem.PanelShadow
    val bottomShadowBrush = remember {
        Brush.verticalGradient(
            colors = listOf(
                DesignSystem.PanelShadow.copy(alpha = 0.5f),
                Color.Transparent
            )
        )
    }

    Row(
        modifier = modifier
            .fillMaxWidth()
            .height(56.dp)
            .background(remember { DesignSystem.brushedMetalGradient() })
            .drawBehind {
                val barHeight = size.height
                val barWidth = size.width

                // Gold piping underline
                val goldStroke = 2.dp.toPx()
                val shadowStroke = 1.dp.toPx()
                drawLine(
                    color = goldPipingColor,
                    start = Offset(0f, barHeight - goldStroke / 2f),
                    end = Offset(barWidth, barHeight - goldStroke / 2f),
                    strokeWidth = goldStroke
                )
                drawLine(
                    color = panelShadowColor,
                    start = Offset(0f, barHeight + shadowStroke / 2f),
                    end = Offset(barWidth, barHeight + shadowStroke / 2f),
                    strokeWidth = shadowStroke
                )

                // Bottom edge shadow gradient
                val shadowHeight = 4.dp.toPx()
                drawRect(
                    brush = bottomShadowBrush,
                    topLeft = Offset(0f, barHeight),
                    size = Size(barWidth, shadowHeight)
                )
            }
            .padding(horizontal = 8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Back arrow
        IconButton(
            onClick = onBack,
            modifier = Modifier.size(40.dp)
        ) {
            Icon(
                imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                contentDescription = "Back to pedalboard",
                tint = DesignSystem.CreamWhite,
                modifier = Modifier.size(20.dp)
            )
        }

        Spacer(modifier = Modifier.width(4.dp))

        // Title in inset panel
        Box(
            modifier = Modifier
                .background(
                    color = DesignSystem.AmpChassisGray,
                    shape = RoundedCornerShape(4.dp)
                )
                .border(
                    width = 1.dp,
                    color = DesignSystem.PanelShadow,
                    shape = RoundedCornerShape(4.dp)
                )
                .padding(horizontal = 10.dp, vertical = 4.dp)
        ) {
            Text(
                text = "DRUM MACHINE",
                fontSize = 14.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                color = DesignSystem.CreamWhite,
                maxLines = 1
            )
        }

        Spacer(modifier = Modifier.weight(1f))

        // Mode toggle button (EDIT in Jam Mode, JAM in Edit Mode)
        val modeLabel = if (editMode) "JAM" else "EDIT"
        val modeBorderColor = if (editMode) DesignSystem.SafeGreen else DesignSystem.VUAmber

        Box(
            modifier = Modifier
                .background(
                    color = DesignSystem.ModuleSurface,
                    shape = RoundedCornerShape(6.dp)
                )
                .border(
                    width = 1.5.dp,
                    color = modeBorderColor.copy(alpha = 0.7f),
                    shape = RoundedCornerShape(6.dp)
                )
                .clickable { onToggleMode() }
                .padding(horizontal = 12.dp, vertical = 6.dp),
            contentAlignment = Alignment.Center
        ) {
            Text(
                text = modeLabel,
                fontSize = 12.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                color = modeBorderColor,
                maxLines = 1
            )
        }
    }
}

// ── Jam Mode Transport (Big Play Button + BPM) ──────────────────────────────

/**
 * Simplified transport bar for Jam Mode.
 *
 * Shows a large play/stop button (64dp tall, full width), BPM display,
 * and tap tempo button. No fill button, no swing display -- those are
 * in the controls row below the pads.
 *
 * @param isPlaying       Whether the sequencer is currently playing.
 * @param tempo           Current BPM.
 * @param tempoMultiplier Current tempo multiplier.
 * @param onPlayStop      Toggle play/stop callback.
 * @param onTapTempo      Tap tempo callback.
 */
@Composable
private fun JamTransportBar(
    isPlaying: Boolean,
    tempo: Float,
    tempoMultiplier: Float,
    onPlayStop: () -> Unit,
    onTapTempo: () -> Unit
) {
    Column(
        modifier = Modifier.fillMaxWidth(),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        // Big Play/Stop Button -- at least 64dp tall, full width
        val playBgColor = if (isPlaying) {
            DesignSystem.JewelRed.copy(alpha = 0.2f)
        } else {
            DesignSystem.SafeGreen.copy(alpha = 0.15f)
        }
        val playBorderColor = if (isPlaying) DesignSystem.JewelRed else DesignSystem.SafeGreen
        val playTextColor = if (isPlaying) DesignSystem.JewelRed else DesignSystem.SafeGreen
        val playLabel = if (isPlaying) "\u25A0  STOP" else "\u25B6  PLAY"

        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(64.dp)
                .background(playBgColor, RoundedCornerShape(12.dp))
                .border(
                    width = 2.dp,
                    color = playBorderColor.copy(alpha = 0.8f),
                    shape = RoundedCornerShape(12.dp)
                )
                .clickable { onPlayStop() },
            contentAlignment = Alignment.Center
        ) {
            Text(
                text = playLabel,
                fontSize = 22.sp,
                fontWeight = FontWeight.Black,
                fontFamily = FontFamily.Monospace,
                color = playTextColor
            )
        }

        // BPM display + Tap Tempo (inline row)
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.Center,
            verticalAlignment = Alignment.CenterVertically
        ) {
            // BPM display
            Box(
                modifier = Modifier
                    .background(
                        color = DesignSystem.AmpChassisGray,
                        shape = RoundedCornerShape(4.dp)
                    )
                    .border(
                        width = 1.dp,
                        color = DesignSystem.PanelShadow,
                        shape = RoundedCornerShape(4.dp)
                    )
                    .padding(horizontal = 14.dp, vertical = 6.dp),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = "%.1f BPM".format(tempo * tempoMultiplier),
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace,
                    color = DesignSystem.VUAmber
                )
            }

            Spacer(modifier = Modifier.width(12.dp))

            // Tap Tempo button
            Box(
                modifier = Modifier
                    .background(
                        color = DesignSystem.ModuleSurface,
                        shape = RoundedCornerShape(6.dp)
                    )
                    .border(
                        width = 1.5.dp,
                        color = DesignSystem.VUAmber.copy(alpha = 0.6f),
                        shape = RoundedCornerShape(6.dp)
                    )
                    .clickable { onTapTempo() }
                    .padding(horizontal = 14.dp, vertical = 6.dp),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = "TAP",
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace,
                    color = DesignSystem.VUAmber
                )
            }
        }
    }
}

// ── Jam Mode Controls Row (Swing + Volume) ──────────────────────────────────

/**
 * Compact row with Swing and Volume sliders for Jam Mode.
 *
 * Uses simple labeled horizontal bars. Swing range: 50-75%.
 * Volume range: 0.0-2.0 (displayed as percentage).
 *
 * @param swing           Current swing percentage (50-75).
 * @param mixLevel        Current drum mix level (0.0-2.0).
 * @param onSwingChange   Swing change callback.
 * @param onMixLevelChange Volume change callback.
 */
@Composable
private fun JamControlsRow(
    swing: Float,
    mixLevel: Float,
    onSwingChange: (Float) -> Unit,
    onMixLevelChange: (Float) -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(
                color = DesignSystem.ControlPanelSurface,
                shape = RoundedCornerShape(8.dp)
            )
            .border(
                width = 1.dp,
                color = DesignSystem.ModuleBorder,
                shape = RoundedCornerShape(8.dp)
            )
            .padding(horizontal = 12.dp, vertical = 10.dp),
        horizontalArrangement = Arrangement.spacedBy(16.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Swing control
        Column(
            modifier = Modifier.weight(1f),
            verticalArrangement = Arrangement.spacedBy(4.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text(
                    text = "SWING",
                    fontSize = 10.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace,
                    color = DesignSystem.TextSecondary
                )
                Text(
                    text = "%.0f%%".format(swing),
                    fontSize = 10.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace,
                    color = DesignSystem.CreamWhite
                )
            }
            // Swing slider track (visual only -- gestures added in future iteration)
            val swingFraction = ((swing - 50f) / 25f).coerceIn(0f, 1f)
            SliderTrack(
                fraction = swingFraction,
                color = DesignSystem.VUAmber
            )
        }

        // Volume control
        Column(
            modifier = Modifier.weight(1f),
            verticalArrangement = Arrangement.spacedBy(4.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text(
                    text = "VOLUME",
                    fontSize = 10.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace,
                    color = DesignSystem.TextSecondary
                )
                Text(
                    text = "%.0f%%".format(mixLevel * 100f / 2f),
                    fontSize = 10.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace,
                    color = DesignSystem.CreamWhite
                )
            }
            val volumeFraction = (mixLevel / 2f).coerceIn(0f, 1f)
            SliderTrack(
                fraction = volumeFraction,
                color = DesignSystem.SafeGreen
            )
        }
    }
}

/**
 * Simple visual slider track (filled bar).
 *
 * @param fraction Fill amount 0.0-1.0.
 * @param color    Fill color.
 */
@Composable
private fun SliderTrack(
    fraction: Float,
    color: Color
) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .height(6.dp)
            .background(
                color = DesignSystem.MeterBg,
                shape = RoundedCornerShape(3.dp)
            )
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth(fraction)
                .height(6.dp)
                .background(
                    color = color.copy(alpha = 0.7f),
                    shape = RoundedCornerShape(3.dp)
                )
        )
    }
}

// ── TrackSelectStrip ─────────────────────────────────────────────────────────

/**
 * Horizontal row of 8 selectable track chips for Edit Mode.
 *
 * Each chip is colored by its track category and shows the track name.
 * The selected track has a brighter border and elevated appearance.
 * Muted tracks are dimmed.
 *
 * @param selectedTrack Currently selected track index (0-7).
 * @param trackNames    Display names for each track.
 * @param trackColors   Category colors for each track.
 * @param trackMutes    Per-track mute states.
 * @param onSelect      Callback invoked with the track index when tapped.
 * @param modifier      Layout modifier.
 */
@Composable
fun TrackSelectStrip(
    selectedTrack: Int,
    trackNames: List<String>,
    trackColors: List<Color>,
    trackMutes: BooleanArray,
    onSelect: (Int) -> Unit,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .horizontalScroll(rememberScrollState())
            .background(DesignSystem.AmpChassisGray)
            .padding(horizontal = 6.dp, vertical = 6.dp),
        horizontalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        for (i in 0 until minOf(8, trackNames.size)) {
            val isSelected = i == selectedTrack
            val isMuted = trackMutes.getOrElse(i) { false }
            val chipColor = trackColors.getOrElse(i) { DesignSystem.TextMuted }
            val alpha = when {
                isMuted -> 0.35f
                isSelected -> 1.0f
                else -> 0.6f
            }

            Box(
                modifier = Modifier
                    .clickable { onSelect(i) }
                    .background(
                        color = if (isSelected) chipColor.copy(alpha = 0.15f)
                        else DesignSystem.ModuleSurface,
                        shape = RoundedCornerShape(4.dp)
                    )
                    .border(
                        width = if (isSelected) 2.dp else 1.dp,
                        color = chipColor.copy(alpha = alpha),
                        shape = RoundedCornerShape(4.dp)
                    )
                    .padding(horizontal = 10.dp, vertical = 6.dp),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = trackNames[i],
                    fontSize = 10.sp,
                    fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium,
                    fontFamily = FontFamily.Monospace,
                    color = chipColor.copy(alpha = alpha),
                    maxLines = 1
                )
            }
        }
    }
}
