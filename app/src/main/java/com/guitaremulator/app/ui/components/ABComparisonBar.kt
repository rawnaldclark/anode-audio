package com.guitaremulator.app.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.ui.theme.DesignSystem

/**
 * A/B comparison control bar shown when A/B mode is active.
 *
 * Displays:
 *   - [A] and [B] toggle buttons (active slot highlighted green)
 *   - Keep A, Keep B, and Cancel buttons to exit A/B mode
 *
 * @param activeSlot Currently active slot ('A' or 'B').
 * @param onToggle Callback to switch between A and B.
 * @param onKeepA Exit A/B mode and keep slot A's settings.
 * @param onKeepB Exit A/B mode and keep slot B's settings.
 * @param onCancel Exit A/B mode and revert to original (slot A) settings.
 * @param modifier Modifier for layout.
 */
@Composable
internal fun ABComparisonBar(
    activeSlot: Char,
    onToggle: () -> Unit,
    onKeepA: () -> Unit,
    onKeepB: () -> Unit,
    onCancel: () -> Unit,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
            .background(DesignSystem.ModuleSurface)
            .padding(horizontal = 8.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        // A/B toggle buttons
        Row(verticalAlignment = Alignment.CenterVertically) {
            // Slot A button
            Button(
                onClick = { if (activeSlot != 'A') onToggle() },
                modifier = Modifier.size(width = 44.dp, height = 36.dp),
                colors = ButtonDefaults.buttonColors(
                    containerColor = if (activeSlot == 'A') DesignSystem.SafeGreen.copy(alpha = 0.7f) else DesignSystem.ModuleBorder
                ),
                shape = RoundedCornerShape(6.dp)
            ) {
                Text(
                    text = "A",
                    fontWeight = FontWeight.Bold,
                    fontSize = 14.sp,
                    color = Color.White
                )
            }

            Spacer(modifier = Modifier.width(4.dp))

            // Slot B button
            Button(
                onClick = { if (activeSlot != 'B') onToggle() },
                modifier = Modifier.size(width = 44.dp, height = 36.dp),
                colors = ButtonDefaults.buttonColors(
                    containerColor = if (activeSlot == 'B') DesignSystem.SafeGreen.copy(alpha = 0.7f) else DesignSystem.ModuleBorder
                ),
                shape = RoundedCornerShape(6.dp)
            ) {
                Text(
                    text = "B",
                    fontWeight = FontWeight.Bold,
                    fontSize = 14.sp,
                    color = Color.White
                )
            }

            Spacer(modifier = Modifier.width(8.dp))

            // Active indicator label
            Text(
                text = "A/B",
                fontSize = 10.sp,
                color = DesignSystem.SafeGreen,
                fontWeight = FontWeight.Bold,
                letterSpacing = 1.sp
            )
        }

        // Exit buttons
        Row(verticalAlignment = Alignment.CenterVertically) {
            TextButton(
                onClick = onKeepA,
                modifier = Modifier.height(32.dp)
            ) {
                Text("Keep A", fontSize = 11.sp, color = DesignSystem.EffectCategory.TIME.knobAccent)
            }

            TextButton(
                onClick = onKeepB,
                modifier = Modifier.height(32.dp)
            ) {
                Text("Keep B", fontSize = 11.sp, color = DesignSystem.EffectCategory.TIME.knobAccent)
            }

            TextButton(
                onClick = onCancel,
                modifier = Modifier.height(32.dp)
            ) {
                Text("Cancel", fontSize = 11.sp, color = DesignSystem.TextSecondary)
            }
        }
    }
}

/**
 * Button to enter A/B comparison mode.
 *
 * @param onClick Callback when tapped.
 * @param modifier Modifier for layout.
 */
@Composable
internal fun ABModeButton(
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    TextButton(
        onClick = onClick,
        modifier = modifier.height(32.dp)
    ) {
        Text(
            text = "A/B",
            fontSize = 11.sp,
            color = DesignSystem.EffectCategory.UTILITY.primary,
            fontWeight = FontWeight.Bold
        )
    }
}
