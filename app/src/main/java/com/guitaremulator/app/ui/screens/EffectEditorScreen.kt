package com.guitaremulator.app.ui.screens

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.slideInVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.draw.scale
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt
import com.guitaremulator.app.data.EffectRegistry
import com.guitaremulator.app.ui.theme.DesignSystem
import com.guitaremulator.app.viewmodel.EffectUiState

/**
 * Modal bottom sheet for advanced detail editing of specific effects.
 *
 * This sheet provides effect-specific advanced controls:
 *   - AmpModel (index 5): Amp mode selector, model description, neural model loader
 *   - CabinetSim (index 6): Cabinet type selector, cabinet description, IR loader
 *   - Delay (index 12): Tap tempo, tempo subdivisions
 *
 * Styled with "Blackface Terminal" skeuomorphic treatment: amp-panel borders,
 * recessed description cards, footswitch-style tap tempo, and connected
 * hardware button strip for subdivisions.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun EffectEditorSheet(
    effectState: EffectUiState,
    onDismiss: () -> Unit,
    onParameterChange: (paramId: Int, value: Float) -> Unit,
    onLoadCustomIR: (() -> Unit)? = null,
    userIRName: String = "",
    isLoadingIR: Boolean = false,
    irLoadError: String = "",
    onLoadNeuralModel: (() -> Unit)? = null,
    onClearNeuralModel: (() -> Unit)? = null,
    neuralModelName: String = "",
    isLoadingNeuralModel: Boolean = false,
    neuralModelError: String = "",
    isNeuralModelLoaded: Boolean = false
) {
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
    val effectInfo = EffectRegistry.getEffectInfo(effectState.index)
    val category = DesignSystem.getCategory(effectState.index)

    // Pre-resolve panel border colors for the drawBehind lambda
    val panelShadow = DesignSystem.PanelShadow
    val panelHighlight = DesignSystem.PanelHighlight
    val panelBorderWidth = DesignSystem.PanelBorderWidth

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        containerColor = DesignSystem.ControlPanelSurface,
        contentColor = DesignSystem.TextPrimary,
        shape = RoundedCornerShape(topStart = 16.dp, topEnd = 16.dp),
        dragHandle = {
            Box(
                modifier = Modifier
                    .padding(top = 10.dp, bottom = 6.dp)
                    .size(width = 32.dp, height = 4.dp)
                    .clip(RoundedCornerShape(2.dp))
                    .background(DesignSystem.ModuleBorder)
            )
        },
        modifier = Modifier.drawBehind {
            // Double-line machined edge on the sheet surface
            val outerStroke = panelBorderWidth.toPx()
            val innerStroke = outerStroke * 0.6f
            val cornerPx = 16.dp.toPx()

            // Outer shadow border
            drawRoundRect(
                color = panelShadow,
                topLeft = Offset.Zero,
                size = size,
                cornerRadius = CornerRadius(cornerPx, cornerPx),
                style = Stroke(width = outerStroke)
            )
            // Inner highlight border (inset by outerStroke)
            val inset = outerStroke + 1.dp.toPx()
            drawRoundRect(
                color = panelHighlight,
                topLeft = Offset(inset, inset),
                size = Size(size.width - inset * 2, size.height - inset * 2),
                cornerRadius = CornerRadius(
                    (cornerPx - inset).coerceAtLeast(0f),
                    (cornerPx - inset).coerceAtLeast(0f)
                ),
                style = Stroke(width = innerStroke)
            )
        }
    ) {
        // Content entrance animation
        var contentVisible by remember { mutableStateOf(false) }
        LaunchedEffect(Unit) { contentVisible = true }

        AnimatedVisibility(
            visible = contentVisible,
            enter = fadeIn(tween(200, delayMillis = 80)) + slideInVertically(
                animationSpec = spring(
                    dampingRatio = 0.72f,
                    stiffness = Spring.StiffnessMediumLow
                ),
                initialOffsetY = { it / 6 }
            )
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 20.dp)
                    .padding(bottom = 32.dp)
                    .verticalScroll(rememberScrollState())
            ) {
                // Header: effect name + "ADVANCED" subtitle + close button
                AdvancedDetailHeader(
                    name = effectState.name,
                    category = category,
                    onClose = onDismiss
                )

                Spacer(modifier = Modifier.height(16.dp))
                HorizontalDivider(color = DesignSystem.Divider)
                Spacer(modifier = Modifier.height(16.dp))

                // Route content by effect index
                if (effectInfo != null) {
                    when (effectState.index) {
                        5 -> { // AmpModel
                            // Enum param slider for Model Type
                            val enumParam = effectInfo.params.find { it.valueLabels.isNotEmpty() }
                            if (enumParam != null) {
                                SectionLabel("Amp Mode")
                                val currentValue = effectState.parameters[enumParam.id]
                                    ?: enumParam.defaultValue
                                ParameterSlider(
                                    name = enumParam.name,
                                    value = currentValue,
                                    min = enumParam.min,
                                    max = enumParam.max,
                                    unit = enumParam.unit,
                                    valueLabels = enumParam.valueLabels,
                                    accentColor = category.knobAccent,
                                    onValueChange = { newValue ->
                                        onParameterChange(enumParam.id, newValue)
                                    }
                                )

                                Spacer(modifier = Modifier.height(12.dp))

                                // Amp model description card
                                AmpModelDescription(
                                    modelType = (effectState.parameters[enumParam.id] ?: 0f).roundToInt()
                                )
                            }

                            Spacer(modifier = Modifier.height(12.dp))
                            HorizontalDivider(color = DesignSystem.Divider)
                            Spacer(modifier = Modifier.height(12.dp))

                            // Neural Amp Model section
                            if (onLoadNeuralModel != null) {
                                SectionLabel("Neural Amp Model")
                                NeuralModelLoader(
                                    onLoadModel = onLoadNeuralModel,
                                    onClearModel = onClearNeuralModel ?: {},
                                    modelName = neuralModelName,
                                    isLoading = isLoadingNeuralModel,
                                    isModelLoaded = isNeuralModelLoaded,
                                    errorMessage = neuralModelError,
                                    accentColor = category.knobAccent
                                )
                            }
                        }

                        6 -> { // CabinetSim
                            // Enum param slider for Cabinet Type
                            val enumParam = effectInfo.params.find { it.valueLabels.isNotEmpty() }
                            if (enumParam != null) {
                                SectionLabel("Cabinet Selection")
                                val currentValue = effectState.parameters[enumParam.id]
                                    ?: enumParam.defaultValue
                                ParameterSlider(
                                    name = enumParam.name,
                                    value = currentValue,
                                    min = enumParam.min,
                                    max = enumParam.max,
                                    unit = enumParam.unit,
                                    valueLabels = enumParam.valueLabels,
                                    accentColor = category.knobAccent,
                                    onValueChange = { newValue ->
                                        onParameterChange(enumParam.id, newValue)
                                    }
                                )

                                Spacer(modifier = Modifier.height(12.dp))

                                // Cabinet description card
                                CabinetDescription(
                                    cabinetType = (effectState.parameters[enumParam.id] ?: 0f).roundToInt()
                                )
                            }

                            Spacer(modifier = Modifier.height(12.dp))
                            HorizontalDivider(color = DesignSystem.Divider)
                            Spacer(modifier = Modifier.height(12.dp))

                            // Custom Impulse Response section
                            if (onLoadCustomIR != null) {
                                SectionLabel("Custom Impulse Response")
                                CabinetIRLoader(
                                    onLoadIR = onLoadCustomIR,
                                    userIRName = userIRName,
                                    isLoading = isLoadingIR,
                                    errorMessage = irLoadError,
                                    accentColor = category.knobAccent
                                )
                            }
                        }

                        12 -> { // Delay
                            SectionLabel("Tap Tempo")
                            TapTempoButton(
                                accentColor = category.knobAccent,
                                onTempoDetected = { tempoMs ->
                                    onParameterChange(0, tempoMs)
                                }
                            )

                            Spacer(modifier = Modifier.height(16.dp))
                            HorizontalDivider(color = DesignSystem.Divider)
                            Spacer(modifier = Modifier.height(12.dp))

                            // Tempo subdivisions
                            SectionLabel("Subdivisions")
                            TempoSubdivisions(
                                currentDelayMs = effectState.parameters[0] ?: 500f,
                                onSubdivisionSelected = { ms ->
                                    onParameterChange(0, ms)
                                },
                                accentColor = category.knobAccent
                            )
                        }
                    }
                }

                Spacer(modifier = Modifier.height(16.dp))
            }
        }
    }
}

/**
 * Header for the advanced detail sheet.
 *
 * Shows effect name (18sp bold) and "ADVANCED" subtitle in the category's
 * primary color, with a close button (X icon) on the right.
 */
@Composable
private fun AdvancedDetailHeader(
    name: String,
    category: DesignSystem.EffectCategory,
    onClose: () -> Unit
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Column {
            Text(
                text = name,
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold,
                color = DesignSystem.TextPrimary
            )
            Text(
                text = "ADVANCED",
                fontSize = 11.sp,
                fontWeight = FontWeight.Bold,
                letterSpacing = 1.5.sp,
                color = category.primary
            )
        }

        IconButton(onClick = onClose) {
            Icon(
                imageVector = Icons.Default.Close,
                contentDescription = "Close",
                tint = DesignSystem.TextSecondary
            )
        }
    }
}

/**
 * Uppercase section label styled as vintage amp panel silk-screening.
 *
 * Monospace, CreamWhite, with wide letter spacing to evoke
 * stamped/engraved panel lettering on real hardware.
 */
@Composable
private fun SectionLabel(text: String) {
    Text(
        text = text.uppercase(),
        fontSize = 11.sp,
        fontWeight = FontWeight.Bold,
        fontFamily = FontFamily.Monospace,
        letterSpacing = 1.5.sp,
        color = DesignSystem.CreamWhite,
        modifier = Modifier.padding(bottom = 10.dp)
    )
}

/**
 * Descriptive card showing the currently selected amp model type and
 * its sonic character, helping the user understand each voicing.
 *
 * Styled as a recessed panel: AmpChassisGray background with PanelShadow
 * border, CreamWhite title text and TextSecondary description.
 *
 * @param modelType Integer index of the current amp model (0=Clean through 8=Silvertone).
 */
@Composable
private fun AmpModelDescription(modelType: Int) {
    val (title, description) = when (modelType) {
        0 -> "Clean" to "Fender-style clean channel. Low compression, bright chimey highs. Responds dynamically to picking attack. Ideal for jazz, country, funk, and clean rhythm."
        1 -> "Crunch" to "British-style breakup reminiscent of Marshall JCM800. Warm midrange push with moderate compression. Natural for blues, classic rock, and crunchy rhythm."
        2 -> "Hi-Gain" to "Modern high-gain voicing with tight low-end tracking. Aggressive clipping with extended sustain. Built for hard rock, metal, and heavy styles."
        3 -> "Marshall Plexi" to "Classic 1959 Super Lead. Rich harmonics, mid-forward growl with channel-jumping character. The backbone of classic rock — Hendrix, Led Zeppelin, early Van Halen."
        4 -> "Hiwatt DR103" to "Exceptionally clean and articulate with massive headroom. Tight, piano-like bass response and crystalline highs. David Gilmour's signature amp for Pink Floyd's expansive tones."
        5 -> "Silver Jubilee" to "Smooth, mid-heavy Marshall voicing with diode clipping in the preamp. Liquid lead tones with rich harmonic overtones. John Frusciante's go-to amp for the Red Hot Chili Peppers."
        6 -> "Vox AC30" to "The chimey British sound. EL84 tubes deliver early breakup with sparkling highs and jangly character. The Beatles, Brian May, The Edge — pure classic."
        7 -> "Fender Twin Reverb" to "Maximum headroom, pristine cleans. Deep bass, scooped mids, and brilliant treble. The definitive clean platform for jazz, country, and studio work."
        8 -> "Silvertone 1485" to "Lo-fi, compressed, early breakup. Cheap transformers and simple circuit create a 'trashy' quality that's become iconic. Jack White's signature raw, garage-rock tone."
        else -> "Unknown" to ""
    }

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
            .background(DesignSystem.AmpChassisGray)
            .border(
                width = 1.dp,
                color = DesignSystem.PanelShadow,
                shape = RoundedCornerShape(8.dp)
            )
            .padding(12.dp)
    ) {
        Text(
            title,
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            color = DesignSystem.CreamWhite
        )
        if (description.isNotEmpty()) {
            Spacer(Modifier.height(4.dp))
            Text(
                description,
                fontSize = 12.sp,
                color = DesignSystem.CreamWhite.copy(alpha = 0.70f),
                lineHeight = 16.sp
            )
        }
    }
}

/**
 * Descriptive card showing the currently selected cabinet type and
 * its tonal characteristics.
 *
 * Styled as a recessed panel: AmpChassisGray background with PanelShadow
 * border, CreamWhite title text and lighter description.
 *
 * @param cabinetType Integer index of the current cabinet (0=1x12 through 9=Jensen C10Q, 3=User IR).
 */
@Composable
private fun CabinetDescription(cabinetType: Int) {
    val (title, description) = when (cabinetType) {
        0 -> "1x12 Combo" to "Open-back single 12\" speaker. Bright and present with looser low-end. Captures the character of a Fender Deluxe Reverb. Best for clean and low-gain tones."
        1 -> "4x12 British" to "Closed-back 4x12 stack with Celestion-style response. Tight bass, forward upper-mids. The classic Marshall sound. Ideal for rock and high-gain."
        2 -> "2x12 Open Back" to "Open-back 2x12 with balanced response. Smooth midrange, gentle top-end rolloff. Inspired by the Vox AC30. Versatile across all gain levels."
        3 -> "User IR" to "Custom impulse response loaded from a .wav file. Use third-party IR packs or capture your own cabinet response."
        4 -> "Greenback (G12M)" to "The original Marshall speaker. Warm, vintage character with smooth treble rolloff above 4kHz. Produces the classic 'creamy' tone heard on countless rock records from the '60s and '70s."
        5 -> "G12H-30" to "Heavier magnet than the Greenback for tighter lows and more aggressive upper-mids. Cuts through dense mixes with more bite and definition. Hendrix live, Angus Young."
        6 -> "G12-65" to "Modern Celestion with flat, transparent response and extended highs. Handles high gain without getting muddy. Eddie Van Halen's speaker of choice for the 'brown sound' era."
        7 -> "Fane Crescendo" to "Hi-fi, almost studio-monitor flat response with extended high frequencies. David Gilmour's speaker in the WEM Starfinder cabinet. Pristine detail and articulation."
        8 -> "Alnico Blue" to "The Vox signature speaker. Shimmering chimey highs with an open, airy quality from the Alnico magnet. Jangly and bright — essential for Beatles-era tones and beyond."
        9 -> "Jensen C10Q" to "10-inch speaker with focused midrange and quick transient response. Limited bass extension creates a tight, 'nasal' lo-fi quality. Jack White's Silvertone character."
        else -> "Unknown" to ""
    }

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
            .background(DesignSystem.AmpChassisGray)
            .border(
                width = 1.dp,
                color = DesignSystem.PanelShadow,
                shape = RoundedCornerShape(8.dp)
            )
            .padding(12.dp)
    ) {
        Text(
            title,
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            color = DesignSystem.CreamWhite
        )
        if (description.isNotEmpty()) {
            Spacer(Modifier.height(4.dp))
            Text(
                description,
                fontSize = 12.sp,
                color = DesignSystem.CreamWhite.copy(alpha = 0.70f),
                lineHeight = 16.sp
            )
        }
    }
}

/**
 * Row of subdivision buttons styled as a connected hardware button strip.
 *
 * Buttons are rendered in a single Row with no horizontal gaps, separated
 * by 1dp PanelShadow dividers. The first button has left-rounded corners
 * (8dp), the last has right-rounded corners, creating a continuous strip.
 *
 * Selected button uses SafeGreen background with CreamWhite text;
 * unselected buttons use ControlPanelSurface with TextSecondary text.
 *
 * @param currentDelayMs Current delay time in milliseconds.
 * @param onSubdivisionSelected Callback with the new delay time in milliseconds.
 * @param accentColor Category accent color (unused in strip mode; SafeGreen is used for consistency).
 */
@Composable
private fun TempoSubdivisions(
    currentDelayMs: Float,
    onSubdivisionSelected: (Float) -> Unit,
    accentColor: Color
) {
    // Infer BPM from current delay time (assuming quarter note)
    val clampedDelayMs = currentDelayMs.coerceIn(30f, 2000f)
    val bpm = 60000f / clampedDelayMs

    val subdivisions = listOf(
        "1/4" to 60000f / bpm,
        "dot 1/8" to 60000f / bpm * 0.75f,
        "1/8" to 60000f / bpm * 0.5f,
        "1/8T" to 60000f / bpm / 3f * 2f,
        "1/16" to 60000f / bpm * 0.25f
    )

    Text(
        text = "%.0f BPM (from current delay)".format(bpm),
        fontSize = 12.sp,
        fontFamily = FontFamily.Monospace,
        color = DesignSystem.TextSecondary,
        modifier = Modifier.padding(bottom = 8.dp)
    )

    // Connected hardware button strip
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
    ) {
        for ((index, entry) in subdivisions.withIndex()) {
            val (label, ms) = entry
            val isActive = kotlin.math.abs(clampedDelayMs - ms) < 1f
            val isFirst = index == 0
            val isLast = index == subdivisions.lastIndex

            // Determine corner shape for this button position
            val shape = when {
                isFirst -> RoundedCornerShape(
                    topStart = 8.dp, bottomStart = 8.dp,
                    topEnd = 0.dp, bottomEnd = 0.dp
                )
                isLast -> RoundedCornerShape(
                    topStart = 0.dp, bottomStart = 0.dp,
                    topEnd = 8.dp, bottomEnd = 8.dp
                )
                else -> RoundedCornerShape(0.dp)
            }

            // Divider between buttons (not before the first)
            if (!isFirst) {
                Box(
                    modifier = Modifier
                        .width(1.dp)
                        .height(48.dp)
                        .background(DesignSystem.PanelShadow)
                )
            }

            Box(
                modifier = Modifier
                    .weight(1f)
                    .height(48.dp)
                    .background(
                        color = if (isActive) DesignSystem.SafeGreen
                        else DesignSystem.ControlPanelSurface,
                        shape = shape
                    )
                    .clip(shape)
                    .clickable { onSubdivisionSelected(ms.coerceIn(1f, 2000f)) },
                contentAlignment = Alignment.Center
            ) {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Text(
                        label,
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace,
                        color = if (isActive) DesignSystem.CreamWhite
                        else DesignSystem.TextSecondary
                    )
                    Text(
                        "%.0f ms".format(ms),
                        fontSize = 9.sp,
                        fontFamily = FontFamily.Monospace,
                        color = if (isActive) DesignSystem.CreamWhite.copy(alpha = 0.8f)
                        else DesignSystem.TextMuted
                    )
                }
            }
        }
    }
}

/**
 * Labeled slider for enum parameters and wet/dry mix.
 * Uses category accent color instead of hardcoded green.
 */
@Composable
private fun ParameterSlider(
    name: String,
    value: Float,
    min: Float,
    max: Float,
    unit: String,
    valueLabels: Map<Int, String> = emptyMap(),
    accentColor: Color = DesignSystem.SafeGreen,
    onValueChange: (Float) -> Unit
) {
    val isEnum = valueLabels.isNotEmpty()
    val steps = if (isEnum) (max - min).toInt().coerceAtLeast(1) - 1 else 0

    Column(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = name,
                fontSize = 14.sp,
                fontWeight = FontWeight.Medium,
                color = DesignSystem.TextSecondary
            )

            val displayText = if (isEnum) {
                valueLabels[value.roundToInt()] ?: formatParameterValue(value, min, max)
            } else {
                val displayValue = formatParameterValue(value, min, max)
                if (unit.isNotEmpty()) "$displayValue $unit" else displayValue
            }

            Text(
                text = displayText,
                fontSize = 13.sp,
                fontFamily = FontFamily.Monospace,
                color = DesignSystem.TextValue
            )
        }

        Spacer(modifier = Modifier.height(2.dp))

        Slider(
            value = value.coerceIn(min, max),
            onValueChange = { newVal ->
                if (isEnum) onValueChange(newVal.roundToInt().toFloat().coerceIn(min, max))
                else onValueChange(newVal)
            },
            valueRange = min..max,
            steps = steps,
            modifier = Modifier.fillMaxWidth(),
            colors = SliderDefaults.colors(
                thumbColor = accentColor,
                activeTrackColor = accentColor.copy(alpha = 0.7f),
                inactiveTrackColor = DesignSystem.ModuleBorder
            )
        )
    }
}

/**
 * Format a parameter value for display with appropriate decimal places.
 */
private fun formatParameterValue(value: Float, min: Float, max: Float): String {
    val range = max - min
    return when {
        range <= 2f -> String.format("%.2f", value)
        range <= 100f -> String.format("%.1f", value)
        else -> String.format("%.0f", value)
    }
}

/**
 * Section for loading a custom cabinet impulse response file.
 */
@Composable
private fun CabinetIRLoader(
    onLoadIR: () -> Unit,
    userIRName: String,
    isLoading: Boolean,
    errorMessage: String,
    accentColor: Color = DesignSystem.SafeGreen
) {
    Column(modifier = Modifier.fillMaxWidth()) {
        if (userIRName.isNotEmpty()) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(8.dp))
                    .background(DesignSystem.AmpChassisGray)
                    .border(
                        width = 1.dp,
                        color = DesignSystem.PanelShadow,
                        shape = RoundedCornerShape(8.dp)
                    )
                    .padding(horizontal = 12.dp, vertical = 8.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "Loaded:",
                    fontSize = 12.sp,
                    color = DesignSystem.TextSecondary
                )
                Text(
                    text = userIRName,
                    fontSize = 13.sp,
                    fontFamily = FontFamily.Monospace,
                    color = accentColor,
                    maxLines = 1
                )
            }
            Spacer(modifier = Modifier.height(8.dp))
        }

        Button(
            onClick = onLoadIR,
            enabled = !isLoading,
            modifier = Modifier.fillMaxWidth(),
            colors = ButtonDefaults.buttonColors(
                containerColor = DesignSystem.ModuleBorder,
                contentColor = DesignSystem.TextPrimary,
                disabledContainerColor = DesignSystem.ModuleSurface,
                disabledContentColor = DesignSystem.TextMuted
            ),
            shape = RoundedCornerShape(8.dp)
        ) {
            if (isLoading) {
                CircularProgressIndicator(
                    modifier = Modifier.size(18.dp),
                    color = accentColor,
                    strokeWidth = 2.dp
                )
                Spacer(modifier = Modifier.width(8.dp))
                Text("Loading IR...", fontSize = 14.sp)
            } else {
                Text("Load Custom IR (.wav)", fontSize = 14.sp)
            }
        }

        if (errorMessage.isNotEmpty()) {
            Spacer(modifier = Modifier.height(6.dp))
            Text(
                text = errorMessage,
                fontSize = 12.sp,
                color = DesignSystem.ClipRed,
                textAlign = TextAlign.Center,
                modifier = Modifier.fillMaxWidth()
            )
        }
    }
}

/**
 * Section for loading a neural amp model (.nam file).
 */
@Composable
private fun NeuralModelLoader(
    onLoadModel: () -> Unit,
    onClearModel: () -> Unit,
    modelName: String,
    isLoading: Boolean,
    isModelLoaded: Boolean,
    errorMessage: String,
    accentColor: Color = DesignSystem.SafeGreen
) {
    Column(modifier = Modifier.fillMaxWidth()) {
        Text(
            text = if (isModelLoaded) "Neural mode active" else "Classic waveshaping active",
            fontSize = 12.sp,
            color = if (isModelLoaded) accentColor else DesignSystem.TextSecondary
        )

        Spacer(modifier = Modifier.height(8.dp))

        if (modelName.isNotEmpty()) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(8.dp))
                    .background(DesignSystem.AmpChassisGray)
                    .border(
                        width = 1.dp,
                        color = DesignSystem.PanelShadow,
                        shape = RoundedCornerShape(8.dp)
                    )
                    .padding(horizontal = 12.dp, vertical = 8.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "Model:",
                    fontSize = 12.sp,
                    color = DesignSystem.TextSecondary
                )
                Text(
                    text = modelName,
                    fontSize = 13.sp,
                    fontFamily = FontFamily.Monospace,
                    color = accentColor,
                    maxLines = 1
                )
            }
            Spacer(modifier = Modifier.height(8.dp))
        }

        Button(
            onClick = onLoadModel,
            enabled = !isLoading,
            modifier = Modifier.fillMaxWidth(),
            colors = ButtonDefaults.buttonColors(
                containerColor = DesignSystem.ModuleBorder,
                contentColor = DesignSystem.TextPrimary,
                disabledContainerColor = DesignSystem.ModuleSurface,
                disabledContentColor = DesignSystem.TextMuted
            ),
            shape = RoundedCornerShape(8.dp)
        ) {
            if (isLoading) {
                CircularProgressIndicator(
                    modifier = Modifier.size(18.dp),
                    color = accentColor,
                    strokeWidth = 2.dp
                )
                Spacer(modifier = Modifier.width(8.dp))
                Text("Loading model...", fontSize = 14.sp)
            } else {
                Text("Load Neural Model (.nam)", fontSize = 14.sp)
            }
        }

        if (isModelLoaded) {
            Spacer(modifier = Modifier.height(6.dp))
            Button(
                onClick = onClearModel,
                enabled = !isLoading,
                modifier = Modifier.fillMaxWidth(),
                colors = ButtonDefaults.buttonColors(
                    containerColor = DesignSystem.DangerSurface,
                    contentColor = DesignSystem.DangerText
                ),
                shape = RoundedCornerShape(8.dp)
            ) {
                Text("Clear Model (use classic)", fontSize = 14.sp)
            }
        }

        if (errorMessage.isNotEmpty()) {
            Spacer(modifier = Modifier.height(6.dp))
            Text(
                text = errorMessage,
                fontSize = 12.sp,
                color = DesignSystem.ClipRed,
                textAlign = TextAlign.Center,
                modifier = Modifier.fillMaxWidth()
            )
        }
    }
}

/**
 * Tap tempo button styled as a footswitch pedal.
 *
 * Features a 56dp height with ControlPanelSurface background and
 * ChromeHighlight border, monospace CreamWhite "TAP" label, and a
 * spring-animated scale effect (1.0 -> 0.93) on press to simulate
 * physical button depression.
 */
@Composable
private fun TapTempoButton(
    accentColor: Color = DesignSystem.SafeGreen,
    onTempoDetected: (Float) -> Unit
) {
    var lastTapTime by remember { mutableLongStateOf(0L) }
    var displayBpm by remember { mutableStateOf("") }
    var intervals by remember { mutableStateOf(listOf<Long>()) }

    // Track press state for the scale animation
    val interactionSource = remember { MutableInteractionSource() }
    val isPressed by interactionSource.collectIsPressedAsState()
    val scale by animateFloatAsState(
        targetValue = if (isPressed) 0.93f else 1.0f,
        animationSpec = spring(
            dampingRatio = Spring.DampingRatioMediumBouncy,
            stiffness = Spring.StiffnessMedium
        ),
        label = "tapTempoScale"
    )

    Column(
        modifier = Modifier.fillMaxWidth(),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Button(
            onClick = {
                val now = System.currentTimeMillis()
                val elapsed = now - lastTapTime

                if (lastTapTime > 0L && elapsed in 120L..2000L) {
                    val newIntervals = (intervals + elapsed).takeLast(3)
                    intervals = newIntervals

                    val avgMs = newIntervals.average().toFloat()
                    val bpm = 60000f / avgMs
                    displayBpm = "%.0f BPM (%.0f ms)".format(bpm, avgMs)

                    onTempoDetected(avgMs.coerceIn(1f, 2000f))
                } else {
                    intervals = emptyList()
                    displayBpm = "Tap again..."
                }

                lastTapTime = now
            },
            interactionSource = interactionSource,
            modifier = Modifier
                .fillMaxWidth()
                .height(56.dp)
                .scale(scale),
            colors = ButtonDefaults.buttonColors(
                containerColor = DesignSystem.ControlPanelSurface,
                contentColor = DesignSystem.CreamWhite
            ),
            shape = RoundedCornerShape(8.dp),
            border = androidx.compose.foundation.BorderStroke(
                width = 1.5.dp,
                color = DesignSystem.ChromeHighlight
            )
        ) {
            Text(
                "TAP",
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                color = DesignSystem.CreamWhite
            )
        }

        if (displayBpm.isNotEmpty()) {
            Spacer(modifier = Modifier.height(6.dp))
            Text(
                text = displayBpm,
                fontSize = 13.sp,
                fontFamily = FontFamily.Monospace,
                color = accentColor
            )
        }
    }
}
