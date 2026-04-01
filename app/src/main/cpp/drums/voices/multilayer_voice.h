#ifndef GUITAR_ENGINE_MULTILAYER_VOICE_H
#define GUITAR_ENGINE_MULTILAYER_VOICE_H

#include <atomic>
#include <cstdint>
#include <memory>

#include "../drum_voice.h"
#include "../drum_types.h"
#include "../../effects/fast_math.h"
#include "fm_voice.h"
#include "subtractive_voice.h"
#include "metallic_voice.h"
#include "noise_voice.h"
#include "physical_voice.h"

/**
 * Multi-layer drum voice combining two sub-voices with crossfade and drive.
 *
 * Owns two independent DrumVoice sub-instances (A and B) created from any
 * of the 5 non-MultiLayer engine types. The outputs are crossfaded and
 * optionally saturated through fast_tanh for harmonic richness.
 *
 * Signal flow:
 *   [Voice A] --\
 *                +--> [Crossfade (mix)] --> [Drive (fast_tanh)] --> out
 *   [Voice B] --/
 *
 *   output = fast_tanh(drive * (A * (1 - mix) + B * mix))
 *
 * Parameters:
 *   0:  voiceAType  - Engine type for voice A (0-4, EngineType, NOT MultiLayer)
 *   1:  voiceBType  - Engine type for voice B (0-4, EngineType, NOT MultiLayer)
 *   2:  mix         - Crossfade between A and B (0 = all A, 1 = all B)
 *   3:  drive       - Post-mix saturation amount (0-5)
 *   4-9:  forwarded to voice A (param 4 -> voice A param 0, etc.)
 *   10-15: forwarded to voice B (param 10 -> voice B param 0, etc.)
 *
 * Safety constraint: createVoice() rejects EngineType::MULTI_LAYER to
 * prevent infinite recursion. If an invalid type is requested, it falls
 * back to an FM voice.
 *
 * Thread safety: All parameters are std::atomic<float> for UI thread writes.
 * Sub-voice type changes (params 0, 1) trigger voice reconstruction which
 * allocates memory -- this is acceptable because type changes only occur
 * from the UI thread when the engine is stopped or during pattern load.
 * process() is called exclusively from the audio thread.
 */

namespace drums {

class MultiLayerVoice : public DrumVoice {
public:
    MultiLayerVoice() {
        params_[0].store(0.0f, std::memory_order_relaxed);  // Voice A: FM
        params_[1].store(1.0f, std::memory_order_relaxed);  // Voice B: Subtractive
        params_[2].store(0.5f, std::memory_order_relaxed);  // 50/50 mix
        params_[3].store(1.0f, std::memory_order_relaxed);  // moderate drive

        // Initialize remaining forwarded params to zero
        for (int i = 4; i < kMaxParams; ++i) {
            params_[i].store(0.0f, std::memory_order_relaxed);
        }

        // Create default sub-voices
        voiceA_ = createVoice(EngineType::FM);
        voiceB_ = createVoice(EngineType::SUBTRACTIVE);
    }

    void trigger(float velocity) override {
        velocity_ = velocity;
        active_ = true;
        fadeCounter_ = 0;
        fadeGain_ = 1.0f;

        // Trigger both sub-voices
        if (voiceA_) voiceA_->trigger(velocity);
        if (voiceB_) voiceB_->trigger(velocity);
    }

    float process() override {
        if (!active_) return 0.0f;

        const float mix   = params_[2].load(std::memory_order_relaxed);
        const float drive = params_[3].load(std::memory_order_relaxed);

        // Process both sub-voices
        const float a = voiceA_ ? voiceA_->process() : 0.0f;
        const float b = voiceB_ ? voiceB_->process() : 0.0f;

        // Crossfade and saturate
        const float blended = a * (1.0f - mix) + b * mix;
        float out = fast_math::fast_tanh(drive * blended);

        // Deactivate when both sub-voices are silent
        const bool aActive = voiceA_ && voiceA_->isActive();
        const bool bActive = voiceB_ && voiceB_->isActive();
        if (!aActive && !bActive) {
            active_ = false;
            return 0.0f;
        }

        return applyFade(out);
    }

    void setParam(int paramId, float value) override {
        if (paramId < 0 || paramId >= kMaxParams) return;

        // Clamp values to valid ranges
        switch (paramId) {
            case 0: // voiceAType
                value = clamp(value, 0.0f, 4.0f);
                break;
            case 1: // voiceBType
                value = clamp(value, 0.0f, 4.0f);
                break;
            case 2: // mix
                value = clamp(value, 0.0f, 1.0f);
                break;
            case 3: // drive
                value = clamp(value, 0.0f, 5.0f);
                break;
            default:
                // Forwarded params: no clamping here, sub-voices handle it
                break;
        }

        const float oldValue = params_[paramId].load(std::memory_order_relaxed);
        params_[paramId].store(value, std::memory_order_relaxed);

        // Handle voice type changes (reconstruct sub-voice)
        if (paramId == 0) {
            const int newType = static_cast<int>(value + 0.5f); // round to nearest int
            const int oldType = static_cast<int>(oldValue + 0.5f);
            if (newType != oldType) {
                voiceA_ = createVoice(static_cast<EngineType>(newType));
                if (voiceA_ && sampleRate_ > 0) {
                    voiceA_->setSampleRate(sampleRate_);
                }
                // Forward current params 4-9 to new voice A
                for (int i = 0; i < 6; ++i) {
                    if (voiceA_) {
                        voiceA_->setParam(i, params_[4 + i].load(std::memory_order_relaxed));
                    }
                }
            }
        } else if (paramId == 1) {
            const int newType = static_cast<int>(value + 0.5f);
            const int oldType = static_cast<int>(oldValue + 0.5f);
            if (newType != oldType) {
                voiceB_ = createVoice(static_cast<EngineType>(newType));
                if (voiceB_ && sampleRate_ > 0) {
                    voiceB_->setSampleRate(sampleRate_);
                }
                // Forward current params 10-15 to new voice B
                for (int i = 0; i < 6; ++i) {
                    if (voiceB_) {
                        voiceB_->setParam(i, params_[10 + i].load(std::memory_order_relaxed));
                    }
                }
            }
        }

        // Forward params 4-9 to voice A
        if (paramId >= 4 && paramId <= 9 && voiceA_) {
            voiceA_->setParam(paramId - 4, value);
        }

        // Forward params 10-15 to voice B
        if (paramId >= 10 && paramId <= 15 && voiceB_) {
            voiceB_->setParam(paramId - 10, value);
        }
    }

    float getParam(int paramId) const override {
        if (paramId < 0 || paramId >= kMaxParams) return 0.0f;
        return params_[paramId].load(std::memory_order_relaxed);
    }

    void setSampleRate(int32_t sr) override {
        sampleRate_ = sr;
        if (voiceA_) voiceA_->setSampleRate(sr);
        if (voiceB_) voiceB_->setSampleRate(sr);
    }

    void reset() override {
        active_ = false;
        fadeCounter_ = 0;
        fadeGain_ = 1.0f;
        if (voiceA_) voiceA_->reset();
        if (voiceB_) voiceB_->reset();
    }

private:
    static constexpr int kMaxParams = 16;

    std::atomic<float> params_[kMaxParams];
    std::unique_ptr<DrumVoice> voiceA_;
    std::unique_ptr<DrumVoice> voiceB_;
    float velocity_ = 0.0f;
    int32_t sampleRate_ = 0;

    /**
     * Factory function to create a DrumVoice by engine type.
     *
     * CRITICAL: Rejects EngineType::MULTI_LAYER to prevent infinite
     * recursion. Falls back to FM voice if an invalid type is given.
     *
     * @param type The engine type to create (must be 0-4, not MULTI_LAYER).
     * @return New voice instance, or FM voice as fallback.
     */
    static std::unique_ptr<DrumVoice> createVoice(EngineType type) {
        // SAFETY: Reject MULTI_LAYER to prevent infinite recursion
        if (type == EngineType::MULTI_LAYER) {
            type = EngineType::FM; // fallback
        }

        switch (type) {
            case EngineType::FM:
                return std::make_unique<FmVoice>();
            case EngineType::SUBTRACTIVE:
                return std::make_unique<SubtractiveVoice>();
            case EngineType::METALLIC:
                return std::make_unique<MetallicVoice>();
            case EngineType::NOISE:
                return std::make_unique<NoiseVoice>();
            case EngineType::PHYSICAL:
                return std::make_unique<PhysicalVoice>();
            default:
                return std::make_unique<FmVoice>(); // safe fallback
        }
    }

    /** Clamp a value to [lo, hi]. */
    static float clamp(float x, float lo, float hi) {
        return (x < lo) ? lo : ((x > hi) ? hi : x);
    }
};

} // namespace drums

#endif // GUITAR_ENGINE_MULTILAYER_VOICE_H
