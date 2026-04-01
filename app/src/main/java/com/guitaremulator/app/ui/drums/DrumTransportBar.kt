package com.guitaremulator.app.ui.drums

import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.spring
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.ui.theme.DesignSystem

/**
 * Transport bar for the drum machine.
 *
 * Provides interactive play/stop button, BPM display, swing display,
 * and TAP tempo button. Play/Stop toggles sequencer playback with
 * visual state feedback. TAP records timestamps for the native median
 * filter to derive BPM.
 *
 * @param isPlaying       Whether the sequencer is currently playing.
 * @param tempo           Current BPM.
 * @param swing           Current swing percentage (50-75).
 * @param tempoMultiplier Current tempo multiplier (0.5/1.0/2.0).
 * @param onPlayStop      Toggle play/stop callback.
 * @param onTempoChange   Tempo change callback (BPM float).
 * @param onSwingChange   Swing change callback (percentage float).
 * @param onTapTempo      Tap tempo callback.
 * @param onFillPress     Fill button press callback.
 * @param onFillRelease   Fill button release callback.
 * @param modifier        Layout modifier.
 */
@Composable
fun DrumTransportBar(
    isPlaying: Boolean,
    tempo: Float,
    swing: Float,
    tempoMultiplier: Float,
    onPlayStop: () -> Unit,
    onTempoChange: (Float) -> Unit,
    onSwingChange: (Float) -> Unit,
    onTapTempo: () -> Unit,
    onFillPress: () -> Unit,
    onFillRelease: () -> Unit,
    modifier: Modifier = Modifier
) {
    val haptic = LocalHapticFeedback.current

    Row(
        modifier = modifier
            .fillMaxWidth()
            .background(DesignSystem.ControlPanelSurface)
            .padding(horizontal = 8.dp, vertical = 6.dp),
        horizontalArrangement = Arrangement.spacedBy(6.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        // ── Play/Stop Button ────────────────────────────────────────────
        // Interactive button that toggles playback state with visual feedback.
        // Uses collectIsPressedAsState for animated press-down color shift.
        val playInteraction = remember { MutableInteractionSource() }
        val playPressed by playInteraction.collectIsPressedAsState()

        val playBgColor by animateColorAsState(
            targetValue = when {
                playPressed -> if (isPlaying) DesignSystem.JewelRed.copy(alpha = 0.4f)
                               else DesignSystem.SafeGreen.copy(alpha = 0.4f)
                isPlaying -> DesignSystem.JewelRed.copy(alpha = 0.2f)
                else -> DesignSystem.ModuleSurface
            },
            animationSpec = spring(stiffness = 600f),
            label = "playBg"
        )

        val playBorderColor = if (isPlaying) DesignSystem.JewelRed else DesignSystem.SafeGreen
        val playTextColor = if (isPlaying) DesignSystem.JewelRed else DesignSystem.SafeGreen

        Box(
            modifier = Modifier
                .clip(RoundedCornerShape(6.dp))
                .background(playBgColor, RoundedCornerShape(6.dp))
                .border(
                    width = 1.5.dp,
                    color = playBorderColor.copy(alpha = 0.7f),
                    shape = RoundedCornerShape(6.dp)
                )
                .clickable(
                    interactionSource = playInteraction,
                    indication = null, // Background animation serves as visual feedback
                    role = Role.Button
                ) {
                    haptic.performHapticFeedback(HapticFeedbackType.LongPress)
                    onPlayStop()
                }
                .padding(horizontal = 14.dp, vertical = 8.dp),
            contentAlignment = Alignment.Center
        ) {
            Text(
                text = if (isPlaying) "\u25A0 STOP" else "\u25B6 PLAY",
                fontSize = 14.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                color = playTextColor
            )
        }

        // ── BPM Display ─────────────────────────────────────────────────
        // Read-only inset display showing the current effective BPM
        // (base tempo multiplied by the tempo multiplier).
        Box(
            modifier = Modifier
                .background(
                    color = DesignSystem.AmpChassisGray,
                    shape = RoundedCornerShape(4.dp)
                )
                .border(
                    width = 1.dp,
                    color = DesignSystem.PanelShadow,
                    shape = RoundedCornerShape(4.dp)
                )
                .padding(horizontal = 10.dp, vertical = 6.dp),
            contentAlignment = Alignment.Center
        ) {
            Text(
                text = "%.1f BPM".format(tempo * tempoMultiplier),
                fontSize = 16.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                color = DesignSystem.VUAmber
            )
        }

        // ── Swing Display ───────────────────────────────────────────────
        // Read-only swing percentage indicator.
        Text(
            text = "SW %.0f%%".format(swing),
            fontSize = 11.sp,
            fontFamily = FontFamily.Monospace,
            color = DesignSystem.TextSecondary
        )

        Spacer(modifier = Modifier.weight(1f))

        // ── TAP Tempo Button ────────────────────────────────────────────
        // Each tap records a System.nanoTime() timestamp; the native
        // median filter computes BPM from the inter-tap intervals.
        val tapInteraction = remember { MutableInteractionSource() }
        val tapPressed by tapInteraction.collectIsPressedAsState()

        val tapBgColor by animateColorAsState(
            targetValue = if (tapPressed) DesignSystem.VUAmber.copy(alpha = 0.35f)
                          else DesignSystem.ModuleSurface,
            animationSpec = spring(stiffness = 600f),
            label = "tapBg"
        )

        Box(
            modifier = Modifier
                .clip(RoundedCornerShape(6.dp))
                .background(tapBgColor, RoundedCornerShape(6.dp))
                .border(
                    width = 1.5.dp,
                    color = DesignSystem.VUAmber.copy(alpha = 0.6f),
                    shape = RoundedCornerShape(6.dp)
                )
                .clickable(
                    interactionSource = tapInteraction,
                    indication = null, // Background animation serves as visual feedback
                    role = Role.Button
                ) {
                    haptic.performHapticFeedback(HapticFeedbackType.LongPress)
                    onTapTempo()
                }
                .padding(horizontal = 12.dp, vertical = 8.dp),
            contentAlignment = Alignment.Center
        ) {
            Text(
                text = "TAP",
                fontSize = 13.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                color = DesignSystem.VUAmber
            )
        }
    }
}
