package com.guitaremulator.app.ui.components

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FilterChipDefaults
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.data.PresetCategory
import com.guitaremulator.app.ui.theme.DesignSystem

/**
 * Dialog for saving the current effect chain as a new preset.
 *
 * Provides a text field for the preset name and a row of category filter chips.
 *
 * @param initialName Pre-filled name (from the current preset, if any).
 * @param onDismiss Callback when the dialog is dismissed.
 * @param onSave Callback with the chosen name and category.
 */
@Composable
internal fun SavePresetDialog(
    initialName: String,
    onDismiss: () -> Unit,
    onSave: (String, PresetCategory) -> Unit
) {
    var presetName by remember { mutableStateOf(initialName) }
    var selectedCategory by remember { mutableStateOf(PresetCategory.CUSTOM) }

    AlertDialog(
        onDismissRequest = onDismiss,
        containerColor = DesignSystem.ModuleSurface,
        title = {
            Text(
                text = "Save Preset",
                color = Color.White,
                fontWeight = FontWeight.Bold
            )
        },
        text = {
            Column {
                OutlinedTextField(
                    value = presetName,
                    onValueChange = { presetName = it.take(100) },
                    label = { Text("Preset Name") },
                    singleLine = true,
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedTextColor = Color.White,
                        unfocusedTextColor = Color(0xFFCCCCCC),
                        focusedBorderColor = DesignSystem.SafeGreen,
                        unfocusedBorderColor = Color(0xFF555555),
                        focusedLabelColor = DesignSystem.SafeGreen,
                        unfocusedLabelColor = Color(0xFF888888),
                        cursorColor = DesignSystem.SafeGreen
                    ),
                    modifier = Modifier.fillMaxWidth()
                )

                Spacer(modifier = Modifier.height(12.dp))

                Text(
                    text = "CATEGORY",
                    fontSize = 10.sp,
                    color = DesignSystem.TextSecondary,
                    fontWeight = FontWeight.Bold,
                    letterSpacing = 1.sp
                )

                Spacer(modifier = Modifier.height(6.dp))

                // Category chips in a flow layout
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    val categories = listOf(
                        PresetCategory.CLEAN to "Clean",
                        PresetCategory.CRUNCH to "Crunch",
                        PresetCategory.HEAVY to "Heavy"
                    )
                    for ((cat, label) in categories) {
                        FilterChip(
                            selected = selectedCategory == cat,
                            onClick = { selectedCategory = cat },
                            label = { Text(label, fontSize = 12.sp) },
                            colors = FilterChipDefaults.filterChipColors(
                                selectedContainerColor = DesignSystem.SafeGreen,
                                selectedLabelColor = Color.Black,
                                containerColor = DesignSystem.ModuleBorder,
                                labelColor = Color(0xFFBBBBBB)
                            )
                        )
                    }
                }

                Spacer(modifier = Modifier.height(4.dp))

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    val categories2 = listOf(
                        PresetCategory.AMBIENT to "Ambient",
                        PresetCategory.ACOUSTIC to "Acoustic",
                        PresetCategory.CUSTOM to "Custom"
                    )
                    for ((cat, label) in categories2) {
                        FilterChip(
                            selected = selectedCategory == cat,
                            onClick = { selectedCategory = cat },
                            label = { Text(label, fontSize = 12.sp) },
                            colors = FilterChipDefaults.filterChipColors(
                                selectedContainerColor = DesignSystem.SafeGreen,
                                selectedLabelColor = Color.Black,
                                containerColor = DesignSystem.ModuleBorder,
                                labelColor = Color(0xFFBBBBBB)
                            )
                        )
                    }
                }
            }
        },
        confirmButton = {
            Button(
                onClick = {
                    val name = presetName.trim()
                    if (name.isNotEmpty()) {
                        onSave(name, selectedCategory)
                    }
                },
                enabled = presetName.trim().isNotEmpty(),
                colors = ButtonDefaults.buttonColors(
                    containerColor = DesignSystem.SafeGreen,
                    contentColor = Color.Black
                )
            ) {
                Text("Save", fontWeight = FontWeight.Bold)
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("Cancel", color = Color(0xFF888888))
            }
        }
    )
}
