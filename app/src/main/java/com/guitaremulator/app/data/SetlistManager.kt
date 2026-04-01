package com.guitaremulator.app.data

import android.content.Context
import android.util.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/**
 * Manages setlist persistence: save, load, delete, list, and rename.
 *
 * Setlists are stored as individual JSON files in the app's internal storage
 * at [filesDir]/setlists/. Each file is named by the setlist's UUID to avoid
 * naming collisions.
 *
 * This class uses [org.json] for JSON serialization to avoid adding external
 * dependencies. All file I/O operations are synchronous and should be called
 * from a background thread or coroutine.
 *
 * @param context Application context for accessing file storage.
 */
class SetlistManager(private val context: Context) {

    companion object {
        private const val TAG = "SetlistManager"
        private const val SETLISTS_DIR = "setlists"
        private const val JSON_EXTENSION = ".json"
    }

    /** Directory where setlist JSON files are stored. */
    private val setlistsDir: File by lazy {
        File(context.filesDir, SETLISTS_DIR).also { dir ->
            if (!dir.exists()) {
                dir.mkdirs()
            }
        }
    }

    // =========================================================================
    // Public API
    // =========================================================================

    /**
     * Save a setlist to internal storage as a JSON file.
     *
     * The file is named using the setlist's UUID to avoid naming collisions.
     * Overwrites any existing file with the same ID.
     *
     * @param setlist The setlist to save.
     * @return True if saved successfully, false on error.
     */
    fun saveSetlist(setlist: Setlist): Boolean {
        return try {
            val json = setlistToJson(setlist)
            val file = File(setlistsDir, setlist.id + JSON_EXTENSION)
            file.writeText(json)
            Log.d(TAG, "Saved setlist: ${setlist.name} (${setlist.id})")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to save setlist: ${setlist.name}", e)
            false
        }
    }

    /**
     * Load a setlist by its unique ID.
     *
     * @param id UUID string of the setlist.
     * @return The loaded [Setlist] or null if not found or corrupted.
     */
    fun loadSetlist(id: String): Setlist? {
        return try {
            val file = File(setlistsDir, id + JSON_EXTENSION)
            if (!file.exists()) {
                Log.w(TAG, "Setlist file not found: $id")
                return null
            }
            jsonToSetlist(file.readText())
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load setlist: $id", e)
            null
        }
    }

    /**
     * Delete a setlist by its unique ID.
     *
     * @param id UUID string of the setlist to delete.
     * @return True if deleted successfully, false if not found.
     */
    fun deleteSetlist(id: String): Boolean {
        val file = File(setlistsDir, id + JSON_EXTENSION)
        if (!file.exists()) return false

        return file.delete().also { success ->
            if (success) Log.d(TAG, "Deleted setlist: $id")
            else Log.w(TAG, "Failed to delete setlist file: $id")
        }
    }

    /**
     * List all saved setlists, sorted by creation date (newest first).
     *
     * @return List of all setlists in storage. Returns empty list on error.
     */
    fun listSetlists(): List<Setlist> {
        val setlists = mutableListOf<Setlist>()
        val files = setlistsDir.listFiles { file ->
            file.extension == "json"
        } ?: return emptyList()

        for (file in files) {
            try {
                jsonToSetlist(file.readText())?.let { setlists.add(it) }
            } catch (e: Exception) {
                Log.w(TAG, "Skipping corrupted setlist file: ${file.name}", e)
            }
        }

        return setlists.sortedByDescending { it.createdAt }
    }

    /**
     * Rename a setlist by its unique ID.
     *
     * Loads the existing setlist, applies the new name, and saves it back.
     *
     * @param id UUID string of the setlist.
     * @param newName New display name for the setlist.
     * @return True if renamed successfully, false if not found.
     */
    fun renameSetlist(id: String, newName: String): Boolean {
        val setlist = loadSetlist(id) ?: return false
        val renamed = setlist.copy(name = newName.take(100))
        return saveSetlist(renamed)
    }

    // =========================================================================
    // JSON Serialization
    // =========================================================================

    /**
     * Serialize a [Setlist] to a JSON string.
     *
     * JSON structure:
     * ```json
     * {
     *   "id": "uuid-string",
     *   "name": "Setlist Name",
     *   "presetIds": ["uuid1", "uuid2"],
     *   "createdAt": 1700000000000
     * }
     * ```
     *
     * @param setlist The setlist to serialize.
     * @return JSON string representation.
     */
    internal fun setlistToJson(setlist: Setlist): String {
        val json = JSONObject()
        json.put("id", setlist.id)
        json.put("name", setlist.name)
        json.put("createdAt", setlist.createdAt)

        val presetIdsArray = JSONArray()
        for (presetId in setlist.presetIds) {
            presetIdsArray.put(presetId)
        }
        json.put("presetIds", presetIdsArray)

        return json.toString(2)
    }

    /**
     * Deserialize a JSON string into a [Setlist].
     *
     * Gracefully handles missing optional fields:
     * - Missing "presetIds" defaults to an empty list.
     * - Missing "createdAt" defaults to current system time.
     *
     * @param json The JSON string to parse.
     * @return The deserialized [Setlist] or null if the JSON is malformed.
     */
    internal fun jsonToSetlist(json: String): Setlist? {
        return try {
            val obj = JSONObject(json)

            // Extract the ordered list of preset IDs, defaulting to empty
            val presetIds = mutableListOf<String>()
            if (obj.has("presetIds")) {
                val array = obj.getJSONArray("presetIds")
                for (i in 0 until array.length()) {
                    presetIds.add(array.getString(i))
                }
            }

            Setlist(
                id = obj.getString("id"),
                name = obj.getString("name"),
                presetIds = presetIds,
                createdAt = obj.optLong("createdAt", System.currentTimeMillis())
            )
        } catch (e: Exception) {
            Log.e(TAG, "Failed to parse setlist JSON", e)
            null
        }
    }
}
