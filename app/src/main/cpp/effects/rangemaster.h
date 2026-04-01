#ifndef GUITAR_ENGINE_RANGEMASTER_H
#define GUITAR_ENGINE_RANGEMASTER_H

#include "effect.h"
#include "oversampler.h"
#include "fast_math.h"
#include <chowdsp_wdf/chowdsp_wdf.h>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * Circuit-accurate emulation of the Dallas Rangemaster Treble Booster (1966).
 *
 * The Rangemaster is a single-transistor germanium treble booster that became
 * a cornerstone of British rock tone. Players like Tony Iommi, Brian May, and
 * Rory Gallagher used it to push tube amps into singing, harmonically rich
 * overdrive. Unlike a fuzz pedal, the Rangemaster is fundamentally a BOOSTER:
 * the signal stays mostly clean at moderate settings, with subtle germanium
 * coloring that adds warmth and "sweetness" from even-order harmonics.
 *
 * The circuit's defining characteristic is the tiny 4.7nF input coupling
 * capacitor (C1). In a standard guitar amplifier, input caps are typically
 * 22nF-100nF to pass the full frequency range. The Rangemaster's 4.7nF
 * creates a high-pass filter with the guitar's pickup impedance:
 *
 *   fc = 1 / (2 * pi * R_guitar * C1)
 *      = 1 / (2 * pi * 7000 * 4.7e-9)
 *      ~ 4.8 kHz
 *
 * This means low frequencies are dramatically attenuated before reaching the
 * transistor. Only treble content passes through to be amplified, giving the
 * Rangemaster its signature bright, cutting character. When placed before an
 * already-overdriven tube amp, the treble boost pushes the amp's preamp into
 * harder clipping in the upper harmonics, creating a thick, singing lead tone
 * with excellent note definition.
 *
 * Circuit topology (single common-emitter amplifier):
 *
 *   Guitar --[C1: 4.7nF]--+--[R1: 68K]-- Vcc (9V)
 *                          |
 *                          +-- Base (Q1: OC44 PNP Germanium)
 *                          |
 *           Collector --[R3: 10K]-- Vcc
 *                |
 *              Output --[C3: 10uF]-- Out
 *                          |
 *           Emitter --[R4: 3.9K]--+-- GND
 *                                 |
 *                            [C2: 47uF] (bypass cap)
 *                                 |
 *                                GND
 *
 *   [R2: 470K] from collector to base (DC feedback for bias stability)
 *   [Range pot: 10K] in series with R4 (varies emitter degeneration)
 *
 * Component values (from original Dallas schematic):
 *   C1  = 4.7 nF   Input coupling / treble emphasis cap (THE key component)
 *   R1  = 68 K     Base bias resistor to Vcc
 *   R2  = 470 K    Collector-to-base feedback resistor
 *   R3  = 10 K     Collector load resistor
 *   R4  = 3.9 K    Emitter resistor (DC stabilization)
 *   C2  = 47 uF    Emitter bypass capacitor
 *   C3  = 10 uF    Output coupling / DC blocking cap
 *   Q1  = OC44     PNP germanium transistor (Is~200uA, Vt~26mV)
 *   Range pot = 10K (varies effective emitter impedance)
 *
 * Germanium transistor characteristics:
 *   The OC44 germanium transistor has a much higher saturation current
 *   (Is ~ 5e-6 A) compared to silicon (Is ~ 2.5e-9 A). This means:
 *   - The diode junction conducts much more gradually (soft knee)
 *   - Clipping onset is gentle and progressive, not abrupt
 *   - Even-order harmonics dominate (asymmetric transfer curve)
 *   - The "warm" and "sweet" character that players describe
 *
 * Anti-aliasing strategy:
 *   2x oversampling around the nonlinear germanium clipping stage. The
 *   Rangemaster produces gentler harmonics than hard-clipping pedals (DS-1,
 *   RAT), so 2x oversampling is more than adequate. The 4.7nF input cap
 *   already band-limits the signal entering the transistor, further reducing
 *   aliasing risk.
 *
 * Parameter IDs (for JNI access):
 *   0 = Range  (0.0 to 1.0) - Gain / bias point (emitter resistance variation)
 *   1 = Volume (0.0 to 1.0) - Output level
 *
 * References:
 *   - Electrosmash: https://www.electrosmash.com/dallas-rangemaster
 *   - R.G. Keen, "The Technology of the Dallas Rangemaster"
 *   - chowdsp_wdf: https://github.com/Chowdhury-DSP/chowdsp_wdf
 */
class Rangemaster : public Effect {
public:
    Rangemaster() = default;

    // =========================================================================
    // Effect interface
    // =========================================================================

    /**
     * Process audio through the Rangemaster treble booster circuit. Real-time safe.
     *
     * Signal flow:
     *   Input -> Treble emphasis high-pass (C1 = 4.7nF, models guitar impedance)
     *         -> Pre-gain (transistor amplification, Range-dependent)
     *         -> 2x Upsample
     *         -> WDF Germanium Clipper (soft Ge clipping, OC44 transistor)
     *         -> 2x Downsample
     *         -> Output coupling high-pass (C3 = 10uF, ~3Hz DC block)
     *         -> Output volume
     *
     * @param buffer   Mono audio samples in [-1.0, 1.0].
     * @param numFrames Number of frames in the buffer.
     */
    void process(float* buffer, int numFrames) override {
        // Guard against being called before setSampleRate() allocates buffers.
        if (preOsBuffer_.empty()) return;

        // ------------------------------------------------------------------
        // Snapshot all parameters once per block. Using relaxed ordering is
        // safe here: we only need atomicity (no torn reads), not ordering
        // guarantees relative to other memory operations. The UI thread
        // writes these; the audio thread reads them.
        // ------------------------------------------------------------------
        const float range  = range_.load(std::memory_order_relaxed);
        const float volume = volume_.load(std::memory_order_relaxed);

        // ------------------------------------------------------------------
        // Pre-compute per-block constants outside the sample loop.
        // ------------------------------------------------------------------

        // Range controls the effective emitter resistance, which determines
        // the voltage gain of the common-emitter stage.
        //
        // In the original circuit, the Range pot (10K) is in series with R4
        // (3.9K emitter resistor). Turning the pot up REDUCES the total
        // emitter impedance (pot resistance shorts to ground through C2),
        // which INCREASES gain.
        //
        // Voltage gain: Av = -R3 / (R4 + R_range_remaining)
        //   range=0 (pot fully CCW): R_emitter = 3.9K + 10K = 13.9K, Av ~ 0.7
        //   range=0.5:               R_emitter ~ 3.9K + 5K = 8.9K,  Av ~ 1.1
        //   range=1 (pot fully CW):  R_emitter ~ 3.9K (bypassed),   Av ~ 2.6
        //
        // With the emitter bypass cap (C2=47uF), AC gain at treble frequencies
        // is much higher because C2 shorts R4 for AC signals above ~1Hz:
        //   fc_bypass = 1 / (2*pi*R4*C2) = 1 / (2*pi*3900*47e-6) ~ 0.87 Hz
        //
        // So at audio frequencies, the gain is primarily set by R3 and the
        // remaining pot resistance:
        //   AC gain at range=1: Av ~ R3 / r_e ~ 10K / 26 ohm ~ 385 (52 dB)
        //   (where r_e = Vt/Ic ~ 26mV/1mA ~ 26 ohm is the transistor's
        //    intrinsic emitter resistance)
        //
        // We map range [0,1] to a practical gain range that produces the
        // expected treble boost character: moderate boost at low settings,
        // progressively more drive at higher settings.
        const float rangeResistance = 10.0e3f * (1.0f - range); // Remaining pot R
        const float emitterR = 3.9e3f + rangeResistance;
        const float acGain = 10.0e3f / emitterR; // R3 / R_emitter

        // Scale the input to realistic circuit voltage levels for the WDF.
        // Guitar signal is ~100mV peak; the transistor stage amplifies this
        // to a few volts. The WDF expects voltages in the range where the
        // Ge diode model is accurate (roughly 0-2V for germanium).
        constexpr float kInputScaling = 2.0f;
        const float preClipGain = acGain * kInputScaling;

        // Output volume with audio taper for natural feel.
        // Audio taper: volume^2 maps [0,1] to perceptually linear loudness.
        const float outputGain = volume * volume;

        // Post-clipping output scaling: the WDF Ge clipper outputs voltages
        // that are lower than silicon (Ge forward voltage ~ 0.3V vs 0.7V).
        // Normalize back to [-1, 1] audio range.
        constexpr float kOutputScaling = 1.0f / 0.35f;

        // ------------------------------------------------------------------
        // Per-sample processing: treble emphasis + gain stage
        // ------------------------------------------------------------------
        for (int i = 0; i < numFrames; ++i) {
            float x = buffer[i];

            // ----- Treble emphasis high-pass (models C1 = 4.7nF) -----
            //
            // The 4.7nF input cap forms a high-pass with the guitar's pickup
            // impedance (~7K for humbuckers). This is THE defining element of
            // the Rangemaster -- it makes it a treble booster rather than a
            // full-range booster.
            //
            //   fc = 1 / (2*pi * R_guitar * C1)
            //      = 1 / (2*pi * 7000 * 4.7e-9)
            //      ~ 4831 Hz (-3dB point)
            //
            // The roll-off is -6dB/octave (first-order), so:
            //   - 2.4 kHz: -6 dB
            //   - 1.2 kHz: -12 dB
            //   - 600 Hz:  -18 dB
            //   - 300 Hz:  -24 dB
            //
            // This is a steep attenuation of bass/mids by guitar standards.
            //
            // Double precision for the HP filter: although 4.8kHz is high
            // enough for float, we use double for consistency with the
            // codebase pattern and to maintain accuracy at lower sample rates.
            double xd = static_cast<double>(x);
            double hpTrebleOut = hpTrebleCoeff_ * (hpTrebleState_ + xd - hpTreblePrevInput_);
            hpTreblePrevInput_ = xd;
            hpTrebleState_ = fast_math::denormal_guard(hpTrebleOut);
            x = static_cast<float>(hpTrebleState_);

            // ----- Gain stage (common-emitter amplification) -----
            x *= preClipGain;

            // Store in pre-oversampling buffer
            preOsBuffer_[i] = x;
        }

        // ------------------------------------------------------------------
        // WDF Germanium Clipping with 2x oversampling
        // ------------------------------------------------------------------
        const int oversampledLen = oversampler_.getOversampledSize(numFrames);
        float* upsampled = oversampler_.upsample(preOsBuffer_.data(), numFrames);

        // Process each oversampled sample through the WDF Ge clipper.
        // Double precision for numerical stability in the diode equation
        // solver (Wright Omega approximation).
        for (int i = 0; i < oversampledLen; ++i) {
            upsampled[i] = static_cast<float>(
                geClipperCircuit_.processSample(static_cast<double>(upsampled[i]))
            );
        }

        // Downsample back to the original rate
        oversampler_.downsample(upsampled, numFrames, postOsBuffer_.data());

        // ------------------------------------------------------------------
        // Output coupling + volume
        // ------------------------------------------------------------------
        for (int i = 0; i < numFrames; ++i) {
            float x = postOsBuffer_[i];

            // Normalize clipped signal back to audio range
            x *= kOutputScaling;

            // ----- Output coupling high-pass (C3 = 10uF) -----
            //
            // The output coupling capacitor blocks DC offset from the
            // transistor's collector bias point (~4-5V). With a typical
            // load impedance of ~1M (next stage input):
            //   fc = 1 / (2*pi * 1M * 10e-6) ~ 0.016 Hz
            //
            // We model this at 3 Hz to also serve as a practical DC blocker
            // that removes any DC offset from the WDF processing.
            // Uses double precision: 3Hz HP coefficient is very close to 1.0,
            // and float cannot represent the difference accurately.
            double xd = static_cast<double>(x);
            double hpOutCouplingOut = hpOutputCoeff_ * (hpOutputState_ + xd - hpOutputPrevInput_);
            hpOutputPrevInput_ = xd;
            hpOutputState_ = fast_math::denormal_guard(hpOutCouplingOut);
            x = static_cast<float>(hpOutputState_);

            // Apply output volume and clamp to prevent downstream clipping
            x *= outputGain;
            buffer[i] = clamp(x, -1.0f, 1.0f);
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
        const int maxFrames = 2048;
        preOsBuffer_.resize(maxFrames, 0.0f);
        postOsBuffer_.resize(maxFrames, 0.0f);

        // Prepare the 2x oversampler
        oversampler_.prepare(maxFrames);

        // Prepare the WDF clipping circuit at the oversampled rate.
        // The capacitors in the WDF tree are reactive elements whose
        // discretization depends on the sample rate (bilinear transform
        // maps s-domain to z-domain, and the mapping depends on T=1/fs).
        const double oversampledRate = static_cast<double>(sampleRate) * 2.0;
        geClipperCircuit_.prepare(oversampledRate);

        // Pre-compute fixed filter coefficients. All cutoff frequencies
        // are determined by circuit component values and do not change
        // at runtime. Computing once here avoids std::exp() per block.
        //
        // Treble emphasis HP: C1(4.7nF) with ~7K guitar impedance ~ 4831 Hz
        hpTrebleCoeff_ = computeOnePoleHPCoeffDouble(4831.0);

        // Output coupling HP: C3(10uF), modeled at 3 Hz for DC blocking
        hpOutputCoeff_ = computeOnePoleHPCoeffDouble(3.0);
    }

    /**
     * Reset all internal filter and WDF state.
     * Called when the audio stream restarts or the effect is re-enabled
     * to prevent stale samples from leaking through as clicks/pops.
     */
    void reset() override {
        oversampler_.reset();
        geClipperCircuit_.reset();

        // Reset all one-pole filter states
        hpTrebleState_ = 0.0;
        hpTreblePrevInput_ = 0.0;
        hpOutputState_ = 0.0;
        hpOutputPrevInput_ = 0.0;
    }

    // =========================================================================
    // Parameter access (called from UI thread via JNI)
    // =========================================================================

    /**
     * Set a parameter by ID. Thread-safe via atomic storage.
     *
     * @param paramId  0=Range, 1=Volume
     * @param value    Parameter value in [0.0, 1.0].
     */
    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0:
                range_.store(clamp(value, 0.0f, 1.0f),
                             std::memory_order_relaxed);
                break;
            case 1:
                volume_.store(clamp(value, 0.0f, 1.0f),
                              std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    /**
     * Get a parameter by ID. Thread-safe via atomic load.
     *
     * @param paramId  0=Range, 1=Volume
     * @return Current parameter value, or 0.0f if paramId is unknown.
     */
    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return range_.load(std::memory_order_relaxed);
            case 1: return volume_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    // =========================================================================
    // WDF Germanium Clipper Sub-Circuit
    // =========================================================================

    /**
     * Wave Digital Filter model of the Rangemaster's germanium clipping stage.
     *
     * This models the nonlinear behavior of the OC44 PNP germanium transistor's
     * collector-emitter junction. The actual transistor is a three-terminal device,
     * but for the purposes of the audio signal path, the clipping behavior is
     * dominated by the base-emitter junction's conduction characteristics. We
     * simplify to a diode pair model with germanium parameters, which captures
     * the essential soft-clipping character.
     *
     * Circuit topology modeled:
     *
     *   Vin ---[ R_source ]---+---[ C_couple (47nF) ]--- GND
     *                         |
     *                         +---[ R_collector (10K) ]--- GND
     *                         |
     *                     Ge diode pair (OC44 model)
     *
     * The source resistance combines the guitar impedance seen through C1 with
     * R1 (68K base bias). The coupling capacitor (47nF) provides DC blocking
     * within the WDF tree. The collector load resistor (R3 = 10K) sets the
     * output impedance and swing range.
     *
     * WDF tree structure (bottom-up):
     *
     *       [DiodePairT] (ROOT -- sole non-adaptable element)
     *            |
     *      [WDFParallelT: pOuter]
     *        /              \
     *  [Rvs (4.7K)]   [WDFParallelT: pInner]
     *                    /           \
     *             [C_couple (47nF)] [R_collector (10K)]
     *
     * ResistiveVoltageSourceT combines the input signal and R_source into
     * a single adapted (non-root) leaf. DiodePairT is the sole root
     * (non-adaptable nonlinearity, solved via Wright Omega function).
     *
     * Germanium diode parameters:
     *   Is = 5.0e-6 A   (OC44 Ge junction model -- ~2,000x higher than Si)
     *   Vt = 26e-3 V    (thermal voltage at room temperature, kT/q)
     *
     * The extremely high Is value compared to silicon (2.52e-9 A) means the
     * germanium junction conducts much more gradually. Rather than a sharp
     * knee at 0.6-0.7V (silicon), germanium starts conducting around 0.15-0.3V
     * with a very soft, progressive transition. This produces:
     *   - Gentle onset of clipping (no harsh transition)
     *   - Lower clipping threshold (~0.3V vs ~0.7V for silicon)
     *   - Asymmetric even-order harmonics (warm, musical character)
     *   - Volume-responsive dynamics (cleans up when guitar volume is lowered)
     */
    struct GeClipperCircuit {
        // --------------------------------------------------------------------
        // Leaf components
        // --------------------------------------------------------------------

        /**
         * Resistive voltage source: combines input signal + R_source.
         *
         * R_source = 4.7K represents the combined impedance seen by the
         * clipping junction: the collector-side source impedance plus
         * the emitter resistance contribution. This value is chosen to
         * produce the correct signal swing and clipping behavior in the
         * WDF simulation.
         */
        chowdsp::wdft::ResistiveVoltageSourceT<double> rvs { 4700.0 };

        /**
         * DC-blocking coupling capacitor, 47nF.
         *
         * This models the AC coupling within the transistor stage. In the
         * actual circuit, C2 (47uF emitter bypass) and the transistor's
         * internal capacitances provide frequency-dependent behavior.
         * We use 47nF in the WDF to provide appropriate high-pass behavior
         * within the WDF tree (fc ~ 72 Hz with R_source), ensuring the
         * WDF doesn't accumulate DC from the diode model.
         */
        chowdsp::wdft::CapacitorT<double> cCouple { 47.0e-9 };

        /**
         * Collector load resistor, 10K ohm (R3 in the original circuit).
         *
         * This sets the output impedance and voltage swing range of the
         * transistor stage. In a common-emitter amplifier, the collector
         * load determines maximum output voltage swing: Vswing = Ic * R3.
         */
        chowdsp::wdft::ResistorT<double> rCollector { 10.0e3 };

        // --------------------------------------------------------------------
        // WDF tree adaptors
        // --------------------------------------------------------------------

        /** Inner parallel: cCouple || rCollector (both to ground from junction) */
        chowdsp::wdft::WDFParallelT<double,
            decltype(cCouple), decltype(rCollector)> pInner { cCouple, rCollector };

        /** Outer parallel: rvs || pInner (the circuit junction node) */
        chowdsp::wdft::WDFParallelT<double,
            decltype(rvs), decltype(pInner)> pOuter { rvs, pInner };

        // --------------------------------------------------------------------
        // Root nonlinearity (ONLY non-adaptable element)
        // --------------------------------------------------------------------

        /**
         * Germanium diode pair (models OC44 transistor junction clipping).
         * SOLE root of the WDF tree. Solved via Wright Omega function.
         *
         *   Is = 5.0e-6 A  (Ge junction model, ~2,000x higher than silicon)
         *   Vt = 26e-3 V   (thermal voltage at room temperature)
         *
         * The high Is value produces the characteristic germanium soft-clipping:
         * gradual onset around 0.15V, full conduction by ~0.3V. This contrasts
         * with silicon's sharp knee at 0.6V. The result is warm, musical
         * compression with rich even-order harmonics.
         */
        chowdsp::wdft::DiodePairT<double, decltype(pOuter)> dp {
            pOuter, 5.0e-6, 26.0e-3, 1.3
        };

        // --------------------------------------------------------------------
        // Lifecycle methods
        // --------------------------------------------------------------------

        /**
         * Prepare the WDF circuit for a given sample rate.
         * Must be called before processSample(). The capacitor's
         * discretization (bilinear transform: s -> 2/T * (z-1)/(z+1))
         * depends on the sample period T = 1/sampleRate.
         *
         * @param sampleRate Sample rate in Hz (should be the oversampled rate).
         */
        void prepare(double sampleRate) {
            cCouple.prepare(sampleRate);
            dp.calcImpedance();
        }

        /**
         * Reset the internal state of all reactive WDF elements.
         * Capacitors store state (voltage from previous sample) via the
         * bilinear transform discretization. Resetting clears this state
         * to prevent artifacts when the effect restarts.
         */
        void reset() {
            cCouple.reset();
        }

        /**
         * Process a single sample through the WDF germanium clipper.
         *
         * Wave propagation sequence:
         *   1. Set the input voltage on the voltage source
         *   2. Reflect waves from the parallel junction up to the diode pair
         *   3. The diode pair computes its reflected wave (nonlinear solve)
         *   4. Propagate the reflected wave back down to the parallel junction
         *   5. Read the output voltage across the coupling capacitor
         *
         * The output is taken across cCouple, which provides DC blocking
         * and a natural frequency-dependent roll-off.
         *
         * @param x Input voltage (amplified guitar signal after treble emphasis).
         * @return Output voltage after germanium clipping, approximately +/-0.3V.
         */
        inline double processSample(double x) {
            rvs.setVoltage(x);

            // Propagate waves: reflected wave travels up from pOuter to
            // the diode pair, which computes the scattered (reflected)
            // wave via Wright Omega, then propagates back down.
            dp.incident(pOuter.reflected());
            pOuter.incident(dp.reflected());

            // Read output voltage across the coupling capacitor.
            // This gives us the clipped signal with DC blocking.
            return chowdsp::wdft::voltage<double>(cCouple);
        }
    };

    // =========================================================================
    // One-Pole Filter Coefficient Computation
    // =========================================================================

    /**
     * Compute one-pole high-pass filter coefficient (double precision).
     *
     * For the difference equation:
     *   y[n] = a * (y[n-1] + x[n] - x[n-1])
     *
     * where a = 1 / (1 + 2*pi*fc/fs).
     *
     * @param cutoffHz Desired cutoff frequency in Hz.
     * @return Filter coefficient 'a' in double precision.
     */
    double computeOnePoleHPCoeffDouble(double cutoffHz) const {
        if (sampleRate_ <= 0) return 0.99;
        const double fs = static_cast<double>(sampleRate_);
        cutoffHz = std::max(1.0, std::min(cutoffHz, fs * 0.49));
        const double wc = 2.0 * M_PI * cutoffHz;
        return 1.0 / (1.0 + wc / fs);
    }

    /**
     * Clamp a value to the range [minVal, maxVal].
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

    /** Range control [0, 1]. Controls gain via emitter resistance variation.
     *  0 = low gain (mild treble boost), 1 = high gain (germanium overdrive).
     *  Default 0.5 gives moderate treble boost with subtle Ge coloring. */
    std::atomic<float> range_{0.5f};

    /** Output volume [0, 1]. Controls final level after the booster stage.
     *  Default 0.5 provides moderate boost. */
    std::atomic<float> volume_{0.5f};

    // =========================================================================
    // Audio thread state (not accessed from UI thread)
    // =========================================================================

    /** Device sample rate in Hz */
    int32_t sampleRate_ = 44100;

    /**
     * 2x oversampler for the nonlinear germanium clipping stage.
     * 2x is chosen because the Rangemaster produces much gentler harmonics
     * than hard-clipping pedals. The 4.7nF input cap band-limits the signal
     * entering the clipper to treble frequencies, further reducing aliasing.
     * 2x pushes the effective Nyquist to 88.2kHz (at 44.1kHz base rate),
     * providing ample headroom for the soft germanium clipping harmonics.
     */
    Oversampler<2> oversampler_;

    /** WDF germanium clipper sub-circuit (double precision) */
    GeClipperCircuit geClipperCircuit_;

    // ---- Pre-allocated buffers ----

    /** Buffer for signal after treble emphasis/gain, before oversampling */
    std::vector<float> preOsBuffer_;

    /** Buffer for signal after oversampling/clipping, before output stage */
    std::vector<float> postOsBuffer_;

    // ---- Pre-computed filter coefficients (set in setSampleRate()) ----

    double hpTrebleCoeff_ = 0.99;   ///< Treble emphasis HP (4831 Hz)
    double hpOutputCoeff_ = 0.99;   ///< Output coupling HP (3 Hz)

    // ---- One-pole filter states (double precision for stability) ----

    /** Treble emphasis high-pass filter state (C1 = 4.7nF, fc ~ 4831 Hz) */
    double hpTrebleState_ = 0.0;

    /** Previous input sample for the treble emphasis high-pass */
    double hpTreblePrevInput_ = 0.0;

    /** Output coupling high-pass filter state (C3 = 10uF, modeled at 3 Hz) */
    double hpOutputState_ = 0.0;

    /** Previous input sample for the output coupling high-pass */
    double hpOutputPrevInput_ = 0.0;
};

#endif // GUITAR_ENGINE_RANGEMASTER_H
