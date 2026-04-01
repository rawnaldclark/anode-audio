#ifndef GUITAR_ENGINE_AMP_MODEL_H
#define GUITAR_ENGINE_AMP_MODEL_H

#include "effect.h"
#include "oversampler.h"
#include "NAM/get_dsp.h"
#include <android/log.h>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// =============================================================================
// Neural Amp Modeling (NeuralAmpModelerCore) Integration:
//   This file supports two processing modes:
//
//   1. CLASSIC MODE (default):
//      Multi-stage tube waveshaping -- a pure-DSP approximation of tube amp
//      behavior using cascaded triode stages, coupling cap filters, and a
//      three-band tone stack. This is the built-in fallback that requires no
//      external model files. Nine voicings model a range of iconic amplifiers.
//
//   2. NEURAL MODE (activated by loading a .nam model file):
//      Uses NeuralAmpModelerCore (nam::DSP) to run neural network inference
//      trained on real amplifier captures. NAM models encode the full
//      nonlinear, stateful behavior of the captured amplifier -- something
//      static waveshaping cannot fully replicate. The neural model replaces
//      all analog processing stages (triode, coupling cap, tone stack) with
//      a single learned transfer function.
//
//   Mode switching:
//     - loadNeuralModel(path) loads a .nam file and activates neural mode
//     - clearNeuralModel() deactivates neural mode and reverts to classic
//     - isNeuralModelLoaded() queries the current mode
//
//   Thread safety:
//     Model loading happens on a non-audio thread (JNI call from Kotlin).
//     process() runs on the audio thread. A std::mutex synchronizes access
//     to the neuralModel_ pointer during swap and processing. The contention
//     window is small: a few ms for process(), nanoseconds for the swap.
//
//   NeuralAmpModelerCore GitHub: https://github.com/sdatkinson/NeuralAmpModelerCore
//   License: MIT (compatible with commercial use)
// =============================================================================

/**
 * Dual-mode amp model effect: classic waveshaping AND neural amp modeling.
 *
 * This effect supports two processing paths:
 *
 * === CLASSIC MODE (built-in fallback) ===
 * Models the nonlinear behavior of a tube guitar amplifier's preamp section.
 * Real tube amps achieve their characteristic sound through multiple cascaded
 * gain stages, each with its own saturation curve, coupling capacitors
 * (DC blocking), and frequency-dependent behavior.
 *
 * Signal flow (modeling a typical tube preamp):
 *   Stage 1: Input gain + coupling cap high-pass filter (~30Hz)
 *            Removes DC offset and subsonic content that would cause
 *            asymmetric saturation artifacts in downstream stages.
 *
 *   Stage 2: Triode waveshaping (asymmetric soft clipping)
 *            Models the 12AX7 triode transfer curve. Positive half-cycles
 *            are clipped harder (grid clipping) while negative half-cycles
 *            have a softer curve (plate saturation). This asymmetry
 *            generates even harmonics (2nd, 4th) which give tubes their
 *            characteristic warmth. A bias offset shifts the operating
 *            point deeper into the nonlinear region at higher gain settings.
 *
 *   Stage 3: Tone stack (simplified Baxandall-style)
 *            Three SVF (State Variable Filter) bands providing bass/mid/treble
 *            shaping. Different preset curves for each voicing approximate
 *            the frequency-dependent behavior of real amp tone stacks.
 *
 *   Stage 4: Output level control + optional Variac sag attenuation
 *            Final volume adjustment with soft limiting to prevent harsh clipping.
 *            When Variac > 0, output is further attenuated to model reduced
 *            power tube headroom (voltage starve technique).
 *
 * Nine built-in amp model voicings:
 *   Clean (0):          Fender-style clean with slight compression.
 *                       Low gain, minimal saturation, bright tone stack.
 *   Crunch (1):         Marshall-style breakup at moderate volume.
 *                       Medium gain, moderate saturation, mid-forward tone.
 *   High-Gain (2):      Mesa/Rectifier-style high-gain distortion.
 *                       Heavy saturation, scooped mids, two cascaded stages.
 *   Plexi (3):          Marshall 1959 Plexi with channel-jumping character.
 *                       Rich harmonics, cascaded EL34 stages, bark at 800Hz-1.5kHz.
 *   Hiwatt (4):         Hiwatt DR103 -- tight, articulate, massive headroom.
 *                       High NFB, symmetric clipping, flat/hi-fi response.
 *   Silver Jubilee (5): Marshall 2555 -- smooth, mid-forward, diode clipping.
 *                       Two cascaded stages, aggressive but singing sustain.
 *   Vox AC30 (6):       Vox AC30 Top Boost -- chimey, bright, jangly.
 *                       EL84 breakup, treble-forward, early compression.
 *   Fender Twin (7):    Fender Twin Reverb -- maximum clean headroom.
 *                       6L6GC, scooped mids, deep bass, jazz/country clean.
 *   Silvertone (8):     Silvertone 1485 -- trashy, compressed, lo-fi grit.
 *                       Early saturation, loose bass, rolled-off treble.
 *
 * === NEURAL MODE (activated by loading a .nam file) ===
 * Uses NeuralAmpModelerCore (nam::DSP) for neural network inference on
 * real amplifier captures. NAM models encode the full nonlinear, stateful
 * behavior of the captured amplifier. When a neural model is loaded, it
 * replaces all classic analog processing stages with a single learned
 * transfer function. Input gain and output level parameters still apply.
 *
 * NAM processes buffers of NAM_SAMPLE (double) values. Since our audio
 * pipeline uses float, we pre-allocate double-precision conversion buffers
 * in setSampleRate() to avoid audio-thread allocations.
 *
 * Parameter IDs for JNI access:
 *   0 = inputGain   (0.0 to 2.0) - drive into the amp model
 *   1 = outputLevel  (0.0 to 1.0) - output volume
 *   2 = modelType   (0-8, integer) - selects one of 9 amp voicings
 *       (modelType is used in classic mode only; ignored in neural mode)
 *   3 = variac      (0.0 to 1.0) - voltage sag amount
 *       Models running the amp at reduced mains voltage (Eddie Van Halen's
 *       technique of running a Marshall Plexi at ~89V instead of 120V).
 *       At 0.0: normal operation. At 1.0: maximum sag/compression.
 *       (variac is used in classic mode only; ignored in neural mode)
 */
class AmpModel : public Effect {
public:
    AmpModel() = default;

    // =========================================================================
    // Effect interface
    // =========================================================================

    /**
     * Process audio through the amp model. Real-time safe.
     *
     * Routes audio through one of two processing paths:
     *
     * NEURAL MODE (useNeuralMode_ == true && neuralModel_ != null):
     *   Applies inputGain, converts float->double, runs NAM inference,
     *   converts double->float, applies outputLevel and soft limiting.
     *   The mutex is held for the duration of neural processing to prevent
     *   the model from being swapped mid-buffer.
     *
     * CLASSIC MODE (default fallback):
     *   Each sample passes through four cascaded stages that model the
     *   behavior of a tube amplifier preamp. The processing is done in
     *   64-bit double precision to maintain numerical stability in the
     *   filter calculations at low frequencies.
     *
     *   The triode waveshaping stages use 2x oversampling to suppress
     *   aliasing. The nonlinear waveshaping (tanh, rational functions)
     *   generates harmonics that can fold back below Nyquist without
     *   oversampling. 2x is sufficient here because the amp model's
     *   softer saturation curves generate fewer high-order harmonics
     *   than the Overdrive's harder clipping (which uses 8x).
     *
     *   When Variac > 0, the effective preGain is reduced, biasShift and
     *   saturationAmount are increased, and the output is attenuated --
     *   modeling the effect of reduced mains voltage on tube operation.
     *
     * @param buffer   Mono audio samples in [-1.0, 1.0].
     * @param numFrames Number of frames in the buffer.
     */
    void process(float* buffer, int numFrames) override {
        // Guard: setSampleRate() must be called before first process().
        // Without this, preBuffer_ and oversampler_ are uninitialized.
        if (preBuffer_.empty()) return;

        // Clamp to pre-allocated buffer capacity to prevent overrun.
        // Oboe may deliver buffers larger than expected on some devices.
        numFrames = std::min(numFrames, kMaxBufferFrames);

        // Snapshot parameters from atomics (one read per process block,
        // not per sample, to avoid excessive atomic loads)
        const float inputGain = inputGain_.load(std::memory_order_relaxed);
        const float outputLevel = outputLevel_.load(std::memory_order_relaxed);

        // =====================================================================
        // Neural mode: route through NeuralAmpModelerCore inference
        // =====================================================================
        if (useNeuralMode_.load(std::memory_order_acquire)) {
            // Use try_lock instead of lock_guard to guarantee the audio thread
            // never blocks. If the lock is held (model being swapped), we fall
            // through to classic mode for this buffer. The swap is near-instant
            // (pointer move), so try_lock failure is extremely rare.
            std::unique_lock<std::mutex> lock(neuralModelMutex_, std::try_to_lock);
            if (!lock.owns_lock()) {
                // Mutex held by loadNeuralModel/clearNeuralModel -- skip neural
                // processing this buffer to maintain real-time safety.
                // Fall through to classic mode below.
            } else if (neuralModel_) {
                // Apply inputGain and convert float -> double (NAM_SAMPLE)
                // into pre-allocated buffer. No audio-thread allocation.
                for (int i = 0; i < numFrames; ++i) {
                    namInputBuffer_[i] =
                        static_cast<NAM_SAMPLE>(buffer[i] * inputGain);
                }

                // NAM processes entire buffers, not sample-by-sample.
                // The model's internal state (LSTM hidden states, etc.)
                // is updated across the buffer in sequence.
                //
                // NAM uses a multi-channel API: process(NAM_SAMPLE**, NAM_SAMPLE**, int)
                // where the double pointers index channels. For our mono pipeline,
                // we create single-element pointer arrays pointing to our buffers.
                NAM_SAMPLE* namInPtr = namInputBuffer_.data();
                NAM_SAMPLE* namOutPtr = namOutputBuffer_.data();
                neuralModel_->process(&namInPtr, &namOutPtr, numFrames);

                // Convert double -> float, apply output level and soft limit.
                // The soft limiter prevents harsh clipping from high-output
                // neural models that may exceed [-1, 1].
                for (int i = 0; i < numFrames; ++i) {
                    double sample = namOutputBuffer_[i];
                    sample *= static_cast<double>(outputLevel);
                    sample = softLimit(sample);
                    buffer[i] = clamp(static_cast<float>(sample),
                                      -1.0f, 1.0f);
                }
                return;
            }
            // If neuralModel_ was null, fall through to classic mode.
        }

        // =====================================================================
        // Classic mode: multi-stage tube waveshaping
        // =====================================================================

        const int modelType = static_cast<int>(
            modelType_.load(std::memory_order_relaxed));

        // Snapshot Variac parameter (classic mode only).
        // Variac models running the amp at reduced mains voltage, which:
        //   - Reduces effective gain (tubes have less voltage swing)
        //   - Shifts bias point (tubes run "cooler", closer to cutoff)
        //   - Lowers saturation threshold (clipping onset at lower levels)
        //   - Reduces output power (voltage sag compresses the signal)
        const float variac = variac_.load(std::memory_order_relaxed);

        // Select model-specific parameters based on the chosen preset.
        // These values approximate the tonal characteristics of each amp type.
        const ModelParams& params = getModelParams(modelType);

        // Apply Variac modulation to the model parameters.
        // These adjusted values are used throughout the processing stages.
        // At variac=0, these equal the original ModelParams values exactly.
        // Variac reduces ALL voltages proportionally (plate, screen, bias).
        // At variac=1.0 (~89V mains = 74% of 120V):
        //   - Gain drops ~45% (plate voltage reduction starves gain stages)
        //   - Bias shift stays UNCHANGED (bias voltage drops proportionally too)
        //   - Saturation increases slightly (earlier clipping from reduced headroom)
        const double effectivePreGain =
            params.preGain * (1.0 - static_cast<double>(variac) * 0.45);
        const double effectiveBiasShift = params.biasShift;  // No bias shift from Variac
        const double effectiveSaturation =
            std::min(params.saturationAmount + static_cast<double>(variac) * 0.2,
                     1.0);  // Cap at 1.0 to prevent over-saturation artifacts
        const double effectiveBiasShift2 = params.biasShift2;  // No bias shift from Variac
        const double effectiveSaturation2 =
            std::min(params.saturationAmount2 + static_cast<double>(variac) * 0.2,
                     1.0);

        // Variac output sag factor: power scales as voltage squared.
        // At 74% voltage (variac=1), power is ~55% of normal -> 40% attenuation.
        const double sagAttenuation = 1.0 - static_cast<double>(variac) * 0.40;

        // Pre-compute tone stack gains OUTSIDE the per-sample loop.
        // std::pow(10.0, ...) is expensive (~50-100 cycles per call).
        // Since params are constant for the entire buffer, compute once.
        const double bassGain = std::pow(10.0, params.bassGainDb / 20.0);
        const double midGain = std::pow(10.0, params.midGainDb / 20.0);
        const double trebleGain = std::pow(10.0, params.trebleGainDb / 20.0);

        // ---- Stage 1: Apply input gain into pre-buffer ----
        // Uses Variac-adjusted preGain instead of raw params.preGain.
        for (int i = 0; i < numFrames; ++i) {
            preBuffer_[i] = buffer[i] * inputGain *
                            static_cast<float>(effectivePreGain);
        }

        // ---- Stage 2: Upsample -> Waveshaping -> Downsample ----
        // The triode waveshaping generates harmonics that alias without
        // oversampling. Upsample by 2x, apply nonlinear stages at the
        // elevated sample rate, then downsample back.
        const int oversampledLen = oversampler_.getOversampledSize(numFrames);
        float* upsampled = oversampler_.upsample(preBuffer_.data(), numFrames);

        for (int i = 0; i < oversampledLen; ++i) {
            double sample = static_cast<double>(upsampled[i]);

            // Coupling cap high-pass filter (~30Hz) at oversampled rate.
            // Uses the 2x-rate coefficient to maintain correct 30Hz cutoff.
            sample = processHighPass(sample, couplingCapState_,
                                     couplingCapCoeffOversampled_);

            // First triode stage (uses Variac-adjusted bias and saturation)
            sample = processTriode(sample, effectiveBiasShift,
                                   effectiveSaturation);

            // Optional second gain stage for high-gain/cascaded models
            if (params.useSecondStage) {
                sample = processHighPass(sample, couplingCap2State_,
                                         couplingCapCoeffOversampled_);
                sample = processTriode(sample, effectiveBiasShift2,
                                       effectiveSaturation2);
            }

            upsampled[i] = static_cast<float>(sample);
        }

        // Downsample back to original rate
        oversampler_.downsample(upsampled, numFrames, buffer);

        // ---- Stage 3: Tone stack (at original sample rate) ----
        // The tone stack is linear (EQ), so it doesn't generate harmonics
        // and doesn't need oversampling. Running it at 1x saves CPU.
        // ---- Stage 4: Output level + Variac sag + soft limiter ----
        for (int i = 0; i < numFrames; ++i) {
            double sample = static_cast<double>(buffer[i]);

            sample = processToneStack(sample, bassGain, midGain, trebleGain);

            // Apply output level and Variac sag attenuation.
            // sagAttenuation models the reduced output power from lower
            // mains voltage -- the power tubes cannot swing as hard.
            sample *= static_cast<double>(outputLevel) * sagAttenuation;
            sample = softLimit(sample);

            buffer[i] = clamp(static_cast<float>(sample), -1.0f, 1.0f);
        }
    }

    /**
     * Configure sample rate and recalculate filter coefficients.
     *
     * The coupling cap high-pass and tone stack filters are sample-rate
     * dependent. We recalculate their coefficients here to maintain
     * consistent behavior across different device sample rates
     * (commonly 44100, 48000, or 96000 Hz on Android).
     *
     * Also pre-allocates the NAM float<->double conversion buffers to
     * ensure zero allocations on the audio thread during neural processing.
     *
     * If a neural model is loaded, resets it to the new sample rate and
     * prewarms it to stabilize internal states (LSTM hidden state, etc.).
     *
     * @param sampleRate Device sample rate in Hz.
     */
    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;
        recalculateCoefficients();

        // Pre-allocate oversampler and input buffer for maximum expected
        // block size. Uses kMaxBufferFrames (8192) to match SignalChain's
        // dry buffer size. Oboe may deliver more frames than the burst size,
        // especially with high buffer multiplier settings.
        oversampler_.prepare(kMaxBufferFrames);
        preBuffer_.resize(kMaxBufferFrames, 0.0f);

        // Pre-allocate NAM double-precision conversion buffers.
        // NAM_SAMPLE is double by default; our pipeline uses float.
        // These buffers avoid per-process-call allocation.
        namInputBuffer_.resize(kMaxBufferFrames, 0.0);
        namOutputBuffer_.resize(kMaxBufferFrames, 0.0);

        // If a neural model is loaded, reset it to the new sample rate.
        // The lock ensures we don't race with a concurrent loadNeuralModel().
        {
            std::lock_guard<std::mutex> lock(neuralModelMutex_);
            if (neuralModel_) {
                neuralModel_->Reset(static_cast<double>(sampleRate),
                                    kMaxBufferFrames);
                prewarmNeuralModel();
            }
        }
    }

    /**
     * Reset all internal filter states.
     *
     * Clears the coupling cap high-pass filter states, tone stack SVF
     * states, and oversampler FIR history. If a neural model is loaded,
     * also resets its internal state (LSTM hidden state, etc.) and
     * prewarms it. Called when the audio stream restarts or the effect
     * is re-enabled to prevent clicks and pops from stale state.
     */
    void reset() override {
        // Coupling cap states
        couplingCapState_ = 0.0;
        couplingCap2State_ = 0.0;

        // Tone stack SVF states
        tsBassZ1_ = 0.0;  tsBassZ2_ = 0.0;
        tsMidZ1_ = 0.0;   tsMidZ2_ = 0.0;
        tsTrebZ1_ = 0.0;  tsTrebZ2_ = 0.0;

        // Oversampler FIR history
        oversampler_.reset();

        // Reset neural model internal state (LSTM hidden state, etc.)
        // and prewarm to stabilize the network before live audio.
        {
            std::lock_guard<std::mutex> lock(neuralModelMutex_);
            if (neuralModel_) {
                neuralModel_->Reset(static_cast<double>(sampleRate_),
                                    kMaxBufferFrames);
                prewarmNeuralModel();
            }
        }
    }

    // =========================================================================
    // Neural model management (called from non-audio thread via JNI)
    // =========================================================================

    /**
     * Load a neural amp model from a .nam file and activate neural mode.
     *
     * Creates a new nam::DSP instance from the file, resets it to the
     * current sample rate, and prewarms it to stabilize internal states.
     * The new model is then swapped in under the mutex, so process() on
     * the audio thread will pick it up on the next buffer.
     *
     * Thread safety: This method is called from a non-audio thread (JNI).
     * The mutex ensures that the model swap does not race with process().
     * The swap itself is near-instantaneous (pointer assignment), so
     * audio-thread blocking is minimal.
     *
     * @param path Absolute filesystem path to a .nam model file.
     * @return true if the model was loaded successfully, false on error.
     */
    bool loadNeuralModel(const std::string& path) {
        try {
            // Create the NAM DSP model from the .nam file.
            // get_dsp() parses the model architecture and weights.
            auto newModel = nam::get_dsp(std::filesystem::path(path));
            if (!newModel) {
                return false;
            }

            // Reset to current sample rate and prewarm BEFORE swapping in.
            // This ensures the model is fully initialized before the audio
            // thread sees it, preventing glitches on the first buffer.
            newModel->Reset(
                static_cast<double>(sampleRate_),
                kMaxBufferFrames);
            prewarmModel(newModel.get());

            // Swap the new model in under the lock.
            // The old model (if any) is destroyed after the lock is released.
            {
                std::lock_guard<std::mutex> lock(neuralModelMutex_);
                neuralModel_ = std::move(newModel);
            }

            // Set the flag AFTER the model is fully installed.
            // memory_order_release pairs with the acquire in process().
            useNeuralMode_.store(true, std::memory_order_release);
            return true;

        } catch (const std::exception& e) {
            __android_log_print(ANDROID_LOG_ERROR, "AmpModel",
                "Failed to load neural model: %s", e.what());
            return false;
        } catch (...) {
            __android_log_print(ANDROID_LOG_ERROR, "AmpModel",
                "Failed to load neural model: unknown exception");
            return false;
        }
    }

    /**
     * Clear the loaded neural model and revert to classic waveshaping.
     *
     * Thread safety: The atomic flag is cleared first (so process() will
     * take the classic path), then the model is destroyed under the lock.
     */
    void clearNeuralModel() {
        // Clear the flag first so the audio thread stops entering the
        // neural path. Even if process() is mid-buffer in the neural path,
        // the lock below will wait for it to finish before destroying
        // the model.
        useNeuralMode_.store(false, std::memory_order_release);

        std::lock_guard<std::mutex> lock(neuralModelMutex_);
        neuralModel_.reset();
    }

    /**
     * Query whether a neural model is currently active.
     *
     * @return true if a neural model is loaded and neural mode is active.
     */
    bool isNeuralModelLoaded() const {
        return useNeuralMode_.load(std::memory_order_acquire);
    }

    // =========================================================================
    // Parameter access (called from UI thread via JNI)
    // =========================================================================

    /**
     * Set a parameter by ID. Thread-safe via atomic stores.
     *
     * @param paramId  0=inputGain, 1=outputLevel, 2=modelType, 3=variac
     * @param value    Parameter value (ranges vary by parameter).
     */
    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0:
                inputGain_.store(clamp(value, 0.0f, 2.0f),
                                 std::memory_order_relaxed);
                break;
            case 1:
                outputLevel_.store(clamp(value, 0.0f, 1.0f),
                                   std::memory_order_relaxed);
                break;
            case 2:
                // Quantize to nearest valid model type (0 through 8)
                modelType_.store(clamp(std::round(value), 0.0f, 8.0f),
                                 std::memory_order_relaxed);
                break;
            case 3:
                // Variac voltage sag: 0.0 = normal, 1.0 = maximum sag
                variac_.store(clamp(value, 0.0f, 1.0f),
                              std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    /**
     * Get a parameter by ID. Thread-safe via atomic loads.
     *
     * @param paramId  0=inputGain, 1=outputLevel, 2=modelType, 3=variac
     * @return Current parameter value.
     */
    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return inputGain_.load(std::memory_order_relaxed);
            case 1: return outputLevel_.load(std::memory_order_relaxed);
            case 2: return modelType_.load(std::memory_order_relaxed);
            case 3: return variac_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    // =========================================================================
    // Model parameter presets
    // =========================================================================

    /**
     * Aggregate of all model-specific tuning parameters for classic mode.
     *
     * These values define the character of each built-in amp model voicing.
     * When neural mode is active, these are bypassed entirely -- the neural
     * model's trained weights encode all tonal behavior implicitly.
     *
     * When the Variac parameter is active (> 0), the preGain, biasShift,
     * and saturationAmount values are modulated before use in process().
     */
    struct ModelParams {
        // Pre-gain stage
        double preGain;            // Gain before waveshaping (sets drive amount)

        // First triode stage
        double biasShift;          // DC bias into the tube (shifts operating point)
        double saturationAmount;   // How hard the tube clips (0 = clean, 1 = max)

        // Second triode stage (high-gain/cascaded models only)
        bool   useSecondStage;     // Whether to use a cascaded second stage
        double biasShift2;         // Bias for second stage
        double saturationAmount2;  // Saturation for second stage

        // Tone stack gains (dB, applied as peaking EQ at bass/mid/treble freqs)
        double bassGainDb;         // Bass boost/cut in dB
        double midGainDb;          // Mid boost/cut in dB
        double trebleGainDb;       // Treble boost/cut in dB
    };

    /**
     * Get model parameters for the specified amp voicing.
     *
     * Each voicing is carefully tuned to approximate the tonal character
     * of a real amplifier type. The parameters control the gain staging,
     * waveshaping characteristics, and tone stack EQ that together define
     * the amp's "voice".
     *
     * Note: When neural mode is active (a .nam model is loaded), these
     * voicings are bypassed entirely. The neural model's trained weights
     * encode all tonal behavior implicitly.
     *
     * @param modelType 0=Clean, 1=Crunch, 2=High-Gain, 3=Plexi, 4=Hiwatt,
     *                  5=Silver Jubilee, 6=Vox AC30, 7=Fender Twin, 8=Silvertone
     * @return Reference to the appropriate ModelParams.
     */
    static const ModelParams& getModelParams(int modelType) {
        // =====================================================================
        // Type 0: Clean -- Fender-style clean amp
        // =====================================================================
        // Minimal saturation, slight compression, bright and open sound.
        // The low preGain keeps the signal in the mostly-linear region
        // of the triode curve, producing gentle even-harmonic warmth
        // without obvious distortion.
        static const ModelParams clean = {
            /* preGain */           1.5,
            /* biasShift */         0.05,
            /* saturationAmount */  0.3,
            /* useSecondStage */    false,
            /* biasShift2 */        0.0,
            /* saturationAmount2 */ 0.0,
            /* bassGainDb */       -1.0,
            /* midGainDb */         0.0,
            /* trebleGainDb */      2.0
        };

        // =====================================================================
        // Type 1: Crunch -- Marshall-style breakup
        // =====================================================================
        // Moderate saturation with prominent midrange. The medium preGain
        // pushes the signal into the nonlinear region on peaks, creating
        // the classic "crunch" sound where the amp breaks up on hard
        // picking but stays relatively clean on soft playing.
        static const ModelParams crunch = {
            /* preGain */           4.0,
            /* biasShift */         0.15,
            /* saturationAmount */  0.6,
            /* useSecondStage */    false,
            /* biasShift2 */        0.0,
            /* saturationAmount2 */ 0.0,
            /* bassGainDb */        1.5,
            /* midGainDb */         3.0,
            /* trebleGainDb */      1.0
        };

        // =====================================================================
        // Type 2: High-Gain -- Mesa/Rectifier-style distortion
        // =====================================================================
        // Heavy saturation with scooped mids (the classic "metal" EQ curve).
        // Two cascaded triode stages provide the extreme gain needed for
        // sustained, harmonically rich distortion. The mid scoop prevents
        // the heavy saturation from sounding muddy.
        static const ModelParams highGain = {
            /* preGain */           8.0,
            /* biasShift */         0.2,
            /* saturationAmount */  0.85,
            /* useSecondStage */    true,
            /* biasShift2 */        0.1,
            /* saturationAmount2 */ 0.7,
            /* bassGainDb */        3.0,
            /* midGainDb */        -2.5,
            /* trebleGainDb */      4.0
        };

        // =====================================================================
        // Type 3: Plexi -- Marshall 1959 Super Lead
        // =====================================================================
        // Rich harmonics, moderate gain, classic channel-jumping character.
        // The 1959 has two channels (Normal and Bright) that players often
        // "jump" together with a patch cable, creating phase cancellation
        // comb filtering that gives the Plexi its distinctive bark.
        //
        // 4 x ECC83 preamp tubes into 2 x EL34 power tubes. When cranked,
        // the cascaded preamp stages produce thick, harmonically complex
        // distortion with a pronounced presence peak around 800Hz-1.5kHz
        // (the famous "Plexi bark" that cuts through a band mix).
        //
        // Players: Hendrix, Angus Young, early Van Halen.
        static const ModelParams plexi = {
            /* preGain */           5.0,
            /* biasShift */         0.12,
            /* saturationAmount */  0.55,
            /* useSecondStage */    true,
            /* biasShift2 */        0.08,
            /* saturationAmount2 */ 0.45,
            /* bassGainDb */        2.0,
            /* midGainDb */         4.0,
            /* trebleGainDb */      2.0
        };

        // =====================================================================
        // Type 4: Hiwatt -- Hiwatt DR103 Custom 100
        // =====================================================================
        // Tight, articulate, extremely high headroom. The DR103 is famous
        // for its massive negative feedback loop (much more NFB than a
        // Marshall), which keeps the amp clean to very high volumes and
        // produces nearly symmetric clipping when it does break up.
        //
        // The result is an almost hi-fi-like response: flat, tight, and
        // articulate. Where a Marshall gets "spongy" at volume, the Hiwatt
        // stays controlled and precise. When it finally clips, the breakup
        // is smooth and symmetric (closer to solid-state than other tubes).
        //
        // Lower preGain models the extended headroom -- the amp simply
        // refuses to distort until pushed very hard.
        //
        // Players: Pete Townshend, David Gilmour, The Who.
        static const ModelParams hiwatt = {
            /* preGain */           3.0,
            /* biasShift */         0.08,
            /* saturationAmount */  0.4,
            /* useSecondStage */    false,
            /* biasShift2 */        0.0,
            /* saturationAmount2 */ 0.0,
            /* bassGainDb */        0.5,
            /* midGainDb */        -0.5,
            /* trebleGainDb */      1.5  // Hiwatt has flattest tone stack of classic amps
        };

        // =====================================================================
        // Type 5: Silver Jubilee -- Marshall 2555 (1987 Anniversary)
        // =====================================================================
        // Smooth, mid-forward, uses diode clipping in the preamp alongside
        // the tube stages. The 2555 added silicon diode clipping to the
        // Marshall circuit, creating a more compressed, singing sustain
        // compared to the raw tube distortion of the JCM800.
        //
        // Two cascaded stages model the diode clip stage followed by a
        // tube gain stage, producing very rich harmonics that are smoother
        // and more "liquid" than the aggressive JCM800. Strong midrange
        // emphasis gives the characteristic vocal quality.
        //
        // Players: John Frusciante ("Dani California"), Slash (some recordings).
        static const ModelParams silverJubilee = {
            /* preGain */           6.0,
            /* biasShift */         0.18,
            /* saturationAmount */  0.7,
            /* useSecondStage */    true,
            /* biasShift2 */        0.1,
            /* saturationAmount2 */ 0.55,
            /* bassGainDb */        1.5,
            /* midGainDb */         4.5,
            /* trebleGainDb */      1.5
        };

        // =====================================================================
        // Type 6: Vox AC30 -- Vox AC30 Top Boost
        // =====================================================================
        // Chimey, bright, jangly, with early breakup at moderate volumes.
        // The AC30 uses EL84 power tubes which have a distinctly different
        // saturation characteristic from EL34s: more "fizzy" and compressed,
        // with earlier breakup due to the lower headroom of the smaller tubes.
        //
        // The Top Boost channel adds a treble-forward EQ that creates the
        // AC30's signature "chime" -- a sparkling, bell-like treble that
        // is immediately recognizable. The reduced bass and slight mid
        // scoop keep the sound open and prevent muddiness even at high gain.
        //
        // Single-channel (Top Boost) means no cascaded stages; the
        // character comes from the EL84 breakup and bright tone stack.
        //
        // Players: The Beatles, Brian May, The Edge.
        static const ModelParams voxAC30 = {
            /* preGain */           3.5,
            /* biasShift */         0.1,
            /* saturationAmount */  0.5,
            /* useSecondStage */    false,
            /* biasShift2 */        0.0,
            /* saturationAmount2 */ 0.0,
            /* bassGainDb */       -1.5,
            /* midGainDb */        -1.0,
            /* trebleGainDb */      4.0
        };

        // =====================================================================
        // Type 7: Fender Twin -- Fender Twin Reverb
        // =====================================================================
        // Maximum headroom, pristine clean, deep bass, scooped mids.
        // The Twin Reverb is the quintessential clean amp: 85-100 watts
        // from 4 x 6L6GC power tubes with massive headroom. It stays
        // sparkling clean at volumes that would overdrive most other amps.
        //
        // The extremely low preGain (1.2) reflects this -- the amp barely
        // enters the nonlinear region even at moderate input levels. The
        // characteristic tone has deep bass, slightly scooped mids (the
        // classic "Fender scoop"), and bright, present treble.
        //
        // When it does finally break up (rare in practice), the 6L6GC
        // clipping is smooth and relatively symmetric.
        //
        // Players: jazz guitarists, country (Telecaster through Twin),
        // any application requiring maximum clean headroom.
        static const ModelParams fenderTwin = {
            /* preGain */           1.2,
            /* biasShift */         0.03,
            /* saturationAmount */  0.2,
            /* useSecondStage */    false,
            /* biasShift2 */        0.0,
            /* saturationAmount2 */ 0.0,
            /* bassGainDb */        2.5,
            /* midGainDb */        -2.0,
            /* trebleGainDb */      3.0
        };

        // =====================================================================
        // Type 8: Silvertone -- Silvertone 1485
        // =====================================================================
        // "Trashy," compressed, lo-fi, early breakup -- the anti-hi-fi amp.
        // The Silvertone 1485 used 6L6GC power tubes like the Fender Twin,
        // but with low-quality output transformers and simpler circuitry
        // that saturates much earlier. The result is a gritty, compressed
        // sound that breaks up at low volumes.
        //
        // The high biasShift (0.2) pushes the operating point deep into
        // the nonlinear region, and the moderate saturationAmount means
        // the tube is always on the edge of clipping. The loose, boosted
        // bass with rolled-off treble creates the characteristic lo-fi,
        // "garage rock" tone -- everything sounds slightly crushed and raw.
        //
        // Players: Jack White (The White Stripes), garage/punk rock.
        static const ModelParams silvertone = {
            /* preGain */           4.5,
            /* biasShift */         0.2,
            /* saturationAmount */  0.65,
            /* useSecondStage */    false,
            /* biasShift2 */        0.0,
            /* saturationAmount2 */ 0.0,
            /* bassGainDb */        3.0,
            /* midGainDb */         2.5,
            /* trebleGainDb */     -1.0
        };

        switch (modelType) {
            case 1:  return crunch;
            case 2:  return highGain;
            case 3:  return plexi;
            case 4:  return hiwatt;
            case 5:  return silverJubilee;
            case 6:  return voxAC30;
            case 7:  return fenderTwin;
            case 8:  return silvertone;
            default: return clean;
        }
    }

    // =========================================================================
    // DSP processing stages
    // =========================================================================

    /**
     * First-order high-pass filter (coupling capacitor / DC blocker).
     *
     * In a tube amp, coupling capacitors between stages block DC and very
     * low frequencies. This prevents DC bias from one stage affecting the
     * next and removes subsonic content that would waste headroom.
     *
     * Standard DC blocker: y[n] = x[n] - x[n-1] + R * y[n-1]
     * where R = coeff = exp(-2*pi*cutoff/sampleRate)
     *
     * At 44100Hz: coeff = exp(-2*pi*30/44100) ~= 0.9957, giving -3dB at ~30Hz
     * with 6dB/octave rolloff below. R close to 1.0 means the filter passes
     * almost everything except very low frequencies and DC.
     *
     * We use the leaky integrator form where state stores the DC estimate
     * and we subtract it from the input.
     *
     * @param input    Current input sample.
     * @param state    Filter state: stores the DC estimate (leaky integrator).
     * @param coeff    Pre-computed coefficient R = exp(-2*pi*fc/fs).
     * @return High-pass filtered output.
     */
    static double processHighPass(double input, double& state, double coeff) {
        // Leaky integrator DC blocker:
        //   dcEstimate = R * dcEstimate + (1 - R) * input
        //   output = input - dcEstimate
        //
        // This is equivalent to a first-order high-pass filter.
        // The dcEstimate tracks the slowly-moving DC component of the signal.
        // Subtracting it removes DC while passing AC components.
        state = coeff * state + (1.0 - coeff) * input;
        return input - state;
    }

    /**
     * Triode waveshaping (asymmetric soft clipping).
     *
     * Models the transfer characteristic of a 12AX7 vacuum tube triode.
     * Real tubes have asymmetric clipping behavior:
     *
     * Positive half-cycle (grid clipping):
     *   When the grid voltage exceeds ~0V, grid current flows and the
     *   signal is hard-clipped. We model this with tanh() which provides
     *   smooth saturation approaching +1.
     *
     * Negative half-cycle (plate saturation):
     *   As the grid goes more negative, the tube approaches cutoff
     *   gradually. We model this with a softer curve: x/(1+|x|) which
     *   has a gentler knee than tanh, matching the tube's gradual cutoff.
     *
     * The asymmetry between positive and negative clipping generates even
     * harmonics (2nd, 4th, 6th) in addition to odd harmonics. This
     * even-harmonic content is what gives tube amps their musical warmth
     * compared to symmetric clippers (transistors, op-amps).
     *
     * A bias shift moves the operating point so that the signal crosses
     * into the nonlinear regions more (higher gain) or less (lower gain).
     *
     * @param input     Input sample.
     * @param biasShift DC offset to shift the operating point.
     * @param saturation Saturation amount (0=clean, 1=heavily saturated).
     * @return Waveshaped output sample.
     */
    static double processTriode(double input, double biasShift,
                                double saturation) {
        // Apply bias shift to move operating point
        double biased = input + biasShift;

        // Scale saturation: at low values, keep signal mostly linear;
        // at high values, drive deep into the nonlinear curves.
        // The blend factor interpolates between the linear input and
        // the nonlinear waveshaped version.
        double shaped;
        if (biased >= 0.0) {
            // Positive half: tanh clipping (models grid clipping)
            // tanh(x) saturates at +/-1, giving hard but smooth clipping
            // on positive peaks. This is the "crunch" part of the sound.
            shaped = fast_math::fast_tanh(biased);
        } else {
            // Negative half: softer saturation (models plate cutoff)
            // x / (1 + |x|) has the same shape as tanh but with a
            // gentler knee, meaning the tube "rounds off" more gradually
            // on negative excursions. This is what gives tube distortion
            // its asymmetric, musical quality.
            shaped = biased / (1.0 + std::abs(biased));
        }

        // Blend between clean (linear) and saturated based on saturation param.
        // At saturation=0: output is nearly the original signal (just biased).
        // At saturation=1: output is fully the waveshaped version.
        double output = input * (1.0 - saturation) + shaped * saturation;

        // Remove the DC component introduced by the bias shift.
        // The asymmetric clipping of a biased signal generates a DC offset
        // in the output. We subtract the expected DC to keep the signal
        // centered. Not perfect (depends on signal level), but the coupling
        // cap high-pass in the next stage handles the rest.
        output -= biasShift * saturation * 0.3;

        return output;
    }

    /**
     * Three-band tone stack using SVF (State Variable Filter) peaking EQ.
     *
     * Models a simplified version of the Baxandall tone stack found in
     * many guitar amplifiers. Uses the Cytomic/Simper SVF topology for
     * numerical stability (same as the ParametricEQ effect).
     *
     * Fixed center frequencies:
     *   Bass:   120 Hz  (guitar's fundamental range)
     *   Mid:    800 Hz  (presence/body range)
     *   Treble: 3500 Hz (sparkle/bite range)
     *
     * The gain for each band comes from the ModelParams preset, giving
     * each amp model its characteristic EQ curve.
     *
     * @param input  Input sample.
     * @param bassGain   Linear gain for the bass band.
     * @param midGain    Linear gain for the mid band.
     * @param trebleGain Linear gain for the treble band.
     * @return Tone-shaped output sample.
     */
    double processToneStack(double input, double bassGain, double midGain,
                            double trebleGain) {
        // Bass band: 120Hz peaking EQ
        input = processSvfPeaking(input, tsBassG_, tsBassK_, bassGain,
                                  tsBassZ1_, tsBassZ2_);

        // Mid band: 800Hz peaking EQ
        input = processSvfPeaking(input, tsMidG_, tsMidK_, midGain,
                                  tsMidZ1_, tsMidZ2_);

        // Treble band: 3500Hz peaking EQ
        input = processSvfPeaking(input, tsTrebG_, tsTrebK_, trebleGain,
                                  tsTrebZ1_, tsTrebZ2_);

        return input;
    }

    /**
     * SVF peaking EQ (identical to ParametricEQ implementation).
     *
     * Uses the Cytomic (Andrew Simper) SVF topology:
     *   hp = (input - (k + g) * z1 - z2) / (1 + k*g + g*g)
     *   bp = g * hp + z1
     *   lp = g * bp + z2
     *   z1 = 2*bp - z1
     *   z2 = 2*lp - z2
     *   output = input + (gain - 1) * bp
     *
     * @param input Input sample.
     * @param g     SVF frequency coefficient: tan(pi * freq / sampleRate).
     * @param k     SVF Q coefficient: 1/Q.
     * @param gain  Linear gain for the peaking EQ.
     * @param z1    SVF state variable 1 (bandpass integrator).
     * @param z2    SVF state variable 2 (lowpass integrator).
     * @return Processed sample with peaking EQ applied.
     */
    static double processSvfPeaking(double input, double g, double k,
                                    double gain, double& z1, double& z2) {
        double hp = (input - (k + g) * z1 - z2) / (1.0 + k * g + g * g);
        double bp = g * hp + z1;
        double lp = g * bp + z2;

        z1 = 2.0 * bp - z1;
        z2 = 2.0 * lp - z2;

        return input + (gain - 1.0) * bp;
    }

    /**
     * Soft limiter using tanh for the output stage.
     *
     * Prevents harsh digital clipping at the output by applying a smooth
     * saturation curve. This models the gentle compression of a tube
     * power amp section, where the output transformer and power tubes
     * gradually limit peaks rather than hard-clipping them.
     *
     * The scaling factor of 0.9 keeps the output slightly below unity
     * to provide headroom for downstream effects.
     *
     * @param input Input sample (potentially > 1.0 from gain stages).
     * @return Soft-limited output in approximately [-0.9, 0.9].
     */
    static double softLimit(double input) {
        return fast_math::fast_tanh(input) * 0.9;
    }

    /**
     * Recalculate sample-rate-dependent filter coefficients.
     *
     * Called from setSampleRate() whenever the device sample rate changes.
     * Pre-computes the SVF 'g' coefficients for the tone stack bands and
     * the coupling cap high-pass coefficient so they don't need to be
     * recomputed every sample.
     */
    void recalculateCoefficients() {
        const double sr = static_cast<double>(sampleRate_);

        // Coupling cap high-pass: ~30Hz cutoff
        // R = e^(-2*pi*fc/fs) where R is close to 1.0
        // At 44100Hz: coeff = exp(-2*pi*30/44100) ~= 0.9957
        couplingCapCoeff_ = std::exp(-2.0 * kPi * 30.0 / sr);

        // Separate coefficient for the 2x oversampled path.
        // The coupling cap inside the oversampled loop runs at 2x sample rate,
        // so its coefficient must be computed at 2x rate to maintain the
        // intended 30Hz cutoff. Without this, the cutoff doubles to 60Hz.
        couplingCapCoeffOversampled_ = std::exp(-2.0 * kPi * 30.0 / (sr * 2.0));

        // Tone stack SVF coefficients
        // g = tan(pi * centerFreq / sampleRate)
        // k = 1/Q (using Q=1.0 for moderate bandwidth, ~1.4 octaves)
        tsBassG_   = std::tan(kPi * 120.0 / sr);
        tsBassK_   = 1.0 / 0.707;  // Butterworth Q for smooth response

        tsMidG_    = std::tan(kPi * 800.0 / sr);
        tsMidK_    = 1.0 / 1.0;    // Moderate Q for mid band

        tsTrebG_   = std::tan(kPi * 3500.0 / sr);
        tsTrebK_   = 1.0 / 0.707;  // Butterworth Q
    }

    // =========================================================================
    // Neural model helpers (private)
    // =========================================================================

    /**
     * Prewarm the currently loaded neural model.
     *
     * Feeds a short buffer of silence through the model to stabilize its
     * internal state (LSTM hidden states, delay lines, etc.). Without
     * prewarming, the first few buffers of real audio may produce transient
     * artifacts as the network's internal state ramps from zero.
     *
     * Caller must hold neuralModelMutex_.
     */
    void prewarmNeuralModel() {
        if (!neuralModel_) return;
        prewarmModel(neuralModel_.get());
    }

    /**
     * Prewarm a specific NAM model instance (may not be the active model).
     *
     * Used by loadNeuralModel() to prewarm a newly created model BEFORE
     * swapping it in, so the audio thread never sees an un-prewarmed model.
     *
     * @param model Pointer to the nam::DSP instance to prewarm.
     */
    static void prewarmModel(nam::DSP* model) {
        if (!model) return;

        // Feed ~100ms of silence at 48kHz (4800 samples) in chunks.
        // This is enough for most LSTM-based models to settle.
        const int prewarmSamples = 4800;
        const int chunkSize = 512;
        std::vector<NAM_SAMPLE> silenceIn(chunkSize, 0.0);
        std::vector<NAM_SAMPLE> silenceOut(chunkSize, 0.0);

        int remaining = prewarmSamples;
        while (remaining > 0) {
            const int n = std::min(remaining, chunkSize);
            // NAM uses multi-channel pointer-to-pointer API.
            // For mono: single-element pointer arrays.
            NAM_SAMPLE* inPtr = silenceIn.data();
            NAM_SAMPLE* outPtr = silenceOut.data();
            model->process(&inPtr, &outPtr, n);
            remaining -= n;
        }
    }

    // =========================================================================
    // Utility
    // =========================================================================

    /** Clamp a float value to [minVal, maxVal]. */
    static float clamp(float value, float minVal, float maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    /** Pi constant in double precision for filter calculations. */
    static constexpr double kPi = 3.14159265358979323846;

    /**
     * Maximum buffer size in frames for pre-allocated buffers.
     * Must match SignalChain's dryBuffer pre-allocation (8192) to handle
     * Oboe's maximum possible callback size.
     */
    static constexpr int kMaxBufferFrames = 8192;

    // =========================================================================
    // Parameters (written from UI thread, read from audio thread)
    // =========================================================================

    std::atomic<float> inputGain_{1.0f};    // [0.0, 2.0] drive into the model
    std::atomic<float> outputLevel_{0.7f};  // [0.0, 1.0] output volume
    std::atomic<float> modelType_{0.0f};    // [0-8] selects amp voicing
    std::atomic<float> variac_{0.0f};       // [0.0, 1.0] voltage sag amount

    // =========================================================================
    // Neural amp model state
    // =========================================================================

    /** NAM DSP model instance. Null when no neural model is loaded. */
    std::unique_ptr<nam::DSP> neuralModel_;

    /**
     * Mutex protecting neuralModel_ during swap and processing.
     *
     * Held briefly by process() (for the neural path duration, ~1-5ms)
     * and by loadNeuralModel()/clearNeuralModel() (for the pointer swap,
     * ~nanoseconds). This ensures the model is not destroyed while the
     * audio thread is using it.
     */
    std::mutex neuralModelMutex_;

    /**
     * Atomic flag: true when neural mode is active.
     *
     * Checked first in process() without taking the mutex, so the classic
     * path has zero synchronization overhead. Uses acquire/release ordering
     * to ensure the audio thread sees the fully constructed model after
     * loadNeuralModel() sets this flag.
     */
    std::atomic<bool> useNeuralMode_{false};

    /**
     * Pre-allocated double-precision input buffer for NAM inference.
     *
     * NAM_SAMPLE defaults to double, but our audio pipeline uses float.
     * This buffer is sized in setSampleRate() and reused every process()
     * call to avoid audio-thread allocation.
     */
    std::vector<NAM_SAMPLE> namInputBuffer_;

    /**
     * Pre-allocated double-precision output buffer for NAM inference.
     * Same rationale as namInputBuffer_.
     */
    std::vector<NAM_SAMPLE> namOutputBuffer_;

    // =========================================================================
    // Classic mode audio thread state (not accessed from UI thread)
    // =========================================================================

    int32_t sampleRate_ = 44100;

    // Coupling cap (DC-blocking high-pass) filter states
    double couplingCapState_ = 0.0;   // First stage coupling cap
    double couplingCap2State_ = 0.0;  // Second stage coupling cap (high-gain only)
    double couplingCapCoeff_ = 0.9957; // Pre-computed: exp(-2*pi*30/44100)
    double couplingCapCoeffOversampled_ = 0.9979; // Coefficient at 2x sample rate

    // Tone stack SVF pre-computed coefficients
    double tsBassG_ = 0.0;    // Bass band: g = tan(pi * 120 / sr)
    double tsBassK_ = 1.414;  // Bass band: k = 1/Q
    double tsMidG_ = 0.0;     // Mid band: g = tan(pi * 800 / sr)
    double tsMidK_ = 1.0;     // Mid band: k = 1/Q
    double tsTrebG_ = 0.0;    // Treble band: g = tan(pi * 3500 / sr)
    double tsTrebK_ = 1.414;  // Treble band: k = 1/Q

    // Tone stack SVF state variables (64-bit for low-frequency stability)
    double tsBassZ1_ = 0.0, tsBassZ2_ = 0.0;
    double tsMidZ1_ = 0.0,  tsMidZ2_ = 0.0;
    double tsTrebZ1_ = 0.0, tsTrebZ2_ = 0.0;

    // 2x oversampler for anti-aliased waveshaping.
    // The triode waveshaping (tanh, rational functions) generates harmonics
    // that alias without oversampling. 2x is sufficient for the amp model's
    // softer saturation curves; the Overdrive uses 8x for its harder clipping.
    Oversampler<2> oversampler_;

    // Pre-allocated buffer for input gain stage (before upsampling).
    // Separates gain application from the oversampled waveshaping loop.
    std::vector<float> preBuffer_;
};

#endif // GUITAR_ENGINE_AMP_MODEL_H
