#include <jni.h>
#include <memory>
#include "audio_engine.h"

/**
 * JNI bridge between Kotlin AudioEngine.kt and the C++ AudioEngine class.
 *
 * Uses a static shared_ptr to manage the engine lifecycle. shared_ptr is
 * required (rather than unique_ptr) so that onErrorAfterClose() can capture
 * shared_from_this() to prevent use-after-free when the restart thread
 * outlives a concurrent stopEngine() call.
 *
 * The engine is created lazily on the first call to startEngine() and
 * destroyed when stopEngine() is called. This matches the Android foreground
 * service lifecycle.
 *
 * Thread safety: JNI calls from Kotlin are serialized by the ViewModel,
 * so concurrent access to the engine pointer is not a concern.
 * The engine's internal state uses atomics for audio thread safety.
 */

namespace {
    // Single global engine instance, managed by start/stop JNI calls.
    // Uses shared_ptr so onErrorAfterClose() can capture shared_from_this()
    // to prevent use-after-free if the engine is destroyed during restart.
    std::shared_ptr<AudioEngine> gEngine;

    // Bounds check for effect index. Returns true if index is valid.
    // Defense-in-depth: C++ layer also checks, but validating at the JNI
    // boundary prevents any future C++ code from receiving bad indices.
    inline bool isValidEffectIndex(jint index) {
        return gEngine && index >= 0 && index < gEngine->getEffectCount();
    }
}

/**
 * Called automatically when System.loadLibrary("guitar_engine") runs.
 * Creates the engine eagerly so that all subsequent JNI configuration
 * calls (setInputDeviceId, setEffectEnabled, etc.) have a valid gEngine
 * to write to -- even before startEngine() opens audio streams.
 *
 * Previously, gEngine was created lazily inside startEngine(). This
 * meant any JNI setter called before startEngine() silently no-oped
 * against nullptr, which caused cold-start bugs:
 *   - USB device IDs not applied → streams opened on phone mic/speaker
 *   - Master gains reset to defaults
 *   - Effect states from Kotlin never reached C++
 */
JNIEXPORT jint JNI_OnLoad(JavaVM* /* vm */, void* /* reserved */) {
    // Destroy any stale engine from a previous app instance (e.g., after APK update).
    // The old engine may hold Oboe stream pointers to closed kernel resources.
    // Without this, gEngine persists across dlopen/dlclose cycles on some devices,
    // and the new instance inherits dangling stream handles that block audio startup.
    if (gEngine) {
        gEngine->stop();
        gEngine.reset();
    }
    gEngine = std::make_shared<AudioEngine>();
    return JNI_VERSION_1_6;
}

extern "C" {

/**
 * Start the audio engine. Creates a new engine instance if one does not
 * exist, then opens audio streams and begins processing.
 *
 * @return JNI_TRUE on success, JNI_FALSE on failure.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_startEngine(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (!gEngine) {
        gEngine = std::make_shared<AudioEngine>();
    }
    return gEngine->start() ? JNI_TRUE : JNI_FALSE;
}

/**
 * Stop the audio engine and release all audio resources.
 * The engine instance is kept alive so that subsequent JNI configuration
 * calls (setEffectEnabled, setInputGain, etc.) can reach it before the
 * next startEngine(). Previously, gEngine.reset() destroyed the engine
 * here, causing all configuration calls between stop/start to silently
 * no-op against a null pointer -- a latent bug exposed by Sprint 18 P5's
 * change to enabled_{false} in the Effect base class.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_stopEngine(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        gEngine->stop();
        // NOTE: We intentionally do NOT call gEngine.reset() here.
        // The engine persists so that Kotlin can configure effects/gains
        // before the next start(). The C++ AudioEngine::start() handles
        // being called on a stopped-but-existing instance correctly.
    }
}

/**
 * Set the preferred input source type.
 * @param type 0=PHONE_MIC, 1=ANALOG_ADAPTER, 2=USB_AUDIO
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setInputSource(
        JNIEnv* /* env */, jobject /* thiz */, jint type) {
    if (gEngine) {
        gEngine->setInputSource(static_cast<int>(type));
    }
}

/**
 * Set the input device ID for the recording stream.
 * Must be called before startEngine() to take effect.
 * @param deviceId Android AudioDeviceInfo ID, or 0 for system default.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setInputDeviceId(
        JNIEnv* /* env */, jobject /* thiz */, jint deviceId) {
    if (gEngine) {
        gEngine->setInputDeviceId(static_cast<int32_t>(deviceId));
    }
}

/**
 * Set the output device ID for the playback stream.
 * Must be called before startEngine() to take effect.
 * Critical for USB audio interfaces: ensures processed audio routes
 * back through USB (to iRig headphones/amp out) instead of the phone speaker.
 * @param deviceId Android AudioDeviceInfo ID, or 0 for system default.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setOutputDeviceId(
        JNIEnv* /* env */, jobject /* thiz */, jint deviceId) {
    if (gEngine) {
        gEngine->setOutputDeviceId(static_cast<int32_t>(deviceId));
    }
}

/**
 * Check if the audio engine is currently running.
 * @return JNI_TRUE if running, JNI_FALSE otherwise.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_isRunning(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return gEngine->isRunning() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

/**
 * Get the current input signal RMS level.
 * @return Float in [0.0, 1.0] representing input loudness.
 */
JNIEXPORT jfloat JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_getInputLevel(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return gEngine->getInputLevel();
    }
    return 0.0f;
}

/**
 * Get the current output signal RMS level.
 * @return Float in [0.0, 1.0] representing output loudness.
 */
JNIEXPORT jfloat JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_getOutputLevel(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return gEngine->getOutputLevel();
    }
    return 0.0f;
}

/**
 * Get the number of effects in the signal chain.
 * @return Number of effects (26 for the default chain).
 */
JNIEXPORT jint JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_getEffectCount(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return static_cast<jint>(gEngine->getEffectCount());
    }
    return 0;
}

/**
 * Enable or disable an effect by its index in the chain.
 *
 * @param effectIndex Zero-based index: 0=NoiseGate, 1=Compressor, 2=Wah,
 *                    3=Boost, 4=Overdrive, 5=AmpModel, 6=CabinetSim,
 *                    7=ParametricEQ, 8=Chorus, 9=Vibrato, 10=Phaser,
 *                    11=Flanger, 12=Delay, 13=Reverb, 14=Tuner,
 *                    15=Tremolo, 16=BossDS1, 17=ProCoRAT, 18=BossDS2,
 *                    19=BossHM2, 20=Univibe, 21=FuzzFace,
 *                    22=Rangemaster, 23=BigMuff, 24=Octavia,
 *                    25=RotarySpeaker
 *                    (for default chain).
 * @param enabled     JNI_TRUE to enable, JNI_FALSE to bypass.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setEffectEnabled(
        JNIEnv* /* env */, jobject /* thiz */,
        jint effectIndex, jboolean enabled) {
    if (isValidEffectIndex(effectIndex)) {
        gEngine->setEffectEnabled(static_cast<int>(effectIndex),
                                  enabled == JNI_TRUE);
    }
}

/**
 * Set a parameter on an effect.
 *
 * @param effectIndex Zero-based effect index in the chain.
 * @param paramId     Effect-specific parameter ID (see effect headers).
 * @param value       New parameter value (range depends on the parameter).
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setEffectParameter(
        JNIEnv* /* env */, jobject /* thiz */,
        jint effectIndex, jint paramId, jfloat value) {
    if (isValidEffectIndex(effectIndex)) {
        gEngine->setEffectParameter(static_cast<int>(effectIndex),
                                    static_cast<int>(paramId),
                                    static_cast<float>(value));
    }
}

/**
 * Get a parameter from an effect.
 *
 * @param effectIndex Zero-based effect index in the chain.
 * @param paramId     Effect-specific parameter ID.
 * @return Current parameter value.
 */
JNIEXPORT jfloat JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_getEffectParameter(
        JNIEnv* /* env */, jobject /* thiz */,
        jint effectIndex, jint paramId) {
    if (isValidEffectIndex(effectIndex)) {
        return static_cast<jfloat>(
            gEngine->getEffectParameter(static_cast<int>(effectIndex),
                                        static_cast<int>(paramId)));
    }
    return 0.0f;
}

/**
 * Set the wet/dry mix for an effect.
 *
 * @param effectIndex Zero-based effect index in the chain.
 * @param mix         0.0 = fully dry (original signal),
 *                    1.0 = fully wet (processed signal).
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setEffectWetDryMix(
        JNIEnv* /* env */, jobject /* thiz */,
        jint effectIndex, jfloat mix) {
    if (isValidEffectIndex(effectIndex)) {
        gEngine->setEffectWetDryMix(static_cast<int>(effectIndex),
                                    static_cast<float>(mix));
    }
}

/**
 * Get the wet/dry mix for an effect.
 *
 * @param effectIndex Zero-based effect index in the chain.
 * @return Current wet/dry mix (0.0 = fully dry, 1.0 = fully wet).
 */
JNIEXPORT jfloat JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_getEffectWetDryMix(
        JNIEnv* /* env */, jobject /* thiz */,
        jint effectIndex) {
    if (isValidEffectIndex(effectIndex)) {
        return static_cast<jfloat>(
            gEngine->getEffectWetDryMix(static_cast<int>(effectIndex)));
    }
    return 1.0f;
}

/**
 * Check if an effect is currently enabled.
 *
 * @param effectIndex Zero-based effect index in the chain.
 * @return JNI_TRUE if enabled, JNI_FALSE if bypassed.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_isEffectEnabled(
        JNIEnv* /* env */, jobject /* thiz */,
        jint effectIndex) {
    if (isValidEffectIndex(effectIndex)) {
        return gEngine->isEffectEnabled(static_cast<int>(effectIndex))
            ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

/**
 * Get the device's native sample rate.
 * @return Sample rate in Hz, or 0 if the engine is not running.
 */
JNIEXPORT jint JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_getSampleRate(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return static_cast<jint>(gEngine->getSampleRate());
    }
    return 0;
}

/**
 * Get the current frames per buffer (burst size).
 * @return Frames per buffer, or 0 if the engine is not running.
 */
JNIEXPORT jint JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_getFramesPerBuffer(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return static_cast<jint>(gEngine->getFramesPerBuffer());
    }
    return 0;
}

/**
 * Get estimated round-trip latency in milliseconds.
 * @return Latency in ms, or 0 if the engine is not running.
 */
JNIEXPORT jfloat JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_getEstimatedLatencyMs(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return static_cast<jfloat>(gEngine->getEstimatedLatencyMs());
    }
    return 0.0f;
}

/**
 * Set the audio buffer size multiplier (1..8).
 * Applies immediately to the open output stream if the engine is running.
 * @param multiplier Buffer size = framesPerBurst * multiplier.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setBufferMultiplier(
        JNIEnv* /* env */, jobject /* thiz */, jint multiplier) {
    if (gEngine) {
        gEngine->setBufferMultiplier(static_cast<int>(multiplier));
    }
}

// =============================================================================
// Master gain and bypass controls
// =============================================================================

/**
 * Set the master input gain applied before effects processing.
 * @param gain Linear gain (0.0 = silence, 1.0 = unity, 2.0 = +6dB).
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setInputGain(
        JNIEnv* /* env */, jobject /* thiz */, jfloat gain) {
    if (gEngine) {
        gEngine->setInputGain(static_cast<float>(gain));
    }
}

/**
 * Set the master output gain applied after effects processing.
 * @param gain Linear gain (0.0 = silence, 1.0 = unity).
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setOutputGain(
        JNIEnv* /* env */, jobject /* thiz */, jfloat gain) {
    if (gEngine) {
        gEngine->setOutputGain(static_cast<float>(gain));
    }
}

/**
 * Set global bypass. When true, audio passes through without effects.
 * @param bypassed JNI_TRUE to bypass all effects, JNI_FALSE to process normally.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setGlobalBypass(
        JNIEnv* /* env */, jobject /* thiz */, jboolean bypassed) {
    if (gEngine) {
        gEngine->setGlobalBypass(bypassed == JNI_TRUE);
    }
}

/**
 * Get the current master input gain.
 * @return Linear gain value.
 */
JNIEXPORT jfloat JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_getInputGain(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return static_cast<jfloat>(gEngine->getInputGain());
    }
    return 1.0f;
}

/**
 * Get the current master output gain.
 * @return Linear gain value.
 */
JNIEXPORT jfloat JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_getOutputGain(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return static_cast<jfloat>(gEngine->getOutputGain());
    }
    return 1.0f;
}

/**
 * Check if global bypass is active.
 * @return JNI_TRUE if bypassed, JNI_FALSE if processing normally.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_isGlobalBypassed(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return gEngine->isGlobalBypassed() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

/**
 * Set the processing order of effects in the signal chain.
 * Enables runtime drag-and-drop reordering of the pedalboard.
 *
 * @param order IntArray of physical effect indices in desired processing order.
 * @return JNI_TRUE on success, JNI_FALSE if the array length mismatches.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setEffectOrder(
        JNIEnv* env, jobject /* thiz */, jintArray order) {
    if (!gEngine || order == nullptr) {
        return JNI_FALSE;
    }

    jint length = env->GetArrayLength(order);
    jint* data = env->GetIntArrayElements(order, nullptr);
    if (data == nullptr) {
        return JNI_FALSE;
    }

    bool result = gEngine->setEffectOrder(data, static_cast<int>(length));
    env->ReleaseIntArrayElements(order, data, JNI_ABORT);
    return result ? JNI_TRUE : JNI_FALSE;
}

/**
 * Get the current processing order of effects.
 * @return IntArray of physical effect indices in processing order.
 */
JNIEXPORT jintArray JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_getEffectOrder(
        JNIEnv* env, jobject /* thiz */) {
    if (!gEngine) {
        return nullptr;
    }

    int count = gEngine->getEffectCount();
    jintArray result = env->NewIntArray(count);
    if (result == nullptr) {
        return nullptr;
    }

    // Heap-allocate via vector (26 ints, negligible cost on UI thread)
    std::vector<int> order(count);
    gEngine->getEffectOrder(order.data(), count);

    env->SetIntArrayRegion(result, 0, count, order.data());
    return result;
}

/**
 * Load a user-provided cabinet impulse response.
 *
 * Decodes float samples from the Kotlin side (WAV already parsed)
 * and passes them to the CabinetSim effect for convolution.
 *
 * @param irData     Float array of IR samples (mono, normalized to [-1,1]).
 * @param sampleRate Sample rate the IR was recorded at.
 * @return JNI_TRUE on success, JNI_FALSE on failure.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_loadCabinetIR(
        JNIEnv* env, jobject /* thiz */,
        jfloatArray irData, jint sampleRate) {
    if (!gEngine || irData == nullptr) {
        return JNI_FALSE;
    }

    jint length = env->GetArrayLength(irData);
    if (length <= 0) {
        return JNI_FALSE;
    }

    // Get a pointer to the Java float array data.
    // Using GetFloatArrayElements with isCopy=nullptr -- JNI may return
    // a direct pointer or a copy depending on the VM implementation.
    jfloat* data = env->GetFloatArrayElements(irData, nullptr);
    if (data == nullptr) {
        return JNI_FALSE;
    }

    bool result = gEngine->loadCabinetIR(
        data, static_cast<int>(length), static_cast<int>(sampleRate));

    // Release the array (JNI_ABORT = don't copy changes back, we only read)
    env->ReleaseFloatArrayElements(irData, data, JNI_ABORT);

    return result ? JNI_TRUE : JNI_FALSE;
}

/**
 * Load a neural amp model (.nam file) into the AmpModel effect.
 *
 * The path must be an absolute filesystem path to a .nam model file
 * (typically copied from a content URI to the app's cache directory).
 *
 * The model is parsed on this thread (non-audio), then atomically
 * swapped into the audio path. Returns false if the file is missing,
 * malformed, or uses an unsupported architecture.
 *
 * @param path Absolute filesystem path to the .nam model file.
 * @return JNI_TRUE on success, JNI_FALSE on failure.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_loadNeuralModel(
        JNIEnv* env, jobject /* thiz */, jstring path) {
    if (!gEngine || path == nullptr) {
        return JNI_FALSE;
    }

    const char* pathChars = env->GetStringUTFChars(path, nullptr);
    if (pathChars == nullptr) {
        return JNI_FALSE;
    }

    std::string pathStr(pathChars);
    env->ReleaseStringUTFChars(path, pathChars);

    return gEngine->loadNeuralModel(pathStr) ? JNI_TRUE : JNI_FALSE;
}

/**
 * Clear the loaded neural amp model, reverting to classic waveshaping.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_clearNeuralModel(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        gEngine->clearNeuralModel();
    }
}

/**
 * Check if a neural amp model is currently loaded and active.
 * @return JNI_TRUE if neural mode is active, JNI_FALSE otherwise.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_isNeuralModelLoaded(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return gEngine->isNeuralModelLoaded() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

// =============================================================================
// Tuner activation (on-demand pitch analysis)
// =============================================================================

/**
 * Activate or deactivate the tuner's pitch analysis.
 * When inactive, the tuner's FFT processing is completely skipped,
 * eliminating its CPU cost from the audio callback.
 * Should be activated when the tuner UI is visible, deactivated when hidden.
 *
 * @param active JNI_TRUE to start pitch analysis, JNI_FALSE to stop.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setTunerActive(
        JNIEnv* /* env */, jobject /* thiz */, jboolean active) {
    if (gEngine) {
        gEngine->setTunerActive(active == JNI_TRUE);
    }
}

/**
 * Check if the tuner is currently active.
 * @return JNI_TRUE if tuner pitch analysis is running, JNI_FALSE otherwise.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_isTunerActive(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return gEngine->isTunerActive() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

// =============================================================================
// Spectrum analyzer
// =============================================================================

/**
 * Get the current spectrum data for UI visualization.
 * Returns 64 dB-scaled magnitude bins in logarithmic frequency spacing
 * (20Hz to 20kHz). Lock-free read from double-buffered output.
 *
 * @return FloatArray of 64 bins with values in [-90, 0] dB.
 */
JNIEXPORT jfloatArray JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_getSpectrumData(
        JNIEnv* env, jobject /* thiz */) {
    static constexpr int kNumBins = 64;

    jfloatArray result = env->NewFloatArray(kNumBins);
    if (result == nullptr) {
        return nullptr;
    }

    if (gEngine) {
        float bins[kNumBins];
        gEngine->getSpectrumData(bins, kNumBins);
        env->SetFloatArrayRegion(result, 0, kNumBins, bins);
    } else {
        // Engine not running: return array of -90 dB (silence floor)
        float silence[kNumBins];
        for (int i = 0; i < kNumBins; ++i) {
            silence[i] = -90.0f;
        }
        env->SetFloatArrayRegion(result, 0, kNumBins, silence);
    }

    return result;
}

// =============================================================================
// Looper engine
// =============================================================================

/**
 * Initialize looper buffers. Safe to call multiple times.
 * @param maxDurationSeconds Maximum loop duration in seconds.
 * @return JNI_TRUE on success.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_looperInit(
        JNIEnv* /* env */, jobject /* thiz */, jint maxDurationSeconds) {
    if (gEngine) {
        auto sr = gEngine->getSampleRate();
        if (sr > 0) {
            gEngine->getLooperEngine().init(static_cast<float>(sr),
                                             static_cast<float>(maxDurationSeconds));
            return JNI_TRUE;
        }
    }
    return JNI_FALSE;
}

/**
 * Release looper buffers and reset state.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_looperRelease(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        gEngine->getLooperEngine().release();
    }
}

/**
 * Send a transport command to the looper.
 * @param command 0=None, 1=Record, 2=Stop, 3=Overdub, 4=Clear, 5=Undo, 6=Toggle
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_looperSendCommand(
        JNIEnv* /* env */, jobject /* thiz */, jint command) {
    if (gEngine) {
        gEngine->getLooperEngine().sendCommand(
            static_cast<LooperEngine::LooperCommand>(command));
    }
}

/**
 * Get the current looper state.
 * @return 0=Idle, 1=Recording, 2=Playing, 3=Overdubbing, 4=Finishing, 5=CountIn
 */
JNIEXPORT jint JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_looperGetState(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return static_cast<jint>(gEngine->getLooperEngine().getState());
    }
    return 0; // IDLE
}

/**
 * Get the current playback position in samples.
 * @return Playback position for waveform cursor display.
 */
JNIEXPORT jint JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_looperGetPlaybackPosition(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return static_cast<jint>(gEngine->getLooperEngine().getPlaybackPosition());
    }
    return 0;
}

/**
 * Get the total loop length in samples.
 * @return Loop length, or 0 if no loop recorded.
 */
JNIEXPORT jint JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_looperGetLoopLength(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return static_cast<jint>(gEngine->getLooperEngine().getLoopLength());
    }
    return 0;
}

/**
 * Set the loop playback volume.
 * @param volume 0.0 = silent, 1.0 = unity gain.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_looperSetVolume(
        JNIEnv* /* env */, jobject /* thiz */, jfloat volume) {
    if (gEngine) {
        gEngine->getLooperEngine().setVolume(static_cast<float>(volume));
    }
}

/**
 * Enable or disable the metronome click.
 * @param active JNI_TRUE to enable, JNI_FALSE to disable.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setMetronomeActive(
        JNIEnv* /* env */, jobject /* thiz */, jboolean active) {
    if (gEngine) {
        gEngine->getLooperEngine().setMetronomeActive(active == JNI_TRUE);
    }
}

/**
 * Set the metronome BPM.
 * @param bpm Tempo in beats per minute (clamped to 30-300).
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setMetronomeBPM(
        JNIEnv* /* env */, jobject /* thiz */, jfloat bpm) {
    if (gEngine) {
        gEngine->getLooperEngine().setBPM(static_cast<float>(bpm));
    }
}

/**
 * Set the time signature.
 * @param beatsPerBar Numerator (1-16).
 * @param beatUnit    Denominator (2, 4, 8, or 16).
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setTimeSignature(
        JNIEnv* /* env */, jobject /* thiz */, jint beatsPerBar, jint beatUnit) {
    if (gEngine) {
        gEngine->getLooperEngine().setBeatsPerBar(static_cast<int>(beatsPerBar));
        gEngine->getLooperEngine().setBeatUnit(static_cast<int>(beatUnit));
    }
}

/**
 * Get the current beat index within the bar (0-based).
 * @return Beat index for metronome indicator UI.
 */
JNIEXPORT jint JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_getCurrentBeat(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return static_cast<jint>(gEngine->getLooperEngine().getCurrentBeat());
    }
    return 0;
}

/**
 * Register a tap for tap-tempo BPM detection.
 * @param timestampMs Monotonic timestamp from System.currentTimeMillis().
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_onTapTempo(
        JNIEnv* /* env */, jobject /* thiz */, jlong timestampMs) {
    if (gEngine) {
        gEngine->getLooperEngine().onTap(static_cast<int64_t>(timestampMs));
    }
}

/**
 * Get the most recently detected tap-tempo BPM.
 * @return BPM from tap detection, or 0.0 if fewer than 2 taps.
 */
JNIEXPORT jfloat JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_getTapTempoBPM(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return static_cast<jfloat>(gEngine->getLooperEngine().getTapBPM());
    }
    return 0.0f;
}

/**
 * Enable or disable quantized (bar-snapping) mode.
 * @param enabled JNI_TRUE for quantized mode, JNI_FALSE for free-form.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setQuantizedMode(
        JNIEnv* /* env */, jobject /* thiz */, jboolean enabled) {
    if (gEngine) {
        gEngine->getLooperEngine().setQuantizedMode(enabled == JNI_TRUE);
    }
}

/**
 * Set the number of bars for quantized loop length.
 * @param numBars Number of bars (clamped to 1-64).
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setQuantizedBars(
        JNIEnv* /* env */, jobject /* thiz */, jint numBars) {
    if (gEngine) {
        gEngine->getLooperEngine().setNumBars(static_cast<int>(numBars));
    }
}

/**
 * Enable or disable count-in (one bar of metronome clicks before recording).
 * @param enabled JNI_TRUE to enable count-in, JNI_FALSE to disable.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setCountInEnabled(
        JNIEnv* /* env */, jobject /* thiz */, jboolean enabled) {
    if (gEngine) {
        gEngine->getLooperEngine().setCountInEnabled(enabled == JNI_TRUE);
    }
}

/**
 * Set the metronome click volume.
 * @param volume 0.0 = silent, 1.0 = full volume.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setMetronomeVolume(
        JNIEnv* /* env */, jobject /* thiz */, jfloat volume) {
    if (gEngine) {
        gEngine->getLooperEngine().setClickVolume(static_cast<float>(volume));
    }
}

/**
 * Set the metronome tone type.
 * @param tone Tone index (0-5): Classic, Soft, Rim, Cowbell, Hi-Hat, Deep.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setMetronomeTone(
        JNIEnv* /* env */, jobject /* thiz */, jint tone) {
    if (gEngine) {
        gEngine->getLooperEngine().setMetronomeTone(static_cast<int>(tone));
    }
}

/**
 * Get the current metronome tone type.
 * @return Tone index (0-5).
 */
JNIEXPORT jint JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_getMetronomeTone(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return static_cast<jint>(gEngine->getLooperEngine().getMetronomeTone());
    }
    return 0;
}

// =============================================================================
// Loop export and re-amping
// =============================================================================

/**
 * Export current loop buffers to WAV files on disk.
 * @param dryPath Absolute path for the dry WAV file.
 * @param wetPath Absolute path for the wet WAV file.
 * @return JNI_TRUE on success.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_looperExportLoop(
        JNIEnv* env, jobject /* thiz */, jstring dryPath, jstring wetPath) {
    if (!gEngine) return JNI_FALSE;

    const char* dryPathC = env->GetStringUTFChars(dryPath, nullptr);
    const char* wetPathC = env->GetStringUTFChars(wetPath, nullptr);

    bool result = gEngine->getLooperEngine().exportLoop(
        std::string(dryPathC), std::string(wetPathC));

    env->ReleaseStringUTFChars(dryPath, dryPathC);
    env->ReleaseStringUTFChars(wetPath, wetPathC);

    return result ? JNI_TRUE : JNI_FALSE;
}

/**
 * Load a dry WAV file into the re-amp buffer.
 * @param path Absolute path to the mono float32 WAV file.
 * @return JNI_TRUE if the file was loaded successfully.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_loadDryForReamp(
        JNIEnv* env, jobject /* thiz */, jstring path) {
    if (!gEngine) return JNI_FALSE;

    const char* pathC = env->GetStringUTFChars(path, nullptr);
    bool result = gEngine->getLooperEngine().loadWavForReamp(std::string(pathC));
    env->ReleaseStringUTFChars(path, pathC);

    return result ? JNI_TRUE : JNI_FALSE;
}

/**
 * Start re-amp playback. Dry signal replaces live input through the effects chain.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_startReamp(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        gEngine->getLooperEngine().startReamp();
    }
}

/**
 * Stop re-amp playback and return to live input.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_stopReamp(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        gEngine->getLooperEngine().stopReamp();
    }
}

/**
 * Check if re-amp playback is currently active.
 * @return JNI_TRUE if re-amping.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_isReamping(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return gEngine->getLooperEngine().isReamping() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

/**
 * Start linear recording to disk.
 * @param dryPath Path for the dry WAV output file.
 * @param wetPath Path for the wet WAV output file.
 * @return JNI_TRUE if recording started.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_startLinearRecording(
        JNIEnv* env, jobject /* thiz */, jstring dryPath, jstring wetPath) {
    if (!gEngine) return JNI_FALSE;

    const char* dryPathC = env->GetStringUTFChars(dryPath, nullptr);
    const char* wetPathC = env->GetStringUTFChars(wetPath, nullptr);

    bool result = gEngine->getLooperEngine().startLinearRecording(
        std::string(dryPathC), std::string(wetPathC));

    env->ReleaseStringUTFChars(dryPath, dryPathC);
    env->ReleaseStringUTFChars(wetPath, wetPathC);

    return result ? JNI_TRUE : JNI_FALSE;
}

/**
 * Stop linear recording and finalize WAV files.
 * @return JNI_TRUE if recording was stopped successfully.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_stopLinearRecording(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (!gEngine) return JNI_FALSE;
    return gEngine->getLooperEngine().stopLinearRecording() ? JNI_TRUE : JNI_FALSE;
}

// =============================================================================
// Loop trim / crop
// =============================================================================

/**
 * Enter trim preview mode. Playback is constrained to [trimStart, trimEnd).
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_startTrimPreview(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        gEngine->getLooperEngine().startTrimPreview();
    }
}

/**
 * Cancel trim preview without committing. Restores full loop playback.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_cancelTrimPreview(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        gEngine->getLooperEngine().cancelTrimPreview();
    }
}

/**
 * Set the trim start offset in samples.
 * @param startSample Desired start position.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setTrimStart(
        JNIEnv* /* env */, jobject /* thiz */, jint startSample) {
    if (gEngine) {
        gEngine->getLooperEngine().setTrimStart(static_cast<int32_t>(startSample));
    }
}

/**
 * Set the trim end offset in samples.
 * @param endSample Desired end position.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_setTrimEnd(
        JNIEnv* /* env */, jobject /* thiz */, jint endSample) {
    if (gEngine) {
        gEngine->getLooperEngine().setTrimEnd(static_cast<int32_t>(endSample));
    }
}

/**
 * Get current trim start position.
 * @return Trim start in samples.
 */
JNIEXPORT jint JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_getTrimStart(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return static_cast<jint>(gEngine->getLooperEngine().getTrimStart());
    }
    return 0;
}

/**
 * Get current trim end position.
 * @return Trim end in samples.
 */
JNIEXPORT jint JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_getTrimEnd(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return static_cast<jint>(gEngine->getLooperEngine().getTrimEnd());
    }
    return 0;
}

/**
 * Check if trim preview is active.
 * @return JNI_TRUE if in trim preview mode.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_isTrimPreviewActive(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return gEngine->getLooperEngine().isTrimPreviewActive() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

/**
 * Commit the current trim. Memmoves buffer data and re-applies crossfade.
 * @return JNI_TRUE on success.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_commitTrim(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return gEngine->getLooperEngine().commitTrim() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

/**
 * Check if crop undo is available.
 * @return JNI_TRUE if the last crop can be undone.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_isCropUndoAvailable(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return gEngine->getLooperEngine().isCropUndoAvailable() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

/**
 * Undo the last crop operation.
 * @return JNI_TRUE if the crop was undone successfully.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_undoCrop(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return gEngine->getLooperEngine().undoCrop() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

/**
 * Get a waveform summary for UI display.
 * Returns min/max pairs per column: [min0, max0, min1, max1, ...].
 *
 * @param numColumns Number of display columns.
 * @param startSample Start of range to summarize.
 * @param endSample End of range (-1 = entire loop).
 * @return FloatArray of size numColumns*2, or null on failure.
 */
JNIEXPORT jfloatArray JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_getWaveformSummary(
        JNIEnv* env, jobject /* thiz */,
        jint numColumns, jint startSample, jint endSample) {
    if (!gEngine || numColumns <= 0) return nullptr;

    const int32_t arraySize = numColumns * 2;
    std::vector<float> minMax(arraySize);

    const int32_t filled = gEngine->getLooperEngine().getWaveformSummary(
        minMax.data(), static_cast<int32_t>(numColumns),
        static_cast<int32_t>(startSample), static_cast<int32_t>(endSample));

    if (filled <= 0) return nullptr;

    jfloatArray result = env->NewFloatArray(arraySize);
    if (result == nullptr) return nullptr;

    env->SetFloatArrayRegion(result, 0, arraySize, minMax.data());
    return result;
}

/**
 * Find the nearest zero crossing to the given sample position.
 * @param targetSample Position to snap.
 * @param searchRadius Search distance in samples (-1 = ~1ms).
 * @return Nearest zero-crossing position.
 */
JNIEXPORT jint JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_findNearestZeroCrossing(
        JNIEnv* /* env */, jobject /* thiz */,
        jint targetSample, jint searchRadius) {
    if (gEngine) {
        return static_cast<jint>(gEngine->getLooperEngine().findNearestZeroCrossing(
            static_cast<int32_t>(targetSample), static_cast<int32_t>(searchRadius)));
    }
    return targetSample;
}

/**
 * Snap a sample position to the nearest beat boundary.
 * @param targetSample Position to snap.
 * @return Snapped position.
 */
JNIEXPORT jint JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_snapToBeat(
        JNIEnv* /* env */, jobject /* thiz */, jint targetSample) {
    if (gEngine) {
        return static_cast<jint>(gEngine->getLooperEngine().snapToBeat(
            static_cast<int32_t>(targetSample)));
    }
    return targetSample;
}

// =============================================================================
// Looper per-layer volume control
// =============================================================================

/**
 * Set the volume of an individual looper layer.
 * @param layer Layer index (0 = base recording, 1+ = overdub passes).
 * @param volume Linear gain (0.0 = silent, 1.0 = unity, up to 1.5 = boost).
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_looperSetLayerVolume(
        JNIEnv* /* env */, jobject /* thiz */, jint layer, jfloat volume) {
    if (gEngine) {
        gEngine->getLooperEngine().setLayerVolume(
            static_cast<int>(layer), static_cast<float>(volume));
    }
}

/**
 * Get the current volume of a looper layer.
 * @param layer Layer index.
 * @return Volume in [0.0, 1.5], or 1.0 if out of range.
 */
JNIEXPORT jfloat JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_looperGetLayerVolume(
        JNIEnv* /* env */, jobject /* thiz */, jint layer) {
    if (gEngine) {
        return static_cast<jfloat>(gEngine->getLooperEngine().getLayerVolume(
            static_cast<int>(layer)));
    }
    return 1.0f;
}

/**
 * Get the number of recorded looper layers.
 * @return 0 if no loop, 1 after base recording, 2+ after overdubs.
 */
JNIEXPORT jint JNICALL
Java_com_guitaremulator_app_audio_AudioEngine_looperGetLayerCount(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return static_cast<jint>(gEngine->getLooperEngine().getLayerCount());
    }
    return 0;
}

// =============================================================================
// Drum engine
// =============================================================================

/**
 * Set the drum sequencer playing state.
 * @param playing JNI_TRUE to start, JNI_FALSE to stop.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetPlaying(
        JNIEnv* /* env */, jobject /* thiz */, jboolean playing) {
    if (gEngine) {
        gEngine->getDrumEngine().setPlaying(playing == JNI_TRUE);
    }
}

/**
 * Check if the drum sequencer is playing.
 * @return JNI_TRUE if playing.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeIsPlaying(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return gEngine->getDrumEngine().isPlaying() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

/**
 * Set the drum sequencer tempo.
 * @param bpm Tempo in BPM (20-300).
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetTempo(
        JNIEnv* /* env */, jobject /* thiz */, jfloat bpm) {
    if (gEngine) {
        gEngine->getDrumEngine().setTempo(static_cast<float>(bpm));
    }
}

/**
 * Get the current drum sequencer tempo.
 * @return Tempo in BPM.
 */
JNIEXPORT jfloat JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeGetTempo(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return static_cast<jfloat>(gEngine->getDrumEngine().getTempo());
    }
    return 120.0f;
}

/**
 * Record a tap for tap tempo calculation.
 * @param timestampNs Monotonic timestamp in nanoseconds.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeTapTempo(
        JNIEnv* /* env */, jobject /* thiz */, jlong timestampNs) {
    if (gEngine) {
        gEngine->getDrumEngine().tapTempo(static_cast<int64_t>(timestampNs));
    }
}

/**
 * Set the swing percentage.
 * @param swing 50 (straight) to 75 (heavy shuffle).
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetSwing(
        JNIEnv* /* env */, jobject /* thiz */, jfloat swing) {
    if (gEngine) {
        gEngine->getDrumEngine().setSwing(static_cast<float>(swing));
    }
}

/**
 * Get the current swing percentage.
 * @return Swing percentage (50-75).
 */
JNIEXPORT jfloat JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeGetSwing(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return static_cast<jfloat>(gEngine->getDrumEngine().getSwing());
    }
    return 50.0f;
}

/**
 * Set the drum mix level relative to guitar signal.
 * @param level Linear gain (0.0-2.0).
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetMixLevel(
        JNIEnv* /* env */, jobject /* thiz */, jfloat level) {
    if (gEngine) {
        gEngine->setDrumMixLevel(static_cast<float>(level));
    }
}

/**
 * Set fill mode active/inactive.
 * @param active JNI_TRUE to activate fill mode.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetFillActive(
        JNIEnv* /* env */, jobject /* thiz */, jboolean active) {
    if (gEngine) {
        gEngine->getDrumEngine().setFillActive(active == JNI_TRUE);
    }
}

/**
 * Get the current step position for UI display.
 * @return Step index (0 to patternLength-1).
 */
JNIEXPORT jint JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeGetCurrentStep(
        JNIEnv* /* env */, jobject /* thiz */) {
    if (gEngine) {
        return static_cast<jint>(gEngine->getDrumEngine().getCurrentStep());
    }
    return 0;
}

/**
 * Set the tempo multiplier (half-time/double-time).
 * @param multiplier 0.5=half, 1.0=normal, 2.0=double.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetTempoMultiplier(
        JNIEnv* /* env */, jobject /* thiz */, jfloat multiplier) {
    if (gEngine) {
        gEngine->getDrumEngine().setTempoMultiplier(static_cast<float>(multiplier));
    }
}

/**
 * Queue a pattern index for switching on the next downbeat.
 * @param patternIndex Index to queue, or -1 to cancel.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeQueuePattern(
        JNIEnv* /* env */, jobject /* thiz */, jint patternIndex) {
    if (gEngine) {
        gEngine->getDrumEngine().queuePattern(static_cast<int>(patternIndex));
    }
}

// --- Track controls ---

/**
 * Set a track's mute state.
 * @param track Track index (0-7).
 * @param muted JNI_TRUE to mute.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetTrackMute(
        JNIEnv* /* env */, jobject /* thiz */, jint track, jboolean muted) {
    if (gEngine) {
        gEngine->getDrumEngine().getTrack(static_cast<int>(track))
            .setMuted(muted == JNI_TRUE);
    }
}

/**
 * Get a track's mute state.
 * @param track Track index (0-7).
 * @return JNI_TRUE if muted.
 */
JNIEXPORT jboolean JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeGetTrackMute(
        JNIEnv* /* env */, jobject /* thiz */, jint track) {
    if (gEngine) {
        return gEngine->getDrumEngine().getTrack(static_cast<int>(track))
            .isMuted() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

/**
 * Set a track's volume level.
 * @param track Track index (0-7).
 * @param volume Volume in [0.0, 1.0].
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetTrackVolume(
        JNIEnv* /* env */, jobject /* thiz */, jint track, jfloat volume) {
    if (gEngine) {
        gEngine->getDrumEngine().getTrack(static_cast<int>(track))
            .setVolume(static_cast<float>(volume));
    }
}

/**
 * Set a track's pan position.
 * @param track Track index (0-7).
 * @param pan Pan in [-1.0 (left), 1.0 (right)].
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetTrackPan(
        JNIEnv* /* env */, jobject /* thiz */, jint track, jfloat pan) {
    if (gEngine) {
        gEngine->getDrumEngine().getTrack(static_cast<int>(track))
            .setPan(static_cast<float>(pan));
    }
}

/**
 * Set a voice synthesis parameter on a track.
 * @param track Track index (0-7).
 * @param paramId Voice-specific parameter ID (0-15).
 * @param value Parameter value.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetVoiceParam(
        JNIEnv* /* env */, jobject /* thiz */,
        jint track, jint paramId, jfloat value) {
    if (gEngine) {
        auto* voice = gEngine->getDrumEngine()
            .getTrack(static_cast<int>(track)).getVoice();
        if (voice) {
            voice->setParam(static_cast<int>(paramId), static_cast<float>(value));
        }
    }
}

/**
 * Get a voice synthesis parameter from a track.
 * @param track Track index (0-7).
 * @param paramId Voice-specific parameter ID (0-15).
 * @return Parameter value, or 0.0 if no voice.
 */
JNIEXPORT jfloat JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeGetVoiceParam(
        JNIEnv* /* env */, jobject /* thiz */,
        jint track, jint paramId) {
    if (gEngine) {
        const auto* voice = gEngine->getDrumEngine()
            .getTrack(static_cast<int>(track)).getVoice();
        if (voice) {
            return static_cast<jfloat>(
                voice->getParam(static_cast<int>(paramId)));
        }
    }
    return 0.0f;
}

/**
 * Trigger a drum pad (finger drumming).
 * @param track Track index (0-7).
 * @param velocity Velocity in [0.0, 1.0].
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeTriggerPad(
        JNIEnv* /* env */, jobject /* thiz */, jint track, jfloat velocity) {
    if (gEngine) {
        gEngine->getDrumEngine().triggerPad(
            static_cast<int>(track), static_cast<float>(velocity));
    }
}

// --- Pattern editing ---

/**
 * Set a step's trigger and velocity.
 * @param track Track index (0-7).
 * @param step Step index (0-63).
 * @param velocity 0=off, 1-127=on with velocity.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetStep(
        JNIEnv* /* env */, jobject /* thiz */,
        jint track, jint step, jint velocity) {
    if (gEngine) {
        gEngine->getDrumEngine().setStep(
            static_cast<int>(track), static_cast<int>(step),
            static_cast<int>(velocity));
    }
}

/**
 * Set a step's probability.
 * @param track Track index (0-7).
 * @param step Step index (0-63).
 * @param percent Probability 1-100.
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetStepProb(
        JNIEnv* /* env */, jobject /* thiz */,
        jint track, jint step, jint percent) {
    if (gEngine) {
        gEngine->getDrumEngine().setStepProbability(
            static_cast<int>(track), static_cast<int>(step),
            static_cast<int>(percent));
    }
}

/**
 * Set a step's retrig count.
 * @param track Track index (0-7).
 * @param step Step index (0-63).
 * @param count Retrig count (0=off, 2, 3, 4, 6, 8).
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetStepRetrig(
        JNIEnv* /* env */, jobject /* thiz */,
        jint track, jint step, jint count) {
    if (gEngine) {
        gEngine->getDrumEngine().setStepRetrig(
            static_cast<int>(track), static_cast<int>(step),
            static_cast<int>(count));
    }
}

/**
 * Set the global pattern length.
 * @param length Pattern length in steps (1-64).
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetPatternLength(
        JNIEnv* /* env */, jobject /* thiz */, jint length) {
    if (gEngine) {
        gEngine->getDrumEngine().setPatternLength(static_cast<int>(length));
    }
}

/**
 * Set a track's drive amount.
 * @param track Track index (0-7).
 * @param drive Drive saturation amount (0-5).
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetTrackDrive(
        JNIEnv* /* env */, jobject /* thiz */, jint track, jfloat drive) {
    if (gEngine) {
        gEngine->getDrumEngine().getTrack(static_cast<int>(track))
            .setDrive(static_cast<float>(drive));
    }
}

/**
 * Set a track's filter cutoff frequency.
 * @param track Track index (0-7).
 * @param cutoff Cutoff in Hz (20-16000).
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetTrackFilterCutoff(
        JNIEnv* /* env */, jobject /* thiz */, jint track, jfloat cutoff) {
    if (gEngine) {
        gEngine->getDrumEngine().getTrack(static_cast<int>(track))
            .setFilterCutoff(static_cast<float>(cutoff));
    }
}

/**
 * Set a track's filter resonance.
 * @param track Track index (0-7).
 * @param resonance Resonance in [0.0, 1.0].
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetTrackFilterRes(
        JNIEnv* /* env */, jobject /* thiz */, jint track, jfloat resonance) {
    if (gEngine) {
        gEngine->getDrumEngine().getTrack(static_cast<int>(track))
            .setFilterRes(static_cast<float>(resonance));
    }
}

/**
 * Set a track's choke group.
 * Tracks in the same choke group (>= 0) mute each other on trigger.
 * @param track Track index (0-7).
 * @param group Choke group (-1=none, 0=hat group, 1-3=user groups).
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetTrackChokeGroup(
        JNIEnv* /* env */, jobject /* thiz */, jint track, jint group) {
    if (gEngine) {
        gEngine->getDrumEngine().getTrack(static_cast<int>(track))
            .setChokeGroup(static_cast<int8_t>(group));
    }
}

/**
 * Set the voice engine type for a drum track.
 * Creates a new synthesis voice matching the requested type.
 *
 * @param track Track index (0-7).
 * @param engineType Engine type (0=FM, 1=Subtractive, 2=Metallic, 3=Noise, 4=Physical, 5=MultiLayer).
 */
JNIEXPORT void JNICALL
Java_com_guitaremulator_app_audio_DrumEngine_nativeSetTrackEngineType(
        JNIEnv* /* env */, jobject /* thiz */, jint track, jint engineType) {
    if (gEngine) {
        gEngine->getDrumEngine().setTrackEngineType(
            static_cast<int>(track), static_cast<int>(engineType));
    }
}

} // extern "C"
