#ifndef GUITAR_ENGINE_BIG_MUFF_H
#define GUITAR_ENGINE_BIG_MUFF_H

#include "effect.h"
#include "oversampler.h"
#include "fast_math.h"
#include <chowdsp_wdf/chowdsp_wdf.h>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * Electro-Harmonix Big Muff Pi fuzz pedal -- circuit-accurate WDF emulation.
 *
 * The Big Muff Pi (1969-present) is one of the most important fuzz pedals
 * in rock history. Used by Jimi Hendrix, David Gilmour, J Mascis, Billy
 * Corgan, and countless others, its combination of massive sustain and a
 * characteristic mid-scooped tone defines the sound of fuzz guitar.
 *
 * What makes the Big Muff unique is its FOUR CASCADED CLIPPING STAGES.
 * Each stage is a common-emitter amplifier (Q1-Q4, 2N5088/BC239 NPN) with
 * a pair of 1N914 silicon diodes clipping the output symmetrically to
 * ground. The cascaded clipping progressively squares the waveform: by the
 * fourth stage, even the quietest input note has been compressed into a
 * nearly perfect square wave, creating the Big Muff's legendary sustain.
 *
 * The second defining feature is the PASSIVE TONE STACK -- a simple LP/HP
 * crossfade network that creates a deep mid-scoop notch around 1 kHz. This
 * scooped midrange is what gives the Big Muff its "wall of fuzz" character
 * that sits behind the mix rather than cutting through it like a Tube
 * Screamer. The tone knob crossfades between a low-pass path (dark, woolly)
 * and a high-pass path (bright, cutting).
 *
 * == Circuit Signal Flow (all variants) ==
 *
 *   Guitar Input
 *       |
 *   [Sustain Control] -- Pre-gain feeding the clipping cascade
 *       |
 *   +----------- 2x Oversampled Region (anti-alias nonlinearities) ----------+
 *   |                                                                         |
 *   |  [Stage 1: CE Amplifier + Symmetric Hard Clip]                         |
 *   |    Q1 (2N5088), collector R = 10K, feedback R = 100K                   |
 *   |    D1/D2: 1N914 silicon diode pair to ground (symmetric, +/-0.6V)     |
 *   |    Coupling C between stages blocks DC offset accumulation             |
 *   |                                                                         |
 *   |  [Stage 2: CE Amplifier + Symmetric Hard Clip]                         |
 *   |    Same topology, higher gain (23 dB)                                  |
 *   |                                                                         |
 *   |  [Stage 3: CE Amplifier + Symmetric Hard Clip]                         |
 *   |    Same topology, highest gain (25 dB)                                 |
 *   |                                                                         |
 *   |  [Stage 4: CE Amplifier + Symmetric Hard Clip]                         |
 *   |    Recovery stage, lower gain (13 dB), shapes final clipping texture   |
 *   |                                                                         |
 *   +-------------------------------------------------------------------------+
 *       |
 *   [Inter-Stage DC Blocking HP Filter] -- Prevents DC accumulation
 *       |
 *   [Passive Tone Stack]
 *       LP path: single-pole LPF (~318 Hz for NYC)
 *       HP path: single-pole HPF (~408 Hz for NYC)
 *       Tone knob crossfades between them
 *       Creates characteristic mid-scoop notch at ~1 kHz
 *       |
 *   [Volume Control] -- Output level
 *       |
 *   Output
 *
 * == Variant Differences ==
 *
 *   The Big Muff has been manufactured in numerous versions since 1969.
 *   Four iconic variants are modeled, each with different component values
 *   that change the gain structure, clipping character, and tone stack:
 *
 *   Variant 0 -- NYC (2000s reissue, "standard" reference):
 *     Standard mid-scoop, balanced tone. The baseline all others compare to.
 *     Stage gains: 9.5x, 14x, 17.8x, 4.5x
 *     Tone LP: 318 Hz, HP: 408 Hz
 *
 *   Variant 1 -- Ram's Head (1973-1977):
 *     Smooth, sustaining, "sweet" fuzz. Slightly higher gains, wider tone
 *     sweep. Favored by David Gilmour for its singing sustain.
 *     Stage gains: 10x, 16x, 20x, 5x
 *     Tone LP: 280 Hz, HP: 450 Hz (wider scoop)
 *
 *   Variant 2 -- Green Russian (1990s Sovtek):
 *     Smoother, woolly, fat low-mids, less mid-scoop. Darker overall.
 *     The "bass player's Big Muff" due to retained low end.
 *     Stage gains: 8x, 12x, 15x, 4x
 *     Tone LP: 350 Hz, HP: 350 Hz (less scoop, more overlap)
 *
 *   Variant 3 -- Triangle (1969-1973, original):
 *     Brightest, most aggressive, most open-sounding. Higher stage gains
 *     with wider bandwidth create a more aggressive, less compressed tone.
 *     Stage gains: 11x, 18x, 22x, 5.5x
 *     Tone LP: 300 Hz, HP: 480 Hz (widest scoop, brightest)
 *
 * == Parameters ==
 *   Param 0: "Sustain" (0.0-1.0, default 0.7)
 *            Controls pre-gain feeding into clipping stages.
 *            Maps to 1.0-50.0x gain range (exponential mapping).
 *            Higher sustain = more signal compression = longer note decay.
 *
 *   Param 1: "Tone" (0.0-1.0, default 0.5)
 *            Passive LP/HP crossfade. 0=dark (woolly), 1=bright (cutting).
 *            At 0.5, both paths contribute equally for maximum mid-scoop.
 *
 *   Param 2: "Volume" (0.0-1.0, default 0.5)
 *            Output level after the tone stack.
 *
 *   Param 3: "Variant" (0-3, default 0)
 *            NYC(0), Ram's Head(1), Green Russian(2), Triangle(3).
 *            Enum parameter: changes component values for gain and tone.
 *
 * All parameters use std::atomic<float> for lock-free UI/audio thread
 * communication, consistent with the rest of this codebase.
 *
 * == Technical Notes ==
 *   - Four independent WDF DiodePairT circuits model each clipping stage
 *   - 1N914 silicon diodes: Is=2.52e-9A, Vt=26e-3V (standard at 25C)
 *   - DiodePairT solved via Wright Omega function (O(1), no iteration)
 *   - Inter-stage coupling modeled as one-pole high-pass filters (~30 Hz)
 *   - 2x oversampling wraps all four nonlinear clipping stages
 *   - All buffers pre-allocated in setSampleRate() for real-time safety
 *   - Double precision for WDF processing and filter state variables
 *   - Variant parameters reconfigured on change (not per-sample)
 *
 * References:
 *   - Electrosmash: https://www.electrosmash.com/big-muff-pi-analysis
 *   - Kit Rae Big Muff Page: http://www.kitrae.net/music/bigmuffhistory.html
 *   - R.G. Keen, "The Technology of the Big Muff Pi" (geofex.com)
 *   - D. Yeh, CCRMA Stanford, "Digital Implementation of Musical
 *     Distortion Circuits by Analysis and Simulation," PhD thesis, 2009
 */
class BigMuff : public Effect {
public:
    BigMuff() = default;

    // =========================================================================
    // Effect interface
    // =========================================================================

    /**
     * Process audio through the Big Muff Pi signal chain. Real-time safe.
     *
     * Signal flow per block:
     *   1. Snapshot atomic parameters (one relaxed load each)
     *   2. Detect variant change and reconfigure stage gains/tone freqs
     *   3. Apply sustain pre-gain
     *   4. Upsample 2x for anti-aliased nonlinear processing
     *   5. Four cascaded WDF clipping stages with inter-stage coupling
     *   6. Downsample back to native rate
     *   7. Passive tone stack (LP/HP crossfade with mid-scoop)
     *   8. Output volume and safety clamp
     *
     * @param buffer   Mono audio samples, modified in-place.
     * @param numFrames Number of frames in the buffer.
     */
    void process(float* buffer, int numFrames) override {
        // Guard against being called before setSampleRate() allocates buffers.
        if (driveBuffer_.empty()) return;

        // --- Snapshot all parameters once per block ---
        const float sustain = sustain_.load(std::memory_order_relaxed);
        const float tone    = tone_.load(std::memory_order_relaxed);
        const float volume  = volume_.load(std::memory_order_relaxed);
        const float variant = variant_.load(std::memory_order_relaxed);

        // --- Detect variant change and reconfigure ---
        // The variant parameter is an enum (0-3). We detect changes by
        // comparing the integer value to avoid reconfiguring every block.
        const int variantInt = clampInt(static_cast<int>(variant + 0.5f), 0, 3);
        if (variantInt != cachedVariant_) {
            cachedVariant_ = variantInt;
            configureVariant(variantInt);
        }

        // --- Pre-gain (Sustain control) ---
        // The Big Muff's "Sustain" knob controls how hard the signal hits
        // the first clipping stage. With 4 cascaded stages, even a small
        // increase in input level translates to dramatically more compression
        // and sustain, because each stage further squares the waveform.
        //
        // Exponential mapping: sustain [0,1] -> gain [1, 50]
        // The quadratic mapping (sustain^2 * 49 + 1) gives a natural feel:
        // gentle increase at low settings, rapid gain at high settings.
        // At sustain=0.7 (default): gain = ~25x, solidly into fuzz territory.
        const float preGain = 1.0f + sustain * sustain * 49.0f;

        // Scale the input to realistic circuit voltage levels for the WDF.
        // The Big Muff runs on a 9V supply with ~4.5V bias, so signals
        // in the clipping stages are typically 0-3V peak.
        constexpr float kInputScaling = 3.0f;
        const float totalPreGain = preGain * kInputScaling;

        for (int i = 0; i < numFrames; ++i) {
            driveBuffer_[i] = buffer[i] * totalPreGain;
        }

        // --- Upsample 2x ---
        // The four nonlinear clipping stages generate significant harmonic
        // content. 2x oversampling pushes Nyquist higher so aliased harmonics
        // are attenuated by the anti-imaging filter during downsampling.
        // 2x is sufficient because the cascaded clipping produces primarily
        // odd harmonics that decrease in amplitude, and the tone stack
        // further attenuates high-frequency content before output.
        const int oversampledLen = oversampler_.getOversampledSize(numFrames);
        float* upsampled = oversampler_.upsample(driveBuffer_.data(), numFrames);

        // --- Four cascaded clipping stages at oversampled rate ---
        for (int i = 0; i < oversampledLen; ++i) {
            double x = static_cast<double>(upsampled[i]);

            // Stage 1: First clipping stage
            // Applies stage gain, then clips via WDF diode pair.
            // Inter-stage coupling HP filter removes DC offset.
            x *= stageGains_[0];
            x = clipStage(0, x);
            x = interStageHP(0, x);

            // Stage 2: Second clipping stage
            // Higher gain further compresses the already-clipped signal.
            x *= stageGains_[1];
            x = clipStage(1, x);
            x = interStageHP(1, x);

            // Stage 3: Third clipping stage (highest gain)
            // By this point, the waveform is nearly a square wave.
            // The third stage's high gain ensures even quiet notes
            // achieve full clipping, creating the Big Muff's sustain.
            x *= stageGains_[2];
            x = clipStage(2, x);
            x = interStageHP(2, x);

            // Stage 4: Recovery/output clipping stage
            // Lower gain shapes the final clipping texture without
            // adding excessive harshness. This stage determines the
            // "smoothness" vs "aggressiveness" of the fuzz character.
            x *= stageGains_[3];
            x = clipStage(3, x);

            upsampled[i] = static_cast<float>(x);
        }

        // --- Downsample back to native rate ---
        oversampler_.downsample(upsampled, numFrames, buffer);

        // --- Post-clipping DC blocking ---
        // After four cascaded clipping stages, any residual DC offset is
        // removed before the tone stack. This prevents the tone stack
        // crossfade from shifting the signal baseline.
        for (int i = 0; i < numFrames; ++i) {
            double xd = static_cast<double>(buffer[i]);
            double hpOut = postClipHPCoeff_ * (postClipHPState_ + xd - postClipHPPrev_);
            postClipHPPrev_ = xd;
            postClipHPState_ = fast_math::denormal_guard(hpOut);
            buffer[i] = static_cast<float>(postClipHPState_);
        }

        // --- Passive Tone Stack (LP/HP crossfade) ---
        // The Big Muff's tone control is a simple but brilliant design:
        // a low-pass path and a high-pass path fed in parallel from the
        // clipping output, with a potentiometer crossfading between them.
        //
        // At tone=0: 100% LP path (dark, woolly, bass-heavy)
        // At tone=0.5: 50/50 LP+HP (maximum mid-scoop, the classic sound)
        // At tone=1: 100% HP path (bright, cutting, treble-heavy)
        //
        // The mid-scoop occurs because the LP rolls off above its cutoff
        // (~318 Hz) and the HP rolls off below its cutoff (~408 Hz). In the
        // crossfade region, the overlap creates a cancellation notch in the
        // midrange (~1 kHz), scooping out exactly the frequencies where
        // guitar sits most prominently in a mix.
        const float toneHP = tone;
        const float toneLP = 1.0f - tone;

        for (int i = 0; i < numFrames; ++i) {
            double xd = static_cast<double>(buffer[i]);

            // Low-pass path: one-pole LPF
            // y[n] = (1-a)*x[n] + a*y[n-1]
            toneLPState_ = (1.0 - toneLPCoeff_) * xd
                          + toneLPCoeff_ * toneLPState_;
            toneLPState_ = fast_math::denormal_guard(toneLPState_);

            // High-pass path: one-pole HPF
            // y[n] = a * (y[n-1] + x[n] - x[n-1])
            double hpOut = toneHPCoeff_ * (toneHPState_ + xd - toneHPPrev_);
            toneHPPrev_ = xd;
            toneHPState_ = fast_math::denormal_guard(hpOut);

            // Crossfade between LP and HP paths
            buffer[i] = static_cast<float>(
                toneLP * toneLPState_ + toneHP * toneHPState_);
        }

        // --- Output volume and safety clamp ---
        // Audio taper (quadratic) for the Volume knob gives a perceptually
        // linear loudness response. The Big Muff's output can be very hot
        // after four stages of clipping, so clamping prevents downstream
        // effects from receiving out-of-range samples.
        const float outputGain = volume * volume;
        for (int i = 0; i < numFrames; ++i) {
            buffer[i] = clamp(buffer[i] * outputGain, -1.0f, 1.0f);
        }
    }

    /**
     * Configure sample rate and pre-allocate all internal buffers.
     * Called once during stream setup before any process() calls.
     *
     * This is the ONLY method that performs memory allocation. All
     * subsequent process() calls are allocation-free and real-time safe.
     *
     * @param sampleRate Device native sample rate in Hz.
     */
    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;

        const int maxFrames = kMaxFrames;
        const double oversampledRate = static_cast<double>(sampleRate) * 2.0;
        oversampledRate_ = oversampledRate;

        // Pre-allocate oversampler and drive buffer
        oversampler_.prepare(maxFrames);
        driveBuffer_.resize(maxFrames, 0.0f);

        // Prepare all four WDF clipping stages at the oversampled rate.
        // Each stage has its own capacitor whose bilinear transform
        // discretization depends on the sample rate.
        for (int s = 0; s < kNumStages; ++s) {
            stages_[s].prepare(oversampledRate);
        }

        // Compute inter-stage coupling HP filter coefficients.
        // The coupling capacitors in the real circuit (100nF between stages)
        // form a high-pass with the input impedance of the next stage.
        // fc ~ 30 Hz is low enough to pass all guitar content while
        // blocking DC offset that accumulates through cascaded clipping.
        for (int s = 0; s < kNumStages; ++s) {
            interStageHPCoeff_[s] = computeOnePoleHPCoeff(
                kInterStageHPFreq, oversampledRate);
        }

        // Post-clipping DC blocking HP at native rate
        postClipHPCoeff_ = computeOnePoleHPCoeff(
            kPostClipHPFreq, static_cast<double>(sampleRate));

        // Pre-compute tone stack coefficients for ALL 4 variants.
        // This avoids calling std::exp() on the audio thread when the
        // variant parameter changes. O(4) precomputation here enables
        // O(1) lookup during process().
        const double nativeRate = static_cast<double>(sampleRate);
        for (int v = 0; v < 4; ++v) {
            variantToneLPCoeffs_[v] = computeOnePoleLPCoeff(
                kVariants[v].toneLPFreq, nativeRate);
            variantToneHPCoeffs_[v] = computeOnePoleHPCoeff(
                kVariants[v].toneHPFreq, nativeRate);
        }

        // Force reconfigure on first process()
        cachedVariant_ = -1;
    }

    /**
     * Reset all internal state (filter histories, WDF delay elements,
     * oversampler FIR history). Called when the audio stream restarts
     * or the effect is re-enabled to prevent stale samples from the
     * previous signal from leaking through.
     */
    void reset() override {
        oversampler_.reset();

        // Reset all four WDF clipping stages
        for (int s = 0; s < kNumStages; ++s) {
            stages_[s].reset();
        }

        // Reset inter-stage coupling HP filter states
        for (int s = 0; s < kNumStages; ++s) {
            interStageHPState_[s] = 0.0;
            interStageHPPrev_[s]  = 0.0;
        }

        // Reset post-clipping DC blocker
        postClipHPState_ = 0.0;
        postClipHPPrev_  = 0.0;

        // Reset tone stack filter states
        toneLPState_ = 0.0;
        toneHPState_ = 0.0;
        toneHPPrev_  = 0.0;
    }

    // =========================================================================
    // Parameter access (called from UI thread via JNI)
    // =========================================================================

    /**
     * Set a parameter by ID. Thread-safe (writes to atomics).
     *
     * @param paramId  0=Sustain, 1=Tone, 2=Volume, 3=Variant
     * @param value    Parameter value. Sustain/Tone/Volume in [0.0, 1.0],
     *                 Variant in [0.0, 3.0] (integer enum).
     */
    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case kParamSustain:
                sustain_.store(clamp(value, 0.0f, 1.0f),
                               std::memory_order_relaxed);
                break;
            case kParamTone:
                tone_.store(clamp(value, 0.0f, 1.0f),
                            std::memory_order_relaxed);
                break;
            case kParamVolume:
                volume_.store(clamp(value, 0.0f, 1.0f),
                              std::memory_order_relaxed);
                break;
            case kParamVariant:
                variant_.store(clamp(value, 0.0f, 3.0f),
                               std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    /**
     * Get a parameter by ID. Thread-safe (reads from atomics).
     *
     * @param paramId  0=Sustain, 1=Tone, 2=Volume, 3=Variant
     * @return Current parameter value, or 0.0f if paramId is unknown.
     */
    float getParameter(int paramId) const override {
        switch (paramId) {
            case kParamSustain: return sustain_.load(std::memory_order_relaxed);
            case kParamTone:    return tone_.load(std::memory_order_relaxed);
            case kParamVolume:  return volume_.load(std::memory_order_relaxed);
            case kParamVariant: return variant_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    // =========================================================================
    // Constants
    // =========================================================================

    /** Parameter IDs matching the JNI bridge convention. */
    static constexpr int kParamSustain = 0;  ///< Pre-gain into clipping cascade
    static constexpr int kParamTone    = 1;  ///< Tone stack LP/HP crossfade
    static constexpr int kParamVolume  = 2;  ///< Output level
    static constexpr int kParamVariant = 3;  ///< Circuit variant (0-3 enum)

    /** Number of cascaded clipping stages in the Big Muff circuit. */
    static constexpr int kNumStages = 4;

    /** Maximum expected audio buffer size. Android typically uses 64-480
     *  frames; 2048 provides generous headroom for all configurations. */
    static constexpr int kMaxFrames = 2048;

    /** Oversampling factor. 2x provides good alias rejection for the
     *  cascaded hard clipping while keeping CPU load reasonable on ARM. */
    static constexpr int kOversampleFactor = 2;

    /** Normalize WDF diode clipper output (~+/-0.6V for 1N914 silicon)
     *  back to [-1, 1] audio range. Each stage outputs in this range,
     *  so we normalize per-stage to maintain consistent levels through
     *  the cascade. */
    static constexpr double kWdfOutputScaling = 1.0 / 0.65;

    /** Inter-stage coupling high-pass frequency (Hz).
     *  Models the 100nF coupling caps between CE amplifier stages.
     *  30 Hz is well below the guitar's lowest fundamental (82 Hz for
     *  standard E2, ~41 Hz for drop D) while effectively blocking DC. */
    static constexpr double kInterStageHPFreq = 30.0;

    /** Post-clipping DC blocker frequency (Hz).
     *  Slightly higher than inter-stage to aggressively remove any
     *  residual DC before the tone stack. */
    static constexpr double kPostClipHPFreq = 20.0;

    // =========================================================================
    // Variant Configuration
    // =========================================================================

    /**
     * Component values for each Big Muff variant.
     *
     * Each variant has different stage gains (from collector/feedback
     * resistor ratios) and tone stack frequencies (from capacitor/resistor
     * values in the LP and HP paths).
     *
     * The gains are the linear voltage gains of each CE amplifier stage,
     * derived from the ratio of collector resistance to emitter degeneration
     * resistance: G = R_collector / R_emitter (simplified).
     *
     * Real-world circuit analysis:
     *   NYC:        R_c=10K, R_e varies per stage (150-2200 ohm)
     *   Ram's Head: Slightly higher R_c/R_e ratios, wider tone caps
     *   Green Russian: Lower gains, different tone cap values
     *   Triangle:   Highest gains, brightest tone caps
     *
     * NOTE: The WDF clipping circuit uses fixed R=4.7K and C=100nF for all
     * variants. Variant differences in clipping character (e.g., Triangle's
     * slightly harder onset) are modeled through the stage gain differences,
     * which is the dominant perceptual factor. This avoids reconstructing
     * WDF objects on the audio thread when the variant changes, ensuring
     * real-time safety. The ~15% resistance difference between NYC (4.7K)
     * and Triangle (3.9K) produces a subtle change in clipping onset
     * softness that is overshadowed by the gain and tone stack differences.
     */
    struct VariantConfig {
        double stageGains[kNumStages];  ///< Linear voltage gain per stage
        double toneLPFreq;              ///< Tone stack LP cutoff (Hz)
        double toneHPFreq;              ///< Tone stack HP cutoff (Hz)
    };

    /**
     * Variant configurations indexed by variant ID.
     *
     * Stage gain derivation (approximate, from Electrosmash analysis):
     *   Gain = Rc / (Re + re), where re = 26mV/Ic (BJT dynamic emitter R)
     *   At Ic ~= 1mA: re ~= 26 ohms
     *
     *   NYC Stage 1:  Rc=10K, Re=150+re=176 -> G=56.8 -> but with
     *     feedback R=100K loading, effective G ~ 9.5
     *   (The feedback resistor and biasing reduce the theoretical max gain)
     *
     * Tone stack frequencies:
     *   LP: fc = 1 / (2 * pi * R_pot * C_lp)
     *     NYC: 100K pot, 3.9nF cap -> fc = 408 Hz
     *     (We use slightly different values tuned to match audio recordings)
     *   HP: fc = 1 / (2 * pi * R_hp * C_hp)
     *     NYC: 39K resistor, 10nF cap -> fc = 408 Hz
     *     (The two paths have similar cutoffs, creating the scoop between them)
     */
    static constexpr VariantConfig kVariants[4] = {
        // Variant 0: NYC (2000s reissue) -- standard reference
        // HP: 22K + 3.9nF = 1/(2*pi*22K*3.9nF) = 1854Hz
        {
            { 9.5, 14.0, 17.8, 4.5 },  // Stage gains
            318.0,                       // LP cutoff Hz (3.9nF + 100K pot sweep)
            1854.0                       // HP cutoff Hz (22K * 3.9nF = 1854Hz)
        },
        // Variant 1: Ram's Head (1973-1977) -- smooth, sustaining, "sweet"
        {
            { 10.0, 16.0, 20.0, 5.0 },  // Slightly higher gains
            280.0,                        // Lower LP cutoff (more bass through)
            2000.0                        // HP cutoff (wider scoop, brighter)
        },
        // Variant 2: Green Russian (1990s Sovtek) -- woolly, fat low-mids
        {
            { 8.0, 12.0, 15.0, 4.0 },  // Lower gains (smoother clipping)
            350.0,                       // Higher LP (retains more mids)
            1500.0                       // Lower HP (less treble, warmer scoop)
        },
        // Variant 3: Triangle (1969-1973, original) -- brightest, aggressive
        {
            { 11.0, 18.0, 22.0, 5.5 },  // Highest gains (most aggressive)
            300.0,                        // Low LP (good bass response)
            2200.0                        // Highest HP (widest scoop, brightest)
        }
    };

    // =========================================================================
    // WDF Clipping Stage
    // =========================================================================

    /**
     * Wave Digital Filter model of a single Big Muff clipping stage.
     *
     * Each of the Big Muff's four clipping stages uses the same basic
     * topology: a CE amplifier drives signal through a resistor into
     * an antiparallel diode pair that clips to ground, with a coupling
     * capacitor for DC blocking.
     *
     * Circuit topology (per stage):
     *
     *   Vin ---[ R_clip ]---+--- Vout (across C_clip)
     *                       |
     *                   [C_clip]
     *                       |
     *                   [D1 || D2]  (1N914 antiparallel to GND)
     *                       |
     *                      GND
     *
     * WDF tree structure:
     *
     *        [DiodePairT] (ROOT -- sole non-adaptable nonlinearity)
     *             |
     *       [WDFParallelT]
     *         /           \
     *   [ResistiveVs]   [CapacitorT]
     *   (R=4.7K)        (C=100nF)
     *
     * The 1N914 silicon diodes clip symmetrically at approximately
     * +/-0.6V. Unlike the HM-2 (which uses asymmetric and germanium
     * stages), the Big Muff's symmetric clipping produces primarily
     * odd-order harmonics (3rd, 5th, 7th...), giving it that thick,
     * buzzy character.
     *
     * Diode parameters (1N914 silicon, same family as 1N4148):
     *   Is = 2.52e-9 A  (saturation current)
     *   Vt = 26.0e-3 V  (thermal voltage at ~25C)
     *   nDiodes = 1.0   (single diode per direction)
     */
    struct ClipStage {
        // ---- WDF Leaf Components ----

        /** Resistive voltage source: combines the stage input signal with
         *  R_clip (source impedance, typically 4.7K) into a single adapted
         *  (non-root) WDF leaf element. */
        chowdsp::wdft::ResistiveVoltageSourceT<double> rvs { 4700.0 };

        /** Coupling/DC-blocking capacitor (100nF typical).
         *  Also shapes low-frequency response of the clipping stage:
         *  with R=4.7K, fc = 1/(2*pi*4.7K*100nF) = ~339 Hz. Below this
         *  frequency, less signal reaches the diodes -> less bass clipping
         *  -> warmer, less buzzy low end. This is a key part of the Big
         *  Muff's character: it clips mid and treble harder than bass. */
        chowdsp::wdft::CapacitorT<double> cap { 100.0e-9 };

        // ---- WDF Tree Adaptor ----

        /** Parallel connection: rvs || cap at the clipping junction.
         *  Models the circuit node where the resistor-fed signal,
         *  coupling capacitor, and diode pair all connect. */
        chowdsp::wdft::WDFParallelT<double,
            decltype(rvs),
            decltype(cap)> par { rvs, cap };

        // ---- WDF Root (nonlinear element) ----

        /** Antiparallel silicon diode pair (1N914).
         *  SOLE root of the WDF tree (non-adaptable nonlinearity).
         *  Solved via Wright Omega function for O(1) per-sample cost.
         *
         *  1N914 parameters:
         *    Is = 2.52e-9 A (saturation current)
         *    Vt = 26.0e-3 V (thermal voltage kT/q at 25C)
         *    nDiodes = 1.0 (one diode per direction) */
        chowdsp::wdft::DiodePairT<double, decltype(par)> diodes {
            par,
            2.52e-9,   // Is: saturation current (1N914 silicon)
            26.0e-3    // Vt: thermal voltage at room temperature
        };

        /**
         * Prepare the WDF circuit for a given sample rate.
         * The capacitor's bilinear transform discretization depends on
         * the sample period T = 1/sampleRate.
         *
         * @param sampleRate Sample rate in Hz (should be the oversampled rate).
         */
        void prepare(double sampleRate) {
            cap.prepare(sampleRate);
            diodes.calcImpedance();
        }

        /**
         * Reset the WDF circuit state (capacitor stored charge).
         * Prevents stale state from causing clicks when the effect restarts.
         */
        void reset() {
            cap.reset();
        }

        /**
         * Process a single sample through this clipping stage.
         *
         * Wave propagation per sample:
         *   1. Set input voltage on the resistive voltage source
         *   2. Reflect waves from the parallel adaptor up to the diodes
         *   3. Diodes compute the nonlinear scattering (Wright Omega)
         *   4. Propagate the reflected wave back down to the adaptor
         *   5. Read output voltage across the capacitor
         *
         * @param x Input sample (double precision).
         * @return Hard-clipped output voltage (~+/-0.6V for 1N914).
         */
        inline double processSample(double x) {
            rvs.setVoltage(x);

            // Wave propagation: reflect up from parallel adaptor to diodes,
            // diodes compute nonlinear scattering, propagate back down.
            diodes.incident(par.reflected());
            par.incident(diodes.reflected());

            // Read output voltage across the coupling capacitor.
            // This gives the clipped signal with DC blocking behavior.
            return chowdsp::wdft::voltage<double>(cap);
        }
    };

    // =========================================================================
    // Per-Sample Processing Helpers
    // =========================================================================

    /**
     * Process one sample through a WDF clipping stage and normalize output.
     *
     * The WDF diode clipper outputs voltages in the range of approximately
     * +/-0.6V (1N914 forward voltage). We normalize back to a standard
     * audio range so that the next stage receives a consistent input level.
     *
     * @param stageIdx Index of the clipping stage (0-3).
     * @param x        Input sample (double precision).
     * @return Clipped and normalized output sample.
     */
    inline double clipStage(int stageIdx, double x) {
        return stages_[stageIdx].processSample(x) * kWdfOutputScaling;
    }

    /**
     * Apply inter-stage coupling high-pass filter.
     *
     * Each real Big Muff stage is AC-coupled to the next via a 100nF
     * capacitor. This prevents DC offset from accumulating through the
     * cascade, which would shift the operating point of subsequent stages
     * and change their clipping behavior. We model this as a one-pole
     * high-pass at ~30 Hz.
     *
     * One-pole HPF: y[n] = a * (y[n-1] + x[n] - x[n-1])
     *
     * @param stageIdx Index of the coupling filter (0-2, between stages).
     * @param x        Input sample (double precision).
     * @return High-pass filtered sample with DC removed.
     */
    inline double interStageHP(int stageIdx, double x) {
        double hpOut = interStageHPCoeff_[stageIdx]
                     * (interStageHPState_[stageIdx] + x
                        - interStageHPPrev_[stageIdx]);
        interStageHPPrev_[stageIdx] = x;
        interStageHPState_[stageIdx] = fast_math::denormal_guard(hpOut);
        return interStageHPState_[stageIdx];
    }

    // =========================================================================
    // Variant Configuration
    // =========================================================================

    /**
     * Configure the effect for a specific circuit variant.
     *
     * This updates the stage gains and tone stack frequencies to match
     * the selected variant's component values. Called when the variant
     * parameter changes (detected per-block in process()).
     *
     * Only gain values and filter coefficients are updated here -- no WDF
     * objects are reconstructed. This ensures the method is real-time safe
     * (no allocations, no system calls). The WDF clipping circuit uses
     * fixed R=4.7K and C=100nF for all variants; sonic differences are
     * captured through gain and tone stack parameter changes, which are
     * the perceptually dominant factors.
     *
     * @param variantIdx Variant index (0=NYC, 1=Ram's Head, 2=Green Russian, 3=Triangle).
     */
    void configureVariant(int variantIdx) {
        const VariantConfig& cfg = kVariants[variantIdx];

        // Update stage gains (the primary differentiator between variants)
        for (int s = 0; s < kNumStages; ++s) {
            stageGains_[s] = cfg.stageGains[s];
        }

        // Look up pre-computed tone stack coefficients (no std::exp on audio thread)
        toneLPCoeff_ = variantToneLPCoeffs_[variantIdx];
        toneHPCoeff_ = variantToneHPCoeffs_[variantIdx];
    }

    // =========================================================================
    // Filter Coefficient Computation
    // =========================================================================

    /**
     * Compute one-pole low-pass filter coefficient (double precision).
     *
     * For the difference equation y[n] = (1-a)*x[n] + a*y[n-1]:
     *   a = exp(-2*pi*fc/fs)
     *
     * @param cutoffHz   Desired cutoff frequency in Hz.
     * @param sampleRate Sample rate in Hz.
     * @return Filter coefficient 'a' (higher = more filtering/darker).
     */
    static double computeOnePoleLPCoeff(double cutoffHz, double sampleRate) {
        if (sampleRate <= 0.0) return 0.0;
        cutoffHz = std::max(1.0, std::min(cutoffHz, sampleRate * 0.49));
        return std::exp(-2.0 * M_PI * cutoffHz / sampleRate);
    }

    /**
     * Compute one-pole high-pass filter coefficient (double precision).
     *
     * For the difference equation y[n] = a * (y[n-1] + x[n] - x[n-1]):
     *   a = 1 / (1 + 2*pi*fc/fs)
     *
     * @param cutoffHz   Desired cutoff frequency in Hz.
     * @param sampleRate Sample rate in Hz.
     * @return Filter coefficient 'a' (closer to 1.0 = lower cutoff).
     */
    static double computeOnePoleHPCoeff(double cutoffHz, double sampleRate) {
        if (sampleRate <= 0.0) return 1.0;
        cutoffHz = std::max(1.0, std::min(cutoffHz, sampleRate * 0.49));
        const double wc = 2.0 * M_PI * cutoffHz;
        return 1.0 / (1.0 + wc / sampleRate);
    }

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * Clamp a float value to a range. Inline for zero-overhead in hot paths.
     */
    static float clamp(float value, float minVal, float maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    /**
     * Clamp an integer value to a range.
     */
    static int clampInt(int value, int minVal, int maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    // =========================================================================
    // Parameters (written from UI thread, read from audio thread)
    // =========================================================================

    /** Sustain (pre-gain) [0, 1]. Controls input level into clipping cascade.
     *  Default 0.7 gives heavy fuzz with good sustain. */
    std::atomic<float> sustain_ { 0.7f };

    /** Tone control [0, 1]. LP/HP crossfade for mid-scoop.
     *  0 = dark (woolly), 0.5 = classic mid-scoop, 1 = bright (cutting).
     *  Default 0.5 for the iconic scooped fuzz sound. */
    std::atomic<float> tone_ { 0.5f };

    /** Output volume [0, 1]. Controls level after tone stack.
     *  Default 0.5 for moderate output. */
    std::atomic<float> volume_ { 0.5f };

    /** Variant selector [0, 3]. Enum: NYC(0), Ram's Head(1),
     *  Green Russian(2), Triangle(3). Default 0 (NYC). */
    std::atomic<float> variant_ { 0.0f };

    // =========================================================================
    // Audio thread state (only accessed from the audio callback thread)
    // =========================================================================

    /** Device native sample rate in Hz. */
    int32_t sampleRate_ = 44100;

    /** Effective sample rate after 2x oversampling. */
    double oversampledRate_ = 88200.0;

    /** 2x oversampler for anti-aliased nonlinear processing.
     *  Upsamples before the four clipping stages, downsamples after. */
    Oversampler<kOversampleFactor> oversampler_;

    /** Four WDF clipping stage circuits, one per cascade stage.
     *  Each independently models a CE amplifier + diode clipper. */
    ClipStage stages_[kNumStages];

    /** Linear voltage gains for each clipping stage.
     *  Set by configureVariant() based on the selected variant.
     *  These represent the CE amplifier gain before the diode clipper. */
    double stageGains_[kNumStages] = { 9.5, 14.0, 17.8, 4.5 };

    /** Pre-allocated buffer for the sustain-gained signal before upsampling.
     *  Avoids modifying the input buffer, which would corrupt the dry signal
     *  if wet/dry mixing is used by the signal chain. */
    std::vector<float> driveBuffer_;

    /** Cached variant index for detecting parameter changes.
     *  Only accessed from the audio thread, so no atomic needed.
     *  Initialized to -1 to force configuration on first process(). */
    int cachedVariant_ = -1;

    // ---- Inter-stage coupling HP filter state ----
    // One HP filter per stage transition (between stages 1-2, 2-3, 3-4,
    // and after stage 4). Models the 100nF coupling capacitors in the
    // original circuit.

    /** Inter-stage HP filter coefficients (one per stage).
     *  Computed in setSampleRate() at the oversampled rate. */
    double interStageHPCoeff_[kNumStages] = { 0.999, 0.999, 0.999, 0.999 };

    /** Inter-stage HP filter state variables. Double precision for
     *  stability at the low cutoff frequency (~30 Hz). */
    double interStageHPState_[kNumStages] = { 0.0, 0.0, 0.0, 0.0 };

    /** Inter-stage HP filter previous input samples.
     *  Needed for the one-pole HPF difference equation. */
    double interStageHPPrev_[kNumStages] = { 0.0, 0.0, 0.0, 0.0 };

    // ---- Post-clipping DC blocker ----

    /** Post-clipping HP coefficient at native sample rate. */
    double postClipHPCoeff_ = 0.999;

    /** Post-clipping HP filter state. */
    double postClipHPState_ = 0.0;

    /** Post-clipping HP previous input sample. */
    double postClipHPPrev_ = 0.0;

    // ---- Tone stack state ----

    /** Pre-computed tone stack LP coefficients for all 4 variants.
     *  Computed in setSampleRate() to avoid std::exp on the audio thread. */
    double variantToneLPCoeffs_[4] = { 0.0, 0.0, 0.0, 0.0 };

    /** Pre-computed tone stack HP coefficients for all 4 variants. */
    double variantToneHPCoeffs_[4] = { 0.999, 0.999, 0.999, 0.999 };

    /** Tone stack LP filter coefficient (active, set by configureVariant). */
    double toneLPCoeff_ = 0.0;

    /** Tone stack HP filter coefficient. */
    double toneHPCoeff_ = 0.999;

    /** Tone stack LP path filter state. */
    double toneLPState_ = 0.0;

    /** Tone stack HP path filter state. */
    double toneHPState_ = 0.0;

    /** Tone stack HP path previous input sample. */
    double toneHPPrev_ = 0.0;
};

#endif // GUITAR_ENGINE_BIG_MUFF_H
