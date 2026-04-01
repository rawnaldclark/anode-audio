package com.guitaremulator.app.ui.drums

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import com.guitaremulator.app.data.DrumStep
import com.guitaremulator.app.ui.theme.DesignSystem

// ── Constants ────────────────────────────────────────────────────────────────

/** Height of the main step cell area. */
private val STEP_CELL_HEIGHT = 40.dp

/** Height of the mini velocity bar area below each step. */
private val VELOCITY_BAR_HEIGHT = 10.dp

/** Corner radius for step cells and velocity bars. */
private val CELL_CORNER_RADIUS = 3.dp

/** Default velocity assigned when toggling a step on. */
private const val DEFAULT_TOGGLE_VELOCITY = 100

/** Beat marker positions (zero-indexed steps at quarter-note boundaries). */
private val BEAT_MARKER_STEPS = setOf(0, 4, 8, 12)

// ── StepSequencerRow Composable ──────────────────────────────────────────────

/**
 * 16-step sequencer row for the selected drum track.
 *
 * Displays a horizontal row of 16 Canvas-drawn step cells with:
 * - Active steps filled with [selectedTrackColor], alpha varies with velocity
 * - Inactive steps in [DesignSystem.MeterBg]
 * - Playback cursor: green top border on the current step
 * - Beat markers: brighter bottom edge on steps 0, 4, 8, 12
 * - Mini velocity bars below each step (height proportional to velocity/127)
 * - Steps beyond [patternLength] shown dimmed (alpha 0.3)
 *
 * Tap toggles a step on/off (velocity defaults to [DEFAULT_TOGGLE_VELOCITY]).
 * Long-press triggers [onLongPressStep] for future probability/retrig editing.
 *
 * @param steps              List of 16 [DrumStep] entries for the selected track.
 * @param currentStep        Playback cursor position (0-15), or -1 if stopped.
 * @param patternLength      Active pattern length in steps (1-64, typically 16).
 * @param selectedTrackColor Category color of the selected track (for active step fill).
 * @param onToggleStep       Callback with step index when a step is tapped.
 * @param onLongPressStep    Callback with step index on long-press (for editing).
 * @param modifier           Layout modifier for the row container.
 */
@Composable
fun StepSequencerRow(
    steps: List<DrumStep>,
    currentStep: Int,
    patternLength: Int,
    selectedTrackColor: Color,
    onToggleStep: (stepIndex: Int) -> Unit,
    onLongPressStep: (stepIndex: Int) -> Unit,
    modifier: Modifier = Modifier
) {
    val haptic = LocalHapticFeedback.current

    // Use rememberUpdatedState for callbacks used in pointerInput so the
    // gesture coroutine always reads the latest references without restarting
    val currentOnToggle by rememberUpdatedState(onToggleStep)
    val currentOnLongPress by rememberUpdatedState(onLongPressStep)
    val currentHaptic by rememberUpdatedState(haptic)

    // Pre-compute which steps are beyond pattern length for dimming
    val beyondLength by remember(patternLength) {
        derivedStateOf { patternLength }
    }

    Column(
        modifier = modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(2.dp)
    ) {
        // ── Step cells row ───────────────────────────────────────────────
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(2.dp)
        ) {
            for (stepIndex in 0 until 16) {
                val step = steps.getOrNull(stepIndex) ?: DrumStep()
                val isCurrent = stepIndex == currentStep
                val isBeyondLength = stepIndex >= beyondLength
                val isBeatMarker = stepIndex in BEAT_MARKER_STEPS

                StepCell(
                    step = step,
                    stepIndex = stepIndex,
                    isCurrent = isCurrent,
                    isBeyondLength = isBeyondLength,
                    isBeatMarker = isBeatMarker,
                    trackColor = selectedTrackColor,
                    onTap = {
                        currentHaptic.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                        currentOnToggle(stepIndex)
                    },
                    onLongPress = {
                        currentHaptic.performHapticFeedback(HapticFeedbackType.LongPress)
                        currentOnLongPress(stepIndex)
                    },
                    modifier = Modifier.weight(1f)
                )
            }
        }

        // ── Mini velocity bars row ───────────────────────────────────────
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(2.dp)
        ) {
            for (stepIndex in 0 until 16) {
                val step = steps.getOrNull(stepIndex) ?: DrumStep()
                val isBeyondLength = stepIndex >= beyondLength

                VelocityBar(
                    step = step,
                    isBeyondLength = isBeyondLength,
                    modifier = Modifier.weight(1f)
                )
            }
        }
    }
}

// ── Individual Step Cell ─────────────────────────────────────────────────────

/**
 * Single Canvas-drawn step cell with tap/long-press gesture handling.
 *
 * @param step           Step data (trigger state, velocity).
 * @param stepIndex      Zero-based step index (for accessibility).
 * @param isCurrent      Whether the playback cursor is on this step.
 * @param isBeyondLength Whether this step is beyond the active pattern length.
 * @param isBeatMarker   Whether this step falls on a beat boundary (0, 4, 8, 12).
 * @param trackColor     Category color for the active step fill.
 * @param onTap          Callback when the cell is tapped.
 * @param onLongPress    Callback when the cell is long-pressed.
 * @param modifier       Layout modifier.
 */
@Composable
private fun StepCell(
    step: DrumStep,
    stepIndex: Int,
    isCurrent: Boolean,
    isBeyondLength: Boolean,
    isBeatMarker: Boolean,
    trackColor: Color,
    onTap: () -> Unit,
    onLongPress: () -> Unit,
    modifier: Modifier = Modifier
) {
    // Pre-compute the active fill alpha based on velocity:
    // Maps velocity 0-127 to alpha 0.4-1.0 range
    val activeAlpha = remember(step.velocity) {
        if (step.trigger) {
            (step.velocity / 127f) * 0.6f + 0.4f
        } else {
            0f
        }
    }

    // Pre-compute colors outside draw lambda
    val activeColor = remember(trackColor, activeAlpha) {
        trackColor.copy(alpha = activeAlpha)
    }

    Canvas(
        modifier = modifier
            .height(STEP_CELL_HEIGHT)
            .semantics {
                contentDescription = buildString {
                    append("Step ${stepIndex + 1}")
                    if (step.trigger) append(", active, velocity ${step.velocity}")
                    else append(", inactive")
                    if (isCurrent) append(", playing")
                }
            }
            .pointerInput(Unit) {
                detectTapGestures(
                    onTap = { onTap() },
                    onLongPress = { onLongPress() }
                )
            }
    ) {
        val w = size.width
        val h = size.height
        val cornerPx = CELL_CORNER_RADIUS.toPx()
        val dimAlpha = if (isBeyondLength) 0.3f else 1.0f

        // ── Background fill ──────────────────────────────────────────────
        if (step.trigger) {
            // Active step: filled with track color, alpha varies with velocity
            drawRoundRect(
                color = activeColor.copy(alpha = activeColor.alpha * dimAlpha),
                cornerRadius = CornerRadius(cornerPx),
                size = Size(w, h)
            )
        } else {
            // Inactive step: dark meter background
            drawRoundRect(
                color = DesignSystem.MeterBg.copy(alpha = dimAlpha),
                cornerRadius = CornerRadius(cornerPx),
                size = Size(w, h)
            )
        }

        // ── Playback cursor: green top border ────────────────────────────
        if (isCurrent) {
            val cursorHeight = 2.dp.toPx()
            drawRoundRect(
                color = DesignSystem.JewelGreen,
                topLeft = Offset.Zero,
                size = Size(w, cursorHeight),
                cornerRadius = CornerRadius(cornerPx, cornerPx)
            )
        }

        // ── Beat marker: brighter bottom edge on beats 1/5/9/13 ─────────
        if (isBeatMarker) {
            val markerHeight = 1.5.dp.toPx()
            drawRoundRect(
                color = DesignSystem.PanelHighlight.copy(alpha = 0.5f * dimAlpha),
                topLeft = Offset(0f, h - markerHeight),
                size = Size(w, markerHeight),
                cornerRadius = CornerRadius(cornerPx, cornerPx)
            )
        }

        // ── Subtle border for definition ─────────────────────────────────
        drawRoundRect(
            color = DesignSystem.ModuleBorder.copy(alpha = 0.4f * dimAlpha),
            cornerRadius = CornerRadius(cornerPx),
            size = Size(w, h),
            style = androidx.compose.ui.graphics.drawscope.Stroke(width = 1.dp.toPx())
        )
    }
}

// ── Velocity Bar ─────────────────────────────────────────────────────────────

/**
 * Mini velocity bar drawn below a step cell.
 *
 * Height is proportional to velocity/127. Colored with [DesignSystem.VUAmber].
 * Non-triggered steps show an empty bar.
 *
 * @param step           Step data.
 * @param isBeyondLength Whether this step is beyond the active pattern length.
 * @param modifier       Layout modifier.
 */
@Composable
private fun VelocityBar(
    step: DrumStep,
    isBeyondLength: Boolean,
    modifier: Modifier = Modifier
) {
    val dimAlpha = if (isBeyondLength) 0.3f else 1.0f

    Canvas(
        modifier = modifier.height(VELOCITY_BAR_HEIGHT)
    ) {
        val w = size.width
        val h = size.height
        val cornerPx = 1.5.dp.toPx()

        // Background track
        drawRoundRect(
            color = DesignSystem.MeterBg.copy(alpha = 0.5f * dimAlpha),
            cornerRadius = CornerRadius(cornerPx),
            size = Size(w, h)
        )

        // Velocity fill (only if step is triggered)
        if (step.trigger && step.velocity > 0) {
            val velocityFraction = (step.velocity / 127f).coerceIn(0f, 1f)
            val barHeight = h * velocityFraction

            drawRoundRect(
                color = DesignSystem.VUAmber.copy(alpha = 0.8f * dimAlpha),
                topLeft = Offset(0f, h - barHeight),
                size = Size(w, barHeight),
                cornerRadius = CornerRadius(cornerPx)
            )
        }
    }
}
