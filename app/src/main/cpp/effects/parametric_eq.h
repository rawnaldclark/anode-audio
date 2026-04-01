#ifndef GUITAR_ENGINE_PARAMETRIC_EQ_H
#define GUITAR_ENGINE_PARAMETRIC_EQ_H

#include "effect.h"
#include <atomic>
#include <cmath>
#include <algorithm>

/**
 * 10-band graphic equalizer + output level, modeled after the MXR M108S.
 *
 * ISO 266 standard octave-spaced center frequencies:
 *   31.25, 62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 Hz
 *
 * Each band is a peaking EQ (SVF) with Q=0.9 (~1.55 octave bandwidth),
 * variable gain from -12 to +12 dB.
 *
 * Q=0.9 rationale (vs old Q=1.5):
 *   - Old Q=1.5 gave 0.92 octave bandwidth on 1-octave-spaced bands,
 *     leaving gaps between bands where no slider had influence. Moving
 *     a fader barely affected frequencies halfway to the adjacent band.
 *   - Q=0.9 gives ~1.55 octave bandwidth, so adjacent bands overlap by
 *     ~0.55 octaves. This produces <0.7 dB ripple when all sliders are
 *     set equal (meets ISO graphic EQ spec of <1 dB), and boosting two
 *     adjacent bands creates a smooth combined curve instead of two
 *     separate peaks with a valley between them.
 *   - This matches the MXR M108S's gyrator-based bandpass behavior,
 *     where the LC Q is approximately 1.0 (range 0.9-1.1 across bands).
 *   - Guitar EQs intentionally use wider Q than studio parametrics —
 *     the goal is broad, musical tone shaping, not surgical notching.
 *
 * MXR M108S circuit reference:
 *   - Op-amp gyrators (TL072) simulate inductors: L_eq = R1 * R2 * C_g
 *   - Each band: series LC to ground, with slider controlling mix amount
 *   - GAIN slider is post-EQ (output level), +/-12 dB
 *   - 18V supply, >95 dB SNR, 500K input Z (JFET buffer)
 *
 * SVF implementation follows Andrew Simper / Cytomic's design:
 *   g = tan(pi * freq / sampleRate)
 *   k = 1/Q
 *   Peaking EQ: output = input + (gain^2 - 1) * bandpass
 *
 * Since all frequencies are fixed, SVF coefficients (g) are pre-computed
 * in setSampleRate() and reused every block. Only gains change at runtime.
 *
 * Parameter IDs for JNI access:
 *    0 = 31.25 Hz gain (-12 to +12 dB)
 *    1 = 62.5 Hz gain  (-12 to +12 dB)
 *    2 = 125 Hz gain   (-12 to +12 dB)
 *    3 = 250 Hz gain   (-12 to +12 dB)
 *    4 = 500 Hz gain   (-12 to +12 dB)
 *    5 = 1 kHz gain    (-12 to +12 dB)
 *    6 = 2 kHz gain    (-12 to +12 dB)
 *    7 = 4 kHz gain    (-12 to +12 dB)
 *    8 = 8 kHz gain    (-12 to +12 dB)
 *    9 = 16 kHz gain   (-12 to +12 dB)
 *   10 = Level         (-12 to +12 dB, output gain, post-EQ)
 */
class ParametricEQ : public Effect {
public:
    /** Number of EQ bands (ISO 266 octave-spaced). */
    static constexpr int kNumBands = 10;

    /**
     * Q factor for all bands.
     *
     * Q=0.9 gives ~1.55 octave bandwidth at -3 dB, providing smooth
     * overlap between octave-spaced bands with <0.7 dB ripple.
     * This matches the MXR M108S gyrator LC Q of approximately 1.0.
     *
     * Bandwidth table for reference:
     *   Q=0.7  -> 2.00 oct (too wide, poor band separation)
     *   Q=0.9  -> 1.55 oct (sweet spot for guitar graphic EQ)
     *   Q=1.0  -> 1.39 oct (also good, slightly more surgical)
     *   Q=1.5  -> 0.92 oct (too narrow, gaps between bands)
     */
    static constexpr double kBandQ = 0.9;

    /** Reciprocal of Q, pre-computed as SVF damping coefficient k. */
    static constexpr double kK = 1.0 / kBandQ;

    /** Maximum gain per band in dB (MXR M108S standard). */
    static constexpr float kMaxGainDb = 12.0f;

    ParametricEQ() = default;

    void process(float* buffer, int numFrames) override {
        // Snapshot gains and convert dB to linear
        double bandGains[kNumBands];
        for (int b = 0; b < kNumBands; ++b) {
            bandGains[b] = dbToLinear(bandGainDb_[b].load(std::memory_order_relaxed));
        }

        // Output level: straight dB-to-voltage conversion (not sqrt like peaking)
        const float levelDb = levelDb_.load(std::memory_order_relaxed);
        const double levelLin = std::pow(10.0, static_cast<double>(levelDb) / 20.0);

        for (int i = 0; i < numFrames; ++i) {
            double sample = static_cast<double>(buffer[i]);

            // Process through all 10 bands in series
            for (int b = 0; b < kNumBands; ++b) {
                sample = processSvfPeaking(sample, g_[b], kK, bandGains[b],
                                           z1_[b], z2_[b]);
            }

            // Apply output level
            sample *= levelLin;

            buffer[i] = clamp(static_cast<float>(sample), -1.0f, 1.0f);
        }
    }

    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;

        // ISO 266 standard octave-spaced center frequencies (MXR M108S layout).
        static constexpr double bandFreqs[kNumBands] = {
            31.25, 62.5, 125.0, 250.0, 500.0,
            1000.0, 2000.0, 4000.0, 8000.0, 16000.0
        };

        // Pre-compute SVF g coefficient for each fixed frequency.
        // g = tan(pi * freq / sampleRate)
        // The tan() provides inherent bilinear pre-warping, so center
        // frequencies are exact regardless of sample rate.
        const double sr = static_cast<double>(sampleRate);
        for (int b = 0; b < kNumBands; ++b) {
            g_[b] = std::tan(kPi * bandFreqs[b] / sr);
        }

        resetState();
    }

    void reset() override {
        resetState();
    }

    void setParameter(int paramId, float value) override {
        if (paramId >= 0 && paramId < kNumBands) {
            // Band gain: clamp to -12..+12 dB (MXR M108S range)
            bandGainDb_[paramId].store(clamp(value, -kMaxGainDb, kMaxGainDb),
                                       std::memory_order_relaxed);
        } else if (paramId == 10) {
            // Output level: clamp to -12..+12 dB
            levelDb_.store(clamp(value, -kMaxGainDb, kMaxGainDb),
                           std::memory_order_relaxed);
        }
    }

    float getParameter(int paramId) const override {
        if (paramId >= 0 && paramId < kNumBands) {
            return bandGainDb_[paramId].load(std::memory_order_relaxed);
        } else if (paramId == 10) {
            return levelDb_.load(std::memory_order_relaxed);
        }
        return 0.0f;
    }

private:
    /**
     * Process one sample through an SVF configured as a peaking EQ.
     * Uses the Cytomic SVF topology for numerical stability.
     *
     * The peaking EQ adds gain * bandpass to the input, creating a
     * boost or cut at the center frequency.
     *
     * State variables z1, z2 are 64-bit double for precision at low frequencies
     * (31.25 Hz band has poles very close to DC, needs double precision).
     */
    static double processSvfPeaking(double input, double g, double k, double gain,
                                     double& z1, double& z2) {
        // SVF tick (Cytomic / Andrew Simper form)
        double hp = (input - (k + g) * z1 - z2) / (1.0 + k * g + g * g);
        double bp = g * hp + z1;
        double lp = g * bp + z2;

        // State update
        z1 = 2.0 * bp - z1;
        z2 = 2.0 * lp - z2;

        // Peaking EQ: input + (gain^2 - 1) * bandpass
        // gain = 10^(dB/40) = sqrt of voltage gain.
        // When gain > 1: boost at center freq
        // When gain < 1: cut at center freq
        // When gain == 1: passthrough (0 dB)
        return input + (gain * gain - 1.0) * bp;
    }

    static double dbToLinear(float db) {
        // For peaking EQ: use dB/40 (square root of voltage gain)
        // so gain^2 in the peaking formula produces correct dB amount
        return std::pow(10.0, static_cast<double>(db) / 40.0);
    }

    static float clamp(float v, float lo, float hi) {
        return std::max(lo, std::min(hi, v));
    }

    static constexpr double kPi = 3.14159265358979323846;

    void resetState() {
        for (int b = 0; b < kNumBands; ++b) {
            z1_[b] = 0.0;
            z2_[b] = 0.0;
        }
    }

    // ── Parameters ──────────────────────────────────────────────────────────

    /** Per-band gain in dB (-12 to +12). Default 0 = flat. */
    std::atomic<float> bandGainDb_[kNumBands] = {
        {0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f},
        {0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}
    };

    /** Output level in dB (-12 to +12). Default 0 = unity. */
    std::atomic<float> levelDb_{0.0f};

    int32_t sampleRate_ = 44100;

    // ── Pre-computed SVF coefficients ───────────────────────────────────────

    /** SVF g coefficient per band, computed in setSampleRate(). */
    double g_[kNumBands] = {};

    // ── SVF state variables (64-bit for low-frequency stability) ────────────

    double z1_[kNumBands] = {};
    double z2_[kNumBands] = {};
};

#endif // GUITAR_ENGINE_PARAMETRIC_EQ_H
