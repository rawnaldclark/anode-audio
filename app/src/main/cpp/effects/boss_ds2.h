#ifndef GUITAR_ENGINE_BOSS_DS2_H
#define GUITAR_ENGINE_BOSS_DS2_H

#include "effect.h"
#include "oversampler.h"
#include "fast_math.h"
#include <chowdsp_wdf/chowdsp_wdf.h>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * Boss DS-2 Turbo Distortion -- circuit-accurate WDF emulation.
 *
 * The DS-2 is unique among Boss distortion pedals because it uses a FULLY
 * DISCRETE design with NO IC op-amps. The gain stages are built from
 * individual JFETs (2SK184GR) and BJTs (2SC3378GR) rather than the
 * standard JRC4558 or NJM2904 chips found in the DS-1/SD-1/OD-3.
 *
 * This gives the DS-2 its signature character: a slightly spongy, organic
 * clipping behavior with inherent slew-rate limiting from the discrete
 * transistor stages, as opposed to the cleaner, faster op-amp response.
 *
 * THE SIGNATURE FEATURE: Mode I / Mode II (Turbo) switch.
 *   - Mode I:  Standard distortion. Warm, mellow, relatively flat EQ.
 *              Signal bypasses the Q23 boost stage.
 *   - Mode II: "Turbo" mode. Q23 (2SC3378GR) boost stage is switched IN,
 *              adding ~10dB of gain. The tone stack shifts to a strong
 *              mid-boost centered at ~900Hz with a treble cut at ~2500Hz.
 *              This produces the characteristic "honky" mid-focused tone
 *              that cuts through a band mix -- ideal for solos.
 *
 * Actual DS-2 signal flow (simplified for WDF modeling):
 *   1. Q6 JFET (2SK184GR) source-follower input buffer
 *   2. Q23 (2SC3378GR) boost stage (Mode II only, +10dB)
 *   3. Q16/Q19 discrete differential-pair "op-amp" (gain/drive stage)
 *   4. Q22 additional gain stage
 *   5. D14/D15 (1SS-188FM) hard-clipping diodes to ground
 *   6. Q12/Q13 tone/EQ control
 *   7. Q5 output buffer
 *
 * Modeled sub-circuits:
 *   - Input gain stage: variable gain (mapped from Dist knob + Mode boost)
 *   - WDF clipping core: R(2.2K) -> C(47nF) -> DiodePair to ground
 *     Models the D14/D15 hard-clipping topology.
 *   - Tone/EQ: Mode I uses a swept one-pole LPF (800Hz-6kHz).
 *              Mode II uses a biquad bandpass at 900Hz (Q~2, +10dB boost)
 *              cascaded with a one-pole LPF at 2500Hz.
 *   - Output level stage: simple gain multiplication.
 *
 * Processing uses 2x oversampling to suppress aliasing from the nonlinear
 * WDF diode clipping, and double precision for the WDF core.
 *
 * Parameter IDs for JNI access:
 *   0 = dist  (0.0 to 1.0) - distortion amount
 *   1 = tone  (0.0 to 1.0) - tone control
 *   2 = level (0.0 to 1.0) - output volume
 *   3 = mode  (0.0 to 1.0) - Mode I (<0.5) or Mode II (>=0.5)
 */
class BossDS2 : public Effect {
public:
    BossDS2() = default;

    // =========================================================================
    // Effect interface
    // =========================================================================

    /**
     * Process audio samples through the DS-2 distortion circuit.
     *
     * Signal flow per sample:
     *   1. Apply input gain (Dist knob + Mode II boost)
     *   2. Upsample 2x for anti-aliased nonlinear processing
     *   3. Run through WDF clipping circuit (DiodePair hard clip)
     *   4. Downsample back to original rate
     *   5. Apply tone/EQ shaping (mode-dependent)
     *   6. Apply output level and soft-limit
     *
     * @param buffer   Mono audio samples, modified in-place.
     * @param numFrames Number of audio frames in the buffer.
     */
    void process(float* buffer, int numFrames) override {
        // Guard against being called before setSampleRate() allocates buffers.
        if (driveBuffer_.empty()) return;

        // Snapshot all parameters once per block for consistency.
        // Using relaxed ordering is safe: these are single-writer (UI thread)
        // single-reader (audio thread) atomics where we only need eventual
        // visibility, not ordering relative to other memory operations.
        const float dist  = dist_.load(std::memory_order_relaxed);
        const float tone  = tone_.load(std::memory_order_relaxed);
        const float level = level_.load(std::memory_order_relaxed);
        const float mode  = mode_.load(std::memory_order_relaxed);
        const bool modeII = (mode >= 0.5f);

        // -- Step 1: Compute input gain from Dist parameter --
        // The DS-2's gain structure:
        //   - Dist knob controls the discrete op-amp feedback (Q16/Q19 stage)
        //   - Exponential mapping models the log-taper pot used in the real pedal
        //   - Mode II engages Q23 boost stage for an additional ~10dB (+3.16x)
        //
        // Mapping: dist [0,1] -> gain [1, 500] via exponential curve.
        // At dist=0, gain ~1x (clean). At dist=1, gain ~500x (max saturation).
        // The exponent 6.215 = ln(500) maps [0,1] to [e^0, e^ln(500)] = [1, 500].
        const float baseGain = std::exp(dist * 6.2146f);  // exp(ln(500)*dist)
        const float modeIIBoostFactor = 3.16f;  // ~10dB = 10^(10/20) = 3.16
        const float inputGain = modeII ? (baseGain * modeIIBoostFactor) : baseGain;

        // -- Step 2: Apply drive gain to pre-allocated buffer --
        // We write to a separate buffer to preserve the original for wet/dry mix.
        for (int i = 0; i < numFrames; ++i) {
            driveBuffer_[i] = buffer[i] * inputGain;
        }

        // -- Step 3: Upsample 2x --
        const int oversampledLen = oversampler_.getOversampledSize(numFrames);
        float* upsampled = oversampler_.upsample(driveBuffer_.data(), numFrames);

        // -- Step 4: WDF clipping at oversampled rate --
        // Process each sample through the Wave Digital Filter circuit.
        // The WDF models the R(2.2K) -> C(47nF) -> DiodePair topology
        // of the D14/D15 hard-clipping stage in the DS-2.
        //
        // Processing pattern:
        //   1. Set input voltage on the resistive voltage source
        //   2. Propagate waves: diodes reflect from the port, port reflects back
        //   3. Read output voltage across the capacitor
        for (int i = 0; i < oversampledLen; ++i) {
            const double input = static_cast<double>(upsampled[i]);
            // Normalize WDF output from diode voltage range (~±0.7V) back to
            // audio range. Without this, the signal is ~4 dB below full scale.
            upsampled[i] = static_cast<float>(processWDFSample(input) * kWdfOutputScaling);
        }

        // -- Step 5: Downsample back to original rate --
        oversampler_.downsample(upsampled, numFrames, buffer);

        // -- Step 6: Apply tone/EQ (mode-dependent) --
        if (modeII) {
            // Mode II: Strong mid-boost bandpass at ~900Hz + treble cut at ~2500Hz.
            // This is the "Turbo" sound -- a prominent mid hump that gives the
            // DS-2 its signature honky, vocal quality ideal for lead playing.
            //
            // The Tone knob shifts the bandpass center frequency by +/-200Hz
            // around 900Hz, allowing fine-tuning of the mid-boost peak.
            const float centerFreq = 700.0f + tone * 400.0f;  // 700Hz - 1100Hz
            updateBandpassCoeffs(centerFreq, 2.0f);           // Q = 2.0
            const float bpBoostGain = 3.16f;  // +10dB mid boost

            // Treble cut LPF coefficient at 2500Hz (fixed in Mode II).
            // This rolls off the harsh upper harmonics created by the more
            // aggressive Mode II clipping, taming the fizz.
            const float trebleCutCoeff = computeOnePoleCoeff(2500.0f);
            const float tcOneMinusCoeff = 1.0f - trebleCutCoeff;

            for (int i = 0; i < numFrames; ++i) {
                // Bandpass filter (biquad Direct Form II Transposed)
                const double bpIn = static_cast<double>(buffer[i]);
                const double bpOut = bpB0_ * bpIn + bpZ1_;
                bpZ1_ = bpB1_ * bpIn - bpA1_ * bpOut + bpZ2_;
                bpZ2_ = bpB2_ * bpIn - bpA2_ * bpOut;
                bpZ1_ = fast_math::denormal_guard(bpZ1_);
                bpZ2_ = fast_math::denormal_guard(bpZ2_);

                // Mix: original + boosted bandpass for mid-boost character
                float shaped = buffer[i] + bpBoostGain * static_cast<float>(bpOut);

                // Treble cut one-pole LPF
                modeIILpfState_ = tcOneMinusCoeff * shaped
                                  + trebleCutCoeff * modeIILpfState_;
                modeIILpfState_ = fast_math::denormal_guard(modeIILpfState_);
                buffer[i] = modeIILpfState_;
            }
        } else {
            // Mode I: Gentle swept one-pole lowpass.
            // The DS-2 Mode I has a relatively flat frequency response,
            // similar to the DS-1's tone stack. The Tone knob sweeps
            // the cutoff from dark (~800Hz) to bright (~6kHz).
            const float cutoffHz = 800.0f * std::pow(7.5f, tone);  // 800Hz - 6kHz
            const float lpCoeff = computeOnePoleCoeff(cutoffHz);
            const float oneMinusCoeff = 1.0f - lpCoeff;

            for (int i = 0; i < numFrames; ++i) {
                modeILpfState_ = oneMinusCoeff * buffer[i]
                                 + lpCoeff * modeILpfState_;
                modeILpfState_ = fast_math::denormal_guard(modeILpfState_);
                buffer[i] = modeILpfState_;
            }
        }

        // -- Step 7: Output level and safety clamp --
        // The Level knob on the real DS-2 is a simple volume pot after
        // the tone stack. Clamping to [-1,1] prevents downstream effects
        // from receiving out-of-range samples.
        for (int i = 0; i < numFrames; ++i) {
            buffer[i] = clamp(buffer[i] * level, -1.0f, 1.0f);
        }
    }

    /**
     * Configure sample rate and allocate all internal buffers.
     *
     * This is the ONLY method that performs memory allocation. All
     * subsequent process() calls are allocation-free and real-time safe.
     *
     * @param sampleRate Device native sample rate in Hz (e.g., 44100, 48000).
     */
    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;

        // The oversampled rate for WDF component preparation.
        // With 2x oversampling, a 48kHz stream becomes 96kHz internally.
        const double oversampledRate = static_cast<double>(sampleRate) * 2.0;

        // Pre-allocate oversampler for maximum expected Android buffer size.
        // Android audio callbacks typically deliver 64-480 frames.
        // 2048 provides generous headroom for all devices.
        const int maxFrames = 2048;
        oversampler_.prepare(maxFrames);

        // Pre-allocate drive gain buffer (at original sample rate)
        driveBuffer_.resize(maxFrames, 0.0f);

        // Prepare WDF capacitor for the oversampled rate.
        // CapacitorT::prepare() recalculates the port impedance
        // R = 1 / (2 * C * fs) for the bilinear transform discretization.
        wdfCap_.prepare(oversampledRate);

        // Recalculate impedances throughout the WDF tree after the
        // capacitor's impedance changed due to the new sample rate.
        // DiodePairT (as root) recalculates its internal parameters
        // based on the updated port impedances propagated up from the leaves.
        wdfDiodes_.calcImpedance();
    }

    /**
     * Reset all internal filter states to zero.
     *
     * Called when the audio stream restarts or the effect is toggled.
     * Prevents stale state from the previous audio context from leaking
     * through as clicks or transient artifacts.
     */
    void reset() override {
        oversampler_.reset();

        // Reset WDF capacitor state (stored charge / wave memory)
        wdfCap_.reset();

        // Reset all tone filter states
        modeILpfState_  = 0.0f;
        modeIILpfState_ = 0.0f;
        bpZ1_ = 0.0;
        bpZ2_ = 0.0;
    }

    // =========================================================================
    // Parameter access (UI thread writes, audio thread reads)
    // =========================================================================

    /**
     * Set a parameter value by ID. Thread-safe via atomics.
     *
     * @param paramId  0=dist, 1=tone, 2=level, 3=mode
     * @param value    Normalized value; clamped to valid range.
     */
    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case kParamDist:
                dist_.store(clamp(value, 0.0f, 1.0f),
                            std::memory_order_relaxed);
                break;
            case kParamTone:
                tone_.store(clamp(value, 0.0f, 1.0f),
                            std::memory_order_relaxed);
                break;
            case kParamLevel:
                level_.store(clamp(value, 0.0f, 1.0f),
                             std::memory_order_relaxed);
                break;
            case kParamMode:
                mode_.store(clamp(value, 0.0f, 1.0f),
                            std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    /**
     * Get a parameter value by ID. Thread-safe via atomics.
     *
     * @param paramId  0=dist, 1=tone, 2=level, 3=mode
     * @return Current parameter value, or 0.0f for unknown IDs.
     */
    float getParameter(int paramId) const override {
        switch (paramId) {
            case kParamDist:  return dist_.load(std::memory_order_relaxed);
            case kParamTone:  return tone_.load(std::memory_order_relaxed);
            case kParamLevel: return level_.load(std::memory_order_relaxed);
            case kParamMode:  return mode_.load(std::memory_order_relaxed);
            default:          return 0.0f;
        }
    }

private:
    // =========================================================================
    // Parameter IDs
    // =========================================================================

    static constexpr int kParamDist  = 0;  ///< Distortion amount [0, 1]
    static constexpr int kParamTone  = 1;  ///< Tone control [0, 1]
    static constexpr int kParamLevel = 2;  ///< Output volume [0, 1]
    static constexpr int kParamMode  = 3;  ///< Mode I/II select [0, 1]

    /** Normalize WDF diode clipper output (~±0.7V) back to [-1, 1] range. */
    static constexpr double kWdfOutputScaling = 1.0 / 0.7;

    // =========================================================================
    // WDF Clipping Circuit
    // =========================================================================
    //
    // Models the DS-2's D14/D15 hard-clipping network:
    //
    //   Schematic view:
    //
    //   Vin ---[ R=2.2K ]---+--- Vout
    //                       |
    //                   +---+---+
    //                   |       |
    //                 [C=47nF] [D14 D15]  (anti-parallel diode pair)
    //                   |       |
    //                  GND     GND
    //
    //   At the junction node, the capacitor and diode pair are both
    //   connected to ground. The resistor feeds the signal from Vin.
    //
    // WDF tree (compile-time template structure):
    //
    //   DiodePairT  (ROOT -- nonlinear, non-adaptable)
    //       |
    //   WDFParallelT  (junction node adaptor)
    //      / \
    //     /   \
    //   Rvs    C
    //   (ResistiveVoltageSourceT  (CapacitorT
    //    R=2.2K, V=input)          C=47nF)
    //
    // The ResistiveVoltageSourceT combines the input voltage source and
    // the 2.2K series resistance into one adapted leaf. The CapacitorT
    // models the coupling cap. They connect in parallel at the junction
    // where the diode pair clips the signal. DiodePairT must be the root
    // because it is the only nonlinear (non-adaptable) element.
    //
    // The 1SS-188FM diodes in the real pedal are small-signal silicon diodes
    // similar to 1N4148. We use Is=2.52e-9A and Vt=25.85mV (standard silicon
    // at room temperature). The hard-clipping topology (diodes to ground)
    // gives the DS-2 its aggressive, squared-off clipping waveform as opposed
    // to the softer feedback-loop clipping of the DS-1 or Tube Screamer.
    //
    // =========================================================================

    // WDF leaf components (constructed with nominal values).
    //
    // The WDF tree must have exactly ONE root (non-adaptable) element.
    // DiodePairT is non-adaptable and therefore must be the root.
    // The voltage source is modeled as a ResistiveVoltageSourceT, which
    // combines the input voltage + the 2.2K series resistance into a
    // single adapted (non-root) leaf element. This is the standard
    // chowdsp_wdf pattern for circuits with nonlinear root elements.
    //
    // WDF tree topology:
    //
    //   DiodePairT  (ROOT -- nonlinear, non-adaptable)
    //       |
    //   WDFParallelT  (adapted junction node)
    //      / \
    //     /   \
    //   Rvs    C
    //   (ResistiveVoltageSourceT, 2.2K)    (CapacitorT, 47nF)
    //
    // The parallel adaptor models the circuit junction where the
    // resistor-fed input, coupling capacitor, and diode pair all meet.
    // The capacitor's impedance is sample-rate dependent and recalculated
    // in setSampleRate() via prepare().

    /// Resistive voltage source: combines the input signal source with
    /// the 2.2 kOhm series resistor in a single adapted WDF element.
    /// This feeds the signal into the clipping junction through R.
    chowdsp::wdft::ResistiveVoltageSourceT<double> wdfRvs_ { 2200.0 };

    /// 47 nF coupling/DC-blocking capacitor.
    /// Also shapes the low-frequency rolloff of the clipping stage --
    /// the RC time constant (2.2K * 47nF ~ 103 us, fc ~ 1.5 kHz)
    /// determines how much bass reaches the clipping diodes.
    chowdsp::wdft::CapacitorT<double> wdfCap_ { 47.0e-9 };

    /// Parallel adaptor: models the circuit junction where the resistive
    /// voltage source and capacitor meet. The diode pair clips at this node.
    chowdsp::wdft::WDFParallelT<double,
        decltype(wdfRvs_),
        decltype(wdfCap_)> wdfParallel_ { wdfRvs_, wdfCap_ };

    /// Anti-parallel diode pair (ROOT element, nonlinear, non-adaptable).
    /// Models D14/D15 (1SS-188FM) hard-clipping diodes to ground.
    ///   Is = 2.52e-9 A  (reverse saturation current, typical silicon)
    ///   Vt = 25.85e-3 V (thermal voltage at ~25 C room temperature)
    ///   nDiodes = 1     (single diode in each direction)
    chowdsp::wdft::DiodePairT<double,
        decltype(wdfParallel_)> wdfDiodes_ { wdfParallel_, 2.52e-9, 25.85e-3, 1.0 };

    /**
     * Process a single sample through the WDF clipping circuit.
     *
     * Wave Digital Filter processing follows the scattering pattern:
     *   1. Set the input voltage on the resistive voltage source
     *   2. Compute the reflected wave from the adapted port (leaves -> root)
     *   3. The root (diode pair) computes its nonlinear scattering
     *   4. The root's reflected wave propagates back down (root -> leaves)
     *   5. Read the output as the voltage across a component
     *
     * The output is taken as the voltage across the capacitor, which
     * represents the signal at the clipping junction -- this is where
     * the real DS-2's output is tapped after the D14/D15 clipping diodes
     * have clamped the signal.
     *
     * @param input  Input sample in double precision.
     * @return Clipped output sample.
     */
    double processWDFSample(double input) {
        // Set the input voltage on the resistive voltage source.
        // This models the signal arriving through the 2.2K resistor.
        wdfRvs_.setVoltage(input);

        // Propagate reflected waves up the tree from leaves to root.
        // The parallel adaptor combines the waves from the resistive
        // voltage source and the capacitor, then presents the combined
        // wave to the diode pair (root).
        wdfDiodes_.incident(wdfParallel_.reflected());

        // The diode pair computes its nonlinear response and reflects
        // the wave back down through the parallel adaptor to the leaves.
        // This completes one full scattering cycle.
        wdfParallel_.incident(wdfDiodes_.reflected());

        // Read the output voltage across the capacitor.
        // voltage<T>(node) = (a + b) / 2, the standard WDF voltage formula.
        // The capacitor voltage represents the clipped signal at the junction.
        return chowdsp::wdft::voltage<double>(wdfCap_);
    }

    // =========================================================================
    // Tone / EQ Filters
    // =========================================================================

    /**
     * Compute one-pole lowpass filter coefficient.
     *
     * For the difference equation y[n] = (1-a)*x[n] + a*y[n-1]:
     *   a = exp(-2*pi*fc/fs)
     *
     * Higher 'a' = more filtering (darker). Lower 'a' = less filtering (brighter).
     *
     * @param cutoffHz Desired -3dB cutoff frequency in Hz.
     * @return Filter coefficient 'a' for the one-pole LPF.
     */
    float computeOnePoleCoeff(float cutoffHz) const {
        if (sampleRate_ <= 0) return 0.0f;
        // Clamp to valid range: above DC, below Nyquist
        cutoffHz = clamp(cutoffHz, 20.0f,
                         static_cast<float>(sampleRate_) * 0.49f);
        return std::exp(-2.0f * kPiF * cutoffHz
                        / static_cast<float>(sampleRate_));
    }

    /**
     * Update the biquad bandpass filter coefficients for Mode II mid-boost.
     *
     * Uses the standard Audio EQ Cookbook (Robert Bristow-Johnson) formulas
     * for a constant-0dB-peak-gain bandpass filter:
     *
     *   w0 = 2*pi*f0/fs
     *   alpha = sin(w0) / (2*Q)
     *
     *   b0 =  alpha
     *   b1 =  0
     *   b2 = -alpha
     *   a0 =  1 + alpha
     *   a1 = -2*cos(w0)
     *   a2 =  1 - alpha
     *
     * All coefficients are pre-divided by a0 for direct use in the
     * difference equation (direct form I).
     *
     * @param centerFreq  Center frequency of the bandpass in Hz.
     * @param Q           Quality factor (higher = narrower bandwidth).
     */
    void updateBandpassCoeffs(float centerFreq, float Q) {
        if (sampleRate_ <= 0) return;
        const double fs = static_cast<double>(sampleRate_);

        // Clamp center frequency to valid range
        const double fc = static_cast<double>(
            clamp(centerFreq, 20.0f, static_cast<float>(sampleRate_) * 0.49f));

        const double w0 = 2.0 * M_PI * fc / fs;
        const double sinW0 = std::sin(w0);
        const double cosW0 = std::cos(w0);
        const double alpha = sinW0 / (2.0 * static_cast<double>(Q));

        // Normalize by a0 so the difference equation uses a0=1 implicitly
        const double a0 = 1.0 + alpha;
        const double invA0 = 1.0 / a0;

        bpB0_ =  alpha    * invA0;
        bpB1_ =  0.0;
        bpB2_ = -alpha    * invA0;
        bpA1_ = -2.0 * cosW0 * invA0;
        bpA2_ = (1.0 - alpha) * invA0;
    }

    /**
     * Clamp a value to a range [minVal, maxVal].
     */
    static float clamp(float value, float minVal, float maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    // =========================================================================
    // Constants
    // =========================================================================

    /// Pi as a float, used throughout filter coefficient calculations
    static constexpr float kPiF = 3.14159265358979323846f;

    // =========================================================================
    // Parameters (std::atomic for lock-free UI thread -> audio thread)
    // =========================================================================

    std::atomic<float> dist_  { 0.5f };  ///< [0, 1] distortion amount
    std::atomic<float> tone_  { 0.5f };  ///< [0, 1] tone (dark to bright)
    std::atomic<float> level_ { 0.5f };  ///< [0, 1] output volume
    std::atomic<float> mode_  { 0.0f };  ///< [0, 1] Mode I (<0.5) or Mode II (>=0.5)

    // =========================================================================
    // Audio-thread state (accessed only from the audio callback)
    // =========================================================================

    int32_t sampleRate_ = 44100;

    /// 2x oversampler for anti-aliased nonlinear processing.
    /// 2x is sufficient for the DS-2 because the hard-clipping diodes produce
    /// primarily odd harmonics with rapidly decreasing amplitude, and the
    /// tone stack further attenuates high-frequency content before output.
    Oversampler<2> oversampler_;

    /// Pre-allocated buffer for the drive-gain stage (before upsampling).
    /// Kept separate from the input buffer to preserve the dry signal
    /// for potential wet/dry mixing via applyWetDryMix().
    std::vector<float> driveBuffer_;

    // -- Mode I tone filter state --
    float modeILpfState_ = 0.0f;   ///< One-pole LPF state for Mode I

    // -- Mode II tone filter state --
    float modeIILpfState_ = 0.0f;  ///< One-pole LPF state for Mode II treble cut

    // Biquad bandpass state for Mode II mid-boost (Direct Form II Transposed)
    // Uses double precision for numerical stability at moderate Q values.
    double bpZ1_ = 0.0;  ///< DF2T state z^-1
    double bpZ2_ = 0.0;  ///< DF2T state z^-2

    // Biquad bandpass coefficients (updated per block in Mode II)
    double bpB0_ = 0.0;  ///< Feedforward coeff b0
    double bpB1_ = 0.0;  ///< Feedforward coeff b1 (always 0 for bandpass)
    double bpB2_ = 0.0;  ///< Feedforward coeff b2
    double bpA1_ = 0.0;  ///< Feedback coeff a1 (normalized, a0=1)
    double bpA2_ = 0.0;  ///< Feedback coeff a2 (normalized, a0=1)
};

#endif // GUITAR_ENGINE_BOSS_DS2_H
