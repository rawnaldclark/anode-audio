package com.guitaremulator.app.ui.components

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TextField
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.ui.theme.DesignSystem

// -- Looper state constants (must match LooperDelegate) -----------------------

private const val STATE_IDLE = 0
private const val STATE_RECORDING = 1
private const val STATE_PLAYING = 2
private const val STATE_OVERDUBBING = 3
private const val STATE_FINISHING = 4
private const val STATE_COUNT_IN = 5

// -- State colors (shared palette across looper components) -------------------

private val StateColorIdle = DesignSystem.ModuleBorder
private val StateColorRecording = Color(0xFFE53935)
private val StateColorPlaying = Color(0xFF43A047)
private val StateColorOverdubbing = Color(0xFFFFA000)
private val StateColorCountIn = Color(0xFFE53935)
private val StateColorFinishing = Color(0xFFFFA000)

/**
 * Full-screen semi-transparent looper overlay.
 *
 * Renders on top of the pedalboard rack, following the same pattern as
 * [TunerOverlay]. The overlay animates in from the top with a fade and
 * slide transition, and provides a semi-transparent backdrop that can be
 * tapped to dismiss.
 *
 * The looper panel is a scrollable column containing:
 * 1. Header bar with close button, "LOOPER" title, and state indicator.
 * 2. Circular waveform display ([LooperWaveform]) showing loop position.
 * 3. Beat indicators (row of circles highlighting the current beat).
 * 4. Tempo controls ([TempoControls]) with BPM, bar count, and time sig.
 * 5. Transport controls ([LooperTransport]) with UNDO, MAIN, and STOP.
 * 6. Loop volume slider.
 * 7. Toggle switches for Metronome, Quantize, and Count-In.
 * 8. Save Session button.
 *
 * @param visible            Whether the overlay is shown.
 * @param looperState        Current looper state (0=Idle..5=CountIn).
 * @param playbackPosition   Current position in samples.
 * @param loopLength         Total loop length in samples.
 * @param currentBeat        Current beat within bar (0-based).
 * @param bpm                Current tempo in BPM.
 * @param beatsPerBar        Number of beats per bar.
 * @param barCount           Number of bars for quantized mode.
 * @param isMetronomeActive  Whether the metronome click is enabled.
 * @param isQuantizedMode    Whether quantized (bar-snapping) mode is on.
 * @param isCountInEnabled   Whether count-in before recording is enabled.
 * @param loopVolume         Loop playback volume (0.0-1.0).
 * @param sampleRate         Engine sample rate (for time display).
 * @param metronomeTone     Current metronome tone type index (0-5).
 * @param onToggle           Main transport button callback.
 * @param onStop             Stop button callback.
 * @param onUndo             Undo button callback.
 * @param onPlayPause        Play/pause toggle callback.
 * @param onClear            Clear/delete loop callback.
 * @param onTapTempo         Tap tempo button callback.
 * @param onMetronomeToneChange Metronome tone selection callback.
 * @param onBpmChange        BPM change callback.
 * @param onBarCountChange   Bar count change callback.
 * @param onBeatsPerBarChange Beats per bar change callback.
 * @param onMetronomeToggle  Metronome on/off callback.
 * @param onQuantizedToggle  Quantized mode on/off callback.
 * @param onCountInToggle    Count-in on/off callback.
 * @param onVolumeChange     Loop volume change callback.
 * @param onSaveSession      Save session callback (receives session name).
 * @param onShowSessions     Callback to open the session library.
 * @param onClose            Close overlay callback.
 * @param modifier           Layout modifier.
 */
@Composable
fun LooperOverlay(
    visible: Boolean,
    looperState: Int,
    playbackPosition: Int,
    loopLength: Int,
    currentBeat: Int,
    bpm: Float,
    beatsPerBar: Int,
    barCount: Int,
    isMetronomeActive: Boolean,
    isQuantizedMode: Boolean,
    isCountInEnabled: Boolean,
    loopVolume: Float,
    layerCount: Int = 0,
    layerVolumes: List<Float> = emptyList(),
    metronomeTone: Int = 0,
    sampleRate: Int,
    onToggle: () -> Unit,
    onStop: () -> Unit,
    onUndo: () -> Unit,
    onPlayPause: () -> Unit = {},
    onClear: () -> Unit = {},
    onTapTempo: () -> Unit,
    onMetronomeToneChange: (Int) -> Unit = {},
    onBpmChange: (Float) -> Unit,
    onBarCountChange: (Int) -> Unit,
    onBeatsPerBarChange: (Int) -> Unit,
    onMetronomeToggle: (Boolean) -> Unit,
    onQuantizedToggle: (Boolean) -> Unit,
    onCountInToggle: (Boolean) -> Unit,
    onVolumeChange: (Float) -> Unit,
    onLayerVolumeChange: (Int, Float) -> Unit = { _, _ -> },
    onSaveSession: (String) -> Unit,
    onShowSessions: () -> Unit,
    onShowTrimEditor: () -> Unit,
    onClose: () -> Unit,
    modifier: Modifier = Modifier
) {
    // Intercept the system back gesture/button when the overlay is showing
    BackHandler(enabled = visible) { onClose() }

    AnimatedVisibility(
        visible = visible,
        enter = fadeIn(tween(200)) + slideInVertically(tween(250)) { -it / 3 },
        exit = fadeOut(tween(150)) + slideOutVertically(tween(200)) { -it / 3 },
        modifier = modifier
    ) {
        Box(modifier = Modifier.fillMaxSize()) {
            // 1. Semi-transparent backdrop
            LooperBackdrop(onDismiss = onClose)

            // 2. Looper panel
            LooperPanel(
                looperState = looperState,
                playbackPosition = playbackPosition,
                loopLength = loopLength,
                currentBeat = currentBeat,
                bpm = bpm,
                beatsPerBar = beatsPerBar,
                barCount = barCount,
                isMetronomeActive = isMetronomeActive,
                isQuantizedMode = isQuantizedMode,
                isCountInEnabled = isCountInEnabled,
                loopVolume = loopVolume,
                layerCount = layerCount,
                layerVolumes = layerVolumes,
                metronomeTone = metronomeTone,
                sampleRate = sampleRate,
                onToggle = onToggle,
                onStop = onStop,
                onUndo = onUndo,
                onPlayPause = onPlayPause,
                onClear = onClear,
                onTapTempo = onTapTempo,
                onBpmChange = onBpmChange,
                onBarCountChange = onBarCountChange,
                onBeatsPerBarChange = onBeatsPerBarChange,
                onMetronomeToggle = onMetronomeToggle,
                onQuantizedToggle = onQuantizedToggle,
                onCountInToggle = onCountInToggle,
                onMetronomeToneChange = onMetronomeToneChange,
                onVolumeChange = onVolumeChange,
                onLayerVolumeChange = onLayerVolumeChange,
                onSaveSession = onSaveSession,
                onShowSessions = onShowSessions,
                onShowTrimEditor = onShowTrimEditor,
                onClose = onClose,
                modifier = Modifier.align(Alignment.TopCenter)
            )
        }
    }
}

// -- Private sub-components ---------------------------------------------------

/**
 * Full-screen semi-transparent backdrop that dismisses the overlay on tap.
 *
 * Mirrors the TunerOverlay backdrop pattern: uses a [MutableInteractionSource]
 * with no indication to suppress ripple effects.
 *
 * @param onDismiss Callback when the backdrop is tapped.
 */
@Composable
private fun LooperBackdrop(onDismiss: () -> Unit) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(DesignSystem.Background.copy(alpha = 0.85f))
            .clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = null,
                onClick = onDismiss
            )
    )
}

/**
 * Main looper panel anchored to the top of the screen.
 *
 * A scrollable column containing all looper controls and displays. Drawn
 * with rounded bottom corners and a state-colored bottom border line.
 */
@Composable
private fun LooperPanel(
    looperState: Int,
    playbackPosition: Int,
    loopLength: Int,
    currentBeat: Int,
    bpm: Float,
    beatsPerBar: Int,
    barCount: Int,
    isMetronomeActive: Boolean,
    isQuantizedMode: Boolean,
    isCountInEnabled: Boolean,
    loopVolume: Float,
    layerCount: Int,
    layerVolumes: List<Float>,
    metronomeTone: Int,
    sampleRate: Int,
    onToggle: () -> Unit,
    onStop: () -> Unit,
    onUndo: () -> Unit,
    onPlayPause: () -> Unit,
    onClear: () -> Unit,
    onTapTempo: () -> Unit,
    onBpmChange: (Float) -> Unit,
    onBarCountChange: (Int) -> Unit,
    onBeatsPerBarChange: (Int) -> Unit,
    onMetronomeToggle: (Boolean) -> Unit,
    onQuantizedToggle: (Boolean) -> Unit,
    onCountInToggle: (Boolean) -> Unit,
    onMetronomeToneChange: (Int) -> Unit,
    onVolumeChange: (Float) -> Unit,
    onLayerVolumeChange: (Int, Float) -> Unit,
    onSaveSession: (String) -> Unit,
    onShowSessions: () -> Unit,
    onShowTrimEditor: () -> Unit,
    onClose: () -> Unit,
    modifier: Modifier = Modifier
) {
    var showSaveDialog by remember { mutableStateOf(false) }

    // Resolve state color for the bottom border
    val borderColor = remember(looperState) {
        when (looperState) {
            STATE_RECORDING -> StateColorRecording
            STATE_PLAYING -> StateColorPlaying
            STATE_OVERDUBBING -> StateColorOverdubbing
            STATE_COUNT_IN -> StateColorCountIn
            STATE_FINISHING -> StateColorFinishing
            else -> DesignSystem.SafeGreen
        }
    }

    // State indicator text
    val stateDisplayText = remember(looperState) {
        when (looperState) {
            STATE_RECORDING -> "RECORDING"
            STATE_PLAYING -> "PLAYING"
            STATE_OVERDUBBING -> "OVERDUBBING"
            STATE_COUNT_IN -> "COUNT IN"
            STATE_FINISHING -> "FINISHING"
            else -> "IDLE"
        }
    }

    val stateDisplayColor = remember(looperState) {
        when (looperState) {
            STATE_RECORDING -> StateColorRecording
            STATE_PLAYING -> StateColorPlaying
            STATE_OVERDUBBING -> StateColorOverdubbing
            STATE_COUNT_IN -> StateColorCountIn
            STATE_FINISHING -> StateColorFinishing
            else -> DesignSystem.TextMuted
        }
    }

    Column(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(bottomStart = 16.dp, bottomEnd = 16.dp))
            .background(DesignSystem.ModuleSurface.copy(alpha = 0.97f))
            .drawBehind {
                val w = size.width
                val h = size.height

                // -- Amp-panel double-line border (machined panel edge) --
                // Outer border: PanelShadow at PanelBorderWidth (1.5dp)
                val outerStroke = DesignSystem.PanelBorderWidth.toPx()
                drawRect(
                    color = DesignSystem.PanelShadow,
                    topLeft = Offset(0f, 0f),
                    size = size,
                    style = androidx.compose.ui.graphics.drawscope.Stroke(outerStroke)
                )
                // Inner border: PanelHighlight at 1dp, inset by 1.5dp
                val innerInset = outerStroke
                val innerStroke = 1.dp.toPx()
                drawRect(
                    color = DesignSystem.PanelHighlight,
                    topLeft = Offset(innerInset, innerInset),
                    size = androidx.compose.ui.geometry.Size(
                        w - innerInset * 2f,
                        h - innerInset * 2f
                    ),
                    style = androidx.compose.ui.graphics.drawscope.Stroke(innerStroke)
                )

                // -- Bottom border: GoldPiping at 60% + PanelShadow shadow --
                val goldStroke = 2.dp.toPx()
                val shadowStroke = 1.dp.toPx()
                // Shadow line below gold
                drawLine(
                    color = DesignSystem.PanelShadow,
                    start = Offset(0f, h - shadowStroke / 2f),
                    end = Offset(w, h - shadowStroke / 2f),
                    strokeWidth = shadowStroke
                )
                // Gold piping above shadow
                drawLine(
                    color = DesignSystem.GoldPiping.copy(alpha = 0.6f),
                    start = Offset(0f, h - shadowStroke - goldStroke / 2f),
                    end = Offset(w, h - shadowStroke - goldStroke / 2f),
                    strokeWidth = goldStroke
                )
            }
            .verticalScroll(rememberScrollState())
            .padding(bottom = 24.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // ── 1. Header bar ───────────────────────────────────────────────
        LooperHeader(
            stateText = stateDisplayText,
            stateColor = stateDisplayColor,
            onClose = onClose
        )

        Spacer(modifier = Modifier.height(12.dp))

        // ── 2. Circular waveform display ────────────────────────────────
        LooperWaveform(
            playbackPosition = playbackPosition,
            loopLength = loopLength,
            looperState = looperState,
            currentBeat = currentBeat,
            beatsPerBar = beatsPerBar,
            modifier = Modifier.size(200.dp)
        )

        // Time display below waveform
        if (loopLength > 0 && sampleRate > 0) {
            Spacer(modifier = Modifier.height(4.dp))
            LoopTimeDisplay(
                playbackPosition = playbackPosition,
                loopLength = loopLength,
                sampleRate = sampleRate
            )
        }

        Spacer(modifier = Modifier.height(8.dp))

        // ── 3. Beat indicators ──────────────────────────────────────────
        BeatIndicators(
            currentBeat = currentBeat,
            beatsPerBar = beatsPerBar,
            looperState = looperState,
            modifier = Modifier.padding(horizontal = 24.dp)
        )

        Spacer(modifier = Modifier.height(16.dp))

        // ── 4. Transport controls ───────────────────────────────────────
        LooperTransport(
            looperState = looperState,
            hasLoop = loopLength > 0,
            onToggle = onToggle,
            onStop = onStop,
            onUndo = onUndo,
            onPlayPause = onPlayPause,
            onClear = onClear,
            modifier = Modifier.padding(horizontal = 12.dp)
        )

        Spacer(modifier = Modifier.height(20.dp))

        // ── 5. Loop volume slider ───────────────────────────────────────
        LoopVolumeSlider(
            volume = loopVolume,
            onVolumeChange = onVolumeChange,
            modifier = Modifier.padding(horizontal = 24.dp)
        )

        // ── 5b. Per-layer volume controls (only shown with 2+ layers) ──
        if (layerCount >= 2) {
            Spacer(modifier = Modifier.height(12.dp))
            LayerVolumeControls(
                layerCount = layerCount,
                layerVolumes = layerVolumes,
                onLayerVolumeChange = onLayerVolumeChange,
                modifier = Modifier.padding(horizontal = 24.dp)
            )
        }

        Spacer(modifier = Modifier.height(16.dp))

        // ── 6. Toggle switches ──────────────────────────────────────────
        ToggleSwitchesSection(
            isMetronomeActive = isMetronomeActive,
            isQuantizedMode = isQuantizedMode,
            isCountInEnabled = isCountInEnabled,
            metronomeTone = metronomeTone,
            onMetronomeToggle = onMetronomeToggle,
            onQuantizedToggle = onQuantizedToggle,
            onCountInToggle = onCountInToggle,
            onMetronomeToneChange = onMetronomeToneChange,
            modifier = Modifier.padding(horizontal = 24.dp)
        )

        Spacer(modifier = Modifier.height(16.dp))

        // ── 7. Tempo controls ───────────────────────────────────────────
        TempoControls(
            bpm = bpm,
            barCount = barCount,
            beatsPerBar = beatsPerBar,
            isQuantizedMode = isQuantizedMode,
            onBpmChange = onBpmChange,
            onBarCountChange = onBarCountChange,
            onBeatsPerBarChange = onBeatsPerBarChange,
            onTapTempo = onTapTempo,
            modifier = Modifier.padding(horizontal = 24.dp)
        )

        Spacer(modifier = Modifier.height(20.dp))

        // ── 8. Save Session button ──────────────────────────────────────
        SaveSessionButton(
            enabled = loopLength > 0,
            onClick = { showSaveDialog = true },
            modifier = Modifier.padding(horizontal = 24.dp)
        )

        Spacer(modifier = Modifier.height(8.dp))

        // ── 8b. Trim Loop button (secondary metallic style) ──────────
        Button(
            onClick = onShowTrimEditor,
            enabled = loopLength > 0,
            colors = ButtonDefaults.buttonColors(
                containerColor = DesignSystem.ControlPanelSurface,
                contentColor = DesignSystem.CreamWhite,
                disabledContainerColor = DesignSystem.ControlPanelSurface.copy(alpha = 0.5f),
                disabledContentColor = DesignSystem.TextMuted
            ),
            shape = RoundedCornerShape(12.dp),
            modifier = Modifier
                .fillMaxWidth()
                .height(40.dp)
                .padding(horizontal = 24.dp)
                .drawBehind {
                    // Subtle PanelHighlight border
                    drawRect(
                        color = DesignSystem.PanelHighlight,
                        size = size,
                        style = androidx.compose.ui.graphics.drawscope.Stroke(1.dp.toPx())
                    )
                }
        ) {
            Text(
                text = "TRIM LOOP",
                fontSize = 12.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                letterSpacing = 2.sp
            )
        }

        Spacer(modifier = Modifier.height(8.dp))

        // ── 9. View Saved Sessions button (secondary metallic style) ─
        Button(
            onClick = onShowSessions,
            colors = ButtonDefaults.buttonColors(
                containerColor = DesignSystem.ControlPanelSurface,
                contentColor = DesignSystem.CreamWhite
            ),
            shape = RoundedCornerShape(12.dp),
            modifier = Modifier
                .fillMaxWidth()
                .height(40.dp)
                .padding(horizontal = 24.dp)
                .drawBehind {
                    // Subtle PanelHighlight border
                    drawRect(
                        color = DesignSystem.PanelHighlight,
                        size = size,
                        style = androidx.compose.ui.graphics.drawscope.Stroke(1.dp.toPx())
                    )
                }
        ) {
            Text(
                text = "SAVED SESSIONS",
                fontSize = 12.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                letterSpacing = 2.sp
            )
        }
    }

    // ── Save session name dialog ────────────────────────────────────────
    if (showSaveDialog) {
        SaveSessionDialog(
            onDismiss = { showSaveDialog = false },
            onSave = { name ->
                showSaveDialog = false
                onSaveSession(name)
            }
        )
    }
}

/**
 * Header row with close button, "LOOPER" title, and state indicator text.
 *
 * @param stateText  Text describing the current state (e.g., "RECORDING").
 * @param stateColor Color matching the current state.
 * @param onClose    Callback when the close button is pressed.
 */
@Composable
private fun LooperHeader(
    stateText: String,
    stateColor: Color,
    onClose: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(48.dp)
            .padding(horizontal = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        // Close button on the left
        IconButton(
            onClick = onClose,
            modifier = Modifier.size(36.dp)
        ) {
            Icon(
                imageVector = Icons.Default.Close,
                contentDescription = "Close looper",
                tint = DesignSystem.TextSecondary,
                modifier = Modifier.size(20.dp)
            )
        }

        // Title (silk-screen style)
        Text(
            text = "LOOPER",
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            letterSpacing = 2.sp,
            color = DesignSystem.CreamWhite
        )

        // State indicator (monospace for panel readout feel)
        Text(
            text = stateText,
            fontSize = 12.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            letterSpacing = 1.sp,
            color = stateColor
        )
    }
}

/**
 * Row of beat indicator circles.
 *
 * Displays one circle per beat in the current bar. The current beat is
 * highlighted with the state color. Beat 1 (the downbeat) gets a larger
 * accent circle. Inactive beats are displayed in muted color.
 *
 * @param currentBeat  Current beat index (0-based).
 * @param beatsPerBar  Total number of beats per bar.
 * @param looperState  Current looper state (for active color selection).
 * @param modifier     Layout modifier.
 */
@Composable
private fun BeatIndicators(
    currentBeat: Int,
    beatsPerBar: Int,
    looperState: Int,
    modifier: Modifier = Modifier
) {
    val stateColor = remember(looperState) {
        when (looperState) {
            STATE_RECORDING -> StateColorRecording
            STATE_PLAYING -> StateColorPlaying
            STATE_OVERDUBBING -> StateColorOverdubbing
            STATE_COUNT_IN -> StateColorCountIn
            STATE_FINISHING -> StateColorFinishing
            else -> DesignSystem.TextMuted
        }
    }

    Row(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.Center,
        verticalAlignment = Alignment.CenterVertically
    ) {
        val safeBeatCount = beatsPerBar.coerceIn(1, 16)
        for (beat in 0 until safeBeatCount) {
            if (beat > 0) {
                Spacer(modifier = Modifier.width(6.dp))
            }

            val isCurrent = beat == currentBeat && looperState != STATE_IDLE
            val isDownbeat = beat == 0
            val dotSize = if (isDownbeat) 14.dp else 10.dp

            val dotColor = when {
                isCurrent && isDownbeat -> stateColor
                isCurrent -> stateColor.copy(alpha = 0.8f)
                isDownbeat -> DesignSystem.TextMuted.copy(alpha = 0.6f)
                else -> DesignSystem.TextMuted.copy(alpha = 0.3f)
            }

            Box(
                modifier = Modifier
                    .size(dotSize)
                    .clip(RoundedCornerShape(50))
                    .background(dotColor)
            )
        }
    }
}

/**
 * Elapsed / total time display for the loop.
 *
 * Formats sample positions into MM:SS.ms format for human-readable
 * loop timing information.
 *
 * @param playbackPosition Current position in samples.
 * @param loopLength       Total loop length in samples.
 * @param sampleRate       Engine sample rate.
 */
@Composable
private fun LoopTimeDisplay(
    playbackPosition: Int,
    loopLength: Int,
    sampleRate: Int
) {
    val currentSeconds = playbackPosition.toFloat() / sampleRate.coerceAtLeast(1)
    val totalSeconds = loopLength.toFloat() / sampleRate.coerceAtLeast(1)

    Text(
        text = "${formatTime(currentSeconds)} / ${formatTime(totalSeconds)}",
        fontSize = 13.sp,
        fontFamily = FontFamily.Monospace,
        color = DesignSystem.TextSecondary,
        textAlign = TextAlign.Center
    )
}

/**
 * Loop volume slider with label and percentage readout.
 *
 * @param volume         Current volume (0.0-1.0).
 * @param onVolumeChange Callback when the slider value changes.
 * @param modifier       Layout modifier.
 */
@Composable
private fun LoopVolumeSlider(
    volume: Float,
    onVolumeChange: (Float) -> Unit,
    modifier: Modifier = Modifier
) {
    Column(modifier = modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "LOOP VOLUME",
                fontSize = 10.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                letterSpacing = 2.sp,
                color = DesignSystem.CreamWhite
            )
            Text(
                text = "${(volume * 100f).toInt()}%",
                fontSize = 12.sp,
                fontFamily = FontFamily.Monospace,
                color = DesignSystem.TextValue
            )
        }

        Spacer(modifier = Modifier.height(4.dp))

        Slider(
            value = volume,
            onValueChange = onVolumeChange,
            valueRange = 0f..1f,
            colors = SliderDefaults.colors(
                thumbColor = DesignSystem.SafeGreen,
                activeTrackColor = DesignSystem.SafeGreen,
                inactiveTrackColor = DesignSystem.ModuleBorder
            ),
            modifier = Modifier.fillMaxWidth()
        )
    }
}

/**
 * Per-layer volume controls. Displays a compact slider for each recorded layer,
 * allowing the user to adjust each overdub pass independently.
 *
 * Layer 0 is labeled "BASE", layers 1+ are labeled "DUB 1", "DUB 2", etc.
 * Range is 0% to 150% (allows slight boost above unity).
 *
 * Only shown when 2+ layers exist (a single base recording uses the master
 * loop volume slider instead).
 *
 * @param layerCount         Number of recorded layers.
 * @param layerVolumes       Current volume for each layer.
 * @param onLayerVolumeChange Callback (layerIndex, newVolume).
 * @param modifier           Layout modifier.
 */
@Composable
private fun LayerVolumeControls(
    layerCount: Int,
    layerVolumes: List<Float>,
    onLayerVolumeChange: (Int, Float) -> Unit,
    modifier: Modifier = Modifier
) {
    Column(modifier = modifier.fillMaxWidth()) {
        Text(
            text = "LAYER VOLUMES",
            fontSize = 10.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            letterSpacing = 2.sp,
            color = DesignSystem.CreamWhite
        )

        Spacer(modifier = Modifier.height(4.dp))

        for (i in 0 until layerCount) {
            val volume = layerVolumes.getOrElse(i) { 1.0f }
            val label = if (i == 0) "BASE" else "DUB $i"
            val percent = (volume * 100f).toInt()

            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Layer label (fixed width for alignment)
                Text(
                    text = label,
                    fontSize = 10.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Medium,
                    color = DesignSystem.TextValue,
                    modifier = Modifier.width(48.dp)
                )

                // Volume slider (0.0 to 1.5 range)
                Slider(
                    value = volume,
                    onValueChange = { onLayerVolumeChange(i, it) },
                    valueRange = 0f..1.5f,
                    colors = SliderDefaults.colors(
                        thumbColor = if (i == 0) DesignSystem.SafeGreen
                                     else DesignSystem.GoldPiping,
                        activeTrackColor = if (i == 0) DesignSystem.SafeGreen
                                           else DesignSystem.GoldPiping,
                        inactiveTrackColor = DesignSystem.ModuleBorder
                    ),
                    modifier = Modifier.weight(1f)
                )

                // Percentage readout
                Text(
                    text = "$percent%",
                    fontSize = 10.sp,
                    fontFamily = FontFamily.Monospace,
                    color = DesignSystem.TextValue,
                    textAlign = TextAlign.End,
                    modifier = Modifier.width(36.dp)
                )
            }
        }
    }
}

/**
 * Section containing toggle switches for Metronome, Quantize, Count-In,
 * and a metronome tone selector.
 *
 * Each toggle is displayed as a horizontal row with a label on the left
 * and a Material3 Switch on the right. The metronome tone selector appears
 * below the metronome toggle as a cycling button that shows the current
 * tone name and advances to the next tone on tap.
 *
 * @param isMetronomeActive Whether metronome is currently on.
 * @param isQuantizedMode   Whether quantized mode is enabled.
 * @param isCountInEnabled  Whether count-in is enabled.
 * @param metronomeTone     Current metronome tone index (0-5).
 * @param onMetronomeToggle Callback when metronome toggle changes.
 * @param onQuantizedToggle Callback when quantized toggle changes.
 * @param onCountInToggle   Callback when count-in toggle changes.
 * @param onMetronomeToneChange Callback when metronome tone selection changes.
 * @param modifier          Layout modifier.
 */
@Composable
private fun ToggleSwitchesSection(
    isMetronomeActive: Boolean,
    isQuantizedMode: Boolean,
    isCountInEnabled: Boolean,
    metronomeTone: Int,
    onMetronomeToggle: (Boolean) -> Unit,
    onQuantizedToggle: (Boolean) -> Unit,
    onCountInToggle: (Boolean) -> Unit,
    onMetronomeToneChange: (Int) -> Unit,
    modifier: Modifier = Modifier
) {
    // Tone names (must match C++ and LooperDelegate.METRONOME_TONE_NAMES)
    val toneNames = remember {
        listOf("Click", "Soft", "Rim", "Cowbell", "Hi-Hat", "Deep")
    }
    val currentToneName = toneNames.getOrElse(metronomeTone) { "Click" }

    Column(modifier = modifier.fillMaxWidth()) {
        ToggleRow(
            label = "Metronome",
            checked = isMetronomeActive,
            onCheckedChange = onMetronomeToggle
        )

        // Metronome tone selector (visible when metronome is active)
        if (isMetronomeActive) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 4.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "Tone",
                    fontSize = 14.sp,
                    fontFamily = FontFamily.Monospace,
                    letterSpacing = 1.sp,
                    color = DesignSystem.CreamWhite
                )

                // Cycling tone button: tap to advance through tone types
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier
                        .clip(RoundedCornerShape(8.dp))
                        .background(DesignSystem.ControlPanelSurface)
                        .clickable {
                            val nextTone = (metronomeTone + 1) % toneNames.size
                            onMetronomeToneChange(nextTone)
                        }
                        .padding(horizontal = 12.dp, vertical = 6.dp)
                ) {
                    Text(
                        text = currentToneName,
                        fontSize = 13.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace,
                        letterSpacing = 1.sp,
                        color = DesignSystem.JewelAmber
                    )
                    Spacer(modifier = Modifier.width(6.dp))
                    Text(
                        text = "\u25B6",  // Right-pointing triangle as "next" indicator
                        fontSize = 10.sp,
                        color = DesignSystem.TextMuted
                    )
                }
            }
        }

        ToggleRow(
            label = "Quantize",
            checked = isQuantizedMode,
            onCheckedChange = onQuantizedToggle
        )
        ToggleRow(
            label = "Count-In",
            checked = isCountInEnabled,
            onCheckedChange = onCountInToggle
        )
    }
}

/**
 * Single toggle switch row with label and switch.
 *
 * @param label          Descriptive label for the toggle.
 * @param checked        Current toggle state.
 * @param onCheckedChange Callback when the toggle state changes.
 */
@Composable
private fun ToggleRow(
    label: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = label,
            fontSize = 14.sp,
            fontFamily = FontFamily.Monospace,
            letterSpacing = 1.sp,
            color = DesignSystem.CreamWhite
        )
        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange,
            colors = SwitchDefaults.colors(
                checkedThumbColor = DesignSystem.SafeGreen,
                checkedTrackColor = DesignSystem.SafeGreen.copy(alpha = 0.3f),
                uncheckedThumbColor = DesignSystem.TextMuted,
                uncheckedTrackColor = DesignSystem.ModuleBorder
            )
        )
    }
}

/**
 * Full-width "Save Session" button.
 *
 * Enabled only when a loop has been recorded (loopLength > 0). Uses
 * the app's accent green color when enabled.
 *
 * @param enabled Whether the button is interactive.
 * @param onClick Callback when pressed.
 * @param modifier Layout modifier.
 */
@Composable
private fun SaveSessionButton(
    enabled: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Button(
        onClick = onClick,
        enabled = enabled,
        colors = ButtonDefaults.buttonColors(
            containerColor = Color.Transparent,
            contentColor = Color.White,
            disabledContainerColor = DesignSystem.ModuleBorder,
            disabledContentColor = DesignSystem.TextMuted
        ),
        shape = RoundedCornerShape(12.dp),
        modifier = modifier
            .fillMaxWidth()
            .height(48.dp)
            .then(
                if (enabled) {
                    Modifier
                        .background(
                            brush = Brush.verticalGradient(
                                colors = listOf(
                                    DesignSystem.SafeGreen,
                                    DesignSystem.SafeGreen.copy(alpha = 0.8f)
                                )
                            ),
                            shape = RoundedCornerShape(12.dp)
                        )
                        .drawBehind {
                            // Chrome highlight at top edge
                            val highlightStroke = 1.dp.toPx()
                            drawLine(
                                color = DesignSystem.ChromeHighlight.copy(alpha = 0.4f),
                                start = Offset(8.dp.toPx(), highlightStroke / 2f),
                                end = Offset(size.width - 8.dp.toPx(), highlightStroke / 2f),
                                strokeWidth = highlightStroke
                            )
                        }
                } else {
                    Modifier
                }
            )
    ) {
        Text(
            text = "SAVE SESSION",
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            letterSpacing = 2.sp
        )
    }
}

/**
 * Alert dialog for entering a session name before saving.
 *
 * Presents a text field with a default name of "My Loop". The user can
 * modify the name before confirming the save.
 *
 * @param onDismiss Callback when the dialog is cancelled.
 * @param onSave    Callback with the session name when confirmed.
 */
@Composable
private fun SaveSessionDialog(
    onDismiss: () -> Unit,
    onSave: (String) -> Unit
) {
    var sessionName by remember { mutableStateOf("My Loop") }

    AlertDialog(
        onDismissRequest = onDismiss,
        containerColor = DesignSystem.ModuleSurface,
        title = {
            Text(
                "SAVE LOOP SESSION",
                color = DesignSystem.CreamWhite,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                letterSpacing = 2.sp
            )
        },
        text = {
            Column {
                Text(
                    "Enter a name for this loop session:",
                    color = DesignSystem.TextSecondary,
                    fontSize = 14.sp
                )
                Spacer(modifier = Modifier.height(8.dp))
                TextField(
                    value = sessionName,
                    onValueChange = { sessionName = it.take(100) },
                    singleLine = true,
                    textStyle = TextStyle(
                        fontSize = 16.sp,
                        color = DesignSystem.TextPrimary
                    ),
                    colors = TextFieldDefaults.colors(
                        focusedContainerColor = DesignSystem.Background,
                        unfocusedContainerColor = DesignSystem.Background,
                        cursorColor = DesignSystem.SafeGreen,
                        focusedIndicatorColor = DesignSystem.SafeGreen,
                        unfocusedIndicatorColor = DesignSystem.ModuleBorder
                    ),
                    modifier = Modifier.fillMaxWidth()
                )
            }
        },
        confirmButton = {
            Button(
                onClick = { onSave(sessionName.trim().ifEmpty { "Untitled Loop" }) },
                enabled = sessionName.isNotBlank(),
                colors = ButtonDefaults.buttonColors(
                    containerColor = DesignSystem.SafeGreen,
                    contentColor = Color.White
                )
            ) {
                Text("Save", fontWeight = FontWeight.Bold)
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("Cancel", color = DesignSystem.TextSecondary)
            }
        }
    )
}

// -- Utility ------------------------------------------------------------------

/**
 * Format a duration in seconds to MM:SS.d format.
 *
 * @param seconds Duration in seconds.
 * @return Formatted string (e.g., "01:23.4").
 */
private fun formatTime(seconds: Float): String {
    val totalMs = (seconds * 1000f).toLong().coerceAtLeast(0)
    val mins = totalMs / 60000
    val secs = (totalMs % 60000) / 1000
    val tenths = (totalMs % 1000) / 100
    return String.format("%02d:%02d.%d", mins, secs, tenths)
}
