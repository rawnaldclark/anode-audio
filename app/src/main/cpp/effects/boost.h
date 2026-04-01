#ifndef GUITAR_ENGINE_BOOST_H
#define GUITAR_ENGINE_BOOST_H

#include "effect.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>

/**
 * Clean gain boost / Echoplex EP-3 preamp emulation.
 *
 * Two modes of operation selectable via paramId 1:
 *
 * Mode 0 (Clean): A transparent gain stage placed before the overdrive/amp
 * model in the signal chain. Linear gain (0 to +24 dB) with tanh soft
 * saturation. This is the principle behind the Tube Screamer boost, Klon
 * Centaur, and MXR Micro Amp.
 *
 * Mode 1 (EP-3 Echoplex Preamp): Models the single-JFET (TIS58/2N5457)
 * preamp stage from the Maestro Echoplex EP-3 tape delay unit. Guitarists
 * discovered that simply plugging into the EP-3 -- even with the echo off --
 * added a desirable sparkle and presence to the signal. The circuit imparts:
 *
 *   1. High-pass filter at ~72 Hz (22nF input coupling capacitor with ~100K
 *      JFET input impedance removes low-end mud)
 *   2. Presence shelf boost of ~3.5 dB above ~2 kHz (22nF source bypass
 *      capacitor creates frequency-dependent negative feedback: at low
 *      frequencies the source resistor provides degeneration, reducing gain;
 *      at high frequencies the bypass cap shorts the resistor, removing
 *      degeneration and boosting gain)
 *   3. JFET square-law asymmetric soft clipping (even-harmonic-rich warmth;
 *      positive half clips softer, negative half clips harder)
 *   4. Gain range limited to 0-11 dB (matching the real EP-3 preamp headroom)
 *
 * Signal flow (EP-3 mode):
 *   Input -> HPF 72Hz -> Gain (0-11dB) -> Presence shelf -> JFET clip -> Output
 *
 * Signal flow (Clean mode):
 *   Input -> Linear gain (0-24 dB) -> Soft limiter (tanh) -> Output
 *
 * Parameter IDs for JNI access:
 *   0 = level (0.0 to 24.0 dB) - boost amount (mapped to 0-11 dB in EP3 mode)
 *   1 = mode  (0.0 = Clean, 1.0 = EP3 Echoplex Preamp)
 */
class Boost : public Effect {
public:
    Boost() = default;

    void process(float* buffer, int numFrames) override {
        const float levelDb = level_.load(std::memory_order_relaxed);
        const float modeVal = mode_.load(std::memory_order_relaxed);
        const bool ep3Mode = (modeVal >= 0.5f);

        if (ep3Mode) {
            // --- EP-3 Echoplex Preamp path ---
            // Scale gain range: 0-24 dB control maps to 0-11 dB (real EP-3 headroom)
            const float ep3GainDb = levelDb * (11.0f / 24.0f);
            // Pre-compute linear gain outside per-sample loop (avoid per-sample std::pow)
            const float gain = std::pow(10.0f, ep3GainDb / 20.0f);

            for (int i = 0; i < numFrames; ++i) {
                double x = static_cast<double>(buffer[i]);

                // (1) Input HPF: 22nF coupling cap, fc ~72Hz
                // One-pole HP: hp = x - lpState, where lpState tracks the DC/LF content.
                // This removes low-end mud the same way the physical coupling cap does.
                hpState_ += hpCoeff_ * (x - hpState_);
                double hp = x - hpState_;

                // (2) Apply JFET preamp gain (0 to +11 dB)
                double gained = hp * gain;

                // (3) Presence shelf boost at ~2kHz
                // The source bypass capacitor creates a natural treble lift by
                // shorting the source degeneration resistor at high frequencies.
                // Implementation: LP filter extracts LF content, the difference
                // (gained - lpState) is the HF content, which gets boosted.
                shelfState_ += shelfCoeff_ * (gained - shelfState_);
                double hfContent = gained - shelfState_;
                double withShelf = gained + hfContent * static_cast<double>(shelfGain_);

                // (4) JFET square-law asymmetric soft clipping
                // The JFET transfer characteristic (Id = Idss * (1 - Vgs/Vp)^2)
                // produces asymmetric saturation: the positive swing has more
                // headroom while the negative swing compresses earlier. This
                // generates predominantly even-order harmonics (2nd, 4th) which
                // sound musical and warm -- the signature EP-3 "fairy dust".
                double shaped;
                if (withShelf >= 0.0) {
                    shaped = fast_math::fast_tanh(withShelf * 0.9);   // softer positive
                } else {
                    shaped = fast_math::fast_tanh(withShelf * 1.2);   // harder negative
                }

                buffer[i] = static_cast<float>(shaped);
            }

            // Flush filter states to zero if they decay to denormal range
            // (prevents 10-100x CPU slowdown on some ARM cores)
            hpState_ = fast_math::denormal_guard(hpState_);
            shelfState_ = fast_math::denormal_guard(shelfState_);

        } else {
            // --- Clean boost path (original behavior) ---
            // Convert dB to linear gain: 0 dB = 1.0x, 12 dB = 4.0x, 24 dB = 15.85x
            const float gain = std::pow(10.0f, levelDb / 20.0f);

            for (int i = 0; i < numFrames; ++i) {
                // Apply gain and continuous soft saturation via tanh.
                // tanh(x) ~ x for small x (transparent at low gain), and
                // smoothly compresses toward +/-1 at high levels. This avoids
                // the audible discontinuity that a threshold-gated limiter
                // would produce at the transition point.
                buffer[i] = fast_math::fast_tanh(buffer[i] * gain);
            }
        }
    }

    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;

        // Pre-compute EP-3 filter coefficients (one-pole digital filters).
        // These are computed once per sample rate change, NOT per sample.

        // EP-3 input coupling cap HPF: fc ~72Hz
        // 22nF cap with ~100K JFET input impedance: fc = 1/(2*pi*R*C) ~ 72Hz
        const double hpFc = 72.0;
        hpCoeff_ = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * hpFc / sampleRate);

        // EP-3 source bypass cap presence shelf: fc ~2kHz
        // 22nF bypass cap across the source resistor creates a shelving boost
        // above the corner frequency where the cap begins to short the resistor.
        const double shelfFc = 2000.0;
        shelfCoeff_ = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * shelfFc / sampleRate);

        // Shelf boost amount: ~3.5 dB above 2kHz (matches measured EP-3 response)
        shelfGain_ = 0.5f;  // additive: LP + HP*1.5 = +3.5dB shelf (was 1.5 = +8dB, too bright)
    }

    void reset() override {
        // Zero all EP-3 filter state to prevent clicks on restart
        hpState_ = 0.0;
        shelfState_ = 0.0;
    }

    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0:
                level_.store(clamp(value, 0.0f, 24.0f),
                             std::memory_order_relaxed);
                break;
            case 1:
                mode_.store(clamp(value, 0.0f, 1.0f),
                            std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return level_.load(std::memory_order_relaxed);
            case 1: return mode_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    static float clamp(float value, float minVal, float maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    // Parameters (atomic for lock-free access from audio thread)
    std::atomic<float> level_{0.0f};  // [0, 24] dB boost
    std::atomic<float> mode_{0.0f};   // 0 = Clean, 1 = EP3 Echoplex Preamp

    // EP-3 filter state (double precision for low-frequency filter stability;
    // one-pole filters below ~100Hz need double because the coefficient is
    // very close to 1.0 and float loses precision in the subtraction)
    int32_t sampleRate_ = 44100;
    double hpState_ = 0.0;     // HPF state (input coupling cap)
    double hpCoeff_ = 0.0;     // HPF coefficient
    double shelfState_ = 0.0;  // Presence shelf LP state
    double shelfCoeff_ = 0.0;  // Shelf LP coefficient
    float shelfGain_ = 0.5f;   // Shelf HF boost multiplier (~3.5 dB)
};

#endif // GUITAR_ENGINE_BOOST_H
