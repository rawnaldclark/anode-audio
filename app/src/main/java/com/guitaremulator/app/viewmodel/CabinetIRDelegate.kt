package com.guitaremulator.app.viewmodel

import android.app.Application
import android.net.Uri
import com.guitaremulator.app.audio.IAudioEngine
import com.guitaremulator.app.audio.WavDecoder
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/**
 * Delegate handling user cabinet impulse response (IR) loading.
 *
 * Cabinet IRs are loaded from SAF content URIs, decoded from WAV format
 * on the IO dispatcher, then passed to the native engine via JNI. On
 * success, the Cabinet Sim effect's type parameter is set to "User IR"
 * and the effect is enabled.
 *
 * @property audioEngine JNI bridge to the native audio engine.
 * @property coroutineScope Scope for launching async loading operations
 *     (typically viewModelScope).
 * @property getApplication Lambda returning the Application context for
 *     content resolver access.
 * @property setEffectParameter Lambda to set an effect parameter by
 *     (effectIndex, paramId, value). Used to set cabinet type to User IR.
 * @property enableEffectIfDisabled Lambda that enables an effect at the
 *     given index only if it is currently disabled. Used to auto-enable
 *     CabinetSim after loading a user IR.
 */
class CabinetIRDelegate(
    private val audioEngine: IAudioEngine,
    private val coroutineScope: CoroutineScope,
    private val getApplication: () -> Application,
    private val setEffectParameter: (Int, Int, Float) -> Unit,
    private val enableEffectIfDisabled: (Int) -> Unit
) {

    // =========================================================================
    // Constants
    // =========================================================================

    /** Index of the Cabinet Sim effect in the signal chain. */
    val cabinetSimIndex = 6

    // =========================================================================
    // Observable State
    // =========================================================================

    /** Name of the currently loaded user cabinet IR file (empty if none). */
    private val _userIRName = MutableStateFlow("")
    val userIRName: StateFlow<String> = _userIRName.asStateFlow()

    /** True while a user IR is being loaded (WAV decode + FFT init). */
    private val _isLoadingIR = MutableStateFlow(false)
    val isLoadingIR: StateFlow<Boolean> = _isLoadingIR.asStateFlow()

    /** Error message from the most recent IR load attempt (empty on success). */
    private val _irLoadError = MutableStateFlow("")
    val irLoadError: StateFlow<String> = _irLoadError.asStateFlow()

    // =========================================================================
    // Public API
    // =========================================================================

    /** Clear any IR load error message (e.g., when the effect editor is dismissed). */
    fun clearIRLoadError() {
        _irLoadError.value = ""
    }

    /**
     * Load a user-provided cabinet impulse response from a content URI.
     *
     * Decodes the WAV file on the IO dispatcher, then passes the float
     * samples to the native engine via JNI. On success, sets the cabinet
     * type parameter to 3 (User IR) and enables the Cabinet Sim effect.
     *
     * @param uri      Content URI from the file picker (SAF).
     * @param fileName Display name of the file for the UI.
     */
    fun loadUserCabinetIR(uri: Uri, fileName: String = "Custom IR") {
        coroutineScope.launch {
            _isLoadingIR.value = true
            _irLoadError.value = ""

            // Step 1: Decode the WAV file on the IO dispatcher (file I/O)
            val wavResult = withContext(Dispatchers.IO) {
                try {
                    val context = getApplication()
                    val inputStream = context.contentResolver.openInputStream(uri)
                        ?: return@withContext Result.failure<WavDecoder.Companion>(
                            Exception("Failed to open file"))

                    val wavData = inputStream.use { stream ->
                        WavDecoder.decode(stream)
                    } ?: return@withContext Result.failure<WavDecoder.Companion>(
                        Exception("Invalid WAV file. Only .wav files (16/24-bit PCM or 32-bit float) are supported."))

                    if (wavData.samples.isEmpty()) {
                        return@withContext Result.failure<WavDecoder.Companion>(
                            Exception("WAV file contains no audio data"))
                    }

                    @Suppress("UNCHECKED_CAST")
                    Result.success(wavData) as Result<Any>
                } catch (e: SecurityException) {
                    Result.failure(Exception("Permission denied: cannot read this file"))
                } catch (e: Exception) {
                    Result.failure(Exception("Error loading IR file"))
                }
            }

            // Step 2: Pass decoded samples to the native engine on the Main thread.
            // The JNI loadCabinetIR call must run on Main to avoid a race with
            // stopEngine() which also runs on Main and resets the global engine pointer.
            val result: String? = if (wavResult.isSuccess) {
                val wavData = wavResult.getOrNull() as? com.guitaremulator.app.audio.WavData
                if (wavData != null) {
                    val loaded = audioEngine.loadCabinetIR(
                        wavData.samples, wavData.sampleRate
                    )
                    if (!loaded) "Failed to load IR into audio engine" else null
                } else {
                    "Unexpected decode error"
                }
            } else {
                wavResult.exceptionOrNull()?.message ?: "Unknown error"
            }

            _isLoadingIR.value = false

            if (result != null) {
                _irLoadError.value = result
            } else {
                _userIRName.value = fileName

                // Set cabinet type to 3 (User IR) and enable the effect
                setEffectParameter(cabinetSimIndex, 0, 3f)

                // Ensure CabinetSim is enabled
                enableEffectIfDisabled(cabinetSimIndex)
            }
        }
    }
}
