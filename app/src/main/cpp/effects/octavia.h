#ifndef GUITAR_ENGINE_OCTAVIA_H
#define GUITAR_ENGINE_OCTAVIA_H

#include "effect.h"
#include "oversampler.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * Roger Mayer Octavia octave-up fuzz pedal emulation.
 *
 * The Octavia (also known as the "Octavio") is the legendary octave-up fuzz
 * pedal famously used by Jimi Hendrix on "Purple Haze," "Fire," and the
 * Band of Gypsys recordings. Roger Mayer originally built it for Hendrix
 * in 1967. The pedal produces a distinctive octave-doubling effect layered
 * with fuzz distortion, creating an aggressive, synth-like tone that is
 * most pronounced on notes above the 12th fret (~330 Hz and above).
 *
 * == How the Octave-Up Effect Works ==
 *
 *   The Octavia achieves its octave-up effect through FULL-WAVE RECTIFICATION
 *   of the guitar signal. The original circuit uses a transformer with a
 *   center-tapped secondary and two germanium diodes (or silicon in later
 *   versions) to perform the rectification:
 *
 *   1. The transformer splits the signal into two out-of-phase copies
 *   2. Each copy is half-wave rectified by a diode
 *   3. The two rectified halves are summed, producing a full-wave rectified
 *      output where every negative half-cycle is "folded" to positive
 *
 *   Mathematically, full-wave rectification is simply |x| (absolute value).
 *   The fundamental frequency of |sin(wt)| is 2w -- exactly one octave up.
 *   This is because the period is halved: what was one full cycle (positive
 *   + negative half) becomes two identical positive humps.
 *
 *   The octave effect is FREQUENCY-DEPENDENT in practice:
 *   - Above ~500 Hz (12th fret): clean, prominent octave-up
 *   - 200-500 Hz: mixed octave + fundamental, "ring modulator" quality
 *   - Below 200 Hz: messy intermodulation, the octave is obscured
 *
 *   This frequency dependence arises naturally from the interaction between
 *   the rectification harmonics and the guitar's harmonic content. No special
 *   filtering is needed to achieve this behavior.
 *
 * == Signal Flow ==
 *
 *   Input
 *     |
 *     v
 *   [Pre-Gain Stage] -- Fuzz knob controls input amplification.
 *     |                  Maps 0-1 to 1x-12x gain (exponential curve).
 *     |                  Models the transistor gain stage (3x silicon BJTs
 *     |                  in the all-silicon version).
 *     v
 *   +--------- 4x Oversampled Region (anti-alias rectification+fuzz) ------+
 *   |                                                                        |
 *   |  [Full-Wave Rectification with First-Order ADAA]                      |
 *   |    The core octave-doubling nonlinearity: y = |x|.                    |
 *   |    Raw abs() has a derivative discontinuity at x=0 that causes        |
 *   |    severe aliasing. ADAA (Anti-Derivative Anti-Aliasing) eliminates   |
 *   |    this by computing the integral analytically:                        |
 *   |      Antiderivative F(x) = x*|x|/2                                   |
 *   |      Output: y[n] = (F(x[n]) - F(x[n-1])) / (x[n] - x[n-1])        |
 *   |    This completely removes the zero-crossing discontinuity.            |
 *   |                                                                        |
 *   |  [Fuzz Clipping Stage]                                                |
 *   |    Soft clipping via tanh waveshaping, scaled by the Fuzz parameter.  |
 *   |    Models the germanium diode (BAT46) clipping in the original        |
 *   |    circuit. The fuzz adds rich even and odd harmonics on top of the    |
 *   |    octave-doubled signal.                                              |
 *   |    Drive amount: 1.0 + fuzz * 9.0 (1x to 10x into tanh)             |
 *   |                                                                        |
 *   +------------------------------------------------------------------------+
 *     |
 *     v
 *   [Post-Clipping Low-Pass Filter] -- One-pole LP at ~8kHz
 *     |   Tames the harsh upper harmonics from rectification + fuzz.
 *     |   The original circuit's output transformer and coupling caps
 *     |   naturally roll off high frequencies.
 *     v
 *   [Output Level] -- Level knob with audio taper (quadratic)
 *     |
 *     v
 *   Output
 *
 * == Anti-Aliasing Strategy ==
 *
 *   Two complementary techniques are used:
 *
 *   1. ADAA (first-order) on the rectification:
 *      The abs() function has infinite bandwidth at the zero-crossing
 *      discontinuity. ADAA analytically removes this discontinuity by
 *      operating on the antiderivative, making the output band-limited
 *      even without oversampling. However, the subsequent fuzz stage
 *      re-introduces harmonics, so we still need oversampling.
 *
 *   2. 4x oversampling around the entire nonlinear section:
 *      The fuzz (tanh waveshaping) generates harmonics at integer multiples
 *      of the input frequency. With 4x oversampling at 48kHz base rate,
 *      the effective Nyquist is 96kHz, providing adequate headroom for
 *      the combined harmonic content of rectification + fuzz.
 *      4x is chosen over 2x because the rectification doubles the
 *      fundamental, then the fuzz generates harmonics of THAT doubled
 *      frequency -- so harmonic content extends much higher than a
 *      standard distortion pedal.
 *
 * == Parameters ==
 *   Param 0: "Fuzz"  (0.0-1.0, default 0.7) Pre-gain and clipping drive
 *   Param 1: "Level" (0.0-1.0, default 0.5) Output volume
 *
 * All parameters use std::atomic<float> for lock-free UI/audio thread
 * communication, consistent with the rest of this codebase.
 *
 * == Technical Notes ==
 *   - ADAA uses double precision for the antiderivative computation to
 *     avoid catastrophic cancellation in (F(x[n]) - F(x[n-1])) when
 *     consecutive samples are close in value
 *   - Post-clipping LP filter state uses double precision for stability
 *   - All buffers pre-allocated in setSampleRate() for real-time safety
 *   - Output normalization prevents hot signal levels from the combined
 *     gain + rectification + fuzz stages
 *
 * == References ==
 *   - R.B.J. Audio EQ Cookbook (tone filter design)
 *   - J. Parker et al., "Reducing the Aliasing of Nonlinear Waveshaping
 *     Using Continuous-Time Convolution," DAFx-16 (ADAA theory)
 *   - Electrosmash: Roger Mayer Octavia circuit analysis
 *   - D. Yeh, "Digital Implementation of Musical Distortion Circuits,"
 *     PhD thesis, CCRMA, Stanford, 2009 (oversampling + ADAA for pedals)
 */
class Octavia : public Effect {
public:
    Octavia() = default;

    // =========================================================================
    // Effect interface
    // =========================================================================

    /**
     * Process audio through the Octavia signal chain. Real-time safe.
     *
     * Signal flow per block:
     *   1. Snapshot atomic parameters (one relaxed load each)
     *   2. Apply pre-gain (Fuzz knob -> exponential gain curve)
     *   3. Upsample 4x for anti-aliased nonlinear processing
     *   4. ADAA full-wave rectification + tanh fuzz at oversampled rate
     *   5. Downsample back to native rate
     *   6. Post-clipping low-pass filter (8kHz)
     *   7. Output level scaling and safety clamp
     *
     * @param buffer    Mono audio samples, modified in-place.
     * @param numFrames Number of frames in the buffer.
     */
    void process(float* buffer, int numFrames) override {
        // Guard against being called before setSampleRate() allocates buffers.
        // This prevents a crash if the audio callback fires before stream setup
        // completes (observed on some Android devices during rapid start/stop).
        if (preOsBuffer_.empty()) return;

        // --- Snapshot all parameters once per block ---
        // Relaxed ordering: we only need atomicity (no torn reads), not ordering
        // guarantees relative to other memory. The UI thread writes these; the
        // audio thread reads them here once and uses the snapshot for the block.
        const float fuzz  = fuzz_.load(std::memory_order_relaxed);
        const float level = level_.load(std::memory_order_relaxed);

        // --- Pre-compute per-block constants ---

        // Pre-gain stage: models the Octavia's transistor gain stages.
        // The original circuit has 3 silicon transistors providing ~20-25 dB
        // of gain before the transformer rectifier. The Fuzz knob controls
        // how hard the signal is driven into the rectification + clipping.
        //
        // Gain mapping: exponential curve from 1x (clean) to 12x (~21.6dB).
        // The quadratic mapping (fuzz^2) gives the knob a natural feel:
        // gradual increase at low settings, rapid gain at high settings.
        //
        // NOTE: Previous value of 30x was too aggressive -- it drove the
        // downstream tanh() into hard square-wave clipping at all useful
        // fuzz settings, losing the soft germanium character. 12x keeps
        // tanh in its musically interesting nonlinear region (input 1-4).
        const float preGain = 1.0f + fuzz * fuzz * 11.0f;

        // Fuzz clipping drive: controls how hard the rectified signal is
        // pushed into the tanh waveshaper. Separate from pre-gain because
        // the clipping occurs AFTER rectification in the original circuit.
        // Range: 1.0 (clean tanh, gentle saturation) to 4.0 (heavy fuzz).
        //
        // NOTE: Previous value of 10.0 max was too aggressive -- combined
        // with the preGain, it pushed tanh(x) to x>10 where tanh~=1.0
        // (hard clipping). Reducing to 4.0 keeps the waveshaper in the
        // soft saturation region (tanh(1-3) = 0.76-0.995), preserving the
        // round, warm character of the germanium Octavia circuit.
        const float fuzzDrive = 1.0f + fuzz * 3.0f;

        // Output level with audio taper (quadratic) for natural volume feel.
        // Audio taper: level^2 approximation maps [0,1] to a perceptually
        // linear loudness curve, matching real audio pot behavior.
        const float outputGain = level * level;

        // --- Pre-gain stage (before oversampling) ---
        for (int i = 0; i < numFrames; ++i) {
            preOsBuffer_[i] = buffer[i] * preGain;
        }

        // --- Upsample 4x ---
        // The rectification + fuzz stages generate very dense harmonic content.
        // Rectification doubles the fundamental, then fuzz generates harmonics
        // of that doubled frequency. 4x oversampling at 48kHz base rate pushes
        // Nyquist to 96kHz, providing adequate headroom. We use 4x (not 2x)
        // specifically because the combination of octave-up + fuzz produces
        // significantly more ultrasonic content than fuzz alone.
        const int oversampledLen = oversampler_.getOversampledSize(numFrames);
        float* upsampled = oversampler_.upsample(preOsBuffer_.data(), numFrames);

        // --- ADAA rectification + fuzz clipping at oversampled rate ---
        for (int i = 0; i < oversampledLen; ++i) {
            // Convert to double for ADAA computation. The antiderivative
            // subtraction (F(x[n]) - F(x[n-1])) suffers from catastrophic
            // cancellation when consecutive samples are close in value.
            // Double precision (52-bit mantissa) vs float (23-bit mantissa)
            // provides the necessary headroom.
            double x = static_cast<double>(upsampled[i]);

            // ---------------------------------------------------------------
            // First-order ADAA full-wave rectification
            // ---------------------------------------------------------------
            //
            // The goal is to compute y = |x| without the zero-crossing
            // discontinuity in the derivative that causes aliasing.
            //
            // First-order ADAA replaces the pointwise nonlinearity f(x) = |x|
            // with an integral-based formula:
            //
            //   y[n] = (F(x[n]) - F(x[n-1])) / (x[n] - x[n-1])
            //
            // where F(x) is the antiderivative of f(x) = |x|:
            //
            //   F(x) = x * |x| / 2
            //         = x^2 / 2    for x >= 0
            //         = -x^2 / 2   for x < 0
            //
            // When x[n] is very close to x[n-1] (within epsilon), the
            // division becomes numerically unstable. In this case we fall
            // back to the direct evaluation f(x) = |x|, which is acceptable
            // because small differences between consecutive samples mean the
            // signal is not crossing zero rapidly (the problematic case).
            //
            double Fx = adaaAntiderivative(x);
            double diff = x - adaaPrevInput_;
            double rectified;

            if (std::abs(diff) > kAdaaEpsilon) {
                // Normal ADAA path: use the antiderivative difference quotient
                rectified = (Fx - adaaPrevFx_) / diff;
            } else {
                // Fallback: direct evaluation when samples are nearly equal.
                // This only occurs when the signal is approximately constant
                // (no zero crossing), so abs() is safe here.
                rectified = std::abs(x);
            }

            // Store state for next sample's ADAA computation
            // Denormal guard prevents 10-100x slowdown on ARM if signal
            // decays to subnormal range during prolonged silence.
            adaaPrevInput_ = fast_math::denormal_guard(x);
            adaaPrevFx_ = fast_math::denormal_guard(Fx);

            // ---------------------------------------------------------------
            // Fuzz clipping stage (tanh waveshaping)
            // ---------------------------------------------------------------
            //
            // After rectification, the signal is always non-negative. The
            // tanh waveshaper adds harmonic richness (both even and odd order)
            // and provides soft clipping that models the germanium diode
            // (BAT46) clipping in the original Octavia circuit.
            //
            // The fuzzDrive parameter controls how aggressively the signal
            // is pushed into saturation:
            //   - fuzzDrive = 1.0 (Fuzz=0): gentle saturation, octave is clean
            //   - fuzzDrive = 10.0 (Fuzz=1): hard fuzz, heavily compressed
            //
            // We use tanh(x * drive) which has the desirable property of
            // being C-infinity smooth (no discontinuities at any derivative
            // order), bounded to (-1, +1), and odd-symmetric. The odd
            // symmetry means it preserves the DC offset from rectification
            // (which is removed later by the AC coupling in the signal chain).
            double fuzzed = fast_math::fast_tanh(rectified * static_cast<double>(fuzzDrive));

            upsampled[i] = static_cast<float>(fuzzed);
        }

        // --- Downsample back to native rate ---
        oversampler_.downsample(upsampled, numFrames, buffer);

        // --- Post-clipping low-pass filter (8kHz) ---
        // Tames the harsh upper harmonics from the rectification + fuzz stages.
        // The original Octavia circuit naturally rolls off high frequencies
        // through its output transformer and coupling capacitors. We model
        // this with a simple one-pole LP at ~8kHz, which is sufficient to
        // remove the "fizzy" quality without dulling the tone.
        //
        // Using double precision for the filter state because one-pole LP
        // filter coefficients near the passband edge can be close to 1.0,
        // and float precision may cause subtle instability over long runs.
        for (int i = 0; i < numFrames; ++i) {
            double x = static_cast<double>(buffer[i]);
            lpPostState_ = (1.0 - lpPostCoeff_) * x
                         + lpPostCoeff_ * lpPostState_;
            lpPostState_ = fast_math::denormal_guard(lpPostState_);
            buffer[i] = static_cast<float>(lpPostState_);
        }

        // --- DC-blocking high-pass filter (~25Hz) ---
        // Full-wave rectification produces a unipolar (always positive) signal.
        // After tanh saturation, the output is ~+0.95 DC with small AC ripple
        // at zero crossings. Without DC removal, the audible AC component is
        // only ~5-15% of the total energy — making the effect sound very quiet.
        // This HP filter models the real Octavia's output coupling capacitor,
        // which AC-couples the signal and removes the DC offset.
        // Both Big Muff and Fuzz Face include similar DC-blocking filters.
        for (int i = 0; i < numFrames; ++i) {
            double x = static_cast<double>(buffer[i]);
            double hpOut = dcBlockCoeff_ * (dcBlockState_ + x - dcBlockPrev_);
            dcBlockPrev_ = x;
            dcBlockState_ = fast_math::denormal_guard(hpOut);
            buffer[i] = static_cast<float>(dcBlockState_);
        }

        // --- Output level and safety clamp ---
        // Apply the Level parameter with audio taper and clamp to prevent
        // downstream effects from receiving out-of-range samples. The
        // normalization constant accounts for the level boost from the
        // combined pre-gain + rectification + fuzz pipeline.
        for (int i = 0; i < numFrames; ++i) {
            buffer[i] = clamp(buffer[i] * outputGain * kOutputNormalization,
                              -1.0f, 1.0f);
        }
    }

    /**
     * Configure sample rate and pre-allocate all internal buffers.
     * Called once during stream setup before any process() calls.
     * This is the ONLY method that allocates memory.
     *
     * @param sampleRate Device native sample rate in Hz.
     */
    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;

        const int maxFrames = 2048;

        // Pre-allocate the oversampler and intermediate buffer
        oversampler_.prepare(maxFrames);
        preOsBuffer_.resize(maxFrames, 0.0f);

        // Pre-compute the post-clipping LP filter coefficient.
        // Cutoff is a fixed circuit characteristic (~8kHz), not user-adjustable.
        // Computing once here avoids std::exp() per block.
        lpPostCoeff_ = computeOnePoleLPCoeff(8000.0);

        // Pre-compute DC-blocking HP filter coefficient (~25Hz).
        // Models the Octavia's output coupling capacitor that removes the
        // DC offset from full-wave rectification.
        dcBlockCoeff_ = computeOnePoleHPCoeff(25.0);
    }

    /**
     * Reset all internal state (filter histories, ADAA state, oversampler
     * FIR history). Called when the audio stream restarts or the effect is
     * re-enabled to prevent stale samples from the previous signal from
     * leaking through as clicks/pops.
     */
    void reset() override {
        oversampler_.reset();

        // Reset ADAA state
        adaaPrevInput_ = 0.0;
        adaaPrevFx_ = 0.0;

        // Reset post-clipping LP filter state
        lpPostState_ = 0.0;

        // Reset DC-blocking HP filter state
        dcBlockState_ = 0.0;
        dcBlockPrev_ = 0.0;
    }

    // =========================================================================
    // Parameter access (called from UI thread via JNI)
    // =========================================================================

    /**
     * Set a parameter by ID. Thread-safe (writes to atomics).
     *
     * @param paramId  0=Fuzz, 1=Level
     * @param value    Parameter value in [0.0, 1.0].
     */
    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case kParamFuzz:
                fuzz_.store(clamp(value, 0.0f, 1.0f),
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
     * @param paramId  0=Fuzz, 1=Level
     * @return Current parameter value, or 0.0f if paramId is unknown.
     */
    float getParameter(int paramId) const override {
        switch (paramId) {
            case kParamFuzz:  return fuzz_.load(std::memory_order_relaxed);
            case kParamLevel: return level_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    // =========================================================================
    // Parameter IDs
    // =========================================================================

    static constexpr int kParamFuzz  = 0;  // Pre-gain + clipping drive
    static constexpr int kParamLevel = 1;  // Output volume

    // =========================================================================
    // Constants
    // =========================================================================

    /**
     * ADAA epsilon threshold for the fallback path.
     *
     * When |x[n] - x[n-1]| < epsilon, the ADAA difference quotient
     * (F(x[n]) - F(x[n-1])) / (x[n] - x[n-1]) becomes numerically
     * unstable (approaching 0/0). In this regime we fall back to the
     * direct evaluation |x|.
     *
     * The value 1e-10 is chosen to be:
     *   - Well above double precision noise floor (~1e-16)
     *   - Well below audible signal levels (~1e-5 for -100dBFS)
     *   - Large enough to catch the problematic near-zero-difference case
     *   - Small enough that the fallback path engages only when the signal
     *     is effectively constant (no audible difference from ADAA)
     */
    static constexpr double kAdaaEpsilon = 1.0e-10;

    /**
     * Output gain compensation constant.
     *
     * The tanh waveshaper bounds the signal to [-1, +1] regardless of
     * input amplitude. After downsampling and the 8kHz LP filter, peak
     * level is typically ~0.8-1.0. The Level knob's audio taper (squared)
     * maps [0,1] to [0,1], so at Level=0.5 the effective gain is only
     * 0.25. Without compensation, this makes the Octavia much quieter
     * than bypass.
     *
     * The real Roger Mayer Octavia is a LOUD effect (+6 to +12 dB above
     * bypass at typical settings). To match this behavior:
     *   Level=0.3: 0.09 * 2.5 = 0.225 (quiet practice level, ~-13dB)
     *   Level=0.5: 0.25 * 2.5 = 0.625 (approximately unity with bypass)
     *   Level=0.7: 0.49 * 2.5 = 1.225 (moderate boost, ~+2dB)
     *   Level=1.0: 1.0  * 2.5 = 2.5   (strong boost, ~+8dB)
     *
     * This matches other effects in the codebase: DS-1 uses 1/0.7=1.43,
     * Rangemaster uses 1/0.35=2.86, RAT uses volume*2.0 (linear).
     * The downstream soft limiter in SignalChain handles any peaks above
     * 1.0 gracefully.
     */
    static constexpr float kOutputNormalization = 2.5f;

    // =========================================================================
    // ADAA Full-Wave Rectification
    // =========================================================================

    /**
     * Compute the antiderivative of f(x) = |x|.
     *
     * The antiderivative (indefinite integral) of the absolute value function:
     *
     *   F(x) = integral of |x| dx = x * |x| / 2
     *
     * This can be verified by differentiation:
     *   For x > 0: F(x) = x^2 / 2, so F'(x) = x = |x|     (correct)
     *   For x < 0: F(x) = -x^2 / 2, so F'(x) = -x = |x|   (correct)
     *   At x = 0: F(x) = 0, and F is C0-continuous            (correct)
     *
     * Note: F(x) is C1-continuous everywhere (including at x=0), which is
     * the key property that makes ADAA work. The derivative discontinuity
     * of |x| at zero is "integrated out" into a smooth F(x).
     *
     * @param x Input sample (double precision for numerical stability).
     * @return Antiderivative value F(x) = x * |x| / 2.
     */
    static inline double adaaAntiderivative(double x) {
        return x * std::abs(x) * 0.5;
    }

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
    double computeOnePoleLPCoeff(double cutoffHz) const {
        if (sampleRate_ <= 0) return 0.0;
        const double fs = static_cast<double>(sampleRate_);
        cutoffHz = std::max(20.0, std::min(cutoffHz, fs * 0.49));
        return std::exp(-2.0 * M_PI * cutoffHz / fs);
    }

    /**
     * Compute one-pole high-pass filter coefficient (double precision).
     *
     * For the difference equation y[n] = a * (y[n-1] + x[n] - x[n-1]):
     *   a = RC / (RC + dt) where RC = 1/(2*pi*fc), dt = 1/fs
     *
     * @param cutoffHz Desired cutoff frequency in Hz.
     * @return Filter coefficient (closer to 1.0 = lower cutoff).
     */
    double computeOnePoleHPCoeff(double cutoffHz) const {
        if (sampleRate_ <= 0) return 0.99;
        const double fs = static_cast<double>(sampleRate_);
        cutoffHz = std::max(1.0, std::min(cutoffHz, fs * 0.49));
        const double rc = 1.0 / (2.0 * M_PI * cutoffHz);
        const double dt = 1.0 / fs;
        return rc / (rc + dt);
    }

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * Clamp a value to a range. Inline for zero-overhead in hot paths.
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

    /** Fuzz amount [0, 1]. Controls pre-gain and clipping drive.
     *  Default 0.7 gives classic Hendrix-style octave fuzz. */
    std::atomic<float> fuzz_  { 0.7f };

    /** Output level [0, 1]. Controls final volume after processing.
     *  Default 0.5 provides moderate output. */
    std::atomic<float> level_ { 0.5f };

    // =========================================================================
    // Audio thread state (not accessed from UI thread)
    // =========================================================================

    /** Device sample rate in Hz */
    int32_t sampleRate_ = 44100;

    /**
     * 4x oversampler for the nonlinear rectification + fuzz stages.
     * 4x is chosen because the combination of octave-doubling (2x frequency)
     * plus fuzz harmonics generates significantly more ultrasonic content
     * than a standard distortion pedal. At 48kHz base rate, 4x oversampling
     * pushes Nyquist to 96kHz, providing adequate anti-aliasing headroom.
     */
    Oversampler<4> oversampler_;

    /** Pre-allocated buffer for the gain stage (before upsampling).
     *  Avoids modifying the input buffer, which would corrupt the dry signal
     *  if wet/dry mixing is used by the signal chain. */
    std::vector<float> preOsBuffer_;

    // ---- ADAA state (double precision for numerical stability) ----

    /** Previous input sample for ADAA difference quotient.
     *  Initialized to 0.0 so the first sample uses the fallback path. */
    double adaaPrevInput_ = 0.0;

    /** Previous antiderivative value F(x[n-1]) for ADAA.
     *  F(0) = 0 * |0| / 2 = 0, consistent with adaaPrevInput_ = 0. */
    double adaaPrevFx_ = 0.0;

    // ---- Post-clipping LP filter (double precision for stability) ----

    /** Post-clipping low-pass filter coefficient (set in setSampleRate()).
     *  For the equation y[n] = (1-a)*x[n] + a*y[n-1]. */
    double lpPostCoeff_ = 0.0;

    /** Post-clipping low-pass filter state. Double precision prevents
     *  subtle instability from coefficient values near 1.0 at high
     *  sample rates (8kHz cutoff at 192kHz = coefficient ~0.77). */
    double lpPostState_ = 0.0;

    // ---- DC-blocking HP filter (removes rectification DC offset) ----

    /** DC-blocking HP filter coefficient (~25Hz). Models the real
     *  Octavia's output coupling capacitor. Double precision needed
     *  because the coefficient is very close to 1.0 at low cutoffs. */
    double dcBlockCoeff_ = 0.99;

    /** DC-blocking HP filter state. */
    double dcBlockState_ = 0.0;

    /** Previous input sample for DC-blocking HP filter. */
    double dcBlockPrev_ = 0.0;
};

#endif // GUITAR_ENGINE_OCTAVIA_H
