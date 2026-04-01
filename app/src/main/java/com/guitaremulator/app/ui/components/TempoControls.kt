package com.guitaremulator.app.ui.components

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
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
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TextField
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.ui.theme.DesignSystem

// -- Bar count options --------------------------------------------------------

private val BAR_COUNT_OPTIONS = listOf(4, 8, 16, 32)

// -- Beats per bar options ----------------------------------------------------

private val BEATS_PER_BAR_OPTIONS = listOf(2, 3, 4, 5, 6, 7)

/**
 * BPM and quantization controls for the looper.
 *
 * Provides:
 * 1. **BPM display**: Large monospace readout of the current tempo, tappable
 *    to open a manual BPM entry dialog. Flanked by +/- buttons for fine
 *    adjustment (increment by 1 BPM per tap).
 * 2. **Bar count selector**: Row of selectable chips (4, 8, 16, 32 bars)
 *    for quantized loop length.
 * 3. **Beats per bar selector**: Row of selectable chips (2-7 beats) for
 *    the time signature numerator.
 * 4. **Tap Tempo button**: Full-width button that sends tap events for
 *    BPM detection. Provides haptic feedback and a press animation.
 *
 * @param bpm               Current BPM value.
 * @param barCount          Current number of bars for quantized mode.
 * @param beatsPerBar       Current beats per bar (time signature numerator).
 * @param isQuantizedMode   Whether quantized mode is active (affects chip visibility).
 * @param onBpmChange       Callback when BPM changes (from +/- or manual entry).
 * @param onBarCountChange  Callback when bar count selection changes.
 * @param onBeatsPerBarChange Callback when beats per bar selection changes.
 * @param onTapTempo        Callback when the tap tempo button is pressed.
 * @param modifier          Layout modifier.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun TempoControls(
    bpm: Float,
    barCount: Int,
    beatsPerBar: Int,
    isQuantizedMode: Boolean,
    onBpmChange: (Float) -> Unit,
    onBarCountChange: (Int) -> Unit,
    onBeatsPerBarChange: (Int) -> Unit,
    onTapTempo: () -> Unit,
    modifier: Modifier = Modifier
) {
    val hapticFeedback = LocalHapticFeedback.current
    var showBpmDialog by remember { mutableStateOf(false) }

    Column(
        modifier = modifier.fillMaxWidth(),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // ── 1. BPM display with +/- controls ────────────────────────────

        SectionLabel(text = "TEMPO")

        Spacer(modifier = Modifier.height(4.dp))

        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.Center,
            modifier = Modifier.fillMaxWidth()
        ) {
            // Minus button
            BpmAdjustButton(
                label = "-",
                onClick = {
                    hapticFeedback.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                    onBpmChange((bpm - 1f).coerceAtLeast(30f))
                }
            )

            Spacer(modifier = Modifier.width(16.dp))

            // BPM readout (tappable to edit manually)
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                modifier = Modifier.clickable(
                    interactionSource = remember { MutableInteractionSource() },
                    indication = null,
                    onClick = { showBpmDialog = true }
                )
            ) {
                Text(
                    text = String.format("%.0f", bpm),
                    fontSize = 36.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace,
                    color = DesignSystem.TextPrimary,
                    textAlign = TextAlign.Center
                )
                Text(
                    text = "BPM",
                    fontSize = 11.sp,
                    fontWeight = FontWeight.Medium,
                    letterSpacing = 2.sp,
                    color = DesignSystem.TextSecondary,
                    textAlign = TextAlign.Center
                )
            }

            Spacer(modifier = Modifier.width(16.dp))

            // Plus button
            BpmAdjustButton(
                label = "+",
                onClick = {
                    hapticFeedback.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                    onBpmChange((bpm + 1f).coerceAtMost(300f))
                }
            )
        }

        Spacer(modifier = Modifier.height(16.dp))

        // ── 2. Bar count selector (only shown in quantized mode) ────────

        if (isQuantizedMode) {
            SectionLabel(text = "BARS")

            Spacer(modifier = Modifier.height(4.dp))

            FlowRow(
                horizontalArrangement = Arrangement.spacedBy(8.dp, Alignment.CenterHorizontally),
                modifier = Modifier.fillMaxWidth()
            ) {
                BAR_COUNT_OPTIONS.forEach { option ->
                    SelectableChip(
                        label = option.toString(),
                        selected = option == barCount,
                        onClick = { onBarCountChange(option) }
                    )
                }
            }

            Spacer(modifier = Modifier.height(12.dp))
        }

        // ── 3. Beats per bar selector ───────────────────────────────────

        SectionLabel(text = "BEATS / BAR")

        Spacer(modifier = Modifier.height(4.dp))

        FlowRow(
            horizontalArrangement = Arrangement.spacedBy(8.dp, Alignment.CenterHorizontally),
            modifier = Modifier.fillMaxWidth()
        ) {
            BEATS_PER_BAR_OPTIONS.forEach { option ->
                SelectableChip(
                    label = option.toString(),
                    selected = option == beatsPerBar,
                    onClick = { onBeatsPerBarChange(option) }
                )
            }
        }

        Spacer(modifier = Modifier.height(16.dp))

        // ── 4. Tap Tempo button ─────────────────────────────────────────

        TapTempoButton(
            onTap = {
                hapticFeedback.performHapticFeedback(HapticFeedbackType.LongPress)
                onTapTempo()
            },
            modifier = Modifier.fillMaxWidth()
        )
    }

    // ── BPM manual entry dialog ─────────────────────────────────────────

    if (showBpmDialog) {
        BpmInputDialog(
            currentBpm = bpm,
            onDismiss = { showBpmDialog = false },
            onConfirm = { newBpm ->
                showBpmDialog = false
                onBpmChange(newBpm)
            }
        )
    }
}

// -- Private sub-components ---------------------------------------------------

/**
 * Small section label in muted uppercase text.
 *
 * Used to label control groups (TEMPO, BARS, BEATS/BAR) within the
 * tempo controls panel.
 *
 * @param text The label text to display.
 */
@Composable
private fun SectionLabel(text: String) {
    Text(
        text = text,
        fontSize = 10.sp,
        fontWeight = FontWeight.Bold,
        letterSpacing = 2.sp,
        color = DesignSystem.TextMuted,
        textAlign = TextAlign.Center,
        modifier = Modifier.fillMaxWidth()
    )
}

/**
 * Circular +/- button for BPM fine adjustment.
 *
 * Renders a 36dp circle with the given label character centered inside.
 * Includes press-scale animation matching the StompSwitch style.
 *
 * @param label   Single character to display (e.g., "+" or "-").
 * @param onClick Callback when pressed.
 */
@Composable
private fun BpmAdjustButton(
    label: String,
    onClick: () -> Unit
) {
    val interactionSource = remember { MutableInteractionSource() }
    val isPressed by interactionSource.collectIsPressedAsState()

    val scale by animateFloatAsState(
        targetValue = if (isPressed) 0.9f else 1.0f,
        animationSpec = spring(dampingRatio = 0.55f, stiffness = 600f),
        label = "bpmAdjustScale"
    )

    Box(
        contentAlignment = Alignment.Center,
        modifier = Modifier
            .size(36.dp)
            .graphicsLayer {
                scaleX = scale
                scaleY = scale
            }
            .clip(CircleShape)
            .background(DesignSystem.ModuleBorder)
            .clickable(
                interactionSource = interactionSource,
                indication = null,
                onClick = onClick
            )
    ) {
        Text(
            text = label,
            fontSize = 20.sp,
            fontWeight = FontWeight.Bold,
            color = DesignSystem.TextPrimary,
            textAlign = TextAlign.Center
        )
    }
}

/**
 * Selectable chip for bar count and beats per bar options.
 *
 * A compact rounded-rect button that visually indicates selection state
 * with color changes. Selected chips use the app's safe green accent;
 * unselected chips use the subdued module surface color.
 *
 * @param label    Text to display inside the chip.
 * @param selected Whether this chip is currently selected.
 * @param onClick  Callback when the chip is tapped.
 */
@Composable
private fun SelectableChip(
    label: String,
    selected: Boolean,
    onClick: () -> Unit
) {
    val bgColor = if (selected) DesignSystem.SafeGreen.copy(alpha = 0.2f) else DesignSystem.ModuleBorder
    val borderColor = if (selected) DesignSystem.SafeGreen else Color.Transparent
    val textColor = if (selected) DesignSystem.SafeGreen else DesignSystem.TextSecondary

    Box(
        contentAlignment = Alignment.Center,
        modifier = Modifier
            .clip(RoundedCornerShape(8.dp))
            .background(bgColor)
            .clickable(onClick = onClick)
            .padding(horizontal = 16.dp, vertical = 8.dp)
    ) {
        Text(
            text = label,
            fontSize = 14.sp,
            fontWeight = if (selected) FontWeight.Bold else FontWeight.Normal,
            fontFamily = FontFamily.Monospace,
            color = textColor,
            textAlign = TextAlign.Center
        )
    }
}

/**
 * Full-width Tap Tempo button.
 *
 * A large button with "TAP" text that provides haptic feedback and a
 * press-scale animation. Used for tap-tempo BPM detection.
 *
 * @param onTap    Callback when the button is tapped.
 * @param modifier Layout modifier.
 */
@Composable
private fun TapTempoButton(
    onTap: () -> Unit,
    modifier: Modifier = Modifier
) {
    val interactionSource = remember { MutableInteractionSource() }
    val isPressed by interactionSource.collectIsPressedAsState()

    val scale by animateFloatAsState(
        targetValue = if (isPressed) 0.97f else 1.0f,
        animationSpec = spring(dampingRatio = 0.55f, stiffness = 600f),
        label = "tapTempoScale"
    )

    Box(
        contentAlignment = Alignment.Center,
        modifier = modifier
            .height(48.dp)
            .graphicsLayer {
                scaleX = scale
                scaleY = scale
            }
            .clip(RoundedCornerShape(12.dp))
            .background(DesignSystem.ModuleBorder)
            .clickable(
                interactionSource = interactionSource,
                indication = null,
                onClick = onTap
            )
    ) {
        Text(
            text = "TAP TEMPO",
            fontSize = 16.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            letterSpacing = 3.sp,
            color = DesignSystem.TextPrimary,
            textAlign = TextAlign.Center
        )
    }
}

/**
 * Alert dialog for manual BPM number input.
 *
 * Presents a text field pre-filled with the current BPM value. The user
 * can type a new value and confirm. Input is validated to ensure it falls
 * within the 30-300 BPM range.
 *
 * @param currentBpm Current BPM value to pre-fill the text field.
 * @param onDismiss  Callback when the dialog is cancelled.
 * @param onConfirm  Callback with the new BPM value when confirmed.
 */
@Composable
private fun BpmInputDialog(
    currentBpm: Float,
    onDismiss: () -> Unit,
    onConfirm: (Float) -> Unit
) {
    var textValue by remember { mutableStateOf(String.format("%.0f", currentBpm)) }
    var isError by remember { mutableStateOf(false) }

    AlertDialog(
        onDismissRequest = onDismiss,
        containerColor = DesignSystem.ModuleSurface,
        title = {
            Text(
                "Set BPM",
                color = DesignSystem.TextPrimary,
                fontWeight = FontWeight.Bold
            )
        },
        text = {
            Column {
                Text(
                    "Enter tempo (30-300 BPM):",
                    color = DesignSystem.TextSecondary,
                    fontSize = 14.sp
                )
                Spacer(modifier = Modifier.height(8.dp))
                TextField(
                    value = textValue,
                    onValueChange = { newValue ->
                        // Allow only digits and one decimal point
                        if (newValue.isEmpty() || newValue.matches(Regex("^\\d{0,3}\\.?\\d{0,1}$"))) {
                            textValue = newValue
                            val parsed = newValue.toFloatOrNull()
                            isError = parsed == null || parsed < 30f || parsed > 300f
                        }
                    },
                    isError = isError,
                    singleLine = true,
                    textStyle = TextStyle(
                        fontSize = 24.sp,
                        fontFamily = FontFamily.Monospace,
                        fontWeight = FontWeight.Bold,
                        color = DesignSystem.TextPrimary,
                        textAlign = TextAlign.Center
                    ),
                    keyboardOptions = KeyboardOptions(
                        keyboardType = KeyboardType.Number,
                        imeAction = ImeAction.Done
                    ),
                    keyboardActions = KeyboardActions(
                        onDone = {
                            val parsed = textValue.toFloatOrNull()
                            if (parsed != null && parsed in 30f..300f) {
                                onConfirm(parsed)
                            }
                        }
                    ),
                    colors = TextFieldDefaults.colors(
                        focusedContainerColor = DesignSystem.Background,
                        unfocusedContainerColor = DesignSystem.Background,
                        cursorColor = DesignSystem.SafeGreen,
                        focusedIndicatorColor = DesignSystem.SafeGreen,
                        unfocusedIndicatorColor = DesignSystem.ModuleBorder,
                        errorIndicatorColor = DesignSystem.ClipRed
                    ),
                    modifier = Modifier.fillMaxWidth()
                )
                if (isError) {
                    Spacer(modifier = Modifier.height(4.dp))
                    Text(
                        "Must be between 30 and 300",
                        color = DesignSystem.ClipRed,
                        fontSize = 12.sp
                    )
                }
            }
        },
        confirmButton = {
            Button(
                onClick = {
                    val parsed = textValue.toFloatOrNull()
                    if (parsed != null && parsed in 30f..300f) {
                        onConfirm(parsed)
                    }
                },
                enabled = !isError && textValue.isNotEmpty(),
                colors = ButtonDefaults.buttonColors(
                    containerColor = DesignSystem.SafeGreen,
                    contentColor = Color.White
                )
            ) {
                Text("Set", fontWeight = FontWeight.Bold)
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("Cancel", color = DesignSystem.TextSecondary)
            }
        }
    )
}
