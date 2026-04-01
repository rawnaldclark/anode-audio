#ifndef GUITAR_ENGINE_FUZZ_FACE_H
#define GUITAR_ENGINE_FUZZ_FACE_H

#include "effect.h"
#include "oversampler.h"
#include "fast_math.h"
#include <chowdsp_wdf/chowdsp_wdf.h>
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
 *                    (common-emitter, WDF gain ~3x)
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
 * WDF Implementation Strategy:
 *
 *   The full 2-transistor circuit with mutual feedback (R4 from Q2 collector
 *   to Q1 base) creates a delay-free loop that cannot be directly modeled
 *   with a standard WDF binary tree. Options include R-type adaptors or
 *   iterative solvers, both of which are too expensive for mobile real-time.
 *
 *   Instead, we decompose into two cascaded WDF sub-circuits:
 *
 *   Stage 1 (Q1): Input coupling + bias network + germanium diode pair
 *     - ResistiveVoltageSource (R_guitar, variable via Guitar Volume)
 *     - C1 (2.2uF input coupling cap)
 *     - R1 (33K bias resistor)
 *     - DiodePairT with Ge parameters (models Q1 collector junction)
 *     - Output = voltage across collector load R3 (rLoad=8.2K)
 *     - Gain ~3x applied externally (drives Ge diode into clipping)
 *
 *   Stage 2 (Q2): Second gain stage + feedback-controlled clipping
 *     - ResistiveVoltageSource (R3=8.2K, Q1 collector impedance)
 *     - C2 (20uF emitter bypass, sets low-freq behavior)
 *     - R_feedback (variable via Fuzz pot: controls clipping intensity)
 *     - DiodePairT with Ge parameters (models Q2 collector junction)
 *     - Harder clipping due to higher gain stage
 *
 *   The R4 feedback is approximated by reducing Stage 1's effective gain
 *   as a function of the Fuzz parameter. At low fuzz, more negative
 *   feedback reduces the overall gain, mimicking the real circuit's
 *   behavior where R4 stabilizes the bias and reduces signal swing.
 *
 * Anti-aliasing:
 *   2x oversampling wraps both WDF nonlinear stages. The germanium diodes
 *   generate softer harmonics than silicon (the gradual clipping knee
 *   produces fewer high-order harmonics), so 2x is sufficient.
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
    // Effect interface
    // =========================================================================

    /**
     * Process audio through the Fuzz Face circuit model. Real-time safe.
     *
     * Signal flow:
     *   Input -> Guitar Volume scaling (impedance interaction model)
     *         -> Input coupling high-pass (C1=2.2uF, ~14Hz)
     *         -> 2x Upsample
     *         -> WDF Stage 1: Q1 germanium clipping (soft, ~3x gain)
     *         -> WDF Stage 2: Q2 germanium clipping (harder, fuzz-dependent)
     *         -> 2x Downsample
     *         -> Output coupling high-pass (C3=10nF, ~31Hz)
     *         -> Output volume
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
        //   2. Shifts the WDF source impedance, changing the Thevenin
        //      equivalent and reducing the voltage delivered to the diodes
        //   3. Alters the bias point, causing Q1 to operate more linearly
        //
        // We model this by:
        //   a) Scaling the input signal amplitude (voltage divider effect)
        //   b) Increasing the ResistiveVoltageSource impedance for Stage 1
        //      (source impedance interaction with diode clipping threshold)
        //
        // guitarVolume=1.0: R_guitar ~7K (low-Z humbucker into 33K || Rbe)
        // guitarVolume=0.0: R_guitar ~250K (high series resistance from pot)
        //
        // Exponential mapping: guitar pots are audio taper, and the impedance
        // increases nonlinearly as the pot is rolled back.
        const double guitarVol = static_cast<double>(guitarVolume);
        const double inputAttenuation = guitarVol * guitarVol;  // Audio taper
        const double rGuitar = 7000.0 + (250000.0 - 7000.0)
                               * (1.0 - guitarVol) * (1.0 - guitarVol);

        // Update the Stage 1 source impedance to reflect guitar volume.
        // This dynamically changes how the WDF tree interacts with the
        // germanium diode pair -- higher impedance means the diodes clip
        // less aggressively (more of the signal appears across the source
        // resistance rather than being shunted through the diodes).
        q1Circuit_.rvs.setResistanceValue(rGuitar);
        q1Circuit_.dp.calcImpedance();

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
        // Stage 1 gain: drives Q1's WDF diodes into clipping.
        // Reduced from theoretical CE gain (~15x) since output is now
        // read from rLoad which develops meaningful voltage directly.
        // Feedback via R4 reduces effective gain at low fuzz settings.
        const double fuzzD = static_cast<double>(fuzz);
        const double feedbackReduction = 0.3 + 0.7 * fuzzD;
        const double q1Gain = kQ1BaseGain * feedbackReduction;

        // Stage 2 gain: models Q2's variable gain controlled by fuzz pot.
        // Q2 is the MAIN gain/clipping stage. A real AC128 with beta=120,
        // Rc=470 and RE=50 ohm has voltage gain ≈ beta*Rc/(RE+re) ≈ 740.
        // The WDF rCollector (470 ohm) develops the output signal, but the
        // Ge diode pair limits output to ~0.15-0.3V. The external gain must
        // drive the Q2 WDF into meaningful Ge saturation.
        //
        // At full fuzz: gain ~25 (heavy Ge saturation, thick fuzz)
        // At zero fuzz: gain ~4 (mild overdrive, sputtery/gated character)
        // Quadratic taper: fuzz pot sweep matches real pot behavior
        const double q2Gain = 4.0 + (25.0 - 4.0) * fuzzD * fuzzD;

        // Update Stage 2 feedback resistance based on fuzz setting.
        // Higher Rfuzz = more emitter degeneration = less clipping.
        // Range: 50 ohm (full fuzz) to 1000 ohm (minimum fuzz).
        const double rFuzz = 50.0 + (1000.0 - 50.0) * (1.0 - fuzzD);
        q2Circuit_.rFeedback.setResistanceValue(rFuzz);
        q2Circuit_.dp.calcImpedance();

        // === Output Volume ===
        //
        // The original Fuzz Face uses a 500K audio taper pot. At noon,
        // a real Fuzz Face produces roughly unity gain or slight boost.
        // Linear mapping with 2x makeup gain gives unity at volume=0.5,
        // matching the RAT's proven gain staging pattern.
        const float outputGain = volume * 2.0f;

        // WDF output normalization: reading voltage across rCollector
        // (470 ohm) and rLoad (8.2K). Ge diode clipping limits voltage
        // to ~0.15-0.3V at these nodes. Two cascaded stages with inter-
        // stage gain produce output in the 0.1-0.3V range after Q2.
        // A real Fuzz Face outputs 2-5Vpp; we need substantial makeup
        // gain to bridge from WDF circuit voltages to normalized audio.
        //
        // 8.0 brings the ~0.15V Ge-clipped rCollector output up to
        // ~1.2, which after soft clipping gives a hot, aggressive signal
        // matching the real pedal's above-unity output character.
        constexpr double kWdfOutputScaling = 8.0;

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
            // signal into the WDF's operating range.
            x *= kInputVoltageScale;

            workBuffer_[i] = static_cast<float>(x);
        }

        // ------------------------------------------------------------------
        // Oversampled WDF processing (2x)
        // ------------------------------------------------------------------
        const int oversampledLen = oversampler_.getOversampledSize(numFrames);
        float* upsampled = oversampler_.upsample(workBuffer_.data(), numFrames);

        for (int i = 0; i < oversampledLen; ++i) {
            double x = static_cast<double>(upsampled[i]);

            // =============================================================
            // Stage 1: Q1 Germanium Clipping (first transistor)
            // =============================================================
            // Q1 is biased as a common-emitter amplifier with low gain
            // (~15x). The germanium transistor's collector-base junction
            // is modeled by the WDF diode pair. The soft Ge clipping
            // rounds the peaks gently, adding warmth and even harmonics.
            //
            // Apply Q1 gain before the WDF clipper (models the voltage
            // amplification of the CE stage).
            x *= q1Gain;

            // Safety limiter: prevent extreme voltages from causing WDF
            // impedance collapse (Ge diodes become near-short above ~5V).
            x = std::max(-5.0, std::min(5.0, x));

            x = q1Circuit_.processSample(x);

            // NaN guard after WDF (Wright Omega edge cases)
            if (std::isnan(x) || std::isinf(x)) x = 0.0;

            // =============================================================
            // Stage 2: Q2 Germanium Clipping (second transistor)
            // =============================================================
            // Q2 is the main gain/clipping stage. Its gain is controlled
            // by the FUZZ pot (emitter degeneration). At full fuzz, Q2
            // clips heavily, producing thick, saturated fuzz. At low fuzz,
            // Q2 clips less and the R4 feedback creates a compressed,
            // slightly gated character.
            x *= q2Gain;

            // Safety limiter for Q2 WDF input
            x = std::max(-5.0, std::min(5.0, x));

            x = q2Circuit_.processSample(x);

            // NaN guard after Q2 WDF
            if (std::isnan(x) || std::isinf(x)) x = 0.0;

            // Normalize WDF output from rCollector voltage back to
            // audio range. The load resistor develops meaningful voltage
            // proportional to clipping current through the Ge diodes.
            x *= kWdfOutputScaling;

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

        // Compute the oversampled rate for WDF capacitor preparation
        // and filter coefficient computation.
        oversampledRate_ = static_cast<double>(sampleRate) * kOversampleFactor;

        // Pre-allocate buffers for maximum expected block size.
        // Android audio typically uses 64-480 frame buffers; 2048
        // provides generous headroom. This is the only allocation point.
        workBuffer_.resize(kMaxFrames, 0.0f);

        // Prepare the 2x oversampler
        oversampler_.prepare(kMaxFrames);

        // Prepare both WDF circuits at the oversampled rate.
        // The capacitors in the WDF trees are reactive elements whose
        // discretization depends on the sample rate (bilinear transform
        // maps s-domain to z-domain, and the mapping depends on T=1/fs).
        q1Circuit_.prepare(oversampledRate_);
        q2Circuit_.prepare(oversampledRate_);

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
     * Reset all internal filter and WDF state.
     * Called when the audio stream restarts or the effect is re-enabled
     * to prevent stale samples from leaking through as clicks/pops.
     */
    void reset() override {
        oversampler_.reset();
        q1Circuit_.reset();
        q2Circuit_.reset();

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
     *  A real AC128 CE stage with R3=8.2K collector load has gain of
     *  approximately gm*R3 = (Ic/Vt)*R3 ≈ (1mA/26mV)*8.2K ≈ 315.
     *  But the WDF diode pair limits the output swing, so we use a
     *  moderate gain that drives the Ge diodes firmly into their soft
     *  clipping region without saturating the WDF (which would collapse
     *  to zero output for Ge Is=5uA).
     *
     *  With kInputVoltageScale=0.5, gain=8.0 puts ~4V peak into Q1's
     *  WDF at full guitar volume, firmly driving the Ge diodes into
     *  their characteristic soft clipping (~0.15-0.3V output). */
    static constexpr double kQ1BaseGain = 8.0;

    /** Input voltage scaling: maps normalized audio [-1,1] to realistic
     *  guitar signal voltages. A typical humbucker produces ~200-500mV
     *  peak, and single-coils ~100-200mV. We scale to represent the
     *  signal level at Q1's base after the input coupling network. */
    static constexpr double kInputVoltageScale = 0.5;

    // =========================================================================
    // WDF Q1 Clipping Circuit (First Transistor Stage)
    // =========================================================================

    /**
     * Wave Digital Filter model of the Q1 (first transistor) stage.
     *
     * This models the input buffer/gain stage of the Fuzz Face. Q1 is
     * an AC128 germanium PNP transistor operating as a common-emitter
     * amplifier with moderate gain (~15x). The collector-base junction
     * behavior is modeled by a germanium diode pair.
     *
     * Circuit sub-section:
     *
     *   Vin (guitar) ---[R_guitar]---+---[C1=2.2uF]---+
     *                                |                  |
     *                           [R1=33K bias]      [R_load=8.2K]
     *                                |                  |
     *                               GND            [Ge diodes]
     *                                                   |
     *                                                  GND
     *
     * WDF tree structure:
     *
     *        [DiodePairT] (ROOT -- sole non-adaptable element)
     *             |
     *       [WDFParallelT: pOuter]
     *         /              \
     *   [Rvs (R_guitar)]  [WDFSeriesT: sInner]
     *                       /           \
     *                [C1 (2.2uF)]   [WDFParallelT: pBias]
     *                                 /           \
     *                          [R1 (33K)]    [R_load (8.2K)]
     *
     * The ResistiveVoltageSource R_guitar is dynamically adjusted based
     * on the Guitar Volume parameter to model the impedance interaction.
     *
     * Germanium diode parameters (AC128 collector-base junction):
     *   Is = 5.0e-6 A  (germanium junction saturation current)
     *   Vt = 26e-3 V   (thermal voltage)
     *   nDiodes = 1.3  (ideality factor for Ge, higher than Si's ~1.0)
     *
     * NOTE: Is=5uA models the effective junction clipping behavior of a
     * Ge transistor in the WDF framework. The original Is=200uA was far
     * too high -- it caused the DiodePairT to present near-zero impedance
     * at ANY signal voltage, shorting the signal and producing zero output.
     * At Is=5uA, the diodes begin conducting around 0.15V and reach full
     * clipping by ~0.3V, matching the characteristic soft Ge knee.
     */
    struct Q1ClipperCircuit {
        // ---- WDF Leaf Components ----

        /** Resistive voltage source: combines input signal + R_guitar.
         *  R_guitar is dynamically adjusted based on Guitar Volume param
         *  to model the pickup-to-fuzz impedance interaction.
         *  Default 7K represents a low-impedance humbucker at full volume.
         *  At guitar vol=0, this increases to ~250K, starving the diodes. */
        chowdsp::wdft::ResistiveVoltageSourceT<double> rvs { 7000.0 };

        /** Input coupling capacitor C1 = 2.2uF.
         *  Large electrolytic cap passes the full guitar frequency range
         *  down to ~14Hz. This is unusually large for a guitar pedal
         *  (most use 47-100nF), preserving deep bass that contributes
         *  to the Fuzz Face's full, round low-end character. */
        chowdsp::wdft::CapacitorT<double> c1 { 2.2e-6 };

        /** Input bias resistor R1 = 33K.
         *  Sets the DC operating point of Q1's base and defines the
         *  input impedance of the pedal (~33K || Rbe). The relatively
         *  low value is why the Fuzz Face loads the guitar signal and
         *  interacts so strongly with the guitar volume pot. */
        chowdsp::wdft::ResistorT<double> rBias { 33000.0 };

        /** Collector load representing R3 = 8.2K.
         *  Q1's collector resistor to the -9V supply rail. Sets the
         *  operating point and provides the load across which the
         *  amplified signal develops. */
        chowdsp::wdft::ResistorT<double> rLoad { 8200.0 };

        // ---- WDF Tree Adaptors ----

        /** Inner parallel: rBias || rLoad (both to ground/supply) */
        chowdsp::wdft::WDFParallelT<double,
            decltype(rBias), decltype(rLoad)> pBias { rBias, rLoad };

        /** Series: c1 in series with the parallel bias/load network.
         *  Models C1 in the signal path between the guitar and the
         *  transistor's base-emitter junction. */
        chowdsp::wdft::WDFSeriesT<double,
            decltype(c1), decltype(pBias)> sInner { c1, pBias };

        /** Outer parallel: rvs || sInner (the main circuit junction) */
        chowdsp::wdft::WDFParallelT<double,
            decltype(rvs), decltype(sInner)> pOuter { rvs, sInner };

        // ---- Root Nonlinearity ----

        /** Germanium diode pair modeling Q1's collector-base junction.
         *  SOLE root of the WDF tree (non-adaptable nonlinearity).
         *
         *  AC128 Germanium parameters:
         *    Is = 5.0e-6 A  (Ge junction model; ~2000x higher than Si)
         *    Vt = 26e-3 V   (thermal voltage at ~25C)
         *    nDiodes = 1.3  (Ge ideality factor, higher = softer knee)
         *
         *  Is=5uA produces gradual clipping onset at ~0.15V, full clip
         *  by ~0.3V — the characteristic soft germanium sound. */
        chowdsp::wdft::DiodePairT<double, decltype(pOuter)> dp {
            pOuter, 5.0e-6, 26.0e-3, 1.3
        };

        // ---- Lifecycle ----

        /**
         * Prepare the WDF circuit at the given (oversampled) sample rate.
         *
         * @param sampleRate Oversampled sample rate in Hz.
         */
        void prepare(double sampleRate) {
            c1.prepare(sampleRate);
            dp.calcImpedance();
        }

        /** Reset all reactive element state to prevent startup artifacts. */
        void reset() {
            c1.reset();
        }

        /**
         * Process a single sample through the Q1 WDF clipper.
         *
         * Wave propagation:
         *   1. Set input voltage on the resistive voltage source
         *   2. Reflected waves propagate up from pOuter to diode pair
         *   3. Diode pair solves nonlinear equation (Wright Omega)
         *   4. Incident waves propagate back down, updating capacitor state
         *   5. Output voltage read across the coupling capacitor C1
         *
         * @param x Input voltage (pre-amplified guitar signal).
         * @return Clipped output voltage (~+/-0.3V range for Ge).
         */
        inline double processSample(double x) {
            rvs.setVoltage(x);

            dp.incident(pOuter.reflected());
            pOuter.incident(dp.reflected());

            // Read output across the collector load resistor (8.2K).
            // The amplified, Ge-clipped signal develops across rLoad.
            //
            // CRITICAL: Previously read from c1 (2.2uF coupling cap) which
            // has Z = 1/(2*C*fs) = 2.37 ohm at 96kHz — essentially zero
            // impedance relative to the 8.2K+33K resistive elements. Voltage
            // across c1 was negligible, producing zero output.
            //
            // rLoad (8.2K) is where the collector current develops a
            // meaningful voltage drop, just like in the real CE amplifier.
            return chowdsp::wdft::voltage<double>(rLoad);
        }
    };

    // =========================================================================
    // WDF Q2 Clipping Circuit (Second Transistor Stage)
    // =========================================================================

    /**
     * Wave Digital Filter model of the Q2 (second transistor) stage.
     *
     * Q2 is the main gain/clipping stage. It receives the amplified
     * output from Q1 and provides further amplification and heavier
     * germanium clipping. The FUZZ pot controls emitter degeneration
     * via a variable resistor to ground, modeled here as a variable
     * feedback resistance in the WDF tree.
     *
     * Circuit sub-section:
     *
     *   Vin (from Q1) ---[R3=8.2K]---+---[C2=20uF]---+
     *                                 |                 |
     *                            [R2=470 ohm]    [Rfuzz=1K pot]
     *                                 |                 |
     *                                -9V              GND
     *                                 |
     *                            [Ge diodes]
     *                                 |
     *                                GND
     *
     * WDF tree structure:
     *
     *        [DiodePairT] (ROOT -- sole non-adaptable element)
     *             |
     *       [WDFParallelT: pOuter]
     *         /              \
     *   [Rvs (8.2K)]    [WDFSeriesT: sInner]
     *                     /           \
     *              [C2 (20uF)]   [WDFParallelT: pFeedback]
     *                              /           \
     *                      [R2 (470)]    [Rfuzz (variable)]
     *
     * The Rfuzz resistance is dynamically adjusted based on the Fuzz
     * parameter. Lower Rfuzz = more emitter bypass = more gain/clipping.
     *
     * Same germanium diode parameters as Q1 (AC128):
     *   Is = 5.0e-6 A, Vt = 26e-3 V, nDiodes = 1.3
     */
    struct Q2ClipperCircuit {
        // ---- WDF Leaf Components ----

        /** Resistive voltage source: R3 = 8.2K (Q1 collector impedance).
         *  This represents the output impedance of the first stage
         *  driving into the second stage. */
        chowdsp::wdft::ResistiveVoltageSourceT<double> rvs { 8200.0 };

        /** Emitter bypass capacitor C2 = 20uF.
         *  Large electrolytic that bypasses the emitter resistor at
         *  audio frequencies. The large value ensures full bypass down
         *  to very low frequencies: fc = 1/(2*pi*1K*20uF) = ~8Hz.
         *  This means the emitter degeneration is only effective at DC
         *  (for bias stability), not at audio frequencies. */
        chowdsp::wdft::CapacitorT<double> c2 { 20.0e-6 };

        /** Collector load R2 = 470 ohm.
         *  Q2's collector resistor to the -9V supply. The low value
         *  (compared to Q1's 8.2K) allows Q2 to swing larger currents,
         *  producing harder clipping. */
        chowdsp::wdft::ResistorT<double> rCollector { 470.0 };

        /** Fuzz pot (variable emitter resistance): 50-1000 ohm.
         *  Models the 1K FUZZ pot that controls emitter degeneration.
         *  Dynamically updated based on the Fuzz parameter.
         *  Low R = full bypass (max fuzz), High R = degeneration (less fuzz). */
        chowdsp::wdft::ResistorT<double> rFeedback { 50.0 };

        // ---- WDF Tree Adaptors ----

        /** Inner parallel: rCollector || rFeedback */
        chowdsp::wdft::WDFParallelT<double,
            decltype(rCollector), decltype(rFeedback)> pFeedback {
                rCollector, rFeedback };

        /** Series: c2 in series with the parallel collector/feedback */
        chowdsp::wdft::WDFSeriesT<double,
            decltype(c2), decltype(pFeedback)> sInner { c2, pFeedback };

        /** Outer parallel: rvs || sInner */
        chowdsp::wdft::WDFParallelT<double,
            decltype(rvs), decltype(sInner)> pOuter { rvs, sInner };

        // ---- Root Nonlinearity ----

        /** Germanium diode pair modeling Q2's collector-base junction.
         *  Same AC128 parameters as Q1 (Is=5uA, Ge ideality=1.3).
         *  Q2 typically has higher beta (110-130 vs 70-80), but the
         *  diode junction characteristics are the same transistor type.
         *  Higher gain is modeled externally (q2Gain multiplier). */
        chowdsp::wdft::DiodePairT<double, decltype(pOuter)> dp {
            pOuter, 5.0e-6, 26.0e-3, 1.3
        };

        // ---- Lifecycle ----

        void prepare(double sampleRate) {
            c2.prepare(sampleRate);
            dp.calcImpedance();
        }

        void reset() {
            c2.reset();
        }

        /**
         * Process a single sample through the Q2 WDF clipper.
         *
         * @param x Input voltage from the Q1 stage (amplified and clipped).
         * @return Clipped output voltage with heavier germanium saturation.
         */
        inline double processSample(double x) {
            rvs.setVoltage(x);

            dp.incident(pOuter.reflected());
            pOuter.incident(dp.reflected());

            // Read output across the collector resistor (470 ohm).
            //
            // CRITICAL: Previously read from c2 (20uF bypass cap) which
            // has Z = 1/(2*C*fs) = 0.26 ohm at 96kHz — virtually zero
            // compared to the 470+variable ohm resistive elements. The
            // voltage across c2 was negligible, producing zero output.
            //
            // rCollector (470 ohm) carries the clipping current and
            // develops the output voltage of the Q2 amplifier stage.
            return chowdsp::wdft::voltage<double>(rCollector);
        }
    };

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

    /** Effective sample rate after oversampling. */
    double oversampledRate_ = 88200.0;

    /** 2x oversampler for the nonlinear WDF stages.
     *  Germanium's soft clipping generates fewer high-order harmonics
     *  than silicon hard clipping, so 2x provides adequate alias
     *  rejection while keeping CPU load minimal on mobile ARM. */
    Oversampler<kOversampleFactor> oversampler_;

    /** WDF Q1 clipping circuit (first transistor stage, moderate gain). */
    Q1ClipperCircuit q1Circuit_;

    /** WDF Q2 clipping circuit (second transistor stage, main clipping). */
    Q2ClipperCircuit q2Circuit_;

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
