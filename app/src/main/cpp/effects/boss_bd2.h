#ifndef GUITAR_ENGINE_BOSS_BD2_H
#define GUITAR_ENGINE_BOSS_BD2_H

#include "effect.h"
#include "oversampler.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * Circuit-accurate emulation of the Boss BD-2 Blues Driver pedal.
 *
 * The Boss BD-2 (1995) is fundamentally different from op-amp-based overdrives.
 * Where the Tube Screamer uses an IC op-amp with diodes in the feedback loop,
 * the BD-2 uses two discrete JFET-based operational amplifiers that clip
 * gracefully against the supply rails (~0V and ~8V) -- behaving more like
 * a tube amplifier than a traditional pedal.
 *
 * Key sonic characteristics:
 *   - No mid-hump (unlike Tube Screamer's 720Hz peak)
 *   - Transparent tone: preserves guitar's natural voicing
 *   - Touch-sensitive dynamics: progressive multi-stage saturation
 *   - JFET square-law creates 2nd-harmonic-dominant distortion (tube-like)
 *   - Asymmetric clipping: positive rail is hard PNP saturation,
 *     negative rail is soft JFET pinchoff
 *
 * Circuit stages modeled (from Electrosmash BD-2 analysis):
 *
 *   1. Input buffer (Q10 2SK184-GR source follower, 1M impedance)
 *
 *   2. First discrete op-amp gain stage (Q3/Q11 diff pair + Q9 PNP output):
 *      - Gain: 0-40dB via GAIN1 pot (250KA dual-gang, first section)
 *      - C4 (150nF) + R5 (1.5K) creates HPF at ~707Hz in feedback path
 *        => bass below 707Hz gets less gain, stays clean
 *      - C5 (~100pF) rolls off treble above ~3kHz (stability + tone)
 *      - Inter-stage diodes D3/D4 (1SS133) clip to ground at ~0.55V
 *        (negligible at high gain -- rail saturation dominates)
 *
 *   3. Fixed inter-stage tone stack:
 *      - Passive Fender-style EQ (fixed Treble=0, Bass=10, Mid~6)
 *      - Compensates for Stage 1's 707Hz bass rolloff
 *      - Significant insertion loss (~10-15dB)
 *      - Modeled as: gentle LP (~3kHz) + mid-range shelving
 *
 *   4. Second discrete op-amp gain stage (Q13/Q14 + Q12):
 *      - Gain: 0-40dB via GAIN2 pot (second section of dual-gang)
 *      - Flatter frequency response than Stage 1 (100Hz-6kHz)
 *      - Clips FIRST at moderate gain (receives pre-boosted signal)
 *      - D7-D10 (two parallel 1SS133 pairs) clip to ground
 *
 *   5. Tone control:
 *      - Passive treble cut (C100=18nF, TONE pot 250KA)
 *      - Modeled as tilt EQ: 0=dark, 0.5=flat, 1=bright
 *
 *   6. Output stage (IC1a M5218AL):
 *      - Gyrator bass boost: +6dB peak at ~120Hz
 *      - L = C22*R25*R26 ~ 3H, resonates with C21 (56nF)
 *      - Restores warmth lost by Stage 1's 707Hz rolloff
 *
 *   7. Level pot (250KA): master output volume
 *
 * Anti-aliasing strategy:
 *   2x oversampling around the nonlinear clipping stages. The JFET
 *   square-law clipping is inherently smoother than diode hard clipping,
 *   generating fewer high-order harmonics, so 2x is sufficient. The
 *   pre-clipping bandwidth limiting at ~3kHz (C5) further reduces the
 *   alias-generating bandwidth.
 *
 * Parameter IDs (for JNI access):
 *   0 = Gain  (0.0 to 1.0) - Drive amount (dual-gang, both stages)
 *   1 = Tone  (0.0 to 1.0) - Tilt EQ (dark to bright)
 *   2 = Level (0.0 to 1.0) - Output volume
 *
 * References:
 *   - Electrosmash: https://www.electrosmash.com/boss-bd2-blues-driver-analysis
 *   - PedalPCB Boneyard (BD-2 schematic trace)
 *   - Analog Is Not Dead: BD-2 circuit analysis (gyrator C21/C22 = 56nF)
 *   - Keeley / Fromel / Wampler mod documentation
 */
class BossBD2 : public Effect {
public:
    BossBD2() = default;

    // =========================================================================
    // Effect interface
    // =========================================================================

    /**
     * Process audio through the BD-2 Blues Driver circuit.
     *
     * Signal flow:
     *   Input -> Stage 1 pre-emphasis HPF (707Hz)
     *         -> Stage 1 gain + JFET clip
     *         -> Inter-stage tone stack (mild LP)
     *         -> Stage 2 gain + JFET clip  (2x oversampled)
     *         -> Tone control (tilt EQ)
     *         -> Gyrator bass boost (120Hz +6dB)
     *         -> Output level
     *
     * @param buffer   Mono audio samples in [-1.0, 1.0].
     * @param numFrames Number of frames in the buffer.
     */
    void process(float* buffer, int numFrames) override {
        if (preOsBuffer_.empty()) return;

        // ------------------------------------------------------------------
        // Snapshot parameters (relaxed: atomicity only, no ordering needed)
        // ------------------------------------------------------------------
        const float gain  = gain_.load(std::memory_order_relaxed);
        const float tone  = tone_.load(std::memory_order_relaxed);
        const float level = level_.load(std::memory_order_relaxed);

        // ------------------------------------------------------------------
        // Pre-compute per-block constants
        // ------------------------------------------------------------------

        // Dual-gang pot: both stages gain simultaneously.
        // Gain pot is 250KA audio taper. In negative feedback:
        //   Stage 1: G1 = 1 + (gain * 250K) / R_ground1
        //   Stage 2: G2 = 1 + (gain * 250K) / R_ground2
        // Audio taper mapping: gain^2 for perceptual linearity
        const float gainTaper = gain * gain;

        // Stage 1: feedback path with R5=1.5K, R9=10K
        // At gain=0: G1 ~ 1 + 1.5K/10K = 1.15 (~1.2dB)
        // At gain=1: G1 ~ 1 + (250K+1.5K)/10K = 26.15 (~28dB)
        const float stage1Gain = 1.0f + gainTaper * (250.0e3f / 10.0e3f);

        // Stage 2: feedback with R10=9.1K, R11=15K
        // Flatter response, clips first due to receiving pre-boosted signal
        // At gain=0: G2 ~ 1 + 0 = 1
        // At gain=1: G2 ~ 1 + 250K/9.1K = 28.5 (~29dB)
        const float stage2Gain = 1.0f + gainTaper * (250.0e3f / 9.1e3f);

        // Input scaling: guitar signal (~0.1-0.5Vpp) into circuit operating
        // at 8V supply with 4V bias. Scale to realistic circuit voltages.
        constexpr float kInputScale = 4.0f;

        // Tone: tilt EQ crossfade. tone=0 => dark (LP dominant),
        // tone=1 => bright (HP dominant), tone=0.5 => roughly flat.
        const float toneLP = 1.0f - tone;
        const float toneHP = tone;

        // Output level with audio taper (250KA pot)
        const float outputGain = level * level;

        // Post-gyrator output scaling to normalize back to [-1, 1]
        // The gyrator boosts 120Hz by +6dB (2x), and the clipping stages
        // output signals in the ~0.5-1.0V range (normalized). Scale down
        // to prevent downstream clipping.
        constexpr float kOutputScale = 0.7f;

        // ------------------------------------------------------------------
        // Per-sample: pre-clipping filtering and Stage 1
        // ------------------------------------------------------------------
        for (int i = 0; i < numFrames; ++i) {
            float x = buffer[i];

            // Scale to circuit voltage domain
            x *= kInputScale;

            // ---- Stage 1: First discrete op-amp ----

            // HPF at 707Hz in feedback path: bass below 707Hz gets LESS gain.
            // Model as HPF on the input before amplification. This creates
            // the BD-2's signature "clean bass, crunchy treble" character.
            // y_hp[n] = coeff * (y_hp[n-1] + x[n] - x[n-1])
            double xd = static_cast<double>(x);
            double hp1Out = stage1HpCoeff_ * (stage1HpState_ + xd - stage1HpPrev_);
            stage1HpPrev_ = xd;
            stage1HpState_ = fast_math::denormal_guard(hp1Out);

            // The HP component gets full Stage 1 gain
            float xHigh = static_cast<float>(stage1HpState_) * stage1Gain;

            // The LP component (original minus HP) passes with unity gain
            // (bass stays clean, not amplified through the clipping stage)
            float xLow = x - static_cast<float>(stage1HpState_);

            // Pre-clipping treble rolloff at ~3kHz (C5, ~100pF)
            // Limits bandwidth before clipping to reduce aliasing
            double xhd = static_cast<double>(xHigh);
            stage1LpState_ = (1.0 - stage1LpCoeff_) * xhd
                           + stage1LpCoeff_ * stage1LpState_;
            stage1LpState_ = fast_math::denormal_guard(stage1LpState_);
            xHigh = static_cast<float>(stage1LpState_);

            // JFET asymmetric soft clipping (rail saturation)
            // Positive: PNP output transistor saturates against V+ rail (hard)
            // Negative: JFET diff pair pinches off (soft, square-law)
            xHigh = jfetClip(xHigh);

            // Recombine: clipped highs + clean bass
            x = xHigh + xLow;

            // ---- Inter-stage tone stack (passive Fender-style) ----
            // Modeled as gentle LP at ~3kHz with ~10dB insertion loss.
            // The real circuit is a complex passive network; we approximate
            // its net effect: treble reduction + level drop.
            double xts = static_cast<double>(x);
            toneStackLpState_ = (1.0 - toneStackLpCoeff_) * xts
                              + toneStackLpCoeff_ * toneStackLpState_;
            toneStackLpState_ = fast_math::denormal_guard(toneStackLpState_);
            x = static_cast<float>(toneStackLpState_);

            // Insertion loss of passive tone stack (~10dB = 0.316x)
            x *= 0.316f;

            // Store pre-oversampled signal for Stage 2
            preOsBuffer_[i] = x;
        }

        // ------------------------------------------------------------------
        // Stage 2: Second discrete op-amp with 2x oversampling
        // ------------------------------------------------------------------
        const int oversampledLen = oversampler_.getOversampledSize(numFrames);
        float* upsampled = oversampler_.upsample(preOsBuffer_.data(), numFrames);

        for (int i = 0; i < oversampledLen; ++i) {
            float x = upsampled[i];

            // Stage 2 gain (flat response, 100Hz-6kHz)
            x *= stage2Gain;

            // JFET asymmetric soft clipping (same topology as Stage 1)
            // This stage clips FIRST at moderate gain because it receives
            // the pre-boosted, tone-shaped signal from Stage 1.
            x = jfetClip(x);

            upsampled[i] = x;
        }

        // Downsample back to original rate
        oversampler_.downsample(upsampled, numFrames, postOsBuffer_.data());

        // ------------------------------------------------------------------
        // Post-clipping: Tone control + Gyrator + Output level
        // ------------------------------------------------------------------
        for (int i = 0; i < numFrames; ++i) {
            float x = postOsBuffer_[i];

            // ---- Tone control (passive treble cut, C100=18nF) ----
            // Low-pass path
            double xd = static_cast<double>(x);
            toneLpState_ = (1.0 - toneLpCoeff_) * xd
                         + toneLpCoeff_ * toneLpState_;
            toneLpState_ = fast_math::denormal_guard(toneLpState_);

            // High-pass path
            double toneHpOut = toneHpCoeff_ * (toneHpState_ + xd - toneHpPrev_);
            toneHpPrev_ = xd;
            toneHpState_ = fast_math::denormal_guard(toneHpOut);

            // Tilt EQ crossfade
            float toned = static_cast<float>(
                toneLP * toneLpState_ + toneHP * toneHpState_);

            // ---- Gyrator bass boost (+6dB at ~120Hz) ----
            // Modeled as a 2nd-order biquad peaking filter.
            // This is the BD-2's signature low-end warmth that compensates
            // for Stage 1's 707Hz bass rolloff.
            double td = static_cast<double>(toned);
            double gyOut = gyrB0_ * td + gyrB1_ * gyrX1_ + gyrB2_ * gyrX2_
                         - gyrA1_ * gyrY1_ - gyrA2_ * gyrY2_;
            gyrX2_ = gyrX1_;
            gyrX1_ = td;
            gyrY2_ = gyrY1_;
            gyrY1_ = fast_math::denormal_guard(gyOut);
            toned = static_cast<float>(gyrY1_);

            // ---- DC blocker (remove any offset from asymmetric clipping) ----
            double dcIn = static_cast<double>(toned);
            double dcOut = dcBlockCoeff_ * (dcBlockState_ + dcIn - dcBlockPrev_);
            dcBlockPrev_ = dcIn;
            dcBlockState_ = fast_math::denormal_guard(dcOut);
            toned = static_cast<float>(dcBlockState_);

            // ---- Output level + scaling ----
            toned *= outputGain * kOutputScale;
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

        // Pre-allocate buffers
        const int maxFrames = 2048;
        preOsBuffer_.resize(maxFrames, 0.0f);
        postOsBuffer_.resize(maxFrames, 0.0f);

        // Prepare 2x oversampler
        oversampler_.prepare(maxFrames);

        // Pre-compute fixed filter coefficients
        const double fs = static_cast<double>(sampleRate);

        // Stage 1 HPF: 707Hz (C4=150nF, R5=1.5K in feedback)
        stage1HpCoeff_ = computeOnePoleHPCoeff(707.0, fs);

        // Stage 1 LPF: 3kHz (C5 ~100pF treble rolloff)
        stage1LpCoeff_ = computeOnePoleLPCoeff(3000.0, fs);

        // Inter-stage tone stack: gentle LP at ~3kHz
        toneStackLpCoeff_ = computeOnePoleLPCoeff(3000.0, fs);

        // Tone control LP path: ~1kHz
        toneLpCoeff_ = computeOnePoleLPCoeff(1000.0, fs);

        // Tone control HP path: ~1kHz
        toneHpCoeff_ = computeOnePoleHPCoeff(1000.0, fs);

        // DC blocker at 10Hz (sample-rate adaptive)
        dcBlockCoeff_ = computeOnePoleHPCoeff(10.0, fs);

        // Gyrator biquad: peaking filter at 120Hz, +6dB, Q~1.5
        computeGyrator(120.0, 6.0, 1.5, fs);
    }

    /**
     * Reset all internal filter state.
     * Called when audio stream restarts or effect is re-enabled.
     */
    void reset() override {
        oversampler_.reset();

        // Stage 1 filter states
        stage1HpState_ = 0.0;
        stage1HpPrev_ = 0.0;
        stage1LpState_ = 0.0;

        // Inter-stage tone stack
        toneStackLpState_ = 0.0;

        // Tone control
        toneLpState_ = 0.0;
        toneHpState_ = 0.0;
        toneHpPrev_ = 0.0;

        // Gyrator biquad
        gyrX1_ = gyrX2_ = gyrY1_ = gyrY2_ = 0.0;

        // DC blocker
        dcBlockState_ = 0.0;
        dcBlockPrev_ = 0.0;
    }

    // =========================================================================
    // Parameter access (called from UI thread via JNI)
    // =========================================================================

    /**
     * Set a parameter by ID. Thread-safe via atomic storage.
     *
     * @param paramId  0=Gain, 1=Tone, 2=Level
     * @param value    Parameter value in [0.0, 1.0].
     */
    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0:
                gain_.store(clamp(value, 0.0f, 1.0f),
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
     * @param paramId  0=Gain, 1=Tone, 2=Level
     * @return Current parameter value, or 0.0f if paramId is unknown.
     */
    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return gain_.load(std::memory_order_relaxed);
            case 1: return tone_.load(std::memory_order_relaxed);
            case 2: return level_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    // =========================================================================
    // JFET Asymmetric Soft Clipping
    // =========================================================================

    /**
     * Model the asymmetric clipping behavior of the BD-2's discrete op-amp.
     *
     * The discrete op-amp clips differently on positive and negative swings:
     *
     *   Positive swing (PNP output stage Q9/Q12 saturating against V+ rail):
     *     - Harder clipping (PNP collector-emitter saturation is abrupt)
     *     - Modeled with fast_tanh at higher drive (1.2x) for sharper knee
     *
     *   Negative swing (JFET differential pair pinching off):
     *     - Softer clipping (JFET square-law: Id = Idss*(1-Vgs/Vp)^2)
     *     - More gradual, creates 2nd-harmonic-dominant distortion
     *     - Modeled with fast_tanh at lower drive (0.85x) + slight gain (1.05x)
     *       to model the asymmetry that generates even harmonics
     *
     * The asymmetry between positive and negative half-cycles is what gives
     * the BD-2 its tube-like warmth (even harmonics) and touch sensitivity.
     *
     * @param x Input signal (in circuit voltage domain).
     * @return Clipped signal.
     */
    static inline float jfetClip(float x) {
        if (x >= 0.0f) {
            // PNP rail saturation: harder clip
            return fast_math::fast_tanh(x * 1.2f);
        } else {
            // JFET pinchoff: softer clip with asymmetric gain
            return fast_math::fast_tanh(x * 0.85f) * 1.05f;
        }
    }

    // =========================================================================
    // Filter Coefficient Computation
    // =========================================================================

    /**
     * Compute one-pole low-pass coefficient (double precision).
     * a = exp(-2*pi*fc/fs)
     * For: y[n] = (1-a)*x[n] + a*y[n-1]
     */
    static double computeOnePoleLPCoeff(double cutoffHz, double fs) {
        if (fs <= 0.0) return 0.0;
        cutoffHz = std::max(20.0, std::min(cutoffHz, fs * 0.49));
        return std::exp(-2.0 * M_PI * cutoffHz / fs);
    }

    /**
     * Compute one-pole high-pass coefficient (double precision).
     * a = 1 / (1 + 2*pi*fc/fs)
     * For: y[n] = a * (y[n-1] + x[n] - x[n-1])
     */
    static double computeOnePoleHPCoeff(double cutoffHz, double fs) {
        if (fs <= 0.0) return 0.99;
        cutoffHz = std::max(1.0, std::min(cutoffHz, fs * 0.49));
        const double wc = 2.0 * M_PI * cutoffHz;
        return 1.0 / (1.0 + wc / fs);
    }

    /**
     * Compute gyrator peaking biquad coefficients.
     *
     * The BD-2 output stage uses a gyrator (R25, R26, C21, C22) to create
     * a virtual inductor (~3H) that resonates with C21 (56nF) at ~120Hz.
     * In the IC1a feedback loop, this creates a +6dB bass peak.
     *
     * Standard audio EQ cookbook peaking filter:
     *   H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
     *
     * @param fc Center frequency in Hz.
     * @param gainDb Boost in dB.
     * @param Q Quality factor.
     * @param fs Sample rate in Hz.
     */
    void computeGyrator(double fc, double gainDb, double Q, double fs) {
        const double A = std::pow(10.0, gainDb / 40.0);  // amplitude
        const double w0 = 2.0 * M_PI * fc / fs;
        const double sinw = std::sin(w0);
        const double cosw = std::cos(w0);
        const double alpha = sinw / (2.0 * Q);

        const double b0 = 1.0 + alpha * A;
        const double b1 = -2.0 * cosw;
        const double b2 = 1.0 - alpha * A;
        const double a0 = 1.0 + alpha / A;
        const double a1 = -2.0 * cosw;
        const double a2 = 1.0 - alpha / A;

        // Normalize by a0
        const double inv_a0 = 1.0 / a0;
        gyrB0_ = b0 * inv_a0;
        gyrB1_ = b1 * inv_a0;
        gyrB2_ = b2 * inv_a0;
        gyrA1_ = a1 * inv_a0;
        gyrA2_ = a2 * inv_a0;
    }

    /**
     * Clamp a value to [minVal, maxVal].
     */
    static float clamp(float value, float minVal, float maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    // =========================================================================
    // Parameters (UI thread writes, audio thread reads)
    // =========================================================================

    /** Gain [0, 1]. Dual-gang pot controlling both discrete op-amp stages.
     *  Default 0.5 gives moderate blues crunch. */
    std::atomic<float> gain_{0.5f};

    /** Tone [0, 1]. Tilt EQ: 0=dark, 0.5=flat, 1=bright.
     *  Default 0.5 is the neutral "noon" position. */
    std::atomic<float> tone_{0.5f};

    /** Level [0, 1]. Output volume (250KA audio taper pot).
     *  Default 0.5 provides moderate output. */
    std::atomic<float> level_{0.5f};

    // =========================================================================
    // Audio thread state
    // =========================================================================

    /** Device sample rate in Hz */
    int32_t sampleRate_ = 44100;

    /** 2x oversampler for Stage 2 nonlinear processing */
    Oversampler<2> oversampler_;

    // ---- Pre-allocated buffers ----
    std::vector<float> preOsBuffer_;
    std::vector<float> postOsBuffer_;

    // ---- Pre-computed filter coefficients (set in setSampleRate()) ----

    double stage1HpCoeff_ = 0.9;     ///< Stage 1 HPF (707Hz, C4/R5 in feedback)
    double stage1LpCoeff_ = 0.0;     ///< Stage 1 LPF (3kHz, C5 treble rolloff)
    double toneStackLpCoeff_ = 0.0;  ///< Inter-stage tone stack (~3kHz LP)
    double toneLpCoeff_ = 0.0;       ///< Tone control LP path (~1kHz)
    double toneHpCoeff_ = 0.99;      ///< Tone control HP path (~1kHz)
    double dcBlockCoeff_ = 0.999;    ///< DC blocker (10Hz)

    // Gyrator biquad coefficients (peaking +6dB at 120Hz)
    double gyrB0_ = 1.0, gyrB1_ = 0.0, gyrB2_ = 0.0;
    double gyrA1_ = 0.0, gyrA2_ = 0.0;

    // ---- One-pole filter states (double precision for low-freq stability) ----

    /** Stage 1 high-pass (707Hz) */
    double stage1HpState_ = 0.0;
    double stage1HpPrev_ = 0.0;

    /** Stage 1 low-pass (3kHz treble limit) */
    double stage1LpState_ = 0.0;

    /** Inter-stage tone stack LP */
    double toneStackLpState_ = 0.0;

    /** Tone control LP path */
    double toneLpState_ = 0.0;

    /** Tone control HP path */
    double toneHpState_ = 0.0;
    double toneHpPrev_ = 0.0;

    /** Gyrator biquad state (direct form II transposed) */
    double gyrX1_ = 0.0, gyrX2_ = 0.0;
    double gyrY1_ = 0.0, gyrY2_ = 0.0;

    /** DC blocker state */
    double dcBlockState_ = 0.0;
    double dcBlockPrev_ = 0.0;
};

#endif // GUITAR_ENGINE_BOSS_BD2_H
