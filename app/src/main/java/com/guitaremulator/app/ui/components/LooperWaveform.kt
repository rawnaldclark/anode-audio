package com.guitaremulator.app.ui.components

import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.ui.theme.DesignSystem
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin

// -- State color constants (matching LooperOverlay) ----------------------------

private val StateColorIdle = DesignSystem.ModuleBorder
private val StateColorRecording = Color(0xFFE53935)
private val StateColorPlaying = Color(0xFF43A047)
private val StateColorOverdubbing = Color(0xFFFFA000)
private val StateColorCountIn = Color(0xFFE53935)
private val StateColorFinishing = Color(0xFFFFA000)

// -- Looper state constants ---------------------------------------------------

private const val STATE_IDLE = 0
private const val STATE_RECORDING = 1
private const val STATE_PLAYING = 2
private const val STATE_OVERDUBBING = 3
private const val STATE_FINISHING = 4
private const val STATE_COUNT_IN = 5

/**
 * Circular waveform display with playback position indicator.
 *
 * Draws a ring-shaped loop visualization with:
 * 1. A dark background circle filling the available space.
 * 2. A track ring (the full loop circumference) in subdued color.
 * 3. A progress arc showing the current playback position, colored by
 *    the current looper state (red for recording, green for playing, etc.).
 * 4. Beat tick marks spaced evenly around the ring based on [beatsPerBar].
 * 5. A rotating position dot on the ring edge at the current playback angle.
 * 6. Center text showing the state abbreviation (REC/PLAY/OVR/IDLE).
 *
 * When the looper is in COUNT_IN state, the progress arc pulses to
 * indicate the countdown visually.
 *
 * @param playbackPosition Current position in samples within the loop.
 * @param loopLength       Total loop length in samples (0 if no loop recorded).
 * @param looperState      Current looper state (0=Idle..5=CountIn).
 * @param currentBeat      Current beat within the bar (0-based).
 * @param beatsPerBar      Number of beats per bar (for tick mark spacing).
 * @param modifier         Layout modifier.
 */
@Composable
fun LooperWaveform(
    playbackPosition: Int,
    loopLength: Int,
    looperState: Int,
    currentBeat: Int,
    beatsPerBar: Int,
    modifier: Modifier = Modifier
) {
    val textMeasurer = rememberTextMeasurer()

    // Resolve the state color for the progress arc and position dot
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

    // State abbreviation for center text
    val stateText = remember(looperState) {
        when (looperState) {
            STATE_RECORDING -> "REC"
            STATE_PLAYING -> "PLAY"
            STATE_OVERDUBBING -> "OVR"
            STATE_COUNT_IN -> "COUNT"
            STATE_FINISHING -> "FIN"
            else -> "IDLE"
        }
    }

    // Calculate the progress fraction (0.0 to 1.0)
    val progressFraction = if (loopLength > 0) {
        (playbackPosition.toFloat() / loopLength.toFloat()).coerceIn(0f, 1f)
    } else {
        0f
    }

    // Smooth the position animation for visual fluidity
    val animatedProgress by animateFloatAsState(
        targetValue = progressFraction,
        animationSpec = tween(durationMillis = 50, easing = LinearEasing),
        label = "looperProgress"
    )

    // Pulsing alpha for COUNT_IN state
    val infiniteTransition = rememberInfiniteTransition(label = "countInPulse")
    val pulseAlpha by infiniteTransition.animateFloat(
        initialValue = 0.4f,
        targetValue = 1.0f,
        animationSpec = infiniteRepeatable(
            animation = tween(durationMillis = 400, easing = LinearEasing),
            repeatMode = RepeatMode.Reverse
        ),
        label = "pulseAlpha"
    )

    // Pre-compute text styles
    val stateTextStyle = TextStyle(
        fontSize = 20.sp,
        fontWeight = FontWeight.Bold,
        fontFamily = FontFamily.Monospace,
        color = DesignSystem.TextPrimary,
        textAlign = TextAlign.Center
    )

    val beatTextStyle = TextStyle(
        fontSize = 12.sp,
        fontWeight = FontWeight.Normal,
        fontFamily = FontFamily.Monospace,
        color = DesignSystem.TextSecondary,
        textAlign = TextAlign.Center
    )

    Canvas(modifier = modifier.size(200.dp)) {
        val canvasSize = size.minDimension
        val center = Offset(size.width / 2f, size.height / 2f)
        val outerRadius = canvasSize / 2f - 16.dp.toPx()
        val ringWidth = 8.dp.toPx()
        val innerRadius = outerRadius - ringWidth

        // 1. Dark background circle
        drawCircle(
            color = DesignSystem.Background,
            radius = outerRadius + 8.dp.toPx(),
            center = center
        )

        // 2. Track ring (full loop circumference, subdued)
        drawCircle(
            color = DesignSystem.ModuleBorder.copy(alpha = 0.5f),
            radius = outerRadius,
            center = center,
            style = Stroke(width = ringWidth)
        )

        // 3. Progress arc (colored by state)
        if (loopLength > 0 && looperState != STATE_IDLE) {
            val arcColor = if (looperState == STATE_COUNT_IN) {
                stateColor.copy(alpha = pulseAlpha)
            } else {
                stateColor
            }

            val sweepAngle = animatedProgress * 360f

            drawArc(
                color = arcColor,
                startAngle = -90f, // Start from top (12 o'clock)
                sweepAngle = sweepAngle,
                useCenter = false,
                style = Stroke(width = ringWidth, cap = StrokeCap.Round),
                topLeft = Offset(
                    center.x - outerRadius,
                    center.y - outerRadius
                ),
                size = androidx.compose.ui.geometry.Size(
                    outerRadius * 2f,
                    outerRadius * 2f
                )
            )
        }

        // 4. Beat tick marks around the ring
        val totalBeats = beatsPerBar.coerceAtLeast(1)
        val tickOuterRadius = outerRadius + 4.dp.toPx()
        val tickInnerRadius = outerRadius - ringWidth - 4.dp.toPx()
        val tickLongInnerRadius = outerRadius - ringWidth - 8.dp.toPx()

        for (beat in 0 until totalBeats) {
            val angle = (beat.toFloat() / totalBeats.toFloat()) * 2.0 * PI - PI / 2.0
            val isDownbeat = beat == 0
            val isCurrent = beat == currentBeat && looperState != STATE_IDLE

            val tickInner = if (isDownbeat) tickLongInnerRadius else tickInnerRadius
            val tickColor = when {
                isCurrent -> stateColor
                isDownbeat -> DesignSystem.TextPrimary.copy(alpha = 0.8f)
                else -> DesignSystem.TextMuted.copy(alpha = 0.5f)
            }
            val tickWidth = if (isDownbeat) 3.dp.toPx() else 1.5f.dp.toPx()

            val outerX = center.x + (tickOuterRadius * cos(angle)).toFloat()
            val outerY = center.y + (tickOuterRadius * sin(angle)).toFloat()
            val innerX = center.x + (tickInner * cos(angle)).toFloat()
            val innerY = center.y + (tickInner * sin(angle)).toFloat()

            drawLine(
                color = tickColor,
                start = Offset(outerX, outerY),
                end = Offset(innerX, innerY),
                strokeWidth = tickWidth,
                cap = StrokeCap.Round
            )
        }

        // 5. Position dot on the ring edge
        if (loopLength > 0 && looperState != STATE_IDLE) {
            val dotAngle = animatedProgress * 2.0 * PI - PI / 2.0
            val dotX = center.x + (outerRadius * cos(dotAngle)).toFloat()
            val dotY = center.y + (outerRadius * sin(dotAngle)).toFloat()
            val dotRadius = 5.dp.toPx()

            // Glow behind the dot
            drawCircle(
                color = stateColor.copy(alpha = 0.3f),
                radius = dotRadius * 2f,
                center = Offset(dotX, dotY)
            )

            // Solid dot
            drawCircle(
                color = stateColor,
                radius = dotRadius,
                center = Offset(dotX, dotY)
            )

            // Bright center
            drawCircle(
                color = Color.White.copy(alpha = 0.6f),
                radius = dotRadius * 0.3f,
                center = Offset(dotX, dotY)
            )
        }

        // 6. Center text: state abbreviation
        val measuredState = textMeasurer.measure(stateText, stateTextStyle)
        drawText(
            textLayoutResult = measuredState,
            topLeft = Offset(
                center.x - measuredState.size.width / 2f,
                center.y - measuredState.size.height / 2f - 6.dp.toPx()
            )
        )

        // Beat counter below state text (e.g., "3 / 4")
        if (looperState != STATE_IDLE) {
            val beatDisplay = "${currentBeat + 1} / $beatsPerBar"
            val measuredBeat = textMeasurer.measure(beatDisplay, beatTextStyle)
            drawText(
                textLayoutResult = measuredBeat,
                topLeft = Offset(
                    center.x - measuredBeat.size.width / 2f,
                    center.y + measuredState.size.height / 2f - 2.dp.toPx()
                )
            )
        }
    }
}
