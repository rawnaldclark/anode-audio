package com.guitaremulator.app.ui.components

import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.Dp
import com.guitaremulator.app.ui.theme.DesignSystem
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin

/**
 * Canvas-drawn LED indicator with realistic jewel facet appearance.
 *
 * When `useJewelStyle` is true (default), the LED is rendered as a faceted
 * gem (octagonal shape) with highlight arcs that catch the light, mimicking
 * vintage amp jewel pilot lights. When active, the jewel glows with a light
 * bleed effect that extends onto the surrounding panel surface. When inactive,
 * the jewel appears as dark glass with a faint ambient reflection.
 *
 * When `useJewelStyle` is false, the original flat LED style is used for
 * backward compatibility.
 *
 * The on/off transition uses a spring animation with slight overshoot
 * for a satisfying "pop" when toggling effects -- matching how real
 * hardware LEDs snap on decisively. The steady state is rock-solid
 * with no pulsing, consistent with professional audio software and
 * real guitar pedal LEDs.
 *
 * @param active Whether the LED is lit.
 * @param activeColor Color when lit (from category palette).
 * @param inactiveColor Color when dim (from category palette).
 * @param modifier Layout modifier.
 * @param size Diameter of the LED body (glow extends beyond this).
 * @param useJewelStyle When true, renders as faceted gem; when false, flat circle.
 */
@Composable
fun LedIndicator(
    active: Boolean,
    activeColor: Color,
    inactiveColor: Color,
    modifier: Modifier = Modifier,
    size: Dp = DesignSystem.LedSize,
    useJewelStyle: Boolean = true
) {
    // Resolve effective size: jewel style uses JewelLedSize, flat uses provided size
    val effectiveSize = if (useJewelStyle) DesignSystem.JewelLedSize else size

    // Spring-based on/off transition: slight overshoot on activation
    // gives a satisfying "pop", then settles to rock-solid steady state.
    val glowAlpha by animateFloatAsState(
        targetValue = if (active) 0.25f else 0f,
        animationSpec = spring(
            dampingRatio = 0.6f,
            stiffness = Spring.StiffnessMedium
        ),
        label = "ledGlow"
    )

    // Pre-compute octagon vertices for the jewel shape (8 sides, rotated so
    // flat edge is at top rather than a vertex pointing up)
    val octagonAngles = remember {
        FloatArray(8) { i ->
            // Start at -PI/2 + half-step offset so a flat edge faces upward
            val angleOffset = (PI / 8.0).toFloat()  // 22.5 degrees
            ((-PI / 2.0).toFloat() + angleOffset + i * (2.0 * PI / 8.0).toFloat())
        }
    }

    // Path holders cached across draw frames; rebuilt only when size changes.
    val octagonPathHolder = remember { arrayOfNulls<Path>(1) }
    val facetPathsHolder = remember { arrayOfNulls<Array<Path>>(1) }
    val lastRadiusHolder = remember { floatArrayOf(0f) }

    // Canvas is 2x LED size to accommodate the glow (matches original layout)
    Canvas(modifier = modifier
        .semantics {
            this.contentDescription = if (active) "Active" else "Inactive"
        }
        .size(effectiveSize * 2)
    ) {
        val center = Offset(this.size.width / 2f, this.size.height / 2f)
        val ledRadius = effectiveSize.toPx() / 2f

        // Rebuild cached paths only when radius changes (effectively on size change)
        if (ledRadius != lastRadiusHolder[0]) {
            lastRadiusHolder[0] = ledRadius
            octagonPathHolder[0] = buildOctagonPath(center, ledRadius, octagonAngles)
            facetPathsHolder[0] = buildFacetArcPaths(center, ledRadius)
        }

        if (useJewelStyle) {
            drawJewelLed(
                center = center,
                ledRadius = ledRadius,
                active = active,
                activeColor = activeColor,
                inactiveColor = inactiveColor,
                glowAlpha = glowAlpha,
                octagonPath = octagonPathHolder[0]!!,
                facetPaths = facetPathsHolder[0]!!
            )
        } else {
            drawFlatLed(
                center = center,
                ledRadius = ledRadius,
                active = active,
                activeColor = activeColor,
                inactiveColor = inactiveColor,
                glowAlpha = glowAlpha
            )
        }
    }
}

// ── Jewel (Faceted Gem) LED Rendering ──────────────────────────────────────

/**
 * Draw the jewel-style faceted LED with octagonal shape, highlight arcs,
 * light bleed glow when active, and dark glass appearance when inactive.
 */
private fun DrawScope.drawJewelLed(
    center: Offset,
    ledRadius: Float,
    active: Boolean,
    activeColor: Color,
    inactiveColor: Color,
    glowAlpha: Float,
    octagonPath: Path,
    facetPaths: Array<Path>
) {
    // ── Light bleed glow (active only) ──────────────────────────────
    // Simplified to a flat-color circle that fades via glowAlpha, avoiding
    // per-frame Brush.radialGradient allocation.
    if (active && glowAlpha > 0f) {
        drawCircle(
            color = activeColor.copy(alpha = glowAlpha * 0.4f),
            radius = ledRadius * 2.0f,
            center = center
        )
    }

    // ── Octagonal jewel body ────────────────────────────────────────
    val jewelPath = octagonPath

    if (active) {
        // Active jewel: flat fill + layered circles for depth.
        // Avoids per-frame Brush.radialGradient allocation.
        drawPath(
            path = jewelPath,
            color = activeColor.copy(alpha = 0.70f)
        )
        // Brighter center fill for gem depth
        drawCircle(
            color = activeColor,
            radius = ledRadius * 0.7f,
            center = center
        )

        // Inner glow: softer ring
        drawCircle(
            color = activeColor.copy(alpha = 0.3f),
            radius = ledRadius * 0.5f,
            center = center
        )

        // Hot spot: bright center dot, slightly smaller and more intense
        drawCircle(
            color = Color.White.copy(alpha = 0.55f),
            radius = ledRadius * 0.22f,
            center = center.copy(y = center.y - ledRadius * 0.1f)
        )
    } else {
        // Inactive jewel: dark glass with flat fill + subtle highlight.
        // Avoids per-frame Brush.radialGradient allocation.
        drawPath(
            path = jewelPath,
            color = inactiveColor
        )

        // Ambient reflection: faint highlight at top for dark glass appearance
        drawCircle(
            color = Color.White.copy(alpha = 0.10f),
            radius = ledRadius * 0.3f,
            center = Offset(center.x, center.y - ledRadius * 0.3f)
        )
    }

    // ── Facet highlight arcs (visible in both states, brighter when active)
    // 2-3 thin bright arcs at top-left to simulate light catching on facets
    drawCachedFacetHighlights(ledRadius, facetPaths, if (active) 0.35f else 0.08f)

    // ── Thin dark border around the octagonal jewel bezel ───────────
    drawPath(
        path = jewelPath,
        color = Color(0xFF0A0A0E).copy(alpha = 0.6f),
        style = Stroke(width = ledRadius * 0.1f)
    )
}

/**
 * Build an octagonal [Path] centered at [center] with the given [radius].
 *
 * @param octagonAngles Pre-computed angle array (8 elements) for vertex positions.
 */
private fun buildOctagonPath(
    center: Offset,
    radius: Float,
    octagonAngles: FloatArray
): Path {
    return Path().apply {
        for (i in octagonAngles.indices) {
            val x = center.x + radius * cos(octagonAngles[i])
            val y = center.y + radius * sin(octagonAngles[i])
            if (i == 0) moveTo(x, y) else lineTo(x, y)
        }
        close()
    }
}

/**
 * Build all 3 facet highlight arc [Path] objects, cached to avoid per-frame allocation.
 *
 * @return Array of 3 Paths: [arc1, arc2, arc3].
 */
private fun buildFacetArcPaths(
    center: Offset,
    ledRadius: Float
): Array<Path> {
    fun buildArcPath(radiusFraction: Float, startAngle: Float, endAngle: Float): Path {
        val radius = ledRadius * radiusFraction
        return Path().apply {
            val steps = 6
            val angleStep = (endAngle - startAngle) / steps
            for (i in 0..steps) {
                val angle = startAngle + i * angleStep
                val x = center.x + radius * cos(angle)
                val y = center.y + radius * sin(angle)
                if (i == 0) moveTo(x, y) else lineTo(x, y)
            }
        }
    }

    return arrayOf(
        // Arc 1: top-left facet edge (longest)
        buildArcPath(0.85f, (-PI * 0.75).toFloat(), (-PI * 0.55).toFloat()),
        // Arc 2: slightly inward and shorter
        buildArcPath(0.65f, (-PI * 0.7).toFloat(), (-PI * 0.55).toFloat()),
        // Arc 3: small dot-like arc near the top
        buildArcPath(0.45f, (-PI * 0.58).toFloat(), (-PI * 0.52).toFloat())
    )
}

/**
 * Draw pre-cached facet highlight arc paths with the given alpha.
 * Avoids per-frame Path allocation.
 */
private fun DrawScope.drawCachedFacetHighlights(
    ledRadius: Float,
    facetPaths: Array<Path>,
    alpha: Float
) {
    val highlightColor = Color.White.copy(alpha = alpha)
    val strokeWidth = ledRadius * 0.06f

    drawPath(facetPaths[0], color = highlightColor, style = Stroke(width = strokeWidth))
    drawPath(facetPaths[1], color = highlightColor * 0.7f, style = Stroke(width = strokeWidth))
    drawPath(facetPaths[2], color = highlightColor * 0.5f, style = Stroke(width = strokeWidth))
}

/**
 * Multiply a Color's alpha by a scalar factor, clamping to [0, 1].
 * Convenience for layered highlight rendering.
 */
private operator fun Color.times(alphaScale: Float): Color =
    this.copy(alpha = (this.alpha * alphaScale).coerceIn(0f, 1f))

// ── Flat (Legacy) LED Rendering ────────────────────────────────────────────

/**
 * Draw the original flat-circle LED style for backward compatibility.
 * Preserves the exact behavior of the pre-jewel implementation.
 */
private fun DrawScope.drawFlatLed(
    center: Offset,
    ledRadius: Float,
    active: Boolean,
    activeColor: Color,
    inactiveColor: Color,
    glowAlpha: Float
) {
    if (active) {
        // Glow: larger circle with low alpha
        if (glowAlpha > 0f) {
            drawCircle(
                color = activeColor.copy(alpha = glowAlpha),
                radius = ledRadius * 2f,
                center = center
            )
        }

        // LED body: flat fill (avoids per-frame radialGradient allocation)
        drawCircle(
            color = activeColor,
            radius = ledRadius,
            center = center
        )

        // Hot spot: bright center dot for "lit" realism
        drawCircle(
            color = Color.White.copy(alpha = 0.4f),
            radius = ledRadius * 0.3f,
            center = center.copy(y = center.y - ledRadius * 0.15f)
        )
    } else {
        // Dim LED: flat circle, no glow
        drawCircle(
            color = inactiveColor,
            radius = ledRadius,
            center = center
        )
    }
}
