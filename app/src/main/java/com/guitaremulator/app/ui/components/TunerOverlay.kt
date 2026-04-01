package com.guitaremulator.app.ui.components

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.ui.theme.DesignSystem
import com.guitaremulator.app.viewmodel.TunerData
import kotlin.math.abs

// -- Constants ----------------------------------------------------------------

/** Minimum confidence to consider the pitch detection reliable. */
private const val MIN_CONFIDENCE = 0.1f

/** Cents threshold for "in tune" (needle turns green). */
private const val IN_TUNE_CENTS = 5f

/** Cents threshold for "almost in tune" (needle turns amber). */
private const val ALMOST_CENTS = 15f

/** Cents threshold for the status text "almost" label. */
private const val ALMOST_STATUS_CENTS = 10f

/**
 * Full-screen semi-transparent tuner overlay.
 *
 * Slides down from the top of the screen and overlays on top of the existing
 * pedalboard rack UI. The backdrop is partially transparent so the rack remains
 * visible underneath. The tuner panel itself occupies approximately the top
 * 320dp of the screen.
 *
 * The overlay displays:
 * 1. A header row with "TUNER" label and close button.
 * 2. A large note name (e.g. "A", "Eb") with monospace font.
 * 3. A custom Canvas-drawn cents deviation bar with an animated indicator needle.
 * 4. The detected frequency readout in Hz.
 * 5. A tuning status text ("In Tune", "Almost", or direction hint).
 *
 * The note name and deviation data come from the native C++ tuner effect
 * (signal chain index 14) via the [TunerData] model.
 *
 * @param visible   Whether the overlay is shown. Controls [AnimatedVisibility].
 * @param tunerData Current tuner readings from the native engine.
 * @param onDismiss Callback invoked when the user dismisses the overlay
 *                  (via backdrop tap, close button, or back gesture).
 * @param modifier  Optional [Modifier] applied to the outermost container.
 */
@Composable
fun TunerOverlay(
    visible: Boolean,
    tunerData: TunerData,
    onDismiss: () -> Unit,
    modifier: Modifier = Modifier
) {
    // Intercept the system back gesture/button when the overlay is showing
    BackHandler(enabled = visible) { onDismiss() }

    AnimatedVisibility(
        visible = visible,
        enter = fadeIn(tween(200)) + slideInVertically(tween(250)) { -it / 3 },
        exit = fadeOut(tween(150)) + slideOutVertically(tween(200)) { -it / 3 },
        modifier = modifier
    ) {
        // Full-screen container: backdrop + panel layered via Box
        Box(modifier = Modifier.fillMaxSize()) {
            // 1. Semi-transparent backdrop -- tapping anywhere outside the panel dismisses
            Backdrop(onDismiss = onDismiss)

            // 2. Tuner panel anchored to the top
            TunerPanel(
                tunerData = tunerData,
                onDismiss = onDismiss,
                modifier = Modifier.align(Alignment.TopCenter)
            )
        }
    }
}

// -- Private sub-components ---------------------------------------------------

/**
 * Full-screen semi-transparent backdrop that dismisses the overlay on tap.
 *
 * Uses a [MutableInteractionSource] with no indication to suppress the default
 * ripple effect, since the entire backdrop is the click target.
 */
@Composable
private fun Backdrop(onDismiss: () -> Unit) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(DesignSystem.Background.copy(alpha = 0.85f))
            .clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = null,
                onClick = onDismiss
            )
    )
}

/**
 * The main tuner panel that slides down from the top of the screen.
 *
 * Contains the header, note display, cents deviation bar, frequency readout,
 * and tuning status text. Drawn with a bottom border in [DesignSystem.SafeGreen]
 * and rounded bottom corners.
 *
 * @param tunerData Current tuner readings.
 * @param onDismiss Callback for the close button.
 * @param modifier  Layout modifier (should include top alignment).
 */
@Composable
private fun TunerPanel(
    tunerData: TunerData,
    onDismiss: () -> Unit,
    modifier: Modifier = Modifier
) {
    val isDetected = tunerData.confidence >= MIN_CONFIDENCE && tunerData.noteName != "--"
    val clampedCents = tunerData.centsDeviation.coerceIn(-50f, 50f)
    val absCents = abs(clampedCents)

    // Bottom border color for the panel
    val borderColor = DesignSystem.SafeGreen

    Column(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(bottomStart = 16.dp, bottomEnd = 16.dp))
            .background(DesignSystem.ModuleSurface.copy(alpha = 0.95f))
            .drawBehind {
                val w = size.width
                val h = size.height

                // -- Amp-panel double-line border (machined panel edge) --
                val outerStroke = DesignSystem.PanelBorderWidth.toPx()
                drawRect(
                    color = DesignSystem.PanelShadow,
                    topLeft = Offset(0f, 0f),
                    size = size,
                    style = androidx.compose.ui.graphics.drawscope.Stroke(outerStroke)
                )
                val innerInset = outerStroke
                val innerStroke = 1.dp.toPx()
                drawRect(
                    color = DesignSystem.PanelHighlight,
                    topLeft = Offset(innerInset, innerInset),
                    size = Size(w - innerInset * 2f, h - innerInset * 2f),
                    style = androidx.compose.ui.graphics.drawscope.Stroke(innerStroke)
                )

                // -- Bottom border: GoldPiping at 60% + PanelShadow shadow --
                val goldStroke = 2.dp.toPx()
                val shadowStroke = 1.dp.toPx()
                drawLine(
                    color = DesignSystem.PanelShadow,
                    start = Offset(0f, h - shadowStroke / 2f),
                    end = Offset(w, h - shadowStroke / 2f),
                    strokeWidth = shadowStroke
                )
                drawLine(
                    color = DesignSystem.GoldPiping.copy(alpha = 0.6f),
                    start = Offset(0f, h - shadowStroke - goldStroke / 2f),
                    end = Offset(w, h - shadowStroke - goldStroke / 2f),
                    strokeWidth = goldStroke
                )
            }
            .padding(bottom = 24.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // a. Header row
        TunerHeader(onDismiss = onDismiss)

        Spacer(modifier = Modifier.height(8.dp))

        // b. Large note name in recessed panel
        Box(
            modifier = Modifier
                .padding(horizontal = 40.dp)
                .fillMaxWidth()
                .clip(RoundedCornerShape(8.dp))
                .background(DesignSystem.AmpChassisGray)
                .drawBehind {
                    // Recessed double-line border (inset panel look)
                    val outerStroke = DesignSystem.PanelBorderWidth.toPx()
                    drawRect(
                        color = DesignSystem.PanelShadow,
                        topLeft = Offset(0f, 0f),
                        size = size,
                        style = androidx.compose.ui.graphics.drawscope.Stroke(outerStroke)
                    )
                    val innerInset = outerStroke
                    val innerStroke = 1.dp.toPx()
                    drawRect(
                        color = DesignSystem.PanelHighlight,
                        topLeft = Offset(innerInset, innerInset),
                        size = Size(
                            size.width - innerInset * 2f,
                            size.height - innerInset * 2f
                        ),
                        style = androidx.compose.ui.graphics.drawscope.Stroke(innerStroke)
                    )
                }
                .padding(vertical = 12.dp),
            contentAlignment = Alignment.Center
        ) {
            NoteDisplay(
                noteName = tunerData.noteName,
                isDetected = isDetected
            )
        }

        Spacer(modifier = Modifier.height(16.dp))

        // c. Cents deviation bar (custom Canvas)
        CentsDeviationBar(
            centsDeviation = if (isDetected) clampedCents else 0f,
            isDetected = isDetected,
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp)
                .height(48.dp)
        )

        Spacer(modifier = Modifier.height(12.dp))

        // d. Frequency readout
        FrequencyReadout(
            frequency = tunerData.frequency,
            isDetected = isDetected
        )

        Spacer(modifier = Modifier.height(8.dp))

        // e. Tuning status text
        TuningStatusText(
            centsDeviation = clampedCents,
            isDetected = isDetected
        )
    }
}

/**
 * Header row with "TUNER" label on the left and a close button on the right.
 */
@Composable
private fun TunerHeader(onDismiss: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(48.dp)
            .padding(horizontal = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Text(
            text = "TUNER",
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            letterSpacing = 2.sp,
            color = DesignSystem.CreamWhite
        )

        IconButton(
            onClick = onDismiss,
            modifier = Modifier.size(36.dp)
        ) {
            Icon(
                imageVector = Icons.Default.Close,
                contentDescription = "Close tuner",
                tint = DesignSystem.TextSecondary,
                modifier = Modifier.size(20.dp)
            )
        }
    }
}

/**
 * Large centered note name display.
 *
 * Shows the detected note (e.g. "A", "Eb") in a prominent 64sp monospace font.
 * When no pitch is detected, shows "--" in muted color.
 *
 * @param noteName   The note name string from [TunerData].
 * @param isDetected Whether a reliable pitch has been detected.
 */
@Composable
private fun NoteDisplay(
    noteName: String,
    isDetected: Boolean
) {
    Text(
        text = if (isDetected) noteName else "--",
        fontSize = 64.sp,
        fontWeight = FontWeight.Bold,
        fontFamily = FontFamily.Monospace,
        color = if (isDetected) DesignSystem.TextPrimary else DesignSystem.TextMuted,
        textAlign = TextAlign.Center
    )
}

/**
 * Custom Canvas-drawn cents deviation indicator bar.
 *
 * Draws a horizontal meter bar with:
 * - A rounded-rect background in [DesignSystem.MeterBg].
 * - Tick marks at -50, -25, 0, +25, and +50 cents.
 * - A prominent center zero line.
 * - Labels ("-50", "0", "+50") below the tick marks.
 * - An animated filled circle indicator ("needle") whose position maps
 *   from the -50..+50 cent range to the bar width, and whose color
 *   reflects how close the pitch is to in-tune.
 *
 * The needle position is smoothly animated using [animateFloatAsState]
 * with a 100ms tween for responsive but non-jittery movement.
 *
 * @param centsDeviation Clamped cents deviation (-50 to +50). Zero when not detected.
 * @param isDetected     Whether a reliable pitch has been detected.
 * @param modifier       Sizing modifier (should include explicit height).
 */
@Composable
private fun CentsDeviationBar(
    centsDeviation: Float,
    isDetected: Boolean,
    modifier: Modifier = Modifier
) {
    // Animate needle position for smooth movement between readings
    val animatedCents by animateFloatAsState(
        targetValue = centsDeviation,
        animationSpec = tween(durationMillis = 100),
        label = "needleCentsAnimation"
    )

    // Determine needle color based on deviation magnitude
    val needleColor = when {
        !isDetected -> Color.Transparent
        abs(animatedCents) < IN_TUNE_CENTS -> DesignSystem.SafeGreen
        abs(animatedCents) < ALMOST_CENTS -> DesignSystem.WarningAmber
        else -> DesignSystem.ClipRed
    }

    val textMeasurer = rememberTextMeasurer()

    // Pre-define text styles and colors outside the draw scope
    val labelStyle = TextStyle(fontSize = 10.sp, color = DesignSystem.TextMuted)
    val tickColor = DesignSystem.TextMuted
    val centerLineColor = DesignSystem.TextMuted
    val meterBgColor = DesignSystem.MeterBg

    Canvas(modifier = modifier) {
        val canvasWidth = size.width
        val canvasHeight = size.height

        // The bar occupies the upper portion; labels sit below
        val labelAreaHeight = 14.dp.toPx()
        val barHeight = canvasHeight - labelAreaHeight
        val barTop = 0f
        val cornerRadiusPx = 6.dp.toPx()

        // Draw rounded-rect background for the meter bar
        drawRoundRect(
            color = meterBgColor,
            topLeft = Offset(0f, barTop),
            size = Size(canvasWidth, barHeight),
            cornerRadius = CornerRadius(cornerRadiusPx, cornerRadiusPx)
        )

        // -- Tick marks at -50, -25, 0, +25, +50 cents --
        // Map cents to X: -50 -> 0, 0 -> center, +50 -> canvasWidth
        val tickCentsValues = listOf(-50f, -25f, 0f, 25f, 50f)
        val tickHeightShort = 8.dp.toPx()
        val tickHeightCenter = 14.dp.toPx()
        val tickStrokeWidth = 1.dp.toPx()

        for (cents in tickCentsValues) {
            val x = centsToX(cents, canvasWidth)
            val isCenter = cents == 0f
            val tickHeight = if (isCenter) tickHeightCenter else tickHeightShort
            val tickTop = (barHeight - tickHeight) / 2f

            drawLine(
                color = if (isCenter) centerLineColor else tickColor,
                start = Offset(x, barTop + tickTop),
                end = Offset(x, barTop + tickTop + tickHeight),
                strokeWidth = tickStrokeWidth
            )
        }

        // -- Labels below the bar: "-50", "0", "+50" --
        val labelY = barTop + barHeight + 2.dp.toPx()
        val labelTexts = listOf("-50" to -50f, "0" to 0f, "+50" to 50f)

        for ((text, cents) in labelTexts) {
            val x = centsToX(cents, canvasWidth)
            val measured = textMeasurer.measure(text, labelStyle)
            val labelX = (x - measured.size.width / 2f).coerceIn(
                0f,
                canvasWidth - measured.size.width
            )
            drawText(
                textLayoutResult = measured,
                topLeft = Offset(labelX, labelY)
            )
        }

        // -- Animated indicator needle (filled circle) --
        if (isDetected) {
            val needleX = centsToX(animatedCents, canvasWidth)
            val needleCenterY = barTop + barHeight / 2f
            val needleRadius = 6.dp.toPx()

            drawCircle(
                color = needleColor,
                radius = needleRadius,
                center = Offset(needleX, needleCenterY)
            )
        }
    }
}

/**
 * Maps a cents deviation value (-50..+50) to an X coordinate within the given width.
 *
 * @param cents Value in [-50, 50].
 * @param width Total width of the drawing area.
 * @return X coordinate corresponding to the cents position.
 */
private fun centsToX(cents: Float, width: Float): Float {
    // Normalize: -50 -> 0.0, 0 -> 0.5, +50 -> 1.0
    val normalized = (cents + 50f) / 100f
    return normalized * width
}

/**
 * Frequency readout display.
 *
 * Shows the detected frequency formatted to one decimal place (e.g. "440.0 Hz").
 * When no pitch is detected, shows "-- Hz" in muted color.
 *
 * @param frequency  Detected frequency in Hz.
 * @param isDetected Whether a reliable pitch has been detected.
 */
@Composable
private fun FrequencyReadout(
    frequency: Float,
    isDetected: Boolean
) {
    Text(
        text = if (isDetected && frequency > 0f) {
            String.format("%.1f Hz", frequency)
        } else {
            "-- Hz"
        },
        fontSize = 16.sp,
        fontFamily = FontFamily.Monospace,
        color = if (isDetected) DesignSystem.CreamWhite else DesignSystem.TextMuted,
        textAlign = TextAlign.Center
    )
}

/**
 * Tuning status text that provides human-readable feedback.
 *
 * Displays one of four states:
 * - **"In Tune"** (green, bold) when |cents| < 2.
 * - **"Almost"** (amber) when |cents| < 10.
 * - **Direction hint** (red) showing a down arrow + "Flat" or up arrow + "Sharp"
 *   when |cents| >= 10.
 * - **Empty** when no pitch is detected.
 *
 * @param centsDeviation Clamped cents deviation (-50 to +50).
 * @param isDetected     Whether a reliable pitch has been detected.
 */
@Composable
private fun TuningStatusText(
    centsDeviation: Float,
    isDetected: Boolean
) {
    if (!isDetected) {
        // Reserve vertical space to prevent layout shifts
        Spacer(modifier = Modifier.height(20.dp))
        return
    }

    val absCents = abs(centsDeviation)

    val (statusText, statusColor, statusWeight) = when {
        absCents < 2f -> Triple("In Tune", DesignSystem.SafeGreen, FontWeight.Bold)
        absCents < ALMOST_STATUS_CENTS -> Triple("Almost", DesignSystem.WarningAmber, FontWeight.Normal)
        centsDeviation < 0f -> Triple("\u2193 Flat", DesignSystem.ClipRed, FontWeight.Normal)
        else -> Triple("\u2191 Sharp", DesignSystem.ClipRed, FontWeight.Normal)
    }

    Text(
        text = statusText,
        fontSize = 14.sp,
        fontWeight = statusWeight,
        color = statusColor,
        textAlign = TextAlign.Center
    )
}
