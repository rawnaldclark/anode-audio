package com.guitaremulator.app.data

import org.json.JSONObject
import java.util.UUID

/**
 * Represents a saved looper session with its metadata and file references.
 *
 * Each session is stored as a directory containing metadata.json (this object
 * serialized), dry.wav (unprocessed recording), wet.wav (processed recording),
 * and optionally re-amped WAV files.
 *
 * Uses [org.json] for serialization, consistent with [Preset] and the rest
 * of the data layer (no external library dependencies).
 *
 * @property id Unique identifier (UUID string). Generated at creation time.
 * @property name User-facing display name for the session.
 * @property createdAt Unix timestamp (millis) of when the session was saved.
 * @property bpm Tempo in BPM if metronome/quantized mode was active, null if free-form.
 * @property timeSignature Time signature as a string (e.g., "4/4", "3/4", "6/8").
 * @property barCount Number of bars if quantized mode was active, null if free-form.
 * @property presetId UUID of the preset that was active when the loop was recorded.
 *     Null if no preset was loaded.
 * @property presetName Display name of the active preset at recording time.
 *     Null if no preset was loaded.
 * @property dryFilePath Absolute path to the dry (unprocessed) WAV file.
 * @property wetFilePath Absolute path to the wet (processed) WAV file.
 * @property durationMs Duration of the loop in milliseconds.
 * @property sampleRate Sample rate the loop was recorded at (Hz).
 * @property isFavorite Whether the user has marked this session as a favorite.
 */
data class LooperSession(
    val id: String = UUID.randomUUID().toString(),
    val name: String,
    val createdAt: Long = System.currentTimeMillis(),
    val bpm: Float? = null,
    val timeSignature: String = "4/4",
    val barCount: Int? = null,
    val presetId: String? = null,
    val presetName: String? = null,
    val dryFilePath: String,
    val wetFilePath: String,
    val durationMs: Long,
    val sampleRate: Int,
    val isFavorite: Boolean = false
) {

    /**
     * Serialize this session to a [JSONObject] for persistence.
     *
     * Nullable fields (bpm, barCount, presetId, presetName) are only written
     * when non-null, so [fromJson] uses opt* methods for backward compatibility.
     *
     * @return JSON representation of this session.
     */
    fun toJson(): JSONObject {
        val json = JSONObject()
        json.put("id", id)
        json.put("name", name)
        json.put("createdAt", createdAt)
        json.put("timeSignature", timeSignature)
        json.put("dryFilePath", dryFilePath)
        json.put("wetFilePath", wetFilePath)
        json.put("durationMs", durationMs)
        json.put("sampleRate", sampleRate)
        json.put("isFavorite", isFavorite)

        // Only write nullable fields when present
        if (bpm != null) json.put("bpm", bpm.toDouble())
        if (barCount != null) json.put("barCount", barCount)
        if (presetId != null) json.put("presetId", presetId)
        if (presetName != null) json.put("presetName", presetName)

        return json
    }

    companion object {

        /**
         * Deserialize a [LooperSession] from a [JSONObject].
         *
         * Handles missing optional fields gracefully for forward/backward
         * compatibility when session format evolves.
         *
         * @param json The JSON object to parse.
         * @return The deserialized [LooperSession].
         * @throws org.json.JSONException if required fields (id, name,
         *     dryFilePath, wetFilePath, durationMs, sampleRate) are missing.
         */
        fun fromJson(json: JSONObject): LooperSession {
            // Read nullable bpm with NaN/Infinity guard (same pattern as Preset)
            val rawBpm = if (json.has("bpm")) json.getDouble("bpm").toFloat() else null
            val safeBpm = if (rawBpm != null && (rawBpm.isNaN() || rawBpm.isInfinite())) {
                null
            } else {
                rawBpm?.coerceIn(30f, 300f)
            }

            return LooperSession(
                id = json.getString("id"),
                name = json.getString("name"),
                createdAt = json.optLong("createdAt", System.currentTimeMillis()),
                bpm = safeBpm,
                timeSignature = json.optString("timeSignature", "4/4"),
                barCount = if (json.has("barCount")) json.getInt("barCount") else null,
                presetId = json.optString("presetId", null),
                presetName = json.optString("presetName", null),
                dryFilePath = json.getString("dryFilePath"),
                wetFilePath = json.getString("wetFilePath"),
                durationMs = json.getLong("durationMs"),
                sampleRate = json.optInt("sampleRate", 48000),
                isFavorite = json.optBoolean("isFavorite", false)
            )
        }
    }
}
