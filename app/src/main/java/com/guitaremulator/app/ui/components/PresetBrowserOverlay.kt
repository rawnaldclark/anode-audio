package com.guitaremulator.app.ui.components

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.horizontalScroll
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
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Star
import androidx.compose.material.icons.outlined.Star
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
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
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.data.Preset
import com.guitaremulator.app.data.PresetCategory
import com.guitaremulator.app.ui.theme.DesignSystem

/**
 * Full-screen preset browser overlay inspired by Guitar Rig 7's preset browser panel.
 *
 * Replaces the cramped dropdown preset selector with a spacious, scrollable overlay
 * that provides search, category filtering, favorites, section headers (Factory/User),
 * and a bottom action bar for preset management.
 *
 * The overlay animates in with a fade + slide from the top and dismisses the same way.
 * Android back button is intercepted to close the overlay when visible.
 *
 * ## Layout Structure
 * 1. **Header** -- "PRESETS" title + close button (fixed top)
 * 2. **Search Field** -- Full-width text input with search icon
 * 3. **Filter Row** -- Horizontally scrollable chips (Favorites, All, categories)
 * 4. **Preset List** -- LazyColumn with Factory/User sections, favorite stars, active indicator
 * 5. **Bottom Action Bar** -- Import, Save As New, Overwrite, Delete (fixed bottom)
 *
 * Long-pressing a user (non-factory) preset opens a context menu dialog with
 * Rename and Delete options.
 *
 * @param visible Whether the overlay is currently shown. Controls AnimatedVisibility.
 * @param currentPresetId ID of the currently loaded/active preset.
 * @param presets All available presets (factory and user).
 * @param onPresetSelected Callback with preset ID when user taps a preset card.
 * @param onSavePreset Callback to trigger the "Save As New" flow.
 * @param onOverwritePreset Callback to overwrite an existing user preset by ID.
 * @param onRenamePreset Callback with (presetId, newName) to rename a preset.
 * @param onDeletePreset Callback with preset ID to delete a user preset.
 * @param onDuplicatePreset Callback with preset ID to duplicate any preset
 *     (factory or user) as a new user preset with a "(Copy)" name suffix.
 * @param onExportPreset Callback to export the current preset.
 * @param onImportPreset Callback to import a preset file.
 * @param onToggleFavorite Callback with preset ID to toggle favorite status.
 * @param onDismiss Callback to close the overlay.
 * @param modifier Optional modifier applied to the outermost container.
 */
@Composable
fun PresetBrowserOverlay(
    visible: Boolean,
    currentPresetId: String,
    presets: List<Preset>,
    onPresetSelected: (String) -> Unit,
    onSavePreset: () -> Unit,
    onOverwritePreset: (String) -> Unit,
    onRenamePreset: (String, String) -> Unit,
    onDeletePreset: (String) -> Unit,
    onDuplicatePreset: (String) -> Unit,
    onExportPreset: () -> Unit,
    onImportPreset: () -> Unit,
    onToggleFavorite: (String) -> Unit,
    onDismiss: () -> Unit,
    modifier: Modifier = Modifier
) {
    // Intercept Android back button to dismiss the overlay instead of navigating away
    BackHandler(enabled = visible) { onDismiss() }

    AnimatedVisibility(
        visible = visible,
        enter = fadeIn(tween(200)) + slideInVertically(tween(250)) { -it / 4 },
        exit = fadeOut(tween(150)) + slideOutVertically(tween(200)) { -it / 4 }
    ) {
        PresetBrowserContent(
            currentPresetId = currentPresetId,
            presets = presets,
            onPresetSelected = onPresetSelected,
            onSavePreset = onSavePreset,
            onOverwritePreset = onOverwritePreset,
            onRenamePreset = onRenamePreset,
            onDeletePreset = onDeletePreset,
            onDuplicatePreset = onDuplicatePreset,
            onExportPreset = onExportPreset,
            onImportPreset = onImportPreset,
            onToggleFavorite = onToggleFavorite,
            onDismiss = onDismiss,
            modifier = modifier
        )
    }
}

// ---------------------------------------------------------------------------
// Internal content composable (extracted for readability)
// ---------------------------------------------------------------------------

/**
 * The actual overlay content rendered inside AnimatedVisibility.
 *
 * Manages local state for search query, category filter, favorites filter,
 * and context menu (long-press) for user presets.
 */
@Composable
private fun PresetBrowserContent(
    currentPresetId: String,
    presets: List<Preset>,
    onPresetSelected: (String) -> Unit,
    onSavePreset: () -> Unit,
    onOverwritePreset: (String) -> Unit,
    onRenamePreset: (String, String) -> Unit,
    onDeletePreset: (String) -> Unit,
    onDuplicatePreset: (String) -> Unit,
    onExportPreset: () -> Unit,
    onImportPreset: () -> Unit,
    onToggleFavorite: (String) -> Unit,
    onDismiss: () -> Unit,
    modifier: Modifier = Modifier
) {
    // -- Local UI state --
    var searchQuery by remember { mutableStateOf("") }
    var selectedCategory by remember { mutableStateOf<PresetCategory?>(null) }
    var showFavoritesOnly by remember { mutableStateOf(false) }

    // Context menu state: which preset (by ID) has the long-press menu open.
    // Both factory and user presets can open this menu; the dialog adapts
    // its offered actions based on preset.isFactory.
    var contextMenuPresetId by remember { mutableStateOf<String?>(null) }
    // Rename dialog state
    var renamePresetId by remember { mutableStateOf<String?>(null) }
    var renameText by remember { mutableStateOf("") }

    // -- Filter presets --
    val filtered = remember(presets, searchQuery, selectedCategory, showFavoritesOnly) {
        presets.filter { preset ->
            val matchesFavorite = !showFavoritesOnly || preset.isFavorite
            val matchesCategory = selectedCategory == null || preset.category == selectedCategory
            val matchesSearch = searchQuery.isBlank() ||
                preset.name.contains(searchQuery, ignoreCase = true)
            matchesFavorite && matchesCategory && matchesSearch
        }
    }
    val factoryPresets = remember(filtered) { filtered.filter { it.isFactory } }
    val userPresets = remember(filtered) { filtered.filter { !it.isFactory } }

    // Look up the current preset to determine if overwrite/delete should be shown
    val currentPreset = remember(presets, currentPresetId) {
        presets.find { it.id == currentPresetId }
    }

    // -- Main layout --
    Box(
        modifier = modifier
            .fillMaxSize()
            .background(DesignSystem.Background.copy(alpha = 0.97f))
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
            }
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // 1. Header
            BrowserHeader(onDismiss = onDismiss)

            // 2. Search field
            BrowserSearchField(
                query = searchQuery,
                onQueryChange = { searchQuery = it }
            )

            // 3. Filter chips row
            BrowserFilterRow(
                showFavoritesOnly = showFavoritesOnly,
                selectedCategory = selectedCategory,
                onFavoritesToggle = {
                    showFavoritesOnly = !showFavoritesOnly
                    if (showFavoritesOnly) selectedCategory = null
                },
                onCategorySelected = { category ->
                    showFavoritesOnly = false
                    selectedCategory = if (selectedCategory == category) null else category
                },
                onAllSelected = {
                    selectedCategory = null
                    showFavoritesOnly = false
                }
            )

            // 4. Preset list (fills remaining space)
            PresetList(
                factoryPresets = factoryPresets,
                userPresets = userPresets,
                currentPresetId = currentPresetId,
                isEmpty = filtered.isEmpty(),
                onPresetSelected = { id ->
                    onPresetSelected(id)
                    onDismiss()
                },
                onToggleFavorite = onToggleFavorite,
                onLongPressPreset = { id -> contextMenuPresetId = id },
                modifier = Modifier.weight(1f)
            )

            // 5. Bottom action bar
            BottomActionBar(
                currentPreset = currentPreset,
                onImportPreset = onImportPreset,
                onSavePreset = onSavePreset,
                onOverwritePreset = onOverwritePreset,
                onDeletePreset = onDeletePreset,
                onDismiss = onDismiss
            )
        }
    }

    // -- Context menu dialog (long-press on any preset) --
    // Factory presets get Duplicate only; user presets get Rename + Duplicate + Delete.
    if (contextMenuPresetId != null) {
        val targetPreset = presets.find { it.id == contextMenuPresetId }
        if (targetPreset != null) {
            ContextMenuDialog(
                presetName = targetPreset.name,
                isFactory = targetPreset.isFactory,
                onRename = {
                    renameText = targetPreset.name
                    renamePresetId = contextMenuPresetId
                    contextMenuPresetId = null
                },
                onDuplicate = {
                    onDuplicatePreset(contextMenuPresetId!!)
                    contextMenuPresetId = null
                },
                onDelete = {
                    onDeletePreset(contextMenuPresetId!!)
                    contextMenuPresetId = null
                },
                onDismiss = { contextMenuPresetId = null }
            )
        } else {
            // Preset not found (e.g., deleted while menu was open) -- clear state
            contextMenuPresetId = null
        }
    }

    // -- Rename dialog --
    if (renamePresetId != null) {
        RenameDialog(
            currentName = renameText,
            onNameChange = { renameText = it },
            onConfirm = {
                val trimmed = renameText.trim()
                if (trimmed.isNotEmpty()) {
                    onRenamePreset(renamePresetId!!, trimmed)
                }
                renamePresetId = null
                renameText = ""
            },
            onDismiss = {
                renamePresetId = null
                renameText = ""
            }
        )
    }
}

// ---------------------------------------------------------------------------
// Header
// ---------------------------------------------------------------------------

/**
 * Fixed header bar with "PRESETS" title and a close button.
 * Renders a 1dp bottom border using [DesignSystem.Divider].
 */
@Composable
private fun BrowserHeader(onDismiss: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(52.dp)
            .background(DesignSystem.ModuleSurface)
            .drawBehind {
                val w = size.width
                val h = size.height
                // Gold piping bottom border + shadow
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
            .padding(horizontal = 16.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Text(
            text = "PRESETS",
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            letterSpacing = 2.sp,
            color = DesignSystem.CreamWhite
        )
        IconButton(onClick = onDismiss) {
            Icon(
                imageVector = Icons.Default.Close,
                contentDescription = "Close preset browser",
                tint = DesignSystem.TextSecondary
            )
        }
    }
}

// ---------------------------------------------------------------------------
// Search field
// ---------------------------------------------------------------------------

/**
 * Full-width search text field with a leading search icon.
 *
 * @param query Current search text.
 * @param onQueryChange Callback when the user types.
 */
@Composable
private fun BrowserSearchField(
    query: String,
    onQueryChange: (String) -> Unit
) {
    OutlinedTextField(
        value = query,
        onValueChange = onQueryChange,
        placeholder = {
            Text(
                text = "Search presets...",
                fontSize = 14.sp,
                color = DesignSystem.TextMuted
            )
        },
        leadingIcon = {
            Icon(
                imageVector = Icons.Default.Search,
                contentDescription = null,
                tint = DesignSystem.TextSecondary
            )
        },
        singleLine = true,
        colors = OutlinedTextFieldDefaults.colors(
            focusedTextColor = DesignSystem.TextPrimary,
            unfocusedTextColor = DesignSystem.TextPrimary,
            focusedContainerColor = DesignSystem.AmpChassisGray,
            unfocusedContainerColor = DesignSystem.AmpChassisGray,
            focusedBorderColor = DesignSystem.PanelShadow,
            unfocusedBorderColor = DesignSystem.PanelShadow,
            cursorColor = DesignSystem.SafeGreen,
            focusedLeadingIconColor = DesignSystem.SafeGreen,
            unfocusedLeadingIconColor = DesignSystem.TextSecondary
        ),
        shape = RoundedCornerShape(8.dp),
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 12.dp, vertical = 8.dp)
    )
}

// ---------------------------------------------------------------------------
// Filter chips row
// ---------------------------------------------------------------------------

/** Gold color used for the favorites star chip when active. */
private val FavoriteGold = Color(0xFFFFD600)

/**
 * Horizontally scrollable row of filter chips: Favorites star, "All", and
 * one chip per [PresetCategory].
 */
@Composable
private fun BrowserFilterRow(
    showFavoritesOnly: Boolean,
    selectedCategory: PresetCategory?,
    onFavoritesToggle: () -> Unit,
    onCategorySelected: (PresetCategory) -> Unit,
    onAllSelected: () -> Unit
) {
    val chipCategories = listOf(
        PresetCategory.CLEAN to "Clean",
        PresetCategory.CRUNCH to "Crunch",
        PresetCategory.HEAVY to "Heavy",
        PresetCategory.AMBIENT to "Ambient",
        PresetCategory.ACOUSTIC to "Acoustic",
        PresetCategory.CUSTOM to "User"
    )

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .horizontalScroll(rememberScrollState())
            .padding(horizontal = 12.dp, vertical = 4.dp),
        horizontalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        // Favorites star chip
        FilterChip(
            selected = showFavoritesOnly,
            onClick = onFavoritesToggle,
            label = {
                Icon(
                    imageVector = if (showFavoritesOnly) Icons.Filled.Star else Icons.Outlined.Star,
                    contentDescription = "Favorites",
                    tint = if (showFavoritesOnly) Color.Black else FavoriteGold,
                    modifier = Modifier.size(16.dp)
                )
            },
            colors = FilterChipDefaults.filterChipColors(
                selectedContainerColor = FavoriteGold,
                selectedLabelColor = Color.Black,
                containerColor = Color(0xFF3A3A3A),
                labelColor = Color(0xFFBBBBBB)
            ),
            shape = RoundedCornerShape(16.dp),
            modifier = Modifier.height(32.dp)
        )

        // "All" chip
        FilterChip(
            selected = selectedCategory == null && !showFavoritesOnly,
            onClick = onAllSelected,
            label = { Text("All", fontSize = 12.sp) },
            colors = filterChipColors(selected = selectedCategory == null && !showFavoritesOnly),
            shape = RoundedCornerShape(16.dp),
            modifier = Modifier.height(32.dp)
        )

        // Category chips
        for ((category, label) in chipCategories) {
            FilterChip(
                selected = selectedCategory == category && !showFavoritesOnly,
                onClick = { onCategorySelected(category) },
                label = { Text(label, fontSize = 12.sp) },
                colors = filterChipColors(selected = selectedCategory == category && !showFavoritesOnly),
                shape = RoundedCornerShape(16.dp),
                modifier = Modifier.height(32.dp)
            )
        }
    }
}

/**
 * Returns consistent [FilterChipDefaults] colors for category filter chips.
 * Selected chips use [DesignSystem.SafeGreen] background with black text.
 */
@Composable
private fun filterChipColors(selected: Boolean) = FilterChipDefaults.filterChipColors(
    selectedContainerColor = DesignSystem.SafeGreen,
    selectedLabelColor = Color.Black,
    containerColor = Color(0xFF3A3A3A),
    labelColor = Color(0xFFBBBBBB)
)

// ---------------------------------------------------------------------------
// Preset list
// ---------------------------------------------------------------------------

/**
 * Scrollable list of presets divided into Factory and User sections.
 *
 * Each preset card shows a favorite star, name, category label, and a
 * green check if it is the currently active preset. Tapping selects; long-pressing
 * any preset (factory or user) triggers the context menu.
 *
 * @param factoryPresets Filtered factory presets to display.
 * @param userPresets Filtered user presets to display.
 * @param currentPresetId ID of the currently active preset.
 * @param isEmpty True when no presets match the current filters.
 * @param onPresetSelected Callback with preset ID on tap.
 * @param onToggleFavorite Callback with preset ID to toggle favorite.
 * @param onLongPressPreset Callback with preset ID on long-press (factory and user).
 * @param modifier Modifier (typically Modifier.weight(1f) to fill available space).
 */
@Composable
private fun PresetList(
    factoryPresets: List<Preset>,
    userPresets: List<Preset>,
    currentPresetId: String,
    isEmpty: Boolean,
    onPresetSelected: (String) -> Unit,
    onToggleFavorite: (String) -> Unit,
    onLongPressPreset: (String) -> Unit,
    modifier: Modifier = Modifier
) {
    if (isEmpty) {
        // Empty state
        Box(
            modifier = modifier.fillMaxWidth(),
            contentAlignment = Alignment.Center
        ) {
            Text(
                text = "No presets match your search",
                fontSize = 14.sp,
                color = DesignSystem.TextSecondary
            )
        }
        return
    }

    LazyColumn(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 12.dp)
    ) {
        // -- Factory section --
        if (factoryPresets.isNotEmpty()) {
            item(key = "header_factory") {
                SectionHeader(title = "FACTORY")
            }
            items(
                items = factoryPresets,
                key = { "factory_${it.id}" }
            ) { preset ->
                PresetCard(
                    preset = preset,
                    isActive = preset.id == currentPresetId,
                    onTap = { onPresetSelected(preset.id) },
                    onToggleFavorite = { onToggleFavorite(preset.id) },
                    onLongPress = { onLongPressPreset(preset.id) }
                )
            }
        }

        // -- Divider between sections --
        if (factoryPresets.isNotEmpty() && userPresets.isNotEmpty()) {
            item(key = "divider_sections") {
                HorizontalDivider(
                    color = DesignSystem.Divider,
                    modifier = Modifier.padding(vertical = 8.dp)
                )
            }
        }

        // -- User section --
        if (userPresets.isNotEmpty()) {
            item(key = "header_user") {
                SectionHeader(title = "USER")
            }
            items(
                items = userPresets,
                key = { "user_${it.id}" }
            ) { preset ->
                PresetCard(
                    preset = preset,
                    isActive = preset.id == currentPresetId,
                    onTap = { onPresetSelected(preset.id) },
                    onToggleFavorite = { onToggleFavorite(preset.id) },
                    onLongPress = { onLongPressPreset(preset.id) }
                )
            }
        }

        // Bottom spacer so the last card is not obscured by the action bar
        item(key = "bottom_spacer") {
            Spacer(modifier = Modifier.height(8.dp))
        }
    }
}

/**
 * Section header label (e.g., "FACTORY", "USER").
 */
@Composable
private fun SectionHeader(title: String) {
    Text(
        text = title,
        fontSize = 11.sp,
        fontWeight = FontWeight.Bold,
        fontFamily = FontFamily.Monospace,
        letterSpacing = 2.sp,
        color = DesignSystem.CreamWhite.copy(alpha = 0.7f),
        modifier = Modifier.padding(top = 12.dp, bottom = 4.dp, start = 4.dp)
    )
}

/**
 * A single preset row in the list.
 *
 * Layout: [Star] [Name + Category] [Check if active]
 *
 * @param preset The preset data.
 * @param isActive Whether this preset is currently loaded.
 * @param onTap Callback on single tap.
 * @param onToggleFavorite Callback to toggle the favorite star.
 * @param onLongPress Optional callback for long-press (null disables long-press behavior).
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun PresetCard(
    preset: Preset,
    isActive: Boolean,
    onTap: () -> Unit,
    onToggleFavorite: () -> Unit,
    onLongPress: (() -> Unit)?
) {
    // ControlPanelSurface base with active highlight
    val backgroundColor = if (isActive) {
        DesignSystem.SafeGreen.copy(alpha = 0.08f)
    } else {
        DesignSystem.ControlPanelSurface
    }

    // Left accent bar color for active state
    val accentColor = if (isActive) {
        DesignSystem.SafeGreen
    } else {
        DesignSystem.PanelHighlight.copy(alpha = 0.4f)
    }

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(56.dp)
            .background(backgroundColor, RoundedCornerShape(6.dp))
            .drawBehind {
                // Subtle left accent bar
                val barWidth = 3.dp.toPx()
                drawRect(
                    color = accentColor,
                    topLeft = Offset(0f, 4.dp.toPx()),
                    size = Size(barWidth, size.height - 8.dp.toPx())
                )
            }
            .then(
                if (onLongPress != null) {
                    Modifier.combinedClickable(
                        onClick = onTap,
                        onLongClick = onLongPress
                    )
                } else {
                    Modifier.clickable(onClick = onTap)
                }
            )
            .padding(horizontal = 4.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Favorite star (tap toggles)
        IconButton(
            onClick = onToggleFavorite,
            modifier = Modifier.size(40.dp)
        ) {
            Icon(
                imageVector = if (preset.isFavorite) Icons.Filled.Star else Icons.Outlined.Star,
                contentDescription = if (preset.isFavorite) "Remove from favorites" else "Add to favorites",
                tint = if (preset.isFavorite) FavoriteGold else Color(0xFF666666),
                modifier = Modifier.size(20.dp)
            )
        }

        // Preset name + category label
        Column(
            modifier = Modifier
                .weight(1f)
                .padding(start = 4.dp)
        ) {
            Text(
                text = preset.name,
                fontSize = 15.sp,
                fontWeight = FontWeight.SemiBold,
                color = DesignSystem.TextPrimary,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            Text(
                text = preset.category.name.lowercase().replaceFirstChar { it.uppercase() },
                fontSize = 11.sp,
                color = DesignSystem.TextMuted
            )
        }

        // Active check indicator
        if (isActive) {
            Icon(
                imageVector = Icons.Default.Check,
                contentDescription = "Currently loaded",
                tint = DesignSystem.SafeGreen,
                modifier = Modifier
                    .size(20.dp)
                    .padding(end = 4.dp)
            )
        }
    }
}

// ---------------------------------------------------------------------------
// Bottom action bar
// ---------------------------------------------------------------------------

/**
 * Fixed bottom bar with Import, Save As New, Overwrite, and Delete actions.
 *
 * Overwrite and Delete are only shown when the current preset is a non-factory user preset.
 */
@Composable
private fun BottomActionBar(
    currentPreset: Preset?,
    onImportPreset: () -> Unit,
    onSavePreset: () -> Unit,
    onOverwritePreset: (String) -> Unit,
    onDeletePreset: (String) -> Unit,
    onDismiss: () -> Unit
) {
    val isUserPreset = currentPreset != null && !currentPreset.isFactory

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(52.dp)
            .background(DesignSystem.ModuleSurface)
            .drawBehind {
                val w = size.width
                // Gold piping top border + shadow (matching TopBar treatment)
                val goldStroke = 2.dp.toPx()
                val shadowStroke = 1.dp.toPx()
                drawLine(
                    color = DesignSystem.GoldPiping.copy(alpha = 0.6f),
                    start = Offset(0f, goldStroke / 2f),
                    end = Offset(w, goldStroke / 2f),
                    strokeWidth = goldStroke
                )
                drawLine(
                    color = DesignSystem.PanelShadow,
                    start = Offset(0f, goldStroke + shadowStroke / 2f),
                    end = Offset(w, goldStroke + shadowStroke / 2f),
                    strokeWidth = shadowStroke
                )
            }
            .padding(horizontal = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceEvenly
    ) {
        // Import
        TextButton(onClick = {
            onImportPreset()
            onDismiss()
        }) {
            Text(
                text = "Import",
                fontSize = 13.sp,
                fontWeight = FontWeight.SemiBold,
                color = Color(0xFF80DEEA)
            )
        }

        // Save As New
        TextButton(onClick = {
            onSavePreset()
            onDismiss()
        }) {
            Text(
                text = "Save As New",
                fontSize = 13.sp,
                fontWeight = FontWeight.SemiBold,
                color = DesignSystem.SafeGreen
            )
        }

        // Overwrite (user presets only)
        if (isUserPreset) {
            TextButton(onClick = {
                onOverwritePreset(currentPreset!!.id)
                onDismiss()
            }) {
                Text(
                    text = "Overwrite",
                    fontSize = 13.sp,
                    fontWeight = FontWeight.SemiBold,
                    color = Color(0xFFFFCC80)
                )
            }
        }

        // Delete (user presets only)
        if (isUserPreset) {
            TextButton(onClick = {
                onDeletePreset(currentPreset!!.id)
                onDismiss()
            }) {
                Text(
                    text = "Delete",
                    fontSize = 13.sp,
                    fontWeight = FontWeight.SemiBold,
                    color = DesignSystem.ClipRed
                )
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Context menu dialog (long-press on user preset)
// ---------------------------------------------------------------------------

/**
 * Alert dialog shown on long-press of a preset card.
 *
 * Action availability depends on [isFactory]:
 * - User presets: Rename, Duplicate, Delete
 * - Factory presets: Duplicate only (they are read-only — renaming or
 *   deleting them would remove the immutable factory bank)
 *
 * @param presetName Display name of the target preset.
 * @param isFactory Whether the target preset is a read-only factory preset.
 * @param onRename Callback to initiate the rename flow (ignored for factory).
 * @param onDuplicate Callback to duplicate the preset (both factory and user).
 * @param onDelete Callback to delete the preset (ignored for factory).
 * @param onDismiss Callback to close the dialog without action.
 */
@Composable
private fun ContextMenuDialog(
    presetName: String,
    isFactory: Boolean,
    onRename: () -> Unit,
    onDuplicate: () -> Unit,
    onDelete: () -> Unit,
    onDismiss: () -> Unit
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        containerColor = DesignSystem.ModuleSurface,
        title = {
            Text(
                text = presetName,
                color = DesignSystem.TextPrimary,
                fontWeight = FontWeight.Bold,
                fontSize = 16.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        },
        text = {
            Column {
                // Rename is user-preset only
                if (!isFactory) {
                    TextButton(
                        onClick = onRename,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(
                            text = "Rename",
                            fontSize = 15.sp,
                            color = DesignSystem.TextPrimary
                        )
                    }
                }
                // Duplicate is always available
                TextButton(
                    onClick = onDuplicate,
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text(
                        text = "Duplicate",
                        fontSize = 15.sp,
                        color = DesignSystem.TextPrimary
                    )
                }
                // Delete is user-preset only
                if (!isFactory) {
                    TextButton(
                        onClick = onDelete,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(
                            text = "Delete",
                            fontSize = 15.sp,
                            color = DesignSystem.ClipRed
                        )
                    }
                }
            }
        },
        confirmButton = {},
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("Cancel", color = DesignSystem.TextSecondary)
            }
        }
    )
}

// ---------------------------------------------------------------------------
// Rename dialog
// ---------------------------------------------------------------------------

/**
 * Simple dialog with a text field for renaming a preset.
 *
 * @param currentName Current text in the rename field.
 * @param onNameChange Callback when the text changes.
 * @param onConfirm Callback to apply the rename.
 * @param onDismiss Callback to cancel renaming.
 */
@Composable
private fun RenameDialog(
    currentName: String,
    onNameChange: (String) -> Unit,
    onConfirm: () -> Unit,
    onDismiss: () -> Unit
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        containerColor = DesignSystem.ModuleSurface,
        title = {
            Text(
                text = "RENAME PRESET",
                color = DesignSystem.CreamWhite,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                letterSpacing = 2.sp
            )
        },
        text = {
            OutlinedTextField(
                value = currentName,
                onValueChange = { onNameChange(it.take(100)) },
                singleLine = true,
                label = { Text("Preset Name") },
                colors = OutlinedTextFieldDefaults.colors(
                    focusedTextColor = DesignSystem.TextPrimary,
                    unfocusedTextColor = DesignSystem.TextValue,
                    focusedBorderColor = DesignSystem.SafeGreen,
                    unfocusedBorderColor = DesignSystem.ModuleBorder,
                    focusedLabelColor = DesignSystem.SafeGreen,
                    unfocusedLabelColor = DesignSystem.TextSecondary,
                    cursorColor = DesignSystem.SafeGreen
                ),
                modifier = Modifier.fillMaxWidth()
            )
        },
        confirmButton = {
            Button(
                onClick = onConfirm,
                enabled = currentName.trim().isNotEmpty(),
                colors = ButtonDefaults.buttonColors(
                    containerColor = DesignSystem.SafeGreen,
                    contentColor = Color.Black
                )
            ) {
                Text("Rename", fontWeight = FontWeight.Bold)
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("Cancel", color = DesignSystem.TextSecondary)
            }
        }
    )
}
