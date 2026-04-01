package com.guitaremulator.app.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.PointerEventPass
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.dp
import com.guitaremulator.app.ui.theme.DesignSystem
import kotlin.math.roundToInt

// -- Layout Constants ----------------------------------------------------------

/** Total composable height. Provides room for notch marks above the track + thumb shadow below. */
private val SLIDER_HEIGHT = 40.dp

/** Height of the recessed groove track. */
private val TRACK_HEIGHT = 6.dp

/** Corner radius for the groove track ends. */
private val TRACK_CORNER_RADIUS = 3.dp

/** Vertical center of the track, measured from the top of the composable. */
private val TRACK_CENTER_Y = 24.dp

/** Diameter of the metallic thumb knob. */
private val THUMB_DIAMETER = 20.dp

/** Radius of the chrome highlight dot on the thumb. */
private val CHROME_DOT_RADIUS = 2.dp

/** Horizontal padding from composable edges to usable track area (half thumb so it doesn't clip). */
private val TRACK_HORIZONTAL_PADDING = 10.dp // THUMB_DIAMETER / 2

/** Height of the notch tick marks above the groove. */
private val NOTCH_HEIGHT = 4.dp

/** Width of each notch tick mark. */
private val NOTCH_WIDTH = 1.dp

/** Thumb drop-shadow vertical offset. */
private val SHADOW_OFFSET_Y = 2.dp

/** Thumb drop-shadow blur approximation radius. */
private val SHADOW_BLUR_RADIUS = 3.dp

/**
 * A custom Canvas-drawn horizontal slider styled to match the "Blackface Terminal"
 * skeuomorphic design language of the Guitar Emulator app.
 *
 * The slider features a recessed groove track with inner shadow / highlight edges,
 * an active fill region, optional discrete notch marks, and a metallic thumb knob
 * with radial gradient, chrome dot highlight, and drop shadow.
 *
 * ## Drawing Order (bottom to top)
 * 1. Notch tick marks above the groove (if [steps] > 0)
 * 2. Recessed groove background with inner shadow and bottom highlight
 * 3. Active fill from left edge to thumb position
 * 4. Thumb drop shadow
 * 5. Thumb outer ring
 * 6. Thumb body (radial gradient)
 * 7. Chrome highlight dot
 *
 * ## Gesture Handling
 * Uses `awaitEachGesture` with `awaitFirstDown` for zero-slop immediate response.
 * Supports both tap-to-position and horizontal drag. For discrete [steps], the
 * value snaps to the nearest step position. All pointer events are consumed to
 * prevent parent scrolling during interaction.
 *
 * @param value Current slider value, clamped to [valueRange].
 * @param onValueChange Callback invoked with the new value during drag or tap.
 * @param modifier Modifier applied to the outer Canvas.
 * @param valueRange The closed range of valid values. Defaults to 0f..1f.
 * @param steps Number of discrete intermediate steps (0 = continuous). When > 0,
 *   the slider snaps to N+1 evenly spaced positions (including both endpoints),
 *   matching Material3 Slider semantics.
 * @param accentColor Color used for the active fill region. Defaults to [DesignSystem.SafeGreen].
 * @param enabled When false, the slider is drawn at 40% opacity and ignores gestures.
 */
@Composable
fun HardwareSlider(
    value: Float,
    onValueChange: (Float) -> Unit,
    modifier: Modifier = Modifier,
    valueRange: ClosedFloatingPointRange<Float> = 0f..1f,
    steps: Int = 0,
    accentColor: Color = DesignSystem.SafeGreen,
    enabled: Boolean = true,
) {
    val density = LocalDensity.current

    // Pre-compute pixel dimensions once (stable across recompositions for same density).
    val layoutPx = remember(density) {
        with(density) {
            SliderLayoutPx(
                trackHeight = TRACK_HEIGHT.toPx(),
                trackCornerRadius = TRACK_CORNER_RADIUS.toPx(),
                trackCenterY = TRACK_CENTER_Y.toPx(),
                trackHPadding = TRACK_HORIZONTAL_PADDING.toPx(),
                thumbRadius = THUMB_DIAMETER.toPx() / 2f,
                chromeDotRadius = CHROME_DOT_RADIUS.toPx(),
                notchHeight = NOTCH_HEIGHT.toPx(),
                notchWidth = NOTCH_WIDTH.toPx(),
                shadowOffsetY = SHADOW_OFFSET_Y.toPx(),
                shadowBlurRadius = SHADOW_BLUR_RADIUS.toPx(),
                innerShadowStroke = 1.dp.toPx(),
                outerRingStroke = 1.dp.toPx(),
            )
        }
    }

    // Pre-compute the active fill color (accent at 70% opacity).
    val fillColor = remember(accentColor) { accentColor.copy(alpha = 0.70f) }

    // Pre-compute inner shadow and highlight colors for the recessed groove.
    val innerShadowColor = remember { DesignSystem.PanelShadow.copy(alpha = 0.40f) }
    val bottomHighlightColor = remember { DesignSystem.PanelHighlight.copy(alpha = 0.30f) }
    val chromeDotColor = remember { DesignSystem.ChromeHighlight.copy(alpha = 0.80f) }

    // Use rememberUpdatedState so the gesture coroutine always reads the latest
    // value/callback without restarting (prevents gesture cancellation mid-drag).
    val currentValue by rememberUpdatedState(value)
    val currentOnValueChange by rememberUpdatedState(onValueChange)
    val currentRange by rememberUpdatedState(valueRange)

    Canvas(
        modifier = modifier
            .fillMaxWidth()
            .height(SLIDER_HEIGHT)
            .graphicsLayer {
                alpha = if (enabled) 1f else 0.4f
            }
            .then(
                if (enabled) {
                    Modifier.pointerInput(Unit) {
                        awaitEachGesture {
                            // Wait for first finger down -- zero slop for immediate response.
                            val down = awaitFirstDown(requireUnconsumed = false)
                            down.consume()

                            val trackWidth = size.width - layoutPx.trackHPadding * 2f

                            // Compute value from an x-position within the composable.
                            fun xToValue(x: Float): Float {
                                val range = currentRange
                                val fraction = ((x - layoutPx.trackHPadding) / trackWidth)
                                    .coerceIn(0f, 1f)
                                val raw = range.start + fraction * (range.endInclusive - range.start)
                                return snapToStep(raw, range, steps)
                            }

                            // Tap-to-position on initial touch.
                            currentOnValueChange(xToValue(down.position.x))

                            // Track drag until finger lifts.
                            val pointerId = down.id
                            while (true) {
                                val event = awaitPointerEvent(PointerEventPass.Main)
                                val change = event.changes.firstOrNull { it.id == pointerId }
                                    ?: break
                                if (!change.pressed) break

                                change.consume()
                                currentOnValueChange(xToValue(change.position.x))
                            }
                        }
                    }
                } else {
                    Modifier
                }
            )
    ) {
        val canvasWidth = size.width
        val trackLeft = layoutPx.trackHPadding
        val trackRight = canvasWidth - layoutPx.trackHPadding
        val trackWidth = trackRight - trackLeft
        val trackTop = layoutPx.trackCenterY - layoutPx.trackHeight / 2f
        val cornerRadius = CornerRadius(layoutPx.trackCornerRadius)

        // Normalize current value to 0..1 fraction within the range.
        val range = valueRange
        val rangeSpan = (range.endInclusive - range.start).coerceAtLeast(Float.MIN_VALUE)
        val fraction = ((value.coerceIn(range.start, range.endInclusive) - range.start) / rangeSpan)
            .coerceIn(0f, 1f)

        val thumbCenterX = trackLeft + fraction * trackWidth

        // -- Layer 1: Notch marks (above the groove) --------------------------
        if (steps > 0) {
            val totalPositions = steps + 1 // endpoints + intermediate steps
            for (i in 0 until totalPositions) {
                val stepFraction = i.toFloat() / steps.toFloat()
                val notchX = trackLeft + stepFraction * trackWidth
                val notchTop = trackTop - layoutPx.notchHeight - 2.dp.toPx()

                drawRect(
                    color = DesignSystem.TextMuted,
                    topLeft = Offset(notchX - layoutPx.notchWidth / 2f, notchTop),
                    size = Size(layoutPx.notchWidth, layoutPx.notchHeight)
                )
            }
        }

        // -- Layer 2: Recessed groove background ------------------------------
        drawRoundRect(
            color = DesignSystem.MeterBg,
            topLeft = Offset(trackLeft, trackTop),
            size = Size(trackWidth, layoutPx.trackHeight),
            cornerRadius = cornerRadius
        )

        // Inner shadow at top of groove (light appears to come from above).
        drawLine(
            color = innerShadowColor,
            start = Offset(trackLeft + layoutPx.trackCornerRadius, trackTop + layoutPx.innerShadowStroke / 2f),
            end = Offset(trackRight - layoutPx.trackCornerRadius, trackTop + layoutPx.innerShadowStroke / 2f),
            strokeWidth = layoutPx.innerShadowStroke
        )

        // Highlight at bottom of groove (reflected light from below).
        drawLine(
            color = bottomHighlightColor,
            start = Offset(
                trackLeft + layoutPx.trackCornerRadius,
                trackTop + layoutPx.trackHeight - layoutPx.innerShadowStroke / 2f
            ),
            end = Offset(
                trackRight - layoutPx.trackCornerRadius,
                trackTop + layoutPx.trackHeight - layoutPx.innerShadowStroke / 2f
            ),
            strokeWidth = layoutPx.innerShadowStroke
        )

        // -- Layer 3: Active fill from left to thumb --------------------------
        if (fraction > 0f) {
            val fillWidth = fraction * trackWidth
            drawRoundRect(
                color = fillColor,
                topLeft = Offset(trackLeft, trackTop),
                size = Size(fillWidth, layoutPx.trackHeight),
                cornerRadius = cornerRadius
            )
        }

        // -- Layer 4: Thumb drop shadow ---------------------------------------
        drawCircle(
            color = DesignSystem.PanelShadow.copy(alpha = 0.45f),
            radius = layoutPx.thumbRadius + layoutPx.shadowBlurRadius / 2f,
            center = Offset(thumbCenterX, layoutPx.trackCenterY + layoutPx.shadowOffsetY)
        )

        // -- Layer 5: Thumb outer ring ----------------------------------------
        drawCircle(
            color = DesignSystem.KnobOuterRing,
            radius = layoutPx.thumbRadius,
            center = Offset(thumbCenterX, layoutPx.trackCenterY),
            style = Stroke(width = layoutPx.outerRingStroke)
        )

        // -- Layer 6: Thumb body (radial gradient) ----------------------------
        // Gradient center offset toward top-left to simulate directional lighting.
        val gradientCenter = Offset(
            thumbCenterX - layoutPx.thumbRadius * 0.20f,
            layoutPx.trackCenterY - layoutPx.thumbRadius * 0.20f
        )
        drawCircle(
            brush = Brush.radialGradient(
                colors = listOf(DesignSystem.KnobHighlight, DesignSystem.KnobBody),
                center = gradientCenter,
                radius = layoutPx.thumbRadius
            ),
            radius = layoutPx.thumbRadius - layoutPx.outerRingStroke / 2f,
            center = Offset(thumbCenterX, layoutPx.trackCenterY)
        )

        // -- Layer 7: Chrome highlight dot ------------------------------------
        // Small specular reflection offset toward top-left of the thumb.
        drawCircle(
            color = chromeDotColor,
            radius = layoutPx.chromeDotRadius,
            center = Offset(
                thumbCenterX - layoutPx.thumbRadius * 0.30f,
                layoutPx.trackCenterY - layoutPx.thumbRadius * 0.30f
            )
        )
    }
}

// -- Helpers ------------------------------------------------------------------

/**
 * Snaps a raw value to the nearest discrete step position within [range].
 *
 * When [steps] is 0 (continuous), the raw value is returned clamped to the range.
 * When [steps] > 0, the value is quantized to one of (steps + 1) evenly spaced
 * positions spanning [range.start] to [range.endInclusive].
 *
 * @param raw The unquantized value.
 * @param range The valid value range.
 * @param steps Number of intermediate steps (0 = continuous).
 * @return The snapped value, guaranteed within [range].
 */
private fun snapToStep(
    raw: Float,
    range: ClosedFloatingPointRange<Float>,
    steps: Int
): Float {
    val clamped = raw.coerceIn(range.start, range.endInclusive)
    if (steps <= 0) return clamped

    val rangeSpan = range.endInclusive - range.start
    if (rangeSpan <= 0f) return range.start

    val fraction = (clamped - range.start) / rangeSpan
    val stepFraction = 1f / steps.toFloat()
    val snappedFraction = (fraction / stepFraction).roundToInt() * stepFraction
    return (range.start + snappedFraction * rangeSpan).coerceIn(range.start, range.endInclusive)
}

/**
 * Pre-computed pixel dimensions for the slider layout.
 *
 * Cached in a `remember` block keyed on density so that dp-to-px conversion
 * happens once rather than on every draw frame.
 */
private data class SliderLayoutPx(
    val trackHeight: Float,
    val trackCornerRadius: Float,
    val trackCenterY: Float,
    val trackHPadding: Float,
    val thumbRadius: Float,
    val chromeDotRadius: Float,
    val notchHeight: Float,
    val notchWidth: Float,
    val shadowOffsetY: Float,
    val shadowBlurRadius: Float,
    val innerShadowStroke: Float,
    val outerRingStroke: Float,
)
