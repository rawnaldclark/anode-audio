package com.guitaremulator.app.ui.components

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.compositeOver
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.data.EffectRegistry
import com.guitaremulator.app.ui.theme.DesignSystem
import com.guitaremulator.app.viewmodel.EffectUiState
import kotlin.math.roundToInt

// ── Constants ────────────────────────────────────────────────────────────────

/** Height of the always-visible collapsed header row. */
private val HEADER_HEIGHT = 52.dp

/** Width of the vertical category accent bar on the left edge (thickened for hardware look). */
private val ACCENT_BAR_WIDTH = DesignSystem.AccentBarThick

/** Index of the Tuner effect in the signal chain. */
private const val TUNER_INDEX = 14

/** Index of the Parametric EQ effect in the signal chain. */
private const val PARAMETRIC_EQ_INDEX = 7

/** Index of the Reverb effect in the signal chain. */
private const val REVERB_INDEX = 13

/** Effect indices that have advanced detail editors (AmpModel, CabinetSim, Delay). */
private val EFFECTS_WITH_ADVANCED_DETAIL = setOf(5, 6, 12)

/** Margin between rack screws and the panel edge. */
private val RACK_SCREW_MARGIN = 4.dp

/**
 * A collapsible effect module for the vertical rack layout.
 *
 * Displays each effect in the signal chain as a compact row that can be
 * expanded to reveal parameter controls (rotary knobs for continuous params,
 * sliders for enum params), a wet/dry mix slider, and a detail button.
 *
 * ## Visual Design ("Blackface Terminal" Hardware Aesthetic)
 * - Double-line machined panel border (outer shadow + inner highlight)
 * - Decorative rack screws in the collapsed header (3D radial gradient)
 * - Embossed groove dividers between parameter sections
 * - Monospace ALL-CAPS effect name with letterSpacing for silk-screen look
 * - Inner shadow at top of expanded content for recessed panel illusion
 *
 * ## Collapsed State (52dp height)
 * Shows the category accent bar, LED indicator, effect name, stomp switch,
 * and decorative rack screws at left/right edges.
 * Tapping the row body toggles expansion; the stomp switch toggles bypass.
 *
 * ## Expanded State (variable height)
 * Adds parameter controls, wet/dry mix slider, and a detail button below
 * the header row, wrapped in [AnimatedVisibility] for smooth transitions.
 *
 * @param effectState Current UI state of the effect (index, name, enabled, params).
 * @param expanded Whether the module is currently showing its parameter panel.
 * @param onToggleExpand Callback when the user taps the row body to expand/collapse.
 * @param onToggleEnabled Callback when the user presses the stomp switch to bypass/enable.
 * @param onParameterChange Callback when a parameter knob or enum slider changes value.
 * @param onWetDryMixChange Callback when the wet/dry mix slider changes.
 * @param onEditDetail Callback to open the full bottom-sheet editor for this effect.
 * @param modifier Outer modifier applied to the Surface container.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun RackModule(
    effectState: EffectUiState,
    expanded: Boolean,
    onToggleExpand: () -> Unit,
    onToggleEnabled: () -> Unit,
    onParameterChange: (paramId: Int, value: Float) -> Unit,
    onWetDryMixChange: (Float) -> Unit,
    onEditDetail: () -> Unit,
    modifier: Modifier = Modifier
) {
    val category = remember(effectState.index) {
        DesignSystem.getCategory(effectState.index)
    }
    val accentColor = remember(effectState.index) {
        DesignSystem.getHeaderColor(effectState.index)
    }

    // Compute the tinted surface color: category tint at 50% alpha over the base module surface
    val surfaceColor = remember(category) {
        category.surfaceTint.copy(alpha = 0.5f).compositeOver(DesignSystem.ModuleSurface)
    }

    // Pre-compute panel border colors outside the draw lambda
    val panelShadow = DesignSystem.PanelShadow
    val panelHighlight = DesignSystem.PanelHighlight
    val panelBorderWidthDp = DesignSystem.PanelBorderWidth

    Surface(
        modifier = modifier
            .fillMaxWidth()
            .padding(bottom = 4.dp),
        shape = RoundedCornerShape(DesignSystem.ModuleCornerRadius),
        color = surfaceColor,
        shadowElevation = DesignSystem.ModuleElevation
    ) {
        Column(
            modifier = Modifier.drawBehind {
                // Draw the vertical category accent bar on the left edge (thickened)
                drawRect(
                    color = accentColor,
                    topLeft = Offset.Zero,
                    size = Size(
                        width = ACCENT_BAR_WIDTH.toPx(),
                        height = size.height
                    )
                )

                // Double-line machined panel edge border
                drawPanelBorder(panelShadow, panelHighlight, panelBorderWidthDp.toPx())
            }
        ) {
            // ── Header Row (always visible, 52dp) ──────────────────────────
            HeaderRow(
                effectState = effectState,
                category = category,
                expanded = expanded,
                onToggleExpand = onToggleExpand,
                onToggleEnabled = onToggleEnabled
            )

            // ── Expanded Content (animated) ────────────────────────────────
            AnimatedVisibility(
                visible = expanded,
                enter = expandVertically(
                    animationSpec = spring(
                        dampingRatio = 0.72f,
                        stiffness = Spring.StiffnessMediumLow
                    ),
                    expandFrom = Alignment.Top
                ) + fadeIn(animationSpec = tween(durationMillis = 180, delayMillis = 40)),
                exit = shrinkVertically(
                    animationSpec = spring(
                        dampingRatio = 0.88f,
                        stiffness = Spring.StiffnessMedium
                    ),
                    shrinkTowards = Alignment.Top
                ) + fadeOut(animationSpec = tween(durationMillis = 120))
            ) {
                ExpandedContent(
                    effectState = effectState,
                    category = category,
                    onParameterChange = onParameterChange,
                    onWetDryMixChange = onWetDryMixChange,
                    onEditDetail = onEditDetail
                )
            }
        }
    }
}

// ── Panel Border Drawing ─────────────────────────────────────────────────────

/**
 * Draw a double-line "machined panel edge" border in the DrawScope.
 *
 * The outer line uses [panelShadow] (dark) and the inner line uses
 * [panelHighlight] (light), creating the illusion of a beveled metal edge
 * pressed into the chassis.
 *
 * @param panelShadow  Outer border color (dark shadow).
 * @param panelHighlight Inner border color (highlight).
 * @param borderWidth  Pixel width of each border line.
 */
private fun DrawScope.drawPanelBorder(
    panelShadow: Color,
    panelHighlight: Color,
    borderWidth: Float
) {
    val w = size.width
    val h = size.height

    // Outer border: PanelShadow
    // Top
    drawRect(
        color = panelShadow,
        topLeft = Offset.Zero,
        size = Size(w, borderWidth)
    )
    // Bottom
    drawRect(
        color = panelShadow,
        topLeft = Offset(0f, h - borderWidth),
        size = Size(w, borderWidth)
    )
    // Left
    drawRect(
        color = panelShadow,
        topLeft = Offset.Zero,
        size = Size(borderWidth, h)
    )
    // Right
    drawRect(
        color = panelShadow,
        topLeft = Offset(w - borderWidth, 0f),
        size = Size(borderWidth, h)
    )

    // Inner border: PanelHighlight (inset by one borderWidth)
    val inset = borderWidth
    // Top
    drawRect(
        color = panelHighlight,
        topLeft = Offset(inset, inset),
        size = Size(w - inset * 2, borderWidth)
    )
    // Bottom
    drawRect(
        color = panelHighlight,
        topLeft = Offset(inset, h - inset - borderWidth),
        size = Size(w - inset * 2, borderWidth)
    )
    // Left
    drawRect(
        color = panelHighlight,
        topLeft = Offset(inset, inset),
        size = Size(borderWidth, h - inset * 2)
    )
    // Right
    drawRect(
        color = panelHighlight,
        topLeft = Offset(w - inset - borderWidth, inset),
        size = Size(borderWidth, h - inset * 2)
    )
}

// ── Rack Screw Drawing ───────────────────────────────────────────────────────

/**
 * Draw a decorative rack screw at a given center point.
 *
 * Uses a radial gradient from [ScrewHighlight] (center) to [ScrewMetal] (edge)
 * for a 3D screw-head appearance, plus a small off-center white highlight dot
 * simulating metallic light reflection.
 *
 * @param center  Center position of the screw.
 * @param radius  Radius of the screw circle.
 */
/** Cached screw colors to avoid re-reading DesignSystem in hot draw path. */
private val SCREW_HIGHLIGHT_COLOR = DesignSystem.ScrewHighlight
private val SCREW_METAL_COLOR = DesignSystem.ScrewMetal
private val SCREW_DOT_COLOR = Color.White.copy(alpha = 0.20f)

private fun DrawScope.drawRackScrew(center: Offset, radius: Float) {
    // Use simple two-color circles instead of radialGradient to avoid
    // Brush allocation on every draw frame. The visual difference is
    // imperceptible at 12dp screw size.
    // Outer ring: darker metal
    drawCircle(
        color = SCREW_METAL_COLOR,
        radius = radius,
        center = center
    )
    // Inner highlight: lighter center
    drawCircle(
        color = SCREW_HIGHLIGHT_COLOR,
        radius = radius * 0.65f,
        center = center
    )

    // Small off-center metallic highlight dot (2dp ~ radius * 0.33)
    val highlightRadius = radius * 0.17f
    val highlightOffset = Offset(center.x - radius * 0.2f, center.y - radius * 0.25f)
    drawCircle(
        color = SCREW_DOT_COLOR,
        radius = highlightRadius,
        center = highlightOffset
    )
}

// ── Header Row ──────────────────────────────────────────────────────────────

/**
 * The always-visible collapsed header containing the LED, effect name,
 * stomp switch, and decorative rack screws at left/right edges.
 *
 * The Row itself is clickable for expand/collapse. The StompSwitch has
 * its own click handler that naturally consumes the touch event before
 * it reaches the Row's clickable modifier, preventing both from firing.
 *
 * Rack screws are drawn in [drawBehind] to avoid layout overhead. They are
 * positioned at the vertical center of the header, flush with the left
 * and right edges (4dp margin from panel edge).
 */
@Composable
private fun HeaderRow(
    effectState: EffectUiState,
    category: DesignSystem.EffectCategory,
    expanded: Boolean,
    onToggleExpand: () -> Unit,
    onToggleEnabled: () -> Unit
) {
    // Pre-compute screw dimensions outside the draw lambda
    val screwSizeDp = DesignSystem.RackScrewSize
    val screwMarginDp = RACK_SCREW_MARGIN

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(HEADER_HEIGHT)
            .drawBehind {
                // Draw decorative rack screws at left and right edges
                val screwRadius = screwSizeDp.toPx() / 2f
                val screwMarginPx = screwMarginDp.toPx()
                val centerY = size.height / 2f

                // Left screw
                drawRackScrew(
                    center = Offset(screwMarginPx + screwRadius, centerY),
                    radius = screwRadius
                )
                // Right screw
                drawRackScrew(
                    center = Offset(size.width - screwMarginPx - screwRadius, centerY),
                    radius = screwRadius
                )
            }
            .clickable(
                onClickLabel = if (expanded) "Collapse" else "Expand",
                onClick = onToggleExpand
            )
            .padding(start = 24.dp + ACCENT_BAR_WIDTH, end = 24.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        // LED indicator: lit when effect is enabled
        LedIndicator(
            active = effectState.enabled,
            activeColor = category.ledActive,
            inactiveColor = category.ledBypass,
            size = DesignSystem.LedSize
        )

        Spacer(modifier = Modifier.width(12.dp))

        // Effect category icon emblem
        Text(
            text = DesignSystem.getEffectIcon(effectState.index),
            fontSize = 16.sp,
            color = category.primary,
            maxLines = 1
        )

        Spacer(modifier = Modifier.width(8.dp))

        // Effect name: monospace, ALL CAPS, letterSpacing for silk-screen look
        Text(
            text = effectState.name.uppercase(),
            fontSize = 14.sp,
            fontWeight = FontWeight.Medium,
            fontFamily = FontFamily.Monospace,
            letterSpacing = 1.5.sp,
            color = if (effectState.enabled) {
                DesignSystem.TextPrimary
            } else {
                DesignSystem.TextMuted
            },
            maxLines = 1,
            modifier = Modifier.weight(1f)
        )

        // Animated expand/collapse chevron
        val chevronRotation by animateFloatAsState(
            targetValue = if (expanded) 90f else 0f,
            animationSpec = tween(durationMillis = 200),
            label = "chevronRotation"
        )

        Text(
            text = "\u25B8",
            fontSize = 12.sp,
            color = DesignSystem.CreamWhite.copy(alpha = 0.5f),
            modifier = Modifier
                .padding(end = 8.dp)
                .graphicsLayer { rotationZ = chevronRotation }
        )

        // Stomp switch: bypass toggle (consumes its own click, does NOT expand)
        StompSwitch(
            active = effectState.enabled,
            categoryColor = category.ledActive,
            onClick = onToggleEnabled,
            size = 44.dp
        )
    }
}

// ── Embossed Groove Divider ─────────────────────────────────────────────────

/**
 * Draw an embossed groove divider at the current vertical position in a Column.
 *
 * Two 1px lines (dark above, light below) create the illusion of a groove
 * pressed into metal. This replaces the flat [HorizontalDivider] for a
 * more authentic hardware look.
 */
@Composable
private fun EmbossedGrooveDivider() {
    val (shadowColor, highlightColor) = remember { DesignSystem.grooveDividerColors() }

    Spacer(
        modifier = Modifier
            .fillMaxWidth()
            .height(2.dp)
            .drawBehind {
                // Top line: dark shadow (groove shadow)
                drawRect(
                    color = shadowColor,
                    topLeft = Offset.Zero,
                    size = Size(size.width, 1f)
                )
                // Bottom line: light highlight at 50% alpha (groove catch light)
                drawRect(
                    color = highlightColor,
                    topLeft = Offset(0f, 1f),
                    size = Size(size.width, 1f)
                )
            }
    )
}

// ── Expanded Content ────────────────────────────────────────────────────────

/**
 * The expandable parameter panel shown below the header row.
 *
 * Content varies based on the effect:
 * - Tuner (index 14): placeholder text directing the user to the overlay.
 * - All others: parameter knobs/sliders, wet/dry mix, and detail button.
 *
 * The entire panel is dimmed to 70% opacity when the effect is bypassed,
 * providing a clear visual cue that the controls are inactive.
 *
 * A subtle inner shadow gradient at the top of the expanded area creates
 * the illusion of a recessed panel inset into the module chassis.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun ExpandedContent(
    effectState: EffectUiState,
    category: DesignSystem.EffectCategory,
    onParameterChange: (paramId: Int, value: Float) -> Unit,
    onWetDryMixChange: (Float) -> Unit,
    onEditDetail: () -> Unit
) {
    // Pre-compute the inner shadow brush for the expanded panel inset.
    // Caching the Brush avoids allocating a new verticalGradient on every draw frame.
    val innerShadowBrush = remember {
        Brush.verticalGradient(
            colors = listOf(
                DesignSystem.PanelShadow.copy(alpha = 0.3f),
                Color.Transparent
            )
        )
    }

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .drawBehind {
                // Expanded state panel inset: 4dp gradient from PanelShadow (alpha 0.3)
                // to transparent at the top of the expanded content area
                val shadowHeight = 4.dp.toPx()
                drawRect(
                    brush = innerShadowBrush,
                    topLeft = Offset.Zero,
                    size = Size(size.width, shadowHeight)
                )
            }
            .graphicsLayer {
                alpha = if (effectState.enabled) 1f else 0.7f
            }
            .padding(start = 12.dp + ACCENT_BAR_WIDTH, end = 12.dp)
    ) {
        Spacer(modifier = Modifier.height(8.dp))

        // Embossed groove divider instead of flat HorizontalDivider
        EmbossedGrooveDivider()

        Spacer(modifier = Modifier.height(12.dp))

        // Tuner special case: "Open Tuner" button opens the tuner overlay
        if (effectState.index == TUNER_INDEX) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 12.dp),
                contentAlignment = Alignment.Center
            ) {
                TextButton(onClick = onEditDetail) {
                    Text(
                        text = "Open Tuner",
                        fontSize = 13.sp,
                        fontWeight = FontWeight.Medium,
                        color = category.primary,
                        letterSpacing = 0.5.sp
                    )
                }
            }
        } else {
            // Look up the effect metadata from the registry
            val effectInfo = remember(effectState.index) {
                EffectRegistry.getEffectInfo(effectState.index)
            }

            if (effectInfo != null && !effectState.isReadOnly) {
                // For the Reverb effect (index 13), filter visible params
                // based on the current Mode value (Hall of Fame 2 layout):
                //   Digital (mode=0): Type, Decay, Tone
                //   Spring (mode=1,2): Dwell, Tone
                // Mode param (id=4) is always shown.
                // Wet/dry mix slider (shown separately) acts as the Level knob.
                val reverbMode = if (effectState.index == REVERB_INDEX) {
                    effectState.parameters[4]?.roundToInt() ?: 0
                } else -1

                val visibleParams = remember(effectInfo, reverbMode) {
                    if (effectState.index == REVERB_INDEX) {
                        val isDigital = reverbMode == 0
                        effectInfo.params.filter { p ->
                            when (p.id) {
                                4 -> true               // Mode: always visible
                                0 -> isDigital          // Decay: digital only
                                8 -> isDigital          // Type: digital only
                                6 -> true               // Tone: both modes
                                5 -> !isDigital         // Dwell: spring only
                                else -> false           // Hide internal params (1,2,3,7)
                            }
                        }
                    } else {
                        effectInfo.params
                    }
                }

                // Separate continuous params from enum params
                val continuousParams = remember(visibleParams) {
                    visibleParams.filter { it.valueLabels.isEmpty() }
                }
                val enumParams = remember(visibleParams) {
                    visibleParams.filter { it.valueLabels.isNotEmpty() }
                }

                // ── Graphic EQ special case: 7-band fader strip + Level ────
                // For the Graphic EQ (index 7), all 11 parameters are gain controls
                // displayed as vertical faders. No rotary knobs needed.
                if (effectState.index == PARAMETRIC_EQ_INDEX) {
                    // MXR M108S 10-band ISO octave center frequencies
                    val eqFreqs = remember {
                        floatArrayOf(31.25f, 62.5f, 125f, 250f, 500f, 1000f, 2000f, 4000f, 8000f, 16000f)
                    }
                    val eqLabels = remember {
                        arrayOf("31", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k", "LVL")
                    }

                    // Build band configurations for the fader strip (10 bands + Level)
                    val eqBands = remember(
                        effectState.parameters[0], effectState.parameters[1],
                        effectState.parameters[2], effectState.parameters[3],
                        effectState.parameters[4], effectState.parameters[5],
                        effectState.parameters[6], effectState.parameters[7],
                        effectState.parameters[8], effectState.parameters[9],
                        effectState.parameters[10]
                    ) {
                        List(11) { i ->
                            if (i < 10) {
                                EqBandConfig(
                                    label = eqLabels[i],
                                    gainParamId = i,
                                    freqHz = eqFreqs[i],
                                    gainDb = effectState.parameters[i] ?: 0f,
                                    qValue = 0.9f
                                )
                            } else {
                                // Level fader (param 10)
                                EqBandConfig(
                                    label = eqLabels[10],
                                    gainParamId = 10,
                                    freqHz = 0f, // Not a frequency band
                                    gainDb = effectState.parameters[10] ?: 0f,
                                    qValue = 0.9f,
                                    isLevel = true
                                )
                            }
                        }
                    }

                    // Fader strip with response curve
                    EqFaderStrip(
                        bands = eqBands,
                        onGainChange = { paramId, gainDb ->
                            onParameterChange(paramId, gainDb.coerceIn(-12f, 12f))
                        },
                        enabled = effectState.enabled,
                        showResponseCurve = true
                    )
                } else if (continuousParams.isNotEmpty()) {
                // ── Default: continuous parameters displayed as rotary knobs ──
                    // Use smaller knobs when there are many parameters
                    val knobSize = if (continuousParams.size <= 2) {
                        KnobSize.STANDARD
                    } else {
                        KnobSize.SMALL
                    }

                    FlowRow(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceEvenly,
                        maxItemsInEachRow = 3
                    ) {
                        for (paramInfo in continuousParams) {
                            val currentValue = effectState.parameters[paramInfo.id]
                                ?: paramInfo.defaultValue
                            val range = paramInfo.max - paramInfo.min

                            // Normalize the parameter value to 0..1 for the knob
                            val normalizedValue = if (range > 0f) {
                                ((currentValue - paramInfo.min) / range).coerceIn(0f, 1f)
                            } else {
                                0f
                            }

                            // Build the display string: formatted value + unit
                            val displayValue = buildString {
                                append(formatParamValue(currentValue, paramInfo.min, paramInfo.max))
                                if (paramInfo.unit.isNotEmpty()) {
                                    append(" ")
                                    append(paramInfo.unit)
                                }
                            }

                            RotaryKnob(
                                value = normalizedValue,
                                onValueChange = { normalized ->
                                    // Denormalize back to the parameter's actual range
                                    val denormalized = paramInfo.min + normalized * range
                                    onParameterChange(paramInfo.id, denormalized)
                                },
                                label = paramInfo.name,
                                displayValue = displayValue,
                                accentColor = category.knobAccent,
                                size = knobSize,
                                enabled = effectState.enabled,
                                steps = 0,
                                modifier = Modifier.padding(vertical = 4.dp)
                            )
                        }
                    }
                }

                // Enum parameters displayed as labeled sliders with snapping
                if (enumParams.isNotEmpty()) {
                    if (continuousParams.isNotEmpty()) {
                        Spacer(modifier = Modifier.height(8.dp))

                        // Embossed groove between continuous and enum sections
                        EmbossedGrooveDivider()

                        Spacer(modifier = Modifier.height(8.dp))
                    }

                    for (paramInfo in enumParams) {
                        val currentValue = effectState.parameters[paramInfo.id]
                            ?: paramInfo.defaultValue

                        EnumParamSlider(
                            paramInfo = paramInfo,
                            value = currentValue,
                            accentColor = category.knobAccent,
                            onValueChange = { newValue ->
                                onParameterChange(paramInfo.id, newValue)
                            }
                        )

                        Spacer(modifier = Modifier.height(4.dp))
                    }
                }
            }
        }

        // Wet/Dry mix slider (hidden for read-only effects like Tuner)
        if (!effectState.isReadOnly) {
            Spacer(modifier = Modifier.height(8.dp))

            // Embossed groove before wet/dry section
            EmbossedGrooveDivider()

            Spacer(modifier = Modifier.height(8.dp))

            WetDrySlider(
                value = effectState.wetDryMix,
                accentColor = category.knobAccent,
                onValueChange = onWetDryMixChange
            )
        }

        // Advanced detail button: only for effects with unique advanced features
        if (effectState.index in EFFECTS_WITH_ADVANCED_DETAIL) {
            Spacer(modifier = Modifier.height(8.dp))
            Box(
                modifier = Modifier.fillMaxWidth(),
                contentAlignment = Alignment.CenterEnd
            ) {
                TextButton(onClick = onEditDetail) {
                    Text(
                        text = "Advanced",
                        fontSize = 12.sp,
                        color = category.primary,
                        fontWeight = FontWeight.Medium,
                        letterSpacing = 0.5.sp
                    )
                }
            }
        }

        Spacer(modifier = Modifier.height(12.dp))
    }
}

// ── Enum Parameter Slider ───────────────────────────────────────────────────

/**
 * Compact labeled slider for enum parameters with discrete step snapping.
 *
 * Displays the parameter name and current label above the slider track.
 * The slider snaps to integer positions matching the enum's value labels.
 *
 * @param paramInfo Metadata describing the enum parameter (range, labels).
 * @param value Current parameter value.
 * @param accentColor Category-specific accent color for the slider thumb/track.
 * @param onValueChange Callback with the new snapped value.
 */
@Composable
private fun EnumParamSlider(
    paramInfo: EffectRegistry.ParamInfo,
    value: Float,
    accentColor: androidx.compose.ui.graphics.Color,
    onValueChange: (Float) -> Unit
) {
    val steps = (paramInfo.max - paramInfo.min).toInt().coerceAtLeast(1) - 1
    val displayText = paramInfo.valueLabels[value.roundToInt()]
        ?: formatParamValue(value, paramInfo.min, paramInfo.max)

    Column(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = paramInfo.name,
                fontSize = 12.sp,
                fontWeight = FontWeight.Medium,
                color = DesignSystem.TextSecondary
            )
            Text(
                text = displayText,
                fontSize = 11.sp,
                fontFamily = FontFamily.Monospace,
                color = DesignSystem.TextValue
            )
        }

        HardwareSlider(
            value = value.coerceIn(paramInfo.min, paramInfo.max),
            onValueChange = { newVal ->
                onValueChange(newVal.roundToInt().toFloat().coerceIn(paramInfo.min, paramInfo.max))
            },
            valueRange = paramInfo.min..paramInfo.max,
            steps = steps,
            accentColor = accentColor,
            modifier = Modifier.fillMaxWidth()
        )
    }
}

// ── Wet/Dry Mix Slider ──────────────────────────────────────────────────────

/**
 * Labeled slider for the effect's wet/dry mix blend.
 *
 * Displays "Mix" label with the current percentage, and a continuous
 * slider from 0% (fully dry) to 100% (fully wet).
 *
 * @param value Current mix value in [0, 1].
 * @param accentColor Category-specific accent color for the slider.
 * @param onValueChange Callback with the new mix value.
 */
@Composable
private fun WetDrySlider(
    value: Float,
    accentColor: androidx.compose.ui.graphics.Color,
    onValueChange: (Float) -> Unit
) {
    Column(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "Mix",
                fontSize = 12.sp,
                fontWeight = FontWeight.Medium,
                color = DesignSystem.TextSecondary
            )
            Text(
                text = "${(value * 100f).roundToInt()}%",
                fontSize = 11.sp,
                fontFamily = FontFamily.Monospace,
                color = DesignSystem.TextValue
            )
        }

        HardwareSlider(
            value = value.coerceIn(0f, 1f),
            onValueChange = onValueChange,
            valueRange = 0f..1f,
            accentColor = accentColor,
            modifier = Modifier.fillMaxWidth()
        )
    }
}

// ── Utility ─────────────────────────────────────────────────────────────────

/**
 * Format a parameter value for display based on the parameter's range.
 *
 * Uses adaptive decimal precision:
 * - Narrow range (<=2): two decimal places (e.g., "0.75")
 * - Medium range (<=100): one decimal place (e.g., "4.5")
 * - Wide range (>100): no decimal places (e.g., "440")
 *
 * @param value The current parameter value.
 * @param min Minimum value of the parameter range.
 * @param max Maximum value of the parameter range.
 * @return Formatted string representation of the value.
 */
private fun formatParamValue(value: Float, min: Float, max: Float): String {
    val range = max - min
    return when {
        range <= 2f -> String.format("%.2f", value)
        range <= 100f -> String.format("%.1f", value)
        else -> String.format("%.0f", value)
    }
}

// ── Focus Mode: Gutter Rail ─────────────────────────────────────────────────

/** Maximum number of colored dots shown in the gutter rail before truncation. */
private const val MAX_GUTTER_DOTS = 10

/** Width of the thin accent bar on bypass strips. */
private val BYPASS_STRIP_ACCENT_WIDTH = 3.dp

/**
 * Compact gutter rail that represents a run of consecutive bypassed effects.
 *
 * Displays colored category dots, a "{N} BYPASSED" label, and an expand/collapse
 * chevron. Tapping expands to reveal individual [BypassStrip] rows for each
 * bypassed effect, where the user can re-enable effects via their stomp switches.
 *
 * @param effects List of bypassed [EffectUiState] items grouped into this gutter.
 * @param expanded Whether the gutter is expanded to show individual bypass strips.
 * @param onToggleExpand Callback to toggle the gutter's expanded/collapsed state.
 * @param onToggleEffect Callback to toggle an effect on/off by its chain index.
 * @param modifier Outer modifier applied to the gutter container.
 */
@Composable
fun GutterRail(
    effects: List<EffectUiState>,
    expanded: Boolean,
    onToggleExpand: () -> Unit,
    onToggleEffect: (Int) -> Unit,
    modifier: Modifier = Modifier
) {
    val count = effects.size
    val names = remember(effects) { effects.map { it.name } }

    // Pre-compute category header bar colors for dots (at 50% alpha)
    val dotColors = remember(effects) {
        effects.map { DesignSystem.getHeaderColor(it.index).copy(alpha = 0.5f) }
    }

    // Pre-compute brushed metal gradient for gutter background
    val gutterBrush = remember { DesignSystem.brushedMetalGradient() }
    val gutterDividerColor = DesignSystem.GutterDivider

    // Animated chevron rotation matching RackModule style
    val chevronRotation by animateFloatAsState(
        targetValue = if (expanded) 90f else 0f,
        animationSpec = tween(durationMillis = 200),
        label = "gutterChevron"
    )

    Column(
        modifier = modifier
            .fillMaxWidth()
            .padding(bottom = 2.dp)
    ) {
        // ── Collapsed gutter rail header (44dp) ──────────────────────
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(DesignSystem.GutterHeight)
                .drawBehind {
                    // Brushed metal background
                    drawRect(brush = gutterBrush)

                    // Single-line bottom divider
                    val dividerHeight = 1.dp.toPx()
                    drawRect(
                        color = gutterDividerColor,
                        topLeft = Offset(0f, size.height - dividerHeight),
                        size = Size(size.width, dividerHeight)
                    )
                }
                .clickable(
                    onClickLabel = if (expanded) "Collapse bypassed effects" else "Expand bypassed effects",
                    onClick = onToggleExpand
                )
                .semantics {
                    contentDescription = "$count bypassed effects: ${names.joinToString()}"
                    stateDescription = if (expanded) "Expanded" else "Collapsed"
                }
                .padding(horizontal = 16.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Left: colored category dots (one per bypassed effect, max MAX_GUTTER_DOTS)
            val dotsToShow = dotColors.take(MAX_GUTTER_DOTS)
            for (color in dotsToShow) {
                Box(
                    modifier = Modifier
                        .padding(end = 3.dp)
                        .drawBehind {
                            drawCircle(
                                color = color,
                                radius = DesignSystem.BypassDotSize.toPx() / 2f
                            )
                        }
                        .width(DesignSystem.BypassDotSize)
                        .height(DesignSystem.BypassDotSize)
                )
            }
            if (dotColors.size > MAX_GUTTER_DOTS) {
                Text(
                    text = "\u2026",
                    fontSize = 10.sp,
                    color = DesignSystem.TextMuted,
                    modifier = Modifier.padding(start = 2.dp)
                )
            }

            Spacer(modifier = Modifier.width(8.dp))

            // Center: "{N} BYPASSED" label
            Text(
                text = "$count BYPASSED",
                fontSize = 11.sp,
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.Medium,
                color = DesignSystem.TextSecondary,
                letterSpacing = 1.sp,
                modifier = Modifier.weight(1f)
            )

            // Right: animated chevron
            Text(
                text = "\u25B8",
                fontSize = 11.sp,
                color = DesignSystem.CreamWhite.copy(alpha = 0.5f),
                modifier = Modifier.graphicsLayer { rotationZ = chevronRotation }
            )
        }

        // ── Expanded bypass strips (animated) ────────────────────────
        AnimatedVisibility(
            visible = expanded,
            enter = expandVertically(
                animationSpec = spring(
                    dampingRatio = 0.72f,
                    stiffness = Spring.StiffnessMediumLow
                ),
                expandFrom = Alignment.Top
            ) + fadeIn(animationSpec = tween(durationMillis = 180, delayMillis = 40)),
            exit = shrinkVertically(
                animationSpec = spring(
                    dampingRatio = 0.88f,
                    stiffness = Spring.StiffnessMedium
                ),
                shrinkTowards = Alignment.Top
            ) + fadeOut(animationSpec = tween(durationMillis = 120))
        ) {
            Column {
                for (effect in effects) {
                    BypassStrip(
                        effectState = effect,
                        onToggleEnabled = { onToggleEffect(effect.index) }
                    )
                }
            }
        }
    }
}

// ── Focus Mode: Gutter Rail Header (first-in-run visual expansion) ──────────

/**
 * Standalone 44dp header card for a run of consecutive bypassed effects.
 *
 * This is a simplified, non-expanding version of [GutterRail] designed for
 * use as an individual item in the LazyColumn. It shows the same brushed
 * metal background, colored category dots, "N BYPASSED" label, and an
 * expand/collapse chevron -- but does NOT embed child [BypassStrip] rows.
 * Expansion is handled by the caller (PedalboardScreen) which toggles
 * sibling items between 0dp spacers and 44dp BypassStrips.
 *
 * @param runSize Number of consecutive bypassed effects in this run.
 * @param runEffects All [EffectUiState] items in this run (for dot colors).
 * @param onExpandToggle Callback when the user taps to expand/collapse.
 * @param modifier Outer modifier (should include drag handle from caller).
 */
@Composable
internal fun GutterRailHeader(
    runSize: Int,
    runEffects: List<EffectUiState>,
    onExpandToggle: () -> Unit,
    modifier: Modifier = Modifier
) {
    // Pre-compute category dot colors (at 50% alpha)
    val dotColors = remember(runEffects) {
        runEffects.map { DesignSystem.getHeaderColor(it.index).copy(alpha = 0.5f) }
    }

    val names = remember(runEffects) { runEffects.map { it.name } }

    // Pre-compute brushed metal gradient for gutter background
    val gutterBrush = remember { DesignSystem.brushedMetalGradient() }
    val gutterDividerColor = DesignSystem.GutterDivider

    Row(
        modifier = modifier
            .fillMaxWidth()
            .height(DesignSystem.GutterHeight)
            .drawBehind {
                // Brushed metal background
                drawRect(brush = gutterBrush)

                // Single-line bottom divider
                val dividerHeight = 1.dp.toPx()
                drawRect(
                    color = gutterDividerColor,
                    topLeft = Offset(0f, size.height - dividerHeight),
                    size = Size(size.width, dividerHeight)
                )
            }
            .clickable(
                onClickLabel = "Expand bypassed effects",
                onClick = onExpandToggle
            )
            .semantics {
                contentDescription = "$runSize bypassed effects: ${names.joinToString()}"
            }
            .padding(horizontal = 16.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Left: colored category dots (one per bypassed effect, max MAX_GUTTER_DOTS)
        val dotsToShow = dotColors.take(MAX_GUTTER_DOTS)
        for (color in dotsToShow) {
            Box(
                modifier = Modifier
                    .padding(end = 3.dp)
                    .drawBehind {
                        drawCircle(
                            color = color,
                            radius = DesignSystem.BypassDotSize.toPx() / 2f
                        )
                    }
                    .width(DesignSystem.BypassDotSize)
                    .height(DesignSystem.BypassDotSize)
            )
        }
        if (dotColors.size > MAX_GUTTER_DOTS) {
            Text(
                text = "\u2026",
                fontSize = 10.sp,
                color = DesignSystem.TextMuted,
                modifier = Modifier.padding(start = 2.dp)
            )
        }

        Spacer(modifier = Modifier.width(8.dp))

        // Center: "{N} BYPASSED" label
        Text(
            text = "$runSize BYPASSED",
            fontSize = 11.sp,
            fontFamily = FontFamily.Monospace,
            fontWeight = FontWeight.Medium,
            color = DesignSystem.TextSecondary,
            letterSpacing = 1.sp,
            modifier = Modifier.weight(1f)
        )

        // Right: chevron indicator (static, no rotation animation needed
        // since expansion is handled at the list level, not within this composable)
        Text(
            text = "\u25B8",
            fontSize = 11.sp,
            color = DesignSystem.CreamWhite.copy(alpha = 0.5f)
        )
    }
}

// ── Focus Mode: Bypass Strip ────────────────────────────────────────────────

/**
 * Compact 44dp row for a single bypassed effect inside an expanded gutter.
 *
 * Shows a thin accent bar, dim LED dot, effect name, and a stomp switch.
 * Tapping anywhere on the strip toggles the effect's enabled state, which
 * causes the effect to "promote" to a full RackModule as the chain re-segments.
 *
 * @param effectState UI state for this bypassed effect.
 * @param onToggleEnabled Callback to toggle the effect on/off.
 */
@Composable
internal fun BypassStrip(
    effectState: EffectUiState,
    onToggleEnabled: () -> Unit,
    modifier: Modifier = Modifier
) {
    val category = remember(effectState.index) {
        DesignSystem.getCategory(effectState.index)
    }
    val accentColor = remember(effectState.index) {
        DesignSystem.getHeaderColor(effectState.index).copy(alpha = 0.35f)
    }

    // Recessed background: ModuleSurface at 50% alpha
    val stripBgColor = remember { DesignSystem.ModuleSurface.copy(alpha = 0.5f) }
    val dividerColor = DesignSystem.Divider

    Row(
        modifier = modifier
            .fillMaxWidth()
            .height(DesignSystem.BypassStripHeight)
            .drawBehind {
                // Recessed background
                drawRect(color = stripBgColor)

                // Thin accent bar on the left edge
                drawRect(
                    color = accentColor,
                    topLeft = Offset.Zero,
                    size = Size(BYPASS_STRIP_ACCENT_WIDTH.toPx(), size.height)
                )

                // Single-pixel bottom divider
                val dividerHeight = 1.dp.toPx()
                drawRect(
                    color = dividerColor,
                    topLeft = Offset(0f, size.height - dividerHeight),
                    size = Size(size.width, dividerHeight)
                )
            }
            .clickable(
                onClickLabel = "Enable ${effectState.name}",
                onClick = onToggleEnabled
            )
            .semantics {
                contentDescription = "${effectState.name}, bypassed. Tap to enable."
                stateDescription = "Disabled"
                role = Role.Button
            }
            .padding(start = BYPASS_STRIP_ACCENT_WIDTH + 12.dp, end = 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Dim LED dot (bypass color)
        LedIndicator(
            active = false,
            activeColor = category.ledActive,
            inactiveColor = category.ledBypass,
            size = DesignSystem.BypassDotSize
        )

        Spacer(modifier = Modifier.width(10.dp))

        // Effect name: monospace, muted, ALL CAPS
        Text(
            text = effectState.name.uppercase(),
            fontSize = 12.sp,
            fontFamily = FontFamily.Monospace,
            fontWeight = FontWeight.Normal,
            letterSpacing = 1.sp,
            color = DesignSystem.TextMuted,
            maxLines = 1,
            modifier = Modifier.weight(1f)
        )

        // Stomp switch (44dp, meets accessibility touch target minimum)
        StompSwitch(
            active = effectState.enabled,
            categoryColor = category.ledActive,
            onClick = onToggleEnabled,
            size = 44.dp
        )
    }
}

// ── Focus Mode: Bypass Marker (thin line for collapsed bypass slots) ────────

/**
 * Minimal-height marker for a bypassed effect in Focus Mode.
 *
 * When Focus Mode is active, bypassed effects render as these thin colored
 * lines rather than full [BypassStrip] rows or grouped [GutterRail] sections.
 * This keeps every effect as an individual item in the LazyColumn (preserving
 * stable indices for drag-and-drop) while taking minimal vertical space.
 *
 * Each marker shows a thin category-colored accent bar so the user can
 * identify effect categories at a glance. Tapping toggles the effect on.
 *
 * The height is [DesignSystem.BypassMarkerHeight] (4dp) -- large enough to
 * be visible as a faint divider and to serve as a valid drop target for the
 * reorderable library, but small enough to be unobtrusive when 20+ effects
 * are bypassed.
 *
 * @param effectState UI state for this bypassed effect.
 * @param onToggleEnabled Callback to toggle the effect on/off.
 * @param modifier Outer modifier (should include drag handle from caller).
 */
@Composable
internal fun BypassMarker(
    effectState: EffectUiState,
    onToggleEnabled: () -> Unit,
    modifier: Modifier = Modifier
) {
    val accentColor = remember(effectState.index) {
        DesignSystem.getHeaderColor(effectState.index).copy(alpha = 0.30f)
    }
    val dividerColor = DesignSystem.Divider

    Box(
        modifier = modifier
            .fillMaxWidth()
            .height(DesignSystem.BypassMarkerHeight)
            .drawBehind {
                // Thin category-colored accent bar spanning the full width
                drawRect(color = accentColor)

                // Single-pixel bottom divider
                val dividerHeight = 0.5.dp.toPx()
                drawRect(
                    color = dividerColor,
                    topLeft = Offset(0f, size.height - dividerHeight),
                    size = Size(size.width, dividerHeight)
                )
            }
            .clickable(
                onClickLabel = "Enable ${effectState.name}",
                onClick = onToggleEnabled
            )
            .semantics {
                contentDescription = "${effectState.name}, bypassed. Tap to enable."
                stateDescription = "Disabled"
                role = Role.Button
            }
    )
}
