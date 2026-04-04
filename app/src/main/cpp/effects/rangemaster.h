#ifndef GUITAR_ENGINE_RANGEMASTER_H
#define GUITAR_ENGINE_RANGEMASTER_H

#include "effect.h"
#include "oversampler.h"
#include "fast_math.h"
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
 * Clipping model — asymmetric germanium transistor (OC44):
 *   The OC44 PNP transistor clips asymmetrically in the common-emitter stage:
 *   - Positive output swing (toward Vcc): transistor approaches cutoff —
 *     soft, gradual clipping as the device simply stops conducting.
 *   - Negative output swing (toward ground): transistor saturates
 *     (Vce_sat ~ 0.1V) — harder, more abrupt clipping.
 *   This asymmetry generates even-order harmonics (2nd, 4th) which are the
 *   source of the "warm, sweet" Rangemaster character. Modeled with a
 *   dual-slope tanh waveshaper (2:1 drive ratio, negative-side scaling).
 *
 * Gain model — C2 bypass cap effect:
 *   The 47uF emitter bypass cap (C2) shorts R4 (3.9K) for all audio
 *   frequencies: fc = 1/(2*pi*3900*47e-6) = 0.87 Hz. Therefore AC gain
 *   is Av = R3 / (r_e + R_range) where r_e ~ 58 ohm (Vt/Ic = 26mV/0.45mA).
 *   At max range (R_range=0): Av = 10K/58 = 172x (45dB). This is essential
 *   for driving the germanium clipper into meaningful soft clipping.
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
     *         -> Asymmetric Ge transistor clipper (OC44 cutoff/saturation model)
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
        // With C2 bypass cap (47uF, fc=0.87Hz), R4 is shorted for AC.
        // AC gain = R3 / (r_e + R_range_remaining)
        // r_e = Vt/Ic ~ 26mV/0.45mA ~ 58 ohm (OC44 at typical bias point)
        //
        // The Range pot (10K) varies the un-bypassed emitter resistance:
        //   range=0 (pot fully CCW): R_range = 10K, Av = 10K/(58+10K) ~ 1.0
        //   range=0.5:               R_range = 5K,  Av = 10K/(58+5K)  ~ 2.0
        //   range=1 (pot fully CW):  R_range = 0,   Av = 10K/58       ~ 172
        //
        // This is the correct gain model: the 47uF C2 bypass cap eliminates
        // R4 from the AC signal path, leaving only r_e and the Range pot.
        constexpr float kRe = 58.0f;  // transistor intrinsic emitter resistance
        const float rangeResistance = 10.0e3f * (1.0f - range);
        const float acGain = 10.0e3f / (kRe + rangeResistance);

        // Scale to drive the Ge clipper appropriately.
        // Guitar signal ~100-300mV peak. With acGain up to 172x, the signal
        // can reach very high amplitudes. We reduce the input scaling because
        // the gain is now much higher than the old (incorrect) model.
        constexpr float kInputScaling = 0.15f;
        const float preClipGain = acGain * kInputScaling;

        // Output volume with audio taper for natural feel.
        // Audio taper: volume^2 maps [0,1] to perceptually linear loudness.
        const float outputGain = volume * volume;

        // Post-clipping output scaling: the Ge clipper outputs in roughly
        // [-1, 1] range (tanh bounded). Scale to appropriate output level.
        constexpr float kOutputScaling = 1.8f;

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
        // Asymmetric Ge transistor clipping with 2x oversampling
        // ------------------------------------------------------------------
        const int oversampledLen = oversampler_.getOversampledSize(numFrames);
        float* upsampled = oversampler_.upsample(preOsBuffer_.data(), numFrames);

        // Process each oversampled sample through the asymmetric Ge clipper.
        // The OC44 PNP transistor clips differently on each half-cycle:
        //   Positive: soft cutoff (transistor stops conducting)
        //   Negative: harder saturation (Vce bottoms out at ~0.1V)
        for (int i = 0; i < oversampledLen; ++i) {
            upsampled[i] = geTransistorClip(upsampled[i]);
        }

        // Downsample back to the original rate
        oversampler_.downsample(upsampled, numFrames, postOsBuffer_.data());

        // ------------------------------------------------------------------
        // Output coupling + volume
        // ------------------------------------------------------------------
        for (int i = 0; i < numFrames; ++i) {
            float x = postOsBuffer_[i];

            // Scale clipped signal to appropriate output level
            x *= kOutputScaling;

            // ----- Output coupling high-pass (C3 = 10uF) -----
            //
            // The output coupling capacitor blocks DC offset from the
            // transistor's collector bias point (~4-5V). With a typical
            // load impedance of ~1M (next stage input):
            //   fc = 1 / (2*pi * 1M * 10e-6) ~ 0.016 Hz
            //
            // We model this at 3 Hz to also serve as a practical DC blocker
            // that removes any DC offset from the clipping stage.
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
     * Reset all internal filter state.
     * Called when the audio stream restarts or the effect is re-enabled
     * to prevent stale samples from leaking through as clicks/pops.
     */
    void reset() override {
        oversampler_.reset();

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
    // Asymmetric Germanium Transistor Clipper
    // =========================================================================

    /**
     * Asymmetric germanium transistor clipping (OC44 common-emitter).
     *
     * The OC44 PNP germanium transistor in the Rangemaster clips differently
     * on each half of the signal swing:
     *
     * Positive output (toward cutoff): soft, gradual — the transistor simply
     * stops conducting as the base-emitter voltage drops below threshold.
     * Modeled with tanh at moderate drive (0.8x).
     *
     * Negative output (toward saturation): harder — the collector-emitter
     * voltage bottoms out at Vce_sat ~ 0.1V, creating a more abrupt limit.
     * Modeled with tanh at higher drive (1.6x) and 0.9x amplitude scaling
     * to reflect the lower Vce_sat headroom.
     *
     * The asymmetry ratio of ~2:1 (negative drive / positive drive) generates
     * even-order harmonics (2nd, 4th) — the "warm, sweet" Rangemaster
     * character that distinguishes it from symmetric clippers which produce
     * odd-order harmonics.
     *
     * @param x Input sample (pre-amplified signal).
     * @return Clipped sample with asymmetric germanium character.
     */
    static inline float geTransistorClip(float x) {
        if (x >= 0.0f) {
            // Cutoff side: soft, gradual (transistor stops conducting)
            return fast_math::fast_tanh(x * 0.8f);
        } else {
            // Saturation side: harder clip (Vce_sat ~ 0.1V)
            return fast_math::fast_tanh(x * 1.6f) * 0.9f;
        }
    }

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
