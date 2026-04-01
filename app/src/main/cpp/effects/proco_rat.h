#ifndef GUITAR_ENGINE_PROCO_RAT_H
#define GUITAR_ENGINE_PROCO_RAT_H

#include "effect.h"
#include "oversampler.h"
#include "fast_math.h"

#include <chowdsp_wdf/chowdsp_wdf.h>

#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * Circuit-accurate emulation of the ProCo RAT distortion pedal.
 *
 * The RAT is one of the most influential distortion pedals ever made (1978).
 * Its unique character comes from three design choices:
 *
 *   1. The LM308 op-amp's extremely slow slew rate (0.3 V/us) which compresses
 *      and rounds transients BEFORE clipping, creating a thick, saturated tone
 *      that no other op-amp produces.
 *
 *   2. Hard clipping via 1N914 silicon diodes to ground (not in the feedback
 *      loop like a Tube Screamer). This creates aggressive, buzzy distortion
 *      with sharp harmonic content.
 *
 *   3. A passive low-pass "Filter" control wired in REVERSE -- fully clockwise
 *      is dark (heavy LPF at ~482 Hz), fully counter-clockwise is bright
 *      (essentially bypassed). Most players run it around 10-11 o'clock.
 *
 * Signal flow models the actual circuit:
 *
 *   Guitar Input
 *       |
 *   [C1=22nF + R1=1M input coupling high-pass, fc ~7.2 Hz]
 *       |
 *   [LM308 Op-Amp Gain Stage]
 *       - Non-inverting configuration
 *       - RDIST (100K log pot) + C4 (100pF) in feedback
 *       - R4 = 47 ohm ground resistor
 *       - C3 = 30pF compensation cap (slew rate limiter)
 *       - Gain = 1 + RDIST/R4, range: 1x to ~2128x (67 dB)
 *       - Slew rate: 0.3 V/us (modeled explicitly)
 *       |
 *   [WDF Diode Clipper]
 *       - R_out = 4.7K (op-amp output impedance + series resistance)
 *       - C_couple = 1uF (DC blocking cap between gain stage and diodes)
 *       - D1, D2: 1N914 silicon diodes, antiparallel to ground
 *       - R_load = 1M (input impedance of filter stage)
 *       - Modeled with chowdsp::wdft::DiodePairT (Wright Omega solver)
 *       |
 *   [Passive LPF "Filter" Control]
 *       - RFILTER (0-100K) + C8 (3.3nF)
 *       - fc range: ~482 Hz (max R) to bypass (min R)
 *       - REVERSED mapping: knob 1.0 = dark, 0.0 = bright
 *       |
 *   [Volume Control]
 *       - Output level attenuator
 *       |
 *   Output
 *
 * Implementation details:
 *   - 2x oversampling for anti-aliasing of the nonlinear clipping stage
 *   - Double precision for all WDF processing to maintain numerical stability
 *   - Input coupling high-pass modeled as a one-pole digital filter
 *   - Op-amp gain stage modeled as frequency-dependent gain + slew limiter
 *   - WDF tree for the diode clipping section using chowdsp_wdf components
 *   - One-pole LPF for the passive filter control
 *   - All parameters use std::atomic<float> for lock-free thread safety
 *
 * Parameter IDs (for JNI access):
 *   0 = Distortion (0.0 to 1.0, default 0.5)
 *   1 = Filter     (0.0 to 1.0, default 0.5) -- reversed: 1.0 = dark
 *   2 = Volume     (0.0 to 1.0, default 0.5)
 *
 * References:
 *   - Electrosmash ProCo RAT Analysis: https://www.electrosmash.com/proco-rat
 *   - LM308 datasheet (National Semiconductor)
 *   - chowdsp_wdf: https://github.com/Chowdhury-DSP/chowdsp_wdf
 */
class ProCoRAT : public Effect {
public:
    ProCoRAT() = default;

    // =========================================================================
    // Effect interface
    // =========================================================================

    /**
     * Process audio through the ProCo RAT circuit model. Real-time safe.
     *
     * The processing chain mirrors the actual pedal's signal flow:
     * input coupling -> gain stage -> slew limiter -> WDF diode clipper
     * -> passive LPF -> volume output.
     *
     * @param buffer   Mono audio samples in [-1.0, 1.0].
     * @param numFrames Number of frames in the buffer.
     */
    void process(float* buffer, int numFrames) override {
        // Guard against being called before setSampleRate() allocates buffers.
        if (workBuffer_.empty()) return;

        // Snapshot all parameters atomically at the start of the block.
        // Using relaxed ordering since we only need eventual consistency
        // between the UI thread writer and audio thread reader.
        const float distortion = distortion_.load(std::memory_order_relaxed);
        const float filter     = filter_.load(std::memory_order_relaxed);
        const float volume     = volume_.load(std::memory_order_relaxed);

        // =====================================================================
        // Compute derived values from parameters (once per block)
        // =====================================================================

        // --- Distortion -> Gain mapping ---
        // The RAT's distortion pot is 100K log taper in the op-amp feedback.
        // Gain = 1 + R_dist / R4 where R4 = 47 ohms.
        // At minimum: R_dist ~ 0 -> gain ~ 1 (clean)
        // At maximum: R_dist = 100K -> gain = 1 + 100000/47 = 2128.6 (~67 dB)
        //
        // Log taper approximation: squaring the parameter models the
        // logarithmic potentiometer taper, giving a perceptually linear
        // sweep from clean to full distortion across the knob range.
        const double minGain = 1.0;
        const double maxGain = 2128.0;
        const double gain = minGain + (maxGain - minGain)
                            * static_cast<double>(distortion * distortion);

        // --- C4 (100pF) feedback cap frequency-dependent gain rolloff ---
        // C4 in parallel with R_dist causes the gain to roll off at high
        // frequencies. The -3dB point is: fc = 1 / (2*pi*R_dist*C4)
        // At full distortion (100K): fc = 1/(2*pi*100K*100pF) = ~15.9 kHz
        // At low distortion (small R): cutoff is very high (negligible effect)
        //
        // The effective R_dist for the cutoff calculation uses the squared
        // taper to match the gain mapping above.
        const double rDist = 100.0 + static_cast<double>(distortion * distortion)
                             * 100000.0;
        const double c4Value = 100.0e-12; // 100pF
        const double fcGainRolloff = 1.0 / (2.0 * M_PI * rDist * c4Value);
        const double gainRolloffCoeff = computeOnePoleCoeff(fcGainRolloff,
                                                            oversampledRate_);

        // --- Slew rate parameters ---
        // The LM308 has a slew rate of 0.3 V/us = 300,000 V/s.
        // At the oversampled rate, the maximum voltage change per sample is:
        //   maxDelta = slewRate / oversampledSampleRate
        //
        // This is THE defining characteristic of the RAT's sound. The slow
        // slew rate acts as a hard limiter on the rate of change, rounding
        // off transients and adding compression before the diode clipper.
        // Without this, the RAT sounds like a generic hard clipper.
        const double maxSlewDelta = kSlewRateVPerSec / oversampledRate_;

        // --- Filter -> Cutoff mapping (REVERSED) ---
        // The RAT's filter is a simple passive RC low-pass:
        //   R_filter = 0 to 100K (log pot), C8 = 3.3nF
        //   fc = 1 / (2*pi*R*C)
        //
        // REVERSED wiring: filter knob at 0 = minimum resistance (bright),
        // filter knob at 1 = maximum resistance (dark, ~482 Hz).
        //
        // Exponential mapping for perceptually smooth sweep:
        //   cutoff = maxCutoff * (minCutoff / maxCutoff) ^ filter
        // This sweeps exponentially from 32 kHz (bright) to 482 Hz (dark).
        const double filterCutoff = kFilterMaxCutoffHz
            * std::pow(kFilterMinCutoffHz / kFilterMaxCutoffHz,
                       static_cast<double>(filter));
        const double filterCoeff = computeOnePoleCoeff(filterCutoff,
                                                       static_cast<double>(sampleRate_));

        // --- Volume -> output gain ---
        // Linear taper with 2x makeup gain. The diode clipper attenuates the
        // signal significantly, so makeup gain keeps output audible at moderate
        // Volume settings: 0 -> silence, 0.5 -> unity, 1.0 -> +6dB boost.
        const double outputGain = static_cast<double>(volume) * 2.0;

        // --- Input coupling high-pass coefficient ---
        // Pre-computed in setSampleRate() since the circuit values are fixed
        // (C1 = 22nF, R1 = 1M, fc = ~7.2 Hz).
        const double inputHPCoeff = inputCouplingCoeff_;

        // =====================================================================
        // Per-sample processing
        // =====================================================================

        // Step 1: Copy input to working buffer (preserves dry signal for
        // potential wet/dry mixing by the base class).
        for (int i = 0; i < numFrames; ++i) {
            workBuffer_[i] = buffer[i];
        }

        // Step 2: Upsample by 2x for anti-aliasing of the nonlinear stages.
        // The oversampler's anti-imaging FIR filter interpolates between
        // input samples, doubling the effective sample rate.
        const int oversampledLen = oversampler_.getOversampledSize(numFrames);
        float* upsampled = oversampler_.upsample(workBuffer_.data(), numFrames);

        // Step 3: Process each oversampled sample through the RAT circuit.
        for (int i = 0; i < oversampledLen; ++i) {
            double x = static_cast<double>(upsampled[i]);

            // -----------------------------------------------------------------
            // Stage 1: Input Coupling High-Pass Filter
            // -----------------------------------------------------------------
            // Models C1 (22nF) in series with R1/R2 (1M) to ground.
            // One-pole high-pass: y[n] = coeff * (y[n-1] + x[n] - x[n-1])
            // Removes DC offset and subsonic content below ~7.2 Hz.
            // At the oversampled rate, the coefficient is very close to 1.0
            // (e.g., ~0.99995 at 88.2 kHz), so audio-band signals pass
            // through essentially unchanged.
            double hpOut = inputHPCoeff * (inputHPState_ + x - inputHPPrev_);
            inputHPPrev_ = x;
            inputHPState_ = fast_math::denormal_guard(hpOut);
            x = inputHPState_;

            // -----------------------------------------------------------------
            // Stage 2: Op-Amp Gain Stage (LM308)
            // -----------------------------------------------------------------
            // Non-inverting amplifier: Vout = Vin * (1 + R_dist / R4)
            // The gain is frequency-dependent due to C4 (100pF) in parallel
            // with R_dist, which rolls off high frequencies at higher gain
            // settings. This prevents the distortion from becoming overly fizzy
            // and is part of why the RAT sounds "thicker" than a simple clipper.
            x *= gain;

            // Apply C4 gain rolloff (one-pole LPF on the gain stage output).
            // At high gain settings with R_dist = 100K, this rolls off at
            // ~15.9 kHz. At low gain settings, the cutoff is much higher
            // and effectively transparent.
            gainRolloffState_ = (1.0 - gainRolloffCoeff) * x
                                + gainRolloffCoeff * gainRolloffState_;
            gainRolloffState_ = fast_math::denormal_guard(gainRolloffState_);
            x = gainRolloffState_;

            // -----------------------------------------------------------------
            // Stage 3: LM308 Slew Rate Limiter
            // -----------------------------------------------------------------
            // The LM308's 0.3 V/us slew rate is the heart of the RAT's sound.
            // This is NOT just a filter -- it is a hard limit on the rate of
            // voltage change at the op-amp output.
            //
            // Physical behavior:
            //   The LM308's internal 30pF compensation capacitor (C3, pins 1-8)
            //   limits how fast the output can change. When the input changes
            //   faster than 0.3V per microsecond, the output "slews" -- it
            //   ramps linearly at the maximum rate instead of following the
            //   input waveform. This converts sinusoidal harmonics into
            //   triangular waves, creating the RAT's characteristic thick,
            //   compressed distortion.
            //
            // At 88.2kHz oversampled rate:
            //   maxDelta = 300,000 / 88,200 = ~3.4V per sample
            //   A 1kHz sine at 1.7V peak would just begin to slew-limit.
            //   At full gain (2128x), even small input signals slew-limit
            //   heavily, which is the correct and intended behavior.
            double delta = x - slewState_;
            if (delta > maxSlewDelta) {
                delta = maxSlewDelta;
            } else if (delta < -maxSlewDelta) {
                delta = -maxSlewDelta;
            }
            slewState_ += delta;
            x = slewState_;

            // -----------------------------------------------------------------
            // Stage 4: WDF Diode Clipping Stage
            // -----------------------------------------------------------------
            // Hard clipping to ground via antiparallel 1N914 silicon diodes.
            //
            // The WDF tree models:
            //   - Ideal voltage source (drives the circuit with the slew-limited
            //     op-amp output signal)
            //   - R_out = 4.7K (combined output impedance of the LM308 and
            //     series resistance to the diode junction)
            //   - C_couple = 1uF (DC blocking capacitor between gain stage
            //     and the diode junction node)
            //   - R_load = 1M (input impedance of the filter stage, provides
            //     DC path to ground for WDF stability)
            //   - DiodePairT (1N914: Is=2.52e-9, Vt=25.85e-3, solved via
            //     the Wright Omega function for iteration-free performance)
            //
            // The diode pair sits at the root of the WDF tree as the sole
            // non-adaptable nonlinear element. All other components are
            // adaptable and compute their reflected waves analytically.
            x = clipperCircuit_.processSample(x);

            // Normalize WDF output from diode voltage range back to audio range.
            // The 1N914 silicon diodes clip at ~0.6-0.7V forward voltage,
            // so the WDF output peaks at roughly ±0.7V. Without this
            // normalization, the signal is ~4 dB below full scale and the
            // volume control cannot reach unity. This matches the DS-1's
            // kOutputScaling = 1.0f / 0.7f pattern.
            x *= kWdfOutputScaling;

            // Write processed sample back to the oversampled buffer.
            upsampled[i] = static_cast<float>(x);
        }

        // Step 4: Downsample back to the original sample rate.
        // The oversampler's anti-aliasing FIR filter removes frequencies
        // above the original Nyquist that were generated by the nonlinear
        // stages (diode clipping and slew rate limiting).
        oversampler_.downsample(upsampled, numFrames, buffer);

        // Step 5: Post-clipping processing at the original sample rate.
        // The filter and volume stages are linear operations that do not
        // generate harmonics, so they do not need oversampling.
        for (int i = 0; i < numFrames; ++i) {
            double x = static_cast<double>(buffer[i]);

            // -----------------------------------------------------------------
            // Stage 5: Passive Low-Pass Filter ("Filter" Control)
            // -----------------------------------------------------------------
            // One-pole LPF: y[n] = (1-a)*x[n] + a*y[n-1]
            // Models the passive RC network: R_filter (0-100K) + C8 (3.3nF).
            //
            // The Filter knob is wired in reverse on the actual pedal:
            //   - Full CCW (knob=0): R near 0, cutoff ~32kHz (bright, bypass)
            //   - Full CW  (knob=1): R = 100K, cutoff ~482Hz (dark, muffled)
            //
            // Most players set this around 10-11 o'clock (filter ~0.3-0.4),
            // which gives a cutoff around 3-5 kHz -- enough to tame the
            // harsh upper harmonics from the hard clipping while retaining
            // the aggressive attack and presence.
            filterState_ = (1.0 - filterCoeff) * x + filterCoeff * filterState_;
            filterState_ = fast_math::denormal_guard(filterState_);
            x = filterState_;

            // -----------------------------------------------------------------
            // Stage 6: Volume Control + Output Clamping
            // -----------------------------------------------------------------
            // Apply output level and hard-clamp to [-1, 1] to prevent
            // downstream effects from receiving out-of-range samples.
            x *= outputGain;
            x = std::max(-1.0, std::min(1.0, x));

            buffer[i] = static_cast<float>(x);
        }
    }

    /**
     * Configure sample rate and pre-allocate all internal buffers.
     *
     * Called once during audio stream setup, before any process() calls.
     * This is the ONLY method that performs memory allocation.
     *
     * @param sampleRate Device native sample rate in Hz.
     */
    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;

        // Compute the oversampled rate (2x). This is used for:
        // - WDF capacitor preparation (bilinear transform depends on sample rate)
        // - Slew rate calculation (max voltage change per sample)
        // - Input coupling high-pass coefficient
        // - Gain rolloff coefficient
        oversampledRate_ = static_cast<double>(sampleRate) * kOversampleFactor;

        // Pre-allocate the oversampler for the maximum expected buffer size.
        // Android audio typically uses 64-480 frame buffers; 2048 provides
        // generous headroom for all Android audio configurations.
        oversampler_.prepare(kMaxFrames);

        // Pre-allocate working buffer for the input copy.
        workBuffer_.resize(kMaxFrames, 0.0f);

        // Prepare the WDF diode clipper circuit at the oversampled rate.
        // This sets the coupling capacitor's port resistance via the bilinear
        // transform: R_p = 1 / (2 * C * fs_oversampled), which must be
        // recalculated whenever the sample rate changes.
        clipperCircuit_.prepare(oversampledRate_);

        // Compute input coupling high-pass coefficient.
        // C1 = 22nF, R1 = 1M -> fc = 1/(2*pi*1M*22nF) = ~7.23 Hz
        // This is computed once since the circuit values are fixed.
        const double inputCouplingFc = 1.0 / (2.0 * M_PI * 1.0e6 * 22.0e-9);
        inputCouplingCoeff_ = computeOnePoleHPCoeff(inputCouplingFc,
                                                     oversampledRate_);
    }

    /**
     * Reset all internal DSP state.
     *
     * Clears filter histories, slew rate state, WDF capacitor state,
     * and the oversampler's FIR delay lines. Called when the audio stream
     * restarts or the effect is re-enabled to prevent stale state from
     * producing clicks or artifacts.
     */
    void reset() override {
        oversampler_.reset();
        clipperCircuit_.reset();

        // Clear all filter and state variables
        inputHPState_      = 0.0;
        inputHPPrev_       = 0.0;
        gainRolloffState_  = 0.0;
        slewState_         = 0.0;
        filterState_       = 0.0;
    }

    // =========================================================================
    // Parameter access (called from UI thread via JNI)
    // =========================================================================

    /**
     * Set a parameter by ID. Thread-safe (writes to atomic).
     *
     * @param paramId  0=Distortion, 1=Filter, 2=Volume
     * @param value    Parameter value, clamped to [0.0, 1.0].
     */
    void setParameter(int paramId, float value) override {
        // Clamp to valid range before storing to prevent downstream
        // calculations from receiving out-of-range values.
        value = clamp(value, 0.0f, 1.0f);

        switch (paramId) {
            case kParamDistortion:
                distortion_.store(value, std::memory_order_relaxed);
                break;
            case kParamFilter:
                filter_.store(value, std::memory_order_relaxed);
                break;
            case kParamVolume:
                volume_.store(value, std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    /**
     * Get a parameter by ID. Thread-safe (reads from atomic).
     *
     * @param paramId  0=Distortion, 1=Filter, 2=Volume
     * @return Current parameter value, or 0.0f if paramId is unknown.
     */
    float getParameter(int paramId) const override {
        switch (paramId) {
            case kParamDistortion:
                return distortion_.load(std::memory_order_relaxed);
            case kParamFilter:
                return filter_.load(std::memory_order_relaxed);
            case kParamVolume:
                return volume_.load(std::memory_order_relaxed);
            default:
                return 0.0f;
        }
    }

private:
    // =========================================================================
    // Constants
    // =========================================================================

    /** Parameter IDs matching the JNI bridge convention. */
    static constexpr int kParamDistortion = 0;
    static constexpr int kParamFilter     = 1;
    static constexpr int kParamVolume     = 2;

    /** Oversampling factor. 2x provides good alias rejection for hard clipping
     *  while keeping CPU load reasonable on mobile ARM processors. */
    static constexpr int kOversampleFactor = 2;

    /** Maximum expected audio buffer size. Android typically uses 64-480 frames;
     *  2048 provides generous headroom for all configurations. */
    static constexpr int kMaxFrames = 2048;

    /** LM308 slew rate in V/s. The datasheet specifies 0.3 V/us = 300,000 V/s.
     *  This is extraordinarily slow compared to modern op-amps (TL072: 13 V/us)
     *  and is the primary reason the RAT sounds the way it does. */
    static constexpr double kSlewRateVPerSec = 0.3e6;

    /** Output scaling to normalize WDF diode clipper output from ~±0.7V
     *  (1N914 forward voltage) back to the [-1, 1] audio range.
     *  Matches the DS-1's kOutputScaling = 1.0f / 0.7f pattern. */
    static constexpr double kWdfOutputScaling = 1.0 / 0.7;

    /** Minimum cutoff frequency for the Filter control.
     *  fc = 1/(2*pi*100K*3.3nF) = ~482 Hz (full CW, maximum darkness). */
    static constexpr double kFilterMinCutoffHz = 482.0;

    /** Maximum cutoff frequency for the Filter control.
     *  Effectively bypasses the filter (full CCW, maximum brightness). */
    static constexpr double kFilterMaxCutoffHz = 32000.0;

    // =========================================================================
    // WDF Diode Clipper Circuit
    // =========================================================================

    /**
     * Wave Digital Filter model of the RAT's hard clipping stage.
     *
     * This struct encapsulates the WDF tree for the diode clipper section
     * of the ProCo RAT circuit. The WDF tree models the following analog
     * sub-circuit:
     *
     *   Vin (from op-amp) ---[R_out=4.7K]---+---[C_couple=1uF]---+--- Vout
     *                                        |                     |
     *                                     [D1,D2]             [R_load=1M]
     *                                   (1N914 pair)               |
     *                                        |                    GND
     *                                       GND
     *
     * WDF tree structure (bottom-up):
     *
     *        [DiodePairT] (ROOT -- sole non-adaptable element)
     *             |
     *       [WDFParallelT: pOuter]
     *         /              \
     *   [Rvs (4.7K)]   [WDFParallelT: pInner]
     *                     /           \
     *              [C_couple (1uF)]  [R_load (1M)]
     *
     * ResistiveVoltageSourceT combines the input signal and R_out
     * into a single adapted (non-root) leaf. DiodePairT is the sole
     * root (non-adaptable nonlinearity, solved via Wright Omega).
     *
     * Component roles:
     *   - R_out (4.7K): Combined output impedance of the LM308 op-amp
     *     and the series resistance in the signal path to the diodes.
     *     This sets the "stiffness" of the source driving the diodes.
     *   - C_couple (1uF): DC blocking capacitor between the gain stage
     *     and the diode clipping node. At audio frequencies with 4.7K
     *     source impedance, fc = 1/(2*pi*4.7K*1uF) = ~33.9 Hz.
     *     This removes any DC offset from the gain stage.
     *   - R_load (1M): Represents the input impedance of the passive
     *     filter stage that follows. Provides a DC path to ground for
     *     proper WDF operation. Its high value minimally affects the
     *     clipping behavior.
     *   - D1, D2 (1N914 antiparallel pair): The hard clipping diodes.
     *     Conduct when the voltage at the junction exceeds ~+/-0.6V,
     *     shunting excess current to ground.
     *
     * Diode parameters (1N914 silicon):
     *   Is = 2.52e-9 A (saturation current)
     *   Vt = 25.85e-3 V (thermal voltage at 25C, kT/q)
     *
     * The DiodePairT uses the Wright Omega function for a fast,
     * iteration-free solution to the Shockley diode equation, avoiding
     * the computational cost of Newton-Raphson iteration.
     */
    struct DiodeClipperCircuit {
        // Namespace alias declared at file scope is not accessible inside
        // a nested struct, so we use fully qualified types or rely on the
        // alias defined in the enclosing scope.

        // ---- WDF Leaf Components ----

        /** Resistive voltage source: combines the LM308 op-amp output
         *  signal with R_out (4.7K) series impedance into a single
         *  adapted (non-root) WDF leaf element. This is the standard
         *  chowdsp_wdf pattern for circuits where DiodePairT must be
         *  the sole root. */
        chowdsp::wdft::ResistiveVoltageSourceT<double> rvs { 4700.0 };

        /** DC blocking capacitor between gain stage and diode junction.
         *  1uF electrolytic. At audio frequencies with 4.7K source:
         *  fc = 1/(2*pi*4700*1e-6) = ~33.9 Hz. Passes all guitar-band
         *  frequencies while blocking DC bias from the op-amp. */
        chowdsp::wdft::CapacitorT<double> cCouple { 1.0e-6 };

        /** Load resistor representing the input impedance of the filter
         *  stage following the clipping section. 1M ohm is high enough
         *  to minimally affect the clipping behavior while providing
         *  the necessary DC path to ground for proper WDF operation. */
        chowdsp::wdft::ResistorT<double> rLoad { 1.0e6 };

        // ---- WDF Tree Adaptors ----

        /** Inner parallel: cCouple || rLoad (both to ground from junction).
         *  Models the shunt elements at the diode junction node. */
        chowdsp::wdft::WDFParallelT<double,
            decltype(cCouple),
            decltype(rLoad)> pInner { cCouple, rLoad };

        /** Outer parallel: rvs || pInner (the circuit junction node).
         *  Models the complete junction where the resistive voltage
         *  source, coupling cap, and load resistor all meet. */
        chowdsp::wdft::WDFParallelT<double,
            decltype(rvs),
            decltype(pInner)> pOuter { rvs, pInner };

        // ---- WDF Root ----

        /** Antiparallel diode pair (1N914 silicon) -- SOLE root element.
         *  DiodePairT is non-adaptable and must be the only root of the
         *  WDF tree. Solved via Wright Omega function for O(1) solution.
         *
         *  1N914 parameters:
         *    Is = 2.52e-9 A (saturation current)
         *    Vt = 25.85e-3 V (thermal voltage at room temperature) */
        chowdsp::wdft::DiodePairT<double, decltype(pOuter)> diodes {
            pOuter,
            2.52e-9,   // Is: saturation current for 1N914
            25.85e-3   // Vt: thermal voltage (kT/q at 25 degrees C)
        };

        /**
         * Prepare the WDF circuit for a given sample rate.
         *
         * The coupling capacitor's port resistance depends on the sample
         * rate via the bilinear transform: R_p = 1 / (2 * C * fs).
         * This must be recalculated whenever the sample rate changes.
         *
         * @param sampleRate The oversampled sample rate in Hz.
         */
        void prepare(double sampleRate) {
            cCouple.prepare(static_cast<double>(sampleRate));
            diodes.calcImpedance();
        }

        /**
         * Reset the WDF circuit state.
         *
         * Clears the coupling capacitor's internal state (stored wave
         * variable from the previous sample) to prevent stale data
         * from causing clicks when the effect restarts.
         */
        void reset() {
            cCouple.reset();
        }

        /**
         * Process a single sample through the WDF diode clipper.
         *
         * The WDF processing follows the standard two-pass algorithm:
         *
         *   1. Set the input voltage on the voltage source (establishes
         *      the driving signal for this time step)
         *
         *   2. Reflected wave pass (bottom-up): pOuter.reflected()
         *      triggers each child to compute its reflected wave.
         *      Leaf nodes compute from their port equations. Adaptors
         *      combine children's reflected waves via scattering
         *      equations. The result propagates up to the root.
         *
         *   3. Nonlinear solve: diodes.incident() receives the reflected
         *      wave from the tree and solves the diode equation against
         *      the Thevenin equivalent using the Wright Omega function.
         *
         *   4. Incident wave pass (top-down): diodes.reflected() sends
         *      the computed wave back into the tree. pOuter.incident()
         *      distributes it to children, updating their states.
         *
         *   5. Read the output: voltage across the coupling capacitor
         *      gives the clipped signal with DC removed.
         *
         * @param x Input sample (voltage from the slew-limited gain stage).
         * @return Clipped output voltage.
         */
        inline double processSample(double x) {
            // Step 1: Set the driving voltage
            rvs.setVoltage(x);

            // Step 2-3: Reflected wave pass + nonlinear solve
            // pOuter.reflected() propagates reflected waves from all
            // leaves up through the tree. The diode pair receives this
            // and computes the nonlinear response via Wright Omega.
            diodes.incident(pOuter.reflected());

            // Step 4: Incident wave pass
            // Send the diode pair's response back into the tree,
            // updating all component states (especially the capacitor).
            pOuter.incident(diodes.reflected());

            // Step 5: Read output voltage across the coupling capacitor.
            // The capacitor voltage represents the clipped signal with
            // DC offset removed. This is the signal that feeds into
            // the passive filter stage.
            return chowdsp::wdft::voltage<double>(cCouple);
        }
    };

    // =========================================================================
    // Filter coefficient computation
    // =========================================================================

    /**
     * Compute one-pole low-pass filter coefficient from cutoff frequency.
     *
     * For a one-pole LPF: y[n] = (1-a)*x[n] + a*y[n-1]
     * The coefficient 'a' is: a = exp(-2*pi*fc/fs)
     *
     * This is the standard one-pole filter design derived from the
     * exponential mapping of the analog prototype's time constant
     * (RC) to the digital domain.
     *
     * @param cutoffHz  Desired cutoff frequency in Hz.
     * @param sampleRateHz  Sample rate in Hz.
     * @return Filter feedback coefficient (higher = more filtering/darker).
     */
    static double computeOnePoleCoeff(double cutoffHz, double sampleRateHz) {
        if (sampleRateHz <= 0.0) return 0.0;
        // Clamp cutoff to valid range: above DC, below Nyquist
        cutoffHz = std::max(1.0, std::min(cutoffHz, sampleRateHz * 0.49));
        return std::exp(-2.0 * M_PI * cutoffHz / sampleRateHz);
    }

    /**
     * Compute one-pole high-pass filter coefficient from cutoff frequency.
     *
     * For a one-pole HPF: y[n] = a * (y[n-1] + x[n] - x[n-1])
     * The coefficient 'a' is: a = RC / (RC + dt) = 1 / (1 + 2*pi*fc*dt)
     *
     * For very low cutoffs (like the 7.2 Hz input coupling), 'a' is
     * very close to 1.0, meaning audio-band signals pass through
     * essentially unchanged while DC and sub-bass are rejected.
     *
     * @param cutoffHz  Desired cutoff frequency in Hz.
     * @param sampleRateHz  Sample rate in Hz.
     * @return Filter coefficient (closer to 1.0 = lower cutoff).
     */
    static double computeOnePoleHPCoeff(double cutoffHz, double sampleRateHz) {
        if (sampleRateHz <= 0.0) return 1.0;
        const double rc = 1.0 / (2.0 * M_PI * cutoffHz);
        const double dt = 1.0 / sampleRateHz;
        return rc / (rc + dt);
    }

    /**
     * Clamp a float value to a range. Used for parameter validation.
     *
     * @param value  Value to clamp.
     * @param minVal Minimum allowed value.
     * @param maxVal Maximum allowed value.
     * @return Clamped value in [minVal, maxVal].
     */
    static float clamp(float value, float minVal, float maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    // =========================================================================
    // Parameters (written from UI thread, read from audio thread)
    // =========================================================================

    /** Distortion amount [0,1]. Controls the op-amp feedback gain.
     *  0.0 = clean (gain ~1), 1.0 = full distortion (gain ~2128). */
    std::atomic<float> distortion_ { 0.5f };

    /** Filter cutoff [0,1]. Controls the passive LPF after clipping.
     *  REVERSED: 0.0 = bright (bypass), 1.0 = dark (~482 Hz cutoff). */
    std::atomic<float> filter_ { 0.5f };

    /** Output volume [0,1]. Controls the final output level.
     *  0.0 = silence, 1.0 = full volume. */
    std::atomic<float> volume_ { 0.5f };

    // =========================================================================
    // Audio thread state (only accessed from the audio callback thread)
    // =========================================================================

    /** Device native sample rate in Hz. */
    int32_t sampleRate_ = 44100;

    /** Effective sample rate after oversampling (sampleRate_ * kOversampleFactor). */
    double oversampledRate_ = 88200.0;

    /** 2x oversampler with anti-imaging/anti-aliasing FIR filters.
     *  Upsamples before nonlinear processing, downsamples after. */
    Oversampler<kOversampleFactor> oversampler_;

    /** WDF diode clipper circuit instance.
     *  Contains all WDF components and handles per-sample processing. */
    DiodeClipperCircuit clipperCircuit_;

    /** Pre-computed input coupling high-pass coefficient.
     *  Set once in setSampleRate() since the circuit values are fixed
     *  (C1 = 22nF, R1 = 1M, fc = ~7.2 Hz). */
    double inputCouplingCoeff_ = 0.999;

    // ---- Filter state variables ----

    /** Input coupling high-pass filter state (one-pole HPF).
     *  Models C1 (22nF) + R1 (1M), fc = ~7.2 Hz. */
    double inputHPState_ = 0.0;

    /** Input coupling high-pass filter previous input sample.
     *  Needed for the one-pole HPF difference equation. */
    double inputHPPrev_ = 0.0;

    /** C4 gain rolloff low-pass filter state.
     *  Models the 100pF capacitor in parallel with the distortion pot,
     *  which causes the gain to decrease at high frequencies. */
    double gainRolloffState_ = 0.0;

    /** LM308 slew rate limiter state.
     *  Tracks the current output voltage of the slew-limited op-amp.
     *  The slew limiter constrains the rate of change of this value
     *  to a maximum of 0.3 V/us (300,000 V/s). */
    double slewState_ = 0.0;

    /** Post-clipping passive low-pass filter state.
     *  Models the Filter control: R_filter (0-100K) + C8 (3.3nF). */
    double filterState_ = 0.0;

    /** Pre-allocated working buffer for input copy before upsampling.
     *  Sized to kMaxFrames in setSampleRate(). */
    std::vector<float> workBuffer_;
};

#endif // GUITAR_ENGINE_PROCO_RAT_H
