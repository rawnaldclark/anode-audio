package com.guitaremulator.app.ui.components

import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.ui.theme.DesignSystem
import com.guitaremulator.app.viewmodel.TunerData
import kotlin.math.abs
import kotlin.math.log2
import kotlin.math.roundToInt

// -- Color aliases from DesignSystem ------------------------------------------

private val BgDefault = DesignSystem.TunerBgDefault
private val BgInTune = DesignSystem.TunerBgInTune
private val LedOff = DesignSystem.TunerLedOff
private val LedCenterDim = DesignSystem.TunerLedCenterDim
private val LedGreen = DesignSystem.TunerLedGreen
private val LedYellow = DesignSystem.TunerLedYellow
private val LedRed = DesignSystem.TunerLedRed
private val InTuneTextColor = DesignSystem.TunerInTuneText
private val DirectionTextColor = DesignSystem.TunerDirectionText
private val FreqTextColor = DesignSystem.TunerFreqText

// -- Constants ----------------------------------------------------------------

/** Total number of LED segments in the meter bar (Boss TU-3 style). */
private const val LED_COUNT = 21

/** Center segment index (0-based). */
private const val LED_CENTER = 10

/** Cents threshold for "in tune" state. */
private const val IN_TUNE_THRESHOLD = 5f

/** Minimum confidence to consider the reading valid. */
private const val MIN_CONFIDENCE = 0.2f

/**
 * Boss TU-3 / Korg Pitchblack style chromatic tuner display.
 *
 * Vertical layout with:
 *   1. Direction indicator (FLAT / IN TUNE / SHARP)
 *   2. 21-segment LED meter bar (Canvas-drawn)
 *   3. Large note name with octave number
 *   4. Detected frequency readout
 *
 * The background animates to a subtle green when the pitch is within
 * [IN_TUNE_THRESHOLD] cents of the target. The entire display dims
 * when confidence is below [MIN_CONFIDENCE].
 *
 * @param tunerData Current tuner readings from the native engine.
 * @param modifier  Modifier for layout customization.
 */
@Composable
fun TunerDisplay(
    tunerData: TunerData,
    modifier: Modifier = Modifier
) {
    // -- Derived state --------------------------------------------------------

    val clamped = tunerData.centsDeviation.coerceIn(-50f, 50f)
    val isConfident = tunerData.confidence > MIN_CONFIDENCE
    val isInTune = isConfident && abs(clamped) <= IN_TUNE_THRESHOLD

    // -- Animations -----------------------------------------------------------

    /** Smooth needle movement — spring animation converges asymptotically
     *  without restarting when the target changes mid-flight (unlike tween). */
    val animatedCents by animateFloatAsState(
        targetValue = clamped,
        animationSpec = spring(
            dampingRatio = Spring.DampingRatioNoBouncy,
            stiffness = 80f
        ),
        label = "centsAnimation"
    )

    /** Background color transitions over 200 ms when entering/leaving tune. */
    val bgColor by animateColorAsState(
        targetValue = if (isInTune) BgInTune else BgDefault,
        animationSpec = tween(durationMillis = 200),
        label = "bgColorAnimation"
    )

    /** Overall display opacity: dimmed when confidence is low. */
    val displayAlpha = if (isConfident) 1f else 0.35f

    // -- Octave calculation ---------------------------------------------------

    val octaveText = if (isConfident && tunerData.frequency > 0f) {
        // MIDI note number: middle A4 = 440 Hz = MIDI 69
        val midiNote = (12.0 * log2(tunerData.frequency.toDouble() / 440.0) + 69.0)
            .roundToInt()
        val octave = midiNote / 12 - 1
        octave.toString()
    } else {
        ""
    }

    // -- Layout ---------------------------------------------------------------

    Box(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(bgColor)
            .padding(horizontal = 16.dp, vertical = 20.dp)
            .alpha(displayAlpha)
    ) {
        Column(
            modifier = Modifier.fillMaxWidth(),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            // 1. Direction indicator
            DirectionIndicator(
                cents = animatedCents,
                isConfident = isConfident,
                isInTune = isInTune
            )

            Spacer(modifier = Modifier.height(14.dp))

            // 2. LED meter bar
            LedMeter(
                cents = animatedCents,
                isConfident = isConfident,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(32.dp)
            )

            Spacer(modifier = Modifier.height(16.dp))

            // 3. Large note name + octave
            Row(
                verticalAlignment = Alignment.Bottom,
                horizontalArrangement = Arrangement.Center
            ) {
                Text(
                    text = tunerData.noteName,
                    fontSize = 64.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace,
                    color = Color.White,
                    textAlign = TextAlign.Center
                )
                if (octaveText.isNotEmpty()) {
                    Text(
                        text = octaveText,
                        fontSize = 28.sp,
                        fontWeight = FontWeight.Normal,
                        fontFamily = FontFamily.Monospace,
                        color = DesignSystem.TunerOctaveText,
                        // Offset down so it sits at the baseline of the note name
                        modifier = Modifier.padding(bottom = 8.dp)
                    )
                }
            }

            Spacer(modifier = Modifier.height(6.dp))

            // 4. Frequency readout
            Text(
                text = if (tunerData.frequency > 0f && isConfident) {
                    String.format("%.1f Hz", tunerData.frequency)
                } else {
                    "-- Hz"
                },
                fontSize = 14.sp,
                color = FreqTextColor,
                fontFamily = FontFamily.Monospace,
                textAlign = TextAlign.Center
            )
        }
    }
}

// -- Sub-components -----------------------------------------------------------

/**
 * Direction label shown above the LED meter.
 *
 * Displays one of three states:
 *   - "< FLAT"   when cents < -[IN_TUNE_THRESHOLD]
 *   - "IN TUNE"  when within +/-[IN_TUNE_THRESHOLD]
 *   - "SHARP >"  when cents > +[IN_TUNE_THRESHOLD]
 *
 * Hidden when confidence is below [MIN_CONFIDENCE].
 *
 * @param cents      Animated cents deviation.
 * @param isConfident Whether the tuner has a reliable reading.
 * @param isInTune   Whether the pitch is within the in-tune threshold.
 */
@Composable
private fun DirectionIndicator(
    cents: Float,
    isConfident: Boolean,
    isInTune: Boolean
) {
    if (!isConfident) {
        // Reserve vertical space even when hidden to prevent layout jumps
        Spacer(modifier = Modifier.height(20.dp))
        return
    }

    val (text, color) = when {
        isInTune -> "IN TUNE" to InTuneTextColor
        cents < 0f -> "\u25C0  FLAT" to DirectionTextColor
        else -> "SHARP  \u25B6" to DirectionTextColor
    }

    Text(
        text = text,
        fontSize = 16.sp,
        fontWeight = FontWeight.Bold,
        fontFamily = FontFamily.Monospace,
        color = color,
        textAlign = TextAlign.Center,
        modifier = Modifier.height(20.dp)
    )
}

/**
 * 21-segment horizontal LED meter drawn with [Canvas].
 *
 * Each segment is a rounded rectangle with a 2 dp gap between neighbors.
 * The center segment (index 10) is taller than the rest to serve as a
 * visual target. The "needle" lights up 2-3 segments around the mapped
 * cents position.
 *
 * Segment color when lit depends on distance from center:
 *   - Green:  distance <= 1 segment  (near center)
 *   - Yellow: distance <= 4 segments (mid-range)
 *   - Red:    distance > 4 segments  (outer edges)
 *
 * When not lit, segments are dark ([LedOff]). The center segment when
 * not lit by the needle shows a dim green ([LedCenterDim]) as a target
 * reference, but only when confidence is sufficient.
 *
 * @param cents       Animated cents deviation (-50 to +50).
 * @param isConfident Whether the tuner has a reliable reading.
 * @param modifier    Modifier for sizing.
 */
@Composable
private fun LedMeter(
    cents: Float,
    isConfident: Boolean,
    modifier: Modifier = Modifier
) {
    Canvas(modifier = modifier) {
        val totalWidth = size.width
        val totalHeight = size.height

        // Gap between LED segments
        val gapPx = 2.dp.toPx()

        // Each segment width = (total - gaps) / count
        val segmentWidth = (totalWidth - gapPx * (LED_COUNT - 1)) / LED_COUNT
        val cornerRadius = CornerRadius(2.dp.toPx(), 2.dp.toPx())

        // Map cents (-50..+50) to needle position (0..20)
        val needlePos = cents / 50f * 10f + 10f

        for (i in 0 until LED_COUNT) {
            val x = i * (segmentWidth + gapPx)

            // Center segment is full height; others are 65%
            val isCenterSegment = (i == LED_CENTER)
            val segHeight = if (isCenterSegment) totalHeight else totalHeight * 0.65f
            val yOffset = (totalHeight - segHeight) / 2f

            // Distance from this segment to the needle position
            val distFromNeedle = abs(i.toFloat() - needlePos)

            // Distance from center (for color zone calculation)
            val distFromCenter = abs(i - LED_CENTER)

            // Determine if this segment is "lit" by the needle
            val isLit = isConfident && distFromNeedle < 1.5f

            val segColor = when {
                isLit -> {
                    // Color depends on how far from center this segment is
                    when {
                        distFromCenter <= 1 -> LedGreen
                        distFromCenter <= 4 -> LedYellow
                        else -> LedRed
                    }
                }
                // Center segment gets a dim green glow as the target marker
                isCenterSegment && isConfident -> LedCenterDim
                // All other unlit segments
                else -> LedOff
            }

            drawRoundRect(
                color = segColor,
                topLeft = Offset(x, yOffset),
                size = Size(segmentWidth, segHeight),
                cornerRadius = cornerRadius
            )
        }
    }
}
