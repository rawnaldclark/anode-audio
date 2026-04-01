#ifndef GUITAR_ENGINE_CHORUS_H
#define GUITAR_ENGINE_CHORUS_H

#include "effect.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * Multi-voice chorus effect with Boss CE-1 Chorus Ensemble mode.
 *
 * Two modes of operation selectable via paramId 2:
 *
 * Mode 0 (Standard): Three voices with staggered LFO phases (0, 120, 240
 *   degrees). Each voice reads from a shared circular delay buffer at a
 *   position modulated by a sine LFO. The delay center is ~7ms with the
 *   LFO sweeping +/- the depth setting.
 *
 * Mode 1 (CE-1 Chorus Ensemble): Emulates the Boss CE-1 (1976), the first
 *   chorus pedal ever made, adapted from the Roland JC-120 amp's built-in
 *   chorus. This is THE chorus sound of the RHCP "Under the Bridge" era
 *   (John Frusciante). The CE-1's character comes from:
 *
 *     1. BBD delay line (MN3002, 1024 stages) with bandwidth limiting at
 *        ~13kHz due to the clock rate. This gives a warmer, slightly
 *        duller modulated signal than a pristine digital delay.
 *     2. Single voice modulated delay (NOT multi-voice like modern chorus).
 *        This produces a simpler, more natural pitch modulation without
 *        the "shimmering" of 3-voice designs.
 *     3. Triangle LFO from a Schmitt trigger-integrator circuit. The
 *        triangle wave spends equal time at all sweep positions (linear
 *        ramp), unlike a sine which lingers at the extremes.
 *     4. ~5ms center delay with ±2ms sweep range (conservative depth).
 *     5. Subtle warmth from the BBD's charge-transfer losses, modeled
 *        here as a gentle LP filter on the wet path.
 *
 * Signal flow (CE-1 mode):
 *   Input -> BBD bandwidth LP (13kHz) -> Delay buffer -> Triangle mod
 *         -> Linear interp read -> Output (wet/dry via SignalChain)
 *
 * Signal flow (Standard mode):
 *   Input -> Delay buffer -> 3x sine-modulated reads -> Average
 *         -> Output (wet/dry via SignalChain)
 *
 * Parameter IDs for JNI access:
 *   0 = rate (0.1 to 10.0 Hz) - LFO speed
 *   1 = depth (0.0 to 1.0) - modulation depth
 *   2 = mode (0.0 = Standard, 1.0 = CE-1 Chorus Ensemble)
 */
class Chorus : public Effect {
public:
    Chorus() = default;

    void process(float* buffer, int numFrames) override {
        const float rate = rate_.load(std::memory_order_relaxed);
        const float depth = depth_.load(std::memory_order_relaxed);
        const float modeVal = mode_.load(std::memory_order_relaxed);
        const bool ce1Mode = (modeVal >= 0.5f);

        // Phase increment per sample for the LFO
        const float phaseInc = rate / static_cast<float>(sampleRate_);

        if (ce1Mode) {
            // =============================================================
            // Boss CE-1 Chorus Ensemble Mode
            // =============================================================
            //
            // The CE-1 uses a single MN3002 BBD (Bucket Brigade Device)
            // delay line with ~1024 stages. The clock rate determines
            // both the delay time and the bandwidth: higher clock = shorter
            // delay but wider bandwidth. At the CE-1's typical operating
            // point, the BBD bandwidth is about 13kHz. Signals above this
            // frequency alias in the BBD, so the input is LP filtered.
            //
            // Center delay: ~5ms (shorter than standard chorus)
            // Mod swing: 0 to ±2ms (controlled by depth knob)
            // LFO: triangle wave (Schmitt trigger integrator circuit)

            const float depthSamples = depth * 0.002f
                                       * static_cast<float>(sampleRate_);
            const float centerDelay = 0.005f
                                      * static_cast<float>(sampleRate_);

            for (int i = 0; i < numFrames; ++i) {
                float input = buffer[i];

                // BBD anti-aliasing LP filter (~13kHz).
                // The real CE-1 has a 2-pole Sallen-Key LP before the BBD
                // (MN3002 512-stage). Two cascaded one-pole LPs approximate
                // the 12dB/oct rolloff for warm BBD bandwidth limiting.
                bbdLPState_ += static_cast<float>(bbdLPCoeff_)
                               * (input - bbdLPState_);
                bbdLPState_ = fast_math::denormal_guard(bbdLPState_);
                bbdLPState2_ += static_cast<float>(bbdLPCoeff_)
                                * (bbdLPState_ - bbdLPState2_);
                bbdLPState2_ = fast_math::denormal_guard(bbdLPState2_);

                // Write 2-pole bandwidth-limited signal to delay buffer
                delayBuffer_[writePos_] = bbdLPState2_;

                // Triangle LFO: CE-1 uses a Schmitt trigger + integrator
                // circuit that produces a linear triangle wave. This creates
                // more even modulation than a sine (which lingers at extremes).
                // Triangle: 0→1 in first half, 1→0 in second half.
                float tri01;
                if (lfoPhases_[0] < 0.5f) {
                    tri01 = lfoPhases_[0] * 2.0f;
                } else {
                    tri01 = 2.0f - lfoPhases_[0] * 2.0f;
                }
                // Map [0,1] → [-1,+1] for symmetric delay swing
                float lfo = tri01 * 2.0f - 1.0f;

                lfoPhases_[0] += phaseInc;
                if (lfoPhases_[0] >= 1.0f) lfoPhases_[0] -= 1.0f;

                // Modulated delay readback (single voice)
                float delaySamples = centerDelay + lfo * depthSamples;
                delaySamples = std::max(1.0f, delaySamples);

                // Read with linear interpolation
                float readPosF = static_cast<float>(writePos_) - delaySamples;
                if (readPosF < 0.0f)
                    readPosF += static_cast<float>(bufferSize_);
                int readInt = static_cast<int>(readPosF);
                float frac = readPosF - static_cast<float>(readInt);
                int idx0 = readInt & bufferMask_;
                int idx1 = (readInt + 1) & bufferMask_;
                float wet = delayBuffer_[idx0] * (1.0f - frac)
                          + delayBuffer_[idx1] * frac;

                // Output wet signal (wet/dry mix handled by SignalChain)
                buffer[i] = clamp(wet, -1.0f, 1.0f);
                writePos_ = (writePos_ + 1) & bufferMask_;
            }
        } else {
            // =============================================================
            // Standard 3-Voice Chorus
            // =============================================================

            // Depth in samples (0-10ms of modulation swing)
            const float depthSamples = depth * 0.010f
                                       * static_cast<float>(sampleRate_);

            // Center delay in samples (~7ms)
            const float centerDelay = 0.007f
                                      * static_cast<float>(sampleRate_);

            for (int i = 0; i < numFrames; ++i) {
                const float input = buffer[i];

                // Write input to delay buffer
                delayBuffer_[writePos_] = input;

                // Three voices with 120-degree phase offsets
                float wet = 0.0f;
                for (int v = 0; v < 3; ++v) {
                    float phase = lfoPhases_[v];
                    float lfo = fast_math::sin2pi(phase);

                    // Modulated delay position
                    float delaySamples = centerDelay + lfo * depthSamples;
                    delaySamples = std::max(1.0f, delaySamples);

                    // Read with linear interpolation
                    float readPosF = static_cast<float>(writePos_)
                                     - delaySamples;
                    if (readPosF < 0.0f)
                        readPosF += static_cast<float>(bufferSize_);
                    int readInt = static_cast<int>(readPosF);
                    float frac = readPosF - static_cast<float>(readInt);
                    int idx0 = readInt & bufferMask_;
                    int idx1 = (readInt + 1) & bufferMask_;
                    float sample = delayBuffer_[idx0] * (1.0f - frac)
                                 + delayBuffer_[idx1] * frac;
                    wet += sample;

                    // Advance LFO phase
                    lfoPhases_[v] += phaseInc;
                    if (lfoPhases_[v] >= 1.0f) lfoPhases_[v] -= 1.0f;
                }

                // Average the three voices
                wet *= (1.0f / 3.0f);

                // Output the wet signal (wet/dry handled by SignalChain)
                buffer[i] = clamp(wet, -1.0f, 1.0f);

                writePos_ = (writePos_ + 1) & bufferMask_;
            }
        }
    }

    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;

        // Allocate delay buffer for maximum delay (~20ms + margin)
        int minSize = static_cast<int>(0.030f * sampleRate) + 1;
        bufferSize_ = nextPowerOfTwo(minSize);
        bufferMask_ = bufferSize_ - 1;
        delayBuffer_.assign(bufferSize_, 0.0f);
        writePos_ = 0;

        // Initialize LFO phases with 120-degree spacing (for standard mode)
        lfoPhases_[0] = 0.0f;
        lfoPhases_[1] = 1.0f / 3.0f;
        lfoPhases_[2] = 2.0f / 3.0f;

        // Pre-compute CE-1 BBD bandwidth-limiting LP coefficient (~13kHz).
        // The MN3002 BBD's clock rate limits usable bandwidth to about
        // 13kHz. One-pole LP: coeff = exp(-2*pi*fc/fs).
        const double fs = static_cast<double>(sampleRate);
        bbdLPCoeff_ = 1.0 - std::exp(-2.0 * 3.14159265358979323846
                                       * 13000.0 / fs);
    }

    void reset() override {
        std::fill(delayBuffer_.begin(), delayBuffer_.end(), 0.0f);
        writePos_ = 0;
        lfoPhases_[0] = 0.0f;
        lfoPhases_[1] = 1.0f / 3.0f;
        lfoPhases_[2] = 2.0f / 3.0f;
        bbdLPState_ = 0.0f;
        bbdLPState2_ = 0.0f;
    }

    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0: rate_.store(clamp(value, 0.1f, 10.0f),
                                std::memory_order_relaxed); break;
            case 1: depth_.store(clamp(value, 0.0f, 1.0f),
                                 std::memory_order_relaxed); break;
            case 2: mode_.store(clamp(value, 0.0f, 1.0f),
                                std::memory_order_relaxed); break;
            default: break;
        }
    }

    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return rate_.load(std::memory_order_relaxed);
            case 1: return depth_.load(std::memory_order_relaxed);
            case 2: return mode_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    static int nextPowerOfTwo(int n) {
        int p = 1; while (p < n) p <<= 1; return p;
    }
    static float clamp(float v, float lo, float hi) {
        return std::max(lo, std::min(hi, v));
    }

    // Parameters (atomic for lock-free access from audio + UI threads)
    std::atomic<float> rate_{1.5f};
    std::atomic<float> depth_{0.5f};
    std::atomic<float> mode_{0.0f};   // 0 = Standard, 1 = CE-1

    int32_t sampleRate_ = 44100;
    std::vector<float> delayBuffer_;
    int bufferSize_ = 0;
    int bufferMask_ = 0;
    int writePos_ = 0;
    float lfoPhases_[3] = {0.0f, 0.333f, 0.667f};

    // CE-1 BBD bandwidth-limiting LP filter state (2-pole cascaded)
    float bbdLPState_ = 0.0f;
    float bbdLPState2_ = 0.0f;  // Second pole for 12dB/oct Sallen-Key match
    double bbdLPCoeff_ = 0.0;
};

#endif // GUITAR_ENGINE_CHORUS_H
