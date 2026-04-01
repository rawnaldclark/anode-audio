package com.guitaremulator.app.audio

/**
 * JNI bridge to the native C++ audio engine built with Oboe.
 *
 * This class provides the Kotlin interface to the underlying native audio
 * processing pipeline. Each method maps 1:1 to a JNI function defined in
 * jni_bridge.cpp.
 *
 * Thread safety: The native engine uses std::atomic for all state shared
 * between the audio callback thread and the JNI calling thread. It is safe
 * to call these methods from any thread, but start/stop should be serialized
 * by the caller (AudioViewModel or Service) to avoid redundant stream opens.
 *
 * Usage:
 *   val engine = AudioEngine()
 *   engine.startEngine()       // Opens streams and begins passthrough
 *   engine.getInputLevel()     // Poll for level metering
 *   engine.stopEngine()        // Closes streams and releases resources
 */
class AudioEngine : IAudioEngine {

    companion object {
        init {
            try {
                System.loadLibrary("guitar_engine")
            } catch (_: UnsatisfiedLinkError) {
                // Expected in JVM unit tests where native libs aren't available.
                // Mocked AudioEngine instances don't call native methods.
            }
        }
    }

    /**
     * Start the audio engine. Opens full-duplex input/output streams using
     * the device's native sample rate and begins audio processing.
     *
     * @return true if the engine started successfully, false on error.
     */
    override external fun startEngine(): Boolean

    /**
     * Stop the audio engine and release all native audio resources.
     * Safe to call even if the engine is not running.
     */
    override external fun stopEngine()

    /**
     * Set the preferred input source for audio capture.
     *
     * @param type Input source type:
     *   0 = PHONE_MIC (built-in microphone)
     *   1 = ANALOG_ADAPTER (wired headset/adapter with mic input)
     *   2 = USB_AUDIO (USB audio interface)
     */
    override external fun setInputSource(type: Int)

    /**
     * Set the input device ID for the recording stream.
     * When > 0, Oboe will open the input stream on this specific device
     * instead of the system default. Must be called before startEngine().
     *
     * @param deviceId Android AudioDeviceInfo.getId(), or 0 for system default.
     */
    override external fun setInputDeviceId(deviceId: Int)

    /**
     * Set the output device ID for the playback stream.
     * When > 0, Oboe will open the output stream on this specific device
     * instead of the system default. Critical for USB audio interfaces:
     * ensures processed audio routes back through USB to the iRig's
     * headphone/amp output instead of the phone speaker.
     * Must be called before startEngine().
     *
     * @param deviceId Android AudioDeviceInfo.getId(), or 0 for system default.
     */
    override external fun setOutputDeviceId(deviceId: Int)

    /**
     * Check if the audio engine is currently running and processing audio.
     *
     * @return true if the engine is active, false otherwise.
     */
    override external fun isRunning(): Boolean

    /**
     * Get the current input signal RMS level.
     * Updated on every audio callback (~every 5-10ms depending on buffer size).
     *
     * @return Float in [0.0, 1.0] representing input loudness.
     */
    override external fun getInputLevel(): Float

    /**
     * Get the current output signal RMS level after effects processing.
     * Updated on every audio callback.
     *
     * @return Float in [0.0, 1.0] representing output loudness.
     */
    override external fun getOutputLevel(): Float

    // =========================================================================
    // Effect chain control
    // =========================================================================

    /**
     * Get the number of effects in the signal chain.
     *
     * Default chain order (25 effects):
     *   Index 0  = Noise Gate
     *   Index 1  = Compressor
     *   Index 2  = Wah (disabled by default)
     *   Index 3  = Boost (disabled by default)
     *   Index 4  = Overdrive
     *   Index 5  = Amp Model (disabled by default)
     *   Index 6  = Cabinet Sim (disabled by default)
     *   Index 7  = Parametric EQ
     *   Index 8  = Chorus
     *   Index 9  = Vibrato (disabled by default)
     *   Index 10 = Phaser (disabled by default)
     *   Index 11 = Flanger (disabled by default)
     *   Index 12 = Delay
     *   Index 13 = Reverb
     *   Index 14 = Tuner (read-only, passthrough)
     *   Index 15 = Tremolo (FAUST-generated, disabled by default)
     *   Index 16 = Boss DS-1 (WDF, disabled by default)
     *   Index 17 = ProCo RAT (WDF, disabled by default)
     *   Index 18 = Boss DS-2 (WDF, disabled by default)
     *   Index 19 = Boss HM-2 (WDF, disabled by default)
     *   Index 20 = Uni-Vibe (disabled by default)
     *   Index 21 = Fuzz Face (WDF Ge, disabled by default)
     *   Index 22 = Rangemaster (WDF Ge, disabled by default)
     *   Index 23 = Big Muff Pi (WDF, disabled by default)
     *   Index 24 = Octavia (octave-up fuzz, disabled by default)
     *
     * @return Number of effects currently in the chain.
     */
    override external fun getEffectCount(): Int

    /**
     * Enable or disable an effect by its index in the chain.
     * Disabled effects pass audio through unchanged with zero processing cost.
     *
     * @param effectIndex Zero-based index of the effect (0-24).
     * @param enabled     True to enable processing, false to bypass.
     */
    override external fun setEffectEnabled(effectIndex: Int, enabled: Boolean)

    /**
     * Set a parameter on a specific effect.
     *
     * Each effect defines its own parameter IDs. See the C++ effect headers
     * or [com.guitaremulator.app.data.EffectRegistry] for the complete list
     * of parameter IDs, ranges, and defaults for all 25 effects.
     *
     * @param effectIndex Zero-based index of the effect in the chain (0-24).
     * @param paramId     Effect-specific parameter identifier.
     * @param value       New parameter value (range depends on the parameter).
     */
    override external fun setEffectParameter(effectIndex: Int, paramId: Int, value: Float)

    /**
     * Get the current value of a parameter on a specific effect.
     *
     * @param effectIndex Zero-based index of the effect in the chain.
     * @param paramId     Effect-specific parameter identifier.
     * @return Current parameter value, or 0.0 if the index is out of range.
     */
    override external fun getEffectParameter(effectIndex: Int, paramId: Int): Float

    /**
     * Set the wet/dry mix for an effect.
     *
     * Controls the blend between the original (dry) signal and the
     * processed (wet) signal. Most useful for time-based effects like delay.
     *
     * @param effectIndex Zero-based index of the effect in the chain.
     * @param mix         0.0 = fully dry (bypass), 1.0 = fully wet (full effect).
     */
    override external fun setEffectWetDryMix(effectIndex: Int, mix: Float)

    /**
     * Get the current wet/dry mix for an effect.
     *
     * @param effectIndex Zero-based index of the effect in the chain.
     * @return Current mix value in [0.0, 1.0], or 1.0 if out of range.
     */
    override external fun getEffectWetDryMix(effectIndex: Int): Float

    /**
     * Check whether an effect is currently enabled.
     *
     * @param effectIndex Zero-based index of the effect in the chain.
     * @return True if the effect is enabled, false if bypassed or out of range.
     */
    override external fun isEffectEnabled(effectIndex: Int): Boolean

    // =========================================================================
    // Master gain and bypass controls
    // =========================================================================

    /**
     * Set the master input gain applied before effects processing.
     * Useful for attenuating hot pickups or boosting quiet acoustic signals
     * before they hit the noise gate and effects chain.
     *
     * @param gain Linear gain (0.0 = silence, 1.0 = unity, 2.0 = +6dB).
     */
    override external fun setInputGain(gain: Float)

    /**
     * Set the master output gain applied after effects processing.
     * Applied before the output soft limiter, so extreme values will be
     * smoothly limited rather than hard-clipped.
     *
     * @param gain Linear gain (0.0 = silence, 1.0 = unity).
     */
    override external fun setOutputGain(gain: Float)

    /**
     * Set global bypass: when true, audio passes through with no effects.
     * Useful for latency-free monitoring or A/B comparison.
     *
     * @param bypassed True to bypass all effects, false to process normally.
     */
    override external fun setGlobalBypass(bypassed: Boolean)

    /** Get the current master input gain. */
    override external fun getInputGain(): Float

    /** Get the current master output gain. */
    override external fun getOutputGain(): Float

    /** Check if global bypass is active. */
    override external fun isGlobalBypassed(): Boolean

    // =========================================================================
    // Buffer size control
    // =========================================================================

    /**
     * Set the audio buffer size multiplier (1..8).
     * Buffer size = framesPerBurst * multiplier. Higher = more latency, fewer glitches.
     * Can be called while the engine is running — applies immediately.
     *
     * @param multiplier Buffer size multiplier (clamped to 1..8).
     */
    override external fun setBufferMultiplier(multiplier: Int)

    // =========================================================================
    // Cabinet IR loading
    // =========================================================================

    /**
     * Load a user-provided cabinet impulse response for the Cabinet Sim effect.
     *
     * The IR data should be mono float samples normalized to [-1.0, 1.0].
     * Maximum supported length is 8192 samples. The native engine normalizes
     * the IR to unit peak amplitude and initializes the FFT convolver.
     *
     * NOT real-time safe — call from any non-audio thread.
     *
     * @param irData     Float array of mono IR samples.
     * @param sampleRate Sample rate the IR was recorded at.
     * @return true if the IR was loaded successfully.
     */
    override external fun loadCabinetIR(irData: FloatArray, sampleRate: Int): Boolean

    // =========================================================================
    // Neural amp model loading
    // =========================================================================

    /**
     * Load a neural amp model (.nam file) into the AmpModel effect.
     *
     * The path must be an absolute filesystem path (not a content URI).
     * The model is parsed and initialized on the calling thread, then
     * atomically swapped into the audio path. This is NOT real-time safe
     * and should be called from a background thread.
     *
     * @param path Absolute filesystem path to the .nam model file.
     * @return true if the model was loaded successfully.
     */
    override external fun loadNeuralModel(path: String): Boolean

    /**
     * Clear the loaded neural model, reverting AmpModel to classic waveshaping.
     */
    override external fun clearNeuralModel()

    /**
     * Check if a neural amp model is currently loaded and active.
     * @return true if neural mode is active, false if using classic waveshaping.
     */
    override external fun isNeuralModelLoaded(): Boolean

    // =========================================================================
    // Effect reordering
    // =========================================================================

    /**
     * Set the processing order of effects in the signal chain.
     * Uses a double-buffered atomic swap in C++ for lock-free reordering.
     *
     * @param order IntArray of physical effect indices in desired processing order.
     * @return true if the order was applied, false if the array length mismatches.
     */
    override external fun setEffectOrder(order: IntArray): Boolean

    /**
     * Get the current processing order of effects.
     * @return IntArray of physical effect indices in processing order.
     */
    override external fun getEffectOrder(): IntArray

    // =========================================================================
    // Tuner activation
    // =========================================================================

    /**
     * Activate or deactivate the tuner's pitch analysis on the audio thread.
     * When inactive, the tuner's 8192-point FFT is completely skipped.
     * Activate when the tuner UI is visible, deactivate when hidden.
     *
     * @param active true to start pitch analysis, false to stop.
     */
    override external fun setTunerActive(active: Boolean)

    /**
     * Check if the tuner is currently active.
     * @return true if tuner pitch analysis is running, false otherwise.
     */
    override external fun isTunerActive(): Boolean

    // =========================================================================
    // Spectrum analyzer
    // =========================================================================

    /**
     * Get the current spectrum data for UI visualization.
     * Returns 64 dB-scaled magnitude bins in logarithmic frequency spacing
     * (20Hz to 20kHz). Lock-free read from double-buffered output.
     *
     * @return FloatArray of 64 bins with values in [-90, 0] dB.
     */
    override external fun getSpectrumData(): FloatArray

    // =========================================================================
    // Device info and latency
    // =========================================================================

    /**
     * Get the device's native sample rate discovered during stream setup.
     * @return Sample rate in Hz, or 0 if the engine is not running.
     */
    override external fun getSampleRate(): Int

    /**
     * Get the audio buffer size (frames per burst).
     * @return Frames per buffer, or 0 if the engine is not running.
     */
    override external fun getFramesPerBuffer(): Int

    /**
     * Get estimated round-trip latency in milliseconds.
     * Combines input and output stream latencies when available.
     * @return Latency in ms, or 0 if the engine is not running.
     */
    override external fun getEstimatedLatencyMs(): Float

    // =========================================================================
    // Looper
    // =========================================================================

    /**
     * Initialize looper buffers. Safe to call multiple times.
     * @param maxDurationSeconds Maximum loop duration in seconds.
     * @return true on success.
     */
    override external fun looperInit(maxDurationSeconds: Int): Boolean

    /**
     * Release looper buffers and reset state.
     */
    override external fun looperRelease()

    /**
     * Send a transport command to the looper.
     * @param command 0=None, 1=Record, 2=Stop, 3=Overdub, 4=Clear, 5=Undo, 6=Toggle
     */
    override external fun looperSendCommand(command: Int)

    /**
     * Get the current looper state.
     * @return 0=Idle, 1=Recording, 2=Playing, 3=Overdubbing, 4=Finishing, 5=CountIn
     */
    override external fun looperGetState(): Int

    /**
     * Get the current playback position in samples.
     * @return Playback position for waveform cursor display.
     */
    override external fun looperGetPlaybackPosition(): Int

    /**
     * Get the total loop length in samples.
     * @return Loop length, or 0 if no loop recorded.
     */
    override external fun looperGetLoopLength(): Int

    /**
     * Set the loop playback volume.
     * @param volume 0.0 = silent, 1.0 = unity gain.
     */
    override external fun looperSetVolume(volume: Float)

    // =========================================================================
    // Per-layer volume control
    // =========================================================================

    /**
     * Set the volume of an individual looper layer.
     * Layer 0 is the base recording; layers 1+ are overdub passes.
     * Triggers recomposition of the composite wet buffer.
     *
     * @param layer Layer index (0 to 7).
     * @param volume Linear gain (0.0 = silent, 1.0 = unity, up to 1.5 = boost).
     */
    override external fun looperSetLayerVolume(layer: Int, volume: Float)

    /**
     * Get the current volume of a looper layer.
     * @param layer Layer index (0 to 7).
     * @return Volume in [0.0, 1.5], or 1.0 if out of range.
     */
    override external fun looperGetLayerVolume(layer: Int): Float

    /**
     * Get the number of recorded looper layers.
     * @return 0 if no loop, 1 after base recording, 2+ after overdubs.
     */
    override external fun looperGetLayerCount(): Int

    // =========================================================================
    // Metronome
    // =========================================================================

    /**
     * Enable or disable the metronome click.
     * @param active true to enable, false to disable.
     */
    override external fun setMetronomeActive(active: Boolean)

    /**
     * Set the metronome BPM.
     * @param bpm Tempo in beats per minute (clamped to 30-300).
     */
    override external fun setMetronomeBPM(bpm: Float)

    /**
     * Set the time signature.
     * @param beatsPerBar Numerator (1-16).
     * @param beatUnit    Denominator (2, 4, 8, or 16).
     */
    override external fun setTimeSignature(beatsPerBar: Int, beatUnit: Int)

    /**
     * Get the current beat index within the bar (0-based).
     * @return Beat index for metronome indicator UI.
     */
    override external fun getCurrentBeat(): Int

    /**
     * Set the metronome click volume.
     * @param volume 0.0 = silent, 1.0 = full volume.
     */
    override external fun setMetronomeVolume(volume: Float)

    /**
     * Set the metronome tone type.
     *
     * @param tone Tone index (0-5):
     *   0 = Classic Click (high woodblock-like sine burst)
     *   1 = Soft Tick (lower, warmer tone)
     *   2 = Rim Shot (noise burst + tone)
     *   3 = Cowbell (dual-frequency metallic)
     *   4 = Hi-Hat (filtered noise burst)
     *   5 = Deep Click (low sine burst)
     */
    override external fun setMetronomeTone(tone: Int)

    /**
     * Get the current metronome tone type.
     * @return Tone index (0-5).
     */
    override external fun getMetronomeTone(): Int

    // =========================================================================
    // Tap tempo
    // =========================================================================

    /**
     * Register a tap for tap-tempo BPM detection.
     * @param timestampMs Monotonic timestamp from System.currentTimeMillis().
     */
    override external fun onTapTempo(timestampMs: Long)

    /**
     * Get the most recently detected tap-tempo BPM.
     * @return BPM from tap detection, or 0.0 if fewer than 2 taps.
     */
    override external fun getTapTempoBPM(): Float

    // =========================================================================
    // Quantization
    // =========================================================================

    /**
     * Enable or disable quantized (bar-snapping) mode.
     * @param enabled true for quantized mode, false for free-form.
     */
    override external fun setQuantizedMode(enabled: Boolean)

    /**
     * Set the number of bars for quantized loop length.
     * @param numBars Number of bars (clamped to 1-64).
     */
    override external fun setQuantizedBars(numBars: Int)

    /**
     * Enable or disable count-in (one bar of metronome clicks before recording).
     * @param enabled true to enable count-in, false to disable.
     */
    override external fun setCountInEnabled(enabled: Boolean)

    // =========================================================================
    // Loop export
    // =========================================================================

    /**
     * Export current loop buffers to WAV files on disk.
     * @param dryPath Absolute path for the dry WAV output file.
     * @param wetPath Absolute path for the wet WAV output file.
     * @return true if both files were written successfully.
     */
    override external fun looperExportLoop(dryPath: String, wetPath: String): Boolean

    // =========================================================================
    // Re-amping
    // =========================================================================

    /**
     * Load a dry WAV file into the re-amp buffer.
     * Replaces live input with recorded dry signal through the effects chain.
     * @param path Absolute path to the mono float32 WAV file.
     * @return true if the file was loaded successfully.
     */
    override external fun loadDryForReamp(path: String): Boolean

    /**
     * Start re-amp playback. Dry signal replaces live input.
     */
    override external fun startReamp()

    /**
     * Stop re-amp playback and return to live input.
     */
    override external fun stopReamp()

    /**
     * Check if re-amp playback is currently active.
     * @return true if re-amping.
     */
    override external fun isReamping(): Boolean

    // =========================================================================
    // Linear recording
    // =========================================================================

    /**
     * Start linear recording to disk. Captures live performance as dual WAV files.
     * @param dryPath Path for the dry WAV output file.
     * @param wetPath Path for the wet WAV output file.
     * @return true if recording started.
     */
    override external fun startLinearRecording(dryPath: String, wetPath: String): Boolean

    /**
     * Stop linear recording and finalize WAV files.
     * @return true if recording was stopped successfully.
     */
    override external fun stopLinearRecording(): Boolean

    // =========================================================================
    // Trim / crop
    // =========================================================================

    /** Enter trim preview mode. Constrains playback to [trimStart, trimEnd). */
    override external fun startTrimPreview()

    /** Cancel trim preview. Restores full loop playback. */
    override external fun cancelTrimPreview()

    /** Set trim start offset in samples. */
    override external fun setTrimStart(startSample: Int)

    /** Set trim end offset in samples. */
    override external fun setTrimEnd(endSample: Int)

    /** Get current trim start position. */
    override external fun getTrimStart(): Int

    /** Get current trim end position. */
    override external fun getTrimEnd(): Int

    /** Check if trim preview is active. */
    override external fun isTrimPreviewActive(): Boolean

    /** Commit the trim. Memmoves buffer data and re-applies crossfade. */
    override external fun commitTrim(): Boolean

    /** Check if crop undo is available. */
    override external fun isCropUndoAvailable(): Boolean

    /** Undo the last crop operation. */
    override external fun undoCrop(): Boolean

    /**
     * Get waveform summary for UI. Returns min/max pairs per column.
     * @param numColumns Display columns.
     * @param startSample Range start.
     * @param endSample Range end (-1 = full loop).
     * @return FloatArray [min0, max0, min1, max1, ...] or null.
     */
    override external fun getWaveformSummary(numColumns: Int, startSample: Int, endSample: Int): FloatArray?

    /**
     * Find nearest zero crossing.
     * @param targetSample Position to snap.
     * @param searchRadius Search distance (-1 = ~1ms).
     */
    override external fun findNearestZeroCrossing(targetSample: Int, searchRadius: Int): Int

    /** Snap sample position to nearest beat boundary. */
    override external fun snapToBeat(targetSample: Int): Int
}
