package com.guitaremulator.app.audio

/**
 * Interface for the audio engine, abstracting the JNI-backed native implementation.
 *
 * This interface exists to enable unit testing with Mockito. The concrete
 * [AudioEngine] class uses `external fun` (JNI native methods) which Mockito
 * cannot mock because they compile to `native` methods in JVM bytecode.
 * By programming against this interface, tests can mock it freely.
 *
 * All methods mirror the corresponding [AudioEngine] JNI functions 1:1.
 */
interface IAudioEngine {

    // Engine lifecycle
    fun startEngine(): Boolean
    fun stopEngine()
    fun setInputSource(type: Int)
    fun setInputDeviceId(deviceId: Int)
    fun setOutputDeviceId(deviceId: Int)
    fun isRunning(): Boolean

    // Level metering
    fun getInputLevel(): Float
    fun getOutputLevel(): Float

    // Effect chain control
    fun getEffectCount(): Int
    fun setEffectEnabled(effectIndex: Int, enabled: Boolean)
    fun setEffectParameter(effectIndex: Int, paramId: Int, value: Float)
    fun getEffectParameter(effectIndex: Int, paramId: Int): Float
    fun setEffectWetDryMix(effectIndex: Int, mix: Float)
    fun getEffectWetDryMix(effectIndex: Int): Float
    fun isEffectEnabled(effectIndex: Int): Boolean

    // Master gain and bypass
    fun setInputGain(gain: Float)
    fun setOutputGain(gain: Float)
    fun setGlobalBypass(bypassed: Boolean)
    fun getInputGain(): Float
    fun getOutputGain(): Float
    fun isGlobalBypassed(): Boolean

    // Buffer size
    fun setBufferMultiplier(multiplier: Int)

    // Cabinet IR
    fun loadCabinetIR(irData: FloatArray, sampleRate: Int): Boolean

    // Neural amp model
    fun loadNeuralModel(path: String): Boolean
    fun clearNeuralModel()
    fun isNeuralModelLoaded(): Boolean

    // Effect reordering
    fun setEffectOrder(order: IntArray): Boolean
    fun getEffectOrder(): IntArray

    // Tuner activation
    fun setTunerActive(active: Boolean)
    fun isTunerActive(): Boolean

    // Spectrum analyzer
    fun getSpectrumData(): FloatArray

    // Device info
    fun getSampleRate(): Int
    fun getFramesPerBuffer(): Int
    fun getEstimatedLatencyMs(): Float

    // Looper
    fun looperInit(maxDurationSeconds: Int): Boolean
    fun looperRelease()
    fun looperSendCommand(command: Int)
    fun looperGetState(): Int
    fun looperGetPlaybackPosition(): Int
    fun looperGetLoopLength(): Int
    fun looperSetVolume(volume: Float)

    // Per-layer volume control
    fun looperSetLayerVolume(layer: Int, volume: Float)
    fun looperGetLayerVolume(layer: Int): Float
    fun looperGetLayerCount(): Int

    // Metronome
    fun setMetronomeActive(active: Boolean)
    fun setMetronomeBPM(bpm: Float)
    fun setTimeSignature(beatsPerBar: Int, beatUnit: Int)
    fun getCurrentBeat(): Int
    fun setMetronomeVolume(volume: Float)
    fun setMetronomeTone(tone: Int)
    fun getMetronomeTone(): Int

    // Tap tempo
    fun onTapTempo(timestampMs: Long)
    fun getTapTempoBPM(): Float

    // Quantization
    fun setQuantizedMode(enabled: Boolean)
    fun setQuantizedBars(numBars: Int)
    fun setCountInEnabled(enabled: Boolean)

    // Loop export
    fun looperExportLoop(dryPath: String, wetPath: String): Boolean

    // Re-amping
    fun loadDryForReamp(path: String): Boolean
    fun startReamp()
    fun stopReamp()
    fun isReamping(): Boolean

    // Linear recording
    fun startLinearRecording(dryPath: String, wetPath: String): Boolean
    fun stopLinearRecording(): Boolean

    // Trim / crop
    fun startTrimPreview()
    fun cancelTrimPreview()
    fun setTrimStart(startSample: Int)
    fun setTrimEnd(endSample: Int)
    fun getTrimStart(): Int
    fun getTrimEnd(): Int
    fun isTrimPreviewActive(): Boolean
    fun commitTrim(): Boolean
    fun isCropUndoAvailable(): Boolean
    fun undoCrop(): Boolean
    fun getWaveformSummary(numColumns: Int, startSample: Int, endSample: Int): FloatArray?
    fun findNearestZeroCrossing(targetSample: Int, searchRadius: Int): Int
    fun snapToBeat(targetSample: Int): Int
}
