#ifndef GUITAR_ENGINE_LOOPER_ENGINE_H
#define GUITAR_ENGINE_LOOPER_ENGINE_H

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <thread>
#include <vector>

#include "../effects/fast_math.h"
#include "spsc_ring_buffer.h"
#include "wav_writer.h"

/**
 * Core looper engine for multi-layer loop recording, playback, and overdubbing.
 *
 * This engine manages pre-allocated audio buffers and provides real-time safe
 * recording, playback, and overdub capabilities with quantization support,
 * crossfading, one-level undo, metronome click generation, and tap tempo.
 *
 * Architecture overview:
 *   - Called from AudioEngine::onAudioReady() AFTER signal chain processing.
 *   - processAudioBlock() receives both dry (pre-effects) and wet (post-effects)
 *     input, and sums loop playback into the output buffer (which already contains
 *     the live processed signal).
 *   - Dry input is stored for re-amp support (replaying DI signal through
 *     modified effects chain).
 *   - All state transitions are driven by a single atomic command slot using
 *     exchange() for consume-once semantics: the audio thread claims the command
 *     exactly once per callback, preventing duplicate processing.
 *
 * Thread safety model:
 *   - UI thread:  calls sendCommand(), setters for BPM/quantize/volume, onTap().
 *   - Audio thread: calls processAudioBlock() which reads the command slot and
 *     all atomic parameters with relaxed ordering (audio thread is sole consumer).
 *   - Cross-thread visibility is guaranteed by acquire/release on command_ and
 *     state_, and relaxed ordering on continuously-polled parameters (volume,
 *     BPM, position) where eventual consistency is acceptable.
 *
 * Buffer layout:
 *   - loopDryBuffer_[0..loopLength_-1]: accumulated dry (DI) signal
 *   - loopWetBuffer_[0..loopLength_-1]: accumulated wet (processed) signal
 *   - loopUndoBuffer_[0..capacity-1]:   snapshot of wet before last overdub
 *
 * Crossfade strategy:
 *   At loop boundaries, a Hann-windowed crossfade eliminates clicks from
 *   discontinuities. The fade length is 5ms (240 samples at 48kHz), capped
 *   at 2% of loop length for very short loops, with a floor of 48 samples.
 *   Pre-computed fade tables avoid per-sample trig in the audio callback.
 *
 * Quantization:
 *   When quantized mode is active, loop length snaps to bar boundaries:
 *     samples = round((60.0 / BPM) * beatsPerBar * numBars * sampleRate)
 *   Recording auto-stops at the computed length. Early stop snaps to the
 *   nearest bar boundary (within 50% threshold) or the next bar.
 *
 * Re-amp (stub):
 *   Stores DI signal for later re-processing through a modified effects chain.
 *   Full implementation deferred to Sprint 25.
 *
 * Linear recording (stub):
 *   Hook for SPSC ring buffer writing to capture full performance to disk.
 *   Full implementation in Sprint 24e.
 *
 * Integration point:
 *   In AudioEngine::onAudioReady(), after signal chain processing and before
 *   the DC blocker / soft limiter, call:
 *     looperEngine_.processAudioBlock(dryBuffer, outputBuffer, outputBuffer, numFrames);
 *
 * Parameter summary (all set via atomic setters from UI/JNI thread):
 *   - volume:         loop playback level (0.0 to 1.0)
 *   - clickVolume:    metronome click level (0.0 to 1.0)
 *   - bpm:            tempo in beats per minute (30.0 to 300.0)
 *   - beatsPerBar:    time signature numerator (1 to 16)
 *   - beatUnit:       time signature denominator (2, 4, 8, 16)
 *   - numBars:        quantized loop length in bars (1 to 64)
 *   - quantizedMode:  snap loop boundaries to bar lines
 *   - countInEnabled: play one bar of clicks before recording starts
 */
class LooperEngine {
public:
    // =========================================================================
    // Public enums
    // =========================================================================

    /**
     * Looper state machine states.
     *
     * State transitions (command -> result):
     *   IDLE       + RECORD/TOGGLE -> COUNT_IN (if countIn) or RECORDING
     *   COUNT_IN   (auto)          -> RECORDING (after 1 bar of clicks)
     *   RECORDING  + STOP/TOGGLE   -> FINISHING (if quantized) or PLAYING
     *   RECORDING  (auto)          -> PLAYING (quantized auto-stop at bar boundary)
     *   FINISHING   (auto)         -> PLAYING (when quantized target reached)
     *   PLAYING    + OVERDUB/TOGGLE-> OVERDUBBING
     *   PLAYING    + STOP          -> IDLE (stops playback, keeps loop)
     *   PLAYING    + RECORD        -> RECORDING (clears and re-records)
     *   OVERDUBBING+ STOP/TOGGLE   -> PLAYING
     *   Any        + CLEAR         -> IDLE (clears all buffers)
     *   PLAYING    + UNDO          -> PLAYING (swaps wet with undo buffer)
     */
    enum class LooperState : int {
        IDLE        = 0,    ///< No loop loaded or recorded. Pass-through.
        RECORDING   = 1,    ///< Writing dry+wet input to loop buffers.
        PLAYING     = 2,    ///< Reading from loop buffer, summing into output.
        OVERDUBBING = 3,    ///< Playing back AND summing new input onto loop.
        FINISHING   = 4,    ///< Auto-completing to quantized bar boundary.
        COUNT_IN    = 5     ///< Playing metronome clicks before recording starts.
    };

    /**
     * Commands sent from UI/JNI thread to audio thread.
     * Consumed exactly once via atomic exchange in processAudioBlock().
     */
    enum class LooperCommand : int {
        NONE    = 0,    ///< No pending command.
        RECORD  = 1,    ///< Start recording (clears existing loop).
        STOP    = 2,    ///< Stop recording/overdubbing/playback.
        OVERDUB = 3,    ///< Start overdubbing onto existing loop.
        CLEAR   = 4,    ///< Clear all buffers and reset to IDLE.
        UNDO    = 5,    ///< Swap wet buffer with undo buffer (PLAYING only).
        TOGGLE  = 6     ///< Context-dependent: the main single-button action.
    };

    /**
     * Re-amp playback state (stub for Sprint 25).
     */
    enum class ReampState : int {
        IDLE    = 0,    ///< No re-amp playback active.
        PLAYING = 1     ///< Playing back dry signal for re-amping.
    };

    // =========================================================================
    // Construction / destruction
    // =========================================================================

    LooperEngine() {
        // Initialize per-layer volumes to unity gain
        for (int i = 0; i < kMaxLayers; ++i) {
            layerVolumes_[i].store(1.0f, std::memory_order_relaxed);
        }
    }
    ~LooperEngine() = default;

    // Non-copyable (contains atomics and large buffers)
    LooperEngine(const LooperEngine&) = delete;
    LooperEngine& operator=(const LooperEngine&) = delete;

    // =========================================================================
    // Initialization / teardown (called from non-audio thread)
    // =========================================================================

    /**
     * Allocate all internal buffers and pre-compute lookup tables.
     *
     * MUST be called before the first processAudioBlock() call, typically
     * during AudioEngine::start() after the sample rate is known.
     * NOT real-time safe (allocates memory).
     *
     * @param sampleRate     Device sample rate in Hz (e.g. 48000).
     * @param maxDurationSec Maximum loop duration in seconds (e.g. 120.0).
     */
    void init(float sampleRate, float maxDurationSec) {
        sampleRate_ = sampleRate;
        maxDurationSec_ = maxDurationSec;

        // Pre-allocate loop buffers at maximum capacity
        const int32_t capacity = static_cast<int32_t>(sampleRate * maxDurationSec);
        bufferCapacity_ = capacity;

        loopDryBuffer_.resize(capacity, 0.0f);
        loopWetBuffer_.resize(capacity, 0.0f);
        loopUndoBuffer_.resize(capacity, 0.0f);

        // Pre-allocate layer 0 buffer (base recording) at full capacity.
        // Subsequent layer buffers are allocated lazily when overdub starts.
        layerBuffers_[0].resize(capacity, 0.0f);

        // Pre-allocate re-amp buffer at same capacity
        reampBuffer_.resize(capacity, 0.0f);

        // Pre-allocate crop undo dry buffer at same capacity
        cropUndoDryBuffer_.resize(capacity, 0.0f);

        // Compute crossfade length: 5ms at current sample rate
        const int32_t nominalFade = static_cast<int32_t>(sampleRate * kFadeTimeMs / 1000.0f);
        crossfadeSamples_ = std::max(nominalFade, kMinCrossfadeSamples);

        // Pre-compute Hann crossfade tables
        fadeInTable_.resize(crossfadeSamples_);
        fadeOutTable_.resize(crossfadeSamples_);
        computeCrossfadeTables(crossfadeSamples_);

        // Compute fade-in length for record start (5ms, eliminates click)
        recordFadeInSamples_ = static_cast<int32_t>(sampleRate * kRecordFadeInMs / 1000.0f);
        if (recordFadeInSamples_ < 1) recordFadeInSamples_ = 1;

        // Pre-compute metronome click waveforms
        computeMetronomeClicks();

        // Reset all state
        resetState();
    }

    /**
     * Release all internal buffers and reset state.
     * Called during AudioEngine::stop() or destruction.
     * NOT real-time safe (frees memory).
     */
    void release() {
        // Stop linear recording if active
        if (linearRecordingActive_.load(std::memory_order_relaxed)) {
            stopLinearRecording();
        }

        resetState();

        loopDryBuffer_.clear();
        loopDryBuffer_.shrink_to_fit();
        loopWetBuffer_.clear();
        loopWetBuffer_.shrink_to_fit();
        loopUndoBuffer_.clear();
        loopUndoBuffer_.shrink_to_fit();
        for (int i = 0; i < kMaxLayers; ++i) {
            layerBuffers_[i].clear();
            layerBuffers_[i].shrink_to_fit();
        }
        reampBuffer_.clear();
        reampBuffer_.shrink_to_fit();
        cropUndoDryBuffer_.clear();
        cropUndoDryBuffer_.shrink_to_fit();
        fadeInTable_.clear();
        fadeInTable_.shrink_to_fit();
        fadeOutTable_.clear();
        fadeOutTable_.shrink_to_fit();
        downbeatClick_.clear();
        downbeatClick_.shrink_to_fit();
        beatClick_.clear();
        beatClick_.shrink_to_fit();

        bufferCapacity_ = 0;
        sampleRate_ = 0.0f;
    }

    // =========================================================================
    // Command interface (called from UI/JNI thread)
    // =========================================================================

    /**
     * Send a command to the looper. The command is consumed exactly once
     * by the audio thread on the next processAudioBlock() call.
     *
     * If a previous command has not yet been consumed, it is overwritten.
     * This is acceptable because commands represent instantaneous user actions
     * and the latest intent should always win.
     *
     * @param cmd The command to send.
     */
    void sendCommand(LooperCommand cmd) {
        command_.store(static_cast<int>(cmd), std::memory_order_release);
    }

    /**
     * Convenience: send the TOGGLE command (single-button looper control).
     *
     * Behavior depends on current state:
     *   IDLE        -> start recording (or count-in)
     *   RECORDING   -> stop recording, begin playback
     *   PLAYING     -> start overdubbing
     *   OVERDUBBING -> stop overdubbing, continue playback
     */
    void toggle() { sendCommand(LooperCommand::TOGGLE); }

    // =========================================================================
    // Parameter setters (called from UI/JNI thread, real-time safe reads)
    // =========================================================================

    /** Set loop playback volume (0.0 = silent, 1.0 = unity). */
    void setVolume(float vol) {
        volume_.store(std::clamp(vol, 0.0f, 1.0f), std::memory_order_release);
    }

    /** Get the current loop playback volume. */
    float getVolume() const {
        return volume_.load(std::memory_order_acquire);
    }

    // =========================================================================
    // Per-layer volume control (called from UI/JNI thread)
    // =========================================================================

    /**
     * Set the volume of an individual layer.
     * Layer 0 is the base recording; layers 1+ are overdub passes.
     * Changing a layer volume triggers recomposition of the composite
     * wet buffer so playback, export, and waveform reflect the change.
     *
     * @param layer Layer index (0 to kMaxLayers-1).
     * @param volume Linear gain (0.0 = silent, 1.0 = unity, up to 1.5 = boost).
     */
    void setLayerVolume(int layer, float volume) {
        if (layer < 0 || layer >= kMaxLayers) return;
        const float clamped = std::clamp(volume, 0.0f, kMaxLayerVolume);
        layerVolumes_[layer].store(clamped, std::memory_order_release);

        // Recomposite the wet buffer so playback uses updated volumes.
        // Only safe when not recording/overdubbing (audio thread is writing).
        const auto state = static_cast<LooperState>(
            currentState_.load(std::memory_order_acquire));
        if (state == LooperState::PLAYING || state == LooperState::IDLE) {
            recompositeWetBuffer();
        }
    }

    /**
     * Get the current volume of a layer.
     * @param layer Layer index (0 to kMaxLayers-1).
     * @return Volume in [0.0, 1.5], or 1.0 if out of range.
     */
    float getLayerVolume(int layer) const {
        if (layer < 0 || layer >= kMaxLayers) return 1.0f;
        return layerVolumes_[layer].load(std::memory_order_acquire);
    }

    /**
     * Get the number of recorded layers.
     * Returns 0 when idle (no loop), 1 after base recording, 2+ after overdubs.
     */
    int getLayerCount() const {
        return layerCount_.load(std::memory_order_acquire);
    }

    /** Enable or disable quantized (bar-snapping) mode. */
    void setQuantizedMode(bool enabled) {
        quantizedMode_.store(enabled, std::memory_order_release);
    }

    /** Check if quantized mode is active. */
    bool isQuantizedMode() const {
        return quantizedMode_.load(std::memory_order_acquire);
    }

    /**
     * Set tempo in beats per minute.
     * @param bpm Tempo value, clamped to [30, 300].
     */
    void setBPM(float bpm) {
        bpm_.store(std::clamp(bpm, kMinBPM, kMaxBPM), std::memory_order_release);
    }

    /** Get the current BPM. */
    float getBPM() const {
        return bpm_.load(std::memory_order_acquire);
    }

    /**
     * Set the time signature numerator (beats per bar).
     * @param beats Numerator, clamped to [1, 16].
     */
    void setBeatsPerBar(int beats) {
        beatsPerBar_.store(std::clamp(beats, 1, 16), std::memory_order_release);
    }

    /** Get the current beats-per-bar setting. */
    int getBeatsPerBar() const {
        return beatsPerBar_.load(std::memory_order_acquire);
    }

    /**
     * Set the time signature denominator (beat unit).
     * @param unit Denominator (2, 4, 8, or 16). Invalid values snap to 4.
     */
    void setBeatUnit(int unit) {
        if (unit != 2 && unit != 4 && unit != 8 && unit != 16) unit = 4;
        beatUnit_.store(unit, std::memory_order_release);
    }

    /** Get the current beat unit. */
    int getBeatUnit() const {
        return beatUnit_.load(std::memory_order_acquire);
    }

    /**
     * Set the number of bars for quantized loop length.
     * @param bars Number of bars, clamped to [1, 64].
     */
    void setNumBars(int bars) {
        numBars_.store(std::clamp(bars, 1, 64), std::memory_order_release);
    }

    /** Get the current bar count setting. */
    int getNumBars() const {
        return numBars_.load(std::memory_order_acquire);
    }

    /** Enable or disable count-in (one bar of metronome clicks before recording). */
    void setCountInEnabled(bool enabled) {
        countInEnabled_.store(enabled, std::memory_order_release);
    }

    /** Check if count-in is enabled. */
    bool isCountInEnabled() const {
        return countInEnabled_.load(std::memory_order_acquire);
    }

    /** Set metronome click volume (0.0 = silent, 1.0 = full). */
    void setClickVolume(float vol) {
        clickVolume_.store(std::clamp(vol, 0.0f, 1.0f), std::memory_order_release);
    }

    /** Get the current metronome click volume. */
    float getClickVolume() const {
        return clickVolume_.load(std::memory_order_acquire);
    }

    /** Enable or disable metronome during playback/recording. */
    void setMetronomeActive(bool active) {
        metronomeActive_.store(active, std::memory_order_release);
    }

    /** Check if the metronome is currently active. */
    bool isMetronomeActive() const {
        return metronomeActive_.load(std::memory_order_acquire);
    }

    /**
     * Set the metronome tone type (0-5). Recomputes click waveforms.
     *
     * Tone types:
     *   0 = Classic Click (high woodblock-like sine burst)
     *   1 = Soft Tick (lower, warmer tone)
     *   2 = Rim Shot (noise burst + tone)
     *   3 = Cowbell (dual-frequency metallic)
     *   4 = Hi-Hat (filtered noise burst)
     *   5 = Deep Click (low sine burst)
     */
    void setMetronomeTone(int tone) {
        const int clamped = std::clamp(tone, 0, kMetronomeToneCount - 1);
        metronomeTone_.store(clamped, std::memory_order_release);
        computeMetronomeClicks();
    }

    /** Get the current metronome tone type (0-5). */
    int getMetronomeTone() const {
        return metronomeTone_.load(std::memory_order_acquire);
    }

    // =========================================================================
    // State queries (called from UI/JNI thread for display)
    // =========================================================================

    /** Get the current looper state. */
    LooperState getState() const {
        return static_cast<LooperState>(
            currentState_.load(std::memory_order_acquire));
    }

    /**
     * Get the current playback/record position in samples.
     * Useful for waveform display cursor positioning.
     */
    int32_t getPlaybackPosition() const {
        return playbackPosition_.load(std::memory_order_acquire);
    }

    /** Get the current loop length in samples. Returns 0 if no loop. */
    int32_t getLoopLength() const {
        return loopLength_.load(std::memory_order_acquire);
    }

    /** Get loop length in seconds. Returns 0.0 if no loop or uninitialized. */
    float getLoopLengthSeconds() const {
        if (sampleRate_ <= 0.0f) return 0.0f;
        return static_cast<float>(loopLength_.load(std::memory_order_acquire))
               / sampleRate_;
    }

    /**
     * Get normalized playback position [0.0, 1.0) for UI progress display.
     * Returns 0.0 if no loop is active.
     */
    float getPlaybackPositionNormalized() const {
        const int32_t len = loopLength_.load(std::memory_order_acquire);
        if (len <= 0) return 0.0f;
        return static_cast<float>(playbackPosition_.load(std::memory_order_acquire))
               / static_cast<float>(len);
    }

    /**
     * Get the current beat index within the bar (0-based).
     * Used for metronome beat indicator UI.
     */
    int getCurrentBeat() const {
        return currentBeat_.load(std::memory_order_acquire);
    }

    /** Check if undo is available (valid only in PLAYING state). */
    bool isUndoAvailable() const {
        return undoAvailable_.load(std::memory_order_acquire);
    }

    /** Check if a loop has been recorded (non-zero length). */
    bool hasLoop() const {
        return loopLength_.load(std::memory_order_acquire) > 0;
    }

    // =========================================================================
    // Trim preview (called from UI/JNI thread, real-time safe reads)
    // =========================================================================

    /**
     * Enter trim preview mode. Sets trim pointers to current loop boundaries.
     * While in preview mode, playback is constrained to [trimStart, trimEnd).
     * This is fully reversible via cancelTrimPreview().
     *
     * Must only be called when a loop exists (hasLoop() == true).
     * NOT real-time safe (conceptually a UI action).
     */
    void startTrimPreview() {
        const int32_t len = loopLength_.load(std::memory_order_acquire);
        if (len <= 0) return;

        trimPreviewStart_.store(0, std::memory_order_release);
        trimPreviewEnd_.store(len, std::memory_order_release);
        trimPreviewActive_.store(true, std::memory_order_release);
    }

    /**
     * Exit trim preview mode without committing. Restores full loop playback.
     * The loop data is unchanged; only the playback range reverts.
     */
    void cancelTrimPreview() {
        trimPreviewActive_.store(false, std::memory_order_release);
        trimPreviewStart_.store(0, std::memory_order_release);
        trimPreviewEnd_.store(0, std::memory_order_release);
    }

    /**
     * Set the trim start offset (in samples from loop start).
     * Clamped to [0, trimEnd - 1) to ensure a minimum 1-sample range.
     *
     * @param startSample Desired start position in samples.
     */
    void setTrimStart(int32_t startSample) {
        if (!trimPreviewActive_.load(std::memory_order_acquire)) return;
        const int32_t end = trimPreviewEnd_.load(std::memory_order_acquire);
        // Clamp: must be at least 1 sample before trimEnd
        startSample = std::clamp(startSample, 0, std::max(0, end - 1));
        trimPreviewStart_.store(startSample, std::memory_order_release);
    }

    /**
     * Set the trim end offset (in samples from loop start).
     * Clamped to (trimStart + 1, loopLength] to ensure a minimum 1-sample range.
     *
     * @param endSample Desired end position in samples.
     */
    void setTrimEnd(int32_t endSample) {
        if (!trimPreviewActive_.load(std::memory_order_acquire)) return;
        const int32_t start = trimPreviewStart_.load(std::memory_order_acquire);
        const int32_t len = loopLength_.load(std::memory_order_acquire);
        // Clamp: must be at least 1 sample after trimStart, and within loop
        endSample = std::clamp(endSample, start + 1, std::max(start + 1, len));
        trimPreviewEnd_.store(endSample, std::memory_order_release);
    }

    /** Get current trim start position (in samples). */
    int32_t getTrimStart() const {
        return trimPreviewStart_.load(std::memory_order_acquire);
    }

    /** Get current trim end position (in samples). */
    int32_t getTrimEnd() const {
        return trimPreviewEnd_.load(std::memory_order_acquire);
    }

    /** Check if trim preview is active. */
    bool isTrimPreviewActive() const {
        return trimPreviewActive_.load(std::memory_order_acquire);
    }

    // =========================================================================
    // Trim commit (NOT real-time safe, called from UI/JNI thread)
    // =========================================================================

    /**
     * Commit the current trim: memmove buffer data, update loop length,
     * re-apply Hann crossfade at new boundaries.
     *
     * NOT real-time safe (memmove on potentially large buffers).
     * Invalidates overdub undo (shares loopUndoBuffer_).
     *
     * @return true on success, false if trim preview is not active or
     *         the trim range is invalid.
     */
    bool commitTrim() {
        if (!trimPreviewActive_.load(std::memory_order_acquire)) return false;

        // Guard: do not mutate buffers during recording, finishing, overdub, or count-in
        const auto state = static_cast<LooperState>(currentState_.load(std::memory_order_acquire));
        if (state == LooperState::RECORDING || state == LooperState::FINISHING ||
            state == LooperState::OVERDUBBING || state == LooperState::COUNT_IN) {
            return false;
        }

        const int32_t start = trimPreviewStart_.load(std::memory_order_acquire);
        const int32_t end = trimPreviewEnd_.load(std::memory_order_acquire);
        const int32_t oldLen = loopLengthInternal_;
        const int32_t newLen = end - start;

        if (newLen <= 0 || start < 0 || end > oldLen) return false;

        // Save undo state BEFORE modifying buffers.
        // This shares loopUndoBuffer_ with overdub undo, so overdub undo
        // is invalidated when we store crop undo data here.
        std::memcpy(loopUndoBuffer_.data(), loopWetBuffer_.data(),
                     oldLen * sizeof(float));
        std::memcpy(cropUndoDryBuffer_.data(), loopDryBuffer_.data(),
                     oldLen * sizeof(float));
        preCropLoopLength_ = oldLen;

        // Invalidate overdub undo (loopUndoBuffer_ now holds crop undo data)
        undoAvailable_.store(false, std::memory_order_release);

        // Signal audio thread to output silence during buffer mutation
        bufferMutationInProgress_.store(true, std::memory_order_release);

        // Shift data to start of buffer (memmove handles overlap correctly)
        if (start > 0) {
            std::memmove(loopWetBuffer_.data(),
                         loopWetBuffer_.data() + start,
                         newLen * sizeof(float));
            std::memmove(loopDryBuffer_.data(),
                         loopDryBuffer_.data() + start,
                         newLen * sizeof(float));
            // Shift all active layer buffers to match
            for (int l = 0; l < layerCountInternal_; ++l) {
                if (!layerBuffers_[l].empty() &&
                    static_cast<int32_t>(layerBuffers_[l].size()) >= oldLen) {
                    std::memmove(layerBuffers_[l].data(),
                                 layerBuffers_[l].data() + start,
                                 newLen * sizeof(float));
                }
            }
        }

        // Update loop length
        loopLengthInternal_ = newLen;
        loopLength_.store(newLen, std::memory_order_release);

        // Re-apply Hann crossfade at new boundaries for seamless looping
        loopWritePos_ = newLen;  // applyCrossfadeToRecording() reads loopWritePos_
        applyCrossfadeToRecording();
        loopWritePos_ = 0;      // Reset after crossfade application

        // Reset playback position to start of the trimmed loop
        playPos_ = 0;
        playbackPosition_.store(0, std::memory_order_release);

        // Buffer mutation complete, allow audio thread to resume reading
        bufferMutationInProgress_.store(false, std::memory_order_release);

        // Exit trim preview mode
        trimPreviewActive_.store(false, std::memory_order_release);
        trimPreviewStart_.store(0, std::memory_order_release);
        trimPreviewEnd_.store(0, std::memory_order_release);

        // Mark crop undo as available
        cropUndoAvailable_.store(true, std::memory_order_release);

        return true;
    }

    // =========================================================================
    // Crop undo (NOT real-time safe, called from UI/JNI thread)
    // =========================================================================

    /** Check if crop undo is available (one-level undo for the last crop). */
    bool isCropUndoAvailable() const {
        return cropUndoAvailable_.load(std::memory_order_acquire);
    }

    /**
     * Undo the last crop operation. Restores pre-crop buffer state.
     *
     * NOT real-time safe (memcpy on potentially large buffers).
     * After undo, crop undo is no longer available (one-level only).
     *
     * @return true if the crop was undone, false if no crop undo is available.
     */
    bool undoCrop() {
        if (!cropUndoAvailable_.load(std::memory_order_acquire)) return false;

        // Guard: do not mutate buffers during recording, finishing, overdub, or count-in
        const auto state = static_cast<LooperState>(currentState_.load(std::memory_order_acquire));
        if (state == LooperState::RECORDING || state == LooperState::FINISHING ||
            state == LooperState::OVERDUBBING || state == LooperState::COUNT_IN) {
            return false;
        }

        const int32_t oldLen = preCropLoopLength_;
        if (oldLen <= 0 || oldLen > bufferCapacity_) return false;

        // Signal audio thread to output silence during buffer mutation
        bufferMutationInProgress_.store(true, std::memory_order_release);

        // Restore wet and dry buffers from undo snapshots
        std::memcpy(loopWetBuffer_.data(), loopUndoBuffer_.data(),
                     oldLen * sizeof(float));
        std::memcpy(loopDryBuffer_.data(), cropUndoDryBuffer_.data(),
                     oldLen * sizeof(float));

        // Restore loop length
        loopLengthInternal_ = oldLen;
        loopLength_.store(oldLen, std::memory_order_release);

        // Reset playback position
        playPos_ = 0;
        playbackPosition_.store(0, std::memory_order_release);

        // Buffer mutation complete, allow audio thread to resume reading
        bufferMutationInProgress_.store(false, std::memory_order_release);

        // Consume the crop undo (one-level only)
        cropUndoAvailable_.store(false, std::memory_order_release);

        return true;
    }

    // =========================================================================
    // Waveform summary (NOT real-time safe, called from UI/JNI thread)
    // =========================================================================

    /**
     * Generate a waveform summary for UI display.
     *
     * Computes min/max sample pairs for each column of the display by
     * scanning the wet loop buffer. This is intended for rendering a
     * waveform overview in the trim editor UI.
     *
     * NOT real-time safe (reads entire loop buffer).
     *
     * @param outMinMax   Output array of size numColumns*2.
     *                    Layout: [min0, max0, min1, max1, ...]
     * @param numColumns  Number of display columns (typically screen
     *                    width in pixels / density).
     * @param startSample Start of range to summarize (default 0).
     * @param endSample   End of range to summarize (default -1 = loopLength).
     * @return Number of columns actually filled.
     */
    int32_t getWaveformSummary(float* outMinMax, int32_t numColumns,
                               int32_t startSample = 0,
                               int32_t endSample = -1) const {
        const int32_t len = loopLength_.load(std::memory_order_acquire);
        if (len <= 0 || numColumns <= 0 || !outMinMax) return 0;

        if (startSample < 0) startSample = 0;
        if (endSample < 0 || endSample > len) endSample = len;
        const int32_t rangeLen = endSample - startSample;
        if (rangeLen <= 0) return 0;

        const float samplesPerCol = static_cast<float>(rangeLen)
                                    / static_cast<float>(numColumns);

        for (int32_t col = 0; col < numColumns; ++col) {
            const int32_t colStart = startSample
                + static_cast<int32_t>(col * samplesPerCol);
            const int32_t colEnd = startSample
                + static_cast<int32_t>((col + 1) * samplesPerCol);
            const int32_t safeEnd = std::min(colEnd, endSample);

            float minVal = 1.0f;
            float maxVal = -1.0f;

            for (int32_t s = colStart; s < safeEnd; ++s) {
                const float sample = loopWetBuffer_[s];
                if (sample < minVal) minVal = sample;
                if (sample > maxVal) maxVal = sample;
            }

            // Handle empty column (defensive guard)
            if (colStart >= safeEnd) {
                minVal = 0.0f;
                maxVal = 0.0f;
            }

            outMinMax[col * 2]     = minVal;
            outMinMax[col * 2 + 1] = maxVal;
        }

        return numColumns;
    }

    // =========================================================================
    // Zero-crossing snap (NOT real-time safe, called from UI/JNI thread)
    // =========================================================================

    /**
     * Find the nearest zero crossing to the given sample position.
     * Searches +/- searchRadius samples around the target for a sign change.
     *
     * When a zero crossing is found, returns the sample closer to zero
     * magnitude (between the two adjacent samples that straddle zero).
     *
     * @param targetSample The sample position to snap.
     * @param searchRadius Maximum search distance in samples.
     *                     Default -1 means ~1ms (sampleRate/1000).
     * @return Nearest zero-crossing position, or targetSample if none found.
     */
    int32_t findNearestZeroCrossing(int32_t targetSample,
                                    int32_t searchRadius = -1) const {
        const int32_t len = loopLength_.load(std::memory_order_acquire);
        if (len <= 0) return targetSample;

        if (searchRadius < 0) {
            searchRadius = static_cast<int32_t>(sampleRate_ / 1000.0f); // ~1ms
        }
        searchRadius = std::max(searchRadius, 1);

        const int32_t searchStart = std::max(0, targetSample - searchRadius);
        const int32_t searchEnd = std::min(len - 1,
                                           targetSample + searchRadius);

        int32_t bestPos = targetSample;
        int32_t bestDist = searchRadius + 1;

        for (int32_t i = searchStart; i < searchEnd; ++i) {
            // Zero crossing: sign change between adjacent samples
            if ((loopWetBuffer_[i] >= 0.0f && loopWetBuffer_[i + 1] < 0.0f) ||
                (loopWetBuffer_[i] < 0.0f && loopWetBuffer_[i + 1] >= 0.0f)) {
                // Choose the sample closer to zero magnitude
                const int32_t crossPos =
                    (std::abs(loopWetBuffer_[i]) <= std::abs(loopWetBuffer_[i + 1]))
                    ? i : i + 1;
                const int32_t dist = std::abs(crossPos - targetSample);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestPos = crossPos;
                }
            }
        }

        return bestPos;
    }

    // =========================================================================
    // Beat snap (NOT real-time safe, called from UI/JNI thread)
    // =========================================================================

    /**
     * Snap a sample position to the nearest beat boundary.
     * Only meaningful when BPM is set (> 0). Uses double precision
     * to prevent rounding drift at high sample rates.
     *
     * @param targetSample The sample position to snap.
     * @return Nearest beat boundary position, clamped to [0, loopLength].
     */
    int32_t snapToBeat(int32_t targetSample) const {
        const float bpm = bpm_.load(std::memory_order_relaxed);
        if (bpm <= 0.0f || sampleRate_ <= 0.0f) return targetSample;

        const double samplesPerBeat = (60.0 / static_cast<double>(bpm))
                                      * static_cast<double>(sampleRate_);
        if (samplesPerBeat <= 0.0) return targetSample;

        const double beatIndex = static_cast<double>(targetSample)
                                 / samplesPerBeat;
        const int32_t nearestBeat = static_cast<int32_t>(std::round(beatIndex));

        int32_t snapped = static_cast<int32_t>(
            std::round(nearestBeat * samplesPerBeat));
        return std::clamp(snapped, 0, loopLength_.load(std::memory_order_acquire));
    }

    // =========================================================================
    // Tap tempo (called from UI/JNI thread)
    // =========================================================================

    /**
     * Register a tap for tap-tempo BPM detection.
     *
     * Uses a trimmed-mean algorithm with 35% outlier rejection over the
     * last 8 taps. Resets if more than 3 seconds elapse between taps.
     *
     * @param timestampMs  Monotonic timestamp in milliseconds (e.g. from
     *                     System.nanoTime()/1000000 on the Kotlin side).
     */
    void onTap(int64_t timestampMs) {
        // Reset if too much time elapsed since last tap
        if (tapCount_ > 0 && (timestampMs - tapTimestamps_[tapWriteIdx_ == 0
                ? kMaxTapHistory - 1 : tapWriteIdx_ - 1]) > kTapTimeoutMs) {
            tapCount_ = 0;
            tapWriteIdx_ = 0;
        }

        // Store this tap
        tapTimestamps_[tapWriteIdx_] = timestampMs;
        tapWriteIdx_ = (tapWriteIdx_ + 1) % kMaxTapHistory;
        if (tapCount_ < kMaxTapHistory) tapCount_++;

        // Need at least 2 taps to compute an interval
        if (tapCount_ < 2) return;

        // Collect intervals from the stored taps
        const int numIntervals = tapCount_ - 1;
        int64_t intervals[kMaxTapHistory - 1];

        for (int i = 0; i < numIntervals; ++i) {
            // Walk backwards from the most recent tap
            int idx = (tapWriteIdx_ - 1 - i + kMaxTapHistory) % kMaxTapHistory;
            int prevIdx = (idx - 1 + kMaxTapHistory) % kMaxTapHistory;
            intervals[i] = tapTimestamps_[idx] - tapTimestamps_[prevIdx];
        }

        // Sort intervals for outlier rejection
        std::sort(intervals, intervals + numIntervals);

        // Trimmed mean: reject top and bottom 35% (by count)
        const int trimCount = static_cast<int>(numIntervals * kTapTrimFraction);
        const int startIdx = trimCount;
        const int endIdx = numIntervals - trimCount;

        // Must have at least one interval after trimming
        if (endIdx <= startIdx) {
            // Not enough intervals after trimming; use raw mean
            int64_t sum = 0;
            for (int i = 0; i < numIntervals; ++i) sum += intervals[i];
            const double avgMs = static_cast<double>(sum) / numIntervals;
            const float detectedBpm = static_cast<float>(60000.0 / avgMs);
            tapBPM_ = std::clamp(detectedBpm, kMinBPM, kMaxBPM);
        } else {
            int64_t sum = 0;
            for (int i = startIdx; i < endIdx; ++i) sum += intervals[i];
            const double avgMs = static_cast<double>(sum) / (endIdx - startIdx);
            const float detectedBpm = static_cast<float>(60000.0 / avgMs);
            tapBPM_ = std::clamp(detectedBpm, kMinBPM, kMaxBPM);
        }

        // Automatically apply detected BPM to the looper
        setBPM(tapBPM_);
    }

    /**
     * Get the most recently detected tap-tempo BPM.
     * Returns 0.0 if fewer than 2 taps have been registered.
     */
    float getTapBPM() const { return tapBPM_; }

    /** Reset the tap tempo history. */
    void resetTapTempo() {
        tapCount_ = 0;
        tapWriteIdx_ = 0;
        tapBPM_ = 0.0f;
    }

    // =========================================================================
    // Re-amp support (stub for Sprint 25)
    // =========================================================================

    /**
     * Load dry audio data into the re-amp buffer for playback through
     * the effects chain. NOT real-time safe (copies data).
     *
     * @param dryData    Pointer to mono float samples.
     * @param numSamples Number of samples to load.
     * @return true if data was loaded successfully (within capacity).
     */
    bool loadForReamp(const float* dryData, int32_t numSamples) {
        if (!dryData || numSamples <= 0 || numSamples > bufferCapacity_) {
            return false;
        }
        std::memcpy(reampBuffer_.data(), dryData, numSamples * sizeof(float));
        reampLength_ = numSamples;
        reampReadPos_ = 0;
        return true;
    }

    /** Start re-amp playback. */
    void startReamp() {
        if (reampLength_ > 0) {
            reampReadPos_ = 0;
            reampState_.store(static_cast<int>(ReampState::PLAYING),
                              std::memory_order_release);
        }
    }

    /** Stop re-amp playback. */
    void stopReamp() {
        reampState_.store(static_cast<int>(ReampState::IDLE),
                          std::memory_order_release);
    }

    /** Check if re-amp playback is active. */
    bool isReamping() const {
        return reampState_.load(std::memory_order_acquire)
               == static_cast<int>(ReampState::PLAYING);
    }

    /**
     * Fill output buffer with re-amp dry samples for processing through
     * the effects chain. Real-time safe (no allocations).
     *
     * @param outputBuffer Buffer to fill with dry samples.
     * @param numFrames    Number of frames to fill.
     * @return Number of frames actually written (0 if not re-amping or exhausted).
     */
    int32_t getReampSamples(float* outputBuffer, int numFrames) {
        if (reampState_.load(std::memory_order_relaxed)
            != static_cast<int>(ReampState::PLAYING)) {
            return 0;
        }

        const int32_t remaining = reampLength_ - reampReadPos_;
        if (remaining <= 0) {
            // Re-amp playback complete
            reampState_.store(static_cast<int>(ReampState::IDLE),
                              std::memory_order_release);
            return 0;
        }

        const int32_t framesToCopy = std::min(numFrames, remaining);
        std::memcpy(outputBuffer, reampBuffer_.data() + reampReadPos_,
                    framesToCopy * sizeof(float));
        reampReadPos_ += framesToCopy;

        // Zero-fill any remaining frames
        if (framesToCopy < numFrames) {
            std::memset(outputBuffer + framesToCopy, 0,
                        (numFrames - framesToCopy) * sizeof(float));
            reampState_.store(static_cast<int>(ReampState::IDLE),
                              std::memory_order_release);
        }

        return framesToCopy;
    }

    // =========================================================================
    // Linear recording support (stub for Sprint 24e)
    // =========================================================================

    /** Enable or disable writing to the SPSC ring buffer (for WAV export). */
    void setRecordToRing(bool enabled) {
        recordToRing_.store(enabled, std::memory_order_release);
    }

    /** Check if ring buffer recording is active. */
    bool isRecordingToRing() const {
        return recordToRing_.load(std::memory_order_acquire);
    }

    // =========================================================================
    // Loop buffer export (called from JNI thread, NOT real-time safe)
    // =========================================================================

    /**
     * Export the current loop buffers to WAV files on disk.
     *
     * Writes both dry and wet loop buffers as 32-bit float mono WAV files.
     * This is a synchronous file I/O operation and must NOT be called from
     * the audio thread.
     *
     * @param dryPath  Absolute path for the dry WAV output file.
     * @param wetPath  Absolute path for the wet WAV output file.
     * @return true if both files were written successfully.
     */
    bool exportLoop(const std::string& dryPath, const std::string& wetPath) {
        const int32_t len = loopLength_.load(std::memory_order_acquire);
        if (len <= 0 || sampleRate_ <= 0.0f) return false;

        const int32_t sr = static_cast<int32_t>(sampleRate_);

        // Write dry WAV
        {
            WavWriter writer;
            if (!writer.open(dryPath, sr, 1)) return false;
            if (!writer.writeSamples(loopDryBuffer_.data(), len)) {
                writer.close();
                return false;
            }
            writer.close();
        }

        // Write wet WAV
        {
            WavWriter writer;
            if (!writer.open(wetPath, sr, 1)) return false;
            if (!writer.writeSamples(loopWetBuffer_.data(), len)) {
                writer.close();
                return false;
            }
            writer.close();
        }

        return true;
    }

    /**
     * Load a mono 32-bit float WAV file into the re-amp buffer.
     *
     * Reads the WAV file, validates the format (must be 32-bit float mono),
     * and copies sample data into the internal reampBuffer_. This is the
     * primary method for loading saved session dry recordings back for
     * re-processing through the current effects chain.
     *
     * NOT real-time safe (reads from disk).
     *
     * @param path Absolute path to the mono float32 WAV file.
     * @return true if the file was read and loaded successfully.
     */
    bool loadWavForReamp(const std::string& path) {
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) return false;

        // Read and validate WAV header
        uint8_t header[44];
        if (std::fread(header, 1, 44, f) != 44) {
            std::fclose(f);
            return false;
        }

        // Verify RIFF/WAVE magic
        if (std::memcmp(header, "RIFF", 4) != 0 ||
            std::memcmp(header + 8, "WAVE", 4) != 0) {
            std::fclose(f);
            return false;
        }

        // Read format tag (offset 20) - must be 3 (IEEE float)
        uint16_t formatTag;
        std::memcpy(&formatTag, header + 20, 2);
        if (formatTag != 3) {
            std::fclose(f);
            return false;
        }

        // Read number of channels (offset 22) - must be 1 (mono)
        uint16_t channels;
        std::memcpy(&channels, header + 22, 2);
        if (channels != 1) {
            std::fclose(f);
            return false;
        }

        // Read data chunk size (offset 40)
        uint32_t dataSize;
        std::memcpy(&dataSize, header + 40, 4);

        // Handle placeholder size from crash-resilient header
        if (dataSize == 0xFFFFFFFFu) {
            // Infer from file size
            std::fseek(f, 0, SEEK_END);
            long fileSize = std::ftell(f);
            std::fseek(f, 44, SEEK_SET);
            dataSize = static_cast<uint32_t>(fileSize - 44);
        }

        const int32_t numSamples = static_cast<int32_t>(dataSize / sizeof(float));
        if (numSamples <= 0 || numSamples > bufferCapacity_) {
            std::fclose(f);
            return false;
        }

        // Read sample data directly into reamp buffer
        const size_t read = std::fread(reampBuffer_.data(), sizeof(float),
                                       static_cast<size_t>(numSamples), f);
        std::fclose(f);

        if (static_cast<int32_t>(read) != numSamples) {
            return false;
        }

        reampLength_ = numSamples;
        reampReadPos_ = 0;
        return true;
    }

    /**
     * Start linear recording: capture the live performance to disk.
     *
     * Creates SPSC ring buffers and WavWriter instances, then spawns a
     * writer thread that drains the ring buffers to WAV files. The audio
     * thread writes into the ring buffers from processAudioBlock().
     *
     * NOT real-time safe (allocates memory, opens files, spawns thread).
     *
     * @param dryPath  Path for the dry WAV output file.
     * @param wetPath  Path for the wet WAV output file.
     * @return true if recording started successfully.
     */
    bool startLinearRecording(const std::string& dryPath,
                              const std::string& wetPath) {
        if (linearRecordingActive_.load(std::memory_order_acquire)) {
            return false; // Already recording
        }

        const int32_t sr = static_cast<int32_t>(sampleRate_);
        if (sr <= 0) return false;

        // Allocate ring buffers (~2s capacity at current sample rate)
        const int32_t ringCapacity = sr * 2;
        dryRing_ = std::make_unique<SPSCRingBuffer<float>>(ringCapacity);
        wetRing_ = std::make_unique<SPSCRingBuffer<float>>(ringCapacity);

        // Open WAV files
        dryWriter_ = std::make_unique<WavWriter>();
        wetWriter_ = std::make_unique<WavWriter>();

        if (!dryWriter_->open(dryPath, sr, 1)) return false;
        if (!wetWriter_->open(wetPath, sr, 1)) {
            dryWriter_->close();
            return false;
        }

        // Enable ring buffer writing (audio thread reads this flag)
        writerShouldRun_.store(true, std::memory_order_release);
        linearRecordingActive_.store(true, std::memory_order_release);
        recordToRing_.store(true, std::memory_order_release);

        // Spawn writer thread
        writerThread_ = std::thread([this]() { writerThreadFunc(); });

        return true;
    }

    /**
     * Stop linear recording: disable ring writes, drain remaining data,
     * finalize WAV files, and join the writer thread.
     *
     * NOT real-time safe (joins thread, closes files).
     *
     * @return true if recording was active and stopped successfully.
     */
    bool stopLinearRecording() {
        if (!linearRecordingActive_.load(std::memory_order_acquire)) {
            return false;
        }

        // Signal the audio thread to stop writing to the ring
        recordToRing_.store(false, std::memory_order_release);

        // Signal the writer thread to stop and join it
        writerShouldRun_.store(false, std::memory_order_release);
        if (writerThread_.joinable()) {
            writerThread_.join();
        }

        // Close WAV files (patches headers with correct sizes)
        if (dryWriter_) dryWriter_->close();
        if (wetWriter_) wetWriter_->close();

        // Release resources
        dryRing_.reset();
        wetRing_.reset();
        dryWriter_.reset();
        wetWriter_.reset();

        linearRecordingActive_.store(false, std::memory_order_release);
        return true;
    }

    /** Check if linear recording is active. */
    bool isLinearRecordingActive() const {
        return linearRecordingActive_.load(std::memory_order_acquire);
    }

    /**
     * Write dry and wet samples to the ring buffers (called from audio thread).
     *
     * REAL-TIME SAFE: No allocations, no locks, no system calls.
     * If the ring buffers are full, samples are silently dropped.
     *
     * @param dryData  Dry (pre-effects) samples.
     * @param wetData  Wet (post-effects) samples.
     * @param numFrames Number of samples to write.
     */
    void writeToRingBuffers(const float* dryData, const float* wetData,
                            int numFrames) {
        if (!recordToRing_.load(std::memory_order_relaxed)) return;
        if (dryRing_) dryRing_->write(dryData, numFrames);
        if (wetRing_) wetRing_->write(wetData, numFrames);
    }

    // =========================================================================
    // Core audio processing (called ONLY from audio thread)
    // =========================================================================

    /**
     * Process one audio block through the looper engine.
     *
     * Called from AudioEngine::onAudioReady() AFTER signal chain processing.
     * The outputBuffer already contains the live processed signal. This method
     * ADDS loop playback (and metronome clicks) to it.
     *
     * REAL-TIME SAFE: No allocations, no locks, no system calls, no logging.
     *
     * @param dryInput     Pre-effects mono input (for dry layer recording).
     * @param wetInput     Post-effects mono input (for wet layer recording).
     *                     Typically the same pointer as outputBuffer.
     * @param outputBuffer Mono output buffer (live signal in, live+loop out).
     * @param numFrames    Number of frames in this audio block.
     */
    void processAudioBlock(const float* dryInput, const float* wetInput,
                           float* outputBuffer, int numFrames) {
        // ---- Consume any pending command (exactly once via exchange) ----
        const auto cmd = static_cast<LooperCommand>(
            command_.exchange(static_cast<int>(LooperCommand::NONE),
                              std::memory_order_acquire));

        // ---- Snapshot state and parameters for this block ----
        auto state = static_cast<LooperState>(
            currentState_.load(std::memory_order_relaxed));
        const float volume = volume_.load(std::memory_order_relaxed);
        const bool quantized = quantizedMode_.load(std::memory_order_relaxed);
        const bool metroActive = metronomeActive_.load(std::memory_order_relaxed);

        // ---- Handle command / state transitions ----
        state = handleCommand(cmd, state, quantized);

        // ---- Per-state processing ----
        switch (state) {
            case LooperState::IDLE:
                // Pass-through: nothing to do
                break;

            case LooperState::COUNT_IN:
                processCountIn(dryInput, wetInput, outputBuffer, numFrames, quantized);
                // Check if count-in is complete (transition handled inside)
                state = static_cast<LooperState>(
                    currentState_.load(std::memory_order_relaxed));
                break;

            case LooperState::RECORDING:
                processRecording(dryInput, wetInput, outputBuffer, numFrames, quantized);
                // Check for auto-stop in quantized mode
                state = static_cast<LooperState>(
                    currentState_.load(std::memory_order_relaxed));
                break;

            case LooperState::PLAYING:
                processPlayback(outputBuffer, numFrames, volume);
                break;

            case LooperState::OVERDUBBING:
                processOverdub(dryInput, wetInput, outputBuffer, numFrames, volume);
                break;

            case LooperState::FINISHING:
                processFinishing(dryInput, wetInput, outputBuffer, numFrames,
                                 volume);
                // May transition to PLAYING
                state = static_cast<LooperState>(
                    currentState_.load(std::memory_order_relaxed));
                break;
        }

        // ---- Metronome clicks (when active and in a relevant state) ----
        if (metroActive && state != LooperState::IDLE) {
            generateMetronomeClick(outputBuffer, numFrames);
        }

        // ---- Linear recording: write to SPSC ring buffers ----
        if (recordToRing_.load(std::memory_order_relaxed)) {
            writeToRingBuffers(dryInput, wetInput, numFrames);
        }

        // ---- Publish position for UI ----
        playbackPosition_.store(playPos_, std::memory_order_release);
    }

private:
    // =========================================================================
    // Constants
    // =========================================================================

    static constexpr float kFadeTimeMs       = 5.0f;   ///< Crossfade / fade-in duration
    static constexpr float kRecordFadeInMs    = 5.0f;   ///< Fade-in on record start
    static constexpr int32_t kMinCrossfadeSamples = 48; ///< Floor: ~1ms at 48kHz
    static constexpr float kMaxFadeRatio     = 0.02f;   ///< Crossfade capped at 2% of loop

    /// Maximum number of independently-controllable layers (base + overdubs).
    /// Layer 0 is the base recording; layers 1..kMaxLayers-1 are overdub passes.
    /// Buffers are allocated lazily (only when an overdub starts on that layer).
    static constexpr int kMaxLayers          = 8;

    /// Maximum per-layer volume (allows slight boost above unity).
    static constexpr float kMaxLayerVolume   = 1.5f;

    static constexpr float kMinBPM           = 30.0f;
    static constexpr float kMaxBPM           = 300.0f;
    static constexpr float kDefaultBPM       = 120.0f;

    // Metronome click parameters
    static constexpr float kDownbeatDurationMs = 10.0f;  ///< Downbeat click length
    static constexpr float kBeatDurationMs   = 8.0f;     ///< Beat click length
    static constexpr float kDownbeatAmplitude = 0.3f;     ///< Downbeat peak amplitude
    static constexpr float kBeatAmplitude    = 0.2f;      ///< Beat peak amplitude
    static constexpr int   kMetronomeToneCount = 6;       ///< Number of available tone types

    // Tap tempo
    static constexpr int kMaxTapHistory      = 8;       ///< Taps to keep for averaging
    static constexpr int64_t kTapTimeoutMs   = 3000;    ///< Reset after 3s gap
    static constexpr float kTapTrimFraction  = 0.35f;   ///< 35% outlier rejection

    // Quantization snapping threshold: if early stop is within 50% of a bar,
    // snap to current bar boundary; otherwise snap to the next bar
    static constexpr double kSnapThreshold   = 0.5;

    // =========================================================================
    // State machine command handler
    // =========================================================================

    /**
     * Process a command and return the new state.
     * Called once per audio block with the consumed command.
     *
     * @param cmd       The command to process (NONE = no-op).
     * @param state     Current looper state.
     * @param quantized Whether quantized mode is active.
     * @return New state after command processing.
     */
    LooperState handleCommand(LooperCommand cmd, LooperState state, bool quantized) {
        if (cmd == LooperCommand::NONE) return state;

        // ---- CLEAR: always returns to IDLE ----
        if (cmd == LooperCommand::CLEAR) {
            clearLoop();
            return setState(LooperState::IDLE);
        }

        // ---- TOGGLE: context-dependent single-button ----
        if (cmd == LooperCommand::TOGGLE) {
            switch (state) {
                case LooperState::IDLE:
                    // If a loop exists, resume playback instead of re-recording.
                    // This is critical for play/pause behavior: stop keeps the loop,
                    // and the next toggle should PLAY it, not clear and re-record.
                    if (loopLengthInternal_ > 0) {
                        playPos_ = 0;
                        return setState(LooperState::PLAYING);
                    }
                    cmd = LooperCommand::RECORD;
                    break;
                case LooperState::RECORDING:
                case LooperState::FINISHING:
                    cmd = LooperCommand::STOP;
                    break;
                case LooperState::PLAYING:
                    cmd = LooperCommand::OVERDUB;
                    break;
                case LooperState::OVERDUBBING:
                    cmd = LooperCommand::STOP;
                    break;
                case LooperState::COUNT_IN:
                    // Cancel count-in, go back to IDLE
                    return setState(LooperState::IDLE);
            }
        }

        // ---- Process resolved command ----
        switch (cmd) {
            case LooperCommand::RECORD:
                return handleRecord(state, quantized);

            case LooperCommand::STOP:
                return handleStop(state, quantized);

            case LooperCommand::OVERDUB:
                return handleOverdub(state);

            case LooperCommand::UNDO:
                return handleUndo(state);

            default:
                return state;
        }
    }

    /**
     * Handle RECORD command.
     * IDLE -> clear and start recording (or count-in).
     * PLAYING -> clear and start re-recording.
     */
    LooperState handleRecord(LooperState state, bool quantized) {
        if (state != LooperState::IDLE && state != LooperState::PLAYING) {
            return state; // Ignore in other states
        }

        clearLoop();

        // Snapshot quantized loop length at recording start so BPM changes
        // mid-recording don't shift the target boundary unpredictably.
        quantizedLengthSnapshot_ = quantized ? computeQuantizedLoopLength() : bufferCapacity_;

        // If quantized mode with count-in, go to COUNT_IN first
        if (quantized && countInEnabled_.load(std::memory_order_relaxed)) {
            startCountIn();
            return setState(LooperState::COUNT_IN);
        }

        startRecording();
        return setState(LooperState::RECORDING);
    }

    /**
     * Handle STOP command.
     * RECORDING -> finalize and play (or enter FINISHING if quantized).
     * OVERDUBBING -> stop overdub, continue playback.
     * PLAYING -> stop playback (keep loop), go to IDLE-like playing stop.
     */
    LooperState handleStop(LooperState state, bool quantized) {
        switch (state) {
            case LooperState::RECORDING: {
                if (loopWritePos_ < kMinCrossfadeSamples * 2) {
                    // Too short to be useful; discard
                    clearLoop();
                    return setState(LooperState::IDLE);
                }

                if (quantized) {
                    // Enter FINISHING: continue recording until quantized boundary
                    finishingTargetLength_ = computeQuantizedStopLength(loopWritePos_);
                    if (finishingTargetLength_ <= loopWritePos_) {
                        // Already past the target, finalize now
                        finalizeRecording();
                        return setState(LooperState::PLAYING);
                    }
                    return setState(LooperState::FINISHING);
                }

                // Non-quantized: finalize immediately
                finalizeRecording();
                return setState(LooperState::PLAYING);
            }

            case LooperState::FINISHING: {
                // Force-stop finishing: finalize at current position
                finalizeRecording();
                return setState(LooperState::PLAYING);
            }

            case LooperState::OVERDUBBING: {
                // Finalize the layer: increment count if not at max
                if (layerCountInternal_ < kMaxLayers) {
                    layerCountInternal_++;
                    layerCount_.store(layerCountInternal_, std::memory_order_release);
                }
                // Stop overdubbing, continue playback
                return setState(LooperState::PLAYING);
            }

            case LooperState::PLAYING: {
                // Stop playback, keep the loop intact for resuming later
                playPos_ = 0;
                return setState(LooperState::IDLE);
            }

            default:
                return state;
        }
    }

    /**
     * Handle OVERDUB command.
     * PLAYING -> start overdubbing (save undo snapshot incrementally).
     */
    LooperState handleOverdub(LooperState state) {
        if (state != LooperState::PLAYING) return state;

        // Prepare undo: mark that we need to snapshot the wet buffer
        // as we overwrite it (incremental copy in processOverdub)
        undoCopyDone_ = false;
        overdubStartPos_ = playPos_;
        undoAvailable_.store(false, std::memory_order_release);

        // Prepare the next layer buffer for this overdub pass.
        // If at max layers, reuse the last slot (audio still accumulates
        // into loopWetBuffer_ correctly, just loses independent volume).
        const int nextLayer = std::min(layerCountInternal_, kMaxLayers - 1);
        if (static_cast<int32_t>(layerBuffers_[nextLayer].size()) < loopLengthInternal_) {
            // Lazy allocation -- NOT real-time safe, but acceptable here
            // because overdub start is a user-triggered event (not per-sample).
            layerBuffers_[nextLayer].resize(loopLengthInternal_, 0.0f);
        } else {
            // Buffer exists; zero only the used portion
            std::memset(layerBuffers_[nextLayer].data(), 0,
                        loopLengthInternal_ * sizeof(float));
        }

        return setState(LooperState::OVERDUBBING);
    }

    /**
     * Handle UNDO command.
     * PLAYING -> swap wet buffer with undo buffer.
     */
    LooperState handleUndo(LooperState state) {
        if (state != LooperState::PLAYING) return state;
        if (!undoAvailable_.load(std::memory_order_relaxed)) return state;

        // Swap wet and undo data pointers
        // std::vector::swap is O(1) pointer swap, real-time safe
        loopWetBuffer_.swap(loopUndoBuffer_);
        undoAvailable_.store(false, std::memory_order_release);

        // Decrement layer count (the undone overdub layer data is still in
        // the buffer but no longer considered active for volume control).
        if (layerCountInternal_ > 1) {
            layerCountInternal_--;
            layerCount_.store(layerCountInternal_, std::memory_order_release);
        }

        // Invalidate crop undo: the swap moved loopUndoBuffer_ contents
        // (which held the pre-crop wet snapshot) into loopWetBuffer_, so
        // the crop undo data is no longer valid in loopUndoBuffer_.
        cropUndoAvailable_.store(false, std::memory_order_release);

        return state;
    }

    // =========================================================================
    // Per-state processing methods (audio thread only)
    // =========================================================================

    /**
     * Process COUNT_IN state: play metronome clicks for one bar, then
     * transition to RECORDING.
     *
     * @param dryInput     Pre-effects mono input (forwarded to processRecording
     *                     if count-in completes mid-block).
     * @param wetInput     Post-effects mono input (forwarded to processRecording).
     * @param outputBuffer Mono output buffer.
     * @param numFrames    Number of frames in this audio block.
     * @param quantized    Whether quantized mode is active.
     */
    void processCountIn(const float* dryInput, const float* wetInput,
                        float* outputBuffer, int numFrames, bool quantized) {
        const float bpm = bpm_.load(std::memory_order_relaxed);
        const int beatsPerBar = beatsPerBar_.load(std::memory_order_relaxed);
        const double samplesPerBeat = (60.0 / static_cast<double>(bpm)) * sampleRate_;

        for (int i = 0; i < numFrames; ++i) {
            // Check if we've crossed a beat boundary
            if (beatSampleCounter_ >= static_cast<int64_t>(samplesPerBeat)) {
                beatSampleCounter_ = 0;
                currentBeatInternal_++;

                if (currentBeatInternal_ >= beatsPerBar) {
                    // Count-in complete: transition to RECORDING
                    currentBeatInternal_ = 0;
                    currentBeat_.store(0, std::memory_order_release);
                    startRecording();
                    setState(LooperState::RECORDING);

                    // Process remaining frames in this block as recording
                    const int remaining = numFrames - (i + 1);
                    if (remaining > 0) {
                        processRecording(dryInput + (i + 1), wetInput + (i + 1),
                                         outputBuffer + (i + 1), remaining, quantized);
                    }
                    return;
                }
            }

            // Generate click at beat boundary (first sample of each beat)
            if (beatSampleCounter_ == 0) {
                currentBeat_.store(currentBeatInternal_, std::memory_order_release);
            }

            beatSampleCounter_++;
        }
    }

    /**
     * Process RECORDING state: write dry and wet input to loop buffers.
     * Applies a linear fade-in on the first samples to eliminate click.
     * In quantized mode, auto-stops at the computed loop length.
     */
    void processRecording(const float* dryInput, const float* wetInput,
                          float* outputBuffer, int numFrames, bool quantized) {
        const int32_t capacity = bufferCapacity_;
        const int32_t quantizedLen = quantized
            ? quantizedLengthSnapshot_
            : capacity;
        const int32_t maxLen = std::min(capacity, quantizedLen);
        const float volume = volume_.load(std::memory_order_relaxed);

        for (int i = 0; i < numFrames; ++i) {
            if (loopWritePos_ >= maxLen) {
                // Reached capacity or quantized boundary: auto-stop
                finalizeRecording();
                setState(LooperState::PLAYING);

                // Process remaining frames in this block as playback
                const int remaining = numFrames - i;
                if (remaining > 0) {
                    processPlayback(outputBuffer + i, remaining, volume);
                }
                return;
            }

            float dry = dryInput[i];
            float wet = wetInput[i];

            // Apply fade-in on the first recordFadeInSamples_ samples
            if (loopWritePos_ < recordFadeInSamples_) {
                const float fadeGain = static_cast<float>(loopWritePos_)
                                       / static_cast<float>(recordFadeInSamples_);
                dry *= fadeGain;
                wet *= fadeGain;
            }

            loopDryBuffer_[loopWritePos_] = dry;
            loopWetBuffer_[loopWritePos_] = wet;
            // Store in layer 0 (base recording) for per-layer volume control
            if (!layerBuffers_[0].empty()) {
                layerBuffers_[0][loopWritePos_] = wet;
            }
            loopWritePos_++;
        }

        // Update beat tracking during recording (for metronome sync)
        updateBeatTracking(numFrames);
    }

    /**
     * Process PLAYING state: read loop wet buffer and add to output.
     * Applies crossfade at the loop boundary.
     *
     * When trim preview is active, playback is constrained to the range
     * [trimPreviewStart_, trimPreviewEnd_) instead of the full loop.
     */
    void processPlayback(float* outputBuffer, int numFrames, float volume) {
        // If a buffer mutation (crop/uncrop) is in progress on the UI thread,
        // output silence for this block (~5ms) to avoid reading inconsistent data.
        if (bufferMutationInProgress_.load(std::memory_order_acquire)) return;

        const int32_t len = loopLengthInternal_;
        if (len <= 0) return;

        // --- Trim preview: constrain playback to [effectiveStart, effectiveEnd) ---
        const bool trimming = trimPreviewActive_.load(std::memory_order_relaxed);
        const int32_t effectiveStart = trimming
            ? trimPreviewStart_.load(std::memory_order_relaxed) : 0;
        const int32_t effectiveEnd = trimming
            ? trimPreviewEnd_.load(std::memory_order_relaxed) : len;
        const int32_t effectiveLen = effectiveEnd - effectiveStart;
        if (effectiveLen <= 0) return;

        const int32_t fadeLen = getEffectiveFadeLength(effectiveLen);

        // Clamp playPos_ to the effective range if it has drifted outside
        if (playPos_ < effectiveStart || playPos_ >= effectiveEnd) {
            playPos_ = effectiveStart;
        }

        for (int i = 0; i < numFrames; ++i) {
            float sample = loopWetBuffer_[playPos_];

            // Apply crossfade near loop boundary (relative to effective range)
            const int32_t relativePos = playPos_ - effectiveStart;
            sample = applyCrossfadeForRange(sample, relativePos, effectiveLen,
                                            fadeLen, effectiveStart);

            // Apply denormal guard to prevent denormal accumulation in long loops
            sample = fast_math::denormal_guard(sample);

            outputBuffer[i] += sample * volume;

            playPos_++;
            if (playPos_ >= effectiveEnd) {
                playPos_ = effectiveStart;
            }
        }

        // Update beat tracking for metronome sync
        updateBeatTracking(numFrames);
    }

    /**
     * Process OVERDUBBING state: play back existing loop AND sum new input.
     * Copies wet buffer to undo buffer incrementally (first pass only).
     *
     * When trim preview is active, overdub is constrained to the range
     * [trimPreviewStart_, trimPreviewEnd_) instead of the full loop.
     */
    void processOverdub(const float* dryInput, const float* wetInput,
                        float* outputBuffer, int numFrames, float volume) {
        // If a buffer mutation (crop/uncrop) is in progress on the UI thread,
        // output silence for this block (~5ms) to avoid reading inconsistent data.
        if (bufferMutationInProgress_.load(std::memory_order_acquire)) return;

        const int32_t len = loopLengthInternal_;
        if (len <= 0) return;

        // --- Trim preview: constrain overdub to [effectiveStart, effectiveEnd) ---
        const bool trimming = trimPreviewActive_.load(std::memory_order_relaxed);
        const int32_t effectiveStart = trimming
            ? trimPreviewStart_.load(std::memory_order_relaxed) : 0;
        const int32_t effectiveEnd = trimming
            ? trimPreviewEnd_.load(std::memory_order_relaxed) : len;
        const int32_t effectiveLen = effectiveEnd - effectiveStart;
        if (effectiveLen <= 0) return;

        const int32_t fadeLen = getEffectiveFadeLength(effectiveLen);

        // Clamp playPos_ to the effective range if it has drifted outside
        if (playPos_ < effectiveStart || playPos_ >= effectiveEnd) {
            playPos_ = effectiveStart;
        }

        for (int i = 0; i < numFrames; ++i) {
            // CRITICAL: Capture the new guitar input BEFORE summing loop playback
            // into outputBuffer. When wetInput == outputBuffer (which is always the
            // case — see AudioEngine::onAudioReady), reading wetInput[i] after
            // adding loop playback to outputBuffer[i] would capture guitar + loop,
            // causing layer 1 to contain a copy of layer 0 (doubling on playback).
            const float newWetSample = wetInput[i];
            const float newDrySample = dryInput[i];

            // Incremental undo copy: save current wet sample before overwriting.
            // We do this on the first complete pass through the loop after
            // overdub starts. This avoids a bulk copy which would be unsafe
            // if the loop is very long.
            if (!undoCopyDone_) {
                loopUndoBuffer_[playPos_] = loopWetBuffer_[playPos_];
            }

            // Read existing loop content for playback
            float existingSample = loopWetBuffer_[playPos_];
            const int32_t relativePos = playPos_ - effectiveStart;
            existingSample = applyCrossfadeForRange(existingSample, relativePos,
                                                    effectiveLen, fadeLen,
                                                    effectiveStart);
            existingSample = fast_math::denormal_guard(existingSample);
            outputBuffer[i] += existingSample * volume;

            // Sum new input onto existing loop content (overdub mix).
            // Uses the pre-captured samples (before loop playback was added to output).
            loopWetBuffer_[playPos_] += newWetSample;
            loopDryBuffer_[playPos_] += newDrySample;

            // Store the overdub delta in the current layer buffer
            const int currentLayer = std::min(layerCountInternal_, kMaxLayers - 1);
            if (!layerBuffers_[currentLayer].empty() &&
                playPos_ < static_cast<int32_t>(layerBuffers_[currentLayer].size())) {
                layerBuffers_[currentLayer][playPos_] += newWetSample;
            }

            playPos_++;
            if (playPos_ >= effectiveEnd) {
                playPos_ = effectiveStart;
                // After one full pass, undo copy is complete
                if (!undoCopyDone_) {
                    undoCopyDone_ = true;
                    undoAvailable_.store(true, std::memory_order_release);
                }
            }
        }

        // Update beat tracking
        updateBeatTracking(numFrames);
    }

    /**
     * Process FINISHING state: continue recording until quantized target
     * length is reached, then transition to PLAYING.
     * Also plays back what has been recorded so far (feedback monitoring).
     */
    void processFinishing(const float* dryInput, const float* wetInput,
                          float* outputBuffer, int numFrames, float volume) {
        const int32_t target = finishingTargetLength_;

        for (int i = 0; i < numFrames; ++i) {
            if (loopWritePos_ >= target || loopWritePos_ >= bufferCapacity_) {
                // Reached quantized target: finalize
                finalizeRecording();
                setState(LooperState::PLAYING);

                // Process remaining frames in this block as playback
                const int remaining = numFrames - i;
                if (remaining > 0) {
                    processPlayback(outputBuffer + i, remaining, volume);
                }
                return;
            }

            // Continue recording
            loopDryBuffer_[loopWritePos_] = dryInput[i];
            loopWetBuffer_[loopWritePos_] = wetInput[i];
            // Store in layer 0 (base recording) for per-layer volume control
            if (!layerBuffers_[0].empty()) {
                layerBuffers_[0][loopWritePos_] = wetInput[i];
            }
            loopWritePos_++;
        }

        // Update beat tracking
        updateBeatTracking(numFrames);
    }

    // =========================================================================
    // Crossfade logic
    // =========================================================================

    /**
     * Get the effective crossfade length for the current loop, capping at
     * 2% of loop length for very short loops.
     *
     * @param loopLen Current loop length in samples.
     * @return Effective crossfade length in samples.
     */
    int32_t getEffectiveFadeLength(int32_t loopLen) const {
        const int32_t maxFade = static_cast<int32_t>(
            static_cast<float>(loopLen) * kMaxFadeRatio);
        return std::clamp(std::min(crossfadeSamples_, maxFade),
                          kMinCrossfadeSamples,
                          crossfadeSamples_);
    }

    /**
     * Apply crossfade to a sample near the loop boundary.
     *
     * At the END of the loop (last fadeLen samples): fade out the current
     * sample and fade in the corresponding sample from the loop START.
     *
     * @param sample  The sample at the current playback position.
     * @param pos     Current position within the loop.
     * @param loopLen Loop length in samples.
     * @param fadeLen Effective crossfade length in samples.
     * @return Crossfaded sample value.
     */
    float applyCrossfade(float sample, int32_t pos, int32_t loopLen,
                         int32_t fadeLen) const {
        // Only apply in the crossfade region at the end of the loop
        const int32_t fadeStart = loopLen - fadeLen;
        if (pos < fadeStart || fadeLen <= 0) return sample;

        const int32_t fadeIdx = pos - fadeStart;

        // Bounds check (fadeIdx should be < fadeLen, but be defensive)
        if (fadeIdx >= static_cast<int32_t>(fadeOutTable_.size())) return sample;

        // Fade out the tail sample, fade in the corresponding head sample
        const float fadeOut = fadeOutTable_[fadeIdx];
        const float fadeIn = fadeInTable_[fadeIdx];

        // The head sample is at the corresponding position from loop start
        const float headSample = (fadeIdx < loopLen)
            ? loopWetBuffer_[fadeIdx] : 0.0f;

        return sample * fadeOut + headSample * fadeIn;
    }

    /**
     * Apply crossfade for a sub-range of the loop buffer (used during
     * trim preview). Works like applyCrossfade() but relative to an
     * arbitrary [rangeStart, rangeStart+rangeLen) window.
     *
     * @param sample       The sample at the current position.
     * @param relativePos  Position relative to rangeStart (0-based).
     * @param rangeLen     Length of the sub-range in samples.
     * @param fadeLen      Effective crossfade length in samples.
     * @param rangeStart   Absolute start position of the range in the buffer.
     * @return Crossfaded sample value.
     */
    float applyCrossfadeForRange(float sample, int32_t relativePos,
                                 int32_t rangeLen, int32_t fadeLen,
                                 int32_t rangeStart) const {
        // Only apply in the crossfade region at the end of the range
        const int32_t fadeStart = rangeLen - fadeLen;
        if (relativePos < fadeStart || fadeLen <= 0) return sample;

        const int32_t fadeIdx = relativePos - fadeStart;

        // Bounds check
        if (fadeIdx >= static_cast<int32_t>(fadeOutTable_.size())) return sample;

        // Fade out the tail sample, fade in the corresponding head sample
        const float fadeOut = fadeOutTable_[fadeIdx];
        const float fadeIn = fadeInTable_[fadeIdx];

        // The head sample is at rangeStart + fadeIdx (beginning of the range)
        const int32_t headAbsPos = rangeStart + fadeIdx;
        const float headSample = (headAbsPos < rangeStart + rangeLen)
            ? loopWetBuffer_[headAbsPos] : 0.0f;

        return sample * fadeOut + headSample * fadeIn;
    }

    /**
     * Pre-compute Hann window crossfade tables.
     * fadeIn ramps 0->1, fadeOut ramps 1->0, and they sum to 1.0 at each point.
     *
     * Hann window: w(n) = 0.5 * (1 - cos(2*pi*n / (N-1)))
     * We use it for equal-power-like crossfading. The sum property
     * fadeIn[i] + fadeOut[i] = 1.0 ensures no gain change at the boundary.
     *
     * @param length Number of samples in the crossfade.
     */
    void computeCrossfadeTables(int32_t length) {
        if (length <= 0) return;

        const float invLen = 1.0f / static_cast<float>(length);

        for (int32_t i = 0; i < length; ++i) {
            // Phase goes from 0 to ~1 across the fade
            const float phase = static_cast<float>(i) * invLen;

            // Hann fade-in: 0.5 * (1 - cos(pi * phase))
            // Using fast_math::sin2pi for cos: cos(pi*t) = sin2pi(t/2 + 0.25)
            // Actually simpler: cos(pi*phase) = -cos(pi*(1-phase))
            // Direct computation using sin2pi:
            //   cos(pi * phase) = sin(pi/2 + pi*phase) = sin2pi(0.25 + phase/2)
            // But the simplest correct formula:
            //   fadeIn = 0.5 * (1 - cos(pi * phase))
            //   cos(pi * phase) = sin2pi(phase * 0.5 + 0.25)
            const float cosVal = fast_math::sin2pi(phase * 0.5f + 0.25f);
            fadeInTable_[i] = 0.5f * (1.0f - cosVal);
            fadeOutTable_[i] = 0.5f * (1.0f + cosVal);
        }
    }

    /**
     * Apply crossfade in-place to the loop start and end regions when
     * first recording is finalized. This creates a smooth loop by blending
     * the end of the recording into the start.
     */
    void applyCrossfadeToRecording() {
        const int32_t len = loopWritePos_;
        const int32_t fadeLen = getEffectiveFadeLength(len);
        if (fadeLen <= 0 || len < fadeLen * 2) return;

        // Ensure we have tables of the right size
        // (they were pre-computed in init, but might need capping for short loops)
        const int32_t effectiveFade = std::min(fadeLen,
            static_cast<int32_t>(fadeInTable_.size()));

        // Crossfade: blend end region into start region
        for (int32_t i = 0; i < effectiveFade; ++i) {
            const int32_t endIdx = len - effectiveFade + i;
            const float fadeIn = fadeInTable_[i];
            const float fadeOut = fadeOutTable_[i];

            // Apply to wet buffer
            const float headWet = loopWetBuffer_[i];
            const float tailWet = loopWetBuffer_[endIdx];
            loopWetBuffer_[i] = headWet * fadeIn + tailWet * fadeOut;

            // Apply to dry buffer
            const float headDry = loopDryBuffer_[i];
            const float tailDry = loopDryBuffer_[endIdx];
            loopDryBuffer_[i] = headDry * fadeIn + tailDry * fadeOut;
        }
    }

    // =========================================================================
    // Per-layer crossfade and recomposition
    // =========================================================================

    /**
     * Apply the same Hann crossfade to a specific layer buffer.
     * Called after finalizeRecording() for the base layer, and could be
     * called for overdub layers if needed.
     *
     * @param layer Layer index to apply crossfade to.
     */
    void applyCrossfadeToLayer(int layer) {
        if (layer < 0 || layer >= kMaxLayers) return;
        auto& buf = layerBuffers_[layer];
        const int32_t len = loopWritePos_;
        if (buf.empty() || len <= 0) return;

        const int32_t fadeLen = getEffectiveFadeLength(len);
        if (fadeLen <= 0 || len < fadeLen * 2) return;

        const int32_t effectiveFade = std::min(fadeLen,
            static_cast<int32_t>(fadeInTable_.size()));

        for (int32_t i = 0; i < effectiveFade; ++i) {
            const int32_t endIdx = len - effectiveFade + i;
            if (endIdx < 0 || endIdx >= static_cast<int32_t>(buf.size())) continue;
            const float fadeIn = fadeInTable_[i];
            const float fadeOut = fadeOutTable_[i];
            const float head = buf[i];
            const float tail = buf[endIdx];
            buf[i] = head * fadeIn + tail * fadeOut;
        }
    }

    /**
     * Recomposite the wet buffer from all active layers with their volumes.
     * Called from UI/JNI thread when a layer volume changes.
     *
     * NOT real-time safe (iterates full loop length * layer count).
     * Only safe to call when not recording/overdubbing.
     *
     * Uses bufferMutationInProgress_ to signal the audio thread to output
     * silence during the recomposition (same pattern as commitTrim).
     */
    void recompositeWetBuffer() {
        const int32_t len = loopLengthInternal_;
        if (len <= 0) return;

        const int layers = layerCountInternal_;
        if (layers <= 0) return;

        // Signal audio thread to output silence during recomposition
        bufferMutationInProgress_.store(true, std::memory_order_release);

        // Rebuild the composite wet buffer from individual layers
        for (int32_t s = 0; s < len; ++s) {
            float sum = 0.0f;
            for (int l = 0; l < layers; ++l) {
                if (!layerBuffers_[l].empty() &&
                    s < static_cast<int32_t>(layerBuffers_[l].size())) {
                    const float vol = layerVolumes_[l].load(std::memory_order_relaxed);
                    sum += layerBuffers_[l][s] * vol;
                }
            }
            loopWetBuffer_[s] = sum;
        }

        // Resume normal playback
        bufferMutationInProgress_.store(false, std::memory_order_release);
    }

    // =========================================================================
    // Metronome click generation
    // =========================================================================

    /**
     * Pre-compute metronome click waveforms based on the current tone type.
     *
     * Six synthesized tone types are available, each producing a distinct
     * percussive timbre by combining sine bursts, noise, and dual-frequency
     * content with an exponential decay envelope:
     *
     *   0 = Classic Click: high woodblock-like 1500Hz sine burst
     *   1 = Soft Tick: lower 800Hz sine, warmer feel (0.7x amplitude)
     *   2 = Rim Shot: 1200Hz sine + 30% noise burst (crack + tone)
     *   3 = Cowbell: dual-freq 587Hz + 845Hz (metallic beating)
     *   4 = Hi-Hat: filtered noise burst (0.5x amplitude)
     *   5 = Deep Click: low 400Hz sine burst
     *
     * Downbeat clicks use 1.5x the frequency of normal beat clicks to
     * distinguish bar downbeats aurally.
     *
     * Uses fast_math::sin2pi for efficient sine generation.
     * The PRNG for noise tones uses a fast xorshift32 to avoid stdlib overhead.
     */
    void computeMetronomeClicks() {
        const int tone = metronomeTone_.load(std::memory_order_relaxed);

        const int32_t downbeatLen = static_cast<int32_t>(
            sampleRate_ * kDownbeatDurationMs / 1000.0f);
        downbeatClick_.resize(std::max(downbeatLen, 1));

        const int32_t beatLen = static_cast<int32_t>(
            sampleRate_ * kBeatDurationMs / 1000.0f);
        beatClick_.resize(std::max(beatLen, 1));

        // Local xorshift32 PRNG state for noise generation (deterministic seed)
        uint32_t rngState = 0xDEADBEEF;
        auto xorshift32 = [&rngState]() -> float {
            rngState ^= rngState << 13;
            rngState ^= rngState >> 17;
            rngState ^= rngState << 5;
            // Map to [-1.0, 1.0] range
            return static_cast<float>(static_cast<int32_t>(rngState))
                   / static_cast<float>(INT32_MAX);
        };

        // Generate downbeat click (1.5x frequency for emphasis)
        generateClickTone(downbeatClick_.data(), downbeatLen, tone,
                          kDownbeatAmplitude, true, xorshift32, rngState);

        // Reset PRNG for deterministic beat click
        rngState = 0xCAFEBABE;
        generateClickTone(beatClick_.data(), beatLen, tone,
                          kBeatAmplitude, false, xorshift32, rngState);
    }

    /**
     * Generate a single click waveform into the provided buffer.
     *
     * @param buf        Output buffer (must be at least len samples).
     * @param len        Number of samples in the click.
     * @param tone       Tone type (0-5).
     * @param amplitude  Peak amplitude.
     * @param isDownbeat Whether this is a downbeat (uses higher frequency).
     * @param noise      Lambda returning random float in [-1, 1].
     * @param rngState   PRNG state (passed by reference for noise tones).
     */
    template<typename NoiseFn>
    void generateClickTone(float* buf, int32_t len, int tone,
                           float amplitude, bool isDownbeat,
                           NoiseFn& noise, uint32_t& /*rngState*/) {
        const float sr = sampleRate_;
        // Exponential decay rate: -500/s gives ~10ms effective duration
        const float decayRate = 500.0f;

        for (int32_t i = 0; i < len; ++i) {
            const float t = static_cast<float>(i) / sr;
            const float env = std::exp(-t * decayRate);
            float sample = 0.0f;

            switch (tone) {
                case 0: {
                    // Classic Click: high woodblock sine burst
                    const float freq = isDownbeat ? 1500.0f * 1.5f : 1500.0f;
                    sample = env * fast_math::sin2pi(freq * t);
                    break;
                }
                case 1: {
                    // Soft Tick: lower, warmer tone
                    const float freq = isDownbeat ? 800.0f * 1.3f : 800.0f;
                    sample = env * fast_math::sin2pi(freq * t) * 0.7f;
                    break;
                }
                case 2: {
                    // Rim Shot: sine + noise burst
                    const float freq = isDownbeat ? 1200.0f * 1.4f : 1200.0f;
                    const float toneComp = fast_math::sin2pi(freq * t);
                    const float noiseComp = noise() * 0.3f;
                    sample = env * (toneComp + noiseComp);
                    break;
                }
                case 3: {
                    // Cowbell: dual-frequency metallic beating
                    const float f1 = isDownbeat ? 587.0f * 1.3f : 587.0f;
                    const float f2 = isDownbeat ? 845.0f * 1.3f : 845.0f;
                    sample = env * (fast_math::sin2pi(f1 * t)
                                  + 0.6f * fast_math::sin2pi(f2 * t));
                    break;
                }
                case 4: {
                    // Hi-Hat: filtered noise burst
                    sample = env * noise() * 0.5f;
                    break;
                }
                case 5: {
                    // Deep Click: low sine burst
                    const float freq = isDownbeat ? 400.0f * 1.5f : 400.0f;
                    sample = env * fast_math::sin2pi(freq * t);
                    break;
                }
                default:
                    break;
            }

            buf[i] = amplitude * sample;
        }
    }

    /**
     * Generate and sum metronome clicks into the output buffer.
     * Beat timing is tracked by beatSampleCounter_ and currentBeatInternal_.
     *
     * Clicks bypass the signal chain entirely (directly summed into output).
     *
     * @param outputBuffer Buffer to sum clicks into.
     * @param numFrames    Number of frames in this block.
     */
    void generateMetronomeClick(float* outputBuffer, int numFrames) {
        const float clickVol = clickVolume_.load(std::memory_order_relaxed);
        if (clickVol <= 0.0f) return;

        for (int i = 0; i < numFrames; ++i) {
            // Sum click sample if within click duration
            if (clickPlayPos_ >= 0) {
                const bool isDownbeat = (currentBeatInternal_ == 0);
                const auto& clickBuf = isDownbeat ? downbeatClick_ : beatClick_;
                const int32_t clickLen = static_cast<int32_t>(clickBuf.size());

                if (clickPlayPos_ < clickLen) {
                    outputBuffer[i] += clickBuf[clickPlayPos_] * clickVol;
                    clickPlayPos_++;
                } else {
                    clickPlayPos_ = -1; // Click finished
                }
            }
        }
    }

    // =========================================================================
    // Beat tracking
    // =========================================================================

    /**
     * Update beat tracking state based on elapsed samples.
     * Uses double precision accumulator to prevent drift over long loops.
     *
     * @param numFrames Number of frames processed in this block.
     */
    void updateBeatTracking(int numFrames) {
        const float bpm = bpm_.load(std::memory_order_relaxed);
        const int beatsPerBar = beatsPerBar_.load(std::memory_order_relaxed);
        const double samplesPerBeat = (60.0 / static_cast<double>(bpm))
                                      * static_cast<double>(sampleRate_);

        beatSampleCounter_ += numFrames;

        while (beatSampleCounter_ >= static_cast<int64_t>(samplesPerBeat)) {
            beatSampleCounter_ -= static_cast<int64_t>(samplesPerBeat);
            currentBeatInternal_++;

            if (currentBeatInternal_ >= beatsPerBar) {
                currentBeatInternal_ = 0;
            }

            // Publish beat for UI
            currentBeat_.store(currentBeatInternal_, std::memory_order_release);

            // Trigger click playback at start of each beat
            clickPlayPos_ = 0;
        }
    }

    // =========================================================================
    // Quantization helpers
    // =========================================================================

    /**
     * Compute the quantized loop length in samples based on current BPM,
     * time signature, and bar count settings.
     *
     * Formula: samples = round((60 / BPM) * beatsPerBar * numBars * sampleRate)
     *
     * Uses double precision to avoid rounding drift at high sample rates.
     *
     * @return Quantized loop length in samples, capped at buffer capacity.
     */
    int32_t computeQuantizedLoopLength() const {
        const double bpm = static_cast<double>(
            bpm_.load(std::memory_order_relaxed));
        const double beats = static_cast<double>(
            beatsPerBar_.load(std::memory_order_relaxed));
        const double bars = static_cast<double>(
            numBars_.load(std::memory_order_relaxed));
        const double sr = static_cast<double>(sampleRate_);

        const double secondsPerBeat = 60.0 / bpm;
        const double totalSamples = secondsPerBeat * beats * bars * sr;

        const int32_t quantized = static_cast<int32_t>(std::round(totalSamples));
        return std::min(quantized, bufferCapacity_);
    }

    /**
     * Compute the quantized stop length when the user presses stop during
     * recording in quantized mode.
     *
     * Snapping rule: if currentPos is within 50% of a bar boundary, snap
     * to that bar boundary. Otherwise, snap to the next bar boundary.
     *
     * @param currentPos Current recording position in samples.
     * @return Target loop length to record to (in samples).
     */
    int32_t computeQuantizedStopLength(int32_t currentPos) const {
        const double bpm = static_cast<double>(
            bpm_.load(std::memory_order_relaxed));
        const double beats = static_cast<double>(
            beatsPerBar_.load(std::memory_order_relaxed));
        const double sr = static_cast<double>(sampleRate_);

        // Samples per bar
        const double samplesPerBar = (60.0 / bpm) * beats * sr;
        if (samplesPerBar <= 0.0) return currentPos;

        // How many complete bars fit in currentPos?
        const double barsRecorded = static_cast<double>(currentPos) / samplesPerBar;
        const int32_t completeBars = static_cast<int32_t>(barsRecorded);
        const double fractionalBar = barsRecorded - static_cast<double>(completeBars);

        // Snap decision: if past 50% of the current bar, round up
        int32_t targetBars;
        if (fractionalBar >= kSnapThreshold) {
            targetBars = completeBars + 1;
        } else {
            targetBars = std::max(completeBars, 1); // At least 1 bar
        }

        const int32_t targetSamples = static_cast<int32_t>(
            std::round(static_cast<double>(targetBars) * samplesPerBar));
        return std::min(targetSamples, bufferCapacity_);
    }

    // =========================================================================
    // Recording finalization
    // =========================================================================

    /**
     * Finalize a recording: set the loop length, apply boundary crossfade,
     * and prepare for playback.
     */
    void finalizeRecording() {
        loopLengthInternal_ = loopWritePos_;
        loopLength_.store(loopLengthInternal_, std::memory_order_release);

        // Apply crossfade at the loop boundary for seamless looping
        applyCrossfadeToRecording();

        // Apply the same crossfade to layer 0 buffer
        applyCrossfadeToLayer(0);

        // Reset playback position to the start
        playPos_ = 0;

        // Reset beat tracking for synchronized metronome
        beatSampleCounter_ = 0;
        currentBeatInternal_ = 0;
        currentBeat_.store(0, std::memory_order_release);
        clickPlayPos_ = 0; // Trigger downbeat click on first play

        // No undo available yet (need an overdub first)
        undoAvailable_.store(false, std::memory_order_release);

        // Mark layer 0 (base recording) as complete
        layerCountInternal_ = 1;
        layerCount_.store(1, std::memory_order_release);
    }

    // =========================================================================
    // State management helpers
    // =========================================================================

    /**
     * Set the looper state atomically and return the new state.
     * @param newState The state to transition to.
     * @return The new state (for chaining in handleCommand).
     */
    LooperState setState(LooperState newState) {
        currentState_.store(static_cast<int>(newState),
                            std::memory_order_release);
        return newState;
    }

    /** Clear all loop data and reset to initial state. */
    void clearLoop() {
        const int32_t len = loopLengthInternal_;

        // Only zero the used portion (not the entire pre-allocated buffer)
        if (len > 0) {
            std::memset(loopDryBuffer_.data(), 0, len * sizeof(float));
            std::memset(loopWetBuffer_.data(), 0, len * sizeof(float));
            std::memset(loopUndoBuffer_.data(), 0, len * sizeof(float));
            // Clear used portion of each populated layer buffer
            for (int i = 0; i < layerCountInternal_; ++i) {
                if (!layerBuffers_[i].empty()) {
                    std::memset(layerBuffers_[i].data(), 0,
                                std::min(len, static_cast<int32_t>(layerBuffers_[i].size()))
                                * sizeof(float));
                }
            }
        }

        loopWritePos_ = 0;
        loopLengthInternal_ = 0;
        playPos_ = 0;
        finishingTargetLength_ = 0;
        quantizedLengthSnapshot_ = 0;
        undoCopyDone_ = false;
        overdubStartPos_ = 0;
        layerCountInternal_ = 0;

        // Publish state
        loopLength_.store(0, std::memory_order_release);
        playbackPosition_.store(0, std::memory_order_release);
        undoAvailable_.store(false, std::memory_order_release);
        layerCount_.store(0, std::memory_order_release);

        // Reset layer volumes to unity
        for (int i = 0; i < kMaxLayers; ++i) {
            layerVolumes_[i].store(1.0f, std::memory_order_relaxed);
        }

        // Reset trim/crop state
        trimPreviewActive_.store(false, std::memory_order_relaxed);
        trimPreviewStart_.store(0, std::memory_order_relaxed);
        trimPreviewEnd_.store(0, std::memory_order_relaxed);
        cropUndoAvailable_.store(false, std::memory_order_relaxed);
        preCropLoopLength_ = 0;

        // Reset beat tracking
        beatSampleCounter_ = 0;
        currentBeatInternal_ = 0;
        currentBeat_.store(0, std::memory_order_release);
        clickPlayPos_ = -1;
    }

    /** Reset all state to initial values (called by init and release). */
    void resetState() {
        command_.store(static_cast<int>(LooperCommand::NONE),
                       std::memory_order_relaxed);
        currentState_.store(static_cast<int>(LooperState::IDLE),
                            std::memory_order_relaxed);
        playbackPosition_.store(0, std::memory_order_relaxed);
        loopLength_.store(0, std::memory_order_relaxed);
        currentBeat_.store(0, std::memory_order_relaxed);
        undoAvailable_.store(false, std::memory_order_relaxed);

        loopWritePos_ = 0;
        loopLengthInternal_ = 0;
        playPos_ = 0;
        finishingTargetLength_ = 0;
        quantizedLengthSnapshot_ = 0;
        undoCopyDone_ = false;
        overdubStartPos_ = 0;
        layerCountInternal_ = 0;

        beatSampleCounter_ = 0;
        currentBeatInternal_ = 0;
        clickPlayPos_ = -1;

        // Layer state
        layerCount_.store(0, std::memory_order_relaxed);
        for (int i = 0; i < kMaxLayers; ++i) {
            layerVolumes_[i].store(1.0f, std::memory_order_relaxed);
        }

        // Trim/crop state
        trimPreviewActive_.store(false, std::memory_order_relaxed);
        trimPreviewStart_.store(0, std::memory_order_relaxed);
        trimPreviewEnd_.store(0, std::memory_order_relaxed);
        cropUndoAvailable_.store(false, std::memory_order_relaxed);
        bufferMutationInProgress_.store(false, std::memory_order_relaxed);
        preCropLoopLength_ = 0;

        // Re-amp state
        reampState_.store(static_cast<int>(ReampState::IDLE),
                          std::memory_order_relaxed);
        reampReadPos_ = 0;
        reampLength_ = 0;

        // Linear recording
        recordToRing_.store(false, std::memory_order_relaxed);

        // Tap tempo
        resetTapTempo();
    }

    /** Prepare for count-in: reset beat tracking to count one bar of clicks. */
    void startCountIn() {
        beatSampleCounter_ = 0;
        currentBeatInternal_ = 0;
        currentBeat_.store(0, std::memory_order_release);
        clickPlayPos_ = 0; // Trigger first downbeat click immediately
    }

    /** Prepare for recording: reset write position and beat tracking. */
    void startRecording() {
        loopWritePos_ = 0;
        playPos_ = 0;
        beatSampleCounter_ = 0;
        currentBeatInternal_ = 0;
        currentBeat_.store(0, std::memory_order_release);
        clickPlayPos_ = 0; // Trigger downbeat click at recording start
    }

    // =========================================================================
    // Writer thread (for linear recording to disk)
    // =========================================================================

    /**
     * Writer thread function: drains SPSC ring buffers to WAV files.
     * Runs until writerShouldRun_ is set to false, then performs one
     * final drain to capture any remaining buffered audio.
     */
    void writerThreadFunc() {
        // Drain buffer for reading from ring (4096 samples at a time)
        constexpr int32_t kDrainBufSize = 4096;
        std::vector<float> drainBuf(kDrainBufSize);

        while (writerShouldRun_.load(std::memory_order_acquire)) {
            drainRingToWriter(drainBuf.data(), kDrainBufSize);

            // Sleep 100ms between drain polls. This is well within the ~2s
            // ring capacity at 48kHz, so no risk of overrun under normal load.
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Final drain: capture any remaining samples after stop signal
        drainRingToWriter(drainBuf.data(), kDrainBufSize);
    }

    /**
     * Drain both ring buffers to their respective WAV writers.
     * Called from the writer thread.
     */
    void drainRingToWriter(float* drainBuf, int32_t bufSize) {
        if (dryRing_ && dryWriter_) {
            int32_t read;
            while ((read = dryRing_->read(drainBuf, bufSize)) > 0) {
                dryWriter_->writeSamples(drainBuf, read);
            }
        }
        if (wetRing_ && wetWriter_) {
            int32_t read;
            while ((read = wetRing_->read(drainBuf, bufSize)) > 0) {
                wetWriter_->writeSamples(drainBuf, read);
            }
        }
    }

    // =========================================================================
    // Member variables
    // =========================================================================

    // ---- Buffers (pre-allocated in init, never resized on audio thread) ----
    std::vector<float> loopDryBuffer_;   ///< Accumulated dry (DI) signal
    std::vector<float> loopWetBuffer_;   ///< Accumulated wet (processed) signal
    std::vector<float> loopUndoBuffer_;  ///< Undo snapshot of wet buffer

    // ---- Crossfade tables (pre-computed in init) ----
    std::vector<float> fadeInTable_;     ///< Hann fade-in: 0 -> 1
    std::vector<float> fadeOutTable_;    ///< Hann fade-out: 1 -> 0

    // ---- Metronome click waveforms (pre-computed in init) ----
    std::vector<float> downbeatClick_;   ///< 1200Hz sine burst for bar downbeat
    std::vector<float> beatClick_;       ///< 800Hz sine burst for other beats

    // ---- Per-layer overdub buffers (allocated lazily during overdub) ----
    /// Each layer stores its individual audio contribution.
    /// Layer 0 = base recording, layers 1+ = overdub passes.
    /// Only layerBuffers_[0..layerCountInternal_-1] are populated.
    std::vector<float> layerBuffers_[kMaxLayers];

    // ---- Crop undo buffer (pre-allocated in init) ----
    std::vector<float> cropUndoDryBuffer_;  ///< Dry buffer snapshot for crop undo

    // ---- Re-amp buffer ----
    std::vector<float> reampBuffer_;     ///< Dry signal for re-amp playback

    // ---- Configuration (set in init, read-only on audio thread) ----
    float sampleRate_       = 0.0f;
    float maxDurationSec_   = 0.0f;
    int32_t bufferCapacity_ = 0;
    int32_t crossfadeSamples_ = 0;       ///< Nominal crossfade length in samples
    int32_t recordFadeInSamples_ = 0;    ///< Fade-in length for record start

    // ---- Audio-thread-only state (no atomics needed) ----
    int32_t loopWritePos_      = 0;      ///< Current write position during recording
    int32_t loopLengthInternal_ = 0;     ///< Loop length (audio thread copy)
    int32_t playPos_           = 0;      ///< Current playback/overdub position
    int32_t finishingTargetLength_ = 0;  ///< Target length when in FINISHING state
    int32_t quantizedLengthSnapshot_ = 0; ///< Quantized length snapshotted at record start

    // Overdub/undo tracking
    bool undoCopyDone_         = false;  ///< True after one full undo pass
    int32_t overdubStartPos_   = 0;      ///< Position where overdub started

    // Layer tracking (audio-thread-only copy, published via layerCount_)
    int layerCountInternal_    = 0;      ///< Number of recorded layers

    // Beat tracking (double precision accumulator for drift prevention)
    int64_t beatSampleCounter_ = 0;      ///< Samples since last beat boundary
    int currentBeatInternal_   = 0;      ///< Current beat (audio thread copy)
    int32_t clickPlayPos_      = -1;     ///< Position within current click (-1 = idle)

    // Crop undo state
    int32_t preCropLoopLength_ = 0;      ///< Loop length before last crop

    // Re-amp (audio-thread state)
    int32_t reampReadPos_      = 0;      ///< Current read position in reamp buffer
    int32_t reampLength_       = 0;      ///< Length of loaded reamp data

    // ---- Cross-thread atomics ----

    /// Pending command from UI thread (consumed via exchange on audio thread)
    std::atomic<int> command_{static_cast<int>(LooperCommand::NONE)};

    /// Current state (written by audio thread, read by UI thread)
    std::atomic<int> currentState_{static_cast<int>(LooperState::IDLE)};

    /// Playback position in samples (for UI waveform cursor)
    std::atomic<int32_t> playbackPosition_{0};

    /// Loop length in samples (for UI display)
    std::atomic<int32_t> loopLength_{0};

    /// Current beat within bar (for UI metronome indicator)
    std::atomic<int> currentBeat_{0};

    /// Whether undo is available
    std::atomic<bool> undoAvailable_{false};

    /// Loop playback volume (0.0 to 1.0)
    std::atomic<float> volume_{1.0f};

    /// Per-layer volumes (0.0 to 1.5, default 1.0).
    /// Indexed by layer number: 0 = base recording, 1+ = overdub passes.
    std::atomic<float> layerVolumes_[kMaxLayers];

    /// Number of recorded layers (published for UI thread)
    std::atomic<int> layerCount_{0};

    /// Metronome click volume (0.0 to 1.0)
    std::atomic<float> clickVolume_{0.5f};

    /// Metronome active flag
    std::atomic<bool> metronomeActive_{false};

    /// Metronome tone type (0-5): Classic, Soft, Rim, Cowbell, Hi-Hat, Deep
    std::atomic<int> metronomeTone_{0};

    /// BPM (30.0 to 300.0)
    std::atomic<float> bpm_{kDefaultBPM};

    /// Time signature numerator
    std::atomic<int> beatsPerBar_{4};

    /// Time signature denominator
    std::atomic<int> beatUnit_{4};

    /// Quantized loop length in bars
    std::atomic<int> numBars_{4};

    /// Quantized (bar-snapping) mode flag
    std::atomic<bool> quantizedMode_{false};

    /// Count-in enabled flag (default true, matching Kotlin LooperDelegate)
    std::atomic<bool> countInEnabled_{true};

    // ---- Trim preview state (cross-thread atomics) ----

    /// Whether trim preview is currently active
    std::atomic<bool> trimPreviewActive_{false};

    /// Trim start offset in samples (from loop start)
    std::atomic<int32_t> trimPreviewStart_{0};

    /// Trim end offset in samples (from loop start)
    std::atomic<int32_t> trimPreviewEnd_{0};

    // ---- Crop undo state (cross-thread atomic) ----

    /// Whether crop undo is available (one-level)
    std::atomic<bool> cropUndoAvailable_{false};

    /// Guard flag: set by UI thread during commitTrim()/undoCrop() buffer
    /// mutations. When true, processPlayback()/processOverdub() output
    /// silence for the current block to avoid reading inconsistent data.
    std::atomic<bool> bufferMutationInProgress_{false};

    /// Re-amp playback state
    std::atomic<int> reampState_{static_cast<int>(ReampState::IDLE)};

    /// Linear recording to SPSC ring buffer flag (stub)
    std::atomic<bool> recordToRing_{false};

    // ---- Linear recording state ----

    /// SPSC ring buffers for streaming audio to disk (owned, nullable)
    std::unique_ptr<SPSCRingBuffer<float>> dryRing_;
    std::unique_ptr<SPSCRingBuffer<float>> wetRing_;

    /// WAV file writers (owned, nullable)
    std::unique_ptr<WavWriter> dryWriter_;
    std::unique_ptr<WavWriter> wetWriter_;

    /// Writer thread (joins on stopLinearRecording)
    std::thread writerThread_;

    /// Flag telling the writer thread to keep running
    std::atomic<bool> writerShouldRun_{false};

    /// Whether linear recording is currently active
    std::atomic<bool> linearRecordingActive_{false};

    // ---- Tap tempo state ----
    // onTap() and resetTapTempo() are called from UI thread.
    // getTapBPM() may be called from any thread, so tapBPM_ is atomic.
    int64_t tapTimestamps_[kMaxTapHistory] = {};
    int tapCount_      = 0;
    int tapWriteIdx_   = 0;
    std::atomic<float> tapBPM_{0.0f};
};

#endif // GUITAR_ENGINE_LOOPER_ENGINE_H
