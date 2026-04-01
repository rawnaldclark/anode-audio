package com.guitaremulator.app.ui.drums

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.ui.theme.DesignSystem

/**
 * Quick-access mixer strip for the currently selected drum track.
 *
 * Displays engine type badge, volume/pan/tone/decay knobs, and mute/solo buttons.
 * Full implementation pending (Task 14) -- this is a functional placeholder.
 *
 * @param engineType    Voice engine type index (0-5).
 * @param volume        Track volume (0.0-1.0).
 * @param pan           Track pan (-1.0 to 1.0).
 * @param muted         Whether the track is muted.
 * @param soloed        Whether the track is soloed.
 * @param onVolumeChange  Volume change callback.
 * @param onPanChange     Pan change callback.
 * @param onMuteToggle    Mute toggle callback.
 * @param onSoloToggle    Solo toggle callback.
 * @param onEditClick     Callback to open the full kit editor.
 * @param modifier        Layout modifier.
 */
@Composable
fun DrumMixerStrip(
    engineType: Int,
    volume: Float,
    pan: Float,
    muted: Boolean,
    soloed: Boolean,
    onVolumeChange: (Float) -> Unit,
    onPanChange: (Float) -> Unit,
    onMuteToggle: () -> Unit,
    onSoloToggle: () -> Unit,
    onEditClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    // TODO: Full implementation with RotaryKnobs, engine badge, and
    //       Mute/Solo/Edit buttons (Task 14).
    val engineName = when (engineType) {
        0 -> "FM"
        1 -> "SUB"
        2 -> "MTL"
        3 -> "NOI"
        4 -> "PHY"
        5 -> "MLT"
        else -> "???"
    }
    Row(
        modifier = modifier
            .fillMaxWidth()
            .background(DesignSystem.ControlPanelSurface)
            .padding(horizontal = 12.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = "[$engineName]",
            fontSize = 12.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            color = JewelBlue
        )
        Text(
            text = "  VOL %.0f%%  PAN %.0f".format(volume * 100, pan * 100),
            fontSize = 11.sp,
            fontFamily = FontFamily.Monospace,
            color = DesignSystem.TextSecondary,
            modifier = Modifier.weight(1f)
        )
        Text(
            text = if (muted) "M" else "",
            fontSize = 12.sp,
            fontWeight = FontWeight.Bold,
            color = DesignSystem.JewelRed
        )
        Text(
            text = if (soloed) " S" else "",
            fontSize = 12.sp,
            fontWeight = FontWeight.Bold,
            color = DesignSystem.VUAmber
        )
    }
}
