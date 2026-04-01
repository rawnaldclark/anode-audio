# Jetpack Compose Techniques for Skeuomorphic Audio UI

## Research Deliverable for Guitar Emulator App
**Date**: March 2026
**Context**: 26-effect signal chain, Oboe audio engine, Jetpack Compose + Material 3, dark theme

---

## Table of Contents
1. [Rotary Knob Controls - Enhanced](#1-rotary-knob-controls---enhanced)
2. [LED Indicator Lights - Enhanced](#2-led-indicator-lights---enhanced)
3. [Metallic / Textured Surfaces Without Bitmaps](#3-metallic--textured-surfaces-without-bitmaps)
4. [VU Meter / LED Bar Graph Meter](#4-vu-meter--led-bar-graph-meter)
5. [Toggle Switch / Footswitch - Enhanced](#5-toggle-switch--footswitch---enhanced)
6. [Performance Considerations](#6-performance-considerations)
7. [Cable / Wire Signal Flow Visualization](#7-cable--wire-signal-flow-visualization)

---

## 1. Rotary Knob Controls - Enhanced

### Current State Analysis

The existing `RotaryKnob.kt` (394 lines) is already well-implemented with:
- Canvas-drawn metallic appearance (radial gradient, shadow ring, pointer line)
- Vertical drag gesture with `awaitEachGesture` (correct pattern, avoids scroll conflicts)
- `rememberUpdatedState` for gesture-safe state reads (avoids recomposition-caused drag cancellation)
- Three sizes (44dp, 56dp, 68dp), discrete step snapping, disabled state
- Value arc, label text, monospace value readout

### What Is Missing / Can Be Improved

**A. Multiple Knob Styles**

The current knob is a single "skirted" style. Professional audio software offers visual variety per-module to break monotony. The following four styles can all share the same gesture handler and arc rendering; only the body drawing changes.

```kotlin
/**
 * Knob visual style. All styles share identical gesture handling
 * and value arc rendering -- only the body drawing differs.
 */
enum class KnobStyle {
    /** Current default: flat metallic disc with radial gradient and pointer line. */
    SKIRTED,

    /** Chicken-head pointer: a wider triangular pointer with no line,
     *  inspired by Fender amp knobs. */
    CHICKEN_HEAD,

    /** Barrel knob: a smaller, darker cylinder shape with a single dot
     *  indicator, commonly seen on hi-fi equipment and some boutique pedals. */
    BARREL,

    /** Minimalist dot: thin ring with a single accent-colored dot indicating
     *  position. Cleanest look, used in modern plugin UIs. */
    DOT
}
```

**Implementation pattern for CHICKEN_HEAD body drawing:**

```kotlin
private fun DrawScope.drawChickenHeadBody(
    center: Offset,
    bodyRadius: Float,
    value: Float,
    pointerColor: Color
) {
    // 1. Dark base circle (the knob body)
    drawCircle(
        brush = Brush.radialGradient(
            colors = listOf(
                Color(0xFF484850), // Lighter center
                Color(0xFF2E2E34)  // Darker edge
            ),
            center = Offset(center.x - bodyRadius * 0.15f, center.y - bodyRadius * 0.15f),
            radius = bodyRadius
        ),
        radius = bodyRadius,
        center = center
    )

    // 2. The "chicken head" pointer -- a filled triangle
    val angleDeg = ARC_START_ANGLE_DEG + value * ARC_SWEEP_ANGLE_DEG
    val angleRad = angleDeg * DEG_TO_RAD
    val cosA = cos(angleRad)
    val sinA = sin(angleRad)

    // Perpendicular direction for the triangle base
    val perpCos = -sinA
    val perpSin = cosA

    val tipDistance = bodyRadius * 0.85f
    val baseDistance = bodyRadius * 0.25f
    val baseHalfWidth = bodyRadius * 0.22f

    val tip = Offset(
        center.x + tipDistance * cosA,
        center.y + tipDistance * sinA
    )
    val baseLeft = Offset(
        center.x + baseDistance * cosA + baseHalfWidth * perpCos,
        center.y + baseDistance * sinA + baseHalfWidth * perpSin
    )
    val baseRight = Offset(
        center.x + baseDistance * cosA - baseHalfWidth * perpCos,
        center.y + baseDistance * sinA - baseHalfWidth * perpSin
    )

    val path = androidx.compose.ui.graphics.Path().apply {
        moveTo(tip.x, tip.y)
        lineTo(baseLeft.x, baseLeft.y)
        lineTo(baseRight.x, baseRight.y)
        close()
    }

    drawPath(path = path, color = pointerColor)
}
```

**Implementation pattern for BARREL body drawing:**

```kotlin
private fun DrawScope.drawBarrelBody(
    center: Offset,
    bodyRadius: Float,
    value: Float,
    accentColor: Color
) {
    // 1. Outer ring (subtle border)
    drawCircle(
        color = Color(0xFF1E1E24),
        radius = bodyRadius,
        center = center
    )

    // 2. Inner cylinder face -- slightly smaller, two-tone gradient
    //    to suggest a curved cylindrical surface
    val innerRadius = bodyRadius * 0.82f
    drawCircle(
        brush = Brush.linearGradient(
            colors = listOf(
                Color(0xFF404048), // Top: lighter (light source above)
                Color(0xFF28282E)  // Bottom: darker
            ),
            start = Offset(center.x, center.y - innerRadius),
            end = Offset(center.x, center.y + innerRadius)
        ),
        radius = innerRadius,
        center = center
    )

    // 3. Single dot indicator at the value position
    val angleDeg = ARC_START_ANGLE_DEG + value * ARC_SWEEP_ANGLE_DEG
    val angleRad = angleDeg * DEG_TO_RAD
    val dotDistance = innerRadius * 0.7f
    val dotCenter = Offset(
        center.x + dotDistance * cos(angleRad),
        center.y + dotDistance * sin(angleRad)
    )
    drawCircle(
        color = accentColor,
        radius = bodyRadius * 0.08f,
        center = dotCenter
    )
}
```

**Implementation pattern for DOT body drawing:**

```kotlin
private fun DrawScope.drawDotBody(
    center: Offset,
    bodyRadius: Float,
    value: Float,
    accentColor: Color
) {
    // 1. Thin outer ring -- just a stroked circle
    drawCircle(
        color = Color(0xFF3A3A42),
        radius = bodyRadius,
        center = center,
        style = Stroke(width = 1.5.dp.toPx())
    )

    // 2. Very subtle filled inner circle
    drawCircle(
        color = Color(0xFF222228),
        radius = bodyRadius - 2.dp.toPx(),
        center = center
    )

    // 3. Accent-colored dot at the current value position
    val angleDeg = ARC_START_ANGLE_DEG + value * ARC_SWEEP_ANGLE_DEG
    val angleRad = angleDeg * DEG_TO_RAD
    val dotDistance = bodyRadius * 0.72f
    val dotCenter = Offset(
        center.x + dotDistance * cos(angleRad),
        center.y + dotDistance * sin(angleRad)
    )

    // Glow behind the dot
    drawCircle(
        color = accentColor.copy(alpha = 0.3f),
        radius = bodyRadius * 0.14f,
        center = dotCenter
    )

    // Dot itself
    drawCircle(
        color = accentColor,
        radius = bodyRadius * 0.07f,
        center = dotCenter
    )
}
```

**B. Tick Marks Around the Knob**

For parameters where musicians need visual reference (e.g., "set the drive to 3 o'clock"), add optional tick marks:

```kotlin
private fun DrawScope.drawTickMarks(
    center: Offset,
    outerRadius: Float,
    innerRadius: Float,
    tickCount: Int,
    color: Color
) {
    val tickStroke = 1.dp.toPx()
    for (i in 0 until tickCount) {
        val t = i.toFloat() / (tickCount - 1).coerceAtLeast(1)
        val angleDeg = ARC_START_ANGLE_DEG + t * ARC_SWEEP_ANGLE_DEG
        val angleRad = angleDeg * DEG_TO_RAD
        val cosA = cos(angleRad)
        val sinA = sin(angleRad)

        // Major ticks at 0, 0.25, 0.5, 0.75, 1.0 are longer
        val isMajor = (i % ((tickCount - 1) / 4).coerceAtLeast(1)) == 0
        val tickInner = if (isMajor) innerRadius * 0.88f else innerRadius * 0.92f

        drawLine(
            color = color.copy(alpha = if (isMajor) 0.6f else 0.3f),
            start = Offset(center.x + tickInner * cosA, center.y + tickInner * sinA),
            end = Offset(center.x + outerRadius * cosA, center.y + outerRadius * sinA),
            strokeWidth = tickStroke,
            cap = StrokeCap.Round
        )
    }
}
```

**C. Optional Circular Drag Gesture**

Some users prefer circular drag (especially on tablets). This can be offered as an alternative input mode alongside vertical drag:

```kotlin
/**
 * Compute normalized value from a touch position relative to the knob center.
 * Maps the angle from the center to the touch point onto the 0..1 range.
 * Returns null if the touch is in the dead zone (6 o'clock region).
 */
private fun angleToValue(touchOffset: Offset, center: Offset): Float? {
    val dx = touchOffset.x - center.x
    val dy = touchOffset.y - center.y
    // atan2 returns radians from -PI to PI, measured counter-clockwise from positive X
    // Canvas uses clockwise from positive X, so negate Y:
    val angleRad = kotlin.math.atan2(dy, dx) // Already clockwise in Canvas coords
    var angleDeg = angleRad * 180f / Math.PI.toFloat()
    if (angleDeg < 0f) angleDeg += 360f

    // Map from canvas angle to value:
    // ARC_START_ANGLE_DEG = 150, ARC_SWEEP_ANGLE_DEG = 270
    // Dead zone: 60 deg to 150 deg (5 o'clock to 7 o'clock through 6 o'clock)
    val relativeAngle = angleDeg - ARC_START_ANGLE_DEG
    val normalized = if (relativeAngle < 0) relativeAngle + 360f else relativeAngle

    return if (normalized <= ARC_SWEEP_ANGLE_DEG) {
        (normalized / ARC_SWEEP_ANGLE_DEG).coerceIn(0f, 1f)
    } else {
        null // In dead zone
    }
}
```

**Performance note**: The current vertical drag approach is correct. The `awaitEachGesture` + `PointerEventPass.Main` pattern ensures zero slop for subsequent pointer events after the initial touchSlop threshold is met, giving responsive tracking. Do NOT switch to `detectDragGestures` which adds unnecessary overhead from the slop calculation on every gesture start.

### Performance Assessment for Rotary Knobs

- **drawCircle with radialGradient**: Very cheap on GPU. The shader is compiled once and cached per gradient configuration. Safe for 6+ knobs visible simultaneously at 60fps.
- **drawArc**: Single draw call, hardware accelerated. No concern.
- **drawPath (for chicken-head)**: Path objects are lightweight. A 3-vertex triangle path has negligible cost. However, allocate the Path object outside the draw scope via `remember { Path() }` and call `.reset()` + `.moveTo/.lineTo/.close` each frame to avoid GC pressure.
- **sin/cos per frame**: Two trig calls per knob per frame is trivial (nanosecond-scale). Do NOT precompute lookup tables -- the added complexity is not justified.

---

## 2. LED Indicator Lights - Enhanced

### Current State Analysis

The existing `LedIndicator.kt` (99 lines) is well-built:
- Canvas-drawn with 3-layer compositing (glow, radial gradient body, hot spot)
- Spring animation with overshoot on activation
- Category-colored active/inactive states
- Glow radius = 2x LED body radius

### What Can Be Added

**A. Pulsing Animation for Recording/Active States**

The looper recording state, metronome beat, and tap-tempo confirmation all benefit from a rhythmic pulse:

```kotlin
/**
 * LED indicator with optional pulse animation.
 *
 * When [pulse] is true, the LED glow oscillates between 0.15 and 0.35 alpha
 * in a smooth sine wave at the specified rate. This is used for:
 * - Looper recording indicator (steady pulse, 1.5s period)
 * - Metronome beat flash (fast pulse triggered externally via [pulse] toggle)
 *
 * @param active    Whether the LED is lit at all.
 * @param pulse     Whether the glow should animate with a pulsing effect.
 * @param pulseRate Period of one full pulse cycle in milliseconds.
 */
@Composable
fun PulsingLedIndicator(
    active: Boolean,
    activeColor: Color,
    inactiveColor: Color,
    modifier: Modifier = Modifier,
    size: Dp = DesignSystem.LedSize,
    pulse: Boolean = false,
    pulseRate: Int = 1500
) {
    // Continuous infinite animation for the pulse
    val infiniteTransition = rememberInfiniteTransition(label = "ledPulse")
    val pulseAlpha by infiniteTransition.animateFloat(
        initialValue = 0.15f,
        targetValue = 0.35f,
        animationSpec = infiniteRepeatable(
            animation = tween(
                durationMillis = pulseRate / 2,
                easing = FastOutSlowInEasing
            ),
            repeatMode = RepeatMode.Reverse
        ),
        label = "pulseAlpha"
    )

    // Static glow for non-pulsing active state
    val staticGlowAlpha by animateFloatAsState(
        targetValue = if (active) 0.25f else 0f,
        animationSpec = spring(dampingRatio = 0.6f, stiffness = Spring.StiffnessMedium),
        label = "staticGlow"
    )

    val effectiveGlowAlpha = when {
        active && pulse -> pulseAlpha
        active -> staticGlowAlpha
        else -> 0f
    }

    Canvas(modifier = modifier.size(size * 2)) {
        val center = Offset(this.size.width / 2f, this.size.height / 2f)
        val ledRadius = size.toPx() / 2f

        if (active) {
            // Glow circle
            drawCircle(
                color = activeColor.copy(alpha = effectiveGlowAlpha),
                radius = ledRadius * 2f,
                center = center
            )

            // LED body with radial gradient
            drawCircle(
                brush = Brush.radialGradient(
                    colors = listOf(activeColor, activeColor.copy(alpha = 0.8f)),
                    center = center,
                    radius = ledRadius
                ),
                radius = ledRadius,
                center = center
            )

            // Hot spot
            drawCircle(
                color = Color.White.copy(alpha = 0.4f),
                radius = ledRadius * 0.3f,
                center = center.copy(y = center.y - ledRadius * 0.15f)
            )
        } else {
            drawCircle(
                color = inactiveColor,
                radius = ledRadius,
                center = center
            )
        }
    }
}
```

**Performance note**: `rememberInfiniteTransition` runs on the Compose animation clock and only triggers recomposition of the containing scope. Since the LED is drawn via Canvas, this translates to a single drawCircle alpha change per frame -- negligible cost. The animation is clock-driven, not time-polled, so it does not spin a thread.

**B. Rectangular LED Variant for Status Indicators**

The CLIP indicator in the level meter uses a Box + Text. A Canvas-drawn rectangular LED would be more consistent:

```kotlin
@Composable
fun RectangularLedIndicator(
    active: Boolean,
    activeColor: Color,
    label: String,
    modifier: Modifier = Modifier,
    width: Dp = 28.dp,
    height: Dp = 10.dp
) {
    Canvas(modifier = modifier.size(width, height)) {
        val cornerRadius = CornerRadius(3.dp.toPx(), 3.dp.toPx())

        if (active) {
            // Glow: expanded rounded rect behind
            drawRoundRect(
                color = activeColor.copy(alpha = 0.2f),
                topLeft = Offset(-2.dp.toPx(), -2.dp.toPx()),
                size = Size(size.width + 4.dp.toPx(), size.height + 4.dp.toPx()),
                cornerRadius = cornerRadius
            )
        }

        // Body
        drawRoundRect(
            color = if (active) activeColor else activeColor.copy(alpha = 0.15f),
            topLeft = Offset.Zero,
            size = Size(size.width, size.height),
            cornerRadius = cornerRadius
        )

        // Text would be rendered via drawText with TextMeasurer
        // (see SpectrumAnalyzer FrequencyLabels for existing pattern)
    }
}
```

---

## 3. Metallic / Textured Surfaces Without Bitmaps

### The Challenge

Real audio hardware has rich surface textures: brushed aluminum, powder-coated metal, hammertone paint, vinyl-wrapped cabinets. Compose's Canvas API does not natively support procedural noise or Perlin texture generation, but we can achieve convincing results through gradient layering.

### Technique A: Brushed Aluminum via Overlapping Linear Gradients

The key insight: brushed metal is characterized by many fine parallel highlights running in one direction. We approximate this by stacking multiple semi-transparent linear gradients with varying positions.

```kotlin
/**
 * Modifier that draws a brushed aluminum texture behind the content.
 *
 * Uses 3 overlapping linear gradients at slightly different angles and
 * offsets to create the appearance of fine directional grain, similar
 * to the face plates on rack-mounted audio equipment.
 *
 * Performance: 3 drawRect calls with linear gradients. GPU-composited
 * in a single pass. No bitmap allocation.
 */
fun Modifier.brushedAluminumSurface(
    baseColor: Color = Color(0xFF2A2A30),
    highlightColor: Color = Color(0xFF404048),
    direction: BrushDirection = BrushDirection.HORIZONTAL
): Modifier = this.drawBehind {
    val w = size.width
    val h = size.height

    // Base fill
    drawRect(color = baseColor)

    // Gradient layer 1: broad highlight band (35% width, 10% alpha)
    val start1: Offset
    val end1: Offset
    if (direction == BrushDirection.HORIZONTAL) {
        start1 = Offset(w * 0.1f, 0f)
        end1 = Offset(w * 0.45f, 0f)
    } else {
        start1 = Offset(0f, h * 0.1f)
        end1 = Offset(0f, h * 0.45f)
    }
    drawRect(
        brush = Brush.linearGradient(
            colors = listOf(
                Color.Transparent,
                highlightColor.copy(alpha = 0.10f),
                Color.Transparent
            ),
            start = start1,
            end = end1
        )
    )

    // Gradient layer 2: narrow highlight (15% width, 6% alpha, offset)
    val start2: Offset
    val end2: Offset
    if (direction == BrushDirection.HORIZONTAL) {
        start2 = Offset(w * 0.55f, 0f)
        end2 = Offset(w * 0.72f, 0f)
    } else {
        start2 = Offset(0f, h * 0.55f)
        end2 = Offset(0f, h * 0.72f)
    }
    drawRect(
        brush = Brush.linearGradient(
            colors = listOf(
                Color.Transparent,
                highlightColor.copy(alpha = 0.06f),
                Color.Transparent
            ),
            start = start2,
            end = end2
        )
    )

    // Gradient layer 3: subtle edge darkening (vignette)
    drawRect(
        brush = Brush.linearGradient(
            colors = listOf(
                Color.Black.copy(alpha = 0.08f),
                Color.Transparent,
                Color.Transparent,
                Color.Black.copy(alpha = 0.08f)
            ),
            start = if (direction == BrushDirection.HORIZONTAL) Offset(0f, 0f) else Offset(0f, 0f),
            end = if (direction == BrushDirection.HORIZONTAL) Offset(0f, h) else Offset(w, 0f)
        )
    )
}

enum class BrushDirection { HORIZONTAL, VERTICAL }
```

### Technique B: Beveled Edge / Inset Panel

The rack module surface can gain depth with a bevel effect -- lighter top/left edges, darker bottom/right edges:

```kotlin
/**
 * Draws beveled edges around the content area to create an inset
 * or raised panel appearance. Used for rack module panels.
 *
 * @param inset If true, top/left are dark (shadow), bottom/right are light (highlight).
 *              If false (raised), top/left are light, bottom/right are dark.
 * @param bevelWidth Width of the bevel in dp.
 */
fun Modifier.beveledEdge(
    inset: Boolean = true,
    bevelWidth: Dp = 1.dp,
    lightColor: Color = Color(0xFF404048),
    shadowColor: Color = Color(0xFF161618)
): Modifier = this.drawBehind {
    val bevelPx = bevelWidth.toPx()
    val topLeftColor = if (inset) shadowColor else lightColor
    val bottomRightColor = if (inset) lightColor else shadowColor

    // Top edge
    drawLine(
        color = topLeftColor,
        start = Offset(0f, 0f),
        end = Offset(size.width, 0f),
        strokeWidth = bevelPx
    )
    // Left edge
    drawLine(
        color = topLeftColor,
        start = Offset(0f, 0f),
        end = Offset(0f, size.height),
        strokeWidth = bevelPx
    )
    // Bottom edge
    drawLine(
        color = bottomRightColor,
        start = Offset(0f, size.height),
        end = Offset(size.width, size.height),
        strokeWidth = bevelPx
    )
    // Right edge
    drawLine(
        color = bottomRightColor,
        start = Offset(size.width, 0f),
        end = Offset(size.width, size.height),
        strokeWidth = bevelPx
    )
}
```

### Technique C: Dark Matte Pedal Enclosure

The PedalCard already uses category-tinted surfaces. For a more tactile powder-coated feel, use a very subtle vertical gradient (slightly lighter at top, simulating overhead lighting) plus a micro-noise simulation via dithered drawPoints:

```kotlin
/**
 * Modifier that draws a matte enclosure surface with subtle overhead
 * lighting gradient. The "hammertone" texture is suggested through a
 * very fine noise pattern drawn as scattered micro-dots.
 *
 * PERFORMANCE NOTE: The noise dots are drawn once using drawWithCache
 * and the resulting drawing list is replayed on subsequent frames.
 * This means the random positions are computed only once per size change.
 */
fun Modifier.matteEnclosureSurface(
    baseColor: Color = DesignSystem.ModuleSurface,
    noiseIntensity: Float = 0.03f
): Modifier = this.drawWithCache {
    // Pre-compute "noise" dot positions -- deterministic based on size
    // Use a seeded pseudo-random to ensure consistency across recompositions
    val w = size.width.toInt()
    val h = size.height.toInt()
    val dotCount = (w * h / 200).coerceAtMost(500) // ~0.5% pixel coverage, max 500
    val random = java.util.Random(42) // Fixed seed for deterministic pattern
    val dots = Array(dotCount) {
        Offset(
            random.nextFloat() * size.width,
            random.nextFloat() * size.height
        )
    }

    onDrawBehind {
        // Base color
        drawRect(color = baseColor)

        // Overhead lighting: very subtle top-to-bottom gradient
        drawRect(
            brush = Brush.linearGradient(
                colors = listOf(
                    Color.White.copy(alpha = 0.03f),
                    Color.Transparent,
                    Color.Black.copy(alpha = 0.03f)
                ),
                start = Offset(size.width / 2f, 0f),
                end = Offset(size.width / 2f, size.height)
            )
        )

        // Micro-noise dots (simulate powder coat texture)
        val noiseColor = Color.White.copy(alpha = noiseIntensity)
        drawPoints(
            points = dots.toList(),
            pointMode = PointMode.Points,
            color = noiseColor,
            strokeWidth = 1f
        )
    }
}
```

**Critical performance note**: `drawWithCache` is essential here. Without it, the random dot positions would be recomputed on every draw call. With `drawWithCache`, the `onDrawBehind` lambda is only re-executed when the cache key (size) changes, so the dots are computed once and the draw commands are replayed from a cached display list.

### Technique D: Radial Gradient for Curved Amp Panel Surface

Amp faceplate panels have a curved feel suggested by a large radial gradient centered above the panel:

```kotlin
/**
 * Background for amp-style panel sections. Creates the illusion
 * of a slightly curved metal surface lit from above.
 */
fun Modifier.ampPanelSurface(
    panelColor: Color = Color(0xFF262630)
): Modifier = this.drawBehind {
    // Base
    drawRect(color = panelColor)

    // Curved highlight: large radial gradient centered above the panel
    drawRect(
        brush = Brush.radialGradient(
            colors = listOf(
                Color.White.copy(alpha = 0.04f),
                Color.Transparent
            ),
            center = Offset(size.width / 2f, -size.height * 0.3f),
            radius = size.width * 0.8f
        )
    )
}
```

---

## 4. VU Meter / LED Bar Graph Meter

### Which Is More Appropriate?

**For a guitar effects app: LED bar graph is the right choice.** Here is why:

1. **Readability**: Segmented LEDs are legible at small sizes on mobile screens. A VU needle requires minimum ~80dp height to be readable.
2. **Latency perception**: LED bars respond instantly to signal changes (fill changes frame-by-frame). Needle VU meters have inherent ballistic behavior (300ms rise time per VU standard) that feels "laggy" to guitarists accustomed to clip LEDs.
3. **Real-world precedent**: Guitar amps, pedalboards, and rackmount gear universally use LED bar graphs, not VU meters. VU meters belong on mixing consoles and mastering chains.
4. **Implementation efficiency**: Drawing segmented rectangles is cheaper than drawing a rotated needle with proper ballistic physics.

### Enhanced Segmented LED Bar Graph

The current `LevelMeter.kt` uses `fillMaxHeight()` with a simple Box fill. This is functional but looks like a progress bar, not audio equipment. The TunerDisplay's `LedMeter` already demonstrates the segmented approach. Here is a vertical version for the metering strip:

```kotlin
/**
 * Vertical segmented LED bar meter mimicking rack-mount audio equipment.
 *
 * Features:
 * - 16 segments with 1dp gaps
 * - Green (bottom 8) -> Yellow (next 4) -> Red (top 4) color zones
 * - Peak hold indicator (bright white segment that decays over 2 seconds)
 * - Segments have subtle rounded corners for a realistic LED look
 * - Unlit segments are dark but visible (not invisible)
 *
 * @param level Signal level in [0.0, 1.0].
 * @param peakLevel Peak hold level in [0.0, 1.0] (managed externally).
 * @param modifier Modifier for sizing. Recommend at least 12dp wide, 80dp tall.
 */
@Composable
fun SegmentedVerticalMeter(
    level: Float,
    peakLevel: Float = 0f,
    modifier: Modifier = Modifier
) {
    val segmentCount = 16
    val animatedLevel by animateFloatAsState(
        targetValue = level.coerceIn(0f, 1f),
        animationSpec = tween(durationMillis = 40), // Fast response for metering
        label = "meterLevel"
    )

    Canvas(modifier = modifier) {
        val totalHeight = size.height
        val totalWidth = size.width
        val gapPx = 1.dp.toPx()
        val segHeight = (totalHeight - gapPx * (segmentCount - 1)) / segmentCount
        val cornerRadius = CornerRadius(1.5.dp.toPx(), 1.5.dp.toPx())

        for (i in 0 until segmentCount) {
            // Segment 0 = top (highest level), segment 15 = bottom (lowest level)
            val segmentFromBottom = segmentCount - 1 - i
            val segmentThreshold = segmentFromBottom.toFloat() / segmentCount
            val isLit = animatedLevel > segmentThreshold
            val isPeak = peakLevel > segmentThreshold &&
                         peakLevel <= (segmentFromBottom + 1f) / segmentCount

            // Color zone based on position
            val baseColor = when {
                segmentFromBottom >= 12 -> DesignSystem.MeterRed     // Top 4: red zone
                segmentFromBottom >= 8  -> DesignSystem.MeterYellow  // Next 4: yellow zone
                else                    -> DesignSystem.MeterGreen   // Bottom 8: green zone
            }

            val segColor = when {
                isPeak -> DesignSystem.MeterPeakHold  // Peak hold: bright white
                isLit  -> baseColor                    // Active: full color
                else   -> baseColor.copy(alpha = 0.08f) // Unlit: very dim
            }

            val y = i * (segHeight + gapPx)
            drawRoundRect(
                color = segColor,
                topLeft = Offset(0f, y),
                size = Size(totalWidth, segHeight),
                cornerRadius = cornerRadius
            )
        }
    }
}
```

**Peak Hold Management:**

```kotlin
/**
 * Manages peak hold level with decay. Call from a LaunchedEffect
 * that runs while the meter is visible.
 *
 * @param currentLevel The current signal level.
 * @param holdTimeMs How long the peak indicator stays before starting to decay.
 * @param decayRate How fast the peak decays per frame (0..1 per 16ms frame).
 * @return Pair of (peakLevel, updateFunction)
 */
@Composable
fun rememberPeakHold(
    currentLevel: Float,
    holdTimeMs: Long = 2000L,
    decayRate: Float = 0.02f
): Float {
    var peakLevel by remember { mutableFloatStateOf(0f) }
    var lastPeakTime by remember { mutableLongStateOf(0L) }

    LaunchedEffect(currentLevel) {
        val now = System.nanoTime() / 1_000_000L
        if (currentLevel > peakLevel) {
            peakLevel = currentLevel
            lastPeakTime = now
        }
    }

    // Decay the peak hold after the hold time
    LaunchedEffect(Unit) {
        while (true) {
            delay(16) // ~60fps
            val now = System.nanoTime() / 1_000_000L
            if (now - lastPeakTime > holdTimeMs && peakLevel > 0f) {
                peakLevel = (peakLevel - decayRate).coerceAtLeast(0f)
            }
        }
    }

    return peakLevel
}
```

### Analog VU Meter (for reference, if ever needed for a mastering view)

If a VU meter is ever desired (e.g., a dedicated "Studio" view or mastering preset), here is the Canvas approach:

```kotlin
/**
 * Analog-style VU meter with needle.
 *
 * NOTE: This is provided for reference only. For the guitar app's main
 * metering, use SegmentedVerticalMeter instead. This would only be
 * appropriate in a dedicated mastering/mixing view.
 */
@Composable
fun AnalogVuMeter(
    level: Float, // 0..1 where 0.7 = 0 VU
    modifier: Modifier = Modifier
) {
    // VU ballistic: 300ms to 99% of steady state
    val animatedLevel by animateFloatAsState(
        targetValue = level.coerceIn(0f, 1f),
        animationSpec = tween(durationMillis = 300, easing = FastOutSlowInEasing),
        label = "vuNeedle"
    )

    Canvas(modifier = modifier) {
        val w = size.width
        val h = size.height
        val pivotX = w / 2f
        val pivotY = h * 0.9f // Pivot point near bottom
        val needleLength = h * 0.75f

        // Background arc scale markings
        val arcStart = -50f // degrees from vertical (left)
        val arcEnd = 50f    // degrees from vertical (right)

        // Draw scale ticks
        for (db in -20..3 step 1) {
            val t = (db + 20f) / 23f // Normalize -20..+3 to 0..1
            val angleDeg = arcStart + t * (arcEnd - arcStart)
            val angleRad = (angleDeg - 90f) * DEG_TO_RAD // -90 because we measure from vertical

            val tickOuter = needleLength
            val tickInner = needleLength - (if (db % 5 == 0) 10.dp.toPx() else 5.dp.toPx())
            val cosA = cos(angleRad)
            val sinA = sin(angleRad)

            drawLine(
                color = if (db >= 0) DesignSystem.MeterRed else DesignSystem.TextSecondary,
                start = Offset(pivotX + tickInner * sinA, pivotY - tickInner * cosA),
                end = Offset(pivotX + tickOuter * sinA, pivotY - tickOuter * cosA),
                strokeWidth = if (db % 5 == 0) 2f else 1f
            )
        }

        // Needle
        val needleAngleDeg = arcStart + animatedLevel * (arcEnd - arcStart)
        val needleAngleRad = (needleAngleDeg - 90f) * DEG_TO_RAD
        val cosN = cos(needleAngleRad)
        val sinN = sin(needleAngleRad)

        drawLine(
            color = Color.White,
            start = Offset(pivotX, pivotY),
            end = Offset(
                pivotX + needleLength * sinN,
                pivotY - needleLength * cosN
            ),
            strokeWidth = 2.dp.toPx(),
            cap = StrokeCap.Round
        )

        // Pivot dot
        drawCircle(
            color = Color(0xFF3A3A42),
            radius = 4.dp.toPx(),
            center = Offset(pivotX, pivotY)
        )
    }
}
```

---

## 5. Toggle Switch / Footswitch - Enhanced

### Current State Analysis

The existing `StompSwitch.kt` (126 lines) is solid:
- Canvas-drawn metallic disc with radial gradient
- Chrome crescent highlight for specular reflection
- Press animation (scale to 0.93x via graphicsLayer)
- Active glow ring behind switch

### Enhancement A: Haptic Feedback

The stomp switch should produce haptic feedback on press, matching the "click" sensation of a real footswitch:

```kotlin
@Composable
fun StompSwitchWithHaptic(
    active: Boolean,
    categoryColor: Color,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    size: Dp = DesignSystem.StompSwitchSize
) {
    val hapticFeedback = LocalHapticFeedback.current

    StompSwitch(
        active = active,
        categoryColor = categoryColor,
        onClick = {
            hapticFeedback.performHapticFeedback(HapticFeedbackType.LongPress)
            onClick()
        },
        modifier = modifier,
        size = size
    )
}
```

**Note**: `HapticFeedbackType.LongPress` produces a medium-intensity "thud" on most Android devices. This is the closest match to a physical stomp switch. `TextHandleMove` is too light; custom vibration patterns require platform-specific code and the `Vibrator` service.

### Enhancement B: Mini Toggle Switch for Mode Selection

For binary mode switches (e.g., Phaser 2-stage vs 6-stage, Compressor OTA vs FET), a mini toggle is more appropriate than a full stomp switch:

```kotlin
/**
 * Canvas-drawn mini toggle switch for binary mode selection.
 *
 * Resembles the small metal toggle switches found on boutique guitar pedals
 * (e.g., JHS Morning Glory's gain toggle, Strymon's "favorites" switch).
 *
 * The toggle has two visual states: up (position A) and down (position B).
 * The chrome lever tilts between positions with a spring animation.
 *
 * @param isPositionB Whether the switch is in position B (down/right).
 * @param onToggle Callback when the switch is tapped.
 * @param labelA Label for position A (top/left).
 * @param labelB Label for position B (bottom/right).
 * @param modifier Layout modifier.
 */
@Composable
fun MiniToggleSwitch(
    isPositionB: Boolean,
    onToggle: () -> Unit,
    labelA: String = "",
    labelB: String = "",
    modifier: Modifier = Modifier,
    accentColor: Color = DesignSystem.TextPrimary
) {
    val leverPosition by animateFloatAsState(
        targetValue = if (isPositionB) 1f else 0f,
        animationSpec = spring(dampingRatio = 0.5f, stiffness = 800f),
        label = "toggleLever"
    )

    Column(
        modifier = modifier,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // Label A (top position)
        if (labelA.isNotEmpty()) {
            Text(
                text = labelA,
                fontSize = 9.sp,
                color = if (!isPositionB) accentColor else DesignSystem.TextMuted,
                fontWeight = if (!isPositionB) FontWeight.Bold else FontWeight.Normal,
                maxLines = 1
            )
            Spacer(modifier = Modifier.height(2.dp))
        }

        Canvas(
            modifier = Modifier
                .size(width = 20.dp, height = 32.dp)
                .clickable(
                    interactionSource = remember { MutableInteractionSource() },
                    indication = null,
                    onClick = onToggle
                )
        ) {
            val w = size.width
            val h = size.height

            // Switch slot (vertical channel the lever sits in)
            val slotWidth = w * 0.4f
            val slotLeft = (w - slotWidth) / 2f
            val cornerRadius = CornerRadius(2.dp.toPx(), 2.dp.toPx())

            drawRoundRect(
                color = Color(0xFF1A1A20),
                topLeft = Offset(slotLeft, 0f),
                size = Size(slotWidth, h),
                cornerRadius = cornerRadius
            )

            // Lever: a small chrome rectangle that moves up/down
            val leverHeight = h * 0.35f
            val leverWidth = w * 0.65f
            val leverLeft = (w - leverWidth) / 2f
            val leverTravel = h - leverHeight
            val leverTop = leverPosition * leverTravel

            // Lever shadow
            drawRoundRect(
                color = Color(0xFF0A0A0E),
                topLeft = Offset(leverLeft + 1.dp.toPx(), leverTop + 1.dp.toPx()),
                size = Size(leverWidth, leverHeight),
                cornerRadius = CornerRadius(3.dp.toPx(), 3.dp.toPx())
            )

            // Lever body: linear gradient for metallic chrome
            drawRoundRect(
                brush = Brush.linearGradient(
                    colors = listOf(
                        Color(0xFF5A5A62), // Top highlight
                        Color(0xFF3A3A42), // Middle
                        Color(0xFF4A4A52)  // Bottom reflection
                    ),
                    start = Offset(leverLeft, leverTop),
                    end = Offset(leverLeft, leverTop + leverHeight)
                ),
                topLeft = Offset(leverLeft, leverTop),
                size = Size(leverWidth, leverHeight),
                cornerRadius = CornerRadius(3.dp.toPx(), 3.dp.toPx())
            )

            // Chrome highlight line across lever center
            val lineY = leverTop + leverHeight / 2f
            drawLine(
                color = Color.White.copy(alpha = 0.15f),
                start = Offset(leverLeft + 2.dp.toPx(), lineY),
                end = Offset(leverLeft + leverWidth - 2.dp.toPx(), lineY),
                strokeWidth = 1f
            )
        }

        // Label B (bottom position)
        if (labelB.isNotEmpty()) {
            Spacer(modifier = Modifier.height(2.dp))
            Text(
                text = labelB,
                fontSize = 9.sp,
                color = if (isPositionB) accentColor else DesignSystem.TextMuted,
                fontWeight = if (isPositionB) FontWeight.Bold else FontWeight.Normal,
                maxLines = 1
            )
        }
    }
}
```

### Enhancement C: Three-Way Toggle (for 3-mode effects)

For Phaser (3 modes), Wah (3 types), extend the toggle to support three positions:

```kotlin
/**
 * Three-position toggle switch for effects with 3 modes.
 * Lever snaps to top (0), middle (1), or bottom (2).
 */
@Composable
fun ThreeWayToggle(
    position: Int, // 0, 1, or 2
    onPositionChange: (Int) -> Unit,
    labels: List<String> = emptyList(), // Up to 3 labels
    modifier: Modifier = Modifier,
    accentColor: Color = DesignSystem.TextPrimary
) {
    val leverPosition by animateFloatAsState(
        targetValue = position.coerceIn(0, 2) / 2f,
        animationSpec = spring(dampingRatio = 0.5f, stiffness = 800f),
        label = "threeWayLever"
    )

    // Tap cycles through positions: 0 -> 1 -> 2 -> 0
    Column(
        modifier = modifier,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // Drawing is identical to MiniToggleSwitch but with:
        // - 3 detent positions instead of 2
        // - onClick cycles: onPositionChange((position + 1) % 3)
        // - Labels at top, middle, and bottom

        // [Canvas implementation follows the same pattern as MiniToggleSwitch
        //  with leverTravel split into 3 positions instead of 2]

        Canvas(
            modifier = Modifier
                .size(width = 20.dp, height = 40.dp)
                .clickable(
                    interactionSource = remember { MutableInteractionSource() },
                    indication = null,
                    onClick = { onPositionChange((position + 1) % 3) }
                )
        ) {
            // Same drawing as MiniToggleSwitch with:
            // val leverTravel = h - leverHeight
            // val leverTop = leverPosition * leverTravel
            // (leverPosition is 0.0, 0.5, or 1.0 for positions 0, 1, 2)

            val w = size.width
            val h = size.height
            val slotWidth = w * 0.4f
            val slotLeft = (w - slotWidth) / 2f
            val cornerRadius = CornerRadius(2.dp.toPx(), 2.dp.toPx())

            drawRoundRect(
                color = Color(0xFF1A1A20),
                topLeft = Offset(slotLeft, 0f),
                size = Size(slotWidth, h),
                cornerRadius = cornerRadius
            )

            // Detent marks (3 small indentations in the slot)
            for (pos in 0..2) {
                val detentY = (pos / 2f) * (h - h * 0.3f) + h * 0.15f
                drawCircle(
                    color = Color(0xFF2A2A30),
                    radius = 1.5.dp.toPx(),
                    center = Offset(w / 2f, detentY)
                )
            }

            val leverHeight = h * 0.3f
            val leverWidth = w * 0.65f
            val leverLeft = (w - leverWidth) / 2f
            val leverTravel = h - leverHeight
            val leverTop = leverPosition * leverTravel

            // Lever shadow + body (same as MiniToggleSwitch)
            drawRoundRect(
                color = Color(0xFF0A0A0E),
                topLeft = Offset(leverLeft + 1.dp.toPx(), leverTop + 1.dp.toPx()),
                size = Size(leverWidth, leverHeight),
                cornerRadius = CornerRadius(3.dp.toPx(), 3.dp.toPx())
            )
            drawRoundRect(
                brush = Brush.linearGradient(
                    colors = listOf(Color(0xFF5A5A62), Color(0xFF3A3A42), Color(0xFF4A4A52)),
                    start = Offset(leverLeft, leverTop),
                    end = Offset(leverLeft, leverTop + leverHeight)
                ),
                topLeft = Offset(leverLeft, leverTop),
                size = Size(leverWidth, leverHeight),
                cornerRadius = CornerRadius(3.dp.toPx(), 3.dp.toPx())
            )
        }
    }
}
```

---

## 6. Performance Considerations

### Compose Drawing Cost Hierarchy (cheapest to most expensive)

| Operation | Relative Cost | Notes |
|-----------|:---:|-------|
| `drawRect(color)` | 1x | Single solid fill. Baseline. |
| `drawCircle(color)` | 1x | Same as rect, just different geometry. |
| `drawLine` | 1x | Trivial. |
| `drawRoundRect` | 1.2x | Adds corner rounding. Still very cheap. |
| `drawRect(Brush.linearGradient)` | 1.5x | GPU shader, compiled once, cached. |
| `drawCircle(Brush.radialGradient)` | 1.5x | Same as linear gradient. |
| `drawArc` | 2x | More geometry computation than drawCircle. |
| `drawPath` (simple, <10 vertices) | 2x | Path construction + fill. |
| `drawImage` (Bitmap) | 3x | Texture upload + sampling. Avoid if possible. |
| `drawText` (via TextMeasurer) | 5x | Text layout is expensive. Cache TextLayoutResult. |
| `Brush.sweepGradient` | 2x | Rarely needed. More shader instructions. |
| `BlendMode` operations | 3-10x | Requires offscreen buffer in some modes. |
| `drawWithContent { clipPath {} }` | 4x | Path clipping requires stencil buffer. |

### Key Rules for Audio App Compose Performance

**Rule 1: Use `drawWithCache` for anything that does not change every frame.**

Our knob tick marks, brushed aluminum texture, bevel edges, and noise dots are all static unless the component resizes. Wrapping these in `drawWithCache` means the draw commands are compiled into a display list once and replayed from cache.

```kotlin
// GOOD: Tick marks computed once
Modifier.drawWithCache {
    val ticks = computeTickMarkPaths(size) // Expensive, runs once
    onDrawBehind {
        ticks.forEach { drawPath(it, color) } // Replayed from cache
    }
}

// BAD: Tick marks recomputed every frame
Modifier.drawBehind {
    val ticks = computeTickMarkPaths(size) // Runs 60x per second!
    ticks.forEach { drawPath(it, color) }
}
```

**Rule 2: Use `graphicsLayer` for animations that only affect position, scale, rotation, or alpha.**

`graphicsLayer` creates a hardware-accelerated RenderNode. Changing `alpha`, `scaleX/Y`, `translationX/Y`, or `rotationZ` on a graphicsLayer does NOT trigger recomposition or re-drawing -- it only changes the compositing parameters of the already-rendered layer.

The existing StompSwitch correctly uses this pattern for the press scale animation. The existing RotaryKnob correctly uses it for the disabled alpha.

```kotlin
// GOOD: Scale change does not re-draw the Canvas
Canvas(
    modifier = Modifier
        .graphicsLayer { scaleX = scale; scaleY = scale }
) { /* complex drawing */ }

// BAD: Alpha change triggers full recomposition + re-draw
Canvas(
    modifier = Modifier.alpha(if (enabled) 1f else 0.4f)
) { /* complex drawing, runs again even though content didn't change */ }
```

**Rule 3: Never allocate objects inside DrawScope.**

`DrawScope` lambdas run on every frame. Object allocations trigger GC, which causes jank.

```kotlin
// BAD: Creates a new Color every frame
drawScope {
    drawCircle(color = Color(0xFFFF0000).copy(alpha = level)) // New Color object
}

// GOOD: Pre-compute colors
val glowColor = remember(accentColor) { accentColor.copy(alpha = 0.3f) }
Canvas(modifier) {
    drawCircle(color = glowColor) // No allocation
}
```

**Rule 4: Pre-compute `Path` objects with `remember`.**

```kotlin
// GOOD: Path allocated once, reset and rebuilt each frame (no GC)
val pointerPath = remember { Path() }
Canvas(modifier) {
    pointerPath.reset()
    pointerPath.moveTo(...)
    pointerPath.lineTo(...)
    pointerPath.close()
    drawPath(pointerPath, color)
}
```

**Rule 5: Avoid `BlendMode` and `clipPath` in frequently-updating components.**

Some BlendModes (e.g., `BlendMode.Screen`, `BlendMode.Multiply`) require an offscreen buffer, which means the GPU must render the component to a temporary texture, apply the blend, then composite it back. This doubles the draw cost.

For the LED glow effect, the current approach of drawing a semi-transparent circle is MUCH cheaper than using `BlendMode.Screen` on a bright circle -- and produces a visually identical result.

**Rule 6: Limit `animateFloatAsState` observers to the narrowest possible scope.**

Each active animation triggers recomposition of its containing scope on every frame. If a `LevelMeter` animation sits inside a large Column, the entire Column recomposes every 16ms. Extract animated components into isolated Composable functions.

```kotlin
// BAD: The entire RackModule recomposes when the meter animates
@Composable
fun RackModule(...) {
    val animatedLevel by animateFloatAsState(...) // Triggers this whole function
    Column {
        HeaderRow(...)
        ExpandedContent(...)
        MeterBar(level = animatedLevel) // Only this needs the animation
    }
}

// GOOD: Only MeterBar recomposes
@Composable
fun MeterBar(rawLevel: Float) {
    val animatedLevel by animateFloatAsState(rawLevel) // Scoped to MeterBar
    Canvas(modifier) { /* draw with animatedLevel */ }
}
```

### Profiling Compose Alongside Oboe Audio

**Layout Inspector** (Android Studio): Use the Live Layout Inspector to check recomposition counts. Any composable showing >60 recompositions per second is likely being over-invalidated.

**System Trace with custom markers**: Add custom trace sections around Canvas draw calls:

```kotlin
import android.os.Trace

Canvas(modifier) {
    Trace.beginSection("RotaryKnob.draw")
    // ... drawing code ...
    Trace.endSection()
}
```

Then capture a system trace in Android Studio Profiler. Look for:
- UI thread frame time > 12ms (leaves <4ms for Oboe callback on the same core)
- GC pauses coinciding with audio glitches
- Draw calls that take >2ms individually

**Thread priority**: Oboe's audio callback runs on a high-priority real-time thread (SCHED_FIFO). Compose runs on the main thread (SCHED_OTHER). These do NOT compete for CPU time on modern SoCs with 4+ cores. The concern is shared memory bandwidth and L2 cache pressure, not CPU contention.

**Worst-case scenario**: Opening the MeteringStrip expanded view with 64-bin spectrum + 2 level meters + 2 rotary knobs + 1 stomp switch means ~6 Canvas composables animating at 30-60fps simultaneously. Based on the cost hierarchy above, this is approximately:
- 64 drawRect calls (spectrum bars): ~64x base cost
- 2x 16 drawRoundRect calls (meter segments): ~38x base cost
- 2 full RotaryKnob draws (~8 draw calls each): ~24x base cost
- 1 StompSwitch draw (~5 draw calls): ~10x base cost
- Total: ~136 draw calls per frame

On a Snapdragon 7-series or better, this completes in well under 4ms. No concern for audio interference.

---

## 7. Cable / Wire Signal Flow Visualization

### Design Context

In the vertical rack layout, effects are stacked top-to-bottom. Signal flows from the top module to the bottom. Unlike horizontal pedalboard layouts (which naturally suggest patch cables between pedals), the vertical rack layout can show signal flow through:

1. **Vertical connecting lines** between rack modules (simplest)
2. **Curved bezier cables** for visual interest (more complex)
3. **Signal flow animation** (subtle pulse traveling along the cable)

### Approach: Vertical Signal Flow Lines with Pulse Animation

For the vertical rack layout, straight vertical lines between modules are the most readable. Add a subtle animated "signal pulse" that travels down the chain when signal is present.

```kotlin
/**
 * Animated signal flow connector drawn between two rack modules.
 *
 * Renders a vertical line from the bottom of one module to the top
 * of the next, with an optional animated pulse that travels downward
 * when signal is present.
 *
 * @param signalPresent Whether audio signal is flowing (controls pulse animation).
 * @param accentColor Color of the pulse. Use the source module's category color.
 * @param modifier Modifier for sizing. Should be fillMaxWidth with small height.
 */
@Composable
fun SignalFlowConnector(
    signalPresent: Boolean,
    accentColor: Color = DesignSystem.SafeGreen,
    modifier: Modifier = Modifier
) {
    val infiniteTransition = rememberInfiniteTransition(label = "signalPulse")

    // Pulse position: 0 = top, 1 = bottom. Repeats every 800ms.
    val pulsePosition by infiniteTransition.animateFloat(
        initialValue = 0f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(
            animation = tween(durationMillis = 800, easing = LinearEasing),
            repeatMode = RepeatMode.Restart
        ),
        label = "pulsePos"
    )

    val lineColor = DesignSystem.ModuleBorder
    val pulseColor = accentColor

    Canvas(
        modifier = modifier
            .fillMaxWidth()
            .height(12.dp) // Gap between rack modules
    ) {
        val centerX = size.width / 2f
        val lineWidth = 2.dp.toPx()

        // Static line
        drawLine(
            color = lineColor,
            start = Offset(centerX, 0f),
            end = Offset(centerX, size.height),
            strokeWidth = lineWidth,
            cap = StrokeCap.Round
        )

        // Animated pulse dot (only when signal is present)
        if (signalPresent) {
            val pulseY = pulsePosition * size.height
            val pulseRadius = 3.dp.toPx()

            // Glow behind pulse
            drawCircle(
                color = pulseColor.copy(alpha = 0.3f),
                radius = pulseRadius * 2.5f,
                center = Offset(centerX, pulseY)
            )

            // Pulse dot
            drawCircle(
                color = pulseColor,
                radius = pulseRadius,
                center = Offset(centerX, pulseY)
            )
        }

        // Input/output node dots at top and bottom
        drawCircle(
            color = lineColor,
            radius = 2.5.dp.toPx(),
            center = Offset(centerX, 0f)
        )
        drawCircle(
            color = lineColor,
            radius = 2.5.dp.toPx(),
            center = Offset(centerX, size.height)
        )
    }
}
```

### Alternative: Curved Bezier Cable Between Horizontal Pedals

If the app ever supports a horizontal pedalboard view (landscape mode), patch cables between pedals are more appropriate:

```kotlin
/**
 * Draws a curved patch cable between two horizontal positions.
 *
 * The cable uses a cubic Bezier curve that sags downward (like a real
 * patch cable affected by gravity), with configurable sag amount.
 *
 * @param startX X coordinate of the cable's left endpoint.
 * @param endX X coordinate of the cable's right endpoint.
 * @param sagAmount How far the cable sags below the endpoints (in dp).
 *                  Longer cables sag more for realism.
 * @param cableColor The cable's color. Use category colors for visual coding.
 */
fun DrawScope.drawPatchCable(
    startX: Float,
    endX: Float,
    yPosition: Float,
    sagAmount: Float,
    cableColor: Color,
    cableWidth: Float = 3.dp.toPx()
) {
    val midX = (startX + endX) / 2f
    val sagY = yPosition + sagAmount

    val path = Path().apply {
        moveTo(startX, yPosition)
        cubicTo(
            x1 = startX + (midX - startX) * 0.3f,
            y1 = sagY * 0.6f,
            x2 = endX - (endX - midX) * 0.3f,
            y2 = sagY * 0.6f,
            x3 = endX,
            y3 = yPosition
        )
    }

    // Cable shadow (offset by 1dp for depth)
    drawPath(
        path = path,
        color = Color.Black.copy(alpha = 0.3f),
        style = Stroke(
            width = cableWidth + 1.dp.toPx(),
            cap = StrokeCap.Round
        )
    )

    // Cable body
    drawPath(
        path = path,
        color = cableColor,
        style = Stroke(
            width = cableWidth,
            cap = StrokeCap.Round
        )
    )

    // Cable highlight (thin bright line along the top edge)
    drawPath(
        path = path,
        color = Color.White.copy(alpha = 0.15f),
        style = Stroke(
            width = 1.dp.toPx(),
            cap = StrokeCap.Round
        )
    )

    // Jack plug endpoints (small circles)
    drawCircle(
        color = Color(0xFF4A4A52),
        radius = cableWidth + 1.dp.toPx(),
        center = Offset(startX, yPosition)
    )
    drawCircle(
        color = Color(0xFF4A4A52),
        radius = cableWidth + 1.dp.toPx(),
        center = Offset(endX, yPosition)
    )
}
```

**Performance note on bezier cables**: A `cubicTo` path has negligible GPU cost -- it is just 4 control points that the GPU rasterizer interpolates. Even 20 cables on screen simultaneously would add <0.5ms to frame time. The concern with cables is visual clutter, not performance.

### Signal Flow Animation Along Bezier (for future horizontal pedalboard view)

To animate a pulse traveling along a bezier curve, compute the point at parameter `t` along the curve:

```kotlin
/**
 * Compute a point along a cubic Bezier curve at parameter t in [0, 1].
 */
private fun bezierPoint(
    p0: Offset, p1: Offset, p2: Offset, p3: Offset, t: Float
): Offset {
    val u = 1f - t
    val tt = t * t
    val uu = u * u
    val uuu = uu * u
    val ttt = tt * t

    return Offset(
        x = uuu * p0.x + 3 * uu * t * p1.x + 3 * u * tt * p2.x + ttt * p3.x,
        y = uuu * p0.y + 3 * uu * t * p1.y + 3 * u * tt * p2.y + ttt * p3.y
    )
}

// Usage in draw scope:
if (signalPresent) {
    val pulsePoint = bezierPoint(startPoint, ctrl1, ctrl2, endPoint, animatedT)
    drawCircle(
        color = accentColor,
        radius = 3.dp.toPx(),
        center = pulsePoint
    )
}
```

---

## Implementation Priority

Based on the current codebase state and maximum visual impact per development effort:

### Phase 1 (Highest Impact, Lowest Effort)
1. **Haptic feedback on StompSwitch** -- 5 lines of code, immediate tactile improvement
2. **PulsingLedIndicator for looper recording** -- enhances existing LedIndicator
3. **SegmentedVerticalMeter** -- replaces Box-based LevelMeter with segmented LEDs
4. **MiniToggleSwitch** -- replaces enum Sliders for 2-mode effects (Phaser, Compressor, Chorus)

### Phase 2 (Medium Impact, Medium Effort)
5. **Knob style variants** (CHICKEN_HEAD, BARREL, DOT) -- visual variety per-module
6. **Tick marks on rotary knobs** -- professional detail
7. **Signal flow connectors** -- visual chain continuity between rack modules
8. **Beveled edges on rack module surface** -- subtle depth enhancement

### Phase 3 (Polish)
9. **Brushed aluminum surface** for metering strip expanded area
10. **Amp panel surface gradient** for the AmpModel rack module when expanded
11. **Matte enclosure surface** with noise texture for pedal modules
12. **Three-way toggle** for 3-mode effects (Wah, Phaser)

---

## DesignSystem Tokens to Add

The following tokens should be added to `DesignSystem.kt` to support the new components:

```kotlin
// ── Toggle Switch Colors ────────────────────────────────────────────
val ToggleSlot       = Color(0xFF1A1A20)  // Toggle switch slot channel
val ToggleLever      = Color(0xFF4A4A52)  // Toggle lever face (chrome)
val ToggleLeverDark  = Color(0xFF3A3A42)  // Toggle lever shadow side
val ToggleHighlight  = Color(0xFF5A5A62)  // Toggle lever specular highlight

// ── Signal Flow Colors ──────────────────────────────────────────────
val SignalFlowLine   = Color(0xFF333338)  // Inactive flow line
val SignalFlowPulse  = Color(0xFF40B858)  // Active signal pulse color

// ── Surface Texture Colors ──────────────────────────────────────────
val BrushedHighlight = Color(0xFF404048)  // Brushed aluminum highlight band
val PanelCurve       = Color(0xFF262630)  // Amp panel base color
```

---

## Files Referenced in This Research

| File | Purpose |
|------|---------|
| `C:\Users\theno\Projects\guitaremulatorapp\app\src\main\java\com\guitaremulator\app\ui\theme\DesignSystem.kt` | Centralized design tokens (196 lines) |
| `C:\Users\theno\Projects\guitaremulatorapp\app\src\main\java\com\guitaremulator\app\ui\components\RotaryKnob.kt` | Existing rotary knob (394 lines) |
| `C:\Users\theno\Projects\guitaremulatorapp\app\src\main\java\com\guitaremulator\app\ui\components\LedIndicator.kt` | Existing LED indicator (99 lines) |
| `C:\Users\theno\Projects\guitaremulatorapp\app\src\main\java\com\guitaremulator\app\ui\components\StompSwitch.kt` | Existing stomp switch (126 lines) |
| `C:\Users\theno\Projects\guitaremulatorapp\app\src\main\java\com\guitaremulator\app\ui\components\LevelMeter.kt` | Existing level meter (145 lines) |
| `C:\Users\theno\Projects\guitaremulatorapp\app\src\main\java\com\guitaremulator\app\ui\components\RackModule.kt` | Rack module with knobs + sliders (555 lines) |
| `C:\Users\theno\Projects\guitaremulatorapp\app\src\main\java\com\guitaremulator\app\ui\components\MeteringStrip.kt` | Bottom metering strip (589 lines) |
| `C:\Users\theno\Projects\guitaremulatorapp\app\src\main\java\com\guitaremulator\app\ui\components\TunerDisplay.kt` | Tuner with LED meter (353 lines) |
| `C:\Users\theno\Projects\guitaremulatorapp\app\src\main\java\com\guitaremulator\app\ui\components\SpectrumAnalyzer.kt` | Spectrum analyzer (235 lines) |
| `C:\Users\theno\Projects\guitaremulatorapp\app\src\main\java\com\guitaremulator\app\ui\screens\PedalboardScreen.kt` | Main screen layout (459 lines) |
| `C:\Users\theno\Projects\guitaremulatorapp\app\src\main\java\com\guitaremulator\app\ui\screens\EffectEditorScreen.kt` | Bottom sheet editor (777 lines) |
