#ifndef GUITAR_ENGINE_SIGNAL_CHAIN_H
#define GUITAR_ENGINE_SIGNAL_CHAIN_H

#include <vector>
#include <array>
#include <atomic>
#include <memory>
#include <cstdint>
#include <string>
#include "effects/effect.h"

/**
 * Manages an ordered chain of audio effects and processes audio through them.
 *
 * The signal chain is the core of the guitar effects processor. It holds
 * a vector of Effect objects in a specific order and processes audio through
 * each enabled effect sequentially. The order matters musically:
 *
 * Default chain order (32 effects):
 *   0. Noise Gate     -- Remove noise before it gets amplified
 *   1. Compressor     -- Even out dynamics before distortion
 *   2. Wah            -- Filter effect before distortion (off by default)
 *   3. Boost          -- Clean gain stage before overdrive (off by default)
 *   4. Overdrive      -- Distortion/saturation
 *   5. Amp Model      -- Neural amp modeling / tube simulation (off by default)
 *   6. Cabinet Sim    -- Speaker cabinet IR convolution (off by default)
 *   7. Parametric EQ  -- Tone sculpting after amp/cab
 *   8. Chorus         -- Thickening effect
 *   9. Vibrato        -- Pitch modulation via delay line (off by default)
 *  10. Phaser         -- Sweeping notch filter (off by default)
 *  11. Flanger        -- Comb filter sweep (off by default)
 *  12. Delay          -- Echoes after modulation
 *  13. Reverb         -- Spatial ambience
 *  14. Tuner          -- Pitch detection (passthrough, no audio modification)
 *  15. Tremolo        -- FAUST-generated amplitude modulation (off by default)
 *  16. Boss DS-1      -- Circuit-accurate WDF distortion (off by default)
 *  17. ProCo RAT      -- Circuit-accurate WDF distortion (off by default)
 *  18. Boss DS-2      -- Circuit-accurate WDF turbo distortion (off by default)
 *  19. Boss HM-2      -- Circuit-accurate WDF heavy metal (off by default)
 *  20. Univibe        -- Thermal LDR modulation effect (off by default)
 *  21. Fuzz Face      -- WDF Germanium 2-transistor fuzz (off by default)
 *  22. Rangemaster    -- WDF Germanium treble booster (off by default)
 *  23. Big Muff Pi   -- WDF 4-stage cascaded fuzz (off by default)
 *  24. Octavia       -- Octave-up rectifier fuzz (off by default)
 *  25. Rotary Speaker -- Leslie cabinet doppler/amplitude modulation (off by default)
 *  26. Fuzzrite     -- Mosrite Fuzzrite circuit-accurate fuzz (off by default)
 *  27. MF-102 Ring Mod -- OTA four-quadrant ring modulator (off by default)
 *  28. Space Echo   -- Roland RE-201 tape echo + spring reverb (off by default)
 *  29. DL4 Delay    -- Line 6 DL4 Delay Modeler, 16 algorithms (off by default)
 *  30. Boss BD-2    -- Blues Driver discrete JFET overdrive (off by default)
 *  31. Tube Screamer -- TS-808/TS9 circuit-accurate overdrive (off by default)
 *
 * Thread safety:
 *   - process() is called on the audio thread and is fully real-time safe.
 *   - setSampleRate(), resetAll(), createDefaultChain() are called on the
 *     main/UI thread during stream setup (before audio processing starts).
 *   - getEffect() is safe to call from any thread for parameter control,
 *     as individual parameter reads/writes are atomic.
 *
 * Wet/dry mixing:
 *   For each effect whose wetDryMix is less than 1.0, the signal chain
 *   copies the dry signal before processing, then blends wet and dry
 *   using the effect's applyWetDryMix() method. The dry buffer is
 *   pre-allocated to avoid audio-thread allocations.
 */
class SignalChain {
public:
    SignalChain();
    ~SignalChain();

    // Non-copyable (owns unique_ptr resources)
    SignalChain(const SignalChain&) = delete;
    SignalChain& operator=(const SignalChain&) = delete;

    /**
     * Process audio through the entire effect chain.
     * Called from the audio callback thread -- must be real-time safe.
     *
     * For each enabled effect:
     *   1. If wetDryMix < 1.0, copy buffer to dry buffer
     *   2. Call effect->process(buffer, numFrames)
     *   3. If wetDryMix < 1.0, blend wet/dry
     *
     * Disabled effects are skipped entirely (zero processing cost).
     *
     * @param buffer   Audio samples to process in-place (mono float).
     * @param numFrames Number of audio frames.
     */
    void process(float* buffer, int numFrames);

    /**
     * Set the sample rate on all effects in the chain.
     * Called during audio stream setup, before processing starts.
     * This triggers buffer pre-allocation in effects that need it.
     *
     * @param sampleRate Device native sample rate in Hz.
     */
    void setSampleRate(int32_t sampleRate);

    /**
     * Reset all effects in the chain.
     * Clears internal state (delay lines, filter history, envelopes).
     * Called when the audio stream restarts.
     */
    void resetAll();

    /**
     * Get the number of effects in the chain.
     * @return Effect count.
     */
    int getEffectCount() const;

    /**
     * Get a pointer to an effect by its index in the chain.
     * Used by the JNI bridge for parameter control.
     *
     * @param index Zero-based index into the effect chain.
     * @return Pointer to the effect, or nullptr if index is out of range.
     */
    Effect* getEffect(int index);

    /** Const overload for read-only access to effects. */
    const Effect* getEffect(int index) const;

    /** Physical index of the Tuner effect in the effects_ vector. */
    static constexpr int kTunerIndex = 14;

    /**
     * Feed the tuner with pre-effects audio for reliable pitch detection.
     * Must be called BEFORE process() so the tuner analyzes the clean
     * guitar DI signal rather than the processed output.
     * The tuner's process() only reads samples (passthrough), so the
     * buffer is not modified.
     *
     * @param buffer   Mono audio samples (not modified by the tuner).
     * @param numFrames Number of frames.
     */
    void feedTuner(float* buffer, int numFrames);

    /**
     * Prepare the tuner for activation after being idle.
     * Resets startup skip counter, clears stale analysis buffer, and
     * restores the phase offset to avoid FFT timing collisions with
     * the spectrum analyzer. Called from the non-audio thread BEFORE
     * setting tunerActive_ = true, so it's safe (audio thread only
     * calls feedTuner when the flag is true).
     */
    void prepareTunerForActivation();

    /**
     * Create the default effect chain.
     * Clears any existing effects and creates the full guitar chain (32 effects):
     * NoiseGate -> Compressor -> Wah -> Boost -> Overdrive -> AmpModel ->
     * CabinetSim -> ParametricEQ -> Chorus -> Vibrato -> Phaser -> Flanger ->
     * Delay -> Reverb -> Tuner -> Tremolo -> BossDS1 -> ProCoRAT -> BossDS2 ->
     * BossHM2 -> Univibe -> FuzzFace -> Rangemaster -> BigMuff -> Octavia ->
     * RotarySpeaker -> Fuzzrite -> MF102RingMod -> SpaceEcho -> DL4Delay -> BossBD2 ->
     * TubeScreamer
     *
     * Called once during engine initialization.
     */
    void createDefaultChain();

    /**
     * Set the processing order of effects in the signal chain.
     *
     * Uses a double-buffered array with an atomic index swap so the audio
     * thread reads the active buffer lock-free while the UI thread writes
     * to the inactive buffer. No allocations, no locks.
     *
     * @param order Array of physical effect indices in desired processing order.
     * @param count Number of entries (must equal getEffectCount()).
     * @return true if the order was applied, false if count mismatches.
     */
    bool setEffectOrder(const int* order, int count);

    /**
     * Get the current processing order.
     * Copies the active order buffer into the caller's array.
     *
     * @param out   Output array to receive the order.
     * @param count Size of the output array (must be >= getEffectCount()).
     */
    void getEffectOrder(int* out, int count) const;

    /**
     * Set a parameter on a specific effect by index and parameter ID.
     * Delegates to the effect's setParameter() method.
     * Thread-safe (atomic parameter writes).
     *
     * @param effectIndex Zero-based index of the effect.
     * @param paramId     Effect-specific parameter ID.
     * @param value       New parameter value.
     */
    void setEffectParameter(int effectIndex, int paramId, float value);

    /**
     * Get a parameter from a specific effect by index and parameter ID.
     * Delegates to the effect's getParameter() method.
     * Thread-safe (atomic parameter reads).
     *
     * @param effectIndex Zero-based index of the effect.
     * @param paramId     Effect-specific parameter ID.
     * @return Current parameter value, or 0.0 if index is out of range.
     */
    float getEffectParameter(int effectIndex, int paramId) const;

    // ----- Neural amp model management -----

    /**
     * Load a neural amp model (.nam file) into the AmpModel effect.
     * Routes to AmpModel at chain index 5.
     * NOT real-time safe (parses file, allocates model). Call from non-audio thread.
     *
     * @param path Absolute filesystem path to the .nam model file.
     * @return true if the model was loaded successfully.
     */
    bool loadNeuralModel(const std::string& path);

    /**
     * Clear the loaded neural amp model and revert to classic waveshaping.
     */
    void clearNeuralModel();

    /**
     * Check whether a neural amp model is currently loaded and active.
     * @return true if neural mode is active.
     */
    bool isNeuralModelLoaded() const;

private:
    /**
     * Ensure the dry buffer is large enough for the given sample count.
     * Called at the start of process() if any effect needs wet/dry blending.
     * Only resizes if the current buffer is too small (which should only
     * happen on the first call, since buffer sizes are typically constant).
     *
     * Note: This could theoretically allocate on the audio thread on the
     * very first call if dryBuffer_ wasn't pre-sized. To prevent this,
     * we pre-allocate in setSampleRate() for the expected buffer size.
     */
    void ensureDryBufferSize(int numSamples);

    static constexpr int kMaxEffects = 32;

    /** Ordered list of effects in the signal chain. */
    std::vector<std::unique_ptr<Effect>> effects_;

    /**
     * Double-buffered processing order for lock-free reordering.
     * Each buffer maps logical processing position to physical effect index.
     * The audio thread reads orderBuf_[activeOrder_] while the UI thread
     * writes to orderBuf_[1 - activeOrder_] then atomically swaps.
     */
    std::array<int, kMaxEffects> orderBuf_[2];

    /** Index (0 or 1) of the order buffer currently read by the audio thread. */
    std::atomic<int> activeOrder_{0};

    /** Current sample rate, stored to pass to newly added effects. */
    int32_t sampleRate_ = 44100;

    /**
     * Pre-allocated buffer for storing the dry (unprocessed) signal
     * when wet/dry blending is needed. Sized during setSampleRate().
     */
    std::vector<float> dryBuffer_;

    /** Current size of the dry buffer in samples. */
    int dryBufferSize_ = 0;
};

#endif // GUITAR_ENGINE_SIGNAL_CHAIN_H
