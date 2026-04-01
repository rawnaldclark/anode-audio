package com.guitaremulator.app.ui.components

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.animation.expandVertically
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
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
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.ui.theme.DesignSystem
import kotlin.math.log10

// ── Constants ────────────────────────────────────────────────────────────────

/** Minimum dB value displayed on meters and spectrum. Below this we show "-inf". */
private const val MIN_DB = -60f

/** Threshold below which a level is considered silence (used for -inf display). */
private const val SILENCE_THRESHOLD = 0.001f

/** Spectrum analyzer dB floor for bar height normalization. */
private const val SPECTRUM_MIN_DB = -90f

/** Spectrum analyzer dB ceiling for bar height normalization. */
private const val SPECTRUM_MAX_DB = 0f

/** Number of frequency bins expected in the spectrum data array. */
private const val SPECTRUM_NUM_BINS = 64

/** Maximum gain value (4x linear = +12dB). */
private const val MAX_GAIN = 4f

/**
 * Collapsible bottom metering strip for the Guitar Emulator app's main screen.
 *
 * In its **collapsed state** (52dp), the strip shows a compact horizontal metering
 * display with input/output level meters, dBFS readouts, and a mini spectrum analyzer.
 * Tapping the collapsed row toggles expansion.
 *
 * In its **expanded state** (~180dp additional), a full-height spectrum analyzer and
 * master controls (input/output gain knobs + global bypass stomp switch) are revealed
 * below the collapsed row via an [AnimatedVisibility] slide.
 *
 * ## Visual Design ("Blackface Terminal" Hardware Aesthetic)
 * - Double-line machined panel border (PanelShadow outer, PanelHighlight inner)
 * - Brushed aluminum gradient background instead of flat ModuleSurface
 * - Warm VU amber backlight glow behind each meter bar
 *
 * ## Design Tokens
 * All colors are sourced from [DesignSystem] tokens. No hardcoded hex values.
 *
 * ## Gain Normalization
 * Gain values are in the range [0, 4] (0x to 4x, i.e., -inf to +12dB).
 * The [RotaryKnob] expects a normalized 0..1 value, so we map linearly:
 * `knobValue = gain / 4f` and `gain = knobValue * 4f`.
 *
 * @param inputLevel  Current input signal level in [0.0, 1.0].
 * @param outputLevel Current output signal level in [0.0, 1.0].
 * @param spectrumData Array of 64 dB-scaled magnitude values in [-90, 0].
 * @param inputGain  Master input gain in [0, 4] (linear).
 * @param outputGain Master output gain in [0, 4] (linear).
 * @param globalBypass Whether the entire signal chain is bypassed.
 * @param onInputGainChange  Callback with the new input gain value in [0, 4].
 * @param onOutputGainChange Callback with the new output gain value in [0, 4].
 * @param onToggleBypass     Callback to toggle the global bypass state.
 * @param modifier           Modifier for the outer container.
 */
@Composable
fun MeteringStrip(
    inputLevel: Float,
    outputLevel: Float,
    spectrumData: FloatArray,
    inputGain: Float,
    outputGain: Float,
    globalBypass: Boolean,
    onInputGainChange: (Float) -> Unit,
    onOutputGainChange: (Float) -> Unit,
    onToggleBypass: () -> Unit,
    modifier: Modifier = Modifier
) {
    var expanded by remember { mutableStateOf(false) }

    // Pre-compute panel border colors outside the draw lambda
    val panelShadow = DesignSystem.PanelShadow
    val panelHighlight = DesignSystem.PanelHighlight
    val panelBorderWidth = DesignSystem.PanelBorderWidth
    val metalGradient = remember { DesignSystem.brushedMetalGradient() }

    Column(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(topStart = 12.dp, topEnd = 12.dp))
            .background(metalGradient)
            .drawBehind {
                // Double-line machined panel border around the entire strip
                drawMeteringPanelBorder(panelShadow, panelHighlight, panelBorderWidth.toPx())
            }
    ) {
        // -- Collapsed row (always visible, 52dp) --
        CollapsedMeterRow(
            inputLevel = inputLevel,
            outputLevel = outputLevel,
            spectrumData = spectrumData,
            onClick = { expanded = !expanded }
        )

        // -- Expanded section (animated slide) --
        AnimatedVisibility(
            visible = expanded,
            enter = expandVertically(
                animationSpec = spring(dampingRatio = 0.78f, stiffness = 300f),
                expandFrom = Alignment.Top
            ),
            exit = shrinkVertically(
                animationSpec = spring(dampingRatio = 0.92f, stiffness = 1000f),
                shrinkTowards = Alignment.Top
            )
        ) {
            ExpandedSection(
                spectrumData = spectrumData,
                inputGain = inputGain,
                outputGain = outputGain,
                globalBypass = globalBypass,
                onInputGainChange = onInputGainChange,
                onOutputGainChange = onOutputGainChange,
                onToggleBypass = onToggleBypass
            )
        }
    }
}

// ── Panel Border Drawing ─────────────────────────────────────────────────────

/**
 * Draw a double-line machined panel border around the metering strip.
 *
 * Outer line: [panelShadow] (dark), Inner line: [panelHighlight] (light).
 * Creates the illusion of a beveled metal panel edge, consistent with the
 * RackModule panel border style.
 *
 * @param panelShadow  Outer border color.
 * @param panelHighlight Inner border color.
 * @param borderWidth  Pixel width of each border line.
 */
private fun DrawScope.drawMeteringPanelBorder(
    panelShadow: Color,
    panelHighlight: Color,
    borderWidth: Float
) {
    val w = size.width
    val h = size.height

    // Outer border: PanelShadow
    drawRect(color = panelShadow, topLeft = Offset.Zero, size = Size(w, borderWidth))
    drawRect(color = panelShadow, topLeft = Offset(0f, h - borderWidth), size = Size(w, borderWidth))
    drawRect(color = panelShadow, topLeft = Offset.Zero, size = Size(borderWidth, h))
    drawRect(color = panelShadow, topLeft = Offset(w - borderWidth, 0f), size = Size(borderWidth, h))

    // Inner border: PanelHighlight (inset by one borderWidth)
    val inset = borderWidth
    drawRect(color = panelHighlight, topLeft = Offset(inset, inset), size = Size(w - inset * 2, borderWidth))
    drawRect(color = panelHighlight, topLeft = Offset(inset, h - inset - borderWidth), size = Size(w - inset * 2, borderWidth))
    drawRect(color = panelHighlight, topLeft = Offset(inset, inset), size = Size(borderWidth, h - inset * 2))
    drawRect(color = panelHighlight, topLeft = Offset(w - inset - borderWidth, inset), size = Size(borderWidth, h - inset * 2))
}

// ── Collapsed Row ────────────────────────────────────────────────────────────

/**
 * Compact horizontal metering row displayed in the collapsed state.
 *
 * Layout: IN label + dBFS | input meter bar | mini spectrum | output meter bar | OUT label + dBFS
 *
 * The top border is drawn with [drawBehind] as a 1dp line in [DesignSystem.Divider].
 *
 * @param inputLevel  Current input signal level in [0.0, 1.0].
 * @param outputLevel Current output signal level in [0.0, 1.0].
 * @param spectrumData Array of 64 dB-scaled magnitude values.
 * @param onClick     Callback when the row is tapped (toggles expand/collapse).
 */
@Composable
private fun CollapsedMeterRow(
    inputLevel: Float,
    outputLevel: Float,
    spectrumData: FloatArray,
    onClick: () -> Unit
) {
    val dividerColor = DesignSystem.Divider

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(52.dp)
            .drawBehind {
                // Top border: 1dp divider line
                drawLine(
                    color = dividerColor,
                    start = Offset(0f, 0f),
                    end = Offset(size.width, 0f),
                    strokeWidth = 1.dp.toPx()
                )
            }
            .clickable(onClick = onClick)
            .padding(horizontal = 12.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        // 1. Input label + dBFS
        LevelLabel(label = "IN", level = inputLevel)

        Spacer(modifier = Modifier.width(6.dp))

        // 2. Input horizontal meter bar (with VU glow)
        HorizontalMeterBar(
            level = inputLevel,
            modifier = Modifier.weight(1f)
        )

        Spacer(modifier = Modifier.width(4.dp))

        // 3. Mini spectrum display (center, wider)
        MiniSpectrumDisplay(
            spectrumData = spectrumData,
            modifier = Modifier
                .weight(1.5f)
                .padding(horizontal = 4.dp)
        )

        Spacer(modifier = Modifier.width(4.dp))

        // 4. Output horizontal meter bar (with VU glow)
        HorizontalMeterBar(
            level = outputLevel,
            modifier = Modifier.weight(1f)
        )

        Spacer(modifier = Modifier.width(6.dp))

        // 5. Output label + dBFS
        LevelLabel(label = "OUT", level = outputLevel)
    }
}

// ── Level Label ──────────────────────────────────────────────────────────────

/**
 * Compact column showing a signal label ("IN" or "OUT") and its current
 * level in dBFS.
 *
 * @param label Label text (e.g., "IN", "OUT").
 * @param level Signal level in [0.0, 1.0].
 */
@Composable
private fun LevelLabel(
    label: String,
    level: Float
) {
    val dbfs = levelToDbfs(level)
    val displayText = formatDbfs(dbfs, level)

    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Text(
            text = label,
            fontSize = 9.sp,
            color = DesignSystem.TextMuted,
            fontWeight = FontWeight.Bold,
            maxLines = 1
        )
        Text(
            text = displayText,
            fontSize = 11.sp,
            color = DesignSystem.TextPrimary,
            fontFamily = FontFamily.Monospace,
            maxLines = 1
        )
    }
}

// ── Horizontal Meter Bar ─────────────────────────────────────────────────────

/**
 * Horizontal level meter bar drawn on a Canvas with VU-meter amber backlight.
 *
 * The bar fills left-to-right based on the signal level, with color thresholds
 * matching [LevelMeter]: green below 0.6, yellow from 0.6 to 0.85, red above 0.85.
 *
 * A subtle warm amber rectangle ([DesignSystem.VUAmber] at 8% alpha) is drawn
 * behind the meter bar to simulate the backlit VU meter aesthetic of real
 * studio rack gear.
 *
 * Uses [animateFloatAsState] with a 50ms tween for smooth metering that tracks
 * the audio level without jitter.
 *
 * @param level Signal level in [0.0, 1.0].
 * @param modifier Modifier for sizing. The bar uses full width and 8dp height.
 */
@Composable
private fun HorizontalMeterBar(
    level: Float,
    modifier: Modifier = Modifier
) {
    val animatedLevel by animateFloatAsState(
        targetValue = level.coerceIn(0f, 1f),
        animationSpec = tween(durationMillis = 50),
        label = "horizontalMeterLevel"
    )

    val meterColor = meterColorForLevel(animatedLevel)
    val bgColor = DesignSystem.MeterBg
    val vuAmber = DesignSystem.VUAmber
    val cornerRadiusDp = 4.dp

    Canvas(
        modifier = modifier
            .height(8.dp)
    ) {
        val cornerRadius = CornerRadius(cornerRadiusDp.toPx(), cornerRadiusDp.toPx())

        // VU amber backlight glow behind the meter (warm backlit VU aesthetic)
        drawRoundRect(
            color = vuAmber.copy(alpha = 0.08f),
            topLeft = Offset.Zero,
            size = Size(size.width, size.height),
            cornerRadius = cornerRadius
        )

        // Background track
        drawRoundRect(
            color = bgColor,
            topLeft = Offset.Zero,
            size = Size(size.width, size.height),
            cornerRadius = cornerRadius
        )

        // Filled portion (left to right)
        if (animatedLevel > 0f) {
            val fillWidth = animatedLevel * size.width
            drawRoundRect(
                color = meterColor,
                topLeft = Offset.Zero,
                size = Size(fillWidth, size.height),
                cornerRadius = cornerRadius
            )
        }
    }
}

// ── Expanded Section ─────────────────────────────────────────────────────────

/**
 * Expanded content displayed below the collapsed row when the strip is open.
 *
 * Contains:
 * 1. A full-size spectrum analyzer (80dp height) with dB reference lines
 * 2. A master controls row with input/output gain knobs and a bypass stomp switch
 *
 * An embossed groove divider separates the collapsed row from the expanded
 * content, consistent with the hardware panel aesthetic.
 *
 * @param spectrumData Array of 64 dB-scaled magnitude values.
 * @param inputGain  Master input gain in [0, 4].
 * @param outputGain Master output gain in [0, 4].
 * @param globalBypass Whether the signal chain is bypassed.
 * @param onInputGainChange  Callback for input gain changes.
 * @param onOutputGainChange Callback for output gain changes.
 * @param onToggleBypass     Callback to toggle bypass.
 */
@Composable
private fun ExpandedSection(
    spectrumData: FloatArray,
    inputGain: Float,
    outputGain: Float,
    globalBypass: Boolean,
    onInputGainChange: (Float) -> Unit,
    onOutputGainChange: (Float) -> Unit,
    onToggleBypass: () -> Unit
) {
    // Pre-compute groove divider colors
    val (grooveShadow, grooveHighlight) = remember { DesignSystem.grooveDividerColors() }

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 12.dp)
            .padding(bottom = 12.dp)
    ) {
        // Embossed groove divider between collapsed row and expanded content
        Spacer(
            modifier = Modifier
                .fillMaxWidth()
                .height(2.dp)
                .drawBehind {
                    drawRect(
                        color = grooveShadow,
                        topLeft = Offset.Zero,
                        size = Size(size.width, 1f)
                    )
                    drawRect(
                        color = grooveHighlight,
                        topLeft = Offset(0f, 1f),
                        size = Size(size.width, 1f)
                    )
                }
                .padding(bottom = 8.dp)
        )

        Spacer(modifier = Modifier.height(6.dp))

        // 1. Full-size spectrum bars (80dp, with reference lines)
        FullSpectrumBars(
            spectrumData = spectrumData,
            modifier = Modifier
                .fillMaxWidth()
                .height(80.dp)
                .clip(RoundedCornerShape(4.dp))
                .background(DesignSystem.MeterBg)
                .padding(horizontal = 4.dp, vertical = 2.dp)
        )

        Spacer(modifier = Modifier.height(12.dp))

        // 2. Master controls row: IN knob | BYPASS switch | OUT knob
        MasterControlsRow(
            inputGain = inputGain,
            outputGain = outputGain,
            globalBypass = globalBypass,
            onInputGainChange = onInputGainChange,
            onOutputGainChange = onOutputGainChange,
            onToggleBypass = onToggleBypass
        )
    }
}

// ── Full Spectrum Bars ───────────────────────────────────────────────────────

/**
 * Full-size spectrum bar chart for the expanded metering strip.
 *
 * Renders 64 logarithmically-spaced frequency bins as vertical bars with
 * dB reference lines at -12dB, -24dB, and -48dB. Bar colors follow the
 * same green/yellow/red scheme as [SpectrumAnalyzerDisplay].
 *
 * This is a local implementation rather than reusing [SpectrumAnalyzerDisplay]
 * (which has its own expand/collapse behavior) or the private [SpectrumBars]
 * composable. The rendering logic is identical to SpectrumBars with
 * reference lines enabled.
 *
 * @param spectrumData Array of 64 dB-scaled magnitude values in [-90, 0].
 * @param modifier Modifier for sizing and layout.
 */
@Composable
private fun FullSpectrumBars(
    spectrumData: FloatArray,
    modifier: Modifier = Modifier
) {
    val dbRange = SPECTRUM_MAX_DB - SPECTRUM_MIN_DB
    val dividerColor = DesignSystem.Divider

    Canvas(modifier = modifier) {
        val canvasWidth = size.width
        val canvasHeight = size.height
        val barWidth = canvasWidth / SPECTRUM_NUM_BINS
        val gap = 1f // 1px gap between bars

        // Draw each frequency bin as a vertical bar
        for (i in 0 until SPECTRUM_NUM_BINS.coerceAtMost(spectrumData.size)) {
            val db = spectrumData[i].coerceIn(SPECTRUM_MIN_DB, SPECTRUM_MAX_DB)
            val normalizedLevel = (db - SPECTRUM_MIN_DB) / dbRange
            val barHeight = normalizedLevel * canvasHeight

            // Color based on magnitude level (matches SpectrumBars thresholds)
            val color = when {
                db > -6f -> DesignSystem.MeterRed
                db > -18f -> DesignSystem.MeterYellow
                db > -36f -> DesignSystem.MeterGreen
                else -> DesignSystem.MeterDarkGreen // Dark green for quiet bins
            }

            val x = i * barWidth + gap
            val effectiveBarWidth = (barWidth - gap * 2).coerceAtLeast(1f)

            drawRect(
                color = color,
                topLeft = Offset(x, canvasHeight - barHeight),
                size = Size(effectiveBarWidth, barHeight)
            )
        }

        // dB reference lines at -12dB, -24dB, -48dB
        for (refDb in listOf(-12f, -24f, -48f)) {
            val y = canvasHeight * (1f - (refDb - SPECTRUM_MIN_DB) / dbRange)
            drawLine(
                color = dividerColor,
                start = Offset(0f, y),
                end = Offset(canvasWidth, y),
                strokeWidth = 1f
            )
        }
    }
}

// ── Master Controls Row ──────────────────────────────────────────────────────

/**
 * Row containing input/output gain knobs and the global bypass stomp switch.
 *
 * Layout: [IN GAIN knob] --- [BYPASS stomp switch] --- [OUT GAIN knob]
 *
 * Gain normalization: The knob operates on a 0..1 scale; the actual gain
 * range is 0..4 (linear). Conversion: `knobValue = gain / MAX_GAIN`.
 *
 * @param inputGain  Master input gain in [0, 4].
 * @param outputGain Master output gain in [0, 4].
 * @param globalBypass Whether the signal chain is bypassed.
 * @param onInputGainChange  Callback for input gain changes (value in [0, 4]).
 * @param onOutputGainChange Callback for output gain changes (value in [0, 4]).
 * @param onToggleBypass     Callback to toggle bypass.
 */
@Composable
private fun MasterControlsRow(
    inputGain: Float,
    outputGain: Float,
    globalBypass: Boolean,
    onInputGainChange: (Float) -> Unit,
    onOutputGainChange: (Float) -> Unit,
    onToggleBypass: () -> Unit
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceEvenly,
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Input gain knob
        RotaryKnob(
            value = inputGain / MAX_GAIN,
            onValueChange = { knobValue ->
                onInputGainChange(knobValue * MAX_GAIN)
            },
            label = "IN",
            displayValue = formatGainDb(inputGain),
            accentColor = DesignSystem.SafeGreen,
            size = KnobSize.STANDARD
        )

        // Global bypass stomp switch
        // Active (not bypassed) shows green glow; bypassed = no glow
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            StompSwitch(
                active = !globalBypass,
                categoryColor = DesignSystem.SafeGreen,
                onClick = onToggleBypass,
                size = 48.dp
            )
            Spacer(modifier = Modifier.height(3.dp))
            Text(
                text = if (globalBypass) "BYPASS" else "ACTIVE",
                fontSize = 9.sp,
                color = if (globalBypass) DesignSystem.ClipRed else DesignSystem.SafeGreen,
                fontWeight = FontWeight.Bold,
                letterSpacing = 1.sp,
                maxLines = 1
            )
        }

        // Output gain knob
        RotaryKnob(
            value = outputGain / MAX_GAIN,
            onValueChange = { knobValue ->
                onOutputGainChange(knobValue * MAX_GAIN)
            },
            label = "OUT",
            displayValue = formatGainDb(outputGain),
            accentColor = DesignSystem.SafeGreen,
            size = KnobSize.STANDARD
        )
    }
}

// ── Utility Functions ────────────────────────────────────────────────────────

/**
 * Convert a linear signal level to dBFS.
 *
 * @param level Signal level in [0.0, 1.0].
 * @return dBFS value, clamped to [MIN_DB, 0]. Returns [MIN_DB] for silence.
 */
private fun levelToDbfs(level: Float): Float {
    return if (level > SILENCE_THRESHOLD) {
        (20f * log10(level)).coerceAtLeast(MIN_DB)
    } else {
        MIN_DB
    }
}

/**
 * Format a dBFS value for display.
 *
 * @param dbfs   The dBFS value.
 * @param level  The original linear level (used to detect silence).
 * @return Formatted string: "-\u221E" for silence, or a rounded integer dB value.
 */
private fun formatDbfs(dbfs: Float, level: Float): String {
    return if (level <= SILENCE_THRESHOLD) {
        "-\u221E"
    } else {
        String.format("%.0f", dbfs)
    }
}

/**
 * Format a linear gain value as a dB string for display on knobs.
 *
 * @param gain Linear gain in [0, 4].
 * @return Formatted string: "-\u221E dB" for silence, or dB value with one decimal.
 */
private fun formatGainDb(gain: Float): String {
    return if (gain < SILENCE_THRESHOLD) {
        "-\u221E dB"
    } else {
        String.format("%.1f dB", 20f * log10(gain))
    }
}

/**
 * Determine the meter bar color based on signal level.
 *
 * Thresholds match [LevelMeter] for visual consistency:
 * - Green: level <= 0.6
 * - Yellow: 0.6 < level <= 0.85
 * - Red: level > 0.85
 *
 * @param level Signal level in [0.0, 1.0].
 * @return The appropriate [Color] from [DesignSystem].
 */
private fun meterColorForLevel(level: Float): Color {
    return when {
        level > 0.85f -> DesignSystem.MeterRed
        level > 0.6f -> DesignSystem.MeterYellow
        else -> DesignSystem.MeterGreen
    }
}
