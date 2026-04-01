#ifndef GUITAR_ENGINE_BOSS_DS1_H
#define GUITAR_ENGINE_BOSS_DS1_H

#include "effect.h"
#include "oversampler.h"
#include "fast_math.h"
#include <chowdsp_wdf/chowdsp_wdf.h>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * Circuit-accurate emulation of the Boss DS-1 Distortion pedal.
 *
 * The Boss DS-1 is one of the most iconic distortion pedals ever made,
 * known for its aggressive, mid-scooped tone that defined the sound of
 * punk, grunge, and metal guitar. Unlike the Tube Screamer (soft clipping
 * in the op-amp feedback loop), the DS-1 uses hard clipping: diodes shunt
 * the amplified signal to ground, creating a sharply squared waveform with
 * rich odd-order harmonics.
 *
 * Circuit stages modeled (from Electrosmash DS-1 analysis and Yeh's thesis):
 *
 *   1. Transistor Booster (Q2, 2SC2240):
 *      Common-emitter amplifier with ~35dB gain (56x). R9=22 ohm emitter
 *      resistor provides minimal degeneration. The feedback network
 *      (R7=470K, C4=250pF) shapes frequency response. This stage is
 *      approximately linear for typical guitar signal levels and is modeled
 *      as a fixed-gain amplifier.
 *
 *   2. Op-Amp Gain Stage (IC1, NJM2904L):
 *      Non-inverting configuration. VR1 (100K DIST pot) in feedback sets
 *      variable gain: G = 1 + VR1/R13 = 1 to 22.3x (0 to 26.5dB).
 *      C10 (0.01uF) in the feedback path creates a low-pass characteristic
 *      at approximately 7.2kHz, rolling off harsh high-frequency content
 *      before clipping. The input is high-passed by C5(68nF)/R10(10K) at
 *      approximately 23Hz to remove subsonic content.
 *
 *   3. Hard Clipping Stage (D4, D5 -- 1N4148 silicon diodes):
 *      Two antiparallel diodes connected from the op-amp output to virtual
 *      ground (4.5V bias rail). This is HARD clipping because the diodes
 *      are to ground, not in the feedback loop. The signal is limited to
 *      approximately +/- 0.7V (diode forward voltage). This stage is
 *      modeled using Wave Digital Filters (chowdsp_wdf) with DiodePairT
 *      for physically accurate nonlinear behavior, including the smooth
 *      transition near the clipping threshold that gives the DS-1 its
 *      characteristic response.
 *
 *   4. Passive Tone Control (Big Muff Pi style):
 *      A crossfade between a low-pass path (R16=6.8K, C12=0.1uF, fc=234Hz)
 *      and a high-pass path (C11=22nF, R17=6.8K, fc=1063Hz). VR3 (20K TONE
 *      pot) blends between the two paths, creating a variable mid-scoop
 *      notch centered around 500Hz. This is the opposite of the Tube
 *      Screamer's mid-hump, giving the DS-1 its scooped, aggressive tone.
 *
 * Anti-aliasing strategy:
 *   2x oversampling is used around the nonlinear WDF clipping stage.
 *   The diode hard clipping generates harmonics at integer multiples of the
 *   input frequency. At 44.1kHz base rate, 2x oversampling pushes Nyquist
 *   to 44.1kHz (from the effective 88.2kHz rate), providing adequate alias
 *   rejection for the relatively band-limited signal that enters the clipper
 *   (the pre-clipping low-pass at ~7.2kHz limits harmonic generation).
 *
 * Parameter IDs (for JNI access):
 *   0 = Dist  (0.0 to 1.0) - Distortion amount (op-amp gain)
 *   1 = Tone  (0.0 to 1.0) - Tone control (dark to bright)
 *   2 = Level (0.0 to 1.0) - Output volume
 *
 * References:
 *   - Electrosmash: https://www.electrosmash.com/boss-ds1-analysis
 *   - D. Yeh, "Simplified, physically-informed models of distortion and
 *     overdrive guitar effects pedals," DAFx-07
 *   - D. Yeh, "Digital Implementation of Musical Distortion Circuits by
 *     Analysis and Simulation," PhD thesis, CCRMA, Stanford, 2009
 */
class BossDS1 : public Effect {
public:
    BossDS1() = default;

    // =========================================================================
    // Effect interface
    // =========================================================================

    /**
     * Process audio through the DS-1 distortion circuit. Real-time safe.
     *
     * Signal flow:
     *   Input -> Transistor booster (fixed gain)
     *         -> Pre-clipping high-pass (23Hz, removes DC/subsonic)
     *         -> Op-amp variable gain (Dist knob)
     *         -> Pre-clipping low-pass (7.2kHz, tames harsh highs)
     *         -> 2x Upsample
     *         -> WDF Diode Clipper (hard clipping, 1N4148 diode pair)
     *         -> 2x Downsample
     *         -> Tone control (LP/HP crossfade with mid-scoop)
     *         -> Output level
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
        const float dist  = dist_.load(std::memory_order_relaxed);
        const float tone  = tone_.load(std::memory_order_relaxed);
        const float level = level_.load(std::memory_order_relaxed);

        // ------------------------------------------------------------------
        // Pre-compute per-block constants outside the sample loop.
        // ------------------------------------------------------------------

        // Transistor booster: fixed gain of ~56x (35dB).
        // In practice, this gain is reduced somewhat by the feedback network
        // (R7=470K, C4=250pF). We model the effective gain as approximately
        // 40x, which accounts for feedback attenuation at mid frequencies.
        // This value was tuned against the Electrosmash analysis.
        constexpr float kBoosterGain = 40.0f;

        // Op-amp gain stage: G = 1 + VR1/R13
        // VR1 = Dist pot (0 to 100K), R13 = 4.7K
        // Gain range: 1.0 (dist=0) to 22.3 (dist=1)
        // Using audio taper (log) mapping for the pot, because the original
        // DS-1 DIST pot is a Type B (linear) pot, but the gain relationship
        // G = 1 + R/4.7K is already inherently exponential in perceived
        // loudness terms, so a linear knob mapping is appropriate here.
        const float opAmpGain = 1.0f + dist * (100.0e3f / 4.7e3f);

        // Combined pre-clipping gain (booster * op-amp).
        // We scale down by a normalization factor to keep the signal in a
        // range where the WDF diode model operates accurately. The WDF
        // voltage source expects realistic circuit voltages (a few volts),
        // not massively amplified values. The original circuit operates
        // with a 9V supply and 4.5V bias, so signals are typically 0-5V
        // peak in the clipping stage.
        constexpr float kInputScaling = 5.0f;
        const float preClipGain = kBoosterGain * opAmpGain * kInputScaling;

        // Filter coefficients are pre-computed in setSampleRate() since
        // cutoff frequencies are fixed circuit values that don't change
        // per block. This avoids redundant std::exp() calls on each block.

        // Tone crossfade: VR3 (20K) blends between LP and HP paths.
        // tone=0 => fully LP (dark), tone=1 => fully HP (bright).
        // At tone=0.5, both paths contribute equally, creating the
        // characteristic mid-scoop notch at ~500Hz.
        const float toneHP = tone;
        const float toneLP = 1.0f - tone;

        // Output level with audio taper for natural volume feel.
        // The original DS-1 uses a 100K audio (Type A) taper pot.
        // Audio taper: level^2 approximation maps [0,1] to a perceptually
        // linear loudness curve.
        const float outputGain = level * level;

        // Post-clipping output scaling: the WDF clipper outputs voltages
        // in the range of roughly +/-0.7V (diode forward voltage). We
        // need to normalize back to the [-1, 1] float audio range.
        constexpr float kOutputScaling = 1.0f / 0.7f;

        // ------------------------------------------------------------------
        // Per-sample processing loop
        // ------------------------------------------------------------------
        for (int i = 0; i < numFrames; ++i) {
            float x = buffer[i];

            // Stage 1+2: Transistor booster + Op-amp gain (combined)
            x *= preClipGain;

            // Pre-clipping high-pass (remove DC and subsonics)
            // y_hp[n] = coeff * (y_hp[n-1] + x[n] - x[n-1])
            // Uses double precision: 234Hz HP coefficient is close to 1.0,
            // and float (23-bit mantissa) cannot represent the difference
            // accurately enough, causing the filter to drift or lock up.
            double xd = static_cast<double>(x);
            double hpOut = hpCoeff_ * (hpState_ + xd - hpPrevInput_);
            hpPrevInput_ = xd;
            hpState_ = fast_math::denormal_guard(hpOut);
            x = static_cast<float>(hpState_);

            // Pre-clipping low-pass (7.2kHz bandwidth limit)
            // y_lp[n] = (1-a)*x[n] + a*y[n-1]
            xd = static_cast<double>(x);
            lpPreClipState_ = (1.0 - lpPreClipCoeff_) * xd
                            + lpPreClipCoeff_ * lpPreClipState_;
            lpPreClipState_ = fast_math::denormal_guard(lpPreClipState_);
            x = static_cast<float>(lpPreClipState_);

            // Store in the pre-oversampling buffer for the oversampler
            preOsBuffer_[i] = x;
        }

        // ------------------------------------------------------------------
        // Stage 3: WDF Diode Clipping with 2x oversampling
        // ------------------------------------------------------------------
        const int oversampledLen = oversampler_.getOversampledSize(numFrames);
        float* upsampled = oversampler_.upsample(preOsBuffer_.data(), numFrames);

        // Process each oversampled sample through the WDF diode clipper.
        // The WDF circuit uses double precision for numerical stability
        // in the diode equation solver (Wright Omega approximation).
        for (int i = 0; i < oversampledLen; ++i) {
            upsampled[i] = static_cast<float>(
                clipperCircuit_.processSample(static_cast<double>(upsampled[i]))
            );
        }

        // Downsample back to the original rate
        oversampler_.downsample(upsampled, numFrames, postOsBuffer_.data());

        // ------------------------------------------------------------------
        // Stage 4: Tone control + output level
        // ------------------------------------------------------------------
        for (int i = 0; i < numFrames; ++i) {
            float x = postOsBuffer_[i];

            // Normalize clipped signal back to audio range
            x *= kOutputScaling;

            // Low-pass path: R16(6.8K) + C12(0.1uF), fc=234Hz
            // 234Hz is low enough that double precision helps maintain
            // filter accuracy at low sample rates.
            double xd2 = static_cast<double>(x);
            lpToneState_ = (1.0 - lpToneCoeff_) * xd2
                         + lpToneCoeff_ * lpToneState_;
            lpToneState_ = fast_math::denormal_guard(lpToneState_);

            // High-pass path: C11(22nF) + R17(6.8K), fc=1063Hz
            double hpToneOut = hpToneCoeff_ * (hpToneState_ + xd2 - hpTonePrevInput_);
            hpTonePrevInput_ = xd2;
            hpToneState_ = fast_math::denormal_guard(hpToneOut);

            // Crossfade between LP and HP paths (tone pot)
            float toned = static_cast<float>(
                toneLP * lpToneState_ + toneHP * hpToneState_);

            // Apply output level and clamp to prevent downstream clipping
            toned *= outputGain;
            buffer[i] = clamp(toned, -1.0f, 1.0f);
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
        // The capacitor in the WDF tree is a reactive element whose
        // discretization depends on the sample rate (bilinear transform
        // maps s-domain to z-domain, and the mapping depends on T=1/fs).
        const double oversampledRate = static_cast<double>(sampleRate) * 2.0;
        clipperCircuit_.prepare(oversampledRate);

        // Pre-compute fixed filter coefficients. All cutoff frequencies
        // are determined by circuit component values and do not change
        // at runtime. Computing once here avoids std::exp() per block.
        hpCoeff_ = computeOnePoleHPCoeffDouble(234.0);      // Pre-clip HP: C5(68nF)/R10(10K) = 234Hz
        lpPreClipCoeff_ = computeOnePoleLPCoeffDouble(7234.0); // Pre-clip LP
        lpToneCoeff_ = computeOnePoleLPCoeffDouble(234.0);   // Tone LP path
        hpToneCoeff_ = computeOnePoleHPCoeffDouble(1063.0);  // Tone HP path
    }

    /**
     * Reset all internal filter and WDF state.
     * Called when the audio stream restarts or the effect is re-enabled
     * to prevent stale samples from leaking through as clicks/pops.
     */
    void reset() override {
        oversampler_.reset();
        clipperCircuit_.reset();

        // Reset all one-pole filter states
        hpState_ = 0.0;
        hpPrevInput_ = 0.0;
        lpPreClipState_ = 0.0;
        lpToneState_ = 0.0;
        hpToneState_ = 0.0;
        hpTonePrevInput_ = 0.0;
    }

    // =========================================================================
    // Parameter access (called from UI thread via JNI)
    // =========================================================================

    /**
     * Set a parameter by ID. Thread-safe via atomic storage.
     *
     * @param paramId  0=Dist, 1=Tone, 2=Level
     * @param value    Parameter value in [0.0, 1.0].
     */
    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0:
                dist_.store(clamp(value, 0.0f, 1.0f),
                            std::memory_order_relaxed);
                break;
            case 1:
                tone_.store(clamp(value, 0.0f, 1.0f),
                            std::memory_order_relaxed);
                break;
            case 2:
                level_.store(clamp(value, 0.0f, 1.0f),
                             std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    /**
     * Get a parameter by ID. Thread-safe via atomic load.
     *
     * @param paramId  0=Dist, 1=Tone, 2=Level
     * @return Current parameter value, or 0.0f if paramId is unknown.
     */
    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return dist_.load(std::memory_order_relaxed);
            case 1: return tone_.load(std::memory_order_relaxed);
            case 2: return level_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    // =========================================================================
    // WDF Diode Clipper Sub-Circuit
    // =========================================================================

    /**
     * Wave Digital Filter model of the DS-1 hard clipping stage.
     *
     * Circuit topology:
     *
     *   Vin ---[ R_in (2.2K) ]---+---[ C_couple (47nF) ]--- GND
     *                            |
     *                            +---[ R_load (100K) ]--- GND
     *                            |
     *                        D4 -+- D5  (1N4148 antiparallel to GND)
     *
     * The input resistor (R_in) represents the output impedance of the
     * op-amp gain stage. The coupling capacitor (C_couple) provides DC
     * blocking and creates a high-pass characteristic. The load resistor
     * (R_load) provides a DC path for the diode bias. The antiparallel
     * diode pair (D4, D5) performs the actual hard clipping.
     *
     * WDF tree structure (bottom-up):
     *
     *        [DiodePairT] (ROOT -- sole non-adaptable element)
     *             |
     *       [WDFParallelT: pOuter]
     *         /              \
     *   [Rvs (2.2K)]   [WDFParallelT: pInner]
     *                     /           \
     *              [C_couple (47nF)] [R_load (100K)]
     *
     * ResistiveVoltageSourceT combines the input signal and R_in into
     * a single adapted (non-root) leaf. DiodePairT is the sole root
     * (non-adaptable nonlinearity, solved via Wright Omega function).
     *
     * Component values are from the Electrosmash DS-1 schematic analysis:
     *   R_in = 2.2K (output impedance of op-amp stage)
     *   C_couple = 47nF (DC blocking / coupling cap, C8 in schematic)
     *   R_load = 100K (bias path to virtual ground)
     *   D4, D5 = 1N4148: Is = 2.52e-9 A, Vt = 25.85e-3 V
     */
    struct DiodeClipperCircuit {
        // --------------------------------------------------------------------
        // Leaf components
        // --------------------------------------------------------------------

        /** Resistive voltage source: combines input signal + R_in (2.2K)
         *  into a single adapted (non-root) WDF leaf element.
         *  This is the standard chowdsp_wdf pattern for circuits where
         *  DiodePairT must be the sole root. */
        chowdsp::wdft::ResistiveVoltageSourceT<double> rvs { 2200.0 };

        /** DC-blocking coupling capacitor, 47nF (C8 in DS-1 schematic) */
        chowdsp::wdft::CapacitorT<double> cCouple { 47.0e-9 };

        /** Load resistor to virtual ground, 100K ohm */
        chowdsp::wdft::ResistorT<double> rLoad { 100.0e3 };

        // --------------------------------------------------------------------
        // WDF tree adaptors
        // --------------------------------------------------------------------

        /** Inner parallel: cCouple || rLoad (both to ground from junction) */
        chowdsp::wdft::WDFParallelT<double,
            decltype(cCouple), decltype(rLoad)> pInner { cCouple, rLoad };

        /** Outer parallel: rvs || pInner (the circuit junction node) */
        chowdsp::wdft::WDFParallelT<double,
            decltype(rvs), decltype(pInner)> pOuter { rvs, pInner };

        // --------------------------------------------------------------------
        // Root nonlinearity (ONLY non-adaptable element)
        // --------------------------------------------------------------------

        /**
         * Antiparallel diode pair (D4, D5 -- 1N4148 silicon).
         * SOLE root of the WDF tree. Solved via Wright Omega function.
         *
         *   Is = 2.52e-9 A  (1N4148 saturation current)
         *   Vt = 25.85e-3 V (thermal voltage at room temp, kT/q)
         */
        chowdsp::wdft::DiodePairT<double, decltype(pOuter)> dp {
            pOuter, 2.52e-9, 25.85e-3
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
         * Process a single sample through the WDF diode clipper.
         *
         * Wave propagation sequence:
         *   1. Set the input voltage on the voltage source
         *   2. Reflect waves from the parallel junction up to the diode pair
         *   3. The diode pair computes its reflected wave (nonlinear solve)
         *   4. Propagate the reflected wave back down to the parallel junction
         *   5. Read the output voltage across the coupling capacitor
         *
         * The output is taken across C_couple, which acts as a high-pass:
         * low frequencies pass through the capacitor less, providing a
         * natural DC-blocking behavior.
         *
         * @param x Input voltage (amplified guitar signal).
         * @return Output voltage after diode clipping, approximately +/-0.7V.
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
     * Compute one-pole low-pass filter coefficient (double precision).
     *
     * For the difference equation y[n] = (1-a)*x[n] + a*y[n-1]:
     *   a = exp(-2*pi*fc/fs)
     *
     * @param cutoffHz Desired cutoff frequency in Hz.
     * @return Filter coefficient 'a' in double precision.
     */
    double computeOnePoleLPCoeffDouble(double cutoffHz) const {
        if (sampleRate_ <= 0) return 0.0;
        const double fs = static_cast<double>(sampleRate_);
        cutoffHz = std::max(20.0, std::min(cutoffHz, fs * 0.49));
        return std::exp(-2.0 * M_PI * cutoffHz / fs);
    }

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

    /** Distortion amount [0, 1]. Controls op-amp feedback gain.
     *  Default 0.5 gives moderate crunch. */
    std::atomic<float> dist_{0.5f};

    /** Tone control [0, 1]. Crossfade between LP (dark) and HP (bright).
     *  Default 0.5 creates the classic mid-scooped sound. */
    std::atomic<float> tone_{0.5f};

    /** Output level [0, 1]. Controls final volume after distortion.
     *  Default 0.5 provides unity-ish gain at moderate settings. */
    std::atomic<float> level_{0.5f};

    // =========================================================================
    // Audio thread state (not accessed from UI thread)
    // =========================================================================

    /** Device sample rate in Hz */
    int32_t sampleRate_ = 44100;

    /**
     * 2x oversampler for the nonlinear clipping stage.
     * 2x is chosen as a balance between alias rejection and CPU cost
     * on mobile. The pre-clipping low-pass at 7.2kHz limits the bandwidth
     * entering the clipper, so 2x provides adequate headroom: the effective
     * Nyquist at 2x (44.1kHz) is well above the 7.2kHz bandwidth.
     */
    Oversampler<2> oversampler_;

    /** WDF diode clipper sub-circuit (double precision) */
    DiodeClipperCircuit clipperCircuit_;

    // ---- Pre-allocated buffers ----

    /** Buffer for signal after gain/filtering, before oversampling */
    std::vector<float> preOsBuffer_;

    /** Buffer for signal after oversampling/clipping, before tone */
    std::vector<float> postOsBuffer_;

    // ---- Pre-computed filter coefficients (set in setSampleRate()) ----

    double hpCoeff_ = 0.967;        ///< Pre-clip HP (234Hz, C5/R10)
    double lpPreClipCoeff_ = 0.0;   ///< Pre-clip LP (7234Hz)
    double lpToneCoeff_ = 0.0;      ///< Tone LP path (234Hz)
    double hpToneCoeff_ = 0.99;     ///< Tone HP path (1063Hz)

    // ---- One-pole filter states (double precision for low-freq stability) ----

    /** Pre-clipping high-pass filter state (234Hz, C5=68nF/R10=10K) */
    double hpState_ = 0.0;

    /** Previous input sample for the pre-clipping high-pass */
    double hpPrevInput_ = 0.0;

    /** Pre-clipping low-pass filter state (7.2kHz bandwidth limit) */
    double lpPreClipState_ = 0.0;

    /** Tone control low-pass path state (234Hz) */
    double lpToneState_ = 0.0;

    /** Tone control high-pass path state (1063Hz) */
    double hpToneState_ = 0.0;

    /** Previous input sample for the tone high-pass path */
    double hpTonePrevInput_ = 0.0;
};

#endif // GUITAR_ENGINE_BOSS_DS1_H
