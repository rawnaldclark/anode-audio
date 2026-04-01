package com.guitaremulator.app.ui.drums

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.ui.theme.DesignSystem
import kotlinx.coroutines.delay

/**
 * Drum pad metadata for rendering a single pad in the grid.
 *
 * @property index          Zero-based pad index (0-7 in Jam Mode).
 * @property name           Short display label (e.g., "KICK", "SNARE").
 * @property categoryColor  Visual category color (Red/Amber/Green/Blue).
 * @property hasActiveSteps Whether the corresponding track has any triggered steps
 *                          in the current pattern (drives the jewel LED).
 */
data class DrumPadInfo(
    val index: Int,
    val name: String,
    val categoryColor: Color,
    val hasActiveSteps: Boolean
)

// ── Category Color Constants ─────────────────────────────────────────────────

/** Blue jewel color for percussion pads (rim, cowbell, shaker, etc.). */
val JewelBlue = Color(0xFF4088E0)

// ── DrumPadGrid Composable ──────────────────────────────────────────────────

/**
 * Configurable grid of velocity-sensitive drum pads in the Blackface Terminal style.
 *
 * Default layout is 2 rows x 4 columns (8 pads) for Jam Mode. Each pad is
 * Canvas-drawn with:
 * - Radial gradient background tinted with its category color
 * - 2px border in category color (brighter when selected)
 * - Monospace label showing the pad name
 * - Jewel LED indicator that flashes when the sequencer triggers the pad's track
 * - Press animation (scale 0.94) with haptic feedback
 *
 * @param pads               List of [DrumPadInfo] items (laid out row-major).
 * @param columns            Number of columns per row (default 4).
 * @param rows               Number of rows (default 2).
 * @param activeTracksOnStep Set of track indices that fired on the current step.
 *                           Drives the LED flash animation (50ms pulse).
 * @param selectedPad        Index of the currently selected pad (-1 for none).
 * @param onPadTap           Callback invoked with the pad index when tapped.
 * @param modifier           Layout modifier for the grid container.
 */
@Composable
fun DrumPadGrid(
    pads: List<DrumPadInfo>,
    columns: Int = 4,
    rows: Int = 2,
    activeTracksOnStep: Set<Int> = emptySet(),
    selectedPad: Int = -1,
    onPadTap: (index: Int) -> Unit,
    modifier: Modifier = Modifier
) {
    val haptic = LocalHapticFeedback.current
    val textMeasurer = rememberTextMeasurer()

    // Pre-compute the label text style once, not per-pad
    val labelStyle = remember {
        TextStyle(
            fontSize = 13.sp,
            color = DesignSystem.CreamWhite,
            fontFamily = FontFamily.Monospace,
            fontWeight = FontWeight.Bold
        )
    }

    Column(
        modifier = modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        for (row in 0 until rows) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f),
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                for (col in 0 until columns) {
                    val padIndex = row * columns + col
                    val pad = pads.getOrNull(padIndex) ?: continue
                    val isSelected = padIndex == selectedPad
                    val isFlashing = pad.index in activeTracksOnStep

                    DrumPad(
                        pad = pad,
                        isSelected = isSelected,
                        isFlashing = isFlashing,
                        labelStyle = labelStyle,
                        textMeasurer = textMeasurer,
                        onTap = {
                            haptic.performHapticFeedback(HapticFeedbackType.LongPress)
                            onPadTap(pad.index)
                        },
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxHeight()
                    )
                }
            }
        }
    }
}

// ── Individual Drum Pad ──────────────────────────────────────────────────────

/**
 * Single Canvas-drawn drum pad with press animation, LED flash, and jewel indicator.
 *
 * The LED flash animation briefly lights the pad when the sequencer triggers
 * the pad's track. Uses a 50ms flash duration with a fast decay animation
 * to create a pulse effect similar to hardware drum machine LEDs.
 *
 * @param pad          Pad metadata (name, color, active state).
 * @param isSelected   Whether this pad is the currently selected track.
 * @param isFlashing   Whether the pad should flash (sequencer hit on this track).
 * @param labelStyle   Pre-computed [TextStyle] for the pad label.
 * @param textMeasurer Text measurer for drawing the label on Canvas.
 * @param onTap        Callback when the pad is tapped.
 * @param modifier     Layout modifier.
 */
@Composable
private fun DrumPad(
    pad: DrumPadInfo,
    isSelected: Boolean,
    isFlashing: Boolean,
    labelStyle: TextStyle,
    textMeasurer: androidx.compose.ui.text.TextMeasurer,
    onTap: () -> Unit,
    modifier: Modifier = Modifier
) {
    val interactionSource = remember { MutableInteractionSource() }
    val isPressed by interactionSource.collectIsPressedAsState()

    // Press animation: scale down to 0.94 for a physical "push" feel
    val scale by animateFloatAsState(
        targetValue = if (isPressed) 0.94f else 1.0f,
        animationSpec = spring(dampingRatio = 0.55f, stiffness = 600f),
        label = "padScale"
    )

    // LED flash: when isFlashing becomes true, start a brief glow then decay
    var flashActive by remember { mutableStateOf(false) }
    LaunchedEffect(isFlashing) {
        if (isFlashing) {
            flashActive = true
            delay(50L) // 50ms flash duration
            flashActive = false
        }
    }

    // Animate the flash brightness for smooth decay
    val flashAlpha by animateFloatAsState(
        targetValue = if (flashActive) 0.35f else 0f,
        animationSpec = if (flashActive) {
            tween(durationMillis = 10) // Instant on
        } else {
            tween(durationMillis = 120) // Smooth 120ms decay
        },
        label = "flashAlpha"
    )

    // Pre-compute colors outside draw lambda to avoid per-frame allocation
    val categoryColor = pad.categoryColor
    val borderColor = remember(categoryColor, isSelected) {
        if (isSelected) categoryColor else categoryColor.copy(alpha = 0.6f)
    }
    val surfaceTint = remember(categoryColor) {
        categoryColor.copy(alpha = 0.08f)
    }

    // Pre-measure the label text to avoid per-frame measurement
    val measuredLabel = remember(pad.name, labelStyle) {
        textMeasurer.measure(pad.name, labelStyle)
    }

    Canvas(
        modifier = modifier
            .graphicsLayer {
                scaleX = scale
                scaleY = scale
            }
            .semantics {
                contentDescription = "${pad.name} drum pad"
            }
            .clickable(
                interactionSource = interactionSource,
                indication = null,
                onClick = onTap
            )
    ) {
        val w = size.width
        val h = size.height
        val cornerRadius = 8.dp.toPx()

        // ── Background: radial gradient tinted with category color ───────
        drawRoundRect(
            brush = Brush.radialGradient(
                colors = listOf(
                    DesignSystem.ModuleSurface.blend(surfaceTint),
                    DesignSystem.ModuleSurface
                ),
                center = Offset(w / 2f, h / 2f),
                radius = w * 0.7f
            ),
            cornerRadius = CornerRadius(cornerRadius),
            size = size
        )

        // ── Flash overlay: full-pad glow when sequencer triggers ─────────
        if (flashAlpha > 0f) {
            drawRoundRect(
                brush = Brush.radialGradient(
                    colors = listOf(
                        categoryColor.copy(alpha = flashAlpha),
                        categoryColor.copy(alpha = flashAlpha * 0.3f)
                    ),
                    center = Offset(w / 2f, h / 2f),
                    radius = w * 0.8f
                ),
                cornerRadius = CornerRadius(cornerRadius),
                size = size
            )
        }

        // ── Border: 2px in category color, brighter when selected ────────
        val borderWidth = 2.dp.toPx()
        val selectedBorderWidth = if (isSelected) 2.5.dp.toPx() else borderWidth
        drawRoundRect(
            color = borderColor,
            cornerRadius = CornerRadius(cornerRadius),
            size = size,
            style = Stroke(width = selectedBorderWidth)
        )

        // ── Selected highlight: subtle inner glow ────────────────────────
        if (isSelected) {
            drawRoundRect(
                brush = Brush.radialGradient(
                    colors = listOf(
                        categoryColor.copy(alpha = 0.12f),
                        Color.Transparent
                    ),
                    center = Offset(w / 2f, h / 2f),
                    radius = w * 0.6f
                ),
                cornerRadius = CornerRadius(cornerRadius),
                size = size
            )
        }

        // ── Label: monospace text centered in the pad ────────────────────
        val textWidth = measuredLabel.size.width.toFloat()
        val textHeight = measuredLabel.size.height.toFloat()
        drawText(
            textLayoutResult = measuredLabel,
            topLeft = Offset(
                x = (w - textWidth) / 2f,
                y = (h - textHeight) / 2f + h * 0.05f  // slightly below center
            )
        )

        // ── Jewel LED: small circle in top-right corner ──────────────────
        // LED is active when the track has steps OR when flashing
        val ledActive = pad.hasActiveSteps || flashAlpha > 0f
        drawJewelLed(
            center = Offset(w - 12.dp.toPx(), 12.dp.toPx()),
            radius = 5.dp.toPx(),
            active = ledActive,
            color = categoryColor,
            flashAlpha = flashAlpha
        )
    }
}

// ── Drawing Helpers ──────────────────────────────────────────────────────────

/**
 * Draw a small jewel LED indicator at the specified position.
 *
 * When active, renders with a radial glow and a bright hot spot.
 * When flashing (flashAlpha > 0), adds extra brightness to the glow.
 * When inactive, renders as a dark glass dot with faint ambient reflection.
 *
 * @param center     Position of the LED center.
 * @param radius     Radius of the LED body.
 * @param active     Whether the LED is lit.
 * @param color      LED color when active.
 * @param flashAlpha Extra flash brightness (0.0-1.0).
 */
private fun DrawScope.drawJewelLed(
    center: Offset,
    radius: Float,
    active: Boolean,
    color: Color,
    flashAlpha: Float = 0f
) {
    if (active) {
        // Glow bleed around the jewel (enhanced during flash)
        val glowMultiplier = 1f + flashAlpha * 2f
        drawCircle(
            brush = Brush.radialGradient(
                colors = listOf(
                    color.copy(alpha = (0.25f + flashAlpha * 0.4f).coerceAtMost(1f)),
                    Color.Transparent
                ),
                center = center,
                radius = radius * 2.5f * glowMultiplier
            ),
            radius = radius * 2.5f * glowMultiplier,
            center = center
        )

        // LED body: bright with radial gradient
        drawCircle(
            brush = Brush.radialGradient(
                colors = listOf(
                    color,
                    color.copy(alpha = 0.75f)
                ),
                center = center,
                radius = radius
            ),
            radius = radius,
            center = center
        )

        // Hot spot: white center for "lit" realism
        val hotSpotAlpha = (0.45f + flashAlpha * 0.3f).coerceAtMost(1f)
        drawCircle(
            color = Color.White.copy(alpha = hotSpotAlpha),
            radius = radius * 0.3f,
            center = Offset(center.x, center.y - radius * 0.15f)
        )
    } else {
        // Dark glass: muted inactive state
        drawCircle(
            color = color.copy(alpha = 0.15f),
            radius = radius,
            center = center
        )

        // Faint ambient reflection at top
        drawCircle(
            brush = Brush.radialGradient(
                colors = listOf(
                    Color.White.copy(alpha = 0.06f),
                    Color.Transparent
                ),
                center = Offset(center.x, center.y - radius * 0.3f),
                radius = radius * 0.5f
            ),
            radius = radius * 0.5f,
            center = Offset(center.x, center.y - radius * 0.3f)
        )
    }

    // Thin dark bezel ring
    drawCircle(
        color = Color(0xFF0A0A0E).copy(alpha = 0.5f),
        radius = radius,
        center = center,
        style = Stroke(width = radius * 0.15f)
    )
}

/**
 * Blend two colors by overlaying [overlay] on top of [this] at the overlay's alpha.
 *
 * Simple screen-like blend for tinting a base surface with a category color.
 */
private fun Color.blend(overlay: Color): Color {
    val a = overlay.alpha
    return Color(
        red = this.red * (1f - a) + overlay.red * a,
        green = this.green * (1f - a) + overlay.green * a,
        blue = this.blue * (1f - a) + overlay.blue * a,
        alpha = 1f
    )
}
