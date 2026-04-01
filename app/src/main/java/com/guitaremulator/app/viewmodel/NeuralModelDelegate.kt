package com.guitaremulator.app.viewmodel

import android.app.Application
import com.guitaremulator.app.audio.IAudioEngine
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/**
 * Delegate handling neural amp model (.nam file) loading and lifecycle.
 *
 * Neural amp models are loaded from SAF content URIs, copied to the app
 * cache (because NeuralAmpModelerCore requires a filesystem path), and
 * then passed to the native engine for initialization. On success, the
 * AmpModel effect is automatically enabled.
 *
 * @property audioEngine JNI bridge to the native audio engine.
 * @property coroutineScope Scope for launching async loading operations
 *     (typically viewModelScope).
 * @property getApplication Lambda returning the Application context for
 *     file operations.
 * @property enableEffectIfDisabled Lambda that enables an effect at the
 *     given index only if it is currently disabled. Used to auto-enable
 *     AmpModel after loading a neural model.
 */
class NeuralModelDelegate(
    private val audioEngine: IAudioEngine,
    private val coroutineScope: CoroutineScope,
    private val getApplication: () -> Application,
    private val enableEffectIfDisabled: (Int) -> Unit
) {

    // =========================================================================
    // Constants
    // =========================================================================

    /** Index of the Amp Model effect in the signal chain. */
    val ampModelIndex = 5

    companion object {
        /** Maximum .nam file size (200 MB). Largest known NAM models are ~50 MB. */
        const val MAX_NAM_FILE_SIZE = 200L * 1_048_576L // 200 MB
    }

    // =========================================================================
    // Observable State
    // =========================================================================

    /** Name of the currently loaded neural amp model file (empty if none). */
    private val _neuralModelName = MutableStateFlow("")
    val neuralModelName: StateFlow<String> = _neuralModelName.asStateFlow()

    /** True while a neural model is being loaded (file copy + model init). */
    private val _isLoadingNeuralModel = MutableStateFlow(false)
    val isLoadingNeuralModel: StateFlow<Boolean> = _isLoadingNeuralModel.asStateFlow()

    /** Error message from the most recent neural model load attempt. */
    private val _neuralModelError = MutableStateFlow("")
    val neuralModelError: StateFlow<String> = _neuralModelError.asStateFlow()

    /** Whether a neural amp model is currently active. */
    private val _isNeuralModelLoaded = MutableStateFlow(false)
    val isNeuralModelLoaded: StateFlow<Boolean> = _isNeuralModelLoaded.asStateFlow()

    // =========================================================================
    // Public API
    // =========================================================================

    /** Clear any neural model error message. */
    fun clearNeuralModelError() {
        _neuralModelError.value = ""
    }

    /**
     * Load a neural amp model (.nam file) from a content URI.
     *
     * Copies the file from the SAF content URI to the app's cache directory
     * (because NeuralAmpModelerCore requires a filesystem path), then passes
     * the path to the native engine for parsing and initialization.
     *
     * On success, enables the AmpModel effect (index 5) so the user hears
     * the neural model immediately.
     *
     * @param uri      Content URI from the file picker (SAF).
     * @param fileName Display name of the .nam file for the UI.
     */
    fun loadNeuralModel(uri: android.net.Uri, fileName: String = "Neural Model") {
        coroutineScope.launch {
            _isLoadingNeuralModel.value = true
            _neuralModelError.value = ""

            try {
                // Copy .nam file from content URI to cache (IO thread).
                // NAMCore needs a real filesystem path; content URIs won't work.
                // Uses a unique filename to prevent races with concurrent loads.
                val cachedPath = withContext(Dispatchers.IO) {
                    val context = getApplication()
                    val inputStream = context.contentResolver.openInputStream(uri)
                        ?: throw IllegalStateException("Cannot open .nam file")
                    val cacheFile = java.io.File(
                        context.cacheDir, "model_${System.nanoTime()}.nam"
                    )
                    inputStream.use { input ->
                        cacheFile.outputStream().use { output ->
                            var totalBytes = 0L
                            val buffer = ByteArray(8192)
                            var bytesRead: Int
                            while (input.read(buffer).also { bytesRead = it } != -1) {
                                totalBytes += bytesRead
                                if (totalBytes > MAX_NAM_FILE_SIZE) {
                                    cacheFile.delete()
                                    throw IllegalStateException(
                                        "File too large (max ${MAX_NAM_FILE_SIZE / 1_048_576} MB)"
                                    )
                                }
                                output.write(buffer, 0, bytesRead)
                            }
                        }
                    }
                    // Clean up old cached model files
                    context.cacheDir.listFiles()?.forEach { file ->
                        if (file.name.startsWith("model_") &&
                            file.name.endsWith(".nam") &&
                            file != cacheFile
                        ) {
                            file.delete()
                        }
                    }
                    cacheFile.absolutePath
                }

                // Load the model in the native engine (IO thread for heavy work).
                val loaded = withContext(Dispatchers.IO) {
                    audioEngine.loadNeuralModel(cachedPath)
                }

                if (loaded) {
                    _neuralModelName.value = fileName
                    _isNeuralModelLoaded.value = true

                    // Enable AmpModel effect if not already enabled
                    enableEffectIfDisabled(ampModelIndex)
                } else {
                    _neuralModelError.value =
                        "Failed to load model. The file may be corrupted or use an unsupported format."
                }
            } catch (e: Exception) {
                _neuralModelError.value = "Error loading model: ${e.message}"
            } finally {
                _isLoadingNeuralModel.value = false
            }
        }
    }

    /**
     * Clear the loaded neural model, reverting to classic waveshaping.
     */
    fun clearNeuralModel() {
        audioEngine.clearNeuralModel()
        _neuralModelName.value = ""
        _isNeuralModelLoaded.value = false
    }

    /**
     * Reset neural model state when the engine is stopped.
     *
     * The native engine is destroyed on stop, so any loaded neural model
     * is gone. This resets the Kotlin-side state to match.
     */
    fun resetOnEngineStop() {
        _neuralModelName.value = ""
        _isNeuralModelLoaded.value = false
    }
}
