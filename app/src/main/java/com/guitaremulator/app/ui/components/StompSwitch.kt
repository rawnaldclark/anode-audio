package com.guitaremulator.app.ui.components

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.unit.Dp
import com.guitaremulator.app.ui.theme.DesignSystem

/**
 * Canvas-drawn metallic stomp switch mimicking a real guitar pedal footswitch.
 *
 * Features:
 *   - Embossed double-ring bezel (outer dark shadow + inner chrome highlight)
 *     for a machined metal appearance
 *   - Inner surface with radial gradient for 3D depth
 *   - Chrome crescent highlight at top for specular reflection
 *   - Press animation (scale to 0.90) with shadow depth reduction
 *   - Active state: surface glow beneath the switch + subtle glow ring
 *
 * @param active Whether the effect is currently enabled.
 * @param categoryColor The category's LED active color (used for active glow).
 * @param onClick Callback when the switch is pressed.
 * @param modifier Layout modifier.
 * @param size Diameter of the switch.
 */
@Composable
fun StompSwitch(
    active: Boolean,
    categoryColor: Color,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    size: Dp = DesignSystem.StompSwitchSize
) {
    val haptic = LocalHapticFeedback.current
    val interactionSource = remember { MutableInteractionSource() }
    val isPressed by interactionSource.collectIsPressedAsState()

    // Press animation: scale down to 0.90 on press for deeper "stomp" feel
    val scale by animateFloatAsState(
        targetValue = if (isPressed) 0.90f else 1.0f,
        animationSpec = spring(dampingRatio = 0.55f, stiffness = 600f),
        label = "stompScale"
    )

    // Outer shadow opacity: fades when pressed to simulate being pushed into the surface
    val shadowAlpha by animateFloatAsState(
        targetValue = if (isPressed) 0.15f else 0.6f,
        animationSpec = spring(dampingRatio = 0.65f, stiffness = 500f),
        label = "stompShadow"
    )

    // Pre-compute design system colors outside draw lambda
    val panelShadowColor = DesignSystem.PanelShadow
    val chromeHighlightColor = DesignSystem.ChromeHighlight

    // Pre-compute fixed surface gradient colors (these never change)
    val surfaceCenterColor = remember { Color(0xFF444448) }
    val surfaceEdgeColor = remember { Color(0xFF38383C) }
    val grooveColor = remember { Color(0xFF0C0C10).copy(alpha = 0.7f) }

    // Pre-compute category-dependent glow colors to avoid allocation in draw lambda
    val glowOuterColor = remember(categoryColor) { categoryColor.copy(alpha = 0.12f) }
    val glowMiddleColor = remember(categoryColor) { categoryColor.copy(alpha = 0.06f) }
    val glowFaceColor = remember(categoryColor) { categoryColor.copy(alpha = 0.08f) }

    Canvas(
        modifier = modifier
            .size(size)
            .graphicsLayer {
                scaleX = scale
                scaleY = scale
            }
            .semantics {
                this.contentDescription = if (active) "Effect enabled" else "Effect bypassed"
                this.stateDescription = if (active) "On" else "Off"
            }
            .clickable(
                interactionSource = interactionSource,
                indication = null,
                onClick = {
                    haptic.performHapticFeedback(HapticFeedbackType.LongPress)
                    onClick()
                }
            )
    ) {
        val center = Offset(this.size.width / 2f, this.size.height / 2f)
        val outerRadius = this.size.minDimension / 2f

        // Pre-compute ring geometry:
        // Outer dark ring edge, gap (~1.5dp equivalent), inner chrome ring, then surface
        val bezelGap = outerRadius * 0.06f        // ~1.5dp gap at 52dp size
        val outerRingWidth = outerRadius * 0.08f   // Outer dark ring thickness
        val innerRingRadius = outerRadius - outerRingWidth - bezelGap
        val innerRingWidth = outerRadius * 0.05f   // Inner chrome ring thickness
        val surfaceRadius = innerRingRadius - innerRingWidth

        // ── Active state: surface glow BENEATH the switch ───────────────
        // Simulates light bleeding from under the button onto the amp panel
        if (active) {
            drawCircle(
                brush = Brush.radialGradient(
                    colors = listOf(glowOuterColor, glowMiddleColor, Color.Transparent),
                    center = center,
                    radius = outerRadius * 1.5f
                ),
                radius = outerRadius * 1.5f,
                center = center
            )
        }

        // ── Outer shadow ring (drop shadow behind the bezel) ────────────
        // Fades on press to simulate switch pushed into the panel surface
        drawCircle(
            color = panelShadowColor.copy(alpha = shadowAlpha),
            radius = outerRadius,
            center = center
        )

        // ── Outer dark bezel ring (machined metal outer edge) ───────────
        drawCircle(
            color = panelShadowColor.copy(alpha = 0.9f),
            radius = outerRadius - 1f, // Inset slightly from shadow
            center = center
        )

        // ── Bezel gap: dark recessed channel between rings ──────────────
        // The gap area is already covered by the dark outer ring; we draw
        // a subtle groove highlight at the inner edge of the gap
        drawCircle(
            color = Color(0xFF0C0C10).copy(alpha = 0.7f),
            radius = innerRingRadius + bezelGap * 0.5f,
            center = center
        )

        // ── Inner chrome highlight ring (machined metal inner edge) ─────
        // Use simpler flat color rings instead of radial gradients to
        // avoid per-frame Brush allocation. At 48dp the gradient is barely
        // visible.
        drawCircle(
            color = chromeHighlightColor.copy(alpha = 0.55f),
            radius = innerRingRadius,
            center = center
        )

        // ── Inner surface with flat color for 3D depth ──────────────────
        drawCircle(
            color = surfaceCenterColor,
            radius = surfaceRadius,
            center = center
        )

        // ── Chrome crescent highlight at top (specular reflection) ──────
        drawChromeHighlight(center, surfaceRadius)

        // ── Active state: subtle glow ring on the switch face ───────────
        if (active) {
            drawCircle(
                color = glowFaceColor,
                radius = surfaceRadius * 0.7f,
                center = center
            )
        }
    }
}

/**
 * Draw a subtle chrome crescent at the top of the switch surface
 * to simulate overhead specular lighting.
 */
/** Cached chrome specular highlight color. */
private val CHROME_SPECULAR_COLOR = Color(0xFF5A5A60).copy(alpha = 0.6f)

private fun DrawScope.drawChromeHighlight(center: Offset, innerRadius: Float) {
    // Simplified to flat circle -- avoids radialGradient allocation per frame.
    // At this size the subtle gradient is imperceptible.
    val highlightCenter = Offset(center.x, center.y - innerRadius * 0.4f)
    drawCircle(
        color = CHROME_SPECULAR_COLOR,
        radius = innerRadius * 0.35f,
        center = highlightCenter
    )
}
