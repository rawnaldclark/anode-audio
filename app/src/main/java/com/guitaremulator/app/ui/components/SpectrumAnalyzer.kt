package com.guitaremulator.app.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.ui.theme.DesignSystem

/**
 * Spectrum analyzer display showing 64 logarithmically-spaced frequency bins
 * as a bar chart. Tappable to collapse/expand.
 *
 * @param spectrumData Array of 64 dB-scaled magnitude values in [-90, 0].
 * @param modifier Modifier for sizing and layout.
 */
@Composable
internal fun SpectrumAnalyzerDisplay(
    spectrumData: FloatArray,
    modifier: Modifier = Modifier
) {
    var expanded by remember { mutableStateOf(true) }

    Column(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
            .background(DesignSystem.MeterBg)
            .clickable { expanded = !expanded }
            .padding(horizontal = 8.dp, vertical = 4.dp)
    ) {
        // Header
        Text(
            text = if (expanded) "SPECTRUM" else "SPECTRUM (tap to expand)",
            fontSize = 9.sp,
            color = DesignSystem.EffectCategory.TIME.primary,
            fontWeight = FontWeight.Bold,
            letterSpacing = 1.sp
        )

        if (expanded) {
            SpectrumBars(
                spectrumData = spectrumData,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(100.dp)
                    .padding(top = 4.dp)
            )

            // Frequency axis labels
            FrequencyLabels(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 2.dp)
            )
        }
    }
}

/**
 * Compact mini spectrum display for the bottom metering strip.
 *
 * Shows the same 64 frequency bins as the full spectrum analyzer but in a
 * stripped-down form: fixed 32dp height, no header, no frequency labels,
 * no dB reference lines, and no tap-to-expand behavior.
 *
 * @param spectrumData Array of 64 dB-scaled magnitude values in [-90, 0].
 * @param modifier Modifier for sizing and layout.
 */
@Composable
fun MiniSpectrumDisplay(
    spectrumData: FloatArray,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier
            .fillMaxWidth()
            .height(32.dp)
            .clip(RoundedCornerShape(4.dp))
            .background(DesignSystem.MeterBg)
            .padding(horizontal = 4.dp)
    ) {
        SpectrumBars(
            spectrumData = spectrumData,
            showReferenceLines = false,
            modifier = Modifier.fillMaxWidth().height(32.dp)
        )
    }
}

/**
 * Canvas-based bar chart rendering 64 spectrum bins.
 *
 * Each bar is colored based on magnitude: green (quiet) -> yellow (medium) -> red (loud).
 * The dB range is [-90, 0] mapped to the full canvas height.
 *
 * @param spectrumData Array of 64 dB-scaled magnitude values in [-90, 0].
 * @param showReferenceLines Whether to draw dB reference lines (-12, -24, -48dB).
 * @param modifier Modifier for sizing and layout.
 */
@Composable
private fun SpectrumBars(
    spectrumData: FloatArray,
    showReferenceLines: Boolean = true,
    modifier: Modifier = Modifier
) {
    val numBins = 64
    val minDb = -90f
    val maxDb = 0f
    val dbRange = maxDb - minDb

    Canvas(modifier = modifier) {
        val canvasWidth = size.width
        val canvasHeight = size.height
        val barWidth = canvasWidth / numBins
        val gap = 1f // 1px gap between bars

        // Draw each frequency bin as a vertical bar
        for (i in 0 until numBins.coerceAtMost(spectrumData.size)) {
            val db = spectrumData[i].coerceIn(minDb, maxDb)

            // Normalize dB to 0..1 range for bar height
            val normalizedLevel = (db - minDb) / dbRange

            // Bar height (from bottom)
            val barHeight = normalizedLevel * canvasHeight

            // Color based on magnitude level
            val color = when {
                db > -6f -> DesignSystem.MeterRed      // Red: loud (above -6dB)
                db > -18f -> DesignSystem.MeterYellow  // Yellow: medium (-18 to -6dB)
                db > -36f -> DesignSystem.MeterGreen   // Green: moderate (-36 to -18dB)
                else -> DesignSystem.MeterDarkGreen     // Dark green: quiet (below -36dB)
            }

            // Draw bar from bottom up
            val x = i * barWidth + gap
            val effectiveBarWidth = (barWidth - gap * 2).coerceAtLeast(1f)

            drawRect(
                color = color,
                topLeft = Offset(x, canvasHeight - barHeight),
                size = Size(effectiveBarWidth, barHeight)
            )
        }

        // Draw dB reference lines (-12dB, -24dB, -48dB) when enabled
        if (showReferenceLines) {
            val lineColor = DesignSystem.Divider
            for (refDb in listOf(-12f, -24f, -48f)) {
                val y = canvasHeight * (1f - (refDb - minDb) / dbRange)
                drawLine(
                    color = lineColor,
                    start = Offset(0f, y),
                    end = Offset(canvasWidth, y),
                    strokeWidth = 1f
                )
            }
        }
    }
}

/**
 * Frequency axis labels below the spectrum bars.
 * Shows key frequency markers at log-spaced positions.
 */
@Composable
private fun FrequencyLabels(
    modifier: Modifier = Modifier
) {
    val textMeasurer = rememberTextMeasurer()
    val labelStyle = TextStyle(
        fontSize = 8.sp,
        color = DesignSystem.TextMuted
    )

    // Labels and their approximate positions (0.0 = left edge, 1.0 = right edge)
    // Positions are based on log mapping: pos = log(freq/20) / log(1000) mapped to 64 bins
    val labels = remember {
        listOf(
            "50" to 0.063f,
            "100" to 0.111f,
            "200" to 0.159f,
            "500" to 0.222f,
            "1k" to 0.270f,
            "2k" to 0.317f,
            "5k" to 0.381f,
            "10k" to 0.429f,
            "20k" to 0.476f
        )
    }

    Canvas(modifier = modifier.height(12.dp)) {
        val canvasWidth = size.width

        for ((text, normalizedPos) in labels) {
            // Map normalized log position to canvas x coordinate
            // The positions are relative fractions of the log range
            val x = normalizedPos * canvasWidth * (63f / 30f) // Scale to full width
            if (x < canvasWidth - 20f) {
                val measuredText = textMeasurer.measure(text, labelStyle)
                drawText(
                    textLayoutResult = measuredText,
                    topLeft = Offset(
                        x = (x - measuredText.size.width / 2f).coerceIn(0f, canvasWidth - measuredText.size.width),
                        y = 0f
                    )
                )
            }
        }
    }
}
