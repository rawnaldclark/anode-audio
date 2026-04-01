package com.guitaremulator.app.data

import android.content.Context
import android.util.Log
import com.guitaremulator.app.audio.IAudioEngine
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.util.UUID

/**
 * Manages preset persistence, loading, and application to the native audio engine.
 *
 * Presets are stored as individual JSON files in the app's internal storage
 * at [filesDir]/presets/. Factory presets are generated on first run if the
 * directory is empty.
 *
 * This class uses [org.json] for JSON serialization to avoid adding external
 * dependencies. All file I/O operations are synchronous and should be called
 * from a background thread or coroutine.
 *
 * @param context Application context for accessing file storage.
 */
class PresetManager(private val context: Context) {

    companion object {
        private const val TAG = "PresetManager"
        private const val PRESETS_DIR = "presets"
        private const val JSON_EXTENSION = ".json"
    }

    /** Directory where user and factory presets are stored. */
    private val presetsDir: File by lazy {
        File(context.filesDir, PRESETS_DIR).also { dir ->
            if (!dir.exists()) {
                dir.mkdirs()
            }
        }
    }

    // =========================================================================
    // Public API
    // =========================================================================

    /**
     * Save a preset to internal storage as a JSON file.
     *
     * The file is named using the preset's UUID to avoid naming collisions.
     * Overwrites any existing file with the same ID.
     *
     * @param preset The preset to save.
     * @return True if saved successfully, false on error.
     */
    fun savePreset(preset: Preset): Boolean {
        return try {
            val json = presetToJson(preset)
            val file = File(presetsDir, preset.id + JSON_EXTENSION)
            file.writeText(json.toString(2))
            Log.d(TAG, "Saved preset: ${preset.name} (${preset.id})")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to save preset: ${preset.name}", e)
            false
        }
    }

    /**
     * Load a preset by its unique ID.
     *
     * @param id UUID string of the preset.
     * @return The loaded [Preset] or null if not found or corrupted.
     */
    fun loadPreset(id: String): Preset? {
        return try {
            val file = File(presetsDir, id + JSON_EXTENSION)
            if (!file.exists()) {
                Log.w(TAG, "Preset file not found: $id")
                return null
            }
            val json = JSONObject(file.readText())
            jsonToPreset(json)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load preset: $id", e)
            null
        }
    }

    /**
     * Rename a preset by its unique ID.
     *
     * Factory presets cannot be renamed.
     *
     * @param id UUID string of the preset.
     * @param newName New display name for the preset.
     * @return True if renamed successfully, false if not found or protected.
     */
    fun renamePreset(id: String, newName: String): Boolean {
        val preset = loadPreset(id) ?: return false
        if (preset.isFactory) return false
        val renamed = preset.copy(name = newName.take(100))
        return savePreset(renamed)
    }

    /**
     * Overwrite an existing preset with new effect states.
     *
     * Preserves the preset's ID, name, author, and category. Updates only
     * the effect chain and the modification timestamp. Factory presets
     * cannot be overwritten.
     *
     * @param id UUID string of the preset to overwrite.
     * @param effects New effect states to save.
     * @return True if overwritten successfully, false if not found or protected.
     */
    fun overwritePreset(
        id: String,
        effects: List<EffectState>,
        inputGain: Float = 1.0f,
        outputGain: Float = 1.0f,
        effectOrder: List<Int>? = null
    ): Boolean {
        val preset = loadPreset(id) ?: return false
        if (preset.isFactory) return false
        val updated = preset.copy(
            effects = effects,
            inputGain = inputGain,
            outputGain = outputGain,
            effectOrder = effectOrder
        )
        return savePreset(updated)
    }

    /**
     * Toggle the favorite status of a preset.
     *
     * Loads the preset, flips [Preset.isFavorite], and saves it back.
     *
     * @param presetId UUID string of the preset to toggle.
     * @return True if toggled and saved successfully, false on error.
     */
    fun toggleFavorite(presetId: String): Boolean {
        val preset = loadPreset(presetId) ?: return false
        val toggled = preset.copy(isFavorite = !preset.isFavorite)
        return savePreset(toggled)
    }

    /**
     * Delete a preset by its unique ID.
     *
     * Factory presets are protected and cannot be deleted.
     *
     * @param id UUID string of the preset to delete.
     * @return True if deleted successfully, false if not found or protected.
     */
    fun deletePreset(id: String): Boolean {
        val file = File(presetsDir, id + JSON_EXTENSION)
        if (!file.exists()) return false

        // Check if it is a factory preset before deleting
        try {
            val json = JSONObject(file.readText())
            if (json.optBoolean("isFactory", false)) {
                Log.w(TAG, "Cannot delete factory preset: $id")
                return false
            }
        } catch (e: Exception) {
            // If we cannot parse it, allow deletion
        }

        return file.delete().also { success ->
            if (success) Log.d(TAG, "Deleted preset: $id")
            else Log.w(TAG, "Failed to delete preset file: $id")
        }
    }

    /**
     * List all saved presets, sorted by creation date (newest first).
     *
     * @return List of all presets in storage. Returns empty list on error.
     */
    fun listPresets(): List<Preset> {
        val presets = mutableListOf<Preset>()
        val files = presetsDir.listFiles { file ->
            file.extension == "json"
        } ?: return emptyList()

        for (file in files) {
            try {
                val json = JSONObject(file.readText())
                jsonToPreset(json)?.let { presets.add(it) }
            } catch (e: Exception) {
                Log.w(TAG, "Skipping corrupted preset file: ${file.name}", e)
            }
        }

        return presets.sortedByDescending { it.createdAt }
    }

    /**
     * Load factory presets into storage, overwriting existing definitions.
     *
     * Called on every app launch to ensure factory presets reflect the
     * latest effect parameters (e.g. new modes added in updates).
     * Preserves the user's isFavorite flag on existing presets.
     *
     * @return Number of factory presets loaded.
     */
    fun loadFactoryPresets(): Int {
        val factoryPresets = FactoryPresets.getAll()
        var loaded = 0

        for (preset in factoryPresets) {
            // Always overwrite factory presets to pick up new/changed
            // parameters from app updates (e.g. new effect modes added
            // after initial install). Preserve the user's isFavorite flag.
            val file = File(presetsDir, preset.id + JSON_EXTENSION)
            val toSave = if (file.exists()) {
                val existing = loadPreset(preset.id)
                if (existing?.isFavorite == true) {
                    preset.copy(isFavorite = true)
                } else {
                    preset
                }
            } else {
                preset
            }
            if (savePreset(toSave)) loaded++
        }

        Log.d(TAG, "Loaded $loaded factory presets (${factoryPresets.size} total)")
        return loaded
    }

    /**
     * Apply a preset to the native audio engine.
     *
     * Sets every effect's enabled state, wet/dry mix, and all parameters
     * via the JNI bridge. The Tuner effect (index 14) is skipped for
     * parameter writes since it is read-only.
     *
     * @param preset The preset to apply.
     * @param audioEngine The JNI bridge to the native engine.
     */
    fun applyPreset(preset: Preset, audioEngine: IAudioEngine) {
        // Build a set of effect types present in the preset for fast lookup
        val presetTypeSet = mutableSetOf<String>()

        // Match each preset effect to the current chain by effectType string,
        // not by list position. This ensures backward compatibility when effects
        // are inserted/reordered between app versions (e.g., 14→16 effect chain).
        for (effectState in preset.effects) {
            val index = EffectRegistry.getIndexForType(effectState.effectType)
            if (index < 0 || index >= EffectRegistry.EFFECT_COUNT) continue

            presetTypeSet.add(effectState.effectType)
            audioEngine.setEffectEnabled(index, effectState.enabled)
            audioEngine.setEffectWetDryMix(index, effectState.wetDryMix)

            val info = EffectRegistry.getEffectInfo(index)
            if (info != null && !info.isReadOnly) {
                for ((paramId, value) in effectState.parameters) {
                    audioEngine.setEffectParameter(index, paramId, value)
                }
            }
        }

        // Effects not in the preset (e.g., newly added effects) get defaults
        for (info in EffectRegistry.effects) {
            if (info.type in presetTypeSet) continue
            audioEngine.setEffectEnabled(info.index, info.enabledByDefault)
            audioEngine.setEffectWetDryMix(info.index, 1.0f)
            if (!info.isReadOnly) {
                for (param in info.params) {
                    audioEngine.setEffectParameter(info.index, param.id, param.defaultValue)
                }
            }
        }

        // Apply master gains from preset
        audioEngine.setInputGain(preset.inputGain)
        audioEngine.setOutputGain(preset.outputGain)

        // Apply effect processing order if present, otherwise reset to identity
        val order = preset.effectOrder
            ?: (0 until EffectRegistry.EFFECT_COUNT).toList()
        audioEngine.setEffectOrder(order.toIntArray())

        Log.d(TAG, "Applied preset: ${preset.name}")
    }

    /**
     * Capture the current state of the audio engine as a new preset.
     *
     * Reads all effect parameters, enabled states, and wet/dry mixes from
     * the native engine via JNI and assembles them into a [Preset].
     *
     * @param name Display name for the new preset.
     * @param audioEngine The JNI bridge to read state from.
     * @param category Optional category for the preset.
     * @return A new [Preset] reflecting the current engine state.
     */
    fun capturePreset(
        name: String,
        audioEngine: IAudioEngine,
        category: PresetCategory = PresetCategory.CUSTOM
    ): Preset {
        val effects = mutableListOf<EffectState>()

        for (info in EffectRegistry.effects) {
            val params = mutableMapOf<Int, Float>()

            // Read each parameter's current value from the engine
            for (paramInfo in info.params) {
                val value = audioEngine.getEffectParameter(info.index, paramInfo.id)
                params[paramInfo.id] = value
            }

            effects.add(
                EffectState(
                    effectType = info.type,
                    enabled = audioEngine.isEffectEnabled(info.index),
                    wetDryMix = audioEngine.getEffectWetDryMix(info.index),
                    parameters = params
                )
            )
        }

        // Capture the current effect processing order
        val currentOrder = audioEngine.getEffectOrder().toList()
        val identityOrder = (0 until EffectRegistry.EFFECT_COUNT).toList()
        val effectOrder = if (currentOrder != identityOrder) currentOrder else null

        return Preset(
            name = name,
            author = "User",
            category = category,
            effects = effects,
            inputGain = audioEngine.getInputGain(),
            outputGain = audioEngine.getOutputGain(),
            effectOrder = effectOrder
        )
    }

    // =========================================================================
    // JSON Serialization
    // =========================================================================

    /**
     * Serialize a [Preset] to a JSON string for export.
     *
     * @param preset The preset to serialize.
     * @return Pretty-printed JSON string.
     */
    fun serializePreset(preset: Preset): String {
        return presetToJson(preset).toString(2)
    }

    /**
     * Deserialize a JSON string into a [Preset] for import.
     *
     * The imported preset is assigned a new UUID and marked as non-factory
     * to prevent collisions with existing presets.
     *
     * @param jsonString The JSON string to parse.
     * @return The deserialized [Preset] or null if the JSON is malformed.
     */
    fun deserializePreset(jsonString: String): Preset? {
        return try {
            val json = JSONObject(jsonString)
            val preset = jsonToPreset(json) ?: return null

            // Validate and clamp parameter values against EffectRegistry ranges
            val sanitizedEffects = preset.effects.map { effect ->
                val info = EffectRegistry.getEffectInfo(
                    EffectRegistry.getIndexForType(effect.effectType)
                )
                if (info == null) return@map effect

                val clampedParams = effect.parameters.mapValues { (paramId, value) ->
                    // Reject NaN/Infinity
                    if (value.isNaN() || value.isInfinite()) {
                        val paramInfo = info.params.find { it.id == paramId }
                        paramInfo?.defaultValue ?: 0f
                    } else {
                        val paramInfo = info.params.find { it.id == paramId }
                        if (paramInfo != null) {
                            value.coerceIn(paramInfo.min, paramInfo.max)
                        } else {
                            value
                        }
                    }
                }
                effect.copy(
                    parameters = clampedParams,
                    wetDryMix = effect.wetDryMix.coerceIn(0f, 1f)
                )
            }

            // Assign new ID, sanitize name, mark as user preset
            val safeName = preset.name.take(100).ifEmpty { "Imported Preset" }
            preset.copy(
                id = UUID.randomUUID().toString(),
                name = safeName,
                isFactory = false,
                createdAt = System.currentTimeMillis(),
                effects = sanitizedEffects
            )
        } catch (e: Exception) {
            Log.e(TAG, "Failed to deserialize preset from string", e)
            null
        }
    }

    /**
     * Serialize a [Preset] to a [JSONObject].
     *
     * JSON structure:
     * ```json
     * {
     *   "id": "uuid-string",
     *   "name": "Preset Name",
     *   "author": "User",
     *   "category": "CLEAN",
     *   "createdAt": 1700000000000,
     *   "isFactory": false,
     *   "effects": [
     *     {
     *       "effectType": "NoiseGate",
     *       "enabled": true,
     *       "wetDryMix": 1.0,
     *       "parameters": { "0": -40.0, "1": 6.0 }
     *     }
     *   ]
     * }
     * ```
     */
    internal fun presetToJson(preset: Preset): JSONObject {
        val json = JSONObject()
        json.put("id", preset.id)
        json.put("name", preset.name)
        json.put("author", preset.author)
        json.put("category", preset.category.name)
        json.put("createdAt", preset.createdAt)
        json.put("isFactory", preset.isFactory)
        json.put("isFavorite", preset.isFavorite)
        json.put("inputGain", preset.inputGain.toDouble())
        json.put("outputGain", preset.outputGain.toDouble())

        // Serialize effect processing order (optional, null = identity order)
        if (preset.effectOrder != null) {
            val orderArray = JSONArray()
            for (idx in preset.effectOrder) {
                orderArray.put(idx)
            }
            json.put("effectOrder", orderArray)
        }

        val effectsArray = JSONArray()
        for (effect in preset.effects) {
            val effectJson = JSONObject()
            effectJson.put("effectType", effect.effectType)
            effectJson.put("enabled", effect.enabled)
            effectJson.put("wetDryMix", effect.wetDryMix.toDouble())

            val paramsJson = JSONObject()
            for ((paramId, value) in effect.parameters) {
                paramsJson.put(paramId.toString(), value.toDouble())
            }
            effectJson.put("parameters", paramsJson)

            effectsArray.put(effectJson)
        }
        json.put("effects", effectsArray)

        return json
    }

    /**
     * Deserialize a [JSONObject] into a [Preset].
     *
     * @param json The JSON object to parse.
     * @return The deserialized [Preset] or null if the JSON is malformed.
     */
    internal fun jsonToPreset(json: JSONObject): Preset? {
        return try {
            val effectsArray = json.getJSONArray("effects")
            val effects = mutableListOf<EffectState>()

            for (i in 0 until effectsArray.length()) {
                val effectJson = effectsArray.getJSONObject(i)
                val paramsJson = effectJson.getJSONObject("parameters")

                val parameters = mutableMapOf<Int, Float>()
                val keys = paramsJson.keys()
                while (keys.hasNext()) {
                    val key = keys.next()
                    parameters[key.toInt()] = paramsJson.getDouble(key).toFloat()
                }

                effects.add(
                    EffectState(
                        effectType = effectJson.getString("effectType"),
                        enabled = effectJson.getBoolean("enabled"),
                        wetDryMix = effectJson.optDouble("wetDryMix", 1.0).toFloat(),
                        parameters = parameters
                    )
                )
            }

            val categoryStr = json.optString("category", "CUSTOM")
            val category = try {
                PresetCategory.valueOf(categoryStr)
            } catch (e: IllegalArgumentException) {
                PresetCategory.CUSTOM
            }

            // Guard against NaN/Infinity from corrupted JSON: optDouble can
            // return NaN if the stored value is "NaN", and toFloat() preserves
            // it. coerceIn does NOT catch NaN (NaN fails all comparisons).
            val rawInputGain = json.optDouble("inputGain", 1.0).toFloat()
            val rawOutputGain = json.optDouble("outputGain", 1.0).toFloat()
            val safeInputGain = if (rawInputGain.isNaN() || rawInputGain.isInfinite()) 1.0f
                else rawInputGain.coerceIn(0.0f, 4.0f)
            val safeOutputGain = if (rawOutputGain.isNaN() || rawOutputGain.isInfinite()) 1.0f
                else rawOutputGain.coerceIn(0.0f, 4.0f)

            // Deserialize optional effect processing order with validation
            val effectOrder = if (json.has("effectOrder")) {
                val orderArray = json.getJSONArray("effectOrder")
                val order = (0 until orderArray.length()).map { orderArray.getInt(it) }
                // Must be a valid permutation of [0, EFFECT_COUNT)
                if (order.size == EffectRegistry.EFFECT_COUNT &&
                    order.all { it in 0 until EffectRegistry.EFFECT_COUNT } &&
                    order.toSet().size == order.size
                ) {
                    order
                } else {
                    null // Invalid order, fall back to default
                }
            } else {
                null
            }

            Preset(
                id = json.getString("id"),
                name = json.getString("name"),
                author = json.optString("author", "Unknown"),
                category = category,
                createdAt = json.optLong("createdAt", System.currentTimeMillis()),
                effects = effects,
                isFactory = json.optBoolean("isFactory", false),
                isFavorite = json.optBoolean("isFavorite", false),
                inputGain = safeInputGain,
                outputGain = safeOutputGain,
                effectOrder = effectOrder
            )
        } catch (e: Exception) {
            Log.e(TAG, "Failed to parse preset JSON", e)
            null
        }
    }

    /**
     * Delegates to [FactoryPresets.buildEffectChain] for backward compatibility
     * with tests and other callers.
     */
    internal fun buildEffectChain(
        enabledMap: Map<Int, Boolean> = emptyMap(),
        paramOverrides: Map<Pair<Int, Int>, Float> = emptyMap(),
        wetDryOverrides: Map<Int, Float> = emptyMap()
    ): List<EffectState> = FactoryPresets.buildEffectChain(enabledMap, paramOverrides, wetDryOverrides)
}
