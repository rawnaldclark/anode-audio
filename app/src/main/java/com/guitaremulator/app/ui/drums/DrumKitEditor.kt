package com.guitaremulator.app.ui.drums

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.ui.components.RotaryKnob
import com.guitaremulator.app.ui.theme.DesignSystem

/**
 * Full-screen overlay for editing a drum track's voice synthesis parameters.
 *
 * Shows engine type selector at top, then a grid of RotaryKnobs for all voice
 * parameters. Param labels change based on engine type. Matches the
 * EffectEditorSheet visual style from PedalboardScreen.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun DrumKitEditor(
    trackIndex: Int,
    engineType: Int,
    voiceParams: List<Float>,
    onParamChange: (Int, Float) -> Unit,
    onEngineTypeChange: ((Int) -> Unit)? = null,
    onDismiss: () -> Unit,
    modifier: Modifier = Modifier
) {
    val engineNames = remember { listOf("FM", "SUB", "MTL", "NOI", "PHY", "MLT") }
    val paramDefs = remember(engineType) { getParamDefs(engineType) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(DesignSystem.Background.copy(alpha = 0.97f))
            .padding(16.dp)
            .verticalScroll(rememberScrollState())
    ) {
        // Header
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "TRACK ${trackIndex + 1} VOICE",
                color = DesignSystem.CreamWhite,
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.Bold,
                fontSize = 14.sp,
                letterSpacing = 2.sp
            )
            Text(
                text = "DONE",
                color = DesignSystem.VUAmber,
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.Bold,
                fontSize = 12.sp,
                letterSpacing = 1.sp,
                modifier = Modifier
                    .clickable { onDismiss() }
                    .padding(8.dp)
            )
        }

        Spacer(Modifier.height(12.dp))

        // Engine type selector
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            engineNames.forEachIndexed { index, name ->
                val isSelected = index == engineType
                Box(
                    modifier = Modifier
                        .weight(1f)
                        .height(32.dp)
                        .clip(RoundedCornerShape(4.dp))
                        .background(
                            if (isSelected) DesignSystem.VUAmber.copy(alpha = 0.15f)
                            else DesignSystem.ModuleSurface
                        )
                        .border(
                            width = 1.dp,
                            color = if (isSelected) DesignSystem.VUAmber
                            else DesignSystem.ModuleBorder,
                            shape = RoundedCornerShape(4.dp)
                        )
                        .clickable { onEngineTypeChange?.invoke(index) },
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = name,
                        color = if (isSelected) DesignSystem.VUAmber
                        else DesignSystem.TextSecondary,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold,
                        fontSize = 11.sp,
                        letterSpacing = 1.sp
                    )
                }
            }
        }

        Spacer(Modifier.height(16.dp))

        // Parameter knobs grid
        FlowRow(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
            maxItemsInEachRow = 3
        ) {
            paramDefs.forEachIndexed { paramIndex, paramDef ->
                val value = voiceParams.getOrElse(paramIndex) { 0.5f }
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    modifier = Modifier.width(100.dp)
                ) {
                    RotaryKnob(
                        value = value,
                        onValueChange = { onParamChange(paramIndex, it) },
                        label = paramDef.label,
                        displayValue = paramDef.formatValue(value),
                    )
                    Text(
                        text = paramDef.label,
                        color = DesignSystem.TextSecondary,
                        fontFamily = FontFamily.Monospace,
                        fontSize = 9.sp,
                        letterSpacing = 1.sp,
                        textAlign = TextAlign.Center
                    )
                }
            }
        }
    }
}

/** Parameter definition for a single knob. */
private data class ParamDef(
    val label: String,
    val min: Float = 0f,
    val max: Float = 1f,
    val unit: String = ""
) {
    fun formatValue(normalized: Float): String {
        val actual = min + normalized * (max - min)
        return if (max > 100f) "%.0f%s".format(actual, unit)
        else "%.1f%s".format(actual, unit)
    }
}

/** Returns parameter definitions based on engine type. */
private fun getParamDefs(engineType: Int): List<ParamDef> = when (engineType) {
    0 -> listOf( // FM
        ParamDef("FREQ", 40f, 400f, "Hz"),
        ParamDef("RATIO", 0.5f, 8f),
        ParamDef("DEPTH", 0f, 10f),
        ParamDef("P.DEC", 5f, 500f, "ms"),
        ParamDef("A.DEC", 20f, 2000f, "ms"),
        ParamDef("FILTER", 100f, 20000f, "Hz"),
        ParamDef("RES", 0f, 1f),
        ParamDef("DRIVE", 0f, 5f)
    )
    1 -> listOf( // Subtractive
        ParamDef("FREQ", 80f, 800f, "Hz"),
        ParamDef("T.DEC", 10f, 500f, "ms"),
        ParamDef("N.DEC", 10f, 500f, "ms"),
        ParamDef("MIX", 0f, 1f),
        ParamDef("FILTER", 200f, 16000f, "Hz"),
        ParamDef("RES", 0f, 1f),
        ParamDef("F.DEC", 5f, 500f, "ms"),
        ParamDef("F.DPTH", 0f, 8f),
        ParamDef("DRIVE", 0f, 5f)
    )
    2 -> listOf( // Metallic
        ParamDef("COLOR", 0f, 1f),
        ParamDef("TONE", 0f, 1f),
        ParamDef("DECAY", 5f, 2000f, "ms"),
        ParamDef("HP CUT", 2000f, 12000f, "Hz")
    )
    3 -> listOf( // Noise
        ParamDef("TYPE", 0f, 1f),
        ParamDef("MODE", 0f, 2f),
        ParamDef("FREQ", 100f, 16000f, "Hz"),
        ParamDef("RES", 0f, 1f),
        ParamDef("DECAY", 5f, 2000f, "ms"),
        ParamDef("SWEEP", 0f, 1f)
    )
    4 -> listOf( // Physical
        ParamDef("MODEL", 0f, 1f),
        ParamDef("PITCH", 20f, 120f),
        ParamDef("DECAY", 0f, 1f),
        ParamDef("BRIGHT", 0f, 1f),
        ParamDef("MATRL", 0f, 1f),
        ParamDef("DAMP", 0f, 1f)
    )
    5 -> listOf( // Multi-Layer
        ParamDef("VOX A", 0f, 4f),
        ParamDef("VOX B", 0f, 4f),
        ParamDef("MIX", 0f, 1f),
        ParamDef("DRIVE", 0f, 5f)
    )
    else -> emptyList()
}
