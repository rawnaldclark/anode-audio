package com.guitaremulator.app.data

import android.content.Context
import android.util.Log
import org.json.JSONObject
import java.io.File

/**
 * Manages persistence of looper sessions on the local filesystem.
 *
 * Each session is stored as a subdirectory under [filesDir]/looper_sessions/
 * with the following structure:
 *
 * ```
 * looper_sessions/
 *   {session-uuid}/
 *     metadata.json   -- Serialized [LooperSession] metadata
 *     dry.wav          -- Unprocessed (dry) recording
 *     wet.wav          -- Processed (wet) recording
 *     reamp_*.wav      -- Optional re-amped versions (future feature)
 * ```
 *
 * All file I/O operations are synchronous and should be called from a
 * background thread or coroutine (same pattern as [PresetManager]).
 *
 * @param context Application context for accessing [Context.getFilesDir].
 */
class LooperSessionManager(private val context: Context) {

    companion object {
        private const val TAG = "LooperSessionManager"
        private const val SESSIONS_DIR = "looper_sessions"
        private const val METADATA_FILE = "metadata.json"
        private const val DRY_WAV_FILE = "dry.wav"
        private const val WET_WAV_FILE = "wet.wav"
    }

    /** Root directory for all looper session data. */
    private val sessionsDir: File = File(context.filesDir, SESSIONS_DIR)

    init {
        if (!sessionsDir.exists()) {
            sessionsDir.mkdirs()
        }
    }

    // =========================================================================
    // Public API
    // =========================================================================

    /**
     * Save a looper session to internal storage.
     *
     * Creates a directory for the session, writes the metadata JSON, and
     * moves the temporary WAV files (dry and wet) into the session directory.
     * If the source WAV files don't exist at the paths specified in the
     * session, they are skipped with a warning log (the metadata is still saved).
     *
     * @param session The session to save. [LooperSession.dryFilePath] and
     *     [LooperSession.wetFilePath] should point to temporary WAV files
     *     that will be MOVED (not copied) into the session directory.
     */
    fun saveSession(session: LooperSession) {
        try {
            val sessionDir = File(sessionsDir, session.id)
            if (!sessionDir.exists()) {
                sessionDir.mkdirs()
            }

            // Move temporary WAV files into the session directory
            val dryDest = File(sessionDir, DRY_WAV_FILE)
            val wetDest = File(sessionDir, WET_WAV_FILE)

            val drySource = File(session.dryFilePath)
            if (drySource.exists()) {
                // Prefer rename (atomic, O(1)) over copy+delete (doubles disk I/O).
                // Falls back to copy+delete if rename fails (cross-filesystem move).
                if (!drySource.renameTo(dryDest)) {
                    drySource.copyTo(dryDest, overwrite = true)
                    drySource.delete()
                }
            } else {
                Log.w(TAG, "Dry WAV not found at: ${session.dryFilePath}")
            }

            val wetSource = File(session.wetFilePath)
            if (wetSource.exists()) {
                if (!wetSource.renameTo(wetDest)) {
                    wetSource.copyTo(wetDest, overwrite = true)
                    wetSource.delete()
                }
            } else {
                Log.w(TAG, "Wet WAV not found at: ${session.wetFilePath}")
            }

            // Update file paths in the session to reflect final location
            val updatedSession = session.copy(
                dryFilePath = dryDest.absolutePath,
                wetFilePath = wetDest.absolutePath
            )

            // Write metadata JSON
            val metadataFile = File(sessionDir, METADATA_FILE)
            metadataFile.writeText(updatedSession.toJson().toString(2))

            Log.d(TAG, "Saved session: ${session.name} (${session.id})")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to save session: ${session.name}", e)
        }
    }

    /**
     * Load a session by its unique ID.
     *
     * @param id UUID string of the session.
     * @return The loaded [LooperSession] or null if not found or corrupted.
     */
    fun loadSession(id: String): LooperSession? {
        return try {
            val metadataFile = File(sessionsDir, "$id/$METADATA_FILE")
            if (!metadataFile.exists()) {
                Log.w(TAG, "Session metadata not found: $id")
                return null
            }
            val json = JSONObject(metadataFile.readText())
            LooperSession.fromJson(json)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load session: $id", e)
            null
        }
    }

    /**
     * List all saved sessions, sorted by creation date (newest first).
     *
     * Skips any session directories with corrupted or missing metadata.
     *
     * @return List of all valid sessions, sorted by [LooperSession.createdAt]
     *     descending. Returns empty list if no sessions exist or on error.
     */
    fun getAllSessions(): List<LooperSession> {
        val sessions = mutableListOf<LooperSession>()
        val dirs = sessionsDir.listFiles { file -> file.isDirectory } ?: return emptyList()

        for (dir in dirs) {
            try {
                val metadataFile = File(dir, METADATA_FILE)
                if (!metadataFile.exists()) {
                    Log.w(TAG, "Skipping session dir without metadata: ${dir.name}")
                    continue
                }
                val json = JSONObject(metadataFile.readText())
                LooperSession.fromJson(json).let { sessions.add(it) }
            } catch (e: Exception) {
                Log.w(TAG, "Skipping corrupted session: ${dir.name}", e)
            }
        }

        return sessions.sortedByDescending { it.createdAt }
    }

    /**
     * Delete a session and all its files (metadata, WAVs, re-amps).
     *
     * @param id UUID string of the session to delete.
     */
    fun deleteSession(id: String) {
        try {
            val sessionDir = File(sessionsDir, id)
            if (sessionDir.exists()) {
                // Delete all files in the session directory, then the directory itself
                sessionDir.deleteRecursively()
                Log.d(TAG, "Deleted session: $id")
            } else {
                Log.w(TAG, "Session not found for deletion: $id")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to delete session: $id", e)
        }
    }

    /**
     * Toggle the favorite status of a session.
     *
     * Loads the session metadata, flips [LooperSession.isFavorite], and
     * writes the updated metadata back to disk.
     *
     * @param id UUID string of the session to toggle.
     */
    fun toggleFavorite(id: String) {
        try {
            val metadataFile = File(sessionsDir, "$id/$METADATA_FILE")
            if (!metadataFile.exists()) {
                Log.w(TAG, "Session not found for favorite toggle: $id")
                return
            }

            val json = JSONObject(metadataFile.readText())
            val session = LooperSession.fromJson(json)
            val toggled = session.copy(isFavorite = !session.isFavorite)
            metadataFile.writeText(toggled.toJson().toString(2))

            Log.d(TAG, "Toggled favorite for session: $id -> ${toggled.isFavorite}")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to toggle favorite: $id", e)
        }
    }

    /**
     * Get the directory for a session's files.
     *
     * Useful for accessing WAV files or adding re-amped versions.
     * The directory may not exist yet if [saveSession] hasn't been called.
     *
     * @param id UUID string of the session.
     * @return [File] pointing to the session directory.
     */
    fun getSessionDir(id: String): File {
        return File(sessionsDir, id)
    }

    /**
     * Calculate the total storage used by all looper sessions.
     *
     * Recursively sums file sizes across all session directories,
     * including metadata, WAV files, and any re-amped versions.
     *
     * @return Total size in bytes. Returns 0 if no sessions exist.
     */
    fun getTotalStorageBytes(): Long {
        return try {
            sessionsDir.walkTopDown()
                .filter { it.isFile }
                .sumOf { it.length() }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to calculate storage size", e)
            0L
        }
    }
}
