package com.guitaremulator.app.data

import android.content.Context
import java.io.IOException
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.floatPreferencesKey
import androidx.datastore.preferences.core.intPreferencesKey
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.catch
import kotlinx.coroutines.flow.map

/**
 * User preferences persisted across app restarts via Jetpack DataStore.
 *
 * @property lastPresetId UUID of the last active preset (empty = none).
 * @property inputSourceType Native type int of the last selected input source (0=mic, 1=analog, 2=usb).
 * @property bufferMultiplier Audio buffer size multiplier (1..8, default 2).
 * @property inputGain Master input gain (0.0..4.0, default 1.0 = unity).
 * @property outputGain Master output gain (0.0..4.0, default 1.0 = unity).
 */
data class UserPreferences(
    val lastPresetId: String = "",
    val inputSourceType: Int = 0,
    val bufferMultiplier: Int = 2,
    val inputGain: Float = 1.0f,
    val outputGain: Float = 1.0f
)

// File-scope delegate (required by DataStore API — NOT inside a class)
private val Context.dataStore by preferencesDataStore(name = "user_preferences")

/**
 * Repository for reading and writing user preferences via Jetpack DataStore.
 *
 * All write methods are suspend functions that should be called from a coroutine.
 * The [userPreferences] flow emits the current preferences and updates on change.
 */
class UserPreferencesRepository(private val context: Context) {

    private object Keys {
        val LAST_PRESET_ID = stringPreferencesKey("last_preset_id")
        val INPUT_SOURCE_TYPE = intPreferencesKey("input_source_type")
        val BUFFER_MULTIPLIER = intPreferencesKey("buffer_multiplier")
        val INPUT_GAIN = floatPreferencesKey("input_gain")
        val OUTPUT_GAIN = floatPreferencesKey("output_gain")
    }

    /**
     * Observable flow of current user preferences.
     * Emits defaults on IOException (corrupted DataStore file).
     */
    val userPreferences: Flow<UserPreferences> = context.dataStore.data
        .catch { exception ->
            if (exception is IOException) {
                emit(androidx.datastore.preferences.core.emptyPreferences())
            } else {
                throw exception
            }
        }
        .map { prefs ->
            UserPreferences(
                lastPresetId = prefs[Keys.LAST_PRESET_ID] ?: "",
                inputSourceType = prefs[Keys.INPUT_SOURCE_TYPE] ?: 0,
                bufferMultiplier = (prefs[Keys.BUFFER_MULTIPLIER] ?: 2).coerceIn(1, 8),
                inputGain = (prefs[Keys.INPUT_GAIN] ?: 1.0f).coerceIn(0.0f, 4.0f),
                outputGain = (prefs[Keys.OUTPUT_GAIN] ?: 1.0f).coerceIn(0.0f, 4.0f)
            )
        }

    /** Persist the last active preset ID. */
    suspend fun setLastPresetId(presetId: String) {
        context.dataStore.edit { prefs ->
            prefs[Keys.LAST_PRESET_ID] = presetId
        }
    }

    /** Persist the selected input source type (0=mic, 1=analog, 2=usb). */
    suspend fun setInputSourceType(nativeType: Int) {
        context.dataStore.edit { prefs ->
            prefs[Keys.INPUT_SOURCE_TYPE] = nativeType
        }
    }

    /** Persist the buffer size multiplier (1..8). */
    suspend fun setBufferMultiplier(multiplier: Int) {
        context.dataStore.edit { prefs ->
            prefs[Keys.BUFFER_MULTIPLIER] = multiplier.coerceIn(1, 8)
        }
    }

    /** Persist the master input gain (0.0..4.0). */
    suspend fun setInputGain(gain: Float) {
        context.dataStore.edit { prefs ->
            prefs[Keys.INPUT_GAIN] = gain.coerceIn(0.0f, 4.0f)
        }
    }

    /** Persist the master output gain (0.0..4.0). */
    suspend fun setOutputGain(gain: Float) {
        context.dataStore.edit { prefs ->
            prefs[Keys.OUTPUT_GAIN] = gain.coerceIn(0.0f, 4.0f)
        }
    }
}
