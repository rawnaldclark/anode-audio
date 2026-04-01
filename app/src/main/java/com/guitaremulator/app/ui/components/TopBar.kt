package com.guitaremulator.app.ui.components

import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.runtime.remember
import androidx.compose.runtime.getValue
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import com.guitaremulator.app.ui.theme.DesignSystem

/**
 * Fixed top bar for the main pedalboard screen.
 *
 * Styled as an amp head faceplate with brushed aluminum gradient background,
 * an inset preset display panel, gold piping underline, silk-screened icon
 * markings, and a subtle bottom edge shadow.
 *
 * Provides a compact 56dp-height strip with three sections:
 * - **Left**: Clickable preset name in a recessed display panel, opening the preset browser.
 * - **Center**: Engine status LED and power toggle button.
 * - **Right**: Quick-access icon buttons for tuner, A/B comparison, and settings.
 *
 * @param presetName Name of the currently active preset, shown truncated if too long.
 * @param isRunning Whether the audio engine is currently running.
 * @param engineError Current engine error message, or empty string if no error.
 * @param abMode Whether A/B comparison mode is currently active.
 * @param activeSlot The currently active A/B slot ('A' or 'B'). Only meaningful when [abMode] is true.
 * @param onPresetClick Callback invoked when the user taps the preset name to open the preset browser.
 * @param onToggleEngine Callback invoked when the user taps the power button to start or stop the engine.
 * @param onTunerClick Callback invoked when the user taps the tuner icon.
 * @param onLooperClick Callback invoked when the user taps the looper icon.
 * @param isLooperActive Whether the looper is currently active (state != IDLE), used to tint the icon.
 * @param onToggleAB Callback invoked when the user taps the A/B button while in A/B mode to switch slots.
 * @param onEnterAbMode Callback invoked when the user taps the A/B button to enter A/B comparison mode.
 * @param onExitAbMode Callback invoked when exiting A/B mode, passing the slot character to keep.
 * @param onSettingsClick Callback invoked when the user taps the settings gear icon.
 * @param focusMode Whether Focus Mode is active (show only enabled effects with bypass gutters).
 * @param onToggleViewMode Callback invoked when the user taps the Focus/Chain toggle.
 * @param modifier Optional [Modifier] applied to the root layout.
 */
@Composable
fun TopBar(
    presetName: String,
    isRunning: Boolean,
    engineError: String,
    abMode: Boolean,
    activeSlot: Char,
    onPresetClick: () -> Unit,
    onToggleEngine: () -> Unit,
    onTunerClick: () -> Unit,
    onLooperClick: () -> Unit = {},
    isLooperActive: Boolean = false,
    onToggleAB: () -> Unit,
    onEnterAbMode: () -> Unit,
    onExitAbMode: (Char) -> Unit,
    onSettingsClick: () -> Unit,
    onDrumsClick: () -> Unit = {},
    focusMode: Boolean = true,
    onToggleViewMode: () -> Unit = {},
    modifier: Modifier = Modifier
) {
    // Resolve LED and power icon colors based on engine state
    val hasError = engineError.isNotEmpty()
    val ledActiveColor = if (hasError) DesignSystem.ClipRed else DesignSystem.SafeGreen
    val powerIconTint = if (isRunning) DesignSystem.SafeGreen else DesignSystem.CreamWhite

    // Colors cached outside drawBehind to avoid unnecessary allocations
    val goldPipingColor = DesignSystem.GoldPiping.copy(alpha = 0.60f)
    val panelShadowColor = DesignSystem.PanelShadow
    val bottomShadowBrush = Brush.verticalGradient(
        colors = listOf(
            DesignSystem.PanelShadow.copy(alpha = 0.5f),
            Color.Transparent
        )
    )

    // Inset panel border colors for the recessed preset display
    val panelHighlightColor = DesignSystem.PanelHighlight
    val panelOuterShadowColor = DesignSystem.PanelShadow

    Row(
        modifier = modifier
            .fillMaxWidth()
            .height(56.dp)
            .background(remember { DesignSystem.brushedMetalGradient() })
            .drawBehind {
                val barHeight = size.height
                val barWidth = size.width

                // -- Gold piping underline (2dp gold + 1dp shadow below) --
                val goldStroke = 2.dp.toPx()
                val shadowStroke = 1.dp.toPx()
                drawLine(
                    color = goldPipingColor,
                    start = Offset(0f, barHeight - goldStroke / 2f),
                    end = Offset(barWidth, barHeight - goldStroke / 2f),
                    strokeWidth = goldStroke
                )
                drawLine(
                    color = panelShadowColor,
                    start = Offset(0f, barHeight + shadowStroke / 2f),
                    end = Offset(barWidth, barHeight + shadowStroke / 2f),
                    strokeWidth = shadowStroke
                )

                // -- Bottom edge shadow gradient (4dp, below the bar) --
                val shadowHeight = 4.dp.toPx()
                drawRect(
                    brush = bottomShadowBrush,
                    topLeft = Offset(0f, barHeight),
                    size = Size(barWidth, shadowHeight)
                )
            }
            .padding(horizontal = 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        // -- Left section: Clickable preset name in inset display panel --

        Row(
            modifier = Modifier
                .weight(1f)
                .clickable(onClickLabel = "Open preset browser", onClick = onPresetClick),
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Recessed display panel wrapping the preset name
            Box(
                modifier = Modifier
                    .weight(1f, fill = false)
                    .background(
                        color = DesignSystem.AmpChassisGray,
                        shape = RoundedCornerShape(4.dp)
                    )
                    .border(
                        width = 1.dp,
                        color = panelOuterShadowColor,
                        shape = RoundedCornerShape(4.dp)
                    )
                    .drawBehind {
                        // Inner highlight edge (1px inset) to simulate beveled recess
                        val inset = 1.dp.toPx()
                        val w = size.width
                        val h = size.height
                        // Top inner highlight
                        drawLine(
                            color = panelHighlightColor,
                            start = Offset(inset, inset),
                            end = Offset(w - inset, inset),
                            strokeWidth = 1.dp.toPx()
                        )
                        // Left inner highlight
                        drawLine(
                            color = panelHighlightColor,
                            start = Offset(inset, inset),
                            end = Offset(inset, h - inset),
                            strokeWidth = 1.dp.toPx()
                        )
                    }
                    .padding(horizontal = 8.dp, vertical = 4.dp)
            ) {
                Text(
                    text = presetName.ifEmpty { "Browse Presets" },
                    fontSize = 16.sp,
                    fontWeight = FontWeight.SemiBold,
                    fontFamily = FontFamily.Monospace,
                    color = DesignSystem.CreamWhite,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
            }

            Spacer(modifier = Modifier.width(4.dp))

            Text(
                text = "\u25BC", // Down-pointing triangle
                fontSize = 10.sp,
                color = DesignSystem.CreamWhite.copy(alpha = 0.6f)
            )
        }

        Spacer(modifier = Modifier.width(4.dp))

        // -- Center section: Engine status LED + power toggle --
        // When engine is OFF: show a prominent "START ENGINE" label with a pulsing
        // play icon so first-time users immediately know what to tap.
        // When engine is ON: collapse to the normal subtle LED + stop icon.

        // Slow breathing pulse for the OFF state (2s cycle, alpha 0.4 -> 1.0)
        val pulseTransition = rememberInfiniteTransition(label = "enginePulse")
        val pulseAlpha by pulseTransition.animateFloat(
            initialValue = 0.4f,
            targetValue = 1.0f,
            animationSpec = infiniteRepeatable(
                animation = tween(durationMillis = 1200, easing = androidx.compose.animation.core.EaseInOut),
                repeatMode = RepeatMode.Reverse
            ),
            label = "pulseAlpha"
        )

        // Smooth fade-out when engine starts (300ms)
        val offLabelAlpha by animateFloatAsState(
            targetValue = if (isRunning) 0f else 1f,
            animationSpec = tween(durationMillis = 300),
            label = "offLabelAlpha"
        )

        if (!isRunning) {
            // -- Engine OFF: prominent tappable start control --
            Row(
                modifier = Modifier
                    .clickable(onClickLabel = "Start audio engine", onClick = onToggleEngine)
                    .graphicsLayer { alpha = offLabelAlpha }
                    .background(
                        color = DesignSystem.AmpChassisGray.copy(alpha = 0.6f),
                        shape = RoundedCornerShape(4.dp)
                    )
                    .border(
                        width = 1.dp,
                        color = DesignSystem.GoldPiping.copy(alpha = 0.4f),
                        shape = RoundedCornerShape(4.dp)
                    )
                    .padding(horizontal = 8.dp, vertical = 4.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Pulsing play icon
                Icon(
                    imageVector = Icons.Default.PlayArrow,
                    contentDescription = "Start engine",
                    tint = DesignSystem.CreamWhite,
                    modifier = Modifier
                        .size(18.dp)
                        .graphicsLayer { alpha = pulseAlpha }
                )

                Spacer(modifier = Modifier.width(4.dp))

                // "START ENGINE" label in monospace CreamWhite
                Text(
                    text = "START",
                    fontSize = 10.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace,
                    color = DesignSystem.CreamWhite,
                    letterSpacing = 1.sp,
                    modifier = Modifier.graphicsLayer { alpha = pulseAlpha }
                )
            }
        } else {
            // -- Engine ON: subtle LED + stop button (normal running state) --
            LedIndicator(
                active = true,
                activeColor = ledActiveColor,
                inactiveColor = DesignSystem.TextMuted,
                size = 10.dp
            )

            IconButton(
                onClick = onToggleEngine,
                modifier = Modifier.size(48.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.PlayArrow,
                    contentDescription = "Stop engine",
                    tint = DesignSystem.SafeGreen,
                    modifier = Modifier.size(20.dp)
                )
            }
        }

        // -- Right section: Focus/Chain toggle, Tuner, Looper, A/B, Settings --
        // Icons use CreamWhite to simulate silk-screened panel markings.
        // Active states use the appropriate category color.

        // Focus/Chain view mode toggle
        // Focus mode: funnel symbol (active filter). Chain mode: list symbol (show all).
        IconButton(
            onClick = onToggleViewMode,
            modifier = Modifier
                .size(48.dp)
                .semantics {
                    this.contentDescription = if (focusMode) "Show all effects" else "Show active effects only"
                }
        ) {
            Text(
                text = if (focusMode) "\u2AF6" else "\u2261", // ⫶ (triple colon) vs ≡ (hamburger)
                fontSize = 18.sp,
                color = if (focusMode) DesignSystem.GoldPiping else DesignSystem.CreamWhite
            )
        }

        // Tuner button
        IconButton(
            onClick = onTunerClick,
            modifier = Modifier
                .size(48.dp)
                .semantics { this.contentDescription = "Tuner" }
        ) {
            Text(
                text = "\u266A", // Musical note
                fontSize = 18.sp,
                color = DesignSystem.CreamWhite
            )
        }

        // Looper button (tinted SafeGreen when active, CreamWhite otherwise)
        IconButton(
            onClick = onLooperClick,
            modifier = Modifier
                .size(48.dp)
                .semantics { this.contentDescription = if (isLooperActive) "Looper active" else "Looper" }
        ) {
            Text(
                text = "\u27F3", // Clockwise gapped circle arrow
                fontSize = 18.sp,
                color = if (isLooperActive) DesignSystem.SafeGreen else DesignSystem.CreamWhite
            )
        }

        // Drum machine button
        IconButton(
            onClick = onDrumsClick,
            modifier = Modifier
                .size(48.dp)
                .semantics { this.contentDescription = "Drum machine" }
        ) {
            Text(
                text = "\u259A", // Grid-like symbol for drums
                fontSize = 18.sp,
                color = DesignSystem.CreamWhite
            )
        }

        // A/B comparison button - contextual based on engine and mode state
        if (abMode) {
            // In A/B mode: show active slot letter, tap to toggle between A and B
            IconButton(
                onClick = onToggleAB,
                modifier = Modifier
                    .size(48.dp)
                    .semantics { this.contentDescription = "Switch to slot ${if (activeSlot == 'A') 'B' else 'A'}" }
            ) {
                Text(
                    text = activeSlot.toString(),
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Bold,
                    color = DesignSystem.SafeGreen
                )
            }
        } else {
            // A/B entry button - always visible, disabled when engine is not running
            IconButton(
                onClick = { if (isRunning) onEnterAbMode() },
                enabled = isRunning,
                modifier = Modifier
                    .size(48.dp)
                    .graphicsLayer { alpha = if (isRunning) 1f else 0.3f }
                    .semantics { this.contentDescription = "Enter A/B comparison" }
            ) {
                Text(
                    text = "A/B",
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Bold,
                    color = DesignSystem.CreamWhite
                )
            }
        }

        // Settings button
        IconButton(
            onClick = onSettingsClick,
            modifier = Modifier.size(48.dp)
        ) {
            Icon(
                imageVector = Icons.Default.Settings,
                contentDescription = "Settings",
                tint = DesignSystem.CreamWhite,
                modifier = Modifier.size(20.dp)
            )
        }
    }
}
