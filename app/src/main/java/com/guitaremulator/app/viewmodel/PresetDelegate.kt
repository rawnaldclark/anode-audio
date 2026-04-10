package com.guitaremulator.app.viewmodel

import android.app.Application
import android.net.Uri
import com.guitaremulator.app.audio.IAudioEngine
import com.guitaremulator.app.data.EffectRegistry
import com.guitaremulator.app.data.EffectState
import com.guitaremulator.app.data.Preset
import com.guitaremulator.app.data.PresetCategory
import com.guitaremulator.app.data.PresetManager
import com.guitaremulator.app.data.UserPreferencesRepository
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/**
 * Delegate handling all preset management operations: loading, saving,
 * renaming, overwriting, deleting, import/export, and favorites.
 *
 * This delegate coordinates between the [PresetManager] (JSON persistence),
 * the [AudioEngine] (native engine application), and the ViewModel's
 * internal state via lambda callbacks.
 *
 * @property presetManager Handles preset persistence and JSON serialization.
 * @property userPreferencesRepository Persists last active preset ID.
 * @property audioEngine JNI bridge for applying presets to the native engine.
 * @property coroutineScope Scope for launching async operations.
 * @property getApplication Lambda returning the Application context.
 * @property isRunning Lambda returning whether the audio engine is running.
 * @property onPresetLoaded Callback invoked after a preset is loaded; the
 *     ViewModel uses this to call updateEffectStatesFromPreset.
 * @property getCurrentEffectStates Lambda returning the current effect UI states.
 * @property getInputGain Lambda returning the current master input gain.
 * @property getOutputGain Lambda returning the current master output gain.
 * @property setInputGain Lambda to set master input gain in the ViewModel.
 * @property setOutputGain Lambda to set master output gain in the ViewModel.
 * @property getCurrentEffectOrder Lambda returning the current effect processing order.
 * @property applyCurrentEffectStatesToEngine Lambda to push all effect states
 *     to the native engine.
 */
class PresetDelegate(
    private val presetManager: PresetManager,
    private val userPreferencesRepository: UserPreferencesRepository,
    private val audioEngine: IAudioEngine,
    private val coroutineScope: CoroutineScope,
    private val getApplication: () -> Application,
    private val isRunning: () -> Boolean,
    private val onPresetLoaded: (Preset) -> Unit,
    private val getCurrentEffectStates: () -> List<EffectUiState>,
    private val getInputGain: () -> Float,
    private val getOutputGain: () -> Float,
    private val setInputGain: (Float) -> Unit,
    private val setOutputGain: (Float) -> Unit,
    private val getCurrentEffectOrder: () -> List<Int>
) {

    // =========================================================================
    // Observable State
    // =========================================================================

    /** Name of the currently active preset (empty if none). */
    private val _currentPresetName = MutableStateFlow("")
    val currentPresetName: StateFlow<String> = _currentPresetName.asStateFlow()

    /** ID of the currently active preset (empty if none). */
    private val _currentPresetId = MutableStateFlow("")
    val currentPresetId: StateFlow<String> = _currentPresetId.asStateFlow()

    /** All available presets for the preset picker. */
    private val _presets = MutableStateFlow<List<Preset>>(emptyList())
    val presets: StateFlow<List<Preset>> = _presets.asStateFlow()

    /** Transient status message for import/export (empty = no message). */
    private val _presetIOStatus = MutableStateFlow("")
    val presetIOStatus: StateFlow<String> = _presetIOStatus.asStateFlow()

    // =========================================================================
    // Public API
    // =========================================================================

    /**
     * Toggle the favorite status of a preset without loading it.
     *
     * @param presetId UUID of the preset to toggle.
     */
    fun togglePresetFavorite(presetId: String) {
        coroutineScope.launch {
            if (presetManager.toggleFavorite(presetId)) {
                refreshPresetList()
            }
        }
    }

    /**
     * Load and apply a preset by its ID.
     *
     * Loads the preset from storage, applies it to the engine, and updates
     * all UI state flows to reflect the new settings.
     *
     * @param presetId UUID of the preset to load.
     */
    fun loadPreset(presetId: String) {
        coroutineScope.launch {
            val preset = presetManager.loadPreset(presetId) ?: return@launch

            // Apply to native engine if running
            if (isRunning()) {
                presetManager.applyPreset(preset, audioEngine)
            }

            // Update UI state from preset (including master gains)
            _currentPresetName.value = preset.name
            _currentPresetId.value = preset.id
            setInputGain(preset.inputGain)
            setOutputGain(preset.outputGain)
            onPresetLoaded(preset)

            // Persist last active preset
            userPreferencesRepository.setLastPresetId(preset.id)
        }
    }

    /**
     * Save the current effect chain state as a new preset.
     *
     * @param name Display name for the new preset.
     * @param category Category for organizing the preset.
     */
    fun saveCurrentAsPreset(name: String, category: PresetCategory = PresetCategory.CUSTOM) {
        coroutineScope.launch {
            val effects = getCurrentEffectStates().map { uiState ->
                EffectState(
                    effectType = uiState.type,
                    enabled = uiState.enabled,
                    wetDryMix = uiState.wetDryMix,
                    parameters = uiState.parameters
                )
            }

            // Save order only if it differs from the default identity order
            val currentOrder = getCurrentEffectOrder()
            val identityOrder = (0 until EffectRegistry.EFFECT_COUNT).toList()
            val orderToSave = if (currentOrder != identityOrder) currentOrder else null

            val preset = Preset(
                name = name,
                category = category,
                effects = effects,
                inputGain = getInputGain(),
                outputGain = getOutputGain(),
                effectOrder = orderToSave
            )

            presetManager.savePreset(preset)
            _currentPresetName.value = name
            _currentPresetId.value = preset.id
            refreshPresetList()

            // Persist as last active preset
            userPreferencesRepository.setLastPresetId(preset.id)
        }
    }

    /**
     * Rename a preset and refresh the preset list.
     *
     * @param presetId UUID of the preset to rename.
     * @param newName New display name.
     */
    fun renamePreset(presetId: String, newName: String) {
        coroutineScope.launch {
            if (presetManager.renamePreset(presetId, newName)) {
                if (_currentPresetId.value == presetId) {
                    _currentPresetName.value = newName.take(100)
                }
                refreshPresetList()
            }
        }
    }

    /**
     * Overwrite an existing preset with the current effect chain state.
     *
     * @param presetId UUID of the preset to overwrite.
     */
    fun overwritePreset(presetId: String) {
        coroutineScope.launch {
            val effects = getCurrentEffectStates().map { uiState ->
                EffectState(
                    effectType = uiState.type,
                    enabled = uiState.enabled,
                    wetDryMix = uiState.wetDryMix,
                    parameters = uiState.parameters
                )
            }
            val currentOrder = getCurrentEffectOrder()
            val identityOrder = (0 until EffectRegistry.EFFECT_COUNT).toList()
            val orderToSave = if (currentOrder != identityOrder) currentOrder else null

            if (presetManager.overwritePreset(
                    presetId, effects,
                    inputGain = getInputGain(),
                    outputGain = getOutputGain(),
                    effectOrder = orderToSave
                )) {
                refreshPresetList()
            }
        }
    }

    /**
     * Duplicate a preset (factory or user) as a new user preset.
     *
     * The copy receives a fresh UUID, a "(Copy)" suffix on the name, and is
     * marked as non-factory so it can be freely edited/renamed/deleted. The
     * preset list is refreshed after the save, but the newly created preset
     * is NOT auto-loaded — the user stays on whatever preset they had active.
     *
     * @param presetId UUID of the source preset to duplicate.
     */
    fun duplicatePreset(presetId: String) {
        coroutineScope.launch {
            val source = presetManager.loadPreset(presetId) ?: return@launch
            val copy = source.copy(
                id = java.util.UUID.randomUUID().toString(),
                name = "${source.name} (Copy)".take(100),
                author = "User",
                isFactory = false,
                createdAt = System.currentTimeMillis()
            )
            if (presetManager.savePreset(copy)) {
                refreshPresetList()
            }
        }
    }

    /**
     * Delete a preset by its ID and refresh the preset list.
     *
     * @param presetId UUID of the preset to delete.
     */
    fun deletePreset(presetId: String, allowFactory: Boolean = false) {
        coroutineScope.launch {
            val deleted = presetManager.deletePreset(presetId, allowFactory)
            if (deleted && _currentPresetId.value == presetId) {
                _currentPresetName.value = ""
                _currentPresetId.value = ""
                userPreferencesRepository.setLastPresetId("")
            }
            refreshPresetList()
        }
    }

    /**
     * Refresh the preset list from storage.
     */
    fun refreshPresetList() {
        _presets.value = presetManager.listPresets()
    }

    /** Clear the import/export status message. */
    fun clearPresetIOStatus() {
        _presetIOStatus.value = ""
    }

    /**
     * Export the currently loaded preset to a SAF URI as JSON.
     * Runs on IO dispatcher to avoid blocking the Main thread.
     *
     * @param uri Content URI from SAF CreateDocument.
     */
    fun exportCurrentPreset(uri: Uri) {
        coroutineScope.launch {
            val presetId = _currentPresetId.value
            if (presetId.isEmpty()) {
                _presetIOStatus.value = "No preset selected to export"
                return@launch
            }

            try {
                withContext(Dispatchers.IO) {
                    val preset = presetManager.loadPreset(presetId)
                        ?: throw IllegalStateException("Preset not found")
                    val json = presetManager.serializePreset(preset)
                    val output = getApplication().contentResolver
                        .openOutputStream(uri)
                        ?: throw IllegalStateException("Cannot open output file")
                    output.use { it.write(json.toByteArray(Charsets.UTF_8)) }
                }
                _presetIOStatus.value = "Preset exported successfully"
            } catch (e: Exception) {
                android.util.Log.e("PresetDelegate", "Failed to export preset", e)
                _presetIOStatus.value = "Export failed: ${e.message}"
            }
        }
    }

    /**
     * Export an arbitrary preset (by ID) to a SAF URI as JSON.
     *
     * Unlike [exportCurrentPreset], this does not require the target preset
     * to be the currently loaded one — used by the preset browser's long-press
     * context menu Export action.
     *
     * @param presetId UUID of the preset to export.
     * @param uri Content URI from SAF CreateDocument.
     */
    fun exportPresetById(presetId: String, uri: Uri) {
        coroutineScope.launch {
            if (presetId.isEmpty()) {
                _presetIOStatus.value = "No preset selected to export"
                return@launch
            }
            try {
                withContext(Dispatchers.IO) {
                    val preset = presetManager.loadPreset(presetId)
                        ?: throw IllegalStateException("Preset not found")
                    val json = presetManager.serializePreset(preset)
                    val output = getApplication().contentResolver
                        .openOutputStream(uri)
                        ?: throw IllegalStateException("Cannot open output file")
                    output.use { it.write(json.toByteArray(Charsets.UTF_8)) }
                }
                _presetIOStatus.value = "Preset exported successfully"
            } catch (e: Exception) {
                android.util.Log.e("PresetDelegate", "Failed to export preset by id", e)
                _presetIOStatus.value = "Export failed: ${e.message}"
            }
        }
    }

    /**
     * Build a sanitized export file name for a specific preset.
     */
    fun getExportFileNameFor(presetName: String): String {
        val name = presetName.ifEmpty { "preset" }
        return name.replace(Regex("[^a-zA-Z0-9 _-]"), "").trim() + ".json"
    }

    /**
     * Import a preset from a SAF URI (JSON file) and save it to storage.
     * Limits file size to 1 MB to prevent memory exhaustion.
     *
     * @param uri Content URI from SAF OpenDocument.
     */
    fun importPreset(uri: Uri) {
        coroutineScope.launch {
            try {
                val json = withContext(Dispatchers.IO) {
                    val input = getApplication().contentResolver
                        .openInputStream(uri)
                        ?: throw IllegalStateException("Cannot open input file")
                    input.use { stream ->
                        val reader = stream.bufferedReader(Charsets.UTF_8)
                        val buffer = CharArray(1_048_576) // 1 MB limit
                        val sb = StringBuilder()
                        var charsRead: Int
                        while (reader.read(buffer).also { charsRead = it } != -1) {
                            sb.append(buffer, 0, charsRead)
                            if (sb.length > 1_048_576) {
                                throw IllegalStateException("File too large (max 1 MB)")
                            }
                        }
                        sb.toString()
                    }
                }

                val preset = presetManager.deserializePreset(json)
                if (preset == null) {
                    _presetIOStatus.value = "Invalid preset file"
                    return@launch
                }

                if (presetManager.savePreset(preset)) {
                    refreshPresetList()
                    loadPreset(preset.id)
                    _presetIOStatus.value = "Imported: ${preset.name}"
                } else {
                    _presetIOStatus.value = "Failed to save imported preset"
                }
            } catch (e: Exception) {
                android.util.Log.e("PresetDelegate", "Failed to import preset", e)
                _presetIOStatus.value = "Import failed: ${e.message}"
            }
        }
    }

    /**
     * Get the current preset name for use as a suggested export filename.
     */
    fun getExportFileName(): String {
        val name = _currentPresetName.value.ifEmpty { "preset" }
        // Sanitize for filesystem
        return name.replace(Regex("[^a-zA-Z0-9 _-]"), "").trim() + ".json"
    }

    // =========================================================================
    // Internal API (used by AudioViewModel init)
    // =========================================================================

    /**
     * Set the current preset name and ID directly.
     *
     * Used during initialization when restoring the last active preset
     * from user preferences.
     */
    internal fun setCurrentPreset(name: String, id: String) {
        _currentPresetName.value = name
        _currentPresetId.value = id
    }
}
