package com.guitaremulator.app.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.RadioButton
import androidx.compose.material3.RadioButtonDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.audio.InputSourceManager
import com.guitaremulator.app.ui.theme.DesignSystem

/**
 * Settings screen displaying audio device information, latency metrics,
 * and battery optimization controls.
 *
 * Styled with "Blackface Terminal" skeuomorphic treatment: brushed-metal
 * top bar, monospace ALL-CAPS section headers with gold piping underlines,
 * amp-panel card borders (double-line machined edge), and recessed
 * value-display panels for numeric readouts.
 *
 * @param sampleRate Device native sample rate in Hz (0 if engine not running).
 * @param framesPerBuffer Audio buffer size in frames (0 if engine not running).
 * @param estimatedLatencyMs Round-trip latency estimate in milliseconds.
 * @param isEngineRunning Whether the audio engine is currently active.
 * @param isBatteryOptimized Whether the app is exempt from battery optimization.
 * @param currentInputSource The currently active input source.
 * @param onInputSourceSelected Callback when the user manually selects an input source.
 * @param onRequestBatteryExemption Callback to prompt the user for battery exemption.
 * @param onBack Callback to navigate back to the pedalboard.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(
    sampleRate: Int,
    framesPerBuffer: Int,
    estimatedLatencyMs: Float,
    isEngineRunning: Boolean,
    isBatteryOptimized: Boolean,
    currentInputSource: InputSourceManager.InputSource,
    onInputSourceSelected: (InputSourceManager.InputSource) -> Unit,
    bufferMultiplier: Int = 2,
    onBufferMultiplierChanged: (Int) -> Unit = {},
    onRequestBatteryExemption: () -> Unit,
    onBack: () -> Unit
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Text(
                        "SETTINGS",
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold,
                        letterSpacing = 2.sp,
                        color = DesignSystem.CreamWhite
                    )
                },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(
                            Icons.AutoMirrored.Filled.ArrowBack,
                            "Back",
                            tint = DesignSystem.CreamWhite
                        )
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = Color.Transparent,
                    titleContentColor = DesignSystem.CreamWhite,
                    navigationIconContentColor = DesignSystem.CreamWhite
                ),
                modifier = Modifier
                    .background(DesignSystem.brushedMetalGradient())
                    .drawBehind {
                        val barWidth = size.width
                        val barHeight = size.height

                        // Gold piping underline (2dp gold + 1dp shadow below)
                        val goldStroke = 2.dp.toPx()
                        val shadowStroke = 1.dp.toPx()
                        drawLine(
                            color = DesignSystem.GoldPiping.copy(alpha = 0.60f),
                            start = Offset(0f, barHeight - goldStroke / 2f),
                            end = Offset(barWidth, barHeight - goldStroke / 2f),
                            strokeWidth = goldStroke
                        )
                        drawLine(
                            color = DesignSystem.PanelShadow,
                            start = Offset(0f, barHeight + shadowStroke / 2f),
                            end = Offset(barWidth, barHeight + shadowStroke / 2f),
                            strokeWidth = shadowStroke
                        )
                    }
            )
        },
        containerColor = DesignSystem.Background
    ) { paddingValues ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues)
                .padding(16.dp)
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            // Audio Device Info
            SectionCard("Audio Device") {
                if (isEngineRunning) {
                    InfoRow("Sample Rate", "$sampleRate Hz")
                    InfoRow("Buffer Size", "$framesPerBuffer frames")
                    InfoRow(
                        "Buffer Duration",
                        if (sampleRate > 0) {
                            "%.1f ms".format(
                                framesPerBuffer.toFloat() / sampleRate * 1000f
                            )
                        } else "--"
                    )
                } else {
                    Text(
                        "Start the engine to see device info",
                        color = DesignSystem.TextSecondary,
                        fontSize = 14.sp
                    )
                }
            }

            // Buffer Size
            SectionCard("Buffer Size") {
                Text(
                    "Buffer multiplier controls the trade-off between latency and stability. " +
                        "Lower values reduce latency but may cause audio glitches on slower devices.",
                    color = DesignSystem.TextSecondary,
                    fontSize = 12.sp,
                    lineHeight = 16.sp
                )
                Spacer(modifier = Modifier.height(12.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text(
                        "Multiplier: ${bufferMultiplier}x",
                        color = DesignSystem.TextPrimary,
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Medium
                    )
                    val label = when (bufferMultiplier) {
                        1 -> "Ultra Low"
                        2 -> "Low (default)"
                        3, 4 -> "Medium"
                        else -> "High"
                    }
                    Text(label, color = DesignSystem.SafeGreen, fontSize = 13.sp)
                }
                Slider(
                    value = bufferMultiplier.toFloat(),
                    onValueChange = { onBufferMultiplierChanged(it.toInt()) },
                    valueRange = 1f..8f,
                    steps = 6,
                    colors = SliderDefaults.colors(
                        thumbColor = DesignSystem.SafeGreen,
                        activeTrackColor = DesignSystem.SafeGreen,
                        inactiveTrackColor = DesignSystem.ModuleBorder
                    )
                )
                if (isEngineRunning && sampleRate > 0) {
                    val totalFrames = framesPerBuffer * bufferMultiplier
                    val bufferMs = totalFrames.toFloat() / sampleRate * 1000f
                    RecessedValueDisplay(
                        "Buffer: $totalFrames frames (%.1f ms)".format(bufferMs)
                    )
                }
            }

            // Input Source
            SectionCard("Input Source") {
                Text(
                    "Select the audio input device. Auto-detection picks the " +
                        "best available source, but you can override it manually.",
                    color = DesignSystem.TextSecondary,
                    fontSize = 12.sp,
                    lineHeight = 16.sp
                )
                Spacer(modifier = Modifier.height(12.dp))
                InputSourceOption(
                    label = "Phone Microphone",
                    description = "Built-in mic (demo quality)",
                    source = InputSourceManager.InputSource.PHONE_MIC,
                    selectedSource = currentInputSource,
                    onSelect = onInputSourceSelected
                )
                InputSourceOption(
                    label = "Analog Adapter",
                    description = "TRRS / iRig-style cable",
                    source = InputSourceManager.InputSource.ANALOG_ADAPTER,
                    selectedSource = currentInputSource,
                    onSelect = onInputSourceSelected
                )
                InputSourceOption(
                    label = "USB Audio",
                    description = "USB interface (lowest latency)",
                    source = InputSourceManager.InputSource.USB_AUDIO,
                    selectedSource = currentInputSource,
                    onSelect = onInputSourceSelected
                )
            }

            // Latency
            SectionCard("Latency") {
                if (isEngineRunning && estimatedLatencyMs > 0f) {
                    InfoRow(
                        "Round-trip Estimate",
                        "%.1f ms".format(estimatedLatencyMs)
                    )

                    Spacer(modifier = Modifier.height(8.dp))

                    // Latency quality indicator
                    val (quality, qualityColor) = when {
                        estimatedLatencyMs < 15f -> "Excellent" to DesignSystem.SafeGreen
                        estimatedLatencyMs < 25f -> "Good" to DesignSystem.MeterYellow
                        estimatedLatencyMs < 40f -> "Acceptable" to DesignSystem.WarningAmber
                        else -> "High" to DesignSystem.ClipRed
                    }
                    Text(
                        quality,
                        color = qualityColor,
                        fontSize = 16.sp,
                        fontWeight = FontWeight.Bold
                    )

                    Spacer(modifier = Modifier.height(4.dp))
                    Text(
                        "For real-time guitar processing, aim for <20ms round-trip latency. " +
                            "USB audio interfaces typically achieve the lowest latency.",
                        color = DesignSystem.TextSecondary,
                        fontSize = 12.sp,
                        lineHeight = 16.sp
                    )
                } else {
                    Text(
                        "Start the engine to measure latency",
                        color = DesignSystem.TextSecondary,
                        fontSize = 14.sp
                    )
                }
            }

            // Battery Optimization
            SectionCard("Battery") {
                if (isBatteryOptimized) {
                    Text(
                        "Battery optimization is disabled for this app",
                        color = DesignSystem.SafeGreen,
                        fontSize = 14.sp
                    )
                    Spacer(modifier = Modifier.height(4.dp))
                    Text(
                        "Audio processing will continue reliably when the screen is off.",
                        color = DesignSystem.TextSecondary,
                        fontSize = 12.sp
                    )
                } else {
                    Text(
                        "Battery optimization may interrupt audio",
                        color = DesignSystem.WarningAmber,
                        fontSize = 14.sp
                    )
                    Spacer(modifier = Modifier.height(4.dp))
                    Text(
                        "When battery optimization is enabled, the system may pause " +
                            "audio processing when the screen turns off. Disable it " +
                            "for uninterrupted guitar processing.",
                        color = DesignSystem.TextSecondary,
                        fontSize = 12.sp,
                        lineHeight = 16.sp
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    Button(
                        onClick = onRequestBatteryExemption,
                        colors = ButtonDefaults.buttonColors(
                            containerColor = DesignSystem.SafeGreen,
                            contentColor = Color.Black
                        )
                    ) {
                        Text("Disable Battery Optimization")
                    }
                }

                Spacer(modifier = Modifier.height(12.dp))
                Text(
                    "Some manufacturers (Samsung, Xiaomi, Huawei) have additional " +
                        "battery restrictions. Visit dontkillmyapp.com for device-specific " +
                        "instructions.",
                    color = DesignSystem.TextSecondary,
                    fontSize = 11.sp,
                    lineHeight = 15.sp
                )
            }

            // About
            SectionCard("About") {
                InfoRow("App", "Guitar Emulator")
                InfoRow("Effects", "${com.guitaremulator.app.data.EffectRegistry.EFFECT_COUNT} (DSP chain)")
                InfoRow("Audio API", "Oboe (AAudio/OpenSL ES)")
                InfoRow("Architecture", "C++ DSP + Kotlin UI")
            }
        }
    }
}

/**
 * Section card styled as an amp-panel module.
 *
 * Features a condensed monospace ALL-CAPS section header with a subtle
 * gold-piping underline, [ControlPanelSurface] background, and a
 * double-line machined edge border (outer [PanelShadow], inner [PanelHighlight])
 * drawn via [drawBehind] for a beveled metal panel illusion.
 */
@Composable
private fun SectionCard(
    title: String,
    content: @Composable () -> Unit
) {
    // Pre-resolve colors outside the draw lambda
    val panelShadow = DesignSystem.PanelShadow
    val panelHighlight = DesignSystem.PanelHighlight
    val cornerRadiusDp = DesignSystem.ModuleCornerRadius
    val borderWidth = DesignSystem.PanelBorderWidth

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(cornerRadiusDp))
            .background(DesignSystem.ControlPanelSurface)
            .drawBehind {
                // Double-line machined edge: outer PanelShadow, inner PanelHighlight
                val outerStroke = borderWidth.toPx()
                val innerStroke = outerStroke * 0.6f
                val cornerPx = cornerRadiusDp.toPx()

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
            .padding(16.dp)
    ) {
        // Section header: condensed monospace ALL CAPS with gold underline
        Text(
            text = title.uppercase(),
            color = DesignSystem.CreamWhite,
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            letterSpacing = 2.sp
        )
        // Gold piping underline at 40% alpha
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 6.dp, bottom = 12.dp)
                .height(1.dp)
                .background(DesignSystem.GoldPiping.copy(alpha = 0.40f))
        )
        content()
    }
}

/**
 * Information row showing a label and its value.
 *
 * The value is displayed in a recessed panel (AmpChassisGray background,
 * PanelShadow border, 4dp corner radius) with monospace font in CreamWhite.
 */
@Composable
private fun InfoRow(label: String, value: String) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(label, color = DesignSystem.TextSecondary, fontSize = 14.sp)
        RecessedValueDisplay(value)
    }
}

/**
 * Small recessed panel for numeric/value readouts.
 *
 * AmpChassisGray background with PanelShadow border, 4dp corner radius,
 * monospace CreamWhite text -- simulates an inset LCD display panel on
 * vintage amp hardware.
 */
@Composable
private fun RecessedValueDisplay(text: String) {
    Box(
        modifier = Modifier
            .background(
                color = DesignSystem.AmpChassisGray,
                shape = RoundedCornerShape(4.dp)
            )
            .border(
                width = 1.dp,
                color = DesignSystem.PanelShadow,
                shape = RoundedCornerShape(4.dp)
            )
            .padding(horizontal = 8.dp, vertical = 3.dp)
    ) {
        Text(
            text = text,
            color = DesignSystem.CreamWhite,
            fontSize = 14.sp,
            fontWeight = FontWeight.Medium,
            fontFamily = FontFamily.Monospace
        )
    }
}

@Composable
private fun InputSourceOption(
    label: String,
    description: String,
    source: InputSourceManager.InputSource,
    selectedSource: InputSourceManager.InputSource,
    onSelect: (InputSourceManager.InputSource) -> Unit
) {
    val isSelected = source == selectedSource
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
            .clickable { onSelect(source) }
            .padding(vertical = 6.dp, horizontal = 4.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        RadioButton(
            selected = isSelected,
            onClick = { onSelect(source) },
            colors = RadioButtonDefaults.colors(
                selectedColor = DesignSystem.SafeGreen,
                unselectedColor = DesignSystem.TextMuted
            )
        )
        Column(modifier = Modifier.padding(start = 4.dp)) {
            Text(
                text = label,
                color = if (isSelected) DesignSystem.TextPrimary else DesignSystem.TextSecondary,
                fontSize = 14.sp,
                fontWeight = if (isSelected) FontWeight.Medium else FontWeight.Normal
            )
            Text(
                text = description,
                color = DesignSystem.TextSecondary,
                fontSize = 12.sp
            )
        }
    }
}
