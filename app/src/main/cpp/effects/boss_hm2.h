#ifndef GUITAR_ENGINE_BOSS_HM2_H
#define GUITAR_ENGINE_BOSS_HM2_H

#include "effect.h"
#include "oversampler.h"
#include "fast_math.h"
#include <chowdsp_wdf/chowdsp_wdf.h>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * Boss HM-2 Heavy Metal circuit-accurate emulation.
 *
 * The HM-2 is legendary for its "Swedish chainsaw" tone, used extensively
 * in Swedish death metal (Entombed, Dismember, Bloodbath) and contemporary
 * heavy music. What makes it unique is the combination of THREE cascaded
 * clipping stages plus an aggressive gyrator-based active EQ that creates
 * a deeply scooped, mid-hollow sound with crushing lows and aggressive
 * upper-mid presence.
 *
 * == Circuit Signal Flow ==
 *
 *   Input
 *     |
 *     v
 *   [Pre-Gain Stage] -- Discrete BJT gain (Q2/Q3 NPN + Q6/Q7 complementary)
 *     |                  Controlled by "Dist" knob. Maps 0-1 to 1-101 gain.
 *     v
 *   +----- 2x Oversampled Region (anti-alias the nonlinearities) -----+
 *   |                                                                   |
 *   |  [Stage 1: Asymmetric Soft Clipping (IC1B)]                      |
 *   |    Tube Screamer-style diodes in op-amp feedback loop.            |
 *   |    ASYMMETRIC: positive half reaches 2x amplitude of negative     |
 *   |    before limiting. Models the unequal forward voltage of the     |
 *   |    feedback diode pair.                                           |
 *   |                                                                   |
 *   |  [Stage 2: Germanium "Coring" (D6, D7 - 1S188FM)]               |
 *   |    Germanium diodes in SERIES (not shunt). Vf = 0.3V.            |
 *   |    Creates a deadband: any signal below ~300mV is BLOCKED.        |
 *   |    This crossover distortion gives the HM-2 its harsh, gritty    |
 *   |    texture and acts as a primitive noise gate.                     |
 *   |                                                                   |
 *   |  [Stage 3: Hard Clipping (D8, D9 - 1S2473 Silicon)]             |
 *   |    Shunt diodes to ground (like RAT/DS-1 topology).              |
 *   |    Modeled with chowdsp_wdf DiodePairT for accuracy.             |
 *   |    C16 (100nF) shunts treble to ground, softening the harsh      |
 *   |    square-wave edges from the hard clipping.                      |
 *   |                                                                   |
 *   +-------------------------------------------------------------------+
 *     |
 *     v
 *   [Gyrator-Based Active EQ] -- THE signature element
 *     Band 1: Low peak    (87 Hz,  Q=2,   gain = Low knob: -15 to +15dB)
 *     Band 2: Mid scoop   (240 Hz, Q=1.5, FIXED -12dB -- always present)
 *     Band 3: High peak 1 (960 Hz, Q=2,   gain = High knob: -15 to +15dB)
 *     Band 4: High peak 2 (1280 Hz,Q=2,   gain = High knob: -15 to +15dB)
 *     |
 *     v
 *   [Output Level] -- "Level" knob
 *     |
 *     v
 *   Output
 *
 * == Resulting EQ curve (all maxed) ==
 *   +15dB at 87 Hz   (crushing lows, right on low-E fundamental at 82 Hz)
 *   -12dB at 240 Hz  (deep mid scoop -- the "hollow" chainsaw sound)
 *   +15dB at 960 Hz  (aggressive upper-mid presence)
 *   +15dB at 1280 Hz (sizzling upper-mid attack)
 *
 * == Parameters ==
 *   Param 0: "Dist"  (0.0-1.0, default 0.7) Pre-gain into clipping stages
 *   Param 1: "Low"   (0.0-1.0, default 0.5) Bass EQ at 87 Hz
 *   Param 2: "High"  (0.0-1.0, default 0.5) Upper-mid EQ at 960/1280 Hz
 *   Param 3: "Level" (0.0-1.0, default 0.5) Output volume
 *
 * All parameters use std::atomic<float> for lock-free UI/audio thread
 * communication, consistent with the rest of this codebase.
 *
 * == Technical Notes ==
 *   - WDF circuit uses double precision for numerical stability
 *   - Biquad states stored as double for stability at low frequencies
 *   - Biquad coefficients recalculated on parameter change (not per-sample)
 *   - Germanium coring creates intentional hard discontinuity at +/-0.3V
 *   - 2x oversampling wraps only the three nonlinear clipping stages
 *   - All buffers pre-allocated in setSampleRate() for real-time safety
 */
class BossHM2 : public Effect {
public:
    BossHM2() = default;

    // =========================================================================
    // Effect interface
    // =========================================================================

    /**
     * Process audio through the HM-2 signal chain. Real-time safe.
     *
     * Signal flow per block:
     *   1. Snapshot atomic parameters (one relaxed load each)
     *   2. Apply pre-gain (Dist knob -> exponential gain curve)
     *   3. Upsample 2x for anti-aliased nonlinear processing
     *   4. Three clipping stages in series at oversampled rate
     *   5. Downsample back to native rate
     *   6. Four-band gyrator EQ (biquad cascade)
     *   7. Output level scaling and safety clamp
     *
     * @param buffer   Mono audio samples, modified in-place.
     * @param numFrames Number of frames in the buffer.
     */
    void process(float* buffer, int numFrames) override {
        // Guard against being called before setSampleRate() allocates buffers.
        if (driveBuffer_.empty()) return;

        // --- Snapshot all parameters once per block ---
        const float dist  = dist_.load(std::memory_order_relaxed);
        const float low   = low_.load(std::memory_order_relaxed);
        const float high  = high_.load(std::memory_order_relaxed);
        const float level = level_.load(std::memory_order_relaxed);

        // --- Recalculate EQ coefficients if parameters changed ---
        // Compare with cached values to avoid redundant coefficient math.
        // The tolerance check avoids recalculation from float noise.
        if (std::abs(low - cachedLow_) > 1.0e-6f ||
            std::abs(high - cachedHigh_) > 1.0e-6f) {
            cachedLow_  = low;
            cachedHigh_ = high;
            recalculateEQ(low, high);
        }

        // --- Pre-gain stage ---
        // The HM-2's discrete BJT gain stage (Q2/Q3 + Q6/Q7) provides
        // massive gain before the clipping stages. The Dist knob controls
        // how hard the signal slams into the clipping.
        //
        // Gain mapping: exponential curve from 1x (clean) to 101x (~40dB).
        // The exponential mapping gives the knob a natural feel: small
        // adjustments at low settings, rapid gain increase at high settings.
        const float inputGain = 1.0f + dist * dist * 100.0f;
        for (int i = 0; i < numFrames; ++i) {
            driveBuffer_[i] = buffer[i] * inputGain;
        }

        // --- Upsample 2x ---
        // The three nonlinear clipping stages generate significant harmonic
        // content. 2x oversampling pushes the Nyquist frequency higher so
        // that aliased harmonics are attenuated by the anti-imaging filter.
        // We use 2x (not 8x) because the HM-2 is already extremely
        // aggressive and some aliasing actually contributes to its character.
        const int oversampledLen = oversampler_.getOversampledSize(numFrames);
        float* upsampled = oversampler_.upsample(driveBuffer_.data(), numFrames);

        // --- Three clipping stages at oversampled rate ---
        for (int i = 0; i < oversampledLen; ++i) {
            double x = static_cast<double>(upsampled[i]);

            // Stage 1: Asymmetric soft clipping (IC1B feedback diodes)
            x = asymmetricSoftClip(x);

            // Stage 2: Germanium coring (D6, D7 series deadband)
            x = germaniumCoringADAA(x);

            // Stage 3: WDF hard clipping (D8, D9 shunt silicon diodes)
            // Normalize from diode voltage range (~±0.7V) back to audio range.
            x = wdfHardClip(x) * kWdfOutputScaling;

            upsampled[i] = static_cast<float>(x);
        }

        // --- Downsample back to native rate ---
        oversampler_.downsample(upsampled, numFrames, buffer);

        // --- Gyrator EQ (4-band biquad cascade) ---
        // This runs at the native sample rate (not oversampled) because
        // the EQ is a linear operation that does not generate new harmonics.
        for (int i = 0; i < numFrames; ++i) {
            double x = static_cast<double>(buffer[i]);

            x = processBiquad(eqLow_,      x);  // 87 Hz peak (Low knob)
            x = processBiquad(eqMidScoop_,  x);  // 240 Hz scoop (fixed)
            x = processBiquad(eqHighPeak1_, x);  // 960 Hz peak (High knob)
            x = processBiquad(eqHighPeak2_, x);  // 1280 Hz peak (High knob)

            buffer[i] = static_cast<float>(x);
        }

        // --- Output level and safety clamp ---
        // Audio taper (quadratic) for the Level knob gives a natural
        // perceived-loudness response. Clamping prevents downstream
        // effects from receiving out-of-range samples.
        const float outputGain = level * level;
        for (int i = 0; i < numFrames; ++i) {
            buffer[i] = clamp(buffer[i] * outputGain, -1.0f, 1.0f);
        }
    }

    /**
     * Configure sample rate and pre-allocate all internal buffers.
     * Called once during stream setup before any process() calls.
     *
     * @param sampleRate Device native sample rate in Hz.
     */
    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;

        const int maxFrames = 2048;

        // Pre-allocate oversampler and drive buffer
        oversampler_.prepare(maxFrames);
        driveBuffer_.resize(maxFrames, 0.0f);

        // Prepare WDF reactive elements at the OVERSAMPLED rate.
        // The WDF capacitor discretization depends on sample rate.
        const double oversampledRate = static_cast<double>(sampleRate) * 2.0;
        wdfCapClip_.prepare(oversampledRate);
        wdfDiodeClip_.calcImpedance();

        // Initialize EQ coefficients with default parameter values
        cachedLow_  = low_.load(std::memory_order_relaxed);
        cachedHigh_ = high_.load(std::memory_order_relaxed);
        recalculateEQ(cachedLow_, cachedHigh_);
    }

    /**
     * Reset all internal state (filter histories, WDF delay elements,
     * oversampler FIR history). Called when the audio stream restarts
     * or the effect is re-enabled to prevent stale samples from the
     * previous signal from leaking through.
     */
    void reset() override {
        oversampler_.reset();

        // Reset biquad filter states
        resetBiquadState(eqLow_);
        resetBiquadState(eqMidScoop_);
        resetBiquadState(eqHighPeak1_);
        resetBiquadState(eqHighPeak2_);

        // Reset ADAA state for germanium coring
        prevCoringInput_ = 0.0;
        prevCoringAntideriv_ = 0.0;

        // Reset WDF circuit state (clears internal wave variables)
        wdfCapClip_.reset();
    }

    // =========================================================================
    // Parameter access (called from UI thread via JNI)
    // =========================================================================

    /**
     * Set a parameter by ID. Thread-safe (writes to atomics).
     *
     * @param paramId  0=Dist, 1=Low, 2=High, 3=Level
     * @param value    Parameter value in [0.0, 1.0].
     */
    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case kParamDist:
                dist_.store(clamp(value, 0.0f, 1.0f),
                            std::memory_order_relaxed);
                break;
            case kParamLow:
                low_.store(clamp(value, 0.0f, 1.0f),
                           std::memory_order_relaxed);
                break;
            case kParamHigh:
                high_.store(clamp(value, 0.0f, 1.0f),
                            std::memory_order_relaxed);
                break;
            case kParamLevel:
                level_.store(clamp(value, 0.0f, 1.0f),
                             std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    /**
     * Get a parameter by ID. Thread-safe (reads from atomics).
     *
     * @param paramId  0=Dist, 1=Low, 2=High, 3=Level
     * @return Current parameter value, or 0.0f if paramId is unknown.
     */
    float getParameter(int paramId) const override {
        switch (paramId) {
            case kParamDist:  return dist_.load(std::memory_order_relaxed);
            case kParamLow:   return low_.load(std::memory_order_relaxed);
            case kParamHigh:  return high_.load(std::memory_order_relaxed);
            case kParamLevel: return level_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    // =========================================================================
    // Parameter IDs
    // =========================================================================

    static constexpr int kParamDist  = 0;  // Pre-gain into clipping stages
    static constexpr int kParamLow   = 1;  // Bass EQ at 87 Hz
    static constexpr int kParamHigh  = 2;  // Upper-mid EQ at 960/1280 Hz
    static constexpr int kParamLevel = 3;  // Output volume

    /** Normalize WDF diode clipper output (~±0.7V) back to [-1, 1] range. */
    static constexpr double kWdfOutputScaling = 1.0 / 0.7;

    // =========================================================================
    // Stage 1: Asymmetric Soft Clipping (IC1B)
    // =========================================================================

    /**
     * Asymmetric soft clipping that models the HM-2's IC1B op-amp
     * feedback diode pair.
     *
     * The positive half-cycle uses a gentler saturation curve (tanh with
     * 0.5x compression) but allows 2x the amplitude before limiting.
     * The negative half-cycle uses a harder saturation (tanh with 1x
     * compression) that clips at unity amplitude.
     *
     * This asymmetry is a deliberate circuit design choice in the HM-2:
     * the feedback diodes have different forward voltages, causing the
     * positive half of the waveform to clip at a higher voltage than the
     * negative half. This generates even-order harmonics (2nd, 4th) in
     * addition to odd-order, contributing to the pedal's rich harmonic
     * content.
     *
     * Transfer function:
     *   x >= 0: y = tanh(x * 0.5) * 2.0   (soft onset, 2x ceiling)
     *   x <  0: y = tanh(x * 1.0) * 1.0   (harder clip, 1x ceiling)
     *
     * @param x Input sample (double precision for WDF pipeline).
     * @return Asymmetrically soft-clipped sample.
     */
    static inline double asymmetricSoftClip(double x) {
        if (x >= 0.0) {
            return fast_math::fast_tanh(x * 0.5) * 2.0;
        } else {
            return fast_math::fast_tanh(x);
        }
    }

    // =========================================================================
    // Stage 2: Germanium Coring (D6, D7 - 1S188FM)
    // =========================================================================

    /**
     * Germanium diode "coring" deadband nonlinearity.
     *
     * Unlike the typical shunt-to-ground clipping topology, the HM-2's
     * germanium diodes (1S188FM) are wired IN SERIES with the signal path.
     * This means they must be forward-biased to conduct; any signal below
     * the germanium forward voltage (~0.3V) is BLOCKED entirely.
     *
     * This creates a DEADBAND (crossover distortion):
     *   |x| < 0.3V: output = 0    (signal blocked)
     *   |x| >= 0.3V: output = x - sign(x) * 0.3V  (offset by Vf)
     *
     * The effect on the audio signal:
     *   - Small-amplitude signals are completely eliminated (noise gate)
     *   - Moderate signals lose their zero-crossing region (harsh texture)
     *   - Large signals pass through with a 0.3V offset
     *
     * This is the single most distinctive element of the HM-2's clipping
     * character. The hard discontinuity at +/-0.3V is intentional and
     * creates the uniquely gritty, "chainsaw" texture. The discontinuity
     * generates wideband harmonic content that no amount of soft clipping
     * or standard hard clipping can replicate.
     *
     * @param x Input sample (double precision).
     * @return Cored sample with deadband at +/-0.3V.
     */
    /**
     * Antiderivative of the germanium coring function for ADAA.
     * F(x) = integral of coring(x):
     *   F(x) = 0                    for |x| < 0.3
     *   F(x) = (x - 0.3)^2 / 2     for x >= 0.3
     *   F(x) = -(x + 0.3)^2 / 2    for x <= -0.3
     */
    static inline double coringAntideriv(double x) {
        constexpr double kT = 0.3;
        if (x >= kT) {
            double d = x - kT;
            return d * d * 0.5;
        } else if (x <= -kT) {
            double d = x + kT;
            return -(d * d * 0.5);
        }
        return 0.0;
    }

    /**
     * First-order ADAA germanium coring (anti-aliased deadband).
     * Uses the antiderivative difference quotient to smooth the
     * discontinuity at +/-0.3V that was the worst aliasing source
     * in the codebase.
     */
    inline double germaniumCoringADAA(double x) {
        double F1 = coringAntideriv(x);
        double F0 = prevCoringAntideriv_;
        double dx = x - prevCoringInput_;

        double result;
        constexpr double kEps = 1e-10;
        if (std::abs(dx) > kEps) {
            result = (F1 - F0) / dx;
        } else {
            // Fallback: direct evaluation at midpoint
            constexpr double kT = 0.3;
            double mid = 0.5 * (x + prevCoringInput_);
            double absMid = std::abs(mid);
            if (absMid < kT) {
                result = 0.0;
            } else {
                result = mid - (mid > 0.0 ? kT : -kT);
            }
        }

        prevCoringInput_ = x;
        prevCoringAntideriv_ = F1;
        return result;
    }

    // =========================================================================
    // Stage 3: WDF Hard Clipping (D8, D9 - 1S2473 Silicon)
    // =========================================================================

    /**
     * WDF hard clipping circuit model.
     *
     * Circuit topology (D8/D9 stage):
     *
     *   Vin ---[R_clip 4.7K]---+--- Vout (across C_clip)
     *                          |
     *                      [C_clip 100nF]
     *                          |
     *                        [D8||D9]  (antiparallel silicon diodes)
     *                          |
     *                         GND
     *
     * The 4.7K resistor (R_clip) sets the impedance seen by the diodes.
     * Higher resistance = softer clipping onset, lower = harder.
     *
     * C16 (100nF capacitor in series with R before the diodes) provides
     * frequency-dependent clipping behavior. At low frequencies, the
     * capacitor's high impedance reduces the signal reaching the diodes,
     * resulting in less clipping of bass content. At high frequencies,
     * the capacitor passes more signal to the diodes, providing the
     * treble-shaping characteristic of the HM-2's hard clipping stage.
     *
     * The silicon diodes (1S2473) are modeled using chowdsp_wdf's
     * DiodePairT with physically accurate parameters:
     *   Is = 2.52e-9 A  (saturation current)
     *   Vt = 25.85e-3 V (thermal voltage at room temperature)
     *
     * WDF Binary Tree Structure:
     *
     *         DiodePairT (root -- non-adaptable nonlinearity)
     *              |
     *       PolarityInverter
     *              |
     *        Series(R, C)
     *         /         \
     *     R_clip       C_clip
     *     4.7K         100nF
     *
     *   IdealVoltageSource drives the Series node.
     *
     * The PolarityInverter is required to ensure correct wave direction
     * between the adapted series subtree and the non-adaptable diode
     * pair at the root. Without it, the reflected waves would have
     * inverted polarity, causing the output voltage to be negated.
     *
     * Wave propagation per sample:
     *   1. Set input voltage on the voltage source
     *   2. DiodePairT receives the reflected wave from the inverter
     *   3. DiodePairT computes the nonlinear scattering (Wright Omega)
     *   4. Inverter receives the diode pair's reflected wave back
     *   5. Read output voltage across the capacitor (Vout)
     *
     * @param x Input sample (double precision).
     * @return Hard-clipped sample with frequency-dependent clipping.
     */
    inline double wdfHardClip(double x) {
        wdfVsClip_.setVoltage(x);

        // Wave propagation: reflect up from parallel adaptor, scatter at
        // diode pair (root nonlinearity), then propagate back down.
        wdfDiodeClip_.incident(wdfParClip_.reflected());
        wdfParClip_.incident(wdfDiodeClip_.reflected());

        // Read voltage across the capacitor (output node)
        return chowdsp::wdft::voltage<double>(wdfCapClip_);
    }

    // =========================================================================
    // WDF Component Declarations (Stage 3 circuit elements)
    // =========================================================================
    //
    // WDF Binary Tree Structure:
    //
    //        DiodePairT (root -- non-adaptable nonlinearity)
    //             |
    //      Parallel(Vs, C)
    //       /           \
    //  ResistiveVs      C_clip
    //  (R=4.7K)         (100nF)
    //
    // Uses ResistiveVoltageSourceT which combines a voltage source with
    // its series resistance (R_clip = 4.7K) into a single WDF element.
    // This avoids the dual-parent problem that would occur if a separate
    // resistor, series adaptor, and ideal voltage source were used.

    // Resistive voltage source: combines the input signal with R_clip (4.7K).
    // The resistance sets the source impedance seen by the diode network.
    chowdsp::wdft::ResistiveVoltageSourceT<double> wdfVsClip_ { 4700.0 };

    // C_clip: 100nF capacitor (C16 in the HM-2 schematic)
    // The capacitor's reactance decreases with frequency (Xc = 1/(2*pi*f*C)),
    // so higher frequencies see less total impedance and are clipped harder.
    // At 340 Hz: Xc ~= 4.7K (equal to R_clip, -3dB frequency of RC).
    chowdsp::wdft::CapacitorT<double> wdfCapClip_ { 100.0e-9 };

    // Parallel connection of the resistive voltage source and capacitor.
    chowdsp::wdft::WDFParallelT<double,
        decltype(wdfVsClip_),
        decltype(wdfCapClip_)> wdfParClip_ { wdfVsClip_, wdfCapClip_ };

    // Antiparallel silicon diode pair (D8 || D9) at the WDF tree root.
    // 1S2473 silicon diodes:
    //   Is = 2.52e-9 A (saturation current)
    //   Vt = 25.85e-3 V (thermal voltage kT/q at ~25C)
    //
    // The DiodePairT uses a fast Wright Omega function approximation
    // rather than Newton-Raphson iteration for real-time performance.
    chowdsp::wdft::DiodePairT<double,
        decltype(wdfParClip_)> wdfDiodeClip_ {
            wdfParClip_,
            2.52e-9,    // Is: saturation current (1S2473 silicon)
            25.85e-3    // Vt: thermal voltage at room temperature
        };

    // =========================================================================
    // Gyrator EQ: Biquad Filter Implementation
    // =========================================================================

    /**
     * Biquad filter state and coefficients.
     *
     * Uses the standard Direct Form II Transposed structure for numerical
     * stability, especially critical at low frequencies (87 Hz) where
     * coefficient values are close to unity and rounding errors accumulate.
     *
     * All state variables are double precision to prevent low-frequency
     * instability that can occur with float biquads below ~200 Hz at
     * 44.1 kHz sample rate.
     */
    struct BiquadState {
        // Coefficients (normalized: a0 = 1.0)
        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;

        // Filter state (Direct Form II Transposed)
        double z1 = 0.0, z2 = 0.0;
    };

    /**
     * Process one sample through a biquad filter.
     *
     * Direct Form II Transposed implementation:
     *   y[n] = b0*x[n] + z1
     *   z1   = b1*x[n] - a1*y[n] + z2
     *   z2   = b2*x[n] - a2*y[n]
     *
     * This form has better numerical properties than Direct Form I for
     * narrow-bandwidth (high-Q) filters, which is relevant for our
     * gyrator EQ bands with Q values of 1.5-2.0.
     *
     * @param state Biquad state struct (coefficients + delay elements).
     * @param x     Input sample.
     * @return Filtered output sample.
     */
    static inline double processBiquad(BiquadState& state, double x) {
        double y = state.b0 * x + state.z1;
        state.z1 = state.b1 * x - state.a1 * y + state.z2;
        state.z2 = state.b2 * x - state.a2 * y;
        // Prevent denormalized floats in feedback path (ARM performance)
        state.z1 = fast_math::denormal_guard(state.z1);
        state.z2 = fast_math::denormal_guard(state.z2);
        return y;
    }

    /**
     * Reset a biquad filter's delay state to zero.
     *
     * @param state Biquad state to reset.
     */
    static void resetBiquadState(BiquadState& state) {
        state.z1 = 0.0;
        state.z2 = 0.0;
    }

    /**
     * Calculate peaking EQ biquad coefficients.
     *
     * Peaking EQ (also called parametric bell or peak/notch) boosts or
     * cuts a band of frequencies centered at f0 with bandwidth controlled
     * by Q. This models the gyrator-based resonant circuits in the HM-2's
     * active EQ section.
     *
     * The gyrator circuit uses an op-amp, capacitor, and resistors to
     * simulate an inductor, creating a resonant LC-like peak. The biquad
     * peaking EQ accurately reproduces this frequency response.
     *
     * Coefficient formulas (from Robert Bristow-Johnson's Audio EQ Cookbook):
     *   A     = 10^(dBgain/40)     -- square root of linear gain
     *   w0    = 2*pi*f0/fs         -- normalized angular frequency
     *   alpha = sin(w0)/(2*Q)      -- bandwidth parameter
     *
     *   b0 = 1 + alpha*A           -- boost: adds energy at resonance
     *   b1 = -2*cos(w0)            -- determines center frequency
     *   b2 = 1 - alpha*A           -- complement of b0
     *   a0 = 1 + alpha/A           -- normalization denominator
     *   a1 = -2*cos(w0)            -- same as b1 (symmetric)
     *   a2 = 1 - alpha/A           -- complement of a0
     *
     * All b/a coefficients are normalized by dividing by a0.
     *
     * @param state   Biquad state struct to write coefficients into.
     * @param fs      Sample rate in Hz.
     * @param f0      Center frequency in Hz.
     * @param Q       Quality factor (bandwidth = f0/Q).
     * @param dBgain  Gain at center frequency in decibels.
     */
    static void calcPeakingEQ(BiquadState& state, double fs,
                               double f0, double Q, double dBgain) {
        // A = 10^(dBgain/40) gives the square root of the linear gain.
        // At the center frequency, the filter gain is A^2 = 10^(dBgain/20).
        // Using A (not A^2) in the coefficient formulas ensures the correct
        // gain at resonance while maintaining filter stability.
        const double A = std::pow(10.0, dBgain / 40.0);

        // Normalized angular frequency
        const double w0 = 2.0 * M_PI * f0 / fs;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);

        // Bandwidth parameter: alpha = sin(w0)/(2*Q)
        // Smaller Q -> wider bandwidth, larger Q -> narrower peak
        const double alpha = sinW0 / (2.0 * Q);

        // Compute raw coefficients
        const double b0 = 1.0 + alpha * A;
        const double b1 = -2.0 * cosW0;
        const double b2 = 1.0 - alpha * A;
        const double a0 = 1.0 + alpha / A;
        const double a1 = -2.0 * cosW0;
        const double a2 = 1.0 - alpha / A;

        // Normalize by a0 (standard biquad convention: a0 = 1.0)
        const double invA0 = 1.0 / a0;
        state.b0 = b0 * invA0;
        state.b1 = b1 * invA0;
        state.b2 = b2 * invA0;
        state.a1 = a1 * invA0;
        state.a2 = a2 * invA0;
    }

    /**
     * Recalculate all four EQ band coefficients from current parameters.
     *
     * This is called when the Low or High parameter changes (detected by
     * the tolerance check in process()). We avoid per-sample coefficient
     * recalculation because:
     *   1. std::cos/std::sin/std::pow are expensive (~50-100ns each on ARM)
     *   2. Parameters change at UI rate (~60Hz), not audio rate (~48kHz)
     *   3. Abrupt coefficient changes cause at most a tiny click, which is
     *      inaudible in the context of heavy distortion
     *
     * The mid scoop is always recalculated too (in case sample rate changed),
     * but its gain is fixed at -12dB regardless of knob positions.
     *
     * @param low  Low parameter value [0.0, 1.0].
     * @param high High parameter value [0.0, 1.0].
     */
    void recalculateEQ(float low, float high) {
        const double fs = static_cast<double>(sampleRate_);
        if (fs <= 0.0) return;

        // Map parameter [0.0, 1.0] -> gain [-15, +15] dB
        // Linear mapping: 0.0 = -15dB (full cut), 0.5 = 0dB, 1.0 = +15dB
        const double lowGainDb  = -15.0 + static_cast<double>(low)  * 30.0;
        const double highGainDb = -15.0 + static_cast<double>(high) * 30.0;

        // Band 1: Low peak at 87 Hz, Q=2
        // Right on the low-E string fundamental (82 Hz). When boosted,
        // this creates the crushing low-end that defines the HM-2 tone
        // in Swedish death metal -- the "rumble" beneath the chainsaw buzz.
        calcPeakingEQ(eqLow_, fs, 87.0, 2.0, lowGainDb);

        // Band 2: Fixed mid scoop at 240 Hz, Q=1.5, -12dB
        // ALWAYS present regardless of knob positions. This is the
        // defining frequency characteristic of the HM-2: a deep notch
        // right in the lower-midrange that creates the "hollow" sound.
        // The 240 Hz center sits between the bass fundamental region
        // and the upper-mid attack region, creating the distinctive
        // scooped profile.
        calcPeakingEQ(eqMidScoop_, fs, 240.0, 1.5, -12.0);

        // Band 3: High peak 1 at 960 Hz, Q=2
        // Band 4: High peak 2 at 1280 Hz, Q=2
        // The "High" knob on the HM-2 does NOT boost treble (despite
        // the label). Instead, it boosts the upper-midrange at two
        // closely-spaced peaks. This creates an aggressive, bark-like
        // presence that cuts through dense mixes. The dual-peak design
        // creates a broader, flatter boost in the 800-1500 Hz region
        // than a single peak would achieve.
        calcPeakingEQ(eqHighPeak1_, fs, 960.0,  2.0, highGainDb);
        calcPeakingEQ(eqHighPeak2_, fs, 1280.0, 2.0, highGainDb);
    }

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * Clamp a value to a range. Inline for zero-overhead in hot paths.
     */
    static float clamp(float value, float minVal, float maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    // =========================================================================
    // Parameters (written from UI thread, read from audio thread)
    // =========================================================================

    std::atomic<float> dist_  { 0.7f };  // [0, 1] distortion / pre-gain
    std::atomic<float> low_   { 0.5f };  // [0, 1] bass EQ (maps to +/-15dB)
    std::atomic<float> high_  { 0.5f };  // [0, 1] upper-mid EQ (maps to +/-15dB)
    std::atomic<float> level_ { 0.5f };  // [0, 1] output volume

    // =========================================================================
    // Audio thread state
    // =========================================================================

    int32_t sampleRate_ = 44100;

    // 2x oversampler for the three nonlinear clipping stages
    Oversampler<2> oversampler_;

    // Pre-allocated buffer for the drive-gain stage (before upsampling).
    // Avoids modifying the input buffer, which would corrupt the dry signal
    // if wet/dry mixing is used by the signal chain.
    std::vector<float> driveBuffer_;

    // Cached parameter values for detecting when EQ recalculation is needed.
    // These are only accessed from the audio thread, so no atomic needed.
    float cachedLow_  = 0.5f;
    float cachedHigh_ = 0.5f;

    // Four biquad sections modeling the gyrator-based active EQ.
    // Each section handles one frequency band of the HM-2's signature EQ curve.
    BiquadState eqLow_;       // 87 Hz peak (Low knob)
    BiquadState eqMidScoop_;  // 240 Hz scoop (fixed -12dB, always on)
    BiquadState eqHighPeak1_; // 960 Hz peak (High knob)
    BiquadState eqHighPeak2_; // 1280 Hz peak (High knob)

    // ADAA state for germanium coring anti-aliasing
    double prevCoringInput_ = 0.0;
    double prevCoringAntideriv_ = 0.0;
};

#endif // GUITAR_ENGINE_BOSS_HM2_H
