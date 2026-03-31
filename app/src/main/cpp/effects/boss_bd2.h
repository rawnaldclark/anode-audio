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
 *      - Modeled as: low shelf -3dB@200Hz + Butterworth LP@1.8kHz + 12dB loss
 *
 *   4. Second discrete op-amp gain stage (Q13/Q14 + Q12):
 *      - Gain: 0-40dB via GAIN2 pot (second section of dual-gang)
 *      - Flatter frequency response than Stage 1 (100Hz-6kHz)
 *      - Clips FIRST at moderate gain (receives pre-boosted signal)
 *      - D7-D10 (two parallel 1SS133 pairs) clip to ground
 *
 *   5. Tone control:
 *      - Passive treble cut (C100=18nF, TONE pot 250KA)
 *      - Variable-cutoff LP: fc=8.8kHz (dark) to 35Hz (bright = LP drops out)
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

        // Output level with audio taper (250KA pot)
        const float outputGain = level * level;

        // Post-gyrator output scaling to normalize back to [-1, 1]
        // The gyrator boosts 120Hz by +6dB (2x), and the clipping stages
        // output signals in the ~0.5-1.0V range (normalized). Scale down
        // to prevent downstream clipping.
        constexpr float kOutputScale = 0.18f;

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

            // Inter-stage tone stack: 2-biquad cascade (Fender-style passive network)
            double xts = static_cast<double>(x);
            // Biquad 1: Low shelf -3dB at 200Hz
            double ts1Out = tsB0_1_ * xts + tsB1_1_ * tsX1_1_ + tsB2_1_ * tsX2_1_
                          - tsA1_1_ * tsY1_1_ - tsA2_1_ * tsY2_1_;
            tsX2_1_ = tsX1_1_; tsX1_1_ = xts;
            tsY2_1_ = tsY1_1_; tsY1_1_ = fast_math::denormal_guard(ts1Out);
            // Biquad 2: Butterworth LP at 1.8kHz
            double ts2Out = tsB0_2_ * ts1Out + tsB1_2_ * tsX1_2_ + tsB2_2_ * tsX2_2_
                          - tsA1_2_ * tsY1_2_ - tsA2_2_ * tsY2_2_;
            tsX2_2_ = tsX1_2_; tsX1_2_ = ts1Out;
            tsY2_2_ = tsY1_2_; tsY1_2_ = fast_math::denormal_guard(ts2Out);
            x = static_cast<float>(tsY2_2_);
            x *= 0.25f;  // -12dB insertion loss

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

            // Tone control: variable-cutoff LP (C100=18nF, TONE pot 250KA)
            // At high resistance (tone=1/bright): fc=35Hz, LP drops out of circuit
            // At low resistance (tone=0/dark): fc=8.8kHz, mild treble cut
            float R_tone = 1000.0f + tone * tone * 249000.0f;
            float fc_tone = 1.0f / (6.2831853f * R_tone * 18.0e-9f);
            fc_tone = std::max(30.0f, std::min(fc_tone, static_cast<float>(sampleRate_) * 0.49f));
            double toneCoeff = std::exp(-2.0 * M_PI * static_cast<double>(fc_tone)
                                        / static_cast<double>(sampleRate_));
            double xd = static_cast<double>(x);
            toneLpState_ = (1.0 - toneCoeff) * xd + toneCoeff * toneLpState_;
            toneLpState_ = fast_math::denormal_guard(toneLpState_);
            float toned = static_cast<float>(toneLpState_);

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

        // Inter-stage tone stack: 2-biquad cascade (Fender-style passive network)
        computeLowShelf(200.0, -3.0, 0.707, fs, tsB0_1_, tsB1_1_, tsB2_1_, tsA1_1_, tsA2_1_);
        computeButterworthLP(1800.0, fs, tsB0_2_, tsB1_2_, tsB2_2_, tsA1_2_, tsA2_2_);

        // Tone control: computed per-sample (variable cutoff)

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

        // Inter-stage tone stack biquad cascade
        tsX1_1_ = tsX2_1_ = tsY1_1_ = tsY2_1_ = 0.0;
        tsX1_2_ = tsX2_2_ = tsY1_2_ = tsY2_2_ = 0.0;

        // Tone control
        toneLpState_ = 0.0;

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
     * Circuit-accurate JFET asymmetric clipping model.
     *
     * The BD-2's discrete op-amp clips differently on each rail:
     *
     *   Positive swing (PNP output stage Q9/Q12 saturating against V+ rail):
     *     - Hard knee at ~0.3V (PNP Vce_sat)
     *     - Output headroom ~3.85V (V+ rail minus bias)
     *     - Modeled as fast_tanh with scaled drive
     *
     *   Negative swing (JFET differential pair pinching off):
     *     - Soft knee at ~0.8V (JFET pinchoff voltage Vp)
     *     - Cubic S-curve: 1.5*t - 0.5*t^3 (square-law approximation)
     *     - Output limited to 2*Vp = 1.6V
     *
     * Asymmetry ratio: 3.85/1.6 = 2.67:1 (generates strong even harmonics).
     * This asymmetry is what gives the BD-2 its tube-like warmth.
     *
     * @param x Input signal (in circuit voltage domain).
     * @return Clipped signal (range approximately [-1.6, +3.85]).
     */
    static inline float jfetClip(float x) {
        constexpr float kVp = 0.8f;
        constexpr float kHeadroom = 3.85f;
        constexpr float kPnpDrive = 1.0f / (kHeadroom * 0.4f);
        if (x >= 0.0f) {
            return kHeadroom * fast_math::fast_tanh(x * kPnpDrive);
        } else {
            float t = std::min(std::abs(x) / (2.0f * kVp), 1.0f);
            float y = 1.5f * t - 0.5f * t * t * t;
            return -(2.0f * kVp) * y;
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
     * Compute low shelf biquad coefficients (Audio EQ Cookbook).
     *
     * @param fc  Shelf frequency in Hz.
     * @param gainDb Gain in dB (negative for cut).
     * @param Q  Quality factor.
     * @param fs Sample rate in Hz.
     * @param b0,b1,b2,a1,a2 Output coefficients (normalized by a0).
     */
    static void computeLowShelf(double fc, double gainDb, double Q, double fs,
                                double& b0, double& b1, double& b2,
                                double& a1, double& a2) {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * M_PI * fc / fs;
        const double sinw = std::sin(w0);
        const double cosw = std::cos(w0);
        const double alpha = sinw / (2.0 * Q);
        const double sqrtA2alpha = 2.0 * std::sqrt(A) * alpha;

        const double a0_raw = (A + 1.0) + (A - 1.0) * cosw + sqrtA2alpha;
        const double inv_a0 = 1.0 / a0_raw;

        b0 = A * ((A + 1.0) - (A - 1.0) * cosw + sqrtA2alpha) * inv_a0;
        b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosw) * inv_a0;
        b2 = A * ((A + 1.0) - (A - 1.0) * cosw - sqrtA2alpha) * inv_a0;
        a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosw) * inv_a0;
        a2 = ((A + 1.0) + (A - 1.0) * cosw - sqrtA2alpha) * inv_a0;
    }

    /**
     * Compute 2nd-order Butterworth low-pass biquad coefficients (Q=0.7071).
     *
     * @param fc Cutoff frequency in Hz.
     * @param fs Sample rate in Hz.
     * @param b0,b1,b2,a1,a2 Output coefficients (normalized by a0).
     */
    static void computeButterworthLP(double fc, double fs,
                                     double& b0, double& b1, double& b2,
                                     double& a1, double& a2) {
        const double w0 = 2.0 * M_PI * fc / fs;
        const double sinw = std::sin(w0);
        const double cosw = std::cos(w0);
        constexpr double Q = 0.7071067811865476;  // 1/sqrt(2)
        const double alpha = sinw / (2.0 * Q);

        const double a0_raw = 1.0 + alpha;
        const double inv_a0 = 1.0 / a0_raw;

        b0 = ((1.0 - cosw) / 2.0) * inv_a0;
        b1 = (1.0 - cosw) * inv_a0;
        b2 = b0;
        a1 = (-2.0 * cosw) * inv_a0;
        a2 = (1.0 - alpha) * inv_a0;
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

    /** Tone [0, 1]. Variable LP: 0=dark (8.8kHz cutoff), 1=bright (LP drops out).
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
    // Inter-stage tone stack biquad cascade
    double tsB0_1_ = 1.0, tsB1_1_ = 0.0, tsB2_1_ = 0.0;
    double tsA1_1_ = 0.0, tsA2_1_ = 0.0;
    double tsX1_1_ = 0.0, tsX2_1_ = 0.0, tsY1_1_ = 0.0, tsY2_1_ = 0.0;

    double tsB0_2_ = 1.0, tsB1_2_ = 0.0, tsB2_2_ = 0.0;
    double tsA1_2_ = 0.0, tsA2_2_ = 0.0;
    double tsX1_2_ = 0.0, tsX2_2_ = 0.0, tsY1_2_ = 0.0, tsY2_2_ = 0.0;
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

    /** Tone control LP path */
    double toneLpState_ = 0.0;

    /** Gyrator biquad state (direct form II transposed) */
    double gyrX1_ = 0.0, gyrX2_ = 0.0;
    double gyrY1_ = 0.0, gyrY2_ = 0.0;

    /** DC blocker state */
    double dcBlockState_ = 0.0;
    double dcBlockPrev_ = 0.0;
};

#endif // GUITAR_ENGINE_BOSS_BD2_H
