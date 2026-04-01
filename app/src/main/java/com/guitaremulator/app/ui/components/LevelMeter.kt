package com.guitaremulator.app.ui.components

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import kotlinx.coroutines.delay
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.ui.theme.DesignSystem

// ── LED Segment Constants ────────────────────────────────────────────────────

/** Total number of discrete LED segments in the meter. */
private const val SEGMENT_COUNT = 12

/** Gap between adjacent LED segments in pixels. */
private const val SEGMENT_GAP_PX = 1f

/** Corner radius for each LED segment in dp. */
private val SEGMENT_CORNER_RADIUS = 1.dp

/** Alpha for inactive (unlit) LED segments, creating faint visible outlines. */
private const val INACTIVE_SEGMENT_ALPHA = 0.15f

/** Height of the ambient glow above each active LED segment in dp. */
private val GLOW_HEIGHT = 3.dp

/** Alpha for the ambient glow emanating from active LED segments. */
private const val GLOW_ALPHA = 0.08f

/** Width of the peak-hold indicator segment in dp. */
private val PEAK_HOLD_WIDTH = 2.dp

/** Duration (ms) the peak-hold indicator persists after the level drops. */
private const val PEAK_HOLD_DECAY_MS = 2500L

/**
 * Determine the color for a given LED segment index.
 *
 * Segments 0-6 (indices 0..6) are green (normal level), segments 7-9 are
 * yellow (hot level), and segments 10-11 are red (clipping). This matches
 * the physical LED meter color layout on professional studio rack gear.
 *
 * @param segmentIndex Zero-based segment index (0 = bottom, 11 = top).
 * @return The segment color from [DesignSystem] tokens.
 */
private fun segmentColor(segmentIndex: Int): Color = when {
    segmentIndex >= 10 -> DesignSystem.MeterRed      // Segments 10-11: red (clipping)
    segmentIndex >= 7  -> DesignSystem.MeterYellow    // Segments 7-9: yellow (hot)
    else               -> DesignSystem.MeterGreen     // Segments 0-6: green (normal)
}

/**
 * Vertical level meter bar with discrete LED-segment rendering, peak hold,
 * and ambient glow.
 *
 * ## Visual Design ("Blackface Terminal" Hardware Aesthetic)
 * - **12 discrete LED segments** with 1px gaps, replacing the continuous gradient bar.
 *   Each segment is a small rounded rectangle (1dp corner radius). Active segments
 *   (below current level) are drawn at full color; inactive segments are drawn at
 *   15% alpha as faint outlines, mimicking unlit LEDs on hardware meters.
 * - **Color zones**: segments 0-6 green, 7-9 yellow, 10-11 red.
 * - **Peak hold**: A 2dp-wide bright segment drawn at the peak position, persisting
 *   for 2.5 seconds after the level drops below it.
 * - **Ambient glow**: Active segments emit a subtle upward glow (3dp height, segment
 *   color at 8% alpha fading to transparent), simulating LED light reflecting on the
 *   amp panel surface above.
 *
 * Uses tween animation for smooth metering that tracks the audio level
 * without being jittery.
 *
 * @param label Text label displayed below the meter.
 * @param level Signal level in [0.0, 1.0].
 * @param modifier Modifier for layout.
 */
@Composable
internal fun LevelMeter(
    label: String,
    level: Float,
    modifier: Modifier = Modifier
) {
    // Smooth animation prevents jittery meter movement
    val animatedLevel by animateFloatAsState(
        targetValue = level.coerceIn(0f, 1f),
        animationSpec = tween(durationMillis = 50),
        label = "levelAnimation"
    )

    // Peak-hold tracking: captures the highest level and decays after PEAK_HOLD_DECAY_MS
    var peakLevel by remember { mutableFloatStateOf(0f) }
    if (animatedLevel > peakLevel) {
        peakLevel = animatedLevel
    }

    // Animated peak hold indicator
    val animatedPeak by animateFloatAsState(
        targetValue = peakLevel,
        animationSpec = tween(durationMillis = 50),
        label = "peakAnimation"
    )

    // Peak-hold decay timer: resets peak to current level after timeout
    LaunchedEffect(peakLevel) {
        if (peakLevel > 0f) {
            delay(PEAK_HOLD_DECAY_MS)
            peakLevel = animatedLevel
        }
    }

    // Peak-hold clipping indicator: stays lit for 2.5 seconds after clip.
    var isClipping by remember { mutableStateOf(false) }
    if (animatedLevel > 0.85f) {
        isClipping = true
    }
    LaunchedEffect(isClipping) {
        if (isClipping) {
            delay(2500L)
            isClipping = false
        }
    }

    // Convert level to dBFS for display
    val dbfs = if (animatedLevel > 0.001f) {
        (20f * kotlin.math.log10(animatedLevel)).coerceAtLeast(-60f)
    } else {
        -60f
    }

    Column(
        modifier = modifier,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // Clip indicator LED
        Box(
            modifier = Modifier
                .size(width = 28.dp, height = 10.dp)
                .clip(RoundedCornerShape(3.dp))
                .background(
                    if (isClipping) DesignSystem.ClipRed else Color(0xFF2A1A1A)
                ),
            contentAlignment = Alignment.Center
        ) {
            Text(
                text = "CLIP",
                fontSize = 7.sp,
                fontWeight = FontWeight.Bold,
                color = if (isClipping) Color.White else Color(0xFF553333)
            )
        }

        Spacer(modifier = Modifier.height(3.dp))

        // LED-segment meter bar drawn on Canvas
        LedSegmentMeterCanvas(
            animatedLevel = animatedLevel,
            peakLevel = animatedPeak,
            modifier = Modifier
                .fillMaxWidth(0.5f)
                .weight(1f)
        )

        Spacer(modifier = Modifier.height(4.dp))

        // Label
        Text(
            text = label,
            fontSize = 10.sp,
            color = DesignSystem.TextSecondary
        )

        // dBFS readout
        Text(
            text = if (animatedLevel > 0.001f) {
                String.format("%.0f dBFS", dbfs)
            } else {
                "-\u221E"
            },
            fontSize = 11.sp,
            color = if (isClipping) DesignSystem.ClipRed else DesignSystem.TextPrimary
        )
    }
}

// ── LED Segment Meter Canvas ─────────────────────────────────────────────────

/**
 * Canvas that renders the vertical LED-segment meter with 12 discrete segments,
 * peak-hold indicator, and ambient glow.
 *
 * All drawing is done in [DrawScope] with no bitmap assets. Segment colors,
 * glow gradients, and the peak-hold indicator are pre-computed from design tokens.
 *
 * @param animatedLevel Current smoothed signal level in [0.0, 1.0].
 * @param peakLevel     Current peak-hold level in [0.0, 1.0].
 * @param modifier      Modifier for sizing.
 */
@Composable
private fun LedSegmentMeterCanvas(
    animatedLevel: Float,
    peakLevel: Float,
    modifier: Modifier = Modifier
) {
    // Pre-compute segment colors array outside the draw lambda
    val segmentColors = remember {
        Array(SEGMENT_COUNT) { i -> segmentColor(i) }
    }
    val meterBg = DesignSystem.MeterBg

    Canvas(modifier = modifier) {
        val canvasWidth = size.width
        val canvasHeight = size.height
        val cornerRadius = CornerRadius(SEGMENT_CORNER_RADIUS.toPx(), SEGMENT_CORNER_RADIUS.toPx())
        val glowHeightPx = GLOW_HEIGHT.toPx()
        val peakHoldWidthPx = PEAK_HOLD_WIDTH.toPx()

        // Background track (dark meter well)
        drawRoundRect(
            color = meterBg,
            topLeft = Offset.Zero,
            size = Size(canvasWidth, canvasHeight),
            cornerRadius = CornerRadius(4.dp.toPx(), 4.dp.toPx())
        )

        // Calculate segment geometry:
        // Total gap space = (SEGMENT_COUNT - 1) * SEGMENT_GAP_PX
        val totalGap = (SEGMENT_COUNT - 1) * SEGMENT_GAP_PX
        val segmentHeight = (canvasHeight - totalGap) / SEGMENT_COUNT

        // Determine which segment the current level reaches
        // Level 0.0 = no segments lit, Level 1.0 = all 12 segments lit
        val activeSegments = (animatedLevel * SEGMENT_COUNT).toInt()
            .coerceIn(0, SEGMENT_COUNT)

        // Determine the peak-hold segment index
        val peakSegment = ((peakLevel * SEGMENT_COUNT) - 1).toInt()
            .coerceIn(0, SEGMENT_COUNT - 1)

        // Draw each LED segment (bottom-up: index 0 = bottom, index 11 = top)
        for (i in 0 until SEGMENT_COUNT) {
            val color = segmentColors[i]

            // Y position: segment 0 is at the bottom, segment 11 at the top
            val segTop = canvasHeight - (i + 1) * segmentHeight - i * SEGMENT_GAP_PX
            val segSize = Size(canvasWidth, segmentHeight)

            val isActive = i < activeSegments

            if (isActive) {
                // Active segment: full color
                drawRoundRect(
                    color = color,
                    topLeft = Offset(0f, segTop),
                    size = segSize,
                    cornerRadius = cornerRadius
                )

                // Ambient glow: gradient above the segment (LED light reflecting upward)
                // Only draw if there is room above (not the topmost pixel)
                if (segTop > glowHeightPx) {
                    drawRect(
                        brush = Brush.verticalGradient(
                            colors = listOf(Color.Transparent, color.copy(alpha = GLOW_ALPHA)),
                            startY = segTop - glowHeightPx,
                            endY = segTop
                        ),
                        topLeft = Offset(0f, segTop - glowHeightPx),
                        size = Size(canvasWidth, glowHeightPx)
                    )
                }
            } else {
                // Inactive segment: faint outline at 15% alpha (unlit LED visible)
                drawRoundRect(
                    color = color.copy(alpha = INACTIVE_SEGMENT_ALPHA),
                    topLeft = Offset(0f, segTop),
                    size = segSize,
                    cornerRadius = cornerRadius
                )
            }
        }

        // Peak-hold indicator: a thin bright segment (2dp wide) at the peak position
        if (peakLevel > 0.01f) {
            val peakColor = segmentColors[peakSegment]
            val peakTop = canvasHeight - (peakSegment + 1) * segmentHeight - peakSegment * SEGMENT_GAP_PX

            drawRoundRect(
                color = peakColor,
                topLeft = Offset(0f, peakTop),
                size = Size(canvasWidth, peakHoldWidthPx.coerceAtMost(segmentHeight)),
                cornerRadius = cornerRadius
            )
        }
    }
}
