package com.guitaremulator.app.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.PointerEventPass
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.input.pointer.positionChange
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.ui.theme.DesignSystem
import kotlin.math.abs
import kotlin.math.pow
import kotlin.math.roundToInt

// ── Layout Constants ─────────────────────────────────────────────────────────

/** Total height of a single vertical fader including labels. */
private val FADER_TOTAL_HEIGHT = 220.dp

/** Height of the canvas area for the fader track and thumb. */
private val FADER_TRACK_HEIGHT = 160.dp

/** Width of a single fader column (narrow to fit 11 across for 10-band + Level). */
private val FADER_COLUMN_WIDTH = 33.dp

/** Width of the recessed groove track. */
private val TRACK_WIDTH = 3.dp

/** Corner radius for the groove track ends. */
private val TRACK_CORNER_RADIUS = 1.5.dp

/** Diameter of the thumb knob. */
private val THUMB_DIAMETER = 16.dp

/** Width of the thumb grip rectangle (narrower to fit 11 faders). */
private val THUMB_GRIP_WIDTH = 20.dp

/** Height of the thumb grip rectangle. */
private val THUMB_GRIP_HEIGHT = 14.dp

/** Vertical padding at top and bottom of track (so thumb doesn't clip). */
private val TRACK_VERTICAL_PADDING = 12.dp

/** Height of the frequency response curve display. */
private val RESPONSE_CURVE_HEIGHT = 60.dp

/** Width of the center-line zero mark. */
private val CENTER_MARK_WIDTH = 16.dp

/** Number of dB tick marks on each side of center. */
private const val DB_TICK_COUNT = 3

// ── Data Classes ─────────────────────────────────────────────────────────────

/**
 * Describes one EQ band's fader configuration.
 *
 * @property label Frequency label shown below the fader (e.g., "200", "LVL").
 * @property gainParamId Parameter ID for the gain control (sent to C++ setParameter).
 * @property freqHz Center frequency in Hz (used for response curve). 0 for non-band faders (Level).
 * @property gainDb Current gain in dB (-15 to +15).
 * @property qValue Q factor for the band (0.9 for MXR M108S-style graphic EQ).
 * @property isLevel True for the output Level fader (drawn with a distinct accent color).
 */
data class EqBandConfig(
    val label: String,
    val gainParamId: Int,
    val freqHz: Float,
    val gainDb: Float,
    val qValue: Float = 0.9f,
    val isLevel: Boolean = false
)

// ── Pre-computed Pixel Dimensions ────────────────────────────────────────────

/**
 * Pre-computed pixel dimensions for the vertical fader layout.
 * Cached in a `remember` block keyed on density to avoid repeated dp-to-px conversion.
 */
private data class FaderLayoutPx(
    val trackWidth: Float,
    val trackCornerRadius: Float,
    val thumbRadius: Float,
    val thumbGripWidth: Float,
    val thumbGripHeight: Float,
    val trackVPadding: Float,
    val centerMarkWidth: Float,
    val innerShadowStroke: Float,
    val outerRingStroke: Float,
)

// ── VerticalEqFader Composable ──────────────────────────────────────────────

/**
 * A single vertical fader for an EQ band's gain control.
 *
 * Canvas-drawn with a recessed groove track, active fill from center (0dB),
 * and a metallic thumb knob. The fader operates in the -12dB to +12dB range
 * with center position representing unity gain (0dB).
 *
 * ## Drawing Order (bottom to top)
 * 1. dB tick marks along the track
 * 2. Recessed groove background with inner shadow
 * 3. Active fill from center to current thumb position
 * 4. Center (0dB) marker line
 * 5. Thumb drop shadow
 * 6. Thumb body (rounded rect with radial gradient)
 * 7. Thumb grip line (chrome highlight)
 *
 * ## Gesture Handling
 * Uses `awaitEachGesture` with `awaitFirstDown` for immediate response.
 * Vertical drag moves the thumb; horizontal motion is ignored to avoid
 * conflicting with parent scroll gestures.
 *
 * @param gainDb Current gain value in dB, clamped to [-maxDb, maxDb].
 * @param onGainChange Callback invoked with the new gain in dB during drag.
 * @param label Frequency label displayed below the fader (e.g., "800").
 * @param modifier Modifier applied to the outer Column.
 * @param maxDb Maximum dB range (symmetric). Default 12 for MXR M108S-style +-12dB.
 * @param enabled When false, the fader is drawn at 40% opacity and ignores gestures.
 * @param trackColor Color of the recessed groove background.
 * @param thumbColor Color of the thumb knob body.
 * @param fillColor Color of the active gain fill region.
 */
@Composable
fun VerticalEqFader(
    gainDb: Float,
    onGainChange: (Float) -> Unit,
    label: String,
    modifier: Modifier = Modifier,
    maxDb: Float = 12f,
    enabled: Boolean = true,
    trackColor: Color = DesignSystem.MeterBg,
    thumbColor: Color = DesignSystem.VUAmber,
    fillColor: Color = DesignSystem.MeterGreen
) {
    val density = LocalDensity.current

    // Pre-compute pixel dimensions once
    val layoutPx = remember(density) {
        with(density) {
            FaderLayoutPx(
                trackWidth = TRACK_WIDTH.toPx(),
                trackCornerRadius = TRACK_CORNER_RADIUS.toPx(),
                thumbRadius = THUMB_DIAMETER.toPx() / 2f,
                thumbGripWidth = THUMB_GRIP_WIDTH.toPx(),
                thumbGripHeight = THUMB_GRIP_HEIGHT.toPx(),
                trackVPadding = TRACK_VERTICAL_PADDING.toPx(),
                centerMarkWidth = CENTER_MARK_WIDTH.toPx(),
                innerShadowStroke = 1.dp.toPx(),
                outerRingStroke = 1.dp.toPx(),
            )
        }
    }

    // Pre-compute colors
    val fillColorAlpha = remember(fillColor) { fillColor.copy(alpha = 0.75f) }
    val fillGlowColor = remember(fillColor) { fillColor.copy(alpha = 0.20f) }
    val innerShadowColor = remember { DesignSystem.PanelShadow.copy(alpha = 0.40f) }
    val chromeDotColor = remember { DesignSystem.ChromeHighlight.copy(alpha = 0.80f) }
    val tickColor = remember { DesignSystem.TextMuted.copy(alpha = 0.5f) }
    val centerLineColor = remember { DesignSystem.CreamWhite.copy(alpha = 0.40f) }

    val currentGainDb by rememberUpdatedState(gainDb)
    val currentOnGainChange by rememberUpdatedState(onGainChange)

    // Format the dB readout text
    val dbText = remember(gainDb) {
        val rounded = (gainDb * 10f).roundToInt() / 10f
        val sign = if (rounded > 0f) "+" else ""
        "$sign${String.format("%.1f", rounded)}"
    }

    Column(
        modifier = modifier.width(FADER_COLUMN_WIDTH),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // dB readout above the fader
        Text(
            text = dbText,
            fontSize = 10.sp,
            fontFamily = FontFamily.Monospace,
            fontWeight = FontWeight.Medium,
            color = if (gainDb.roundToInt() == 0) {
                DesignSystem.TextSecondary
            } else {
                DesignSystem.TextValue
            },
            maxLines = 1
        )

        Spacer(modifier = Modifier.height(2.dp))

        // The fader canvas
        Canvas(
            modifier = Modifier
                .width(FADER_COLUMN_WIDTH)
                .height(FADER_TRACK_HEIGHT)
                .graphicsLayer {
                    alpha = if (enabled) 1f else 0.4f
                }
                .semantics {
                    this.contentDescription = "$label EQ gain"
                    this.stateDescription = "${dbText}dB"
                }
                .then(
                    if (enabled) {
                        Modifier.pointerInput(Unit) {
                            val trackHeight = size.height - layoutPx.trackVPadding * 2f

                            fun yToGainDb(y: Float): Float {
                                // Top of track = +maxDb, bottom = -maxDb
                                val fraction = ((y - layoutPx.trackVPadding) / trackHeight)
                                    .coerceIn(0f, 1f)
                                // Invert: top = max, bottom = min
                                return maxDb - fraction * 2f * maxDb
                            }

                            awaitEachGesture {
                                val down = awaitFirstDown(requireUnconsumed = false)
                                down.consume()

                                // Set value on initial tap
                                currentOnGainChange(yToGainDb(down.position.y))

                                val pointerId = down.id
                                while (true) {
                                    val event = awaitPointerEvent(PointerEventPass.Main)
                                    val change = event.changes.firstOrNull { it.id == pointerId }
                                        ?: break
                                    if (!change.pressed) break

                                    change.consume()
                                    currentOnGainChange(yToGainDb(change.position.y))
                                }
                            }
                        }
                    } else {
                        Modifier
                    }
                )
        ) {
            val canvasWidth = size.width
            val canvasHeight = size.height
            val centerX = canvasWidth / 2f
            val trackTop = layoutPx.trackVPadding
            val trackBottom = canvasHeight - layoutPx.trackVPadding
            val trackHeight = trackBottom - trackTop

            // Current value as fraction: 0 = +maxDb (top), 1 = -maxDb (bottom)
            val clampedDb = gainDb.coerceIn(-maxDb, maxDb)
            val fraction = (maxDb - clampedDb) / (2f * maxDb)
            val thumbCenterY = trackTop + fraction * trackHeight

            // Center line position (0dB = middle of track)
            val centerY = trackTop + trackHeight / 2f

            // -- Layer 1: dB tick marks -----------------------------------------------
            for (i in 0..DB_TICK_COUNT * 2) {
                val tickFraction = i.toFloat() / (DB_TICK_COUNT * 2).toFloat()
                val tickY = trackTop + tickFraction * trackHeight
                val isMajor = (i == 0 || i == DB_TICK_COUNT || i == DB_TICK_COUNT * 2)
                val tickHalfWidth = if (isMajor) 8.dp.toPx() else 5.dp.toPx()

                drawLine(
                    color = tickColor,
                    start = Offset(centerX - tickHalfWidth, tickY),
                    end = Offset(centerX + tickHalfWidth, tickY),
                    strokeWidth = if (isMajor) 1.dp.toPx() else 0.5.dp.toPx()
                )
            }

            // -- Layer 2: Recessed groove background ----------------------------------
            val grooveLeft = centerX - layoutPx.trackWidth / 2f
            drawRoundRect(
                color = trackColor,
                topLeft = Offset(grooveLeft, trackTop),
                size = Size(layoutPx.trackWidth, trackHeight),
                cornerRadius = CornerRadius(layoutPx.trackCornerRadius)
            )

            // Inner shadow on left edge of groove
            drawLine(
                color = innerShadowColor,
                start = Offset(grooveLeft + layoutPx.innerShadowStroke / 2f, trackTop + layoutPx.trackCornerRadius),
                end = Offset(grooveLeft + layoutPx.innerShadowStroke / 2f, trackBottom - layoutPx.trackCornerRadius),
                strokeWidth = layoutPx.innerShadowStroke
            )

            // -- Layer 3: Active fill from center (0dB) to thumb position -------------
            if (abs(clampedDb) > 0.1f) {
                val fillTop: Float
                val fillBottom: Float
                if (clampedDb > 0f) {
                    // Boosting: fill from thumb up to center
                    fillTop = thumbCenterY
                    fillBottom = centerY
                } else {
                    // Cutting: fill from center down to thumb
                    fillTop = centerY
                    fillBottom = thumbCenterY
                }

                // Glow behind the fill (wider, transparent)
                drawRoundRect(
                    color = fillGlowColor,
                    topLeft = Offset(centerX - layoutPx.trackWidth, fillTop),
                    size = Size(layoutPx.trackWidth * 2f, fillBottom - fillTop),
                    cornerRadius = CornerRadius(layoutPx.trackCornerRadius)
                )

                // Solid fill
                drawRoundRect(
                    color = fillColorAlpha,
                    topLeft = Offset(grooveLeft, fillTop),
                    size = Size(layoutPx.trackWidth, fillBottom - fillTop),
                    cornerRadius = CornerRadius(layoutPx.trackCornerRadius)
                )
            }

            // -- Layer 4: Center (0dB) marker line ------------------------------------
            drawLine(
                color = centerLineColor,
                start = Offset(centerX - layoutPx.centerMarkWidth / 2f, centerY),
                end = Offset(centerX + layoutPx.centerMarkWidth / 2f, centerY),
                strokeWidth = 1.dp.toPx(),
                cap = StrokeCap.Round
            )

            // -- Layer 5: Thumb drop shadow -------------------------------------------
            drawRoundRect(
                color = DesignSystem.PanelShadow.copy(alpha = 0.45f),
                topLeft = Offset(
                    centerX - layoutPx.thumbGripWidth / 2f - 1.dp.toPx(),
                    thumbCenterY - layoutPx.thumbGripHeight / 2f + 2.dp.toPx()
                ),
                size = Size(
                    layoutPx.thumbGripWidth + 2.dp.toPx(),
                    layoutPx.thumbGripHeight + 2.dp.toPx()
                ),
                cornerRadius = CornerRadius(3.dp.toPx())
            )

            // -- Layer 6: Thumb body (rounded rect with gradient) ---------------------
            val thumbLeft = centerX - layoutPx.thumbGripWidth / 2f
            val thumbTop = thumbCenterY - layoutPx.thumbGripHeight / 2f

            // Outer ring
            drawRoundRect(
                color = DesignSystem.KnobOuterRing,
                topLeft = Offset(thumbLeft, thumbTop),
                size = Size(layoutPx.thumbGripWidth, layoutPx.thumbGripHeight),
                cornerRadius = CornerRadius(3.dp.toPx()),
                style = Stroke(width = layoutPx.outerRingStroke)
            )

            // Gradient body
            val gradientCenter = Offset(
                centerX - layoutPx.thumbGripWidth * 0.15f,
                thumbCenterY - layoutPx.thumbGripHeight * 0.20f
            )
            drawRoundRect(
                brush = Brush.radialGradient(
                    colors = listOf(DesignSystem.KnobHighlight, DesignSystem.KnobBody),
                    center = gradientCenter,
                    radius = layoutPx.thumbGripWidth * 0.8f
                ),
                topLeft = Offset(
                    thumbLeft + layoutPx.outerRingStroke,
                    thumbTop + layoutPx.outerRingStroke
                ),
                size = Size(
                    layoutPx.thumbGripWidth - layoutPx.outerRingStroke * 2f,
                    layoutPx.thumbGripHeight - layoutPx.outerRingStroke * 2f
                ),
                cornerRadius = CornerRadius(2.dp.toPx())
            )

            // -- Layer 7: Thumb center groove line (channel indicator) ----------------
            drawLine(
                color = thumbColor,
                start = Offset(centerX - 6.dp.toPx(), thumbCenterY),
                end = Offset(centerX + 6.dp.toPx(), thumbCenterY),
                strokeWidth = 1.5.dp.toPx(),
                cap = StrokeCap.Round
            )

            // Small chrome highlight dot on thumb
            drawCircle(
                color = chromeDotColor,
                radius = 1.5.dp.toPx(),
                center = Offset(
                    centerX - layoutPx.thumbGripWidth * 0.25f,
                    thumbCenterY - layoutPx.thumbGripHeight * 0.15f
                )
            )
        }

        Spacer(modifier = Modifier.height(4.dp))

        // Frequency label below the fader
        Text(
            text = label,
            fontSize = 10.sp,
            fontFamily = FontFamily.Monospace,
            fontWeight = FontWeight.Medium,
            color = DesignSystem.TextSecondary,
            maxLines = 1
        )
    }
}

// ── EQ Frequency Response Curve ─────────────────────────────────────────────

/**
 * Draws a simplified frequency response curve showing the combined
 * EQ shape across all bands.
 *
 * The curve is computed by summing the gain contributions of each band
 * at logarithmically-spaced frequency points. This gives visual feedback
 * of the overall EQ shape as the user adjusts faders.
 *
 * @param bands List of [EqBandConfig] describing each band's frequency, gain, and Q.
 * @param modifier Modifier applied to the Canvas.
 */
@Composable
fun EqResponseCurve(
    bands: List<EqBandConfig>,
    modifier: Modifier = Modifier
) {
    val curveColor = remember { DesignSystem.MeterGreen.copy(alpha = 0.70f) }
    val curveGlowColor = remember { DesignSystem.MeterGreen.copy(alpha = 0.15f) }
    val gridColor = remember { DesignSystem.TextMuted.copy(alpha = 0.20f) }
    val zeroLineColor = remember { DesignSystem.CreamWhite.copy(alpha = 0.25f) }
    val bgColor = remember { DesignSystem.MeterBg.copy(alpha = 0.50f) }

    // Pre-compute the response curve points (64 frequency points, log-spaced 20Hz-20kHz)
    // Only include actual frequency bands (skip Level fader which has freqHz=0)
    val responsePoints = remember(bands) {
        val numPoints = 64
        val minFreqLog = kotlin.math.ln(20f)
        val maxFreqLog = kotlin.math.ln(20000f)
        // Filter to only bands with a real frequency (exclude Level fader)
        val freqBands = bands.filter { it.freqHz > 0f }
        // Find the Level fader's gain to add as a flat offset
        val levelOffsetDb = bands.firstOrNull { it.isLevel }?.gainDb ?: 0f

        FloatArray(numPoints) { i ->
            val freqLog = minFreqLog + (maxFreqLog - minFreqLog) * i.toFloat() / (numPoints - 1).toFloat()
            val freq = kotlin.math.exp(freqLog)

            // Sum gain contributions from each band using a 2nd-order peaking EQ approximation
            var totalGainDb = levelOffsetDb
            for (band in freqBands) {
                val ratio = freq / band.freqHz
                val logRatio = kotlin.math.ln(ratio)
                // Gaussian-like approximation of peaking EQ response in log-frequency domain
                // Width inversely proportional to Q
                val bandwidth = 1f / band.qValue
                val response = band.gainDb * kotlin.math.exp(-(logRatio * logRatio) / (2f * bandwidth * bandwidth)).toFloat()
                totalGainDb += response
            }
            totalGainDb.coerceIn(-16f, 16f)
        }
    }

    Canvas(
        modifier = modifier
            .fillMaxWidth()
            .height(RESPONSE_CURVE_HEIGHT)
    ) {
        val w = size.width
        val h = size.height
        val midY = h / 2f

        // Background
        drawRoundRect(
            color = bgColor,
            topLeft = Offset.Zero,
            size = size,
            cornerRadius = CornerRadius(4.dp.toPx())
        )

        // Horizontal grid lines at +6, 0, -6 dB
        for (db in listOf(-6f, 0f, 6f)) {
            val y = midY - (db / 16f) * (h / 2f)
            val color = if (db == 0f) zeroLineColor else gridColor
            drawLine(
                color = color,
                start = Offset(0f, y),
                end = Offset(w, y),
                strokeWidth = if (db == 0f) 1.dp.toPx() else 0.5.dp.toPx()
            )
        }

        // Draw the response curve
        if (responsePoints.isNotEmpty()) {
            val path = Path()
            val glowPath = Path()

            for (i in responsePoints.indices) {
                val x = w * i.toFloat() / (responsePoints.size - 1).toFloat()
                val gainDb = responsePoints[i]
                val y = midY - (gainDb / 16f) * (h / 2f)

                if (i == 0) {
                    path.moveTo(x, y)
                    glowPath.moveTo(x, y)
                } else {
                    path.lineTo(x, y)
                    glowPath.lineTo(x, y)
                }
            }

            // Glow (wider, more transparent)
            drawPath(
                path = glowPath,
                color = curveGlowColor,
                style = Stroke(width = 6.dp.toPx(), cap = StrokeCap.Round)
            )

            // Main curve line
            drawPath(
                path = path,
                color = curveColor,
                style = Stroke(width = 2.dp.toPx(), cap = StrokeCap.Round)
            )
        }

        // Vertical frequency markers at band center frequencies (skip Level)
        for (band in bands.filter { it.freqHz > 0f }) {
            val freqLog = kotlin.math.ln(band.freqHz)
            val minFreqLog = kotlin.math.ln(20f)
            val maxFreqLog = kotlin.math.ln(20000f)
            val x = w * (freqLog - minFreqLog) / (maxFreqLog - minFreqLog)

            drawLine(
                color = DesignSystem.VUAmber.copy(alpha = 0.25f),
                start = Offset(x, 0f),
                end = Offset(x, h),
                strokeWidth = 1.dp.toPx()
            )
        }
    }
}

// ── EqFaderStrip Composable ─────────────────────────────────────────────────

/**
 * Horizontal strip of vertical EQ faders arranged side by side, with an
 * optional frequency response curve visualization above.
 *
 * This composable is designed to replace the default rotary knob grid for
 * the Parametric EQ effect (index 7) in the RackModule's expanded content.
 * It displays one fader per EQ band, each controlling the band's gain
 * parameter (-12dB to +12dB).
 *
 * The strip maps fader positions directly to EQ gain parameter IDs via
 * the [onGainChange] callback, which the caller routes to the C++ engine's
 * setParameter API through the ViewModel.
 *
 * @param bands List of [EqBandConfig] describing each band (label, param ID, freq, gain, Q).
 * @param onGainChange Callback invoked with (paramId, newGainDb) when a fader moves.
 * @param modifier Modifier applied to the outer Column.
 * @param enabled When false, all faders are disabled (40% opacity, no gestures).
 * @param showResponseCurve Whether to show the frequency response curve above the faders.
 */
@Composable
fun EqFaderStrip(
    bands: List<EqBandConfig>,
    onGainChange: (paramId: Int, gainDb: Float) -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    showResponseCurve: Boolean = true
) {
    Column(
        modifier = modifier.fillMaxWidth(),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // Frequency response curve visualization
        if (showResponseCurve) {
            EqResponseCurve(
                bands = bands,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 8.dp)
            )

            Spacer(modifier = Modifier.height(8.dp))
        }

        // Row of vertical faders
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceEvenly,
            verticalAlignment = Alignment.Top
        ) {
            for (band in bands) {
                VerticalEqFader(
                    gainDb = band.gainDb,
                    onGainChange = { newDb ->
                        onGainChange(band.gainParamId, newDb)
                    },
                    label = band.label,
                    maxDb = 12f,
                    enabled = enabled,
                    // Level fader uses amber accent to distinguish from band faders
                    fillColor = if (band.isLevel) DesignSystem.VUAmber else DesignSystem.MeterGreen,
                    thumbColor = if (band.isLevel) DesignSystem.CreamWhite else DesignSystem.VUAmber
                )
            }
        }
    }
}

// ── Utility: Format Frequency Label ─────────────────────────────────────────

/**
 * Format a frequency value into a compact, human-readable label.
 *
 * Frequencies >= 1000Hz are displayed in kHz with one decimal place
 * (e.g., "3.2kHz"), while lower frequencies use integer Hz (e.g., "200Hz").
 *
 * @param freqHz Frequency in Hertz.
 * @return Formatted frequency string.
 */
fun formatFrequencyLabel(freqHz: Float): String {
    return if (freqHz >= 1000f) {
        val kHz = freqHz / 1000f
        if (kHz == kHz.roundToInt().toFloat()) {
            "${kHz.roundToInt()}kHz"
        } else {
            "${String.format("%.1f", kHz)}kHz"
        }
    } else {
        "${freqHz.roundToInt()}Hz"
    }
}
