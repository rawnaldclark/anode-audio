#ifndef GUITAR_ENGINE_FUZZ_FACE_H
#define GUITAR_ENGINE_FUZZ_FACE_H

#include "effect.h"
#include "oversampler.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * Circuit-accurate emulation of the Dallas Arbiter Fuzz Face (PNP Germanium).
 *
 * The Fuzz Face (1966) is one of the most revered fuzz pedals ever made,
 * famously used by Jimi Hendrix, David Gilmour, and Eric Johnson. Its sound
 * is defined by three characteristics that set it apart from all other fuzzes:
 *
 *   1. GERMANIUM TRANSISTORS (AC128 PNP):
 *      Germanium has a saturation current ~2,000x higher than silicon
 *      (Is=5uA effective vs. 2.52nA). This means the transition from clean to
 *      clipping is extremely gradual and smooth, producing round, warm
 *      distortion rich in even harmonics. Silicon fuzzes (BC108) clip
 *      harder and sound harsher by comparison.
 *
 *   2. GUITAR VOLUME CLEANUP:
 *      The Fuzz Face's low input impedance (~10K, set by R1=33K || guitar
 *      pickup impedance) interacts heavily with the guitar's volume pot.
 *      Rolling the guitar volume down from 10 to 7-8 reduces the signal
 *      level entering the fuzz AND shifts the bias point of Q1, causing
 *      the fuzz to clean up into a warm, slightly compressed overdrive.
 *      This is THE defining Fuzz Face behavior -- players use the guitar
 *      volume knob as a gain control, going from full fuzz to clean by
 *      rolling back. No other fuzz design does this as naturally.
 *
 *   3. TWO-TRANSISTOR FEEDBACK TOPOLOGY:
 *      Q1 (low beta ~70-80) provides moderate gain and feeds Q2 (high
 *      beta ~110-130) which provides the main clipping. R4 (100K)
 *      provides DC feedback from Q2's collector to Q1's base, stabilizing
 *      the bias. The FUZZ pot (1K) controls the amount of AC signal
 *      bypassed around Q2's emitter, effectively controlling the amount
 *      of distortion. At low fuzz settings, more signal is shunted to
 *      ground through the fuzz pot, reducing Q2's gain and creating the
 *      characteristic sputtery, gated fuzz at the bottom of the range.
 *
 * Circuit topology modeled (original Dallas Arbiter PNP schematic):
 *
 *   Guitar --[C1=2.2uF]--+--[R1=33K]-- GND
 *                         |
 *                    [Q1: AC128 PNP, beta~75]
 *                    (common-emitter, gain ~6x with feedback)
 *                         |
 *                    [R3=8.2K collector load]
 *                         |
 *                    [Q2: AC128 PNP, beta~120]
 *                    (common-emitter, high gain)
 *                         |
 *                    [R2=470 ohm collector load]
 *                         |
 *              +----[R4=100K feedback]----+  (Q2 collector -> Q1 base)
 *              |                          |
 *              +----[Rfuzz=1K pot]-- GND  |
 *                   (emitter bypass)      |
 *                         |               |
 *                    [C2=20uF emitter bypass on Q2]
 *                         |
 *                    [C3=10nF]--[Rvol=500K]-- Output
 *
 * Implementation Strategy (direct asymmetric Ge transistor models):
 *
 *   Previous versions used WDF DiodePair elements to model the transistors.
 *   A DiodePair is two symmetric passive clippers — it produces odd harmonics
 *   from symmetric clipping, not the even harmonics characteristic of real
 *   transistor amplifier stages. Real CE-configured germanium transistors
 *   clip asymmetrically: soft toward cutoff, hard toward saturation.
 *
 *   This version uses direct asymmetric clipping functions for each stage:
 *
 *   Stage 1 (Q1): Common-emitter Ge amp (AC128, hFE~75)
 *     - Moderate gain (~6x with R4 feedback reduction)
 *     - Soft cutoff on positive excursion (transistor runs out of base current)
 *     - Harder saturation on negative excursion (Vce_sat ~ 0.1V)
 *     - Produces characteristic even-harmonic warmth
 *
 *   Stage 2 (Q2): Common-emitter Ge amp (AC128, hFE~120)
 *     - High gain controlled by Fuzz pot (4-25x range)
 *     - Stronger asymmetric clipping than Q1
 *     - Bias-dependent gating: at low fuzz, Q2 is biased near cutoff
 *       so weak signals (note decay) drop below the bias threshold and
 *       get gated — this is the classic sputtery/velcro Fuzz Face character
 *
 *   The R4 (100K) feedback is modeled by reducing Q1's effective gain
 *   as a function of the Fuzz parameter, approximating the real circuit's
 *   negative feedback loop that stabilizes bias and reduces gain at low
 *   fuzz settings.
 *
 * Anti-aliasing:
 *   2x oversampling wraps both nonlinear clipping stages. Germanium's
 *   soft clipping generates fewer high-order harmonics than silicon, so
 *   2x is sufficient for alias rejection on mobile.
 *
 * Parameter IDs (for JNI access):
 *   0 = Fuzz          (0.0 to 1.0) - Feedback/clipping intensity
 *   1 = Volume        (0.0 to 1.0) - Output level
 *   2 = Guitar Volume (0.0 to 1.0) - Input impedance interaction
 *
 * References:
 *   - Electrosmash: https://www.electrosmash.com/fuzz-face
 *   - R.G. Keen, "The Technology of the Fuzz Face" (geofex.com)
 *   - D. Yeh, "Digital Implementation of Musical Distortion Circuits by
 *     Analysis and Simulation," PhD thesis, CCRMA, Stanford, 2009
 *   - Original Dallas Arbiter Fuzz Face schematic (1966)
 */
class FuzzFace : public Effect {
public:
    FuzzFace() = default;

    // =========================================================================
    // Asymmetric Germanium Transistor Clipping Models
    // =========================================================================

    /**
     * Q1: Common-emitter germanium amplifier (AC128, hFE~75).
     * Moderate gain stage that feeds Q2.
     *
     * Gain: gm * R3 = (Ic/Vt) * R3 ~ (0.5mA/26mV) * 8.2K ~ 158
     * But biased so collector sits at ~4.5V (half of 9V supply).
     *
     * Asymmetric clipping:
     *   Positive output (toward cutoff): soft -- transistor stops conducting
     *     gradually as it runs out of base current
     *   Negative output (toward saturation): harder -- Vce_sat ~ 0.1V,
     *     collector-emitter bottoms out creating flat-top clipping
     *
     * This asymmetry produces even harmonics (2nd, 4th) which give germanium
     * its characteristic warm, musical distortion character.
     *
     * @param x Input signal (pre-amplified by q1Gain).
     * @return Asymmetrically clipped signal in approximately [-0.85, 1.0].
     */
    static inline float geQ1Clip(float x) {
        if (x >= 0.0f) {
            // Soft cutoff: transistor gradually stops conducting
            return fast_math::fast_tanh(x * 0.7f);
        } else {
            // Harder saturation: collector-emitter bottoms out at Vce_sat
            return fast_math::fast_tanh(x * 1.4f) * 0.85f;
        }
    }

    /**
     * Q2: Common-emitter germanium amplifier (AC128, hFE~120).
     * Main clipping stage with bias-dependent gating.
     *
     * Asymmetric clipping (stronger than Q1):
     *   Positive: soft cutoff (same mechanism, slightly softer coefficient)
     *   Negative: hard saturation with pronounced flat-top clipping
     *
     * Gating: At low signal levels (below biasThreshold), output drops
     * abruptly to zero -- models Q2 biased near cutoff at low fuzz settings.
     * This creates the sputtery/velcro Fuzz Face character that players love.
     * As a note decays, the signal drops below the bias threshold and the
     * transistor stops amplifying, creating an abrupt cutoff instead of a
     * natural decay.
     *
     * @param x              Input signal (pre-amplified by q2Gain).
     * @param biasThreshold  Gating threshold [0, ~0.15]. Higher = more gating.
     * @return Asymmetrically clipped and gated signal.
     */
    static inline float geQ2Clip(float x, float biasThreshold) {
        // Asymmetric soft clipping -- stronger than Q1
        float clipped;
        if (x >= 0.0f) {
            // Soft cutoff: gradual roll-off toward supply rail
            clipped = fast_math::fast_tanh(x * 0.6f);
        } else {
            // Hard saturation: flat-top clipping characteristic
            // The 2.0x coefficient drives into tanh harder, and 0.8x scaling
            // models the reduced output swing when transistor saturates
            clipped = fast_math::fast_tanh(x * 2.0f) * 0.8f;
        }

        // Gating: signal below bias threshold gets attenuated sharply.
        // This models Q2 biased near cutoff -- weak signals don't get
        // amplified because they can't push the transistor past its
        // operating threshold.
        //
        // biasThreshold ranges from ~0.0 (full fuzz, no gate) to
        // ~0.15 (low fuzz, strong gate).
        //
        // Quadratic taper provides a smooth transition rather than a
        // hard gate, matching the gradual turn-on of a real Ge transistor.
        float absClipped = std::abs(clipped);
        if (absClipped < biasThreshold) {
            float t = absClipped / (biasThreshold + 1e-10f);  // avoid div-by-zero
            clipped *= t * t;  // quadratic fade to zero
        }

        return clipped;
    }

    // =========================================================================
    // Effect interface
    // =========================================================================

    /**
     * Process audio through the Fuzz Face circuit model. Real-time safe.
     *
     * Signal flow:
     *   Input -> Guitar Volume scaling (impedance interaction model)
     *         -> Input coupling high-pass (C1=2.2uF, ~14Hz)
     *         -> 2x Upsample
     *         -> Stage 1: Q1 asymmetric Ge clipping (soft cutoff/hard sat)
     *         -> Stage 2: Q2 asymmetric Ge clipping + bias-dependent gating
     *         -> 2x Downsample
     *         -> Output coupling high-pass (C3=10nF, ~31Hz)
     *         -> Output volume + soft limiting
     *
     * @param buffer   Mono audio samples in [-1.0, 1.0].
     * @param numFrames Number of frames in the buffer.
     */
    void process(float* buffer, int numFrames) override {
        // Guard against being called before setSampleRate() allocates buffers.
        if (workBuffer_.empty()) return;

        // ------------------------------------------------------------------
        // Snapshot all parameters once per block. Using relaxed ordering is
        // safe here: we only need atomicity (no torn reads), not ordering
        // guarantees relative to other memory operations. The UI thread
        // writes these; the audio thread reads them.
        // ------------------------------------------------------------------
        const float fuzz         = fuzz_.load(std::memory_order_relaxed);
        const float volume       = volume_.load(std::memory_order_relaxed);
        const float guitarVolume = guitarVolume_.load(std::memory_order_relaxed);

        // ------------------------------------------------------------------
        // Pre-compute per-block constants outside the sample loop.
        // ------------------------------------------------------------------

        // === Guitar Volume -> Input Impedance Interaction ===
        //
        // The Fuzz Face's magic lies in its low input impedance (~10K from
        // R1=33K in parallel with the transistor's base impedance). When the
        // guitar's volume pot is rolled back, the source impedance seen by
        // the fuzz increases dramatically (the pot acts as a voltage divider
        // with increasing series resistance). This:
        //
        //   1. Reduces the signal amplitude entering the fuzz (obvious)
        //   2. Changes the Thevenin equivalent seen by Q1, reducing the
        //      voltage delivered to the transistor's base
        //   3. Alters the bias point, causing Q1 to operate more linearly
        //
        // We model this by scaling the input signal amplitude (voltage
        // divider effect with audio taper).
        //
        // guitarVolume=1.0: Full signal, maximum fuzz
        // guitarVolume=0.0: Signal heavily attenuated, clean/warm overdrive
        //
        // Exponential mapping: guitar pots are audio taper, and the impedance
        // increases nonlinearly as the pot is rolled back.
        const double guitarVol = static_cast<double>(guitarVolume);
        const double inputAttenuation = guitarVol * guitarVol;  // Audio taper

        // === Fuzz -> Gain and Feedback Mapping ===
        //
        // The FUZZ pot (1K) is wired as a variable resistor from Q2's
        // emitter to ground, controlling how much of Q2's emitter signal
        // is bypassed by C2 (20uF). At maximum fuzz:
        //   - Rfuzz is at minimum (~50 ohm) -> C2 fully bypasses the emitter
        //   - Q2 operates at maximum gain (no emitter degeneration)
        //   - R4 feedback has minimal effect on the large signal swing
        //
        // At minimum fuzz:
        //   - Rfuzz is at maximum (1K) -> significant emitter degeneration
        //   - Q2's gain is drastically reduced
        //   - R4 feedback dominates, creating the sputtery/gated sound
        //
        // Stage 1 gain: drives Q1 into asymmetric clipping.
        // Feedback via R4 reduces effective gain at low fuzz settings.
        const double fuzzD = static_cast<double>(fuzz);
        const double feedbackReduction = 0.3 + 0.7 * fuzzD;
        const double q1Gain = kQ1BaseGain * feedbackReduction;

        // Stage 2 gain: models Q2's variable gain controlled by fuzz pot.
        // Q2 is the MAIN gain/clipping stage. A real AC128 with beta=120,
        // Rc=470 and RE=50 ohm has voltage gain ~ beta*Rc/(RE+re) ~ 740.
        // The asymmetric clipping limits output swing. The external gain
        // must drive Q2 into meaningful Ge saturation.
        //
        // At full fuzz: gain ~25 (heavy Ge saturation, thick fuzz)
        // At zero fuzz: gain ~4 (mild overdrive, sputtery/gated character)
        // Quadratic taper: fuzz pot sweep matches real pot behavior
        const double q2Gain = 4.0 + (25.0 - 4.0) * fuzzD * fuzzD;

        // Gating threshold: at low fuzz, Q2 is biased near cutoff.
        // Signals below this threshold get gated (the sputtery/velcro sound).
        // At full fuzz: threshold = 0 (no gating, full sustain)
        // At zero fuzz: threshold = 0.15 (strong gating, notes die abruptly)
        // Quadratic taper matches the gradual bias shift of the real circuit.
        const float biasThreshold = 0.15f * (1.0f - fuzz) * (1.0f - fuzz);

        // === Output Volume ===
        //
        // The original Fuzz Face uses a 500K audio taper pot. At noon,
        // a real Fuzz Face produces roughly unity gain or slight boost.
        // Linear mapping with 2x makeup gain gives unity at volume=0.5,
        // matching the RAT's proven gain staging pattern.
        const float outputGain = volume * 2.0f;

        // ------------------------------------------------------------------
        // Per-sample pre-processing (before oversampling)
        // ------------------------------------------------------------------
        for (int i = 0; i < numFrames; ++i) {
            double x = static_cast<double>(buffer[i]);

            // Guitar volume attenuation (voltage divider effect)
            x *= inputAttenuation;

            // Input coupling high-pass: C1 = 2.2uF, fc ~14Hz
            // Models the input coupling capacitor that blocks DC and
            // removes subsonic content. The 2.2uF value creates a very
            // low cutoff (~14Hz with the ~5K source impedance), preserving
            // the full guitar signal bandwidth.
            // Uses double precision: sub-20Hz HP coefficient is very close
            // to 1.0, and float cannot represent the difference accurately.
            double hpOut = inputHPCoeff_ * (inputHPState_ + x - inputHPPrev_);
            inputHPPrev_ = x;
            inputHPState_ = fast_math::denormal_guard(hpOut);
            x = inputHPState_;

            // Scale input to realistic circuit voltage levels.
            // Guitar signal is typically 100-500mV peak. With the Q1 bias
            // network, the signal at Q1's base is a few hundred mV.
            // We scale by a modest amount to bring the normalized audio
            // signal into the clipping functions' operating range.
            x *= kInputVoltageScale;

            workBuffer_[i] = static_cast<float>(x);
        }

        // ------------------------------------------------------------------
        // Oversampled nonlinear processing (2x)
        // ------------------------------------------------------------------
        const int oversampledLen = oversampler_.getOversampledSize(numFrames);
        float* upsampled = oversampler_.upsample(workBuffer_.data(), numFrames);

        for (int i = 0; i < oversampledLen; ++i) {
            double x = static_cast<double>(upsampled[i]);

            // =============================================================
            // Stage 1: Q1 Germanium Amplifier (first transistor)
            // =============================================================
            // Q1 is biased as a common-emitter amplifier. The asymmetric
            // Ge clipping rounds positive peaks softly (cutoff) and clips
            // negative peaks harder (saturation at Vce_sat ~ 0.1V).
            // This produces the even harmonics (2nd, 4th) characteristic
            // of germanium — warm and musical, not harsh.
            //
            // feedbackReduction models R4 (100K) negative feedback from
            // Q2's collector back to Q1's base. At low fuzz, the feedback
            // reduces Q1's effective gain significantly.
            x *= q1Gain;
            x = static_cast<double>(geQ1Clip(static_cast<float>(x)));

            // =============================================================
            // Stage 2: Q2 Germanium Amplifier (main clipping stage)
            // =============================================================
            // Q2 is the main gain/clipping stage. Its gain is controlled
            // by the FUZZ pot (emitter degeneration). At full fuzz, Q2
            // clips heavily with strong asymmetry, producing thick,
            // saturated fuzz. At low fuzz, Q2's bias point shifts near
            // cutoff, creating gating behavior where weak signals (note
            // decay) get cut off abruptly — the classic velcro/sputter.
            x *= q2Gain;
            x = static_cast<double>(geQ2Clip(static_cast<float>(x), biasThreshold));

            // Output scaling: bring Ge-clipped signal back to usable level.
            // Two stages of asymmetric tanh clipping produce output in
            // approximately [-0.8, 1.0] range. Makeup gain of 2.5x brings
            // the signal to a healthy level for the output stage.
            x *= 2.5;

            upsampled[i] = static_cast<float>(x);
        }

        // Downsample back to original rate
        oversampler_.downsample(upsampled, numFrames, buffer);

        // ------------------------------------------------------------------
        // Post-processing at native sample rate
        // ------------------------------------------------------------------
        for (int i = 0; i < numFrames; ++i) {
            double x = static_cast<double>(buffer[i]);

            // Output coupling high-pass: C3 = 10nF, fc ~31Hz
            // The small output coupling cap creates a higher cutoff than
            // the input, rolling off low bass. This is part of the Fuzz
            // Face's characteristic "tight" bass response -- it doesn't
            // produce the massive sub-bass of a Big Muff, instead keeping
            // the low end focused and punchy.
            double hpOut = outputHPCoeff_ * (outputHPState_ + x - outputHPPrev_);
            outputHPPrev_ = x;
            outputHPState_ = fast_math::denormal_guard(hpOut);
            x = outputHPState_;

            // Apply output volume. A real Fuzz Face is a LOUD pedal:
            // 20-40dB of gain is typical. The volume pot attenuates from
            // this high level, so even at noon you get substantial output.
            x *= static_cast<double>(outputGain);

            // Soft-clip the output using fast_tanh for Ge-appropriate
            // saturation character. Hard clipping (std::max/min) creates
            // harsh digital artifacts that destroy the warm Ge sound.
            // fast_tanh naturally saturates to [-1, +1], preserving the
            // smooth, round clipping character of germanium transistors.
            x = fast_math::fast_tanh(x);

            buffer[i] = static_cast<float>(x);
        }
    }

    /**
     * Configure sample rate and pre-allocate all internal buffers.
     * Called once during stream setup before any process() calls.
     *
     * @param sampleRate Device sample rate in Hz.
     */
    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;

        // Pre-allocate buffers for maximum expected block size.
        // Android audio typically uses 64-480 frame buffers; 2048
        // provides generous headroom. This is the only allocation point.
        workBuffer_.resize(kMaxFrames, 0.0f);

        // Prepare the 2x oversampler
        oversampler_.prepare(kMaxFrames);

        // Pre-compute fixed filter coefficients for input/output coupling.
        // These cutoffs are determined by circuit component values and
        // do not change at runtime.

        // Input coupling HP: C1=2.2uF with R_guitar (~7K at full vol)
        // fc = 1/(2*pi*R*C) = 1/(2*pi*7000*2.2e-6) ~= 10.3 Hz
        // We use a slightly higher value (14Hz) to account for the
        // parallel bias network (R1=33K || Rbe ~= 5.5K effective).
        inputHPCoeff_ = computeOnePoleHPCoeff(14.0,
                                               static_cast<double>(sampleRate));

        // Output coupling HP: C3=10nF with Rvol (500K pot, ~250K average)
        // fc = 1/(2*pi*250K*10nF) ~= 63 Hz at mid-pot
        // At full volume (low R from collector): fc higher
        // We model the effective output coupling at ~31Hz (a common
        // measured value for Fuzz Face output with typical load).
        outputHPCoeff_ = computeOnePoleHPCoeff(31.0,
                                                static_cast<double>(sampleRate));
    }

    /**
     * Reset all internal filter state.
     * Called when the audio stream restarts or the effect is re-enabled
     * to prevent stale samples from leaking through as clicks/pops.
     */
    void reset() override {
        oversampler_.reset();

        // Reset all one-pole filter states
        inputHPState_  = 0.0;
        inputHPPrev_   = 0.0;
        outputHPState_ = 0.0;
        outputHPPrev_  = 0.0;
    }

    // =========================================================================
    // Parameter access (called from UI thread via JNI)
    // =========================================================================

    /**
     * Set a parameter by ID. Thread-safe via atomic storage.
     *
     * @param paramId  0=Fuzz, 1=Volume, 2=Guitar Volume
     * @param value    Parameter value in [0.0, 1.0].
     */
    void setParameter(int paramId, float value) override {
        value = clamp(value, 0.0f, 1.0f);

        switch (paramId) {
            case kParamFuzz:
                fuzz_.store(value, std::memory_order_relaxed);
                break;
            case kParamVolume:
                volume_.store(value, std::memory_order_relaxed);
                break;
            case kParamGuitarVolume:
                guitarVolume_.store(value, std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    /**
     * Get a parameter by ID. Thread-safe via atomic load.
     *
     * @param paramId  0=Fuzz, 1=Volume, 2=Guitar Volume
     * @return Current parameter value, or 0.0f if paramId is unknown.
     */
    float getParameter(int paramId) const override {
        switch (paramId) {
            case kParamFuzz:
                return fuzz_.load(std::memory_order_relaxed);
            case kParamVolume:
                return volume_.load(std::memory_order_relaxed);
            case kParamGuitarVolume:
                return guitarVolume_.load(std::memory_order_relaxed);
            default:
                return 0.0f;
        }
    }

private:
    // =========================================================================
    // Constants
    // =========================================================================

    /** Parameter IDs matching the JNI bridge convention. */
    static constexpr int kParamFuzz         = 0;
    static constexpr int kParamVolume       = 1;
    static constexpr int kParamGuitarVolume = 2;

    /** Oversampling factor. 2x provides adequate alias rejection for
     *  germanium's soft clipping (generates fewer high-order harmonics
     *  than silicon hard clipping). */
    static constexpr int kOversampleFactor = 2;

    /** Maximum expected audio buffer size. Android typically uses 64-480
     *  frames; 2048 provides generous headroom for all configurations. */
    static constexpr int kMaxFrames = 2048;

    /** Q1 base gain: common-emitter amplifier gain.
     *  A real AC128 CE stage with R3=8.2K collector load has theoretical
     *  gain of gm*R3 = (Ic/Vt)*R3 ~ (0.5mA/26mV)*8.2K ~ 158.
     *  We use a much lower value because the asymmetric clipping functions
     *  operate directly on the signal (no WDF impedance network to absorb
     *  excess gain). With kInputVoltageScale=0.5 and feedbackReduction,
     *  effective Q1 gain ranges from ~1.8x (low fuzz) to ~6x (full fuzz),
     *  which drives the Q1 clipper into its characteristic soft region. */
    static constexpr double kQ1BaseGain = 6.0;

    /** Input voltage scaling: maps normalized audio [-1,1] to realistic
     *  guitar signal voltages. A typical humbucker produces ~200-500mV
     *  peak, and single-coils ~100-200mV. We scale to represent the
     *  signal level at Q1's base after the input coupling network. */
    static constexpr double kInputVoltageScale = 0.5;

    // =========================================================================
    // Filter Coefficient Computation
    // =========================================================================

    /**
     * Compute one-pole high-pass filter coefficient (double precision).
     *
     * For the difference equation:
     *   y[n] = a * (y[n-1] + x[n] - x[n-1])
     *
     * where a = RC / (RC + dt) = 1 / (1 + 2*pi*fc/fs).
     *
     * For very low cutoffs (like 14Hz input coupling), 'a' is very
     * close to 1.0. Double precision is essential here because float's
     * 23-bit mantissa cannot represent the difference from 1.0
     * accurately enough, causing filter drift.
     *
     * @param cutoffHz Desired cutoff frequency in Hz.
     * @param sampleRateHz Sample rate in Hz.
     * @return Filter coefficient (closer to 1.0 = lower cutoff).
     */
    static double computeOnePoleHPCoeff(double cutoffHz, double sampleRateHz) {
        if (sampleRateHz <= 0.0) return 0.99;
        cutoffHz = std::max(1.0, std::min(cutoffHz, sampleRateHz * 0.49));
        const double rc = 1.0 / (2.0 * M_PI * cutoffHz);
        const double dt = 1.0 / sampleRateHz;
        return rc / (rc + dt);
    }

    /**
     * Clamp a float value to [minVal, maxVal].
     *
     * @param value  The value to clamp.
     * @param minVal Minimum allowed value.
     * @param maxVal Maximum allowed value.
     * @return Clamped value.
     */
    static float clamp(float value, float minVal, float maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    // =========================================================================
    // Parameters (written from UI thread, read from audio thread)
    // =========================================================================

    /** Fuzz amount [0, 1]. Controls emitter degeneration and feedback.
     *  0.0 = minimal fuzz (gated/sputtery), 1.0 = full fuzz (saturated).
     *  Default 0.7 gives the classic thick germanium fuzz sound. */
    std::atomic<float> fuzz_ { 0.7f };

    /** Output volume [0, 1]. Controls final output level.
     *  Default 0.5 provides moderate output. */
    std::atomic<float> volume_ { 0.5f };

    /** Guitar volume [0, 1]. Simulates the guitar's volume knob.
     *  1.0 = full volume (low source impedance, maximum fuzz).
     *  0.0 = volume rolled off (high source impedance, cleans up).
     *  This is THE defining Fuzz Face interaction. */
    std::atomic<float> guitarVolume_ { 1.0f };

    // =========================================================================
    // Audio thread state (only accessed from the audio callback thread)
    // =========================================================================

    /** Device native sample rate in Hz. */
    int32_t sampleRate_ = 44100;

    /** 2x oversampler for the nonlinear clipping stages.
     *  Germanium's soft clipping generates fewer high-order harmonics
     *  than silicon hard clipping, so 2x provides adequate alias
     *  rejection while keeping CPU load minimal on mobile ARM. */
    Oversampler<kOversampleFactor> oversampler_;

    // ---- Pre-allocated buffer ----

    /** Working buffer for pre-processed input before oversampling.
     *  Sized to kMaxFrames in setSampleRate(). */
    std::vector<float> workBuffer_;

    // ---- Pre-computed filter coefficients (set in setSampleRate()) ----

    /** Input coupling HP coefficient (C1=2.2uF, ~14Hz). */
    double inputHPCoeff_ = 0.999;

    /** Output coupling HP coefficient (C3=10nF, ~31Hz). */
    double outputHPCoeff_ = 0.99;

    // ---- One-pole filter states (double precision for low-freq stability) ----

    /** Input coupling high-pass filter state. */
    double inputHPState_ = 0.0;

    /** Previous input sample for input coupling HP. */
    double inputHPPrev_ = 0.0;

    /** Output coupling high-pass filter state. */
    double outputHPState_ = 0.0;

    /** Previous input sample for output coupling HP. */
    double outputHPPrev_ = 0.0;
};

#endif // GUITAR_ENGINE_FUZZ_FACE_H
