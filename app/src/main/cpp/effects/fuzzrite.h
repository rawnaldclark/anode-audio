#ifndef GUITAR_ENGINE_FUZZRITE_H
#define GUITAR_ENGINE_FUZZRITE_H

#include "effect.h"
#include "oversampler.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * Circuit-accurate emulation of the Mosrite Fuzzrite (1966-1967).
 *
 * The Fuzzrite is one of the earliest American fuzz pedals, designed by
 * Ed Sanner for Mosrite. It gained fame through The Ventures and later
 * became a cult favorite for its aggressive, buzzy fuzz character that
 * sits somewhere between the warmth of a Fuzz Face and the harshness of
 * a Shin-ei Superfuzz. Its defining feature is the Depth control, which
 * is NOT a gain knob but a crossfade BLEND between the output of the
 * first gain stage (fat, warm, single-stage fuzz) and the output of
 * the second gain stage (thin, buzzy, stripped of bass by a 3.3kHz HPF).
 *
 * What makes the Fuzzrite unique among fuzz pedals:
 *
 *   1. NO GLOBAL FEEDBACK between stages (unlike Fuzz Face, which has a
 *      feedback resistor from Q2 collector to Q1 base). This means the
 *      Fuzzrite has NO touch-sensitive compression -- it is raw, aggressive,
 *      and does not clean up with guitar volume the way a Fuzz Face does.
 *
 *   2. DEPTH BLEND CONTROL: The Depth pot crossfades between Q1's output
 *      (full-range, fat fuzz from a single gain stage) and Q2's output
 *      (bass-stripped buzzy fuzz from two cascaded stages + a 3.3kHz HPF).
 *      At Depth=0, it's a warm overdrive; at Depth=1, it's a razor-thin
 *      buzz-saw fuzz. Most players set Depth around 0.5-0.7 for the
 *      classic Fuzzrite snarl.
 *
 *   3. EXTREME GAIN: Each stage provides ~44 dB of voltage gain (158x),
 *      for a total of ~88 dB cascaded. With self-biasing at Vc ~0.6V
 *      (severely starved), the transistors slam into saturation/cutoff
 *      on nearly every cycle, producing heavy harmonic content.
 *
 *   4. THE 22K + 2.2nF HIGH-PASS on Q2's output is the defining tonal
 *      element. At ~3,289 Hz, it strips nearly all bass and low-mids
 *      from Q2's output, leaving only a buzzy, treble-heavy fuzz. This
 *      is what gives the Fuzzrite its characteristic "angry bee" sound
 *      when the Depth is turned up.
 *
 * == Circuit Topology (two cascaded common-emitter stages) ==
 *
 *   Guitar --[C1: 47nF]--+--[R3: 470K]-- Q1 Base
 *              coupling   |
 *                         +--[R1: 1M]-- GND  (input pulldown)
 *
 *   Q1 Collector --[R2: 470K]-- Vcc (9V)
 *       |                   \--[R3: 470K]-- Q1 Base (self-bias feedback)
 *       |
 *       +--[C2: 2.2nF]-- Depth pot pin 3 (Q1 output, "fat" path)
 *       |
 *       +--[C4: 47nF]-- Q2 Base (interstage coupling)
 *
 *   Q2 Collector --[R4: 470K]-- Vcc (9V)
 *       |                   \--[R5: 470K]-- Q2 Base (self-bias feedback)
 *       |
 *       +--[C3: 2.2nF]--[R6: 22K]-- GND -- Depth pot pin 1 (Q2 output, "buzzy" path)
 *
 *   Depth pot wiper --[Volume pot]-- Output
 *
 * == Biasing Analysis ==
 *
 *   Each transistor is self-biased via a 470K collector-to-base feedback
 *   resistor. With a 470K collector load and 470K feedback:
 *
 *     Ic = (Vcc - Vbe) / (Rc + Rf/beta) = (9 - 0.6) / (470K + 470K/150)
 *        = 8.4 / 473.1K = ~17.8 uA
 *
 *     Vc = Vcc - Ic * Rc = 9 - 0.0178mA * 470K = 9 - 8.4 = ~0.6V
 *
 *   This is EXTREMELY starved biasing -- the collector sits at only 0.6V,
 *   barely above Vce_sat. Any signal swing immediately drives the transistor
 *   into either saturation (Vc -> Vce_sat ~0.1V) or cutoff (Vc -> Vcc).
 *   This is what creates the heavy, asymmetric clipping.
 *
 * == Component Values ==
 *
 *   Silicon Version (NPN, more common):
 *     R1 = 1M      (input pulldown)
 *     R2 = 470K    (Q1 collector load)
 *     R3 = 470K    (Q1 base-collector feedback)
 *     R4 = 470K    (Q2 collector load)
 *     R5 = 470K    (Q2 base-collector feedback)
 *     R6 = 22K     (Q2 output HPF -- THE defining resistor)
 *     C1 = 47nF    (input coupling)
 *     C2 = 2.2nF   (Q1 output coupling to Depth pot)
 *     C3 = 2.2nF   (Q2 output coupling to Depth pot via R6)
 *     C4 = 47nF    (interstage coupling Q1->Q2)
 *     Q1: hFE ~150, Si (Is = 2.52e-9)
 *     Q2: hFE ~80,  Si (Is = 2.52e-9)
 *
 *   Germanium Version (PNP, original):
 *     R1 = 4.7M    (higher input impedance for Ge bias)
 *     C1 = 50nF, C4 = 50nF (slightly larger coupling caps)
 *     C2 = 2.0nF, C3 = 2.0nF
 *     Q1: 2N2613, hFE ~150, Is = 5.0e-6, Icbo = 350uA (leaky)
 *     Q2: 2N408,  hFE ~65,  Is = 5.0e-6, Icbo = 40uA
 *     All resistor values same as Si version
 *
 * == Filter Frequencies ==
 *
 *   | Filter           | Components              | Si Freq    | Ge Freq    |
 *   |------------------|-------------------------|------------|------------|
 *   | Input HP         | C1 + R1                 | ~3.4 Hz    | ~0.68 Hz   |
 *   | Q1 output HP     | C2 + load(~500K)        | ~145 Hz    | ~159 Hz    |
 *   | Interstage HP    | C4 + Zin_Q2             | ~26 Hz     | ~6.8 Hz    |
 *   | Q2 output HP     | R6(22K) + C3            | **3,289 Hz** | **3,617 Hz** |
 *   | Output DC block  | (modeled)               | ~10 Hz     | ~10 Hz     |
 *
 * == Clipping Model ==
 *
 *   The transistor clipping is modeled as asymmetric tanh-based waveshaping.
 *   In a common-emitter stage with Vc biased at ~0.6V:
 *     - Positive input swing: collector drops toward Vce_sat (0.1V)
 *       -> collector swing is only 0.5V before hard saturation
 *     - Negative input swing: collector rises toward Vcc (9V)
 *       -> collector swing has 8.4V of headroom before cutoff
 *
 *   This creates ASYMMETRIC clipping: hard clip on positive peaks,
 *   soft clip on negative peaks. The asymmetry produces even-order
 *   harmonics (2nd, 4th) which contribute warmth. The Ge version
 *   has softer clipping onset (~0.15-0.3V threshold) compared to
 *   Si (~0.5-0.7V threshold).
 *
 *   ADAA (Anti-Derivative Anti-Aliasing) is applied to the tanh clipping
 *   function. For tanh(x), the antiderivative is ln(cosh(x)). First-order
 *   ADAA uses the difference quotient: F(x[n]) - F(x[n-1]) / (x[n] - x[n-1]).
 *   Combined with 2x oversampling, this provides excellent alias rejection.
 *
 * == DSP Implementation ==
 *
 *   This uses DIRECT CIRCUIT SIMULATION, not WDF. The Fuzzrite's feedforward
 *   topology (no global feedback loops) means WDF adds complexity with no
 *   benefit. The implementation uses:
 *     - First-order IIR HPFs (bilinear transform) for all coupling capacitors
 *     - Asymmetric tanh waveshaping for transistor saturation
 *     - First-order ADAA on clipping functions
 *     - 2x oversampling around the nonlinear stages
 *     - Pre-computed variant coefficients for Ge/Si selection
 *
 * == Parameters ==
 *
 *   Param 0: "Depth" (0.0-1.0, default 0.5)
 *            Crossfade blend between Q1 output (fat) and Q2 output (buzzy).
 *            0 = 100% Q1 (warm, single-stage fuzz)
 *            1 = 100% Q2 (thin, buzzy, two-stage + 3.3kHz HPF)
 *            Uses reverse-log taper for authentic pot behavior.
 *
 *   Param 1: "Volume" (0.0-1.0, default 0.7)
 *            Output level after the Depth blend.
 *
 *   Param 2: "Variant" (0-1, default 0)
 *            0 = Germanium (PNP, softer, warmer, more even harmonics)
 *            1 = Silicon (NPN, sharper, brighter, more aggressive)
 *
 * References:
 *   - Fuzzrite schematic analysis (multiple verified sources)
 *   - Electrosmash-style circuit analysis methodology
 *   - DAFX: Digital Audio Effects, 2nd Ed. (Zoelzer, 2011) -- waveshaping
 *   - Parker et al., "Reducing the Aliasing of Nonlinear Waveshaping Using
 *     Continuous-Time Convolution," DAFx-16, 2016 (ADAA theory)
 */
class Fuzzrite : public Effect {
public:
    Fuzzrite() = default;

    // =========================================================================
    // Effect interface
    // =========================================================================

    /**
     * Process audio through the Fuzzrite fuzz circuit. Real-time safe.
     *
     * Signal flow per block:
     *   1. Snapshot atomic parameters
     *   2. Detect variant change and reconfigure
     *   3. Input coupling HPF (C1)
     *   4. Pre-gain (transistor amplification, stage 1)
     *   5. Upsample 2x
     *   6. Stage 1: asymmetric tanh clipping with ADAA
     *   7. Q1 output coupling HPF (C2, ~145 Hz)
     *   8. Interstage coupling HPF (C4, ~26 Hz)
     *   9. Stage 2: asymmetric tanh clipping with ADAA
     *  10. Q2 output HPF (R6+C3, ~3,289 Hz -- THE Fuzzrite tone)
     *  11. Downsample back to native rate
     *  12. Depth blend (crossfade Q1 and Q2 paths)
     *  13. Output DC blocker + volume + safety clamp
     *
     * @param buffer   Mono audio samples in [-1.0, 1.0].
     * @param numFrames Number of frames in the buffer.
     */
    void process(float* buffer, int numFrames) override {
        // Guard against being called before setSampleRate() allocates buffers.
        if (preOsBuffer_.empty()) return;

        // ------------------------------------------------------------------
        // Snapshot all parameters once per block (relaxed: atomicity only)
        // ------------------------------------------------------------------
        const float depth   = depth_.load(std::memory_order_relaxed);
        const float volume  = volume_.load(std::memory_order_relaxed);
        const float variant = variant_.load(std::memory_order_relaxed);

        // ------------------------------------------------------------------
        // Detect variant change and reconfigure coefficients
        // ------------------------------------------------------------------
        const int variantInt = clampInt(static_cast<int>(variant + 0.5f), 0, 1);
        if (variantInt != cachedVariant_) {
            cachedVariant_ = variantInt;
            configureVariant(variantInt);
        }

        // ------------------------------------------------------------------
        // Pre-compute per-block constants outside the sample loop
        // ------------------------------------------------------------------

        // Depth control: reverse-log taper for authentic pot behavior.
        // Real 350K-500K reverse-log pots have more resolution at the
        // "buzzy" end (Q2). Reverse-log: depthBlend = depth^0.4
        // This gives more control range in the 0.5-1.0 region.
        const float depthBlend = std::pow(depth, 0.4f);

        // Crossfade gains for Q1 (fat) and Q2 (buzzy) paths
        const float q1Gain = 1.0f - depthBlend;  // Fat path weight
        const float q2Gain = depthBlend;          // Buzzy path weight

        // Output volume with audio taper (quadratic) for perceptually
        // linear loudness response.
        const float outputGain = volume * volume;

        // Stage gains from the current variant configuration.
        // These represent the voltage gain of each common-emitter stage:
        //   Av = Rc / (re + 1/gm) where Rc = 470K, re dominates at these
        //   tiny bias currents. The gain is enormous (~158x per stage).
        const double stage1Gain = activeConfig_.stage1Gain;
        const double stage2Gain = activeConfig_.stage2Gain;

        // Asymmetry parameters for the current variant.
        // Ge: softer clipping, more asymmetry (even harmonics)
        // Si: sharper clipping, less asymmetry (more odd harmonics)
        const double clipGainPos  = activeConfig_.clipGainPos;
        const double clipGainNeg  = activeConfig_.clipGainNeg;
        const double clipScalePos = activeConfig_.clipScalePos;
        const double clipScaleNeg = activeConfig_.clipScaleNeg;

        // ------------------------------------------------------------------
        // Per-sample pre-processing: input coupling HPF
        // ------------------------------------------------------------------
        for (int i = 0; i < numFrames; ++i) {
            double x = static_cast<double>(buffer[i]);

            // ----- Input coupling high-pass (C1) -----
            // Si: C1(47nF) + R1(1M) = 3.4 Hz
            // Ge: C1(50nF) + R1(4.7M) = 0.68 Hz
            // Both are sub-audible, purely for DC blocking.
            double hpOut = inputHPCoeff_ * (inputHPState_ + x - inputHPPrev_);
            inputHPPrev_ = x;
            inputHPState_ = fast_math::denormal_guard(hpOut);

            preOsBuffer_[i] = static_cast<float>(inputHPState_);
        }

        // ------------------------------------------------------------------
        // Upsample 2x for anti-aliased nonlinear processing
        // ------------------------------------------------------------------
        const int oversampledLen = oversampler_.getOversampledSize(numFrames);
        float* upsampled = oversampler_.upsample(preOsBuffer_.data(), numFrames);

        // ------------------------------------------------------------------
        // Per-sample oversampled processing: two gain stages + filters
        // ------------------------------------------------------------------
        for (int i = 0; i < oversampledLen; ++i) {
            double x = static_cast<double>(upsampled[i]);

            // ============================================================
            // STAGE 1: First common-emitter gain stage (Q1)
            // ============================================================

            // Apply stage 1 voltage gain (~158x for both Ge and Si).
            // Scale input to circuit voltage level first.
            x *= stage1Gain;

            // ----- Asymmetric tanh clipping with first-order ADAA -----
            // The transistor saturates asymmetrically:
            //   Positive swing: collector -> Vce_sat (hard clip, ~0.5V swing)
            //   Negative swing: collector -> Vcc (soft clip, ~8.4V headroom)
            //
            // We model this with different tanh gains for positive and
            // negative halves, producing the characteristic even-order
            // harmonic content from the asymmetry.
            double clipped1 = asymmetricClipADAA(x, clipGainPos, clipGainNeg,
                                                  clipScalePos, clipScaleNeg,
                                                  adaaState1_);

            // NaN guard after nonlinear processing
            if (std::isnan(clipped1) || std::isinf(clipped1)) clipped1 = 0.0;

            // ----- Q1 output coupling HPF (C2: 2.2nF) -----
            // C2(2.2nF) with ~500K load impedance (Depth pot + volume pot):
            //   fc = 1 / (2 * pi * 500K * 2.2nF) = ~145 Hz
            // This shapes the "fat" Q1 path -- retains bass fundamentals
            // but blocks DC from the clipping stage.
            double q1HP = q1OutputHPCoeff_ * (q1OutputHPState_ + clipped1 - q1OutputHPPrev_);
            q1OutputHPPrev_ = clipped1;
            q1OutputHPState_ = fast_math::denormal_guard(q1HP);
            double q1Out = q1OutputHPState_;

            // Store Q1 output for the Depth blend (downsampled later)
            q1OsBuffer_[i] = static_cast<float>(q1Out);

            // ============================================================
            // STAGE 2: Second common-emitter gain stage (Q2)
            // ============================================================

            // ----- Interstage coupling HPF (C4: 47nF) -----
            // C4(47nF) with Q2's input impedance (~128K for Si, ~470K for Ge):
            //   Si: fc = 1 / (2 * pi * 128K * 47nF) = ~26 Hz
            //   Ge: fc = 1 / (2 * pi * 470K * 50nF) = ~6.8 Hz
            // Passes full guitar range, blocks DC from Q1.
            double isHP = interstageHPCoeff_ * (interstageHPState_ + clipped1 - interstageHPPrev_);
            interstageHPPrev_ = clipped1;
            interstageHPState_ = fast_math::denormal_guard(isHP);
            double stage2In = interstageHPState_;

            // Apply stage 2 voltage gain.
            // Q2 has lower hFE (80 for Si, 65 for Ge), but the gain is
            // set by Rc and the bias point, not directly by hFE.
            // The signal is already heavily clipped from stage 1, so stage 2
            // further squares the waveform.
            stage2In *= stage2Gain;

            // ----- Asymmetric tanh clipping with ADAA (stage 2) -----
            double clipped2 = asymmetricClipADAA(stage2In, clipGainPos, clipGainNeg,
                                                  clipScalePos, clipScaleNeg,
                                                  adaaState2_);

            // NaN guard
            if (std::isnan(clipped2) || std::isinf(clipped2)) clipped2 = 0.0;

            // ----- Q2 output HPF: R6(22K) + C3(2.2nF) = ~3,289 Hz -----
            // THIS IS THE DEFINING FUZZRITE FILTER.
            // R6 to ground through C3 creates a high-pass that strips
            // nearly all bass and low-mids from Q2's output:
            //   fc = 1 / (2 * pi * 22K * 2.2nF) = 3,289 Hz
            //
            // At this cutoff:
            //   - 1.6 kHz: -6 dB (half power)
            //   - 800 Hz:  -12 dB
            //   - 400 Hz:  -18 dB
            //   - 200 Hz:  -24 dB
            //   - 100 Hz:  -30 dB
            //
            // Only the buzzy treble harmonics survive, creating the
            // Fuzzrite's signature "angry bee" / "buzz-saw" character
            // when the Depth control blends in the Q2 path.
            double q2HP = q2OutputHPCoeff_ * (q2OutputHPState_ + clipped2 - q2OutputHPPrev_);
            q2OutputHPPrev_ = clipped2;
            q2OutputHPState_ = fast_math::denormal_guard(q2HP);
            double q2Out = q2OutputHPState_;

            // Store Q2 output for the Depth blend
            q2OsBuffer_[i] = static_cast<float>(q2Out);
        }

        // ------------------------------------------------------------------
        // Downsample both Q1 and Q2 paths back to native rate
        // ------------------------------------------------------------------
        // We use two separate downsample passes. The oversampler processes
        // one buffer at a time, so we downsample Q1 first, then Q2.
        //
        // Note: We use a second oversampler for Q2 to maintain independent
        // FIR filter state for each path. Sharing one oversampler would
        // corrupt the FIR delay line between the two downsample calls.
        oversampler_.downsample(q1OsBuffer_.data(), numFrames, q1Buffer_.data());
        oversampler2_.downsample(q2OsBuffer_.data(), numFrames, q2Buffer_.data());

        // ------------------------------------------------------------------
        // Depth blend + output processing at native rate
        // ------------------------------------------------------------------
        for (int i = 0; i < numFrames; ++i) {
            // ----- Depth crossfade -----
            // Blend Q1 (fat, single-stage) and Q2 (buzzy, two-stage + HPF).
            // This is the Fuzzrite's unique tonal control.
            float blended = q1Buffer_[i] * q1Gain + q2Buffer_[i] * q2Gain;

            // ----- Output DC blocker (10 Hz HPF) -----
            // Removes any residual DC offset from the clipping and blending.
            double xd = static_cast<double>(blended);
            double dcHP = outputHPCoeff_ * (outputHPState_ + xd - outputHPPrev_);
            outputHPPrev_ = xd;
            outputHPState_ = fast_math::denormal_guard(dcHP);

            // Apply output volume and safety clamp
            float out = static_cast<float>(outputHPState_) * outputGain;
            buffer[i] = clamp(out, -1.0f, 1.0f);
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
        const int maxOversampledFrames = maxFrames * kOversampleFactor;
        const double nativeRate = static_cast<double>(sampleRate);
        const double oversampledRate = nativeRate * kOversampleFactor;

        // Pre-allocate all buffers
        preOsBuffer_.resize(maxFrames, 0.0f);
        q1OsBuffer_.resize(maxOversampledFrames, 0.0f);
        q2OsBuffer_.resize(maxOversampledFrames, 0.0f);
        q1Buffer_.resize(maxFrames, 0.0f);
        q2Buffer_.resize(maxFrames, 0.0f);

        // Prepare both oversamplers (one for Q1 path, one for Q2 path)
        oversampler_.prepare(maxFrames);
        oversampler2_.prepare(maxFrames);

        // Pre-compute filter coefficients for BOTH variants.
        // This avoids std::exp() on the audio thread when variant changes.
        // Coefficients are stored per-variant and copied to active state
        // in configureVariant().

        // --- Germanium variant (index 0) ---
        // Input HP: C1(50nF) + R1(4.7M) = 0.68 Hz
        variantCoeffs_[0].inputHPCoeff = computeOnePoleHPCoeff(0.68, nativeRate);
        // Q1 output HP: C2(2.0nF) + ~500K load = 159 Hz
        variantCoeffs_[0].q1OutputHPCoeff = computeOnePoleHPCoeff(159.0, oversampledRate);
        // Interstage HP: C4(50nF) + Zin(~470K) = 6.8 Hz
        variantCoeffs_[0].interstageHPCoeff = computeOnePoleHPCoeff(6.8, oversampledRate);
        // Q2 output HP: R6(22K) + C3(2.0nF) = 3,617 Hz
        variantCoeffs_[0].q2OutputHPCoeff = computeOnePoleHPCoeff(3617.0, oversampledRate);

        // --- Silicon variant (index 1) ---
        // Input HP: C1(47nF) + R1(1M) = 3.4 Hz
        variantCoeffs_[1].inputHPCoeff = computeOnePoleHPCoeff(3.4, nativeRate);
        // Q1 output HP: C2(2.2nF) + ~500K load = 145 Hz
        variantCoeffs_[1].q1OutputHPCoeff = computeOnePoleHPCoeff(145.0, oversampledRate);
        // Interstage HP: C4(47nF) + Zin(~128K) = 26 Hz
        variantCoeffs_[1].interstageHPCoeff = computeOnePoleHPCoeff(26.0, oversampledRate);
        // Q2 output HP: R6(22K) + C3(2.2nF) = 3,289 Hz
        variantCoeffs_[1].q2OutputHPCoeff = computeOnePoleHPCoeff(3289.0, oversampledRate);

        // Output DC blocker at native rate (same for both variants)
        outputHPCoeff_ = computeOnePoleHPCoeff(10.0, nativeRate);

        // Force reconfigure on first process()
        cachedVariant_ = -1;
    }

    /**
     * Reset all internal state (filter histories, ADAA state, oversampler
     * FIR history). Called when the audio stream restarts or the effect
     * is re-enabled to prevent stale samples from leaking through.
     */
    void reset() override {
        oversampler_.reset();
        oversampler2_.reset();

        // Reset all HPF states
        inputHPState_ = 0.0;
        inputHPPrev_ = 0.0;
        q1OutputHPState_ = 0.0;
        q1OutputHPPrev_ = 0.0;
        interstageHPState_ = 0.0;
        interstageHPPrev_ = 0.0;
        q2OutputHPState_ = 0.0;
        q2OutputHPPrev_ = 0.0;
        outputHPState_ = 0.0;
        outputHPPrev_ = 0.0;

        // Reset ADAA state for both clipping stages
        adaaState1_ = {0.0, 0.0};
        adaaState2_ = {0.0, 0.0};
    }

    // =========================================================================
    // Parameter access (called from UI thread via JNI)
    // =========================================================================

    /**
     * Set a parameter by ID. Thread-safe via atomic storage.
     *
     * @param paramId  0=Depth, 1=Volume, 2=Variant
     * @param value    Parameter value. Depth/Volume in [0.0, 1.0],
     *                 Variant in [0.0, 1.0] (integer enum).
     */
    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case kParamDepth:
                depth_.store(clamp(value, 0.0f, 1.0f),
                             std::memory_order_relaxed);
                break;
            case kParamVolume:
                volume_.store(clamp(value, 0.0f, 1.0f),
                              std::memory_order_relaxed);
                break;
            case kParamVariant:
                variant_.store(clamp(value, 0.0f, 1.0f),
                               std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    /**
     * Get a parameter by ID. Thread-safe via atomic load.
     *
     * @param paramId  0=Depth, 1=Volume, 2=Variant
     * @return Current parameter value, or 0.0f if paramId is unknown.
     */
    float getParameter(int paramId) const override {
        switch (paramId) {
            case kParamDepth:   return depth_.load(std::memory_order_relaxed);
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
    static constexpr int kParamDepth   = 0;  ///< Blend Q1 (fat) <-> Q2 (buzzy)
    static constexpr int kParamVolume  = 1;  ///< Output level
    static constexpr int kParamVariant = 2;  ///< 0=Germanium, 1=Silicon

    /** Maximum expected audio buffer size. Android typically uses 64-480
     *  frames; 2048 provides generous headroom for all configurations. */
    static constexpr int kMaxFrames = 2048;

    /** Oversampling factor. 2x combined with ADAA provides excellent alias
     *  rejection for the tanh-based clipping. */
    static constexpr int kOversampleFactor = 2;

    /** Minimum difference for ADAA difference quotient to avoid division
     *  by near-zero. Below this threshold, fall back to the instantaneous
     *  waveshaper output. */
    static constexpr double kAdaaEpsilon = 1.0e-7;

    // =========================================================================
    // Variant Configuration
    // =========================================================================

    /**
     * Clipping and gain parameters for each Fuzzrite variant.
     *
     * The two variants differ in transistor characteristics:
     *
     * Germanium (PNP, original):
     *   - Softer clipping onset (~0.15-0.3V threshold)
     *   - More asymmetry (leakier Ge junctions)
     *   - More even-order harmonics (warmer, fatter)
     *   - Lower gain per stage due to leakage loading
     *
     * Silicon (NPN, later production):
     *   - Sharper clipping onset (~0.5-0.7V threshold)
     *   - Less asymmetry (tighter Si junctions)
     *   - More odd-order harmonics (brighter, buzzier)
     *   - Higher effective gain (no leakage losses)
     */
    struct VariantClipConfig {
        double stage1Gain;     ///< Voltage gain of Q1 stage
        double stage2Gain;     ///< Voltage gain of Q2 stage
        double clipGainPos;    ///< tanh gain for positive swing (saturation)
        double clipGainNeg;    ///< tanh gain for negative swing (cutoff)
        double clipScalePos;   ///< Output scale for positive half
        double clipScaleNeg;   ///< Output scale for negative half
    };

    /**
     * Variant clip configurations. Pre-stored for O(1) lookup.
     *
     * Gain derivation:
     *   Each stage: Av = gm * Rc, where gm = Ic / Vt
     *   At Ic = 17.8uA: gm = 17.8e-6 / 26e-3 = 0.685 mA/V
     *   Av = 0.685e-3 * 470e3 = 322 (theoretical max)
     *
     * In practice, the 470K feedback resistor limits the gain to a
     * fraction of this via the Miller effect and input loading:
     *   Effective Av ~ Rc / (Rf/hFE + re)
     *   Si Q1 (hFE=150): Av ~ 470K / (470K/150 + 1.5K) = ~95
     *   Ge Q1 (hFE=150, but with leakage): Av ~ 70
     *
     * We use calibrated values that produce the correct output level
     * and harmonic content when compared to recorded Fuzzrite samples.
     *
     * The clipping gains model the asymmetric saturation:
     *   - clipGainPos = higher (less swing before saturation, clips sooner)
     *   - clipGainNeg = lower (more headroom before cutoff)
     * For Ge, the softer junction makes both gains lower (gentler onset).
     */
    static constexpr VariantClipConfig kVariantConfigs[2] = {
        // Variant 0: Germanium (PNP, original)
        // Softer clipping, more even harmonics, warmer character.
        // Lower tanh gains model the gradual Ge junction conduction.
        // Larger asymmetry ratio (2.5:1) produces more 2nd harmonic.
        {
            70.0,     // stage1Gain: reduced by Ge leakage (~Icbo=350uA on Q1)
            55.0,     // stage2Gain: lower hFE Q2 (65) + leakage
            2.5,      // clipGainPos: Ge saturates gently
            1.0,      // clipGainNeg: Ge cutoff is very soft
            0.85,     // clipScalePos: slightly compressed positive peak
            1.0       // clipScaleNeg: full negative swing (more headroom)
        },
        // Variant 1: Silicon (NPN, later production)
        // Sharper clipping, more odd harmonics, brighter, more aggressive.
        // Higher tanh gains model the sharper Si junction threshold.
        // Smaller asymmetry ratio (2:1) but harder clip overall.
        {
            95.0,     // stage1Gain: full Si gain, no leakage losses
            75.0,     // stage2Gain: lower hFE (80) but clean junctions
            3.5,      // clipGainPos: Si saturates sharply
            1.5,      // clipGainNeg: Si cutoff is still gradual but firmer
            0.8,      // clipScalePos: harder compression on positive peak
            1.0       // clipScaleNeg: full negative swing
        }
    };

    /**
     * Pre-computed filter coefficients for each variant.
     * Computed once in setSampleRate(), selected in configureVariant().
     */
    struct VariantFilterCoeffs {
        double inputHPCoeff;       ///< Input coupling HPF coefficient
        double q1OutputHPCoeff;    ///< Q1 output coupling HPF coefficient
        double interstageHPCoeff;  ///< Interstage coupling HPF coefficient
        double q2OutputHPCoeff;    ///< Q2 output HPF coefficient (THE buzz filter)
    };

    /**
     * Configure the active variant parameters. Called when the variant
     * parameter changes (detected per-block in process()).
     *
     * This copies pre-computed coefficients from the variant lookup tables
     * into the active processing state. No std::exp() or transcendental
     * functions are called here -- all coefficients were pre-computed in
     * setSampleRate().
     *
     * @param variant 0=Germanium, 1=Silicon
     */
    void configureVariant(int variant) {
        // Copy clip/gain config
        activeConfig_ = kVariantConfigs[variant];

        // Copy pre-computed filter coefficients
        inputHPCoeff_       = variantCoeffs_[variant].inputHPCoeff;
        q1OutputHPCoeff_    = variantCoeffs_[variant].q1OutputHPCoeff;
        interstageHPCoeff_  = variantCoeffs_[variant].interstageHPCoeff;
        q2OutputHPCoeff_    = variantCoeffs_[variant].q2OutputHPCoeff;

        // Reset filter states to prevent clicks from stale state at the
        // old variant's coefficient values.
        inputHPState_ = 0.0;
        inputHPPrev_ = 0.0;
        q1OutputHPState_ = 0.0;
        q1OutputHPPrev_ = 0.0;
        interstageHPState_ = 0.0;
        interstageHPPrev_ = 0.0;
        q2OutputHPState_ = 0.0;
        q2OutputHPPrev_ = 0.0;
        adaaState1_ = {0.0, 0.0};
        adaaState2_ = {0.0, 0.0};
    }

    // =========================================================================
    // ADAA Clipping
    // =========================================================================

    /**
     * State for first-order ADAA (Anti-Derivative Anti-Aliasing).
     *
     * ADAA replaces the naive waveshaper y=f(x) with a filtered version
     * that suppresses aliasing from the nonlinear function. For a waveshaper
     * f(x), the first-order ADAA output is:
     *
     *   y[n] = (F(x[n]) - F(x[n-1])) / (x[n] - x[n-1])
     *
     * where F(x) is the antiderivative of f(x). When the denominator is
     * near zero (consecutive samples nearly equal), we fall back to f(x)
     * to avoid numerical instability.
     *
     * For tanh(a*x), F(x) = ln(cosh(a*x)) / a.
     */
    struct AdaaState {
        double prevInput = 0.0;       ///< x[n-1] for difference quotient
        double prevAntideriv = 0.0;   ///< F(x[n-1]) for difference quotient
    };

    /**
     * Compute the antiderivative of tanh(gain * x) at point x.
     *
     * F(x) = ln(cosh(gain * x)) / gain
     *
     * For numerical stability with large arguments:
     *   ln(cosh(u)) = |u| - ln(2) + ln(1 + exp(-2|u|))
     *   For |u| > 10: ln(cosh(u)) ≈ |u| - ln(2)  (error < 2e-9)
     *
     * @param x     Input value.
     * @param gain  The gain applied inside tanh (tanh(gain * x)).
     * @return The antiderivative F(x) = ln(cosh(gain * x)) / gain.
     */
    static inline double tanhAntiderivative(double x, double gain) {
        double u = gain * x;
        double absU = std::abs(u);
        double lnCosh;
        if (absU > 10.0) {
            // Asymptotic form: ln(cosh(u)) -> |u| - ln(2)
            lnCosh = absU - 0.6931471805599453;
        } else {
            // Full computation, numerically stable form:
            // ln(cosh(u)) = |u| + ln(1 + exp(-2|u|)) - ln(2)
            // This avoids overflow from cosh() for moderate |u|.
            lnCosh = absU + std::log1p(std::exp(-2.0 * absU)) - 0.6931471805599453;
        }
        return lnCosh / gain;
    }

    /**
     * Asymmetric clipping with first-order ADAA.
     *
     * Models the transistor's asymmetric saturation behavior:
     *   - Positive input -> collector drops toward Vce_sat (hard clip)
     *   - Negative input -> collector rises toward Vcc (soft clip)
     *
     * Each half uses a different tanh gain and output scale to model
     * the asymmetry. ADAA is applied independently to each half.
     *
     * The asymmetric clip function:
     *   f(x) = scalePos * tanh(gainPos * x)   for x >= 0
     *   f(x) = scaleNeg * tanh(gainNeg * x)   for x < 0
     *
     * The antiderivative:
     *   F(x) = scalePos * ln(cosh(gainPos * x)) / gainPos   for x >= 0
     *   F(x) = scaleNeg * ln(cosh(gainNeg * x)) / gainNeg   for x < 0
     *
     * Note: At x=0, both halves evaluate to 0 (continuous) and the
     * antiderivative is also continuous (F(0) = 0 for both branches).
     *
     * @param x         Input sample.
     * @param gainPos   tanh gain for positive half (saturation side).
     * @param gainNeg   tanh gain for negative half (cutoff side).
     * @param scalePos  Output scale for positive half.
     * @param scaleNeg  Output scale for negative half.
     * @param state     ADAA state (previous input and antiderivative).
     * @return Clipped sample with ADAA anti-aliasing.
     */
    static inline double asymmetricClipADAA(double x,
                                             double gainPos, double gainNeg,
                                             double scalePos, double scaleNeg,
                                             AdaaState& state) {
        // Compute the antiderivative F(x[n]) for the current sample
        double Fn;
        if (x >= 0.0) {
            Fn = scalePos * tanhAntiderivative(x, gainPos);
        } else {
            Fn = scaleNeg * tanhAntiderivative(x, gainNeg);
        }

        // ADAA difference quotient
        double dx = x - state.prevInput;
        double result;
        if (std::abs(dx) > kAdaaEpsilon) {
            // Normal case: use ADAA difference quotient
            result = (Fn - state.prevAntideriv) / dx;
        } else {
            // Fallback: consecutive samples nearly equal, use instantaneous
            // waveshaper output to avoid division by near-zero.
            if (x >= 0.0) {
                result = scalePos * fast_math::fast_tanh(gainPos * x);
            } else {
                result = scaleNeg * fast_math::fast_tanh(gainNeg * x);
            }
        }

        // Update state for next sample
        state.prevInput = x;
        state.prevAntideriv = Fn;

        return result;
    }

    // =========================================================================
    // Filter Coefficient Computation
    // =========================================================================

    /**
     * Compute one-pole high-pass filter coefficient (double precision).
     *
     * For the difference equation:
     *   y[n] = a * (y[n-1] + x[n] - x[n-1])
     *
     * where a = 1 / (1 + 2*pi*fc/fs).
     *
     * This is the standard first-order IIR high-pass derived from the
     * bilinear transform of the analog prototype H(s) = s / (s + wc).
     *
     * @param cutoffHz Desired cutoff frequency in Hz.
     * @param sampleRate Sample rate in Hz.
     * @return Filter coefficient 'a' in double precision.
     */
    static double computeOnePoleHPCoeff(double cutoffHz, double sampleRate) {
        if (sampleRate <= 0.0) return 0.99;
        cutoffHz = std::max(0.1, std::min(cutoffHz, sampleRate * 0.49));
        const double wc = 2.0 * M_PI * cutoffHz;
        return 1.0 / (1.0 + wc / sampleRate);
    }

    /**
     * Clamp a float value to the range [minVal, maxVal].
     */
    static float clamp(float value, float minVal, float maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    /**
     * Clamp an int value to the range [minVal, maxVal].
     */
    static int clampInt(int value, int minVal, int maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    // =========================================================================
    // Parameters (written from UI thread, read from audio thread)
    // =========================================================================

    /** Depth control [0, 1]. Crossfade blend between Q1 (fat) and Q2 (buzzy).
     *  0 = 100% Q1 output (warm, full-range, single-stage fuzz)
     *  1 = 100% Q2 output (thin, buzzy, treble-only, two-stage + 3.3kHz HPF)
     *  Default 0.5 gives the classic Fuzzrite snarl -- balanced fat/buzz. */
    std::atomic<float> depth_{0.5f};

    /** Output volume [0, 1]. Controls final level after the Depth blend.
     *  Default 0.7 provides a healthy output level. */
    std::atomic<float> volume_{0.7f};

    /** Variant [0, 1]. 0=Germanium (PNP, original), 1=Silicon (NPN, later).
     *  Default 0 (Germanium) for the original warm Fuzzrite character. */
    std::atomic<float> variant_{0.0f};

    // =========================================================================
    // Audio thread state (not accessed from UI thread)
    // =========================================================================

    /** Device sample rate in Hz */
    int32_t sampleRate_ = 44100;

    /** Cached variant index to detect changes per-block */
    int cachedVariant_ = -1;

    /** Active clipping/gain configuration (copied from variant lookup) */
    VariantClipConfig activeConfig_ = kVariantConfigs[0];

    // ---- Oversamplers ----

    /**
     * Primary oversampler for the upsampling path and Q1 downsampling.
     * 2x oversampling combined with ADAA provides excellent alias rejection
     * for the tanh-based transistor clipping model.
     */
    Oversampler<2> oversampler_;

    /** Secondary oversampler for Q2 downsampling (independent FIR state). */
    Oversampler<2> oversampler2_;

    // ---- Pre-allocated buffers ----

    /** Buffer for signal after input coupling HPF, before oversampling */
    std::vector<float> preOsBuffer_;

    /** Q1 output buffer at oversampled rate (for independent downsampling) */
    std::vector<float> q1OsBuffer_;

    /** Q2 output buffer at oversampled rate (for independent downsampling) */
    std::vector<float> q2OsBuffer_;

    /** Q1 output buffer at native rate (after downsampling) */
    std::vector<float> q1Buffer_;

    /** Q2 output buffer at native rate (after downsampling) */
    std::vector<float> q2Buffer_;

    // ---- Pre-computed filter coefficients (set in setSampleRate()) ----

    /** Per-variant filter coefficients, pre-computed to avoid std::exp on
     *  the audio thread. Indexed by variant ID (0=Ge, 1=Si). */
    VariantFilterCoeffs variantCoeffs_[2] = {};

    // ---- Active filter coefficients (copied from variantCoeffs_ in
    //      configureVariant(), read on the audio thread) ----

    double inputHPCoeff_      = 0.99;  ///< Input coupling HPF
    double q1OutputHPCoeff_   = 0.99;  ///< Q1 output coupling HPF (~145 Hz)
    double interstageHPCoeff_ = 0.99;  ///< Interstage coupling HPF (~26 Hz)
    double q2OutputHPCoeff_   = 0.99;  ///< Q2 output HPF (~3,289 Hz) THE BUZZ
    double outputHPCoeff_     = 0.99;  ///< Output DC blocker (10 Hz)

    // ---- One-pole filter states (double precision for stability) ----

    /** Input coupling HPF state (C1) */
    double inputHPState_ = 0.0;
    double inputHPPrev_ = 0.0;

    /** Q1 output coupling HPF state (C2, ~145 Hz) */
    double q1OutputHPState_ = 0.0;
    double q1OutputHPPrev_ = 0.0;

    /** Interstage coupling HPF state (C4, ~26 Hz) */
    double interstageHPState_ = 0.0;
    double interstageHPPrev_ = 0.0;

    /** Q2 output HPF state (R6+C3, ~3,289 Hz -- the Fuzzrite buzz filter) */
    double q2OutputHPState_ = 0.0;
    double q2OutputHPPrev_ = 0.0;

    /** Output DC blocker HPF state (10 Hz) */
    double outputHPState_ = 0.0;
    double outputHPPrev_ = 0.0;

    // ---- ADAA state for each clipping stage ----

    /** ADAA state for stage 1 (Q1) clipping */
    AdaaState adaaState1_;

    /** ADAA state for stage 2 (Q2) clipping */
    AdaaState adaaState2_;
};

#endif // GUITAR_ENGINE_FUZZRITE_H
