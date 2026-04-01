#include "audio_engine.h"
#include "effects/cabinet_sim.h"
#include "effects/fast_math.h"

#include <android/log.h>
#include <algorithm>
#include <cstring>
#include <thread>
#include <chrono>

// Logging macros -- these must NEVER be used inside onAudioReady().
// Logging on the audio thread causes priority inversion and glitches.
#define LOG_TAG "GuitarEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

AudioEngine::AudioEngine() {
    // Create the default 21-effect signal chain.
    // Effects are created here but configured with the sample rate in start()
    // once we discover the device's native rate.
    signalChain_.createDefaultChain();
}

AudioEngine::~AudioEngine() {
    // Safe to call here: stop() does not invoke shared_from_this().
    // Oboe does not fire error callbacks synchronously during explicit close.
    stop();
}

bool AudioEngine::start() {
    if (running_.load(std::memory_order_acquire)) {
        LOGI("Engine already running");
        return true;
    }

    // Allow automatic restart on stream errors (cleared by stop())
    allowRestart_.store(true, std::memory_order_release);

    // Open the output stream first to discover the device's native sample rate.
    // We then use that same rate for the input stream to avoid resampling.
    if (!openOutputStream()) {
        LOGE("Failed to open output stream");
        return false;
    }

    if (!openInputStream()) {
        LOGE("Failed to open input stream");
        closeStreams();
        return false;
    }

    // Configure all effects with the discovered sample rate.
    // This triggers internal buffer pre-allocation in effects that need it
    // (overdrive oversampler, delay circular buffer, etc.).
    signalChain_.setSampleRate(sampleRate_);
    signalChain_.resetAll();

    // Initialize the spectrum analyzer with the device's sample rate.
    // This pre-allocates FFT buffers and computes the log-frequency bin mapping.
    spectrumAnalyzer_.setSampleRate(sampleRate_);

    // Looper engine initialization is deferred to LooperDelegate.ensureInitialized()
    // via the looperInit JNI call. This avoids a data race: if we called init()
    // here AND the delegate called it again later, the audio thread could be
    // accessing buffers that are being resized on the UI thread.
    // The delegate calls looperInit(60) on first user interaction with the looper.

    // Initialize drum engine with device sample rate and reset state.
    drumEngine_.setSampleRate(sampleRate_);
    drumEngine_.reset();

    // Pre-allocate stereo drum output buffers with headroom.
    // Sized to 2x framesPerBurst * bufferMultiplier for safety.
    {
        const int32_t maxDrumFrames = framesPerBuffer_ *
            bufferMultiplier_.load(std::memory_order_acquire) * 2;
        drumBufferL_.resize(maxDrumFrames, 0.0f);
        drumBufferR_.resize(maxDrumFrames, 0.0f);
    }

    // Allocate the intermediate input buffer with generous headroom.
    // Oboe may deliver numFrames larger than framesPerBurst, so allocate
    // 4x the burst size to handle any reasonable callback size.
    // This happens on the calling thread, not the audio thread.
    const int32_t maxFrames = framesPerBuffer_ * 4;
    inputBuffer_.resize(maxFrames * std::max(inputChannelCount_, outputChannelCount_), 0.0f);

    // Pre-allocate dry capture buffer for looper (same size as max frames per callback).
    // This captures the post-inputGain, pre-effects DI signal for re-amp support.
    dryCapture_.resize(maxFrames, 0.0f);

    // Start the input stream first, then the output stream.
    // The output stream's callback will read from the input stream.
    oboe::Result inputResult = inputStream_->requestStart();
    if (inputResult != oboe::Result::OK) {
        LOGE("Failed to start input stream: %s", oboe::convertToText(inputResult));
        closeStreams();
        return false;
    }

    oboe::Result outputResult = outputStream_->requestStart();
    if (outputResult != oboe::Result::OK) {
        LOGE("Failed to start output stream: %s", oboe::convertToText(outputResult));
        closeStreams();
        return false;
    }

    running_.store(true, std::memory_order_release);

    // Reset restart counter on successful start
    restartAttempts_.store(0, std::memory_order_release);

    // First-callback init (FTZ + ADPF) is deferred to onAudioReady so that
    // ARM FTZ is set on the correct thread and gettid() captures the Oboe
    // callback thread ID (not the JNI calling thread).
    adpfInitialized_.store(false, std::memory_order_release);

    LOGI("Audio engine started: sampleRate=%d, framesPerBuffer=%d, inCh=%d, outCh=%d",
         sampleRate_, framesPerBuffer_, inputChannelCount_, outputChannelCount_);
    return true;
}

void AudioEngine::stop() {
    // Prevent automatic restart from onErrorAfterClose() after intentional stop
    allowRestart_.store(false, std::memory_order_release);

    bool wasRunning = running_.exchange(false, std::memory_order_acq_rel);

    // Always clean up streams -- an error callback may have closed one stream
    // and set running_=false, leaving the other stream still open.
    releasePerformanceHints();
    closeStreams();

    // Release looper buffers (frees ~66MB of pre-allocated loop memory)
    looperEngine_.release();

    inputLevel_.store(0.0f, std::memory_order_release);
    outputLevel_.store(0.0f, std::memory_order_release);

    // Reset DC blocker state so stale values don't bleed into the next session.
    // Without this, the first few samples after restart could produce a click
    // from the step discontinuity in the filter state.
    dcBlockPrevIn_ = 0.0f;
    dcBlockPrevOut_ = 0.0f;

    if (wasRunning) {
        LOGI("Audio engine stopped");
    }
}

void AudioEngine::setInputSource(int type) {
    inputSourceType_.store(type, std::memory_order_release);
}

void AudioEngine::setInputDeviceId(int32_t deviceId) {
    inputDeviceId_.store(deviceId, std::memory_order_release);
    LOGI("Input device ID set to %d", deviceId);
}

void AudioEngine::setOutputDeviceId(int32_t deviceId) {
    outputDeviceId_.store(deviceId, std::memory_order_release);
    LOGI("Output device ID set to %d", deviceId);
}

void AudioEngine::setBufferMultiplier(int multiplier) {
    int clamped = std::clamp(multiplier, 1, 8);
    bufferMultiplier_.store(clamped, std::memory_order_release);

    // Apply immediately to the open output stream if running
    if (outputStream_ && running_.load(std::memory_order_acquire)) {
        outputStream_->setBufferSizeInFrames(framesPerBuffer_ * clamped);
        // Update the latency tuner's floor to match the user's choice.
        // This prevents the tuner from reducing buffer below the user's
        // chosen multiplier when it attempts periodic latency reduction.
        if (latencyTuner_) {
            latencyTuner_->requestReset();
        }
        LOGI("Buffer multiplier set to %d (bufferSize=%d frames)",
             clamped, framesPerBuffer_ * clamped);
    }
}

bool AudioEngine::openInputStream() {
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Input)
           ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
           ->setSharingMode(oboe::SharingMode::Exclusive)
           ->setFormat(oboe::AudioFormat::Float)
           ->setChannelCount(oboe::ChannelCount::Mono) // Guitar is mono; device may give stereo
           ->setSampleRate(sampleRate_)  // Match output stream's native rate
           ->setInputPreset(oboe::InputPreset::Unprocessed); // Raw signal, no AGC/NS

    // Route to a specific device (e.g., USB audio interface) if set.
    // When 0, Oboe uses the system default device.
    int32_t devId = inputDeviceId_.load(std::memory_order_acquire);
    if (devId > 0) {
        builder.setDeviceId(devId);
        LOGI("Input stream targeting device ID %d", devId);
    }

    oboe::Result result = builder.openStream(inputStream_);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open input stream: %s", oboe::convertToText(result));
        return false;
    }

    // Store actual input channel count (may differ from our mono request)
    inputChannelCount_ = inputStream_->getChannelCount();

    LOGI("Input stream opened: sampleRate=%d, bufferSize=%d, channels=%d, format=%d",
         inputStream_->getSampleRate(),
         inputStream_->getBufferSizeInFrames(),
         inputChannelCount_,
         static_cast<int>(inputStream_->getFormat()));
    return true;
}

bool AudioEngine::openOutputStream() {
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
           ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
           ->setSharingMode(oboe::SharingMode::Exclusive)
           ->setFormat(oboe::AudioFormat::Float)
           ->setChannelCount(oboe::ChannelCount::Mono) // Request mono; device may give stereo
           ->setCallback(this) // This class receives the audio callback
           ->setUsage(oboe::Usage::Game); // Low-latency audio path

    // Route to a specific device (e.g., USB audio interface) if set.
    // This is critical for USB interfaces like iRig: without an explicit
    // device ID, Android may route output to the phone speaker while input
    // correctly goes to USB, causing the user to hear dry guitar only.
    int32_t devId = outputDeviceId_.load(std::memory_order_acquire);
    if (devId > 0) {
        builder.setDeviceId(devId);
        LOGI("Output stream targeting device ID %d", devId);
    }

    // Do NOT set a sample rate -- let Oboe query the device's native rate.
    // This avoids resampling which adds latency and artifacts.

    oboe::Result result = builder.openStream(outputStream_);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open output stream: %s", oboe::convertToText(result));
        return false;
    }

    // Store the device's native properties for use by the input stream.
    // The output device may report stereo even though we requested mono.
    // We track this separately so the signal chain always processes mono.
    sampleRate_ = outputStream_->getSampleRate();
    framesPerBuffer_ = outputStream_->getFramesPerBurst();

    // Compute sample-rate-adaptive DC blocker coefficient: R = exp(-2*pi*fc/fs)
    // fc=10Hz gives -3dB at 10Hz regardless of sample rate (was hardcoded 0.995
    // which gave -3dB at 38Hz@48kHz, eating bass on drop tunings).
    dcBlockR_ = std::exp(-2.0 * 3.14159265358979323846 * 10.0 / sampleRate_);
    dcBlockPrevIn_ = 0.0;
    dcBlockPrevOut_ = 0.0;
    outputChannelCount_ = outputStream_->getChannelCount();

    // Start with a generous buffer to prevent crackling during startup.
    // The CPU frequency governor (ADPF) needs ~500ms to ramp up, and the
    // LatencyTuner needs time to converge. Starting with the user's chosen
    // multiplier (e.g., 2x = 8ms) often causes underruns during ramp-up,
    // each of which is an audible crackle. By starting at max(user, 4) the
    // LatencyTuner begins from a stable point and optimizes DOWN over time.
    // This eliminates startup crackling: the only trade-off is ~8ms extra
    // latency for the first few seconds, which the LatencyTuner removes.
    int mult = bufferMultiplier_.load(std::memory_order_acquire);
    int startupMult = std::max(mult, 4);
    outputStream_->setBufferSizeInFrames(framesPerBuffer_ * startupMult);

    // Create the adaptive buffer tuner. It monitors xrun count and
    // increases buffer size when underruns are detected. The user's
    // chosen buffer multiplier sets the floor (minimum buffer size).
    latencyTuner_ = std::make_unique<oboe::LatencyTuner>(*outputStream_);

    LOGI("Output stream opened: sampleRate=%d, framesPerBurst=%d, channels=%d",
         sampleRate_, framesPerBuffer_, outputChannelCount_);
    return true;
}

void AudioEngine::closeStreams() {
    if (inputStream_) {
        inputStream_->stop();
        inputStream_->close();
        inputStream_.reset();
    }
    // Release the latency tuner before closing streams
    latencyTuner_.reset();
    if (outputStream_) {
        outputStream_->stop();
        outputStream_->close();
        outputStream_.reset();
    }
}

// =============================================================================
// AUDIO CALLBACK -- REAL-TIME SAFE ZONE
// No allocations, no locks, no logging, no system calls beyond Oboe reads.
// =============================================================================

oboe::DataCallbackResult AudioEngine::onAudioReady(
        oboe::AudioStream* /* outputStream */,
        void* audioData,
        int32_t numFrames) {

    // First-callback initialization on the Oboe audio thread:
    //  1. ARM FTZ: Flush denormals to zero to prevent 10-100x CPU stalls
    //     in feedback paths (reverb, delay, filter decay). -ffast-math
    //     hints to the compiler but doesn't set the hardware FPCR bit.
    //  2. ADPF: Create performance hint session with correct thread ID.
    if (!adpfInitialized_.load(std::memory_order_acquire)) {
#if defined(__aarch64__)
        uint64_t fpcr;
        asm volatile("mrs %0, fpcr" : "=r"(fpcr));
        fpcr |= (1 << 24); // FZ: flush denormals to zero
        asm volatile("msr fpcr, %0" : : "r"(fpcr));
#elif defined(__arm__)
        uint32_t fpscr;
        asm volatile("vmrs %0, fpscr" : "=r"(fpscr));
        fpscr |= (1 << 24); // FZ: flush denormals to zero
        asm volatile("vmsr fpscr, %0" : : "r"(fpscr));
#endif
#if __ANDROID_API__ >= 31
        initPerformanceHints();
#endif
        adpfInitialized_.store(true, std::memory_order_release);
    }

    // ADPF: Record the start time of this callback for performance reporting
    auto callbackStart = std::chrono::steady_clock::now();

    auto* outputBuffer = static_cast<float*>(audioData);

    // Snapshot master parameters (one atomic read each, cached for this callback)
    const float inputGain = params_.inputGain.get();
    const float outputGain = params_.outputGain.get();
    const bool bypassed = params_.bypassed.get();

    // ========================================================================
    // INPUT: Read from input stream and extract mono
    // ========================================================================
    // The input device may provide stereo (e.g., iRig reports 2 channels).
    // We always extract mono (first channel) for the signal chain.
    // All processing below operates on numFrames MONO samples in
    // outputBuffer[0..numFrames-1]. At the very end, we expand mono back
    // to the output channel count (stereo duplication if needed).

    const int32_t safeFrames = std::min(numFrames,
        static_cast<int32_t>(inputBuffer_.size()) / std::max(inputChannelCount_, 1));

    bool hasInput = false;
    int32_t monoFramesRead = 0;

    if (inputStream_) {
        auto readResult = inputStream_->read(
            inputBuffer_.data(), safeFrames, 0 /* timeout: non-blocking */);

        if (readResult && readResult.value() > 0) {
            hasInput = true;
            monoFramesRead = readResult.value();

            // Extract mono from input: take first channel only.
            // Guitar input is inherently mono; if the device provides stereo,
            // the second channel is either a duplicate or silence.
            if (inputChannelCount_ > 1) {
                for (int32_t i = 0; i < monoFramesRead; ++i) {
                    outputBuffer[i] = inputBuffer_[i * inputChannelCount_];
                }
            } else {
                std::memcpy(outputBuffer, inputBuffer_.data(),
                            monoFramesRead * sizeof(float));
            }
        }
    }

    if (!hasInput) {
        monoFramesRead = 0;
    }

    // Zero any remaining mono frames
    if (monoFramesRead < numFrames) {
        std::memset(outputBuffer + monoFramesRead, 0,
                    (numFrames - monoFramesRead) * sizeof(float));
    }

    // Input level meter on mono data
    if (monoFramesRead > 0) {
        inputLevel_.store(
            calculateRmsLevel(outputBuffer, monoFramesRead),
            std::memory_order_release);
    } else {
        inputLevel_.store(0.0f, std::memory_order_release);
    }

    // ========================================================================
    // PROCESSING: All operations on mono (numFrames samples)
    // ========================================================================

    // ---- Re-amp: replace live input with dry recording playback ----
    // When re-amping, the dry signal from a saved session replaces the
    // live microphone input, letting the user process old recordings
    // through the current effects chain ("Try Different Tones").
    if (looperEngine_.isReamping()) {
        int32_t filled = looperEngine_.getReampSamples(outputBuffer, numFrames);
        if (filled <= 0) {
            // Re-amp playback finished: zero the buffer to prevent stale data
            std::memset(outputBuffer, 0, numFrames * sizeof(float));
        }
    }

    // ---- Master input gain ----
    if (inputGain != 1.0f) {
        for (int32_t i = 0; i < numFrames; ++i) {
            outputBuffer[i] *= inputGain;
        }
    }

    // Capture dry signal for looper (post-inputGain, pre-effects DI for re-amping).
    // Always capture: the looper consumes pending commands inside processAudioBlock(),
    // so we must have valid dry data ready even on the callback where RECORD is first
    // commanded (state is still IDLE at this point). Cost is one memcpy per callback.
    {
        const int32_t capFrames = std::min(numFrames,
            static_cast<int32_t>(dryCapture_.size()));
        std::memcpy(dryCapture_.data(), outputBuffer, capFrames * sizeof(float));
    }

    // Feed the tuner with clean pre-effects input for reliable pitch detection.
    // The tuner uses MPM (McLeod Pitch Method), which requires a periodic
    // signal. After delay, reverb, and chorus, the signal is aperiodic and
    // pitch detection fails. Feeding the tuner the raw DI signal (after input
    // gain but before effects) gives it the cleanest possible input.
    // Only when the user has the tuner UI open (tunerActive_ = true).
    // This eliminates the tuner's 8192-point FFT from every audio callback
    // when not needed, significantly reducing per-callback CPU cost.
    if (tunerActive_.load(std::memory_order_acquire)) {
        signalChain_.feedTuner(outputBuffer, numFrames);
    }

    // ---- Effects processing ----
    if (!bypassed) {
        signalChain_.process(outputBuffer, numFrames);
    }

    // ---- Master output gain ----
    if (outputGain != 1.0f) {
        for (int32_t i = 0; i < numFrames; ++i) {
            outputBuffer[i] *= outputGain;
        }
    }

    // ---- DC blocker ----
    // y[n] = x[n] - x[n-1] + R * y[n-1]
    // R computed from sample rate: R = exp(-2*pi*fc/fs), fc=10Hz
    // At 48kHz: R=0.99869 (-3dB at 10Hz). At 96kHz: R=0.99935.
    {
        for (int32_t i = 0; i < numFrames; ++i) {
            double inp = static_cast<double>(outputBuffer[i]);
            double out = inp - dcBlockPrevIn_ + dcBlockR_ * dcBlockPrevOut_;
            dcBlockPrevIn_ = inp;
            dcBlockPrevOut_ = out;
            outputBuffer[i] = static_cast<float>(out);
        }
    }

    // ---- Infinity/NaN safety net + Output soft limiter ----
    // Check for non-finite BEFORE soft limiter (Inf/Inf=NaN would cause click)
    for (int32_t i = 0; i < numFrames; ++i) {
        if (!std::isfinite(outputBuffer[i])) {
            outputBuffer[i] = 0.0f;
        }
        outputBuffer[i] = fast_math::softLimit(outputBuffer[i]);
    }

    // ---- Looper: process transport, mix loop playback into output ----
    // The looper reads the soft-limited output as "wet" capture and sums
    // loop playback into the output buffer. Called unconditionally so that
    // pending commands (e.g., RECORD from IDLE) are consumed on the audio
    // thread; processAudioBlock handles IDLE as a cheap no-op pass-through.
    {
        const auto prevState = looperEngine_.getState();
        looperEngine_.processAudioBlock(dryCapture_.data(), outputBuffer,
                                         outputBuffer, numFrames);
        // Soft limit after loop playback summing to prevent clipping.
        // Skip the extra pass if looper was and remains IDLE (common case).
        if (prevState != LooperEngine::LooperState::IDLE
            || looperEngine_.getState() != LooperEngine::LooperState::IDLE) {
            for (int32_t i = 0; i < numFrames; ++i) {
                outputBuffer[i] = fast_math::softLimit(outputBuffer[i]);
            }
        }
    }

    // ---- Drum Machine: render stereo drum mix ----
    // Render drums into pre-allocated stereo buffers. This happens AFTER the
    // looper so that loop recordings contain only guitar audio (no drums).
    // The drum mix is summed in during the stereo expansion step below.
    //
    // Always call renderBlock() so that pending pad triggers (stored in an
    // atomic slot) are consumed and rendered. The previous check
    // (isPlaying || hasActiveVoices) missed the case where a pad trigger
    // is pending but no voices are yet active -- the trigger would never
    // be consumed and the pad tap would produce no sound. renderBlock()
    // has its own efficient early-exit after consuming the trigger.
    {
        const int32_t drumFrames = std::min(numFrames,
            static_cast<int32_t>(drumBufferL_.size()));
        drumEngine_.renderBlock(drumBufferL_.data(), drumBufferR_.data(), drumFrames);
    }
    // Check if drums actually produced output for the stereo mix step below
    const bool drumsActive = drumEngine_.isPlaying() || drumEngine_.hasActiveVoices();

    // Output level meter on mono data
    outputLevel_.store(
        calculateRmsLevel(outputBuffer, numFrames),
        std::memory_order_release);

    // Spectrum analyzer: always fed mono data now (no extraction needed)
    spectrumAnalyzer_.pushSamples(outputBuffer, numFrames);

    // ========================================================================
    // OUTPUT: Stereo expansion + drum mix
    // ========================================================================
    // If the output device is stereo, expand the mono guitar signal and sum
    // in the stereo drum mix. We iterate backwards so that in-place expansion
    // doesn't overwrite unread mono samples (frame i is at position i, and
    // its stereo destination starts at i*2, which is always >= i).
    // Soft-limit the final mix to prevent clipping from guitar + drums.
    const float drumMix = drumMixLevel_.load(std::memory_order_relaxed);
    if (outputChannelCount_ > 1) {
        for (int32_t i = numFrames - 1; i >= 0; --i) {
            const float monoGuitar = outputBuffer[i];
            float left = monoGuitar;
            float right = monoGuitar;
            if (drumsActive) {
                left += drumBufferL_[i] * drumMix;
                right += drumBufferR_[i] * drumMix;
            }
            outputBuffer[i * outputChannelCount_] = fast_math::softLimit(left);
            outputBuffer[i * outputChannelCount_ + 1] = fast_math::softLimit(right);
        }
    } else {
        if (drumsActive) {
            for (int32_t i = 0; i < numFrames; ++i) {
                outputBuffer[i] += (drumBufferL_[i] + drumBufferR_[i]) * 0.5f * drumMix;
                outputBuffer[i] = fast_math::softLimit(outputBuffer[i]);
            }
        }
    }

    // Adaptive buffering: detect underruns and increase buffer size.
    // Must be called every callback for reliable xrun detection.
    if (latencyTuner_) {
        latencyTuner_->tune();
    }

    // ADPF: Report actual work duration to the CPU governor.
#if __ANDROID_API__ >= 31
    if (perfHintSession_) {
        auto callbackEnd = std::chrono::steady_clock::now();
        int64_t actualNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            callbackEnd - callbackStart).count();
        // Report 10% more than actual to maintain CPU frequency headroom.
        // Without this, the governor may reduce frequency between callbacks,
        // causing the next callback to miss its deadline when CPU needs to
        // ramp back up (~200ms ramp time, during which dozens of callbacks
        // can glitch). This is a documented ADPF best practice.
        int64_t reportedNs = static_cast<int64_t>(actualNs * 1.1);
        APerformanceHintSession_reportActualWorkDuration(perfHintSession_, reportedNs);
    }
#endif

    return oboe::DataCallbackResult::Continue;
}

// =============================================================================
// END REAL-TIME SAFE ZONE
// =============================================================================

float AudioEngine::calculateRmsLevel(const float* buffer, int numSamples) {
    if (numSamples <= 0) return 0.0f;

    float sumSquares = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        sumSquares += buffer[i] * buffer[i];
    }

    // Clamp to [0, 1] range. The sqrt gives us a value proportional
    // to perceived loudness rather than raw energy.
    float rms = std::sqrt(sumSquares / static_cast<float>(numSamples));
    return std::min(rms, 1.0f);
}

bool AudioEngine::loadCabinetIR(const float* irData, int irLength, int irSampleRate) {
    // CabinetSim is always at index 6 in the default signal chain
    // (set by SignalChain::createDefaultChain).
    constexpr int kCabinetSimIndex = 6;
    Effect* effect = signalChain_.getEffect(kCabinetSimIndex);
    if (!effect) return false;

    auto* cabSim = static_cast<CabinetSim*>(effect);
    return cabSim->loadUserIR(irData, irLength, irSampleRate);
}

void AudioEngine::setTunerActive(bool active) {
    if (active) {
        // Prepare tuner state BEFORE setting the flag. This is safe because
        // the audio thread only calls feedTuner() when tunerActive_ is true,
        // and we haven't set it yet.
        signalChain_.prepareTunerForActivation();

        // Atomically set the flag and detect false→true transition.
        bool wasActive = tunerActive_.exchange(true, std::memory_order_acq_rel);
        if (!wasActive && outputStream_ && running_.load(std::memory_order_acquire)) {
            // The tuner's 8192-point FFT adds a CPU spike every ~21 callbacks.
            // The LatencyTuner may have optimized the buffer down during idle.
            // Proactively boost the buffer to absorb the new workload.
            //
            // IMPORTANT: Use max(current, target) so we never REDUCE the buffer
            // below LatencyTuner's current converged size. And do NOT call
            // requestReset() — it drops buffer to the internal minimum (2 bursts)
            // and enters an 8-callback idle period that ignores underruns,
            // which is strictly worse than the original problem.
            int32_t currentBuffer = outputStream_->getBufferSizeInFrames();
            int mult = bufferMultiplier_.load(std::memory_order_acquire);
            int32_t boostTarget = framesPerBuffer_ * std::max(mult, 4);
            if (boostTarget > currentBuffer) {
                outputStream_->setBufferSizeInFrames(boostTarget);
            }
            // No requestReset() — LatencyTuner continues in Active state and
            // can respond immediately if any underruns still occur.
        }
    } else {
        tunerActive_.store(false, std::memory_order_release);
    }
}

void AudioEngine::onErrorBeforeClose(
        oboe::AudioStream* /* stream */, oboe::Result error) {
    LOGE("Audio stream error before close: %s", oboe::convertToText(error));
}

void AudioEngine::onErrorAfterClose(
        oboe::AudioStream* stream, oboe::Result error) {
    LOGE("Audio stream error after close: %s", oboe::convertToText(error));

    // Attempt to restart the engine if it was running when the error occurred.
    // Common cause: USB audio device disconnected or Bluetooth route changed.
    // IMPORTANT: Restart on a separate thread to avoid deadlock -- this callback
    // is invoked from an internal Oboe thread, and start() opens new streams.
    //
    // Exponential backoff: 500ms, 1000ms, 2000ms. After 3 failures, give up.
    // This prevents busy-looping when hardware is genuinely unavailable.
    //
    // Lifetime safety: We capture shared_from_this() to prevent use-after-free
    // if stopEngine() destroys the AudioEngine while the restart thread is pending.
    // The allowRestart_ flag prevents restart after an intentional stop().
    // Use exchange to guarantee exactly one thread wins the restart race.
    if (running_.exchange(false, std::memory_order_acq_rel) &&
        allowRestart_.load(std::memory_order_acquire)) {

        const int attempt = restartAttempts_.fetch_add(1, std::memory_order_acq_rel);
        if (attempt >= kMaxRestartAttempts) {
            LOGE("Automatic restart abandoned after %d failed attempts. "
                 "User must restart manually.", kMaxRestartAttempts);
            restartAttempts_.store(0, std::memory_order_release);
            return;
        }

        // Exponential backoff: 500ms * 2^attempt (500, 1000, 2000)
        const int delayMs = 500 * (1 << attempt);
        LOGI("Scheduling automatic restart (attempt %d/%d, backoff %dms)...",
             attempt + 1, kMaxRestartAttempts, delayMs);

        auto self = shared_from_this();
        std::thread([self, delayMs]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            if (self->allowRestart_.load(std::memory_order_acquire)) {
                if (!self->start()) {
                    LOGE("Automatic restart failed (will retry with backoff)");
                    // onErrorAfterClose will be called again if the stream
                    // fails, continuing the backoff sequence.
                }
            }
        }).detach();
    }
}

// =============================================================================
// ADPF Performance Hints (Android 12+ / API 31+)
// =============================================================================

void AudioEngine::initPerformanceHints() {
#if __ANDROID_API__ >= 31
    perfHintManager_ = APerformanceHintManager_getInstance();
    if (!perfHintManager_) {
        LOGI("ADPF: Performance hint manager not available");
        return;
    }

    // Get the current thread ID for the audio callback thread.
    // Since the audio callback runs on Oboe's internal thread (not this thread),
    // we'll set up the session with the current thread for now and the hints
    // will still help the system understand our workload pattern.
    pid_t tid = gettid();
    int32_t tids[] = { tid };

    // Target duration: the audio callback must complete within one buffer period.
    // bufferPeriodNs = (framesPerBuffer / sampleRate) * 1e9
    int64_t targetNs = static_cast<int64_t>(
        static_cast<double>(framesPerBuffer_) / static_cast<double>(sampleRate_) * 1e9);

    perfHintSession_ = APerformanceHintManager_createSession(
        perfHintManager_, tids, 1, targetNs);

    if (perfHintSession_) {
        LOGI("ADPF: Performance hint session created (target=%lld ns)", (long long)targetNs);
    } else {
        LOGI("ADPF: Failed to create performance hint session");
    }
#endif
}

void AudioEngine::releasePerformanceHints() {
#if __ANDROID_API__ >= 31
    if (perfHintSession_) {
        APerformanceHintSession_close(perfHintSession_);
        perfHintSession_ = nullptr;
        LOGI("ADPF: Performance hint session closed");
    }
    perfHintManager_ = nullptr;
#endif
}
