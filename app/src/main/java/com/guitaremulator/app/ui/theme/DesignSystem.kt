package com.guitaremulator.app.ui.theme

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import kotlin.random.Random

/**
 * Centralized design token system for the Guitar Emulator app.
 *
 * All colors, dimensions, and visual constants are defined here so that
 * UI components reference tokens instead of hardcoded hex values.
 * "Blackface Terminal" aesthetic: vintage Fender amp hardware meets
 * terminal/console minimalism.
 */
object DesignSystem {

    // ── Global Background & Surface ──────────────────────────────────

    val Background       = Color(0xFF141416)  // Near-black with slight blue undertone
    val ModuleSurface    = Color(0xFF222226)  // Base module surface before category tint
    val ModuleBorder     = Color(0xFF333338)  // Subtle border around modules
    val Divider          = Color(0xFF2E2E34)  // Section dividers within modules

    // ── Hardware-Authentic Surface Colors ────────────────────────────

    val AmpChassisGray       = Color(0xFF1E1E22)  // Brushed aluminum dark
    val ControlPanelSurface  = Color(0xFF252528)  // Amp face plate
    val CreamWhite           = Color(0xFFF5F0E0)  // Chicken-head knob / silk-screen
    val ChromeHighlight      = Color(0xFFC8C8D0)  // Metallic reflections
    val ToasterGrill         = Color(0xFF2A2A2E)  // Ventilation pattern color

    // ── Jewel Pilot Light Colors ────────────────────────────────────

    val JewelRed             = Color(0xFFE04040)  // Power / recording jewel
    val JewelGreen           = Color(0xFF40C848)  // Active / safe jewel
    val JewelAmber           = Color(0xFFE8B040)  // Warning jewel

    // ── Hardware Accent Colors ──────────────────────────────────────

    val VUAmber              = Color(0xFFD8A030)  // VU meter backlight
    val GoldPiping           = Color(0xFFC8A840)  // Marshall-style gold trim

    // ── Panel Chrome Colors ─────────────────────────────────────────

    val PanelHighlight       = Color(0xFF3A3A40)  // Inner highlight edge
    val PanelShadow          = Color(0xFF101014)  // Outer shadow edge
    val ScrewMetal           = Color(0xFF4A4A50)  // Decorative screw body
    val ScrewHighlight       = Color(0xFF606068)  // Screw highlight spot

    // ── Text Colors ──────────────────────────────────────────────────

    val TextPrimary      = Color(0xFFE0E0E4)  // Main labels, effect names
    val TextSecondary    = Color(0xFF888890)  // Parameter labels, descriptions
    val TextValue        = Color(0xFFD0D0D8)  // Parameter value readouts
    val TextMuted        = Color(0xFF555560)  // Disabled, placeholder text

    // ── Knob Colors ──────────────────────────────────────────────────

    val KnobBody         = Color(0xFF3C3C42)  // Knob face color
    val KnobOuterRing    = Color(0xFF2A2A30)  // Shadow ring around knob
    val KnobHighlight    = Color(0xFF4A4A52)  // Top highlight on knob surface
    val KnobPointer      = Color(0xFFF0F0F0)  // White pointer line on knob

    // ── Meter & Indicator Colors ─────────────────────────────────────

    val MeterBg          = Color(0xFF181820)  // Meter track background
    val MeterGreen       = Color(0xFF40A850)  // Normal level
    val MeterYellow      = Color(0xFFD8C030)  // Hot level
    val MeterRed         = Color(0xFFD83838)  // Clipping level
    val MeterPeakHold    = Color(0xFFE0E0E0)  // Peak hold line

    val MeterDarkGreen   = Color(0xFF1B4020)  // Quiet spectrum bins / below threshold
    val ClipRed          = Color(0xFFF04040)  // Clipping indicator
    val WarningAmber     = Color(0xFFE8A030)  // Approaching limit
    val DangerSurface    = Color(0xFF2A1A1A)  // Destructive action button background
    val DangerText       = Color(0xFFFF8A80)  // Destructive action button label
    val SafeGreen        = Color(0xFF40B858)  // Safe level / active state

    // ── Tuner Display Colors ──────────────────────────────────────────

    val TunerBgDefault       = Color(0xFF1A1A1A)  // Dark background when out of tune
    val TunerBgInTune        = Color(0xFF0D2E0D)  // Subtle green background when in tune
    val TunerLedOff          = Color(0xFF252525)  // Unlit LED segment
    val TunerLedCenterDim    = Color(0xFF1A3A1A)  // Dimly-lit center LED
    val TunerLedGreen        = Color(0xFF4CAF50)  // Lit green LEDs near center
    val TunerLedYellow       = Color(0xFFFFC107)  // Yellow LEDs mid-range
    val TunerLedRed          = Color(0xFFF44336)  // Red LEDs at outer edges
    val TunerInTuneText      = Color(0xFF4CAF50)  // Green "IN TUNE" label
    val TunerDirectionText   = Color(0xFFCCCCCC)  // FLAT/SHARP label gray
    val TunerFreqText        = Color(0xFF888888)  // Frequency readout muted gray
    val TunerOctaveText      = Color(0xFFAAAAAA)  // Octave number light gray

    // ── Gutter Rail Colors (Focus Mode) ──────────────────────────────

    val GutterSurface    = Color(0xFF1A1A1E)  // Slightly darker than ModuleSurface
    val GutterDivider    = Color(0xFF2A2A2E)  // Subtle bottom divider
    val BypassDotSize: Dp = 6.dp              // Category dot size on gutter rail
    val GutterHeight: Dp  = 44.dp             // Gutter rail height
    val BypassStripHeight: Dp = 44.dp         // Bypass strip height
    val BypassMarkerHeight: Dp = 4.dp         // Thin marker line for bypassed effects in Focus Mode

    // ── Dimensions ───────────────────────────────────────────────────

    val KnobSizeSmall: Dp    = 44.dp
    val KnobSizeStandard: Dp = 56.dp
    val KnobSizeLarge: Dp    = 68.dp

    val ModuleCollapsedHeight: Dp = 48.dp
    val LedSize: Dp              = 10.dp
    val StompSwitchSize: Dp      = 48.dp
    val AccentBarHeight: Dp      = 3.dp
    val ModuleCornerRadius: Dp   = 6.dp
    val ModuleElevation: Dp      = 2.dp

    // ── Skeuomorphic Dimensions ─────────────────────────────────────

    val KnobScaleRingPadding: Dp = 4.dp   // Gap between knob body and scale markings
    val JewelLedSize: Dp         = 12.dp  // Jewel pilot light (slightly larger than flat LED)
    val FootswitchSize: Dp       = 52.dp  // Enlarged stomp switch
    val PanelBorderWidth: Dp     = 1.5.dp // Amp panel border thickness
    val RackScrewSize: Dp        = 12.dp  // Decorative rack screw diameter
    val AccentBarThick: Dp       = 4.dp   // Thickened category accent bar

    // ── Effect Category System ───────────────────────────────────────

    /**
     * Visual category for grouping effects by color palette.
     *
     * Each category carries a complete color set so that modules, knobs,
     * LEDs, and accent bars automatically match the category's palette.
     */
    enum class EffectCategory(
        val primary: Color,
        val surfaceTint: Color,
        val knobAccent: Color,
        val ledActive: Color,
        val ledBypass: Color,
        val headerBar: Color
    ) {
        GAIN(
            primary     = Color(0xFFD4533B),
            surfaceTint = Color(0xFF2D1F1A),
            knobAccent  = Color(0xFFE8724A),
            ledActive   = Color(0xFFFF6B3D),
            ledBypass   = Color(0xFF3D2218),
            headerBar   = Color(0xFFC04030)
        ),
        AMP(
            primary     = Color(0xFFC8963C),
            surfaceTint = Color(0xFF2A2418),
            knobAccent  = Color(0xFFD4A84A),
            ledActive   = Color(0xFFF0C040),
            ledBypass   = Color(0xFF382E18),
            headerBar   = Color(0xFFB88830)
        ),
        TONE(
            primary     = Color(0xFF909098),
            surfaceTint = Color(0xFF242428),
            knobAccent  = Color(0xFFA8A8B0),
            ledActive   = Color(0xFFC0C0CC),
            ledBypass   = Color(0xFF2A2A30),
            headerBar   = Color(0xFF707078)
        ),
        MODULATION(
            primary     = Color(0xFF6B7EC8),
            surfaceTint = Color(0xFF1C1E2D),
            knobAccent  = Color(0xFF7B90E0),
            ledActive   = Color(0xFF6888F0),
            ledBypass   = Color(0xFF1E2038),
            headerBar   = Color(0xFF5868A8)
        ),
        TIME(
            primary     = Color(0xFF3CA8A0),
            surfaceTint = Color(0xFF182A28),
            knobAccent  = Color(0xFF50C8BE),
            ledActive   = Color(0xFF40D8C8),
            ledBypass   = Color(0xFF183028),
            headerBar   = Color(0xFF308880)
        ),
        UTILITY(
            primary     = Color(0xFF607088),
            surfaceTint = Color(0xFF1C2028),
            knobAccent  = Color(0xFF7890A8),
            ledActive   = Color(0xFF6898C0),
            ledBypass   = Color(0xFF1E2430),
            headerBar   = Color(0xFF506878)
        )
    }

    /**
     * Map a signal chain effect index (0..25) to its visual category.
     */
    fun getCategory(effectIndex: Int): EffectCategory = when (effectIndex) {
        0, 1, 3, 4, 16, 17, 18, 19,
        21, 22, 23, 24               -> EffectCategory.GAIN    // Fuzz/boost pedals
        5, 6                         -> EffectCategory.AMP
        7                            -> EffectCategory.TONE
        2, 8, 9, 10, 11, 15, 20, 25 -> EffectCategory.MODULATION // +RotarySpeaker
        12, 13                       -> EffectCategory.TIME
        14                           -> EffectCategory.UTILITY
        else                         -> EffectCategory.UTILITY
    }

    /**
     * Per-pedal header color overrides for effects that reference
     * their real-world hardware color (e.g. Boss DS-1 orange).
     *
     * Returns null for effects that use the default category header color.
     */
    fun getPedalHeaderColor(effectIndex: Int): Color? = when (effectIndex) {
        16 -> Color(0xFFD06020)  // Boss DS-1: orange
        17 -> Color(0xFF484848)  // ProCo RAT: charcoal
        18 -> Color(0xFFD07020)  // Boss DS-2: deep orange
        19 -> Color(0xFFC04830)  // Boss HM-2: burnt orange-red
        20 -> Color(0xFF5858A8)  // Univibe: deep purple-blue
        21 -> Color(0xFFD85050)  // Fuzz Face: germanium red
        22 -> Color(0xFFD8A840)  // Rangemaster: gold
        23 -> Color(0xFF484848)  // Big Muff: charcoal/black
        24 -> Color(0xFFC86040)  // Octavia: vintage orange
        25 -> Color(0xFF886838)  // Rotary Speaker: wood brown
        else -> null
    }

    /**
     * Get the resolved header bar color for an effect, preferring
     * pedal-specific overrides and falling back to category color.
     */
    fun getHeaderColor(effectIndex: Int): Color =
        getPedalHeaderColor(effectIndex) ?: getCategory(effectIndex).headerBar

    /**
     * Map a signal chain effect index to a Unicode symbol used as a
     * visual emblem in the rack module's collapsed header row.
     *
     * Each symbol is chosen to evoke the effect's character at a glance.
     * No bitmap resources are needed — these are rendered as plain text.
     */
    fun getEffectIcon(effectIndex: Int): String = when (effectIndex) {
        0  -> "\u2759"     // Noise Gate: medium vertical bar (❙ — gate)
        1  -> "\u25BC"     // Compressor: down-pointing triangle (▼ — squeeze)
        2  -> "\u223F"     // Wah: sine wave (∿ — swept filter)
        3  -> "\u25B2"     // Boost: up-pointing triangle (▲ — gain up)
        4  -> "\u2736"     // Overdrive: six-pointed star (✶ — saturation)
        5  -> "\u25AE"     // Amp Model: vertical rectangle (▮ — amp stack)
        6  -> "\u25A3"     // Cabinet Sim: square with inner (▣ — speaker)
        7  -> "\u2261"     // Parametric EQ: identical to (≡ — frequency bars)
        8  -> "\u224B"     // Chorus: triple tilde (≋ — shimmering)
        9  -> "\u2248"     // Vibrato: approximately equal (≈ — wobble)
        10 -> "\u00D8"     // Phaser: O-with-stroke (Ø — phase rotation)
        11 -> "\u2195"     // Flanger: up-down arrow (↕ — jet sweep)
        12 -> "\u2026"     // Delay: ellipsis (… — echoes)
        13 -> "\u2234"     // Reverb: therefore dots (∴ — reflections)
        14 -> "\u266A"     // Tuner: musical note (♪)
        15 -> "\u2194"     // Tremolo: left-right arrow (↔ — pulsing)
        16 -> "\u2666"     // DS-1: diamond suit (♦ — orange pedal)
        17 -> "\u25C6"     // RAT: black diamond (◆ — dark pedal)
        18 -> "\u25C8"     // DS-2: diamond-in-diamond (◈ — variant pedal)
        19 -> "\u2588"     // HM-2: full block (█ — heavy)
        20 -> "\u25CB"     // Univibe: white circle (○ — rotating speaker)
        21 -> "\u2605"     // Fuzz Face: star (★ — fuzz emblem)
        22 -> "\u2191"     // Rangemaster: up arrow (↑ — treble boost)
        23 -> "\u25CF"     // Big Muff: filled circle (● — sustain)
        24 -> "\u266B"     // Octavia: double note (♫ — octave up)
        25 -> "\u21BB"     // Rotary Speaker: clockwise arrow (↻ — rotation)
        else -> "\u2022"   // Fallback: bullet (•)
    }

    // ── Texture Helpers (Pure Compose, No Bitmaps) ──────────────────

    /**
     * Horizontal linear gradient simulating brushed aluminum.
     *
     * Use as a background brush for amp faceplate surfaces.
     */
    fun brushedMetalGradient(): Brush = Brush.horizontalGradient(
        colors = listOf(
            AmpChassisGray,
            ControlPanelSurface,
            AmpChassisGray.copy(alpha = 0.95f),
            ControlPanelSurface.copy(alpha = 0.98f),
            AmpChassisGray
        )
    )

    /**
     * Composable that draws a faint noise pattern overlay, simulating
     * the texture of tolex-covered amp enclosures.
     *
     * Place this as an overlay on background surfaces using Box.
     * The noise is deterministic (seeded random) so it doesn't flicker
     * on recomposition.
     *
     * @param alpha Overall opacity of the noise (default 0.03f for subtle effect).
     */
    @Composable
    fun TolexNoiseOverlay(alpha: Float = 0.03f) {
        // Pre-compute noise dots so they don't change on recomposition.
        // Reduced from 600 to 200 dots -- at 0.03 alpha with sub-pixel radii
        // the visual density is nearly identical, but draw cost drops 3x.
        val noiseDots = remember {
            val rng = Random(42)
            List(200) {
                Triple(rng.nextFloat(), rng.nextFloat(), rng.nextFloat())
            }
        }

        // Pre-compute the two dot colors outside the draw lambda to avoid
        // Color.copy() allocation on every frame.
        val dotColor = remember(alpha) { Color.White.copy(alpha = alpha) }
        val darkDotColor = remember(alpha) { Color.Black.copy(alpha = alpha * 1.5f) }

        Canvas(modifier = Modifier.fillMaxSize()) {
            val w = size.width
            val h = size.height

            for ((xFrac, yFrac, brightness) in noiseDots) {
                val color = if (brightness > 0.5f) dotColor else darkDotColor
                val radius = 0.5f + brightness * 0.8f
                drawCircle(
                    color = color,
                    radius = radius,
                    center = Offset(xFrac * w, yFrac * h)
                )
            }
        }
    }

    /**
     * Draw a double-line "machined panel edge" border.
     *
     * Inner line: [PanelHighlight] (light), Outer line: [PanelShadow] (dark).
     * Creates the illusion of a beveled metal panel edge.
     */
    fun panelEdgeColors(): Pair<Color, Color> = PanelHighlight to PanelShadow

    /**
     * Embossed groove divider colors (1px dark + 1px light below).
     */
    fun grooveDividerColors(): Pair<Color, Color> =
        PanelShadow to PanelHighlight.copy(alpha = 0.5f)
}

// ── DrawScope Extensions ────────────────────────────────────────────────────

/**
 * Draw the standard "machined panel edge" double-line border.
 *
 * Outer border: [DesignSystem.PanelShadow], inner border: [DesignSystem.PanelHighlight].
 * This creates the illusion of a beveled metal panel edge seen on real amp faceplates.
 *
 * Call from within a `drawBehind` or `Canvas` scope.
 */
fun DrawScope.drawPanelBorder() {
    val outerStroke = DesignSystem.PanelBorderWidth.toPx()
    drawRect(
        color = DesignSystem.PanelShadow,
        topLeft = Offset.Zero,
        size = size,
        style = Stroke(outerStroke)
    )
    val innerInset = outerStroke + 1f
    val innerStroke = 1.dp.toPx()
    drawRect(
        color = DesignSystem.PanelHighlight,
        topLeft = Offset(innerInset, innerInset),
        size = Size(
            (size.width - innerInset * 2f).coerceAtLeast(0f),
            (size.height - innerInset * 2f).coerceAtLeast(0f)
        ),
        style = Stroke(innerStroke)
    )
}

// ── Modifier Extensions ─────────────────────────────────────────────────────

/**
 * Modifier that draws the standard amp-panel double-line border behind the content.
 *
 * Convenience for `drawBehind { drawPanelBorder() }`.
 */
fun Modifier.panelBorder(): Modifier = this.drawBehind { drawPanelBorder() }
