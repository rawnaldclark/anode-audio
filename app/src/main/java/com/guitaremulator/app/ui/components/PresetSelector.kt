package com.guitaremulator.app.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowDropDown
import androidx.compose.material.icons.filled.Star
import androidx.compose.material.icons.outlined.Star
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FilterChipDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.data.Preset
import com.guitaremulator.app.data.PresetCategory
import com.guitaremulator.app.ui.theme.DesignSystem

/**
 * Preset selector bar showing the current preset name with a dropdown
 * menu to switch between available presets.
 *
 * The dropdown groups presets: factory presets first, then user presets,
 * separated by a divider.
 *
 * @param currentPresetName Name of the active preset.
 * @param currentPresetId ID of the active preset.
 * @param presets All available presets.
 * @param onPresetSelected Callback with preset ID when a preset is chosen.
 * @param onSavePreset Callback to trigger saving a new preset.
 * @param onOverwritePreset Callback to overwrite an existing preset.
 * @param onRenamePreset Callback to rename a preset.
 * @param onDeletePreset Callback to delete a preset.
 * @param onExportPreset Callback to export the current preset.
 * @param onImportPreset Callback to import a preset.
 * @param onToggleFavorite Callback to toggle a preset's favorite status (by ID).
 */
@Composable
internal fun PresetBar(
    currentPresetName: String,
    currentPresetId: String,
    presets: List<Preset>,
    onPresetSelected: (String) -> Unit,
    onSavePreset: () -> Unit,
    onOverwritePreset: (String) -> Unit,
    onRenamePreset: (String, String) -> Unit,
    onDeletePreset: (String) -> Unit,
    onExportPreset: () -> Unit,
    onImportPreset: () -> Unit,
    onToggleFavorite: (String) -> Unit = {}
) {
    var expanded by remember { mutableStateOf(false) }

    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(10.dp))
            .background(DesignSystem.ModuleSurface)
    ) {
        TextButton(
            onClick = { expanded = true },
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 4.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = "PRESET",
                        fontSize = 10.sp,
                        color = DesignSystem.TextSecondary,
                        fontWeight = FontWeight.Bold,
                        letterSpacing = 1.sp
                    )
                    Text(
                        text = currentPresetName.ifEmpty { "No Preset" },
                        fontSize = 16.sp,
                        fontWeight = FontWeight.SemiBold,
                        color = DesignSystem.TextPrimary,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                }
                Icon(
                    imageVector = Icons.Default.ArrowDropDown,
                    contentDescription = "Select preset",
                    tint = DesignSystem.TextSecondary
                )
            }
        }

        // Category filter and search state for preset browsing
        var selectedFilter by remember { mutableStateOf<PresetCategory?>(null) }
        var searchQuery by remember { mutableStateOf("") }
        var showFavoritesOnly by remember { mutableStateOf(false) }

        DropdownMenu(
            expanded = expanded,
            onDismissRequest = {
                expanded = false
                selectedFilter = null
                searchQuery = ""
                showFavoritesOnly = false
            },
            modifier = Modifier
                .background(Color(0xFF262630))
        ) {
            // Search field
            OutlinedTextField(
                value = searchQuery,
                onValueChange = { searchQuery = it },
                placeholder = { Text("Search presets...", fontSize = 13.sp, color = DesignSystem.TextMuted) },
                singleLine = true,
                colors = OutlinedTextFieldDefaults.colors(
                    focusedTextColor = Color.White,
                    unfocusedTextColor = Color(0xFFCCCCCC),
                    focusedBorderColor = DesignSystem.SafeGreen,
                    unfocusedBorderColor = DesignSystem.ModuleBorder,
                    cursorColor = DesignSystem.SafeGreen
                ),
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 8.dp, vertical = 4.dp)
                    .height(44.dp)
            )

            // Category filter chips
            Row(
                modifier = Modifier
                    .padding(horizontal = 8.dp, vertical = 4.dp),
                horizontalArrangement = Arrangement.spacedBy(4.dp)
            ) {
                FilterChip(
                    selected = showFavoritesOnly,
                    onClick = {
                        showFavoritesOnly = !showFavoritesOnly
                        if (showFavoritesOnly) selectedFilter = null
                    },
                    label = {
                        Icon(
                            imageVector = if (showFavoritesOnly) Icons.Filled.Star else Icons.Outlined.Star,
                            contentDescription = "Favorites",
                            tint = if (showFavoritesOnly) Color.Black else Color(0xFFFFD600)
                        )
                    },
                    colors = FilterChipDefaults.filterChipColors(
                        selectedContainerColor = Color(0xFFFFD600),
                        selectedLabelColor = Color.Black,
                        containerColor = Color(0xFF3A3A3A),
                        labelColor = Color(0xFFBBBBBB)
                    ),
                    modifier = Modifier.height(28.dp)
                )
                FilterChip(
                    selected = selectedFilter == null && !showFavoritesOnly,
                    onClick = {
                        selectedFilter = null
                        showFavoritesOnly = false
                    },
                    label = { Text("All", fontSize = 11.sp) },
                    colors = FilterChipDefaults.filterChipColors(
                        selectedContainerColor = DesignSystem.SafeGreen,
                        selectedLabelColor = Color.Black,
                        containerColor = Color(0xFF3A3A3A),
                        labelColor = Color(0xFFBBBBBB)
                    ),
                    modifier = Modifier.height(28.dp)
                )
                val chipCategories = listOf(
                    PresetCategory.CLEAN to "Clean",
                    PresetCategory.CRUNCH to "Crunch",
                    PresetCategory.HEAVY to "Heavy",
                    PresetCategory.AMBIENT to "Amb",
                    PresetCategory.ACOUSTIC to "Acou",
                    PresetCategory.CUSTOM to "User"
                )
                for ((cat, label) in chipCategories) {
                    FilterChip(
                        selected = selectedFilter == cat && !showFavoritesOnly,
                        onClick = {
                            showFavoritesOnly = false
                            selectedFilter = if (selectedFilter == cat) null else cat
                        },
                        label = { Text(label, fontSize = 11.sp) },
                        colors = FilterChipDefaults.filterChipColors(
                            selectedContainerColor = DesignSystem.SafeGreen,
                            selectedLabelColor = Color.Black,
                            containerColor = Color(0xFF3A3A3A),
                            labelColor = Color(0xFFBBBBBB)
                        ),
                        modifier = Modifier.height(28.dp)
                    )
                }
            }

            HorizontalDivider(color = DesignSystem.ModuleBorder)

            // Filter presets by favorites, search query, and selected category
            val filtered = presets.filter { preset ->
                (!showFavoritesOnly || preset.isFavorite) &&
                (selectedFilter == null || preset.category == selectedFilter) &&
                (searchQuery.isBlank() || preset.name.contains(searchQuery, ignoreCase = true))
            }
            val factoryPresets = filtered.filter { it.isFactory }
            val userPresets = filtered.filter { !it.isFactory }

            if (factoryPresets.isNotEmpty()) {
                // Factory presets header
                DropdownMenuItem(
                    text = {
                        Text(
                            text = "FACTORY",
                            fontSize = 11.sp,
                            color = DesignSystem.TextMuted,
                            fontWeight = FontWeight.Bold,
                            letterSpacing = 1.sp
                        )
                    },
                    onClick = {},
                    enabled = false
                )

                for (preset in factoryPresets) {
                    DropdownMenuItem(
                        text = {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                IconButton(
                                    onClick = { onToggleFavorite(preset.id) },
                                    modifier = Modifier.padding(end = 4.dp)
                                ) {
                                    Icon(
                                        imageVector = if (preset.isFavorite) Icons.Filled.Star else Icons.Outlined.Star,
                                        contentDescription = if (preset.isFavorite) "Remove from favorites" else "Add to favorites",
                                        tint = if (preset.isFavorite) Color(0xFFFFD600) else Color(0xFF666666)
                                    )
                                }
                                Text(
                                    text = preset.name,
                                    color = Color.White,
                                    fontSize = 14.sp,
                                    modifier = Modifier.weight(1f)
                                )
                                Text(
                                    text = preset.category.name.lowercase()
                                        .replaceFirstChar { it.uppercase() },
                                    fontSize = 10.sp,
                                    color = DesignSystem.TextMuted
                                )
                            }
                        },
                        onClick = {
                            onPresetSelected(preset.id)
                            expanded = false
                            selectedFilter = null
                            searchQuery = ""
                            showFavoritesOnly = false
                        }
                    )
                }
            }

            if (factoryPresets.isNotEmpty() && userPresets.isNotEmpty()) {
                HorizontalDivider(color = DesignSystem.ModuleBorder)
            }

            if (userPresets.isNotEmpty()) {
                // User presets header
                DropdownMenuItem(
                    text = {
                        Text(
                            text = "USER",
                            fontSize = 11.sp,
                            color = DesignSystem.TextMuted,
                            fontWeight = FontWeight.Bold,
                            letterSpacing = 1.sp
                        )
                    },
                    onClick = {},
                    enabled = false
                )

                for (preset in userPresets) {
                    DropdownMenuItem(
                        text = {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                IconButton(
                                    onClick = { onToggleFavorite(preset.id) },
                                    modifier = Modifier.padding(end = 4.dp)
                                ) {
                                    Icon(
                                        imageVector = if (preset.isFavorite) Icons.Filled.Star else Icons.Outlined.Star,
                                        contentDescription = if (preset.isFavorite) "Remove from favorites" else "Add to favorites",
                                        tint = if (preset.isFavorite) Color(0xFFFFD600) else Color(0xFF666666)
                                    )
                                }
                                Text(
                                    text = preset.name,
                                    color = Color.White,
                                    fontSize = 14.sp,
                                    modifier = Modifier.weight(1f)
                                )
                                Text(
                                    text = preset.category.name.lowercase()
                                        .replaceFirstChar { it.uppercase() },
                                    fontSize = 10.sp,
                                    color = DesignSystem.TextMuted
                                )
                            }
                        },
                        onClick = {
                            onPresetSelected(preset.id)
                            expanded = false
                            selectedFilter = null
                            searchQuery = ""
                            showFavoritesOnly = false
                        }
                    )
                }
            }

            if (filtered.isEmpty()) {
                DropdownMenuItem(
                    text = {
                        Text(
                            text = when {
                                searchQuery.isNotBlank() && selectedFilter != null ->
                                    "No ${selectedFilter!!.name.lowercase()} presets matching \"$searchQuery\""
                                searchQuery.isNotBlank() ->
                                    "No presets matching \"$searchQuery\""
                                selectedFilter != null ->
                                    "No ${selectedFilter!!.name.lowercase()} presets"
                                else -> "No presets available"
                            },
                            color = DesignSystem.TextSecondary,
                            fontSize = 14.sp
                        )
                    },
                    onClick = {},
                    enabled = false
                )
            }

            // Preset management actions
            HorizontalDivider(color = DesignSystem.ModuleBorder)

            DropdownMenuItem(
                text = {
                    Text(
                        text = "Save As New...",
                        color = Color(0xFF80DEEA),
                        fontSize = 14.sp
                    )
                },
                onClick = {
                    expanded = false
                    onSavePreset()
                }
            )

            // Show overwrite only for non-factory user presets
            val currentPreset = presets.find { it.id == currentPresetId }
            if (currentPreset != null && !currentPreset.isFactory) {
                DropdownMenuItem(
                    text = {
                        Text(
                            text = "Overwrite \"${currentPreset.name}\"",
                            color = Color(0xFFFFCC80),
                            fontSize = 14.sp,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis
                        )
                    },
                    onClick = {
                        expanded = false
                        onOverwritePreset(currentPresetId)
                    }
                )

                DropdownMenuItem(
                    text = {
                        Text(
                            text = "Delete \"${currentPreset.name}\"",
                            color = Color(0xFFEF9A9A),
                            fontSize = 14.sp,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis
                        )
                    },
                    onClick = {
                        expanded = false
                        onDeletePreset(currentPresetId)
                    }
                )
            }

            // Import / Export actions
            HorizontalDivider(color = DesignSystem.ModuleBorder)

            DropdownMenuItem(
                text = {
                    Text(
                        text = "Import Preset...",
                        color = Color(0xFF80DEEA),
                        fontSize = 14.sp
                    )
                },
                onClick = {
                    expanded = false
                    onImportPreset()
                }
            )

            DropdownMenuItem(
                text = {
                    Text(
                        text = "Export Current...",
                        color = Color(0xFF80DEEA),
                        fontSize = 14.sp
                    )
                },
                onClick = {
                    expanded = false
                    onExportPreset()
                },
                enabled = currentPresetName.isNotEmpty()
            )
        }
    }
}
