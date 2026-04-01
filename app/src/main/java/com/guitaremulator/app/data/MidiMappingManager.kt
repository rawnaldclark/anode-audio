package com.guitaremulator.app.data

import android.content.Context
import android.util.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/**
 * Manages MIDI CC-to-parameter mapping persistence and lookup.
 *
 * All mappings are stored in a single JSON file ([MAPPINGS_FILE]) as a JSON
 * array. Each element describes one CC-to-parameter mapping. The file is read
 * and written atomically on every mutation to keep the on-disk state consistent.
 *
 * This class uses [org.json] for JSON serialization to avoid adding external
 * dependencies. All file I/O operations are synchronous and should be called
 * from a background thread or coroutine.
 *
 * @param context Application context for accessing file storage.
 */
class MidiMappingManager(private val context: Context) {

    companion object {
        private const val TAG = "MidiMappingManager"
        private const val MAPPINGS_FILE = "midi_mappings.json"
    }

    /** Lazily resolved file handle for the mappings JSON file. */
    private val mappingsFile: File by lazy {
        File(context.filesDir, MAPPINGS_FILE)
    }

    // =========================================================================
    // Public API
    // =========================================================================

    /**
     * Save a complete list of mappings to the mappings file.
     *
     * Overwrites any existing file contents. An empty list produces an empty
     * JSON array, preserving the file for future reads.
     *
     * @param mappings The full list of mappings to persist.
     * @return True if saved successfully, false on I/O error.
     */
    fun saveMappings(mappings: List<MidiMapping>): Boolean {
        return try {
            val json = mappingsToJsonString(mappings)
            mappingsFile.writeText(json)
            Log.d(TAG, "Saved ${mappings.size} MIDI mappings")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to save MIDI mappings", e)
            false
        }
    }

    /**
     * Load all mappings from the mappings file.
     *
     * @return List of all persisted mappings, or an empty list if the file
     *     does not exist or cannot be parsed.
     */
    fun loadMappings(): List<MidiMapping> {
        if (!mappingsFile.exists()) return emptyList()
        return try {
            val text = mappingsFile.readText()
            jsonStringToMappings(text)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load MIDI mappings", e)
            emptyList()
        }
    }

    /**
     * Add a single mapping after validating its fields.
     *
     * Validation rules:
     * - [MidiMapping.ccNumber] must be in 0..127.
     * - [MidiMapping.midiChannel] must be in 0..15.
     * - [MidiMapping.targetEffectIndex] must be in 0..20.
     * - [MidiMapping.targetParamId] must exist in [EffectRegistry] for the
     *   given effect index.
     *
     * If validation passes the mapping is appended to the existing list and
     * the file is re-written.
     *
     * @param mapping The mapping to add.
     * @return True if the mapping was validated and saved, false otherwise.
     */
    fun addMapping(mapping: MidiMapping): Boolean {
        // --- Validate CC number ---
        if (mapping.ccNumber !in 0..127) {
            Log.w(TAG, "Invalid ccNumber: ${mapping.ccNumber} (must be 0..127)")
            return false
        }

        // --- Validate MIDI channel ---
        if (mapping.midiChannel !in 0..15) {
            Log.w(TAG, "Invalid midiChannel: ${mapping.midiChannel} (must be 0..15)")
            return false
        }

        // --- Validate target effect index ---
        if (mapping.targetEffectIndex !in 0..20) {
            Log.w(TAG, "Invalid targetEffectIndex: ${mapping.targetEffectIndex} (must be 0..20)")
            return false
        }

        // --- Validate target param ID against EffectRegistry ---
        val effectInfo = EffectRegistry.getEffectInfo(mapping.targetEffectIndex)
        if (effectInfo == null || !effectInfo.params.any { it.id == mapping.targetParamId }) {
            Log.w(
                TAG,
                "Invalid targetParamId: ${mapping.targetParamId} " +
                    "for effect index ${mapping.targetEffectIndex}"
            )
            return false
        }

        // --- Append and save ---
        val current = loadMappings().toMutableList()
        current.add(mapping)
        return saveMappings(current)
    }

    /**
     * Remove a mapping by its unique [id].
     *
     * Loads the current list, removes the first mapping whose [MidiMapping.id]
     * matches, and re-writes the file. Returns false if no mapping with the
     * given ID exists.
     *
     * @param id UUID string of the mapping to remove.
     * @return True if the mapping was found and removed, false otherwise.
     */
    fun removeMapping(id: String): Boolean {
        val current = loadMappings().toMutableList()
        val removed = current.removeAll { it.id == id }
        if (!removed) {
            Log.w(TAG, "No mapping found with id: $id")
            return false
        }
        return saveMappings(current)
    }

    /**
     * Delete the mappings file entirely, clearing all stored mappings.
     */
    fun clearAllMappings() {
        if (mappingsFile.exists()) {
            mappingsFile.delete()
            Log.d(TAG, "Cleared all MIDI mappings")
        }
    }

    /**
     * Find the first mapping that matches a given CC number and channel.
     *
     * Channel matching rules:
     * - A stored mapping with [MidiMapping.midiChannel] == 0 (omni) matches
     *   any incoming channel.
     * - An incoming [channel] of 0 matches any stored mapping regardless of
     *   its channel.
     * - Otherwise, the stored channel must equal the incoming channel exactly.
     *
     * @param ccNumber The MIDI CC number to look up (0-127).
     * @param channel The MIDI channel of the incoming message (0-15).
     * @return The first matching [MidiMapping], or null if none match.
     */
    fun getMappingForCC(ccNumber: Int, channel: Int): MidiMapping? {
        val mappings = loadMappings()
        return mappings.firstOrNull { mapping ->
            mapping.ccNumber == ccNumber &&
                (mapping.midiChannel == 0 || channel == 0 || mapping.midiChannel == channel)
        }
    }

    // =========================================================================
    // JSON Serialization (internal for testing)
    // =========================================================================

    /**
     * Serialize a single [MidiMapping] to a [JSONObject].
     *
     * JSON structure:
     * ```json
     * {
     *   "id": "uuid-string",
     *   "ccNumber": 7,
     *   "midiChannel": 0,
     *   "targetEffectIndex": 4,
     *   "targetParamId": 0,
     *   "minValue": 0.0,
     *   "maxValue": 1.0,
     *   "curve": "LINEAR"
     * }
     * ```
     */
    internal fun mappingToJson(mapping: MidiMapping): JSONObject {
        val json = JSONObject()
        json.put("id", mapping.id)
        json.put("ccNumber", mapping.ccNumber)
        json.put("midiChannel", mapping.midiChannel)
        json.put("targetEffectIndex", mapping.targetEffectIndex)
        json.put("targetParamId", mapping.targetParamId)
        json.put("minValue", mapping.minValue.toDouble())
        json.put("maxValue", mapping.maxValue.toDouble())
        json.put("curve", mapping.curve.name)
        return json
    }

    /**
     * Deserialize a [JSONObject] into a [MidiMapping].
     *
     * Returns null if any required field is missing or if the curve string
     * does not match a valid [MappingCurve] enum constant.
     *
     * @param json The JSON object to parse.
     * @return The deserialized [MidiMapping] or null if the data is invalid.
     */
    internal fun jsonToMapping(json: JSONObject): MidiMapping? {
        return try {
            val curveStr = json.optString("curve", "LINEAR")
            val curve = try {
                MappingCurve.valueOf(curveStr)
            } catch (e: IllegalArgumentException) {
                Log.w(TAG, "Unknown curve type: $curveStr, defaulting to LINEAR")
                MappingCurve.LINEAR
            }

            MidiMapping(
                id = json.getString("id"),
                ccNumber = json.getInt("ccNumber"),
                midiChannel = json.optInt("midiChannel", 0),
                targetEffectIndex = json.getInt("targetEffectIndex"),
                targetParamId = json.getInt("targetParamId"),
                minValue = json.getDouble("minValue").toFloat(),
                maxValue = json.getDouble("maxValue").toFloat(),
                curve = curve
            )
        } catch (e: Exception) {
            Log.e(TAG, "Failed to parse MIDI mapping JSON", e)
            null
        }
    }

    /**
     * Serialize a list of mappings to a pretty-printed JSON array string.
     *
     * @param mappings The mappings to serialize.
     * @return Pretty-printed JSON string containing a JSON array.
     */
    internal fun mappingsToJsonString(mappings: List<MidiMapping>): String {
        val array = JSONArray()
        for (mapping in mappings) {
            array.put(mappingToJson(mapping))
        }
        return array.toString(2)
    }

    /**
     * Deserialize a JSON array string into a list of mappings.
     *
     * Invalid individual entries are silently skipped so that a single
     * corrupted mapping does not prevent loading the rest.
     *
     * @param json The JSON string to parse (must be a JSON array).
     * @return List of successfully parsed mappings.
     */
    internal fun jsonStringToMappings(json: String): List<MidiMapping> {
        val mappings = mutableListOf<MidiMapping>()
        return try {
            val array = JSONArray(json)
            for (i in 0 until array.length()) {
                val obj = array.getJSONObject(i)
                val mapping = jsonToMapping(obj)
                if (mapping != null) {
                    mappings.add(mapping)
                } else {
                    Log.w(TAG, "Skipping corrupted MIDI mapping at index $i")
                }
            }
            mappings
        } catch (e: Exception) {
            Log.e(TAG, "Failed to parse MIDI mappings JSON string", e)
            emptyList()
        }
    }
}
