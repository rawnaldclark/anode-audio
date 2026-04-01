package com.guitaremulator.app.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.ui.theme.DesignSystem
import kotlin.math.pow

/**
 * Master controls section: input gain, output gain, and global bypass toggle.
 *
 * Compact layout with labeled sliders for gain control and a bypass switch
 * for quick A/B comparison between processed and dry signal.
 */
@Composable
internal fun MasterControls(
    inputGain: Float,
    outputGain: Float,
    globalBypass: Boolean,
    onInputGainChange: (Float) -> Unit,
    onOutputGainChange: (Float) -> Unit,
    onToggleBypass: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(DesignSystem.ModuleCornerRadius))
            .background(DesignSystem.ModuleSurface)
            .padding(horizontal = 12.dp, vertical = 10.dp)
    ) {
        // Header row with title and bypass toggle
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "MASTER",
                fontSize = 10.sp,
                color = DesignSystem.TextSecondary,
                fontWeight = FontWeight.Bold,
                letterSpacing = 1.sp
            )
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    text = if (globalBypass) "BYPASS" else "ACTIVE",
                    fontSize = 10.sp,
                    color = if (globalBypass) DesignSystem.ClipRed else DesignSystem.SafeGreen,
                    fontWeight = FontWeight.Bold,
                    letterSpacing = 1.sp
                )
                Spacer(modifier = Modifier.width(6.dp))
                Switch(
                    checked = !globalBypass,
                    onCheckedChange = { onToggleBypass() },
                    colors = SwitchDefaults.colors(
                        checkedThumbColor = Color.White,
                        checkedTrackColor = DesignSystem.SafeGreen,
                        uncheckedThumbColor = Color(0xFFBBBBBB),
                        uncheckedTrackColor = Color(0xFF4A1A1A)
                    ),
                    modifier = Modifier.height(24.dp)
                )
            }
        }

        Spacer(modifier = Modifier.height(6.dp))

        // Input gain slider
        GainSlider(
            label = "IN",
            gain = inputGain,
            onGainChange = onInputGainChange
        )

        // Output gain slider
        GainSlider(
            label = "OUT",
            gain = outputGain,
            onGainChange = onOutputGainChange
        )
    }
}

/**
 * Compact gain slider with dB-linear scale and readout.
 *
 * The slider position maps linearly to dB (-60 to +12), which feels
 * natural to musicians. Equal slider distances = equal dB changes.
 * Special case: the bottom of the range is "Mute" (gain = 0).
 */
@Composable
internal fun GainSlider(
    label: String,
    gain: Float,
    onGainChange: (Float) -> Unit
) {
    // Convert linear gain to dB for the slider position
    val dbValue = if (gain > 0.001f) {
        (20f * kotlin.math.log10(gain)).coerceIn(-60f, 12f)
    } else {
        -60f
    }

    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = label,
            fontSize = 11.sp,
            color = DesignSystem.TextSecondary,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.width(28.dp)
        )
        Slider(
            value = dbValue,
            onValueChange = { newDb ->
                val newGain = if (newDb <= -59f) 0f
                    else 10f.pow(newDb / 20f).coerceIn(0f, 4f)
                onGainChange(newGain)
            },
            valueRange = -60f..12f,
            colors = SliderDefaults.colors(
                thumbColor = DesignSystem.SafeGreen,
                activeTrackColor = DesignSystem.SafeGreen,
                inactiveTrackColor = DesignSystem.ModuleBorder
            ),
            modifier = Modifier.weight(1f)
        )
        Text(
            text = when {
                dbValue <= -59f -> "Mute"
                else -> "%+.1f dB".format(dbValue)
            },
            fontSize = 11.sp,
            color = if (dbValue <= -59f) DesignSystem.TextSecondary else DesignSystem.TextPrimary,
            modifier = Modifier.width(56.dp)
        )
    }
}
