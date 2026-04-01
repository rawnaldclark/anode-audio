#ifndef GUITAR_ENGINE_COMPRESSOR_H
#define GUITAR_ENGINE_COMPRESSOR_H

#include "effect.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>

/**
 * Feed-forward dynamic range compressor with soft knee and optional
 * MXR Dyna Comp OTA character mode.
 *
 * A compressor reduces the dynamic range of a signal by attenuating
 * levels that exceed a threshold. For guitar, this serves several purposes:
 *   - Evens out picking dynamics for a more consistent volume
 *   - Adds sustain by boosting quiet tail of notes via makeup gain
 *   - Placed before overdrive, it can tighten the drive response
 *
 * Feed-forward design means the gain computer operates on the INPUT level
 * rather than the output level (feedback design). Feed-forward is more
 * predictable, easier to analyze, and the standard for modern compressors.
 *
 * Algorithm per sample:
 *   1. Compute input level in dB: x_dB = 20 * log10(|x|)
 *      (OTA mode: apply sidechain frequency weighting before dB conversion)
 *   2. Compute gain reduction via gain computer with soft knee
 *   3. Smooth gain with attack/release envelope (ballistics)
 *   4. Apply gain reduction and makeup gain
 *      (OTA mode: add subtle even-harmonic coloration)
 *
 * Soft knee:
 *   Instead of an abrupt transition at the threshold (hard knee),
 *   the ratio gradually increases over a region of width W centered
 *   on the threshold. This sounds more natural and transparent.
 *
 *   In the knee region [T-W/2, T+W/2]:
 *     gain_dB = x_dB + (1/R - 1) * (x_dB - T + W/2)^2 / (2*W)
 *   Below knee: gain_dB = x_dB (no compression)
 *   Above knee: gain_dB = T + (x_dB - T) / R (full compression)
 *
 * Character modes:
 *   VCA (0): Standard transparent VCA-style compression. Default behavior.
 *   OTA (1): Emulates the MXR Dyna Comp's CA3080 OTA gain element:
 *     - Faster attack (0.3x) from passive 1N914 diode envelope detector
 *     - Harder knee (0.4x width) from OTA's non-linear gain control
 *     - Treble-emphasis sidechain (~4kHz LP subtraction) from CA3080's
 *       frequency-dependent transconductance
 *     - Subtle even-harmonic coloration (2nd harmonic) from OTA's
 *       asymmetric transfer function
 *
 * Parameter IDs for JNI access:
 *   0 = threshold (-60 to 0 dB)
 *   1 = ratio (1.0 to 20.0)
 *   2 = attack (0.1 to 100 ms)
 *   3 = release (10 to 1000 ms)
 *   4 = makeupGain (0 to 30 dB)
 *   5 = kneeWidth (0 to 12 dB)
 *   6 = character (0 = VCA, 1 = OTA)
 */
class Compressor : public Effect {
public:
    Compressor() = default;

    // =========================================================================
    // Effect interface
    // =========================================================================

    /**
     * Process audio through the compressor. Real-time safe.
     *
     * When character = OTA, the sidechain uses treble-emphasis weighting,
     * attack/release are scaled faster, knee is narrowed, and subtle
     * even-harmonic content is added after the gain stage.
     *
     * @param buffer   Mono audio samples in [-1.0, 1.0].
     * @param numFrames Number of frames in the buffer.
     */
    void process(float* buffer, int numFrames) override {
        // Snapshot parameters once per buffer to avoid mid-buffer changes
        const float thresholdDb = threshold_.load(std::memory_order_relaxed);
        const float ratio = ratio_.load(std::memory_order_relaxed);
        const float attackMs = attack_.load(std::memory_order_relaxed);
        const float releaseMs = release_.load(std::memory_order_relaxed);
        const float makeupDb = makeupGain_.load(std::memory_order_relaxed);
        const float kneeW = kneeWidth_.load(std::memory_order_relaxed);
        const bool otaMode = character_.load(std::memory_order_relaxed) >= 0.5f;

        // OTA character scaling factors:
        // - Attack: 0.3x (CA3080 passive diode detector is ~3x faster)
        // - Knee: 0.4x (OTA gain control has harder onset)
        const float effectiveAttackMs = otaMode ? (attackMs * kOtaAttackScale) : attackMs;
        const float effectiveKneeW = otaMode ? (kneeW * kOtaKneeScale) : kneeW;

        // Pre-compute coefficients outside the sample loop
        const float attackCoeff = computeCoefficient(effectiveAttackMs);
        const float releaseCoeff = computeCoefficient(releaseMs);
        const float makeupLin = dbToLinear(makeupDb);
        const float halfKnee = effectiveKneeW / 2.0f;

        // Cache the OTA sidechain LP coefficient for this buffer.
        // This is pre-computed in setSampleRate() and does not change
        // per-sample, so reading it once here avoids repeated member access.
        const double scCoeff = otaSidechainCoeff_;

        for (int i = 0; i < numFrames; ++i) {
            const float inputSample = buffer[i];

            // ---- Step 1: Compute sidechain level ----
            // In VCA mode: straightforward |input| -> dB.
            // In OTA mode: the CA3080's transconductance responds more
            // strongly to high-frequency content. We model this by running
            // a one-pole lowpass on |input| and then boosting the difference
            // (i.e., the high-frequency residual) by 50%. This makes the
            // detector trigger faster on pick attacks and string transients,
            // which is the characteristic Dyna Comp "squash" on the attack.
            double scInput = static_cast<double>(std::abs(inputSample));

            if (otaMode) {
                // Sidechain HPF (~60Hz): remove sub-bass from envelope detector
                // to prevent low-E string pumping. The Ross Compressor added this
                // as its main improvement over the original Dyna Comp.
                otaSidechainHPState_ += otaSidechainHPCoeff_ * (scInput - otaSidechainHPState_);
                otaSidechainHPState_ = fast_math::denormal_guard(otaSidechainHPState_);
                scInput = scInput - otaSidechainHPState_;  // HP = input - LP

                // One-pole lowpass on the absolute sidechain signal (~4kHz)
                otaSidechainState_ += scCoeff * (scInput - otaSidechainState_);
                otaSidechainState_ = fast_math::denormal_guard(otaSidechainState_);

                // Treble-emphasis: add 50% of the highpass residual.
                // scInput = original + 0.5 * (original - lowpassed)
                // This lifts content above ~4kHz in the detector path only,
                // without altering the audio signal itself.
                scInput = scInput + (scInput - otaSidechainState_) * 0.5;
            }

            // Convert sidechain level to dB.
            // Use a floor of -96dB to avoid log(0) = -infinity.
            // -96dB corresponds to approximately 16-bit noise floor.
            const float scLevel = static_cast<float>(scInput);
            const float inputDb = (scLevel > kMinLevel)
                ? 20.0f * fast_math::fastLog10(scLevel)
                : kFloorDb;

            // ---- Step 2: Gain computer with soft knee ----
            // Compute the desired output level in dB based on the
            // compression curve (threshold, ratio, knee).
            float gainReductionDb = computeGainReduction(
                inputDb, thresholdDb, ratio, halfKnee);

            // ---- Step 3: Smooth with attack/release ballistics ----
            // Attack = gain reduction is increasing (signal getting louder)
            // Release = gain reduction is decreasing (signal getting quieter)
            //
            // The compressor smoothing is applied to the gain reduction (dB),
            // not the gain itself. This gives musically appropriate behavior:
            // - Fast attack catches transients quickly
            // - Slow release allows natural decay
            if (gainReductionDb > smoothedGainReductionDb_) {
                // Attack: gain reduction increasing (signal exceeding threshold)
                smoothedGainReductionDb_ = attackCoeff * smoothedGainReductionDb_ +
                                           (1.0f - attackCoeff) * gainReductionDb;
            } else {
                // Release: gain reduction decreasing (signal falling below threshold)
                smoothedGainReductionDb_ = releaseCoeff * smoothedGainReductionDb_ +
                                           (1.0f - releaseCoeff) * gainReductionDb;
            }

            // ---- Step 4: Apply gain reduction + makeup gain ----
            // Convert dB reduction to linear multiplier and apply
            const float gainLin = dbToLinear(-smoothedGainReductionDb_) * makeupLin;
            float output = inputSample * gainLin;

            // ---- Step 5 (OTA only): Harmonic coloration ----
            // The CA3080 OTA has an inherently asymmetric transfer function
            // that generates even-order harmonics (primarily 2nd). We model
            // this with a mild quadratic term: y = x + k*x^2, where k=0.05.
            // At full scale (x=1.0), this adds ~5% second harmonic (-26 dB),
            // which matches measured CA3080 THD at typical bias currents.
            // The asymmetry is musically pleasant - it adds warmth and body
            // similar to tube saturation character.
            if (otaMode) {
                output = output + 0.05f * output * output;
            }

            buffer[i] = output;
        }
    }

    /**
     * Configure sample rate. Pre-computes the OTA sidechain lowpass
     * coefficient so that no transcendental math runs per-sample.
     *
     * @param sampleRate Device sample rate in Hz.
     */
    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;

        // OTA sidechain frequency weighting: one-pole lowpass at ~4kHz.
        // The CA3080's transconductance rolls off above ~4kHz, so the
        // sidechain detector effectively emphasizes content near and above
        // this frequency. We use the bilinear-matched one-pole formula:
        //   coeff = 1 - exp(-2*pi*fc / fs)
        // Pre-computing here keeps the per-sample loop free of exp().
        if (sampleRate > 0) {
            otaSidechainCoeff_ = 1.0 - std::exp(
                -2.0 * 3.14159265358979323846 * kOtaSidechainFc
                / static_cast<double>(sampleRate));
            // Sidechain HPF at ~60Hz to prevent bass pumping (Ross mod)
            otaSidechainHPCoeff_ = 1.0 - std::exp(
                -2.0 * 3.14159265358979323846 * 60.0
                / static_cast<double>(sampleRate));
        }
    }

    /**
     * Reset internal state. Clears the envelope follower and the OTA
     * sidechain filter state so that previously processed audio does not
     * bleed into the next activation.
     */
    void reset() override {
        smoothedGainReductionDb_ = 0.0f;
        otaSidechainState_ = 0.0;
        otaSidechainHPState_ = 0.0;
    }

    // =========================================================================
    // Parameter access (called from UI thread via JNI)
    // =========================================================================

    /**
     * Set a parameter by ID. Thread-safe.
     * @param paramId 0=threshold, 1=ratio, 2=attack, 3=release,
     *                4=makeupGain, 5=kneeWidth, 6=character
     * @param value   Parameter value (see ranges in class doc).
     */
    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0:
                threshold_.store(clamp(value, -60.0f, 0.0f),
                                 std::memory_order_relaxed);
                break;
            case 1:
                ratio_.store(clamp(value, 1.0f, 20.0f),
                             std::memory_order_relaxed);
                break;
            case 2:
                attack_.store(clamp(value, 0.1f, 100.0f),
                              std::memory_order_relaxed);
                break;
            case 3:
                release_.store(clamp(value, 10.0f, 1000.0f),
                               std::memory_order_relaxed);
                break;
            case 4:
                makeupGain_.store(clamp(value, 0.0f, 30.0f),
                                  std::memory_order_relaxed);
                break;
            case 5:
                kneeWidth_.store(clamp(value, 0.0f, 12.0f),
                                 std::memory_order_relaxed);
                break;
            case 6:
                character_.store(clamp(value, 0.0f, 1.0f),
                                 std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    /**
     * Get a parameter by ID. Thread-safe.
     * @param paramId 0=threshold, 1=ratio, 2=attack, 3=release,
     *                4=makeupGain, 5=kneeWidth, 6=character
     * @return Current parameter value.
     */
    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return threshold_.load(std::memory_order_relaxed);
            case 1: return ratio_.load(std::memory_order_relaxed);
            case 2: return attack_.load(std::memory_order_relaxed);
            case 3: return release_.load(std::memory_order_relaxed);
            case 4: return makeupGain_.load(std::memory_order_relaxed);
            case 5: return kneeWidth_.load(std::memory_order_relaxed);
            case 6: return character_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    // =========================================================================
    // Constants
    // =========================================================================

    /** Minimum detectable level to avoid log(0). ~16-bit noise floor. */
    static constexpr float kMinLevel = 1.5849e-5f;  // 10^(-96/20)

    /** Floor dB when signal is below minimum level. */
    static constexpr float kFloorDb = -96.0f;

    /** OTA attack time scale factor (0.3x = ~3x faster than VCA).
     *  The CA3080's passive 1N914 diode envelope detector has a much
     *  faster charge time than typical VCA detector circuits. */
    static constexpr float kOtaAttackScale = 0.15f;  // ~0.75ms from 5ms VCA default (real Dyna Comp ~0.5ms)

    /** OTA knee width scale factor (0.4x = 60% narrower / harder knee).
     *  The OTA's exponential transconductance-vs-current characteristic
     *  produces a naturally harder compression onset than VCA designs. */
    static constexpr float kOtaKneeScale = 0.4f;

    /** OTA sidechain lowpass cutoff frequency in Hz.
     *  The CA3080 responds more strongly to high-frequency content due to
     *  the frequency-dependent behavior of its differential pair. We model
     *  this by lowpass-filtering the sidechain at ~4kHz and then adding
     *  back the HF residual as emphasis. */
    static constexpr double kOtaSidechainFc = 4000.0;

    // =========================================================================
    // Internal helpers (all real-time safe)
    // =========================================================================

    /**
     * Compute gain reduction in dB using the compression curve.
     *
     * The gain computer maps input level to the amount of gain reduction
     * that should be applied. With a soft knee, the compression ratio
     * gradually increases across the knee region rather than switching
     * abruptly at the threshold.
     *
     * @param inputDb     Input level in dB.
     * @param thresholdDb Compression threshold in dB.
     * @param ratio       Compression ratio (e.g., 4.0 means 4:1).
     * @param halfKnee    Half the knee width in dB.
     * @return Gain reduction in dB (positive = reduce gain).
     */
    static float computeGainReduction(float inputDb, float thresholdDb,
                                       float ratio, float halfKnee) {
        // Below the knee region: no compression
        if (inputDb <= thresholdDb - halfKnee) {
            return 0.0f;
        }

        // Above the knee region: full compression
        if (inputDb >= thresholdDb + halfKnee || halfKnee < 0.001f) {
            // Gain reduction = excess * (1 - 1/ratio)
            // where excess = inputDb - thresholdDb
            float excess = inputDb - thresholdDb;
            return excess * (1.0f - 1.0f / ratio);
        }

        // Inside the soft knee region: quadratic interpolation
        // This creates a smooth curve between no compression and full compression.
        // The formula is derived from a second-order polynomial that matches
        // the slope at both edges of the knee region.
        float kneeWidth = halfKnee * 2.0f;
        float x = inputDb - thresholdDb + halfKnee;
        return (1.0f - 1.0f / ratio) * x * x / (2.0f * kneeWidth);
    }

    /**
     * Convert decibels to linear amplitude.
     */
    static float dbToLinear(float db) {
        return std::pow(10.0f, db / 20.0f);
    }

    /**
     * Compute exponential smoothing coefficient from time in milliseconds.
     */
    float computeCoefficient(float timeMs) const {
        if (timeMs <= 0.0f || sampleRate_ <= 0) return 0.0f;
        const float timeSec = timeMs / 1000.0f;
        return std::exp(-1.0f / (timeSec * static_cast<float>(sampleRate_)));
    }

    /**
     * Clamp a value to a range. Real-time safe.
     */
    static float clamp(float value, float minVal, float maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    // =========================================================================
    // Parameters (written from UI thread, read from audio thread)
    // =========================================================================

    std::atomic<float> threshold_{-20.0f};    // dB
    std::atomic<float> ratio_{4.0f};          // ratio (4:1 default)
    std::atomic<float> attack_{5.0f};         // ms
    std::atomic<float> release_{100.0f};      // ms
    std::atomic<float> makeupGain_{0.0f};     // dB
    std::atomic<float> kneeWidth_{6.0f};      // dB
    std::atomic<float> character_{0.0f};      // 0=VCA, 1=OTA (Dyna Comp)

    // =========================================================================
    // Audio thread state
    // =========================================================================

    int32_t sampleRate_ = 44100;
    float smoothedGainReductionDb_ = 0.0f;  // Envelope follower state

    /** OTA sidechain one-pole lowpass state.
     *  Tracks the low-frequency envelope of |input| so we can derive
     *  the HF residual for treble-emphasis in the detector path.
     *  Uses double precision because the coefficient at 4kHz/48kHz is
     *  ~0.41, which is fine in float, but double avoids any precision
     *  concerns if the sample rate changes to very high values. */
    double otaSidechainState_ = 0.0;

    /** Pre-computed one-pole LP coefficient for OTA sidechain weighting.
     *  Calculated in setSampleRate() as: 1 - exp(-2*pi*4000/fs). */
    double otaSidechainCoeff_ = 0.0;

    /** OTA sidechain HPF state and coefficient (~60Hz).
     *  Removes sub-bass from envelope detector to prevent pumping. */
    double otaSidechainHPState_ = 0.0;
    double otaSidechainHPCoeff_ = 0.0;
};

#endif // GUITAR_ENGINE_COMPRESSOR_H
