#ifndef GUITAR_ENGINE_AUDIO_ENGINE_H
#define GUITAR_ENGINE_AUDIO_ENGINE_H

#include <oboe/Oboe.h>
#include <oboe/LatencyTuner.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <cmath>

// ADPF Performance Hints (Android 12+ / API 31+)
#if __ANDROID_API__ >= 31
#include <android/performance_hint_manager.h>
#endif

#include "effects/effect.h"
#include "signal_chain.h"
#include "utils/atomic_params.h"
#include "dsp/spectrum_analyzer.h"
#include "looper/looper_engine.h"
#include "drums/drum_engine.h"

/**
 * Core audio engine that manages full-duplex audio I/O using Oboe.
 *
 * Architecture:
 *   - Separate input (recording) and output (playback) streams for full-duplex operation.
 *   - The output stream drives the audio callback via onAudioReady().
 *   - Inside the callback, input samples are read synchronously from the input stream,
 *     processed through the effects chain, and written to the output buffer.
 *   - All callback code is real-time safe: no allocations, no locks, no logging.
 *
 * Input source types for JNI:
 *   0 = PHONE_MIC (built-in microphone)
 *   1 = ANALOG_ADAPTER (wired headset/adapter with mic input)
 *   2 = USB_AUDIO (USB audio interface)
 */
class AudioEngine : public oboe::AudioStreamCallback,
                    public std::enable_shared_from_this<AudioEngine> {
public:
    AudioEngine();
    ~AudioEngine();

    // Non-copyable, non-movable (due to callback pointers held by Oboe)
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    /**
     * Start the audio engine. Opens input and output streams using the
     * device's native sample rate and optimal buffer size.
     * @return true on success, false if stream creation fails.
     */
    bool start();

    /** Stop the audio engine and close both streams. */
    void stop();

    /** Check if the audio engine is currently running. Thread-safe. */
    bool isRunning() const { return running_.load(std::memory_order_acquire); }

    /**
     * Set the preferred input source type.
     * @param type 0=PHONE_MIC, 1=ANALOG_ADAPTER, 2=USB_AUDIO
     */
    void setInputSource(int type);

    /**
     * Set the input device ID for the recording stream.
     * When > 0, the stream builder will use this specific device instead of
     * the system default. Required for reliable USB audio interface routing.
     * Must be called before start() or before a restart to take effect.
     * @param deviceId Android AudioDeviceInfo ID, or 0 for system default.
     */
    void setInputDeviceId(int32_t deviceId);

    /**
     * Set the output device ID for the playback stream.
     * When > 0, the stream builder will use this specific device instead of
     * the system default. Required for routing processed audio back through
     * USB audio interfaces (iRig, Focusrite, etc.) instead of the phone speaker.
     * Must be called before start() or before a restart to take effect.
     * @param deviceId Android AudioDeviceInfo ID, or 0 for system default.
     */
    void setOutputDeviceId(int32_t deviceId);

    /**
     * Set the buffer size multiplier (1..8).
     * Buffer size = framesPerBurst * multiplier. Higher = more latency, fewer glitches.
     * If the engine is running, the new multiplier is applied to the open output stream.
     * @param multiplier Buffer multiplier (clamped to 1..8).
     */
    void setBufferMultiplier(int multiplier);

    /** Get the current RMS input level in [0.0, 1.0]. Real-time safe read. */
    float getInputLevel() const { return inputLevel_.load(std::memory_order_acquire); }

    /** Get the current RMS output level in [0.0, 1.0]. Real-time safe read. */
    float getOutputLevel() const { return outputLevel_.load(std::memory_order_acquire); }

    /**
     * Set the master input gain applied before effects processing.
     * @param gain Linear gain (0.0 = silence, 1.0 = unity, 2.0 = +6dB).
     */
    void setInputGain(float gain) {
        params_.inputGain.set(std::clamp(gain, 0.0f, 4.0f));
    }

    /**
     * Set the master output gain applied after effects processing.
     * @param gain Linear gain (0.0 = silence, 1.0 = unity, max 4.0 = +12dB).
     */
    void setOutputGain(float gain) {
        params_.outputGain.set(std::clamp(gain, 0.0f, 4.0f));
    }

    /**
     * Set global bypass: when true, audio passes through with no processing.
     * @param bypassed True to bypass all effects.
     */
    void setGlobalBypass(bool bypassed) { params_.bypassed.set(bypassed); }

    /** Get the current master input gain. */
    float getInputGain() const { return params_.inputGain.get(); }

    /** Get the current master output gain. */
    float getOutputGain() const { return params_.outputGain.get(); }

    /** Check if global bypass is active. */
    bool isGlobalBypassed() const { return params_.bypassed.get(); }

    // ----- Effect chain control (called from JNI) -----

    /** Get the number of effects in the signal chain. */
    int getEffectCount() const { return signalChain_.getEffectCount(); }

    /**
     * Enable or disable an effect by its chain index.
     * @param effectIndex Zero-based index of the effect.
     * @param enabled     True to enable, false to bypass.
     */
    void setEffectEnabled(int effectIndex, bool enabled) {
        Effect* effect = signalChain_.getEffect(effectIndex);
        if (effect) {
            effect->setEnabled(enabled);
        }
    }

    /**
     * Set a parameter on an effect.
     * @param effectIndex Zero-based index of the effect.
     * @param paramId     Effect-specific parameter ID.
     * @param value       New parameter value.
     */
    void setEffectParameter(int effectIndex, int paramId, float value) {
        signalChain_.setEffectParameter(effectIndex, paramId, value);
    }

    /**
     * Get a parameter from an effect.
     * @param effectIndex Zero-based index of the effect.
     * @param paramId     Effect-specific parameter ID.
     * @return Current parameter value.
     */
    float getEffectParameter(int effectIndex, int paramId) const {
        return signalChain_.getEffectParameter(effectIndex, paramId);
    }

    /**
     * Set the wet/dry mix for an effect.
     * @param effectIndex Zero-based index of the effect.
     * @param mix         0.0 = fully dry, 1.0 = fully wet.
     */
    void setEffectWetDryMix(int effectIndex, float mix) {
        Effect* effect = signalChain_.getEffect(effectIndex);
        if (effect) {
            effect->setWetDryMix(mix);
        }
    }

    /**
     * Get the wet/dry mix for an effect.
     * @param effectIndex Zero-based index of the effect.
     * @return Current wet/dry mix, or 1.0 if index is out of range.
     */
    float getEffectWetDryMix(int effectIndex) const {
        const Effect* effect = signalChain_.getEffect(effectIndex);
        if (effect) {
            return effect->getWetDryMix();
        }
        return 1.0f;
    }

    /**
     * Check if an effect is currently enabled.
     * @param effectIndex Zero-based index of the effect.
     * @return true if enabled, false if bypassed or index is out of range.
     */
    bool isEffectEnabled(int effectIndex) const {
        const Effect* effect = signalChain_.getEffect(effectIndex);
        if (effect) {
            return effect->isEnabled();
        }
        return false;
    }

    // ----- Effect reordering (called from JNI) -----

    /**
     * Set the processing order of effects.
     * Thread-safe: uses double-buffered atomic swap (no locks).
     * @param order Array of physical effect indices in desired processing order.
     * @param count Number of entries (must equal getEffectCount()).
     * @return true if applied successfully.
     */
    bool setEffectOrder(const int* order, int count) {
        return signalChain_.setEffectOrder(order, count);
    }

    /**
     * Get the current processing order.
     * @param out   Output array to receive the order.
     * @param count Size of the output array.
     */
    void getEffectOrder(int* out, int count) const {
        signalChain_.getEffectOrder(out, count);
    }

    // ----- Neural amp model management (called from JNI) -----

    /**
     * Load a neural amp model from a .nam file.
     * Routes to AmpModel via the signal chain.
     * NOT real-time safe. Call from a non-audio thread.
     *
     * @param path Absolute filesystem path to the .nam file.
     * @return true if the model was loaded successfully.
     */
    bool loadNeuralModel(const std::string& path) {
        return signalChain_.loadNeuralModel(path);
    }

    /**
     * Clear the loaded neural model, reverting to classic waveshaping.
     */
    void clearNeuralModel() {
        signalChain_.clearNeuralModel();
    }

    /**
     * Check if a neural amp model is currently loaded and active.
     * @return true if neural mode is active.
     */
    bool isNeuralModelLoaded() const {
        return signalChain_.isNeuralModelLoaded();
    }

    // ----- Looper access (called from JNI bridge) -----

    /**
     * Get a reference to the looper engine for JNI bridge access.
     * The looper is parallel to the signal chain (not an effect in the chain).
     */
    LooperEngine& getLooperEngine() { return looperEngine_; }

    // ----- Drum engine access (called from JNI bridge) -----

    /**
     * Get a reference to the drum engine for JNI bridge access.
     * The drum engine is parallel to the signal chain (mixed after looper).
     */
    drums::DrumEngine& getDrumEngine() { return drumEngine_; }

    /**
     * Set the drum mix level relative to the guitar signal.
     * @param level Linear gain (0.0 = silent, 1.0 = full level).
     */
    void setDrumMixLevel(float level) {
        drumMixLevel_.store(std::clamp(level, 0.0f, 2.0f),
                            std::memory_order_relaxed);
    }

    // ----- Spectrum analyzer (called from JNI for UI visualization) -----

    /**
     * Get the current spectrum data for UI visualization.
     * Reads from the lock-free double buffer (no contention with audio thread).
     * @param bins   Output array for dB magnitude values [-90, 0].
     * @param count  Number of bins to read (typically 64).
     */
    void getSpectrumData(float* bins, int count) const {
        spectrumAnalyzer_.getSpectrum(bins, count);
    }

    /**
     * Activate or deactivate the tuner's pitch analysis.
     * When inactive, feedTuner() is skipped entirely — zero CPU cost.
     * The tuner is off by default and should be activated only when the
     * user opens the tuner UI.
     *
     * On false→true transition: prepares tuner state (skip counter,
     * phase offset, buffer clear), boosts the output buffer to prevent
     * LatencyTuner underrun cascade from the sudden FFT load, and
     * resets the LatencyTuner so it can re-converge from the new floor.
     */
    void setTunerActive(bool active);

    /** Check if the tuner is currently active. */
    bool isTunerActive() const {
        return tunerActive_.load(std::memory_order_acquire);
    }

    /** Get the current sample rate discovered from the device. */
    int32_t getSampleRate() const { return sampleRate_; }

    /** Get the current frames per buffer (burst size). */
    int32_t getFramesPerBuffer() const { return framesPerBuffer_; }

    /**
     * Load a user-provided cabinet impulse response.
     *
     * Routes through the signal chain to CabinetSim::loadUserIR().
     * NOT real-time safe (allocates memory in FFTConvolver::init).
     * Safe to call from any non-audio thread. The CabinetSim uses an
     * internal mutex to synchronize with the audio thread.
     *
     * @param irData      Pointer to float IR samples.
     * @param irLength    Number of samples in the IR.
     * @param irSampleRate Sample rate the IR was recorded at.
     * @return true if the IR was loaded successfully.
     */
    bool loadCabinetIR(const float* irData, int irLength, int irSampleRate);

    /**
     * Get estimated round-trip latency in milliseconds.
     * Combines input and output stream latencies when available.
     */
    float getEstimatedLatencyMs() const {
        if (!inputStream_ || !outputStream_) return 0.0f;
        double latencyMs = 0.0;
        // Output latency
        auto outputLatency = outputStream_->calculateLatencyMillis();
        if (outputLatency) {
            latencyMs += outputLatency.value();
        }
        // Input latency (estimated from buffer size if not directly available)
        auto inputLatency = inputStream_->calculateLatencyMillis();
        if (inputLatency) {
            latencyMs += inputLatency.value();
        } else {
            // Estimate from buffer size
            latencyMs += static_cast<double>(framesPerBuffer_) /
                         static_cast<double>(sampleRate_) * 1000.0;
        }
        return static_cast<float>(latencyMs);
    }

    // oboe::AudioStreamCallback implementation
    oboe::DataCallbackResult onAudioReady(
        oboe::AudioStream* outputStream,
        void* audioData,
        int32_t numFrames) override;

    void onErrorBeforeClose(oboe::AudioStream* stream, oboe::Result error) override;
    void onErrorAfterClose(oboe::AudioStream* stream, oboe::Result error) override;

private:
    /** Open the input (recording) stream. */
    bool openInputStream();

    /** Open the output (playback) stream. */
    bool openOutputStream();

    /** Close and release both streams. */
    void closeStreams();

    /**
     * Calculate RMS level from an audio buffer. Real-time safe.
     * Uses a simple sum-of-squares approach without sqrt for performance,
     * then applies sqrt only once at the end.
     */
    static float calculateRmsLevel(const float* buffer, int numSamples);

    // Audio streams
    std::shared_ptr<oboe::AudioStream> inputStream_;
    std::shared_ptr<oboe::AudioStream> outputStream_;

    // Adaptive buffer tuning: automatically increases buffer size when
    // underruns (xruns) are detected, then tries to reduce after stability.
    // This handles transient CPU load spikes from UI rendering or
    // background CPU frequency scaling without permanently increasing latency.
    std::unique_ptr<oboe::LatencyTuner> latencyTuner_;

    // Stream properties discovered at runtime
    int32_t sampleRate_ = 0;
    int32_t framesPerBuffer_ = 0;

    // Channel counts discovered from the actual opened streams.
    // Guitar input is mono, but USB interfaces may report stereo.
    // We track these separately so the signal chain always processes mono
    // while I/O adapts to whatever the device provides.
    int32_t inputChannelCount_ = 1;
    int32_t outputChannelCount_ = 1;

    // Intermediate buffer for reading input samples in the output callback
    std::vector<float> inputBuffer_;

    // Signal chain manages ordered effects and wet/dry mixing
    SignalChain signalChain_;

    // Spectrum analyzer: passive tap on the post-effects output for UI visualization.
    // Mutable because getSpectrum() is const but reads from atomic double buffer.
    mutable SpectrumAnalyzer spectrumAnalyzer_;

    // Looper engine (parallel to signal chain, not an effect).
    // Manages multi-layer loop recording, playback, overdub with crossfading,
    // quantization, metronome, and tap tempo. Processed in onAudioReady()
    // AFTER the signal chain + soft limiter.
    LooperEngine looperEngine_;

    // Dry signal capture buffer for looper (pre-allocated in start()).
    // Captures the post-inputGain, pre-effects signal for re-amp support.
    std::vector<float> dryCapture_;

    // Drum engine (parallel to signal chain, mixed after looper).
    // Outputs stereo audio that is summed with the mono guitar signal
    // during the stereo expansion stage of onAudioReady().
    drums::DrumEngine drumEngine_;

    // Pre-allocated stereo drum output buffers (sized in start()).
    std::vector<float> drumBufferL_;
    std::vector<float> drumBufferR_;

    // Drum mix level: linear gain applied to the drum bus before summing
    // with guitar. 0.0 = silent, 0.8 = default, 1.0+ = boosted.
    std::atomic<float> drumMixLevel_{0.8f};

    // Engine state
    std::atomic<bool> running_{false};
    std::atomic<bool> allowRestart_{true}; // Cleared by stop(), prevents restart after intentional shutdown
    std::atomic<int> inputSourceType_{0};
    std::atomic<int32_t> inputDeviceId_{0};   // 0 = system default
    std::atomic<int32_t> outputDeviceId_{0};  // 0 = system default
    std::atomic<int> bufferMultiplier_{2};  // Buffer size = framesPerBurst * multiplier (1..8)
    std::atomic<bool> tunerActive_{false};  // Tuner off by default; activated from UI
    std::atomic<int> restartAttempts_{0};   // Tracks consecutive restart failures for backoff
    static constexpr int kMaxRestartAttempts = 3; // Give up after 3 failed restarts

    // Level metering (written on audio thread, read from UI thread)
    std::atomic<float> inputLevel_{0.0f};
    std::atomic<float> outputLevel_{0.0f};

    // Shared audio parameters
    AudioParams params_;

    // DC blocker state (one-pole highpass at 10 Hz, sample-rate adaptive)
    // R = exp(-2*pi*10/fs). 64-bit state for filter stability.
    double dcBlockPrevIn_ = 0.0;
    double dcBlockPrevOut_ = 0.0;
    double dcBlockR_ = 0.99869; // default for 48kHz, recomputed in start()

    // First-callback initialization flag. Used to:
    //  1. Set ARM FTZ (flush-to-zero) on the audio thread's FPCR register
    //  2. Create the ADPF performance hint session with the correct thread ID
    // Must be outside the API guard because FTZ applies to all API levels.
    std::atomic<bool> adpfInitialized_{false};

    // ADPF Performance Hints (Android 12+ / API 31+)
    // Reports actual audio callback work duration to the system so the CPU
    // governor can boost clock speed when needed and throttle when idle,
    // preventing both underruns (too slow) and unnecessary battery drain.
#if __ANDROID_API__ >= 31
    APerformanceHintManager* perfHintManager_ = nullptr;
    APerformanceHintSession* perfHintSession_ = nullptr;
#endif

    /**
     * Initialize ADPF performance hint session on the audio callback thread.
     * Called from onAudioReady() on the first callback so that gettid()
     * captures the correct (Oboe callback) thread ID.
     */
    void initPerformanceHints();

    /** Release ADPF resources. Called from stop(). */
    void releasePerformanceHints();
};

#endif // GUITAR_ENGINE_AUDIO_ENGINE_H
