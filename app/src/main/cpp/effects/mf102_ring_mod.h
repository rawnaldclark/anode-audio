#ifndef GUITAR_ENGINE_MF102_RING_MOD_H
#define GUITAR_ENGINE_MF102_RING_MOD_H

#include "effect.h"
#include "fast_math.h"
#include "oversampler.h"
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * Moog MF-102 Moogerfooger Ring Modulator -- OTA-based four-quadrant multiplier.
 *
 * The MF-102 (1998, designed by Bob Moog) uses an OTA (Operational Transconductance
 * Amplifier) Gilbert cell topology for ring modulation. Unlike ideal mathematical
 * multiplication, the dual tanh() transfer function of the OTA differential pairs
 * creates soft saturation at signal extremes, adding harmonic coloration that is
 * the hallmark of the Moog analog character.
 *
 * ## Signal Flow
 *
 *   Input -> Drive preamp (tanh soft clip) -> Ring Mod Core -> Squelch gate
 *         -> Dry/Wet Mix -> Output
 *                                ^
 *                                |
 *                       Carrier VCO (sine + harmonics)
 *                                ^
 *                                |
 *                          LFO (sine/square)
 *
 * ## Ring Modulator Core (Gilbert Cell Equation)
 *
 *   output = Iee * tanh(Vaudio / (2*Vt)) * tanh(Vcarrier / (2*Vt))
 *
 * Where Vt = 26mV (thermal voltage at 25C). At guitar signal levels (~100-500mV),
 * the ratio Vsignal/Vt is 4-19, placing both signals firmly in the nonlinear tanh
 * region. This is the Moog character -- a pure mathematical multiply sounds sterile
 * by comparison.
 *
 * ## Carrier Oscillator
 *
 * Two frequency ranges (LO/HI rocker switch):
 *   - LO: 0.6 Hz to 80 Hz  (~7 octaves, sub-audio tremolo/AM territory)
 *   - HI: 30 Hz to 4000 Hz (~7 octaves, ring mod/bell tones)
 *
 * The carrier has slight harmonic impurity (2nd: ~1.5%, 3rd: ~0.8%) from the
 * analog triangle-to-sine waveshaper, contributing warmth.
 *
 * ## LFO
 *
 * 0.1 to 25 Hz, sine or square waveforms. Modulates carrier frequency via
 * exponential (V/oct) conversion: f = fBase * 2^(lfoAmount * lfoOutput),
 * where amount spans 0 to 3 octaves (1.5 octaves each direction).
 *
 * ## Anti-Aliasing
 *
 * 2x oversampling is used because ring modulation creates sum frequencies
 * (fCarrier + fInput) that can exceed Nyquist. At 48kHz sample rate with
 * max carrier of 4kHz and guitar harmonics extending to 10-12kHz, sum
 * frequencies reach ~16kHz -- safely below 48kHz oversampled Nyquist.
 *
 * ## Carrier Leakage & Squelch
 *
 * OTA component mismatch causes ~-46dB carrier bleed in the output. The
 * squelch circuit (envelope follower controlling an output VCA) attenuates
 * this leakage when no input signal is present.
 *
 * Parameter IDs:
 *   0 = Mix          (0..1, dry/wet crossfade)
 *   1 = Frequency    (0..1, logarithmic mapping across LO or HI range)
 *   2 = LFO Rate     (0..1, logarithmic mapping to 0.1..25 Hz)
 *   3 = LFO Amount   (0..1, modulation depth in octaves, 0..3)
 *   4 = Drive        (0..1, input gain 1x..10x)
 *   5 = Range        (0=LO 0.6-80Hz, 1=HI 30-4000Hz)
 *   6 = LFO Wave     (0=sine, 1=square)
 *
 * References:
 *   - Moog MF-102 schematic analysis
 *   - CA3080 / LM13700 OTA datasheets (Gilbert cell transfer function)
 *   - Bob Moog's original modular ring modulator designs (Bode Models 6401/6402)
 */
class MF102RingMod : public Effect {
public:
    MF102RingMod() = default;

    /**
     * Process audio through the MF-102 ring modulator. Real-time safe.
     *
     * Per-sample signal flow at the oversampled rate:
     *   1. Apply input drive (gain + tanh soft clip, models the preamp)
     *   2. Advance LFO phase, compute LFO output (sine or square)
     *   3. Compute modulated carrier frequency via V/oct exponential
     *   4. Advance carrier phase, generate sine carrier with harmonic impurity
     *   5. Ring mod core: dual tanh() Gilbert cell multiplication
     *   6. Add carrier leakage (~-46dB of carrier signal)
     *   7. Apply squelch (envelope follower gates output when no input)
     *   8. Crossfade dry/wet via mix parameter
     */
    void process(float* buffer, int numFrames) override {
        // Load all parameters atomically (block-level reads, not per-sample)
        const float mix = mix_.load(std::memory_order_relaxed);
        const float freq = freq_.load(std::memory_order_relaxed);
        const float lfoRate = lfoRate_.load(std::memory_order_relaxed);
        const float lfoAmount = lfoAmount_.load(std::memory_order_relaxed);
        const float drive = drive_.load(std::memory_order_relaxed);
        const float range = range_.load(std::memory_order_relaxed);
        const float lfoWave = lfoWave_.load(std::memory_order_relaxed);

        // Determine carrier frequency range from LO/HI switch
        const bool hiRange = (range >= 0.5f);
        const float minFreq = hiRange ? 30.0f : 0.6f;
        const float maxFreq = hiRange ? 4000.0f : 80.0f;

        // Logarithmic frequency mapping: f = minFreq * (maxFreq/minFreq)^knobValue
        // Pre-compute the log2 ratio for exp2 mapping
        const float log2Ratio = std::log2(maxFreq / minFreq);
        const float baseCarrierFreq = minFreq * fast_math::exp2_approx(freq * log2Ratio);

        // Drive maps 0..1 to 1x..10x input gain
        const float inputGain = 1.0f + drive * 9.0f;

        // LFO rate: logarithmic mapping 0..1 -> 0.1..25 Hz
        const float lfoFreq = 0.1f * fast_math::exp2_approx(
            lfoRate * std::log2(25.0f / 0.1f));

        // LFO modulation depth in octaves (0..3 total, ±1.5 each direction)
        const float modDepthOctaves = lfoAmount * 1.5f;

        const bool useSineLFO = (lfoWave < 0.5f);

        // Oversampled sample rate for phase increments
        const float osRate = static_cast<float>(sampleRate_) * 2.0f;

        // LFO phase increment (at base sample rate -- LFO runs at 1x)
        const float lfoPhaseInc = lfoFreq / static_cast<float>(sampleRate_);

        // --- Upsample input to 2x ---
        float* osBuffer = oversampler_.upsample(buffer, numFrames);
        const int osFrames = oversampler_.getOversampledSize(numFrames);

        // --- Process at oversampled rate ---
        for (int i = 0; i < osFrames; ++i) {
            const float input = osBuffer[i];

            // ============================================================
            // 1. Input drive stage (preamp with tanh soft clip)
            // ============================================================
            // Scale to approximate analog voltage range for OTA:
            //   Guitar ~100-500mV peak, Vt=26mV, so ratio = 4-19
            //   At unity drive, inputGain=1.0, audioScale maps normalized
            //   [-1,1] to ~[-10Vt, +10Vt] for realistic OTA nonlinearity.
            const float drivenInput = input * inputGain;
            const float clippedInput = static_cast<float>(
                fast_math::fast_tanh(static_cast<double>(drivenInput * 2.0f)));

            // ============================================================
            // 2. LFO (runs at 1x rate, sample-and-hold for 2x)
            // ============================================================
            // Advance LFO only on even samples (native rate)
            float lfoOutput;
            if ((i & 1) == 0) {
                if (useSineLFO) {
                    // Sine-like LFO using fast parabolic approximation
                    lfoOutput = fast_math::sin2pi(lfoPhase_);
                } else {
                    // Square LFO: +1 for first half, -1 for second half
                    lfoOutput = (lfoPhase_ < 0.5f) ? 1.0f : -1.0f;
                }
                lfoOutputHeld_ = lfoOutput;

                // Advance LFO phase
                lfoPhase_ += lfoPhaseInc;
                if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;
            } else {
                lfoOutput = lfoOutputHeld_;
            }

            // ============================================================
            // 3. Carrier frequency modulation (Moog V/oct exponential)
            // ============================================================
            // f_carrier = f_base * 2^(lfoAmount * lfoOutput)
            // lfoOutput in [-1, +1], modDepthOctaves in [0, 1.5]
            // Total sweep = 2 * modDepthOctaves = 0..3 octaves
            float carrierFreq = baseCarrierFreq;
            if (modDepthOctaves > 0.001f) {
                carrierFreq *= fast_math::exp2_approx(
                    modDepthOctaves * lfoOutput);
            }

            // Clamp to the selected range
            carrierFreq = clamp(carrierFreq, minFreq, maxFreq);

            // ============================================================
            // 4. Carrier oscillator (sine with harmonic impurity)
            // ============================================================
            // Phase increment at oversampled rate
            const float carrierPhaseInc = carrierFreq / osRate;

            carrierPhase_ += carrierPhaseInc;
            if (carrierPhase_ >= 1.0f) carrierPhase_ -= 1.0f;

            // Pure sine via fast parabolic approximation
            float carrier = fast_math::sin2pi(carrierPhase_);

            // Add harmonic impurity (analog VCO triangle-to-sine shaper residual)
            //   2nd harmonic: ~1.5% (-36 dB)
            //   3rd harmonic: ~0.8% (-42 dB)
            // Use double-angle identities to avoid separate phase tracking:
            //   sin(2*phase) = 2*sin(phase)*cos(phase)
            //   sin(3*phase) via fast_sin2pi(3*phase)
            // But for simplicity and CPU efficiency, just use the phase directly:
            float carrier2 = fast_math::sin2pi(carrierPhase_ * 2.0f);
            float carrier3 = fast_math::sin2pi(carrierPhase_ * 3.0f);
            carrier += 0.015f * carrier2 + 0.008f * carrier3;

            // ============================================================
            // 5. Ring modulator core (OTA Gilbert cell)
            // ============================================================
            // Gilbert cell: output = Iee * tanh(Vaudio/(2*Vt)) * tanh(Vcarrier/(2*Vt))
            //
            // Voltage scaling for realistic OTA nonlinearity:
            //   Vt = 26mV, 2*Vt = 52mV
            //   Audio voltage: clippedInput (already tanh'd, range [-1,1])
            //     maps to ~[-300mV, +300mV] with audioVoltageScale
            //   Carrier voltage: ~200mV peak typical OTA driving level
            //
            // The division by (2*Vt) creates the large argument ratios
            // that push tanh into its nonlinear region -- this IS the
            // Moog ring mod character.
            constexpr float kTwoVt = 2.0f * 0.026f;       // 52 mV
            constexpr float kInvTwoVt = 1.0f / kTwoVt;    // ~19.23
            constexpr float kAudioScale = 0.3f;            // ~300mV peak
            constexpr float kCarrierScale = 0.2f;          // ~200mV peak

            const float audioArg = clippedInput * kAudioScale * kInvTwoVt;
            const float carrierArg = carrier * kCarrierScale * kInvTwoVt;

            float ringOut = fast_math::fast_tanh(audioArg)
                          * fast_math::fast_tanh(carrierArg);

            // ============================================================
            // 6. Carrier leakage (OTA mismatch)
            // ============================================================
            // Real OTAs have slight mismatch causing carrier bleed-through.
            // ~-46 dB = 0.005 linear. The squelch reduces this at low input.
            constexpr float kCarrierLeak = 0.005f;
            ringOut += kCarrierLeak * carrier;

            // ============================================================
            // 7. Squelch (envelope-controlled output gate)
            // ============================================================
            // One-pole envelope follower on the absolute input level.
            // When input is silent, the squelch attenuates the output
            // (primarily to suppress carrier leakage).
            const float absInput = std::abs(input);
            if (absInput > envFollower_) {
                // Attack: fast (~1ms) to track transients
                envFollower_ += envAttackCoeff_ * (absInput - envFollower_);
            } else {
                // Release: slower (~50ms) for smooth decay
                envFollower_ += envReleaseCoeff_ * (absInput - envFollower_);
            }
            envFollower_ = fast_math::denormal_guard(envFollower_);

            // Smooth step: fully open above threshold, gradually close below
            constexpr float kSquelchThreshold = 0.005f;
            constexpr float kSquelchRange = 0.01f;
            float squelchGain = clamp(
                (envFollower_ - kSquelchThreshold) / kSquelchRange,
                0.0f, 1.0f);
            // Smooth the squelch gain to avoid clicks
            squelchGain = squelchGain * squelchGain * (3.0f - 2.0f * squelchGain);

            ringOut *= squelchGain;

            // ============================================================
            // 8. Output normalization
            // ============================================================
            // The dual tanh product peaks at ~1.0 * 1.0 = 1.0 for large
            // signals, but typical operation is below that. Scale to
            // match the input level approximately.
            constexpr float kOutputScale = 2.5f;
            ringOut *= kOutputScale;

            // ============================================================
            // 9. Mix (dry/wet crossfade)
            // ============================================================
            osBuffer[i] = (1.0f - mix) * input + mix * ringOut;
        }

        // --- Downsample back to native rate ---
        oversampler_.downsample(osBuffer, numFrames, buffer);
    }

    /**
     * Configure sample rate and pre-allocate all buffers.
     * Called once during audio stream setup, before process().
     */
    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;

        // Prepare 2x oversampler for maximum expected block size (2048 frames)
        oversampler_.prepare(2048);

        // Pre-compute envelope follower coefficients
        // Attack: ~1ms (fast transient tracking)
        // Release: ~50ms (smooth decay to avoid squelch clicks)
        const float fs = static_cast<float>(sampleRate) * 2.0f; // oversampled
        envAttackCoeff_ = 1.0f - std::exp(-1.0f / (0.001f * fs));
        envReleaseCoeff_ = 1.0f - std::exp(-1.0f / (0.050f * fs));
    }

    /**
     * Reset all internal state (oscillator phases, envelope follower, filters).
     * Called when audio stream restarts or effect is re-enabled.
     */
    void reset() override {
        carrierPhase_ = 0.0f;
        lfoPhase_ = 0.0f;
        lfoOutputHeld_ = 0.0f;
        envFollower_ = 0.0f;
        oversampler_.reset();
    }

    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0: mix_.store(clamp(value, 0.0f, 1.0f),
                               std::memory_order_relaxed); break;
            case 1: freq_.store(clamp(value, 0.0f, 1.0f),
                                std::memory_order_relaxed); break;
            case 2: lfoRate_.store(clamp(value, 0.0f, 1.0f),
                                   std::memory_order_relaxed); break;
            case 3: lfoAmount_.store(clamp(value, 0.0f, 1.0f),
                                     std::memory_order_relaxed); break;
            case 4: drive_.store(clamp(value, 0.0f, 1.0f),
                                 std::memory_order_relaxed); break;
            case 5: range_.store(clamp(value, 0.0f, 1.0f),
                                 std::memory_order_relaxed); break;
            case 6: lfoWave_.store(clamp(value, 0.0f, 1.0f),
                                   std::memory_order_relaxed); break;
            default: break;
        }
    }

    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return mix_.load(std::memory_order_relaxed);
            case 1: return freq_.load(std::memory_order_relaxed);
            case 2: return lfoRate_.load(std::memory_order_relaxed);
            case 3: return lfoAmount_.load(std::memory_order_relaxed);
            case 4: return drive_.load(std::memory_order_relaxed);
            case 5: return range_.load(std::memory_order_relaxed);
            case 6: return lfoWave_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    static float clamp(float v, float lo, float hi) {
        return std::max(lo, std::min(hi, v));
    }

    // =========================================================================
    // Parameters (atomic for lock-free UI <-> audio thread communication)
    // =========================================================================

    /** Dry/wet mix. 0.0 = fully dry, 1.0 = fully wet (ring modulated). */
    std::atomic<float> mix_{1.0f};

    /** Carrier frequency knob position. 0.0..1.0, logarithmic mapping
     *  across the selected range (LO: 0.6-80Hz, HI: 30-4000Hz). */
    std::atomic<float> freq_{0.5f};

    /** LFO rate knob. 0.0..1.0, maps logarithmically to 0.1-25 Hz. */
    std::atomic<float> lfoRate_{0.3f};

    /** LFO modulation depth. 0.0 = no modulation, 1.0 = 3 octaves total sweep. */
    std::atomic<float> lfoAmount_{0.0f};

    /** Input drive. 0.0 = unity gain (1x), 1.0 = maximum gain (10x). */
    std::atomic<float> drive_{0.3f};

    /** Frequency range switch. 0 = LO (0.6-80Hz), 1 = HI (30-4000Hz). */
    std::atomic<float> range_{1.0f};

    /** LFO waveform select. 0 = sine, 1 = square. */
    std::atomic<float> lfoWave_{0.0f};

    // =========================================================================
    // Internal state (audio thread only, not atomic)
    // =========================================================================

    int32_t sampleRate_ = 44100;

    /** Carrier oscillator phase accumulator [0, 1). */
    float carrierPhase_ = 0.0f;

    /** LFO phase accumulator [0, 1). */
    float lfoPhase_ = 0.0f;

    /** LFO output held for sample-and-hold at 1x rate during 2x oversampling. */
    float lfoOutputHeld_ = 0.0f;

    /** Input envelope follower level for squelch circuit. */
    float envFollower_ = 0.0f;

    /** One-pole envelope follower attack coefficient (~1ms). */
    float envAttackCoeff_ = 0.0f;

    /** One-pole envelope follower release coefficient (~50ms). */
    float envReleaseCoeff_ = 0.0f;

    /** 2x oversampler for anti-aliasing the ring modulation products. */
    Oversampler<2> oversampler_;
};

#endif // GUITAR_ENGINE_MF102_RING_MOD_H
