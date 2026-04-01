package com.guitaremulator.app.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.PointerEventPass
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.input.pointer.positionChange
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.ui.theme.DesignSystem
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.roundToInt
import kotlin.math.sin

/**
 * Available sizes for the rotary knob, corresponding to small parameter
 * controls, standard knobs, and featured/primary parameter knobs.
 *
 * @property dp The diameter of the knob in density-independent pixels.
 */
enum class KnobSize(val dp: Dp) {
    SMALL(44.dp),
    STANDARD(56.dp),
    LARGE(68.dp)
}

/**
 * Visual style for the knob's position indicator.
 *
 * - [LINE]: Classic thin white line from center toward edge (default, backward compatible).
 * - [CHICKEN_HEAD]: Triangular pointer inspired by vintage Fender chicken-head knobs.
 * - [DOT]: Small circular dot near the knob edge, common on modern amp controls.
 */
enum class KnobPointerStyle {
    LINE,
    CHICKEN_HEAD,
    DOT
}

// ── Constants ───────────────────────────────────────────────────────────────

/** Start angle for the value arc and pointer, in degrees from the 3 o'clock axis (clockwise). */
private const val ARC_START_ANGLE_DEG = 150f

/** Total angular sweep of the knob range (from 7 o'clock to 5 o'clock, clockwise). */
private const val ARC_SWEEP_ANGLE_DEG = 270f

/** Base vertical drag distance (in dp) to traverse the full 0..1 range. */
private const val BASE_DRAG_RANGE_DP = 200f

/** Degrees-to-radians conversion factor. */
private const val DEG_TO_RAD = Math.PI.toFloat() / 180f

/** Number of scale markings on the numbered ring (0 through 10 inclusive). */
private const val SCALE_MARK_COUNT = 11

/** Drop shadow vertical offset as a fraction of knob radius. */
private const val DROP_SHADOW_OFFSET_FRACTION = 0.04f

/**
 * Cached [Path] for chicken-head pointer drawing, avoiding per-frame allocation.
 * Safe because DrawScope runs on a single UI thread.
 */
private val chickenHeadPathCache = Path()

/**
 * A custom Canvas-drawn rotary knob inspired by Guitar Rig 7's 3D metallic
 * knob aesthetic with skeuomorphic "Blackface Terminal" enhancements.
 *
 * The knob responds to vertical drag gestures: dragging up increases the
 * value, dragging down decreases it. For enum parameters, the value snaps
 * to the nearest discrete step position.
 *
 * ## Drawing Order (bottom to top)
 * 1. Drop shadow -- subtle offset dark circle beneath everything
 * 2. Value arc glow -- wider, semi-transparent arc for LED underglow effect
 * 3. Value arc -- colored sweep indicating current value
 * 4. Scale ring -- numbered tick marks (0-10) around the knob (STANDARD/LARGE only)
 * 5. Outer shadow ring -- dark circle creating depth
 * 6. Inner concentric shadow -- recessed-into-panel illusion
 * 7. Knob body -- radial gradient for 3D metallic illusion
 * 8. Pointer -- style-dependent indicator showing current position
 *
 * ## Angle Convention
 * Canvas measures angles clockwise from the 3 o'clock position.
 * The knob range spans 270 degrees:
 * - value=0 -> 150 degrees (approximately 7 o'clock)
 * - value=1 -> 420 degrees = 60 degrees (approximately 5 o'clock)
 * - Dead zone: 60 to 150 degrees (5 o'clock to 7 o'clock, through 6 o'clock)
 *
 * @param value Normalized value in [0, 1]. Values outside this range are clamped.
 * @param onValueChange Callback invoked with the new normalized value during drag.
 * @param modifier Modifier for the outer Column layout.
 * @param label Text label displayed below the knob (e.g., "GAIN", "TONE").
 *     Pass empty string to hide.
 * @param displayValue Formatted value text displayed below the label (e.g., "4.5 dB").
 *     Pass empty string to hide.
 * @param accentColor Color used for the value arc. Defaults to the GAIN category
 *     knob accent (warm orange).
 * @param size Knob diameter. Defaults to [KnobSize.STANDARD].
 * @param enabled When false, the knob is drawn at 40% opacity and ignores gestures.
 * @param steps Number of discrete steps. 0 = continuous. For enum parameters,
 *     set to the number of discrete positions (e.g., 3 for a 3-way switch).
 *     The value will snap to the nearest 1/(steps) increment.
 * @param pointerStyle Visual style for the position indicator.
 *     Defaults to [KnobPointerStyle.LINE] for backward compatibility.
 * @param showScaleRing Whether to display the numbered 0-10 scale ring around
 *     the knob. Automatically forced to false for [KnobSize.SMALL] knobs
 *     regardless of this parameter (too crowded at that size).
 */
@Composable
fun RotaryKnob(
    value: Float,
    onValueChange: (Float) -> Unit,
    modifier: Modifier = Modifier,
    label: String = "",
    displayValue: String = "",
    accentColor: Color = Color(0xFFE8724A),
    size: KnobSize = KnobSize.STANDARD,
    enabled: Boolean = true,
    steps: Int = 0,
    pointerStyle: KnobPointerStyle = KnobPointerStyle.LINE,
    showScaleRing: Boolean = false
) {
    val density = LocalDensity.current
    val haptic = LocalHapticFeedback.current
    val knobSizeDp = size.dp

    // Scale ring is only shown on STANDARD and LARGE knobs -- too crowded on SMALL.
    val effectiveShowScale = showScaleRing && size != KnobSize.SMALL

    // When the scale ring is shown, the Canvas needs extra space around the knob
    // body for tick marks and numbers. We enlarge the Canvas and inset the knob.
    val scaleRingPaddingDp = if (effectiveShowScale) DesignSystem.KnobScaleRingPadding + 12.dp else 0.dp
    val canvasSizeDp = knobSizeDp + scaleRingPaddingDp * 2

    // Pre-compute the drag sensitivity: pixels per full-range sweep.
    // Larger knobs use proportionally more drag distance for finer control.
    val dragRangePx = remember(knobSizeDp, density) {
        with(density) {
            val sizeScale = knobSizeDp / KnobSize.STANDARD.dp
            (BASE_DRAG_RANGE_DP * sizeScale).dp.toPx()
        }
    }

    // Pre-compute colors that depend on the accent color to avoid
    // allocating new Color objects on every draw call.
    val arcColor = remember(accentColor) {
        accentColor.copy(alpha = 0.4f)
    }
    val arcGlowColor = remember(accentColor) {
        accentColor.copy(alpha = 0.12f)
    }

    // Pre-compute the scale ring padding in pixels for the draw lambda.
    val scaleRingPaddingPx = with(density) { scaleRingPaddingDp.toPx() }

    // TextMeasurer for drawing scale numbers -- must be obtained in Composable scope.
    val textMeasurer = if (effectiveShowScale) rememberTextMeasurer() else null

    // Pre-compute scale number text style.
    val scaleTextStyle = remember {
        TextStyle(
            fontSize = 7.sp,
            color = DesignSystem.CreamWhite,
            fontFamily = FontFamily.Monospace
        )
    }

    // Pre-compute measured text layout results for scale numbers 0..10.
    // This avoids re-measuring on every draw frame.
    val scaleMeasuredTexts = remember(textMeasurer, scaleTextStyle) {
        textMeasurer?.let { measurer ->
            (0..10).map { i ->
                measurer.measure(i.toString(), scaleTextStyle)
            }
        }
    }

    // Pre-compute the angles (in radians) for each of the 11 scale marks.
    // Each mark i maps to normalized value i/10, so its angle is:
    //   ARC_START_ANGLE_DEG + (i / 10) * ARC_SWEEP_ANGLE_DEG
    val scaleAnglesRad = remember {
        FloatArray(SCALE_MARK_COUNT) { i ->
            (ARC_START_ANGLE_DEG + (i.toFloat() / 10f) * ARC_SWEEP_ANGLE_DEG) * DEG_TO_RAD
        }
    }

    // Use rememberUpdatedState so the gesture coroutine always reads
    // the latest value/callback without restarting (which would cancel
    // an in-progress drag).
    val currentValue by rememberUpdatedState(value)
    val currentOnValueChange by rememberUpdatedState(onValueChange)
    val currentHaptic by rememberUpdatedState(haptic)

    Column(
        modifier = modifier
            .graphicsLayer {
                // Reduce opacity when disabled; graphicsLayer avoids
                // invalidating the draw scope on enable/disable toggle.
                alpha = if (enabled) 1f else 0.4f
            },
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Canvas(
            modifier = Modifier
                .size(canvasSizeDp)
                .semantics {
                    this.contentDescription = label.ifEmpty { "Knob" }
                    this.stateDescription = displayValue.ifEmpty { "${(value * 100).roundToInt()}%" }
                }
                .then(
                    if (enabled) {
                        Modifier.pointerInput(Unit) {
                            // Manual gesture detection using awaitEachGesture so we
                            // can consume pointer events and prevent the parent
                            // verticalScroll / ModalBottomSheet from intercepting
                            // vertical drags intended for the knob.
                            val touchSlop = viewConfiguration.touchSlop
                            awaitEachGesture {
                                // Wait for first finger down
                                val down = awaitPointerEvent(PointerEventPass.Main)
                                    .changes.firstOrNull() ?: return@awaitEachGesture
                                if (!down.pressed) return@awaitEachGesture

                                // Track the original pointer to avoid multi-touch jumps
                                val pointerId = down.id

                                var dragStarted = false
                                var totalX = 0f
                                var totalY = 0f

                                // Track pointer events until release or cancel
                                while (true) {
                                    val event = awaitPointerEvent(PointerEventPass.Main)
                                    val change = event.changes.firstOrNull { it.id == pointerId }
                                        ?: break

                                    if (!change.pressed) break // Finger lifted

                                    val posChange = change.positionChange()
                                    val dx = posChange.x
                                    val dy = posChange.y
                                    totalX += dx
                                    totalY += dy

                                    if (!dragStarted && abs(totalY) > touchSlop) {
                                        // Only claim gesture if predominantly vertical
                                        if (abs(totalY) > abs(totalX)) {
                                            dragStarted = true
                                        } else {
                                            // Horizontal swipe -- let parent handle it
                                            break
                                        }
                                    }

                                    if (dragStarted) {
                                        // Consume the event so parent scroll doesn't see it
                                        change.consume()

                                        // Compute new knob value from drag delta
                                        val delta = -dy / dragRangePx
                                        val rawValue = (currentValue + delta).coerceIn(0f, 1f)

                                        val snappedValue = if (steps > 0) {
                                            val stepSize = 1f / steps
                                            (rawValue / stepSize).roundToInt() * stepSize
                                        } else {
                                            rawValue
                                        }

                                        val finalValue = snappedValue.coerceIn(0f, 1f)
                                        // Haptic tick when crossing a step boundary
                                        if (steps > 0 && finalValue != currentValue) {
                                            currentHaptic.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                                        }
                                        currentOnValueChange(finalValue)
                                    }
                                }
                            }
                        }
                    } else {
                        Modifier
                    }
                )
        ) {
            val canvasSize = this.size
            val canvasCenter = Offset(canvasSize.width / 2f, canvasSize.height / 2f)

            // When scale ring is shown, the knob body is inset from the canvas edges.
            // The knob radius is computed from the original knobSizeDp, not the
            // enlarged canvas.
            val knobRadius = (canvasSize.minDimension / 2f) - scaleRingPaddingPx
            val center = canvasCenter

            // Dimension constants in pixels
            val shadowRingExtra = 4.dp.toPx()
            val arcStrokeWidth = 2.dp.toPx()
            val arcGlowStrokeWidth = 6.dp.toPx()
            val pointerStrokeWidth = 2.dp.toPx()
            val pointerInnerOffset = 8.dp.toPx()
            val pointerOuterOffset = 4.dp.toPx()
            val innerShadowWidth = 2.dp.toPx()

            val clampedValue = value.coerceIn(0f, 1f)

            // ── Layer 1: Drop shadow beneath the knob ───────────────────
            // A slightly offset dark circle behind everything creates the
            // illusion of the knob being raised off the panel surface.
            val dropShadowOffset = knobRadius * DROP_SHADOW_OFFSET_FRACTION
            drawCircle(
                color = DesignSystem.PanelShadow.copy(alpha = 0.5f),
                radius = knobRadius - shadowRingExtra / 2f + 1.dp.toPx(),
                center = Offset(center.x, center.y + dropShadowOffset + 1.dp.toPx())
            )

            // ── Layer 2: Value arc glow (LED underglow effect) ──────────
            // A wider, more transparent version of the value arc drawn
            // behind the main arc to simulate diffuse LED underglow.
            if (clampedValue > 0f) {
                drawValueArc(
                    center = center,
                    radius = knobRadius,
                    value = clampedValue,
                    strokeWidth = arcGlowStrokeWidth,
                    color = arcGlowColor
                )
            }

            // ── Layer 3: Value arc ──────────────────────────────────────
            drawValueArc(
                center = center,
                radius = knobRadius,
                value = clampedValue,
                strokeWidth = arcStrokeWidth,
                color = arcColor
            )

            // ── Layer 4: Scale ring (tick marks + numbers) ──────────────
            if (effectiveShowScale && scaleMeasuredTexts != null) {
                drawScaleRing(
                    center = center,
                    knobRadius = knobRadius,
                    scaleRingPadding = DesignSystem.KnobScaleRingPadding.toPx(),
                    scaleAnglesRad = scaleAnglesRad,
                    measuredTexts = scaleMeasuredTexts,
                    tickColor = DesignSystem.CreamWhite.copy(alpha = 0.7f)
                )
            }

            // ── Layer 5: Outer shadow ring ──────────────────────────────
            drawCircle(
                color = DesignSystem.KnobOuterRing,
                radius = knobRadius - shadowRingExtra / 2f,
                center = center
            )

            // ── Layer 6: Inner concentric shadow ring ───────────────────
            // A dark ring drawn between the outer ring and the knob body
            // creates a "recessed into panel" beveled look.
            val innerShadowRadius = knobRadius - shadowRingExtra + innerShadowWidth / 2f
            drawCircle(
                color = DesignSystem.PanelShadow.copy(alpha = 0.6f),
                radius = innerShadowRadius,
                center = center,
                style = Stroke(width = innerShadowWidth)
            )

            // ── Layer 7: Knob body ──────────────────────────────────────
            // Use two flat circles (darker outer, lighter inner) instead of
            // a per-frame radialGradient allocation. The visual appearance
            // remains a 3D metallic illusion with the highlight shifted
            // upper-left, simulating a top-left light source.
            val bodyRadius = knobRadius - shadowRingExtra
            drawCircle(
                color = DesignSystem.KnobBody,
                radius = bodyRadius,
                center = center
            )
            // Highlight: smaller, offset upper-left
            val highlightCenter = Offset(
                x = center.x - bodyRadius * 0.15f,
                y = center.y - bodyRadius * 0.15f
            )
            drawCircle(
                color = DesignSystem.KnobHighlight,
                radius = bodyRadius * 0.65f,
                center = highlightCenter
            )

            // ── Layer 8: Pointer (style-dependent) ──────────────────────
            when (pointerStyle) {
                KnobPointerStyle.LINE -> {
                    drawPointerLine(
                        center = center,
                        innerRadius = pointerInnerOffset,
                        outerRadius = bodyRadius - pointerOuterOffset,
                        value = clampedValue,
                        strokeWidth = pointerStrokeWidth,
                        color = DesignSystem.KnobPointer
                    )
                }

                KnobPointerStyle.CHICKEN_HEAD -> {
                    drawChickenHeadPointer(
                        center = center,
                        bodyRadius = bodyRadius,
                        value = clampedValue,
                        color = DesignSystem.CreamWhite
                    )
                }

                KnobPointerStyle.DOT -> {
                    drawDotPointer(
                        center = center,
                        bodyRadius = bodyRadius,
                        value = clampedValue,
                        color = DesignSystem.KnobPointer,
                        pointerOuterOffset = pointerOuterOffset
                    )
                }
            }
        }

        // Label text below the knob
        if (label.isNotEmpty()) {
            Spacer(modifier = Modifier.height(3.dp))
            Text(
                text = label,
                fontSize = 11.sp,
                color = DesignSystem.TextSecondary,
                maxLines = 1
            )
        }

        // Value readout below the label
        if (displayValue.isNotEmpty()) {
            if (label.isEmpty()) {
                Spacer(modifier = Modifier.height(3.dp))
            }
            Text(
                text = displayValue,
                fontSize = 11.sp,
                fontFamily = FontFamily.Monospace,
                color = DesignSystem.TextValue,
                maxLines = 1
            )
        }
    }
}

// ── Private Drawing Helpers ─────────────────────────────────────────────────

/**
 * Draws the value arc indicating how much of the knob range is "filled".
 *
 * The arc starts at [ARC_START_ANGLE_DEG] (7 o'clock) and sweeps clockwise
 * by [value] * [ARC_SWEEP_ANGLE_DEG] degrees.
 *
 * @param center Center of the arc's bounding oval.
 * @param radius Radius of the arc.
 * @param value Normalized value in [0, 1] controlling sweep extent.
 * @param strokeWidth Width of the arc stroke in pixels.
 * @param color Color of the arc stroke.
 */
private fun DrawScope.drawValueArc(
    center: Offset,
    radius: Float,
    value: Float,
    strokeWidth: Float,
    color: Color
) {
    if (value <= 0f) return

    val sweepAngle = value * ARC_SWEEP_ANGLE_DEG

    // drawArc expects a bounding rectangle (top-left + size)
    val arcRadius = radius - strokeWidth / 2f
    val topLeft = Offset(center.x - arcRadius, center.y - arcRadius)
    val arcSize = Size(arcRadius * 2f, arcRadius * 2f)

    drawArc(
        color = color,
        startAngle = ARC_START_ANGLE_DEG,
        sweepAngle = sweepAngle,
        useCenter = false,
        topLeft = topLeft,
        size = arcSize,
        style = Stroke(width = strokeWidth, cap = StrokeCap.Round)
    )
}

/**
 * Draws the pointer line from near the knob center to the edge of the knob body.
 *
 * The angle is computed from the normalized [value]:
 * - value=0 -> [ARC_START_ANGLE_DEG] (150 degrees, approximately 7 o'clock)
 * - value=1 -> [ARC_START_ANGLE_DEG] + [ARC_SWEEP_ANGLE_DEG] (420 degrees = 60 degrees,
 *   approximately 5 o'clock)
 *
 * In Canvas coordinates (clockwise from 3 o'clock), sin/cos naturally produce
 * screen-space coordinates because the Y axis points downward.
 *
 * @param center Center of the knob.
 * @param innerRadius Distance from center to the start of the pointer line (pixels).
 * @param outerRadius Distance from center to the end of the pointer line (pixels).
 * @param value Normalized value in [0, 1].
 * @param strokeWidth Width of the pointer line in pixels.
 * @param color Color of the pointer line.
 */
private fun DrawScope.drawPointerLine(
    center: Offset,
    innerRadius: Float,
    outerRadius: Float,
    value: Float,
    strokeWidth: Float,
    color: Color
) {
    val angleDegrees = ARC_START_ANGLE_DEG + value * ARC_SWEEP_ANGLE_DEG
    val angleRadians = angleDegrees * DEG_TO_RAD

    val cosAngle = cos(angleRadians)
    val sinAngle = sin(angleRadians)

    val start = Offset(
        x = center.x + innerRadius * cosAngle,
        y = center.y + innerRadius * sinAngle
    )
    val end = Offset(
        x = center.x + outerRadius * cosAngle,
        y = center.y + outerRadius * sinAngle
    )

    drawLine(
        color = color,
        start = start,
        end = end,
        strokeWidth = strokeWidth,
        cap = StrokeCap.Round
    )
}

/**
 * Draws a chicken-head triangular pointer inspired by vintage Fender amp knobs.
 *
 * The pointer is a filled isosceles triangle with its pointed tip directed
 * toward the knob edge and a wider base near the knob center. The triangle
 * base width is proportional to the body radius for consistent proportions
 * across knob sizes.
 *
 * @param center Center of the knob.
 * @param bodyRadius Radius of the knob body circle (pixels).
 * @param value Normalized value in [0, 1].
 * @param color Fill color for the pointer triangle.
 */
private fun DrawScope.drawChickenHeadPointer(
    center: Offset,
    bodyRadius: Float,
    value: Float,
    color: Color
) {
    val angleDegrees = ARC_START_ANGLE_DEG + value * ARC_SWEEP_ANGLE_DEG
    val angleRadians = angleDegrees * DEG_TO_RAD

    val cosAngle = cos(angleRadians)
    val sinAngle = sin(angleRadians)

    // Perpendicular direction for the triangle base width
    val perpCos = -sinAngle
    val perpSin = cosAngle

    // Triangle geometry: tip near edge, base near center.
    // The tip extends to ~85% of body radius, base starts at ~25% of body radius.
    val tipRadius = bodyRadius * 0.85f
    val baseRadius = bodyRadius * 0.25f
    val halfBaseWidth = bodyRadius * 0.14f

    val tipX = center.x + tipRadius * cosAngle
    val tipY = center.y + tipRadius * sinAngle
    val baseCenterX = center.x + baseRadius * cosAngle
    val baseCenterY = center.y + baseRadius * sinAngle
    val baseLeftX = baseCenterX + halfBaseWidth * perpCos
    val baseLeftY = baseCenterY + halfBaseWidth * perpSin
    val baseRightX = baseCenterX - halfBaseWidth * perpCos
    val baseRightY = baseCenterY - halfBaseWidth * perpSin

    // Reuse a thread-local-like cached Path to avoid per-frame allocation.
    // DrawScope is single-threaded on the UI thread so this is safe.
    chickenHeadPathCache.reset()
    chickenHeadPathCache.moveTo(tipX, tipY)
    chickenHeadPathCache.lineTo(baseLeftX, baseLeftY)
    chickenHeadPathCache.lineTo(baseRightX, baseRightY)
    chickenHeadPathCache.close()

    drawPath(path = chickenHeadPathCache, color = color)
}

/**
 * Draws a circular dot near the knob edge as a position indicator.
 *
 * The dot is positioned at ~75% of the body radius along the angle
 * corresponding to the current value.
 *
 * @param center Center of the knob.
 * @param bodyRadius Radius of the knob body circle (pixels).
 * @param value Normalized value in [0, 1].
 * @param color Fill color for the dot.
 * @param pointerOuterOffset Inset from body edge used for positioning.
 */
private fun DrawScope.drawDotPointer(
    center: Offset,
    bodyRadius: Float,
    value: Float,
    color: Color,
    pointerOuterOffset: Float
) {
    val angleDegrees = ARC_START_ANGLE_DEG + value * ARC_SWEEP_ANGLE_DEG
    val angleRadians = angleDegrees * DEG_TO_RAD

    val cosAngle = cos(angleRadians)
    val sinAngle = sin(angleRadians)

    // Position the dot at 75% of the way from center to the body edge
    val dotRadius = bodyRadius * 0.075f
    val dotDistance = bodyRadius - pointerOuterOffset - dotRadius * 2f

    val dotCenter = Offset(
        x = center.x + dotDistance * cosAngle,
        y = center.y + dotDistance * sinAngle
    )

    drawCircle(
        color = color,
        radius = dotRadius,
        center = dotCenter
    )
}

/**
 * Draws the numbered scale ring around the knob with tick marks and numbers
 * at each integer position from 0 to 10.
 *
 * Tick marks at 0, 5, and 10 are drawn longer (major ticks) while the
 * remaining positions get shorter minor ticks. Numbers are positioned
 * radially outward from the tick mark endpoints.
 *
 * @param center Center of the knob.
 * @param knobRadius Outer radius of the knob (before body inset).
 * @param scaleRingPadding Gap between knob body and the start of tick marks (pixels).
 * @param scaleAnglesRad Pre-computed angles (in radians) for each of the 11 marks.
 * @param measuredTexts Pre-measured TextLayoutResult for each number "0".."10".
 * @param tickColor Color for the tick marks.
 */
private fun DrawScope.drawScaleRing(
    center: Offset,
    knobRadius: Float,
    scaleRingPadding: Float,
    scaleAnglesRad: FloatArray,
    measuredTexts: List<androidx.compose.ui.text.TextLayoutResult>,
    tickColor: Color
) {
    val tickStartRadius = knobRadius + scaleRingPadding
    val majorTickLength = 4.dp.toPx()
    val minorTickLength = 2.5.dp.toPx()
    val tickStrokeWidth = 1.dp.toPx()
    val numberPadding = 1.5.dp.toPx()

    for (i in 0 until SCALE_MARK_COUNT) {
        val angleRad = scaleAnglesRad[i]
        val cosA = cos(angleRad)
        val sinA = sin(angleRad)

        // Major ticks at 0, 5, 10 -- minor ticks elsewhere
        val isMajor = (i == 0 || i == 5 || i == 10)
        val tickLength = if (isMajor) majorTickLength else minorTickLength

        val tickStart = Offset(
            x = center.x + tickStartRadius * cosA,
            y = center.y + tickStartRadius * sinA
        )
        val tickEnd = Offset(
            x = center.x + (tickStartRadius + tickLength) * cosA,
            y = center.y + (tickStartRadius + tickLength) * sinA
        )

        drawLine(
            color = tickColor,
            start = tickStart,
            end = tickEnd,
            strokeWidth = tickStrokeWidth,
            cap = StrokeCap.Round
        )

        // Draw number text radially outside the tick mark.
        // Only draw numbers at every other position (0, 2, 4, 6, 8, 10)
        // to avoid crowding, but always draw 0, 5, and 10.
        if (i % 2 == 0 || i == 5) {
            val measured = measuredTexts[i]
            val textWidth = measured.size.width.toFloat()
            val textHeight = measured.size.height.toFloat()

            // Position the text center at the end of the tick + padding
            val textCenterRadius = tickStartRadius + tickLength + numberPadding + textHeight / 2f
            val textCenterX = center.x + textCenterRadius * cosA
            val textCenterY = center.y + textCenterRadius * sinA

            drawText(
                textLayoutResult = measured,
                topLeft = Offset(
                    x = textCenterX - textWidth / 2f,
                    y = textCenterY - textHeight / 2f
                )
            )
        }
    }
}
