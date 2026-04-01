#ifndef GUITAR_ENGINE_TREMOLO_H
#define GUITAR_ENGINE_TREMOLO_H

#include "effect.h"
#include "vintage_tremolo.h"
#include "../faust/faust_effect.h"
#include "../faust/generated/FaustTremolo.h"

#include <atomic>
#include <algorithm>
#include <cstring>

/**
 * Dual-mode tremolo effect combining standard and vintage optical algorithms.
 *
 * Mode 0 (Standard): The existing FAUST-generated sine/square amplitude
 *   modulation tremolo. Clean, electronic-sounding tremolo with blendable
 *   sine and square LFO waveforms. Parameters: Rate, Depth, Shape.
 *
 * Mode 1 (Vintage Optical): Circuit-accurate Guyatone VT-X model with:
 *   - 12AX7 tube gain stage (warmth + volume boost)
 *   - Neon lamp NE-2 threshold/hysteresis model (choppy character)
 *   - CdS LDR asymmetric response (fast attack, slow release)
 *   - Logarithmic resistance voltage-divider VCA
 *   - Variable tone filter (800Hz - 20kHz)
 *   Parameters: Rate, Depth, Tone, Intensity.
 *
 * The mode selection preserves backward compatibility: existing presets that
 * only set params 0-2 (Rate/Depth/Shape) continue to work as Mode 0. New
 * presets can set param 3 (Mode) to 1 and use params 4-5 for Tone/Intensity.
 *
 * Parameter IDs:
 *   0: Rate      - LFO frequency (0.5 - 20.0 Hz)
 *   1: Depth     - Modulation depth (0.0 - 1.0)
 *   2: Shape     - Sine/square blend (0.0 - 1.0), Mode 0 only
 *   3: Mode      - 0.0 = Standard, 1.0 = Vintage Optical
 *   4: Tone      - LP filter cutoff (0.0 - 1.0), Vintage mode only
 *   5: Intensity - Neon lamp drive (0.0 - 1.0), Vintage mode only
 *   6: Phase Flip - Output polarity inversion (0.0 = normal, 1.0 = inverted)
 *
 * The Phase Flip is the defining feature of the Guyatone "Flip" series.
 * When the inverted signal is combined with a dry signal (e.g., via a
 * second amp or the dry/wet mix), the tremolo creates a push-pull effect
 * where the modulated signal cancels and reinforces alternately. This is
 * crucial for the Frusciante La Cigale intro tone.
 *
 * Thread safety: Parameters are stored as atomics. Both sub-engines read
 * parameters once per block into local variables, so there are no torn
 * reads mid-buffer.
 */
class Tremolo : public Effect {
public:
    Tremolo() {
        // The FaustEffect constructor calls buildUserInterface() to register
        // FAUST parameters. The vintage engine initializes its VCA table.
    }

    /**
     * Process audio through the selected tremolo mode.
     *
     * Reads the mode parameter and dispatches to either the FAUST standard
     * engine or the vintage optical engine. Only one engine processes per
     * block -- the other is dormant (no CPU cost for the inactive mode).
     */
    void process(float* buffer, int numFrames) override {
        const float modeVal = mode_.load(std::memory_order_relaxed);
        const bool vintageMode = (modeVal >= 0.5f);

        if (vintageMode) {
            vintage_.process(buffer, numFrames);
        } else {
            standard_.process(buffer, numFrames);
        }

        // Phase Flip: invert polarity if enabled.
        // This is the defining feature of the Guyatone "Flip" series.
        if (phaseFlip_.load(std::memory_order_relaxed) >= 0.5f) {
            for (int i = 0; i < numFrames; ++i) {
                buffer[i] = -buffer[i];
            }
        }
    }

    /**
     * Set sample rate for both sub-engines.
     *
     * Both engines must stay initialized even when inactive, because mode
     * switches can happen at any time and we need the inactive engine ready
     * to produce audio without glitches.
     */
    void setSampleRate(int32_t sampleRate) override {
        standard_.setSampleRate(sampleRate);
        vintage_.setSampleRate(sampleRate);
    }

    /**
     * Reset both sub-engines.
     *
     * Clears all filter state, LFO phase, and envelope followers in both
     * engines to prevent clicks when re-enabling the effect.
     */
    void reset() override {
        standard_.reset();
        vintage_.reset();
    }

    /**
     * Set a parameter by ID.
     *
     * Parameters 0-2 are forwarded to both engines where applicable.
     * Parameter 3 (Mode) is handled locally.
     * Parameters 4-5 are vintage-mode-specific.
     *
     * @param paramId Parameter index (0-5).
     * @param value   New value (clamped to valid range).
     */
    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0: // Rate: shared by both modes
                standard_.setParameter(0, value);
                vintage_.setRate(value);
                break;
            case 1: // Depth: shared by both modes
                standard_.setParameter(1, value);
                vintage_.setDepth(value);
                break;
            case 2: // Shape: standard mode only (sine/square blend)
                standard_.setParameter(2, value);
                break;
            case 3: // Mode: 0=Standard, 1=Vintage Optical
                mode_.store(clamp(value, 0.0f, 1.0f), std::memory_order_relaxed);
                break;
            case 4: // Tone: vintage mode only
                vintage_.setTone(value);
                break;
            case 5: // Intensity: vintage mode only
                vintage_.setIntensity(value);
                break;
            case 6: // Phase Flip: 0=normal, 1=inverted
                phaseFlip_.store(clamp(value, 0.0f, 1.0f), std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    /**
     * Get a parameter by ID.
     *
     * @param paramId Parameter index (0-5).
     * @return Current value, or 0.0f if paramId is out of range.
     */
    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return standard_.getParameter(0);  // Rate
            case 1: return standard_.getParameter(1);  // Depth
            case 2: return standard_.getParameter(2);  // Shape
            case 3: return mode_.load(std::memory_order_relaxed);
            case 4: return vintage_.getTone();
            case 5: return vintage_.getIntensity();
            case 6: return phaseFlip_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    static float clamp(float v, float lo, float hi) {
        return std::max(lo, std::min(hi, v));
    }

    // Mode selection: 0.0 = Standard, >= 0.5 = Vintage Optical
    std::atomic<float> mode_{0.0f};

    // Phase Flip: 0.0 = normal polarity, >= 0.5 = inverted
    std::atomic<float> phaseFlip_{0.0f};

    // Standard tremolo engine (FAUST-generated sine/square AM)
    FaustEffect<FaustTremolo> standard_;

    // Vintage optical tremolo engine (VT-X circuit model)
    VintageTremolo vintage_;
};

#endif // GUITAR_ENGINE_TREMOLO_H
