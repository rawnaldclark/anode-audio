#ifndef GUITAR_ENGINE_TUBE_SCREAMER_H
#define GUITAR_ENGINE_TUBE_SCREAMER_H

#include "effect.h"
#include "oversampler.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * Circuit-accurate emulation of the Ibanez TS-808 Tube Screamer (1979)
 * with TS9 mode switch.
 *
 * The Tube Screamer is the most cloned guitar pedal in history. Its signature
 * sound comes from three key design choices:
 *   1. Diodes in the FEEDBACK loop (not to ground) -- creates soft, compressive
 *      clipping where gain smoothly decreases as signal increases
 *   2. 47nF cap in feedback path -- rolls off bass gain below 720Hz, creating
 *      the famous "mid-hump" (bass stays clean, mids/treble get clipped)
 *   3. Relatively low gain ceiling (~40dB) -- the TS is an overdrive, not
 *      a distortion; it's designed to push a tube amp into breakup
 *
 * Circuit stages (from Electrosmash / R.G. Keen GEOfex analysis):
 *
 *   1. Input buffer: Q1 2SC1815GR emitter follower (510K input impedance)
 *      - Isolates guitar pickup from clipping circuit
 *      - 1uF coupling cap to clipping stage
 *
 *   2. Clipping amplifier (the heart):
 *      - Op-amp: JRC4558D (TS-808) in non-inverting configuration
 *      - Feedback network: R4=4.7K fixed + VR1=500K Drive pot
 *      - C3=47nF in feedback: HPF at 720Hz (bass below this gets unity gain)
 *        f = 1/(2*pi*4700*47e-9) = 720.8 Hz
 *      - C4=51pF across feedback: treble rolloff (5.66kHz at max drive)
 *      - D3/D4: two 1N914 silicon diodes anti-parallel IN the feedback loop
 *        Is=2.52nA, n=1.752, Vf~0.55V
 *      - Gain: 1 + (4700 + R_drive) / 4700
 *        Min drive: 2.0x (6dB), Max drive: 108.4x (40.7dB)
 *
 *   3. Tone control:
 *      - Mixing topology: LP-filtered path blends with unfiltered path
 *      - C5=220nF, R_tone_fixed=1K+220R, VR2=20K Tone pot
 *      - Tone=0 (dark): mostly LP filtered, fc~590Hz
 *      - Tone=10 (bright): mostly unfiltered (LP drops out of circuit)
 *
 *   4. Output buffer + volume: IC1b unity buffer, VR3=100K Level pot
 *
 *   5. Output network (TS-808 vs TS9 -- the ONLY circuit difference):
 *      - TS-808: 100R series + 10K shunt (output Z = 133 ohm)
 *      - TS9:    470R series + 100K shunt (output Z = 503 ohm)
 *
 * DSP approach: Yeh Decomposition (David Yeh, Stanford CCRMA 2009)
 *   Decomposes the implicit diode-in-feedback loop into sequential stages:
 *   HPF @ 720Hz -> freq-dependent gain -> Newton-Raphson diode clipper ->
 *   treble rolloff -> tone control -> output
 *
 * Parameter IDs:
 *   0 = Drive (0.0-1.0) -> maps to 0-500K pot (audio taper)
 *   1 = Tone  (0.0-1.0) -> LP/dry crossfade (dark to bright)
 *   2 = Level (0.0-1.0) -> output volume (100K pot, audio taper)
 *   3 = Mode  (0.0 or 1.0) -> 0=TS-808, 1=TS9
 *
 * References:
 *   - Electrosmash: https://www.electrosmash.com/tube-screamer-analysis
 *   - R.G. Keen / GEOfex: http://www.geofex.com/article_folders/tstech/tsxtech.htm
 *   - David Yeh PhD Thesis (CCRMA Stanford, 2009)
 *   - Analogman: https://www.analogman.com/tshist.htm
 */
class TubeScreamer : public Effect {
public:
    TubeScreamer() = default;

    void process(float* buffer, int numFrames) override {
        if (preOsBuffer_.empty()) return;

        // Snapshot parameters
        const float drive = drive_.load(std::memory_order_relaxed);
        const float tone  = tone_.load(std::memory_order_relaxed);
        const float level = level_.load(std::memory_order_relaxed);
        const float mode  = mode_.load(std::memory_order_relaxed);

        // ----------------------------------------------------------------
        // Pre-compute per-block constants
        // ----------------------------------------------------------------

        // Drive pot: 500K audio taper (quadratic mapping for perceptual linearity)
        const float driveTaper = drive * drive;
        const float R_drive = driveTaper * 500000.0f;  // 0 to 500K

        // Frequency-dependent gain above 720Hz:
        //   G = 1 + (R4 + R_drive) / R4 = 1 + (4700 + R_drive) / 4700
        const float gain = 1.0f + (4700.0f + R_drive) / 4700.0f;

        // Treble rolloff from 51pF cap across feedback:
        //   fc = 1 / (2*pi * (R4 + R_drive) * 51e-12)
        // At max drive (R_drive=500K): fc = 5.66kHz
        // At min drive (R_drive=0):    fc = 61.2kHz (inaudible)
        const float R_feedback = 4700.0f + R_drive;
        const float fc_treble = 1.0f / (6.2831853f * R_feedback * 51.0e-12f);
        const double trebleLpCoeff = computeOnePoleLPCoeff(
            static_cast<double>(fc_treble), static_cast<double>(osSampleRate_));

        // Tone control: mixing topology
        // tone=0 (dark): output = mostly LP-filtered signal
        // tone=1 (bright): output = mostly unfiltered signal
        // The LP filter cutoff itself varies with the pot, but the primary
        // audible effect is the wet/dry crossfade between filtered and unfiltered
        const float toneDry = tone;           // unfiltered (bright) path weight
        const float toneWet = 1.0f - tone;    // LP-filtered (dark) path weight

        // Tone LP: variable cutoff based on pot position
        //   R_tone = 1220 + tone * 20000 (1.22K to 21.22K)
        //   fc = 1 / (2*pi * R_tone * 220e-9)
        const float R_tone = 1220.0f + tone * 20000.0f;
        const float fc_tone = 1.0f / (6.2831853f * R_tone * 220.0e-9f);
        const double toneLpCoeff = computeOnePoleLPCoeff(
            static_cast<double>(fc_tone), static_cast<double>(sampleRate_));

        // Output level with audio taper
        const float outputGain = level * level;

        // Output network gain difference (TS-808 vs TS9)
        // TS-808: 10K/(10K+100R) = 0.990 (negligible loss)
        // TS9:    100K/(100K+470R) = 0.995 (negligible loss)
        // The real difference is in output impedance affecting cable
        // capacitance interaction. For direct monitoring (no cable sim),
        // the gain difference is <0.5dB. We model a subtle treble rolloff
        // for TS9 mode to approximate the higher output impedance.
        const bool isTS9 = mode >= 0.5f;

        // ----------------------------------------------------------------
        // Stage A + B: Input HPF (720Hz mid-hump) + frequency-dependent gain
        // ----------------------------------------------------------------
        for (int i = 0; i < numFrames; ++i) {
            float x = buffer[i];

            // Scale to circuit voltage domain (guitar ~100-500mVpp into 9V/4.5V bias)
            x *= 4.0f;

            // HPF at 720Hz: bass below this gets unity gain (stays clean),
            // treble above gets full clipping gain. This IS the mid-hump.
            double xd = static_cast<double>(x);
            double hpOut = hpCoeff_ * (hpState_ + xd - hpPrev_);
            hpPrev_ = xd;
            hpState_ = fast_math::denormal_guard(hpOut);

            // High-frequency component gets gain
            float xHigh = static_cast<float>(hpState_) * gain;

            // Low-frequency component passes at unity (clean bass)
            float xLow = x - static_cast<float>(hpState_);

            // Recombine before clipping (bass + gained treble)
            preOsBuffer_[i] = xHigh + xLow;
        }

        // ----------------------------------------------------------------
        // Stage C: Newton-Raphson diode clipper (2x oversampled)
        // ----------------------------------------------------------------
        // R_feedback for diode equation: R4 + R_drive (drive-dependent)
        const float R_fb = 4700.0f + R_drive;

        const int oversampledLen = oversampler_.getOversampledSize(numFrames);
        float* upsampled = oversampler_.upsample(preOsBuffer_.data(), numFrames);

        for (int i = 0; i < oversampledLen; ++i) {
            float x = upsampled[i];

            // Treble rolloff from 51pF cap (pre-clipping bandwidth limiting)
            // Applied at oversampled rate for accuracy
            double xd = static_cast<double>(x);
            trebleLpState_ = (1.0 - trebleLpCoeff) * xd
                           + trebleLpCoeff * trebleLpState_;
            trebleLpState_ = fast_math::denormal_guard(trebleLpState_);
            x = static_cast<float>(trebleLpState_);

            // Newton-Raphson diode clipper
            // Solves: f(Vout) = Vin - Vout - R * 2*Is*sinh(Vout/(n*Vt)) = 0
            // where R = R_feedback (affects clipping softness), Is=2.52nA, n=1.752
            //
            // The feedback topology means the diodes progressively reduce gain
            // as signal increases -- this is SOFT, COMPRESSIVE clipping, not
            // hard clipping. The gain equation becomes:
            //   G_effective = G / (1 + G * I_diode * R_load / V_signal)
            //
            // Newton-Raphson converges in 3-4 iterations with warm start.
            float Vin = x;
            float Vout = diodeState_;  // warm start from previous sample

            for (int iter = 0; iter < 4; ++iter) {
                // sinh(Vout / (n*Vt)) and cosh(Vout / (n*Vt))
                float v_nVt = Vout * kInvNVt_;
                float exp_pos = std::exp(v_nVt);
                float exp_neg = 1.0f / exp_pos;  // exp(-v_nVt)
                float sinh_v = 0.5f * (exp_pos - exp_neg);
                float cosh_v = 0.5f * (exp_pos + exp_neg);

                // f(Vout) = Vin - Vout - R * 2*Is * sinh(Vout/(n*Vt))
                float f = Vin - Vout - R_fb * kTwoIs_ * sinh_v;

                // f'(Vout) = -1 - R_fb * 2*Is/(n*Vt) * cosh(Vout/(n*Vt))
                float fp = -1.0f - R_fb * kTwoIsOverNVt_ * cosh_v;

                // Newton step
                Vout -= f / fp;
            }

            diodeState_ = Vout;  // save for warm start
            upsampled[i] = Vout;
        }

        // Downsample back to original rate
        oversampler_.downsample(upsampled, numFrames, postOsBuffer_.data());

        // ----------------------------------------------------------------
        // Stage D: Tone control + Stage E: Output
        // ----------------------------------------------------------------
        for (int i = 0; i < numFrames; ++i) {
            float x = postOsBuffer_[i];

            // Tone control: LP filter for dark path
            double xd = static_cast<double>(x);
            toneLpState_ = (1.0 - toneLpCoeff) * xd + toneLpCoeff * toneLpState_;
            toneLpState_ = fast_math::denormal_guard(toneLpState_);

            // Mix filtered (dark) and unfiltered (bright) paths
            float toned = toneDry * x + toneWet * static_cast<float>(toneLpState_);

            // TS9 mode: subtle treble rolloff from higher output impedance
            // (~503 ohm output into cable capacitance ~1nF/m)
            if (isTS9) {
                double td = static_cast<double>(toned);
                ts9LpState_ = (1.0 - ts9LpCoeff_) * td + ts9LpCoeff_ * ts9LpState_;
                ts9LpState_ = fast_math::denormal_guard(ts9LpState_);
                toned = static_cast<float>(ts9LpState_);
            }

            // DC blocker (remove offset from asymmetric diode conduction)
            double dcIn = static_cast<double>(toned);
            double dcOut = dcBlockCoeff_ * (dcBlockState_ + dcIn - dcBlockPrev_);
            dcBlockPrev_ = dcIn;
            dcBlockState_ = fast_math::denormal_guard(dcOut);
            toned = static_cast<float>(dcBlockState_);

            // Output level + normalization back to [-1, 1]
            toned *= outputGain * 0.5f;  // 0.5 = output scaling factor
            buffer[i] = clamp(toned, -1.0f, 1.0f);
        }
    }

    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;
        osSampleRate_ = sampleRate * 2;  // 2x oversampled rate

        const int maxFrames = 2048;
        preOsBuffer_.resize(maxFrames, 0.0f);
        postOsBuffer_.resize(maxFrames, 0.0f);
        oversampler_.prepare(maxFrames);

        const double fs = static_cast<double>(sampleRate);

        // HPF at 720Hz (C3=47nF, R4=4.7K in feedback)
        hpCoeff_ = computeOnePoleHPCoeff(720.8, fs);

        // DC blocker at 10Hz
        dcBlockCoeff_ = computeOnePoleHPCoeff(10.0, fs);

        // TS9 output LP: model ~12kHz rolloff from 503 ohm output Z + cable cap
        ts9LpCoeff_ = computeOnePoleLPCoeff(12000.0, fs);

        // Diode parameters (1N914: Is=2.52nA, n=1.752)
        // Pre-compute constants used in Newton-Raphson inner loop
        constexpr float Is = 2.52e-9f;
        constexpr float n = 1.752f;
        constexpr float Vt = 0.02585f;  // thermal voltage at 25C
        kInvNVt_ = 1.0f / (n * Vt);    // 1/(n*Vt) = 22.08
        kTwoIs_ = 2.0f * Is;            // 5.04e-9
        // For the Jacobian: 2*Is/(n*Vt)
        kTwoIsOverNVt_ = kTwoIs_ * kInvNVt_;  // 1.113e-7
    }

    void reset() override {
        oversampler_.reset();
        hpState_ = 0.0;
        hpPrev_ = 0.0;
        trebleLpState_ = 0.0;
        toneLpState_ = 0.0;
        ts9LpState_ = 0.0;
        dcBlockState_ = 0.0;
        dcBlockPrev_ = 0.0;
        diodeState_ = 0.0f;
    }

    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0: drive_.store(clamp(value, 0.0f, 1.0f), std::memory_order_relaxed); break;
            case 1: tone_.store(clamp(value, 0.0f, 1.0f), std::memory_order_relaxed); break;
            case 2: level_.store(clamp(value, 0.0f, 1.0f), std::memory_order_relaxed); break;
            case 3: mode_.store(value >= 0.5f ? 1.0f : 0.0f, std::memory_order_relaxed); break;
            default: break;
        }
    }

    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return drive_.load(std::memory_order_relaxed);
            case 1: return tone_.load(std::memory_order_relaxed);
            case 2: return level_.load(std::memory_order_relaxed);
            case 3: return mode_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    // -- Filter coefficient computation --------------------------------------

    static double computeOnePoleLPCoeff(double cutoffHz, double fs) {
        if (fs <= 0.0) return 0.0;
        cutoffHz = std::max(20.0, std::min(cutoffHz, fs * 0.49));
        return std::exp(-2.0 * M_PI * cutoffHz / fs);
    }

    static double computeOnePoleHPCoeff(double cutoffHz, double fs) {
        if (fs <= 0.0) return 0.99;
        cutoffHz = std::max(1.0, std::min(cutoffHz, fs * 0.49));
        const double wc = 2.0 * M_PI * cutoffHz;
        return 1.0 / (1.0 + wc / fs);
    }

    static float clamp(float value, float minVal, float maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    // -- Parameters ----------------------------------------------------------

    std::atomic<float> drive_{0.5f};
    std::atomic<float> tone_{0.5f};
    std::atomic<float> level_{0.5f};
    std::atomic<float> mode_{0.0f};  // 0=TS-808, 1=TS9

    // -- Audio thread state --------------------------------------------------

    int32_t sampleRate_ = 44100;
    int32_t osSampleRate_ = 88200;
    Oversampler<2> oversampler_;

    std::vector<float> preOsBuffer_;
    std::vector<float> postOsBuffer_;

    // Pre-computed filter coefficients
    double hpCoeff_ = 0.9;        // 720Hz HPF (mid-hump)
    double dcBlockCoeff_ = 0.999; // 10Hz DC blocker
    double ts9LpCoeff_ = 0.0;    // TS9 output treble rolloff

    // Newton-Raphson diode constants (pre-computed in setSampleRate)
    float kInvNVt_ = 22.08f;         // 1/(n*Vt)
    float kTwoIs_ = 5.04e-9f;        // 2*Is
    float kTwoIsOverNVt_ = 1.113e-7f; // 2*Is/(n*Vt)

    // Filter states (64-bit double for low-frequency stability)
    double hpState_ = 0.0;
    double hpPrev_ = 0.0;
    double trebleLpState_ = 0.0;
    double toneLpState_ = 0.0;
    double ts9LpState_ = 0.0;
    double dcBlockState_ = 0.0;
    double dcBlockPrev_ = 0.0;

    // Newton-Raphson warm start state
    float diodeState_ = 0.0f;
};

#endif // GUITAR_ENGINE_TUBE_SCREAMER_H
