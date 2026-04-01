package com.guitaremulator.app.ui.components

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalHapticFeedback
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

// -- State colors -------------------------------------------------------------

private val StateColorIdle = DesignSystem.ModuleBorder
private val StateColorRecording = Color(0xFFE53935)
private val StateColorPlaying = Color(0xFF43A047)
private val StateColorOverdubbing = Color(0xFFFFA000)
private val StateColorCountIn = Color(0xFFE53935)
private val StateColorFinishing = Color(0xFFFFA000)

/**
 * Looper transport control row with five distinct buttons.
 *
 * Layout (left to right):
 * - **UNDO** (medium): Reverts the last overdub layer. Enabled when playing.
 * - **STOP** (medium): Stops playback/recording, loop remains in memory.
 * - **MAIN** (large, center): Primary transport toggle (REC/PLAY/OVR).
 *   Color-coded by state: red=recording, green=playing, amber=overdubbing.
 * - **PLAY/PAUSE** (medium): Toggles between play and pause when a loop exists.
 *   Green when playing, amber when paused.
 * - **DELETE** (medium, danger red): Clears the loop entirely. Positioned at
 *   the far right with danger coloring to prevent accidental taps.
 *
 * All buttons are 56dp+ circles with haptic feedback and press animations,
 * sized for easy targeting by guitarists looking down at their phone.
 *
 * @param looperState Current looper state (0=Idle..5=CountIn).
 * @param hasLoop     Whether a recorded loop exists (loopLength > 0).
 * @param onToggle    Callback for the main transport toggle button.
 * @param onStop      Callback for the stop button.
 * @param onUndo      Callback for the undo button.
 * @param onPlayPause Callback for the play/pause button.
 * @param onClear     Callback for the delete/clear button (after confirmation).
 * @param modifier    Layout modifier.
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
fun LooperTransport(
    looperState: Int,
    hasLoop: Boolean,
    onToggle: () -> Unit,
    onStop: () -> Unit,
    onUndo: () -> Unit,
    onPlayPause: () -> Unit,
    onClear: () -> Unit,
    modifier: Modifier = Modifier
) {
    val hapticFeedback = LocalHapticFeedback.current

    // Confirmation state for the delete button
    var showDeleteConfirm by remember { mutableStateOf(false) }

    // Resolve state-dependent visuals for the main button
    val stateColor = remember(looperState) {
        when (looperState) {
            STATE_RECORDING -> StateColorRecording
            STATE_PLAYING -> StateColorPlaying
            STATE_OVERDUBBING -> StateColorOverdubbing
            STATE_COUNT_IN -> StateColorCountIn
            STATE_FINISHING -> StateColorFinishing
            else -> StateColorIdle
        }
    }

    val stateLabel = remember(looperState) {
        when (looperState) {
            STATE_RECORDING -> "REC"
            STATE_PLAYING -> "PLAY"
            STATE_OVERDUBBING -> "OVR"
            STATE_COUNT_IN -> "..."
            STATE_FINISHING -> "FIN"
            else -> "REC"
        }
    }

    // Button enable states
    val undoEnabled = looperState == STATE_PLAYING
    val stopEnabled = looperState != STATE_IDLE
    val playPauseEnabled = hasLoop
    val clearEnabled = hasLoop || looperState != STATE_IDLE

    // Play/Pause visual state
    val isPlaying = looperState == STATE_PLAYING || looperState == STATE_OVERDUBBING
    val playPauseColor = when {
        isPlaying -> StateColorPlaying
        playPauseEnabled -> DesignSystem.JewelAmber
        else -> DesignSystem.TextMuted
    }
    val playPauseLabel = if (isPlaying) "PAUSE" else "PLAY"

    Column(modifier = modifier.fillMaxWidth()) {
        // ── Top row: UNDO, STOP, MAIN, PLAY/PAUSE ──────────────────────────
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceEvenly,
            verticalAlignment = Alignment.CenterVertically
        ) {
            // ── UNDO button ─────────────────────────────────────────────
            TransportMediumButton(
                label = "UNDO",
                enabled = undoEnabled,
                iconPainter = { center, radius, color -> drawUndoIcon(center, radius, color) },
                onClick = {
                    hapticFeedback.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                    onUndo()
                }
            )

            // ── STOP button ─────────────────────────────────────────────
            TransportMediumButton(
                label = "STOP",
                enabled = stopEnabled,
                iconPainter = { center, radius, color -> drawStopIcon(center, radius * 0.75f, color) },
                onClick = {
                    hapticFeedback.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                    onStop()
                }
            )

            // ── MAIN transport button (largest, center) ──────────────────
            MainTransportButton(
                stateColor = stateColor,
                stateLabel = stateLabel,
                onClick = {
                    hapticFeedback.performHapticFeedback(HapticFeedbackType.LongPress)
                    onToggle()
                }
            )

            // ── PLAY/PAUSE button ───────────────────────────────────────
            TransportMediumButton(
                label = playPauseLabel,
                enabled = playPauseEnabled,
                buttonColor = playPauseColor,
                iconPainter = { center, radius, color ->
                    if (isPlaying) {
                        drawPauseIcon(center, radius * 0.65f, color)
                    } else {
                        drawPlayIcon(center, radius * 0.75f, color)
                    }
                },
                onClick = {
                    hapticFeedback.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                    onPlayPause()
                }
            )

            // ── DELETE button (danger) ──────────────────────────────────
            if (showDeleteConfirm) {
                // Confirmation state: button turns fully red with "TAP" label
                TransportDangerButton(
                    label = "TAP",
                    enabled = true,
                    onClick = {
                        hapticFeedback.performHapticFeedback(HapticFeedbackType.LongPress)
                        showDeleteConfirm = false
                        onClear()
                    }
                )
            } else {
                TransportDangerButton(
                    label = "DELETE",
                    enabled = clearEnabled,
                    onClick = {
                        hapticFeedback.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                        showDeleteConfirm = true
                    }
                )
            }
        }
    }
}

// -- Private sub-components ---------------------------------------------------

/**
 * Large circular main transport button (80dp).
 *
 * The button face is a circle with a radial gradient from the state
 * color, a metallic ring, and the state abbreviation + record dot.
 *
 * @param stateColor The color reflecting the current looper state.
 * @param stateLabel Short abbreviation displayed below (e.g., "REC").
 * @param onClick    Callback when pressed.
 */
@Composable
private fun MainTransportButton(
    stateColor: Color,
    stateLabel: String,
    onClick: () -> Unit
) {
    val interactionSource = remember { MutableInteractionSource() }
    val isPressed by interactionSource.collectIsPressedAsState()

    val scale by animateFloatAsState(
        targetValue = if (isPressed) 0.92f else 1.0f,
        animationSpec = spring(dampingRatio = 0.55f, stiffness = 600f),
        label = "mainButtonScale"
    )

    Column(
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Canvas(
            modifier = Modifier
                .size(80.dp)
                .graphicsLayer {
                    scaleX = scale
                    scaleY = scale
                }
                .clip(CircleShape)
                .clickable(
                    interactionSource = interactionSource,
                    indication = null,
                    onClick = onClick
                )
        ) {
            val center = Offset(size.width / 2f, size.height / 2f)
            val outerRadius = size.minDimension / 2f
            val innerRadius = outerRadius * 0.85f

            // Glow behind the button
            drawCircle(
                color = stateColor.copy(alpha = 0.18f),
                radius = outerRadius * 1.15f,
                center = center
            )

            // Outer metallic ring
            drawCircle(
                color = Color(0xFF2A2A30),
                radius = outerRadius,
                center = center
            )

            // Ring stroke
            drawCircle(
                color = stateColor.copy(alpha = 0.5f),
                radius = outerRadius,
                center = center,
                style = Stroke(width = 2.5.dp.toPx())
            )

            // Inner face with radial gradient
            drawCircle(
                brush = Brush.radialGradient(
                    colors = listOf(
                        stateColor.copy(alpha = 0.9f),
                        stateColor.copy(alpha = 0.6f)
                    ),
                    center = center,
                    radius = innerRadius
                ),
                radius = innerRadius,
                center = center
            )

            // Chrome highlight at top
            val highlightCenter = Offset(center.x, center.y - innerRadius * 0.35f)
            drawCircle(
                brush = Brush.radialGradient(
                    colors = listOf(
                        Color.White.copy(alpha = 0.22f),
                        Color.Transparent
                    ),
                    center = highlightCenter,
                    radius = innerRadius * 0.5f
                ),
                radius = innerRadius * 0.5f,
                center = highlightCenter
            )

            // Record dot in center (when label would be "REC")
            if (stateLabel == "REC") {
                drawCircle(
                    color = Color.White.copy(alpha = 0.9f),
                    radius = innerRadius * 0.22f,
                    center = center
                )
            }
        }

        Spacer(modifier = Modifier.height(4.dp))

        Text(
            text = stateLabel,
            fontSize = 12.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            color = stateColor,
            textAlign = TextAlign.Center,
            letterSpacing = 1.sp
        )
    }
}

/**
 * Medium circular transport button (56dp) with a custom icon.
 *
 * Used for UNDO, STOP, and PLAY/PAUSE. When disabled, dims to muted
 * colors and ignores clicks.
 *
 * @param label       Short label below the button.
 * @param enabled     Whether the button is interactive.
 * @param buttonColor Optional override color for active state (defaults to TextSecondary).
 * @param iconPainter Lambda to draw the icon inside the button canvas.
 * @param onClick     Callback when pressed (only fires if enabled).
 */
@Composable
private fun TransportMediumButton(
    label: String,
    enabled: Boolean,
    buttonColor: Color = DesignSystem.TextSecondary,
    iconPainter: DrawScope.(center: Offset, radius: Float, color: Color) -> Unit,
    onClick: () -> Unit
) {
    val interactionSource = remember { MutableInteractionSource() }
    val isPressed by interactionSource.collectIsPressedAsState()

    val scale by animateFloatAsState(
        targetValue = if (isPressed && enabled) 0.93f else 1.0f,
        animationSpec = spring(dampingRatio = 0.55f, stiffness = 600f),
        label = "mediumButtonScale"
    )

    val effectiveColor = if (enabled) buttonColor else DesignSystem.TextMuted
    val labelColor = if (enabled) buttonColor else DesignSystem.TextMuted.copy(alpha = 0.5f)

    Column(
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Canvas(
            modifier = Modifier
                .size(56.dp)
                .graphicsLayer {
                    scaleX = scale
                    scaleY = scale
                }
                .clip(CircleShape)
                .clickable(
                    interactionSource = interactionSource,
                    indication = null,
                    enabled = enabled,
                    onClick = onClick
                )
        ) {
            val center = Offset(size.width / 2f, size.height / 2f)
            val outerRadius = size.minDimension / 2f
            val innerRadius = outerRadius * 0.80f

            // Outer ring
            drawCircle(
                color = Color(0xFF2A2A30),
                radius = outerRadius,
                center = center
            )

            // Inner face
            drawCircle(
                brush = Brush.radialGradient(
                    colors = listOf(
                        Color(0xFF3C3C42),
                        Color(0xFF323236)
                    ),
                    center = center,
                    radius = innerRadius
                ),
                radius = innerRadius,
                center = center
            )

            // Colored border stroke (state-aware)
            drawCircle(
                color = effectiveColor.copy(alpha = 0.4f),
                radius = outerRadius,
                center = center,
                style = Stroke(width = 1.5.dp.toPx())
            )

            // Draw the icon
            iconPainter(center, innerRadius * 0.45f, effectiveColor)
        }

        Spacer(modifier = Modifier.height(4.dp))

        Text(
            text = label,
            fontSize = 10.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            color = labelColor,
            textAlign = TextAlign.Center,
            letterSpacing = 1.sp
        )
    }
}

/**
 * Danger-styled circular transport button (56dp) for destructive actions.
 *
 * Uses [DesignSystem.DangerSurface] background and [DesignSystem.DangerText]
 * coloring so the button is visually distinct from all other transport buttons.
 * Draws an "X" icon inside when in default state, or a red filled circle
 * when in confirmation state (label == "TAP").
 *
 * @param label   Label below the button ("DELETE" or "TAP" for confirmation).
 * @param enabled Whether the button is interactive.
 * @param onClick Callback when pressed.
 */
@Composable
private fun TransportDangerButton(
    label: String,
    enabled: Boolean,
    onClick: () -> Unit
) {
    val interactionSource = remember { MutableInteractionSource() }
    val isPressed by interactionSource.collectIsPressedAsState()

    val scale by animateFloatAsState(
        targetValue = if (isPressed && enabled) 0.93f else 1.0f,
        animationSpec = spring(dampingRatio = 0.55f, stiffness = 600f),
        label = "dangerButtonScale"
    )

    val isConfirming = label == "TAP"
    val effectiveColor = if (enabled) DesignSystem.DangerText else DesignSystem.TextMuted
    val bgColor = if (isConfirming) Color(0xFF5A1A1A) else DesignSystem.DangerSurface

    Column(
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Canvas(
            modifier = Modifier
                .size(56.dp)
                .graphicsLayer {
                    scaleX = scale
                    scaleY = scale
                }
                .clip(CircleShape)
                .clickable(
                    interactionSource = interactionSource,
                    indication = null,
                    enabled = enabled,
                    onClick = onClick
                )
        ) {
            val center = Offset(size.width / 2f, size.height / 2f)
            val outerRadius = size.minDimension / 2f
            val innerRadius = outerRadius * 0.80f

            // Outer ring
            drawCircle(
                color = Color(0xFF2A2A30),
                radius = outerRadius,
                center = center
            )

            // Inner face (danger-tinted)
            drawCircle(
                color = bgColor,
                radius = innerRadius,
                center = center
            )

            // Red border stroke
            drawCircle(
                color = effectiveColor.copy(alpha = if (isConfirming) 0.7f else 0.4f),
                radius = outerRadius,
                center = center,
                style = Stroke(width = if (isConfirming) 2.dp.toPx() else 1.5.dp.toPx())
            )

            if (isConfirming) {
                // Pulsing red glow effect for confirmation
                drawCircle(
                    color = effectiveColor.copy(alpha = 0.15f),
                    radius = outerRadius * 1.15f,
                    center = center
                )
                // Filled red dot
                drawCircle(
                    color = effectiveColor.copy(alpha = 0.8f),
                    radius = innerRadius * 0.3f,
                    center = center
                )
            } else {
                // X icon (trash/delete symbol)
                drawDeleteIcon(center, innerRadius * 0.35f, effectiveColor)
            }
        }

        Spacer(modifier = Modifier.height(4.dp))

        Text(
            text = label,
            fontSize = 10.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            color = effectiveColor,
            textAlign = TextAlign.Center,
            letterSpacing = 1.sp
        )
    }
}

// -- Icon drawing functions ---------------------------------------------------

/**
 * Draw an undo arrow icon (curved arrow pointing left).
 */
private fun DrawScope.drawUndoIcon(center: Offset, radius: Float, color: Color) {
    drawArc(
        color = color,
        startAngle = -30f,
        sweepAngle = -240f,
        useCenter = false,
        style = Stroke(width = 2.dp.toPx(), cap = StrokeCap.Round),
        topLeft = Offset(center.x - radius, center.y - radius),
        size = Size(radius * 2f, radius * 2f)
    )

    val arrowTipAngle = Math.toRadians((-30f - 240f).toDouble())
    val tipX = center.x + (radius * kotlin.math.cos(arrowTipAngle)).toFloat()
    val tipY = center.y + (radius * kotlin.math.sin(arrowTipAngle)).toFloat()
    val arrowSize = radius * 0.4f

    drawLine(
        color = color,
        start = Offset(tipX, tipY),
        end = Offset(tipX + arrowSize, tipY - arrowSize * 0.6f),
        strokeWidth = 2.dp.toPx(),
        cap = StrokeCap.Round
    )
    drawLine(
        color = color,
        start = Offset(tipX, tipY),
        end = Offset(tipX + arrowSize, tipY + arrowSize * 0.6f),
        strokeWidth = 2.dp.toPx(),
        cap = StrokeCap.Round
    )
}

/**
 * Draw a stop icon (filled square).
 */
private fun DrawScope.drawStopIcon(center: Offset, halfSize: Float, color: Color) {
    drawRect(
        color = color,
        topLeft = Offset(center.x - halfSize, center.y - halfSize),
        size = Size(halfSize * 2f, halfSize * 2f)
    )
}

/**
 * Draw a play icon (right-pointing triangle).
 */
private fun DrawScope.drawPlayIcon(center: Offset, radius: Float, color: Color) {
    val path = Path().apply {
        // Right-pointing triangle, slightly offset right for optical centering
        val offsetX = radius * 0.15f
        moveTo(center.x - radius * 0.7f + offsetX, center.y - radius)
        lineTo(center.x + radius + offsetX, center.y)
        lineTo(center.x - radius * 0.7f + offsetX, center.y + radius)
        close()
    }
    drawPath(path = path, color = color)
}

/**
 * Draw a pause icon (two vertical bars).
 */
private fun DrawScope.drawPauseIcon(center: Offset, halfSize: Float, color: Color) {
    val barWidth = halfSize * 0.35f
    val gap = halfSize * 0.25f

    // Left bar
    drawRect(
        color = color,
        topLeft = Offset(center.x - gap - barWidth, center.y - halfSize),
        size = Size(barWidth, halfSize * 2f)
    )
    // Right bar
    drawRect(
        color = color,
        topLeft = Offset(center.x + gap, center.y - halfSize),
        size = Size(barWidth, halfSize * 2f)
    )
}

/**
 * Draw a delete/X icon (two crossed lines).
 */
private fun DrawScope.drawDeleteIcon(center: Offset, halfSize: Float, color: Color) {
    val strokeWidth = 2.5.dp.toPx()
    drawLine(
        color = color,
        start = Offset(center.x - halfSize, center.y - halfSize),
        end = Offset(center.x + halfSize, center.y + halfSize),
        strokeWidth = strokeWidth,
        cap = StrokeCap.Round
    )
    drawLine(
        color = color,
        start = Offset(center.x + halfSize, center.y - halfSize),
        end = Offset(center.x - halfSize, center.y + halfSize),
        strokeWidth = strokeWidth,
        cap = StrokeCap.Round
    )
}
