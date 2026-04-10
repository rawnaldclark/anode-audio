#include "signal_chain.h"

#include "effects/noise_gate.h"
#include "effects/compressor.h"
#include "effects/wah.h"
#include "effects/boost.h"
#include "effects/overdrive.h"
#include "effects/amp_model.h"
#include "effects/cabinet_sim.h"
#include "effects/parametric_eq.h"
#include "effects/chorus.h"
#include "effects/vibrato.h"
#include "effects/phaser.h"
#include "effects/flanger.h"
#include "effects/delay.h"
#include "effects/reverb.h"
#include "effects/tuner.h"
#include "effects/boss_ds1.h"
#include "effects/proco_rat.h"
#include "effects/boss_ds2.h"
#include "effects/boss_hm2.h"
#include "effects/univibe.h"
#include "effects/fuzz_face.h"
#include "effects/rangemaster.h"
#include "effects/big_muff.h"
#include "effects/octavia.h"
#include "effects/rotary_speaker.h"
#include "effects/fuzzrite.h"
#include "effects/mf102_ring_mod.h"
#include "effects/space_echo.h"
#include "effects/dl4_delay.h"
#include "effects/boss_bd2.h"
#include "effects/tube_screamer.h"
#include "effects/tremolo.h"
#include "faust/faust_effect.h"

#include <cstring>

// =============================================================================
// Construction / Destruction
// =============================================================================

SignalChain::SignalChain() {
    // Reserve space for the full default chain (32 effects) to avoid
    // vector reallocation when effects are added in createDefaultChain().
    effects_.reserve(kMaxEffects);

    // Initialize both order buffers to identity [0, 1, 2, ..., 29]
    for (int i = 0; i < kMaxEffects; ++i) {
        orderBuf_[0][i] = i;
        orderBuf_[1][i] = i;
    }
}

SignalChain::~SignalChain() = default;

// =============================================================================
// Audio processing (real-time safe zone)
// =============================================================================

void SignalChain::process(float* buffer, int numFrames) {
    // numFrames is for mono (1 channel), so numSamples == numFrames
    // Clamp to dry buffer capacity to prevent overrun (safety net).
    // When dryBufferSize_ is 0 (setSampleRate not yet called), we still
    // process effects but skip wet/dry blending to avoid writing to an
    // empty vector.
    const int numSamples = (dryBufferSize_ > 0)
        ? std::min(numFrames, dryBufferSize_)
        : numFrames;

    // Read the active order buffer atomically. The UI thread writes to
    // the other buffer and then swaps the index, so this read is always
    // consistent (no torn reads on a single int).
    const auto& order = orderBuf_[activeOrder_.load(std::memory_order_acquire)];
    const int effectCount = static_cast<int>(effects_.size());

    for (int i = 0; i < effectCount; ++i) {
        const int idx = order[i];
        if (idx < 0 || idx >= effectCount) continue; // safety check

        auto& effect = effects_[idx];

        // Skip the tuner -- it is fed directly with clean pre-effects
        // input by AudioEngine::onAudioReady() via feedTuner().
        // Processing it again here would double-feed the analysis buffer.
        // Checked before isEnabled() to avoid an unnecessary atomic load.
        if (idx == kTunerIndex) {
            continue;
        }

        // Skip disabled effects entirely -- no processing cost
        if (!effect->isEnabled()) {
            continue;
        }

        const float mix = effect->getWetDryMix();

        if (mix < 1.0f && dryBufferSize_ > 0) {
            // Wet/dry blending needed: save the dry signal before processing.
            // This allows the user to blend the original (dry) signal with
            // the processed (wet) signal for effects like delay where you
            // typically want to hear the original note plus the echoes.
            //
            // Guard: only blend when the dry buffer has been allocated
            // (dryBufferSize_ > 0, set by setSampleRate). Before that,
            // process fully wet to avoid memcpy into a zero-length vector.

            // Copy the current buffer (dry signal) before the effect modifies it
            std::memcpy(dryBuffer_.data(), buffer, numSamples * sizeof(float));

            // Process the buffer in-place (now contains wet signal)
            effect->process(buffer, numFrames);

            // Blend wet and dry signals using the effect's mixing method
            effect->applyWetDryMix(buffer, dryBuffer_.data(), numSamples);
        } else {
            // Fully wet or dry buffer not yet allocated: process in-place.
            // This is the common case and avoids the memcpy overhead.
            effect->process(buffer, numFrames);
        }
    }
}

// =============================================================================
// Configuration (called from main thread during setup)
// =============================================================================

void SignalChain::setSampleRate(int32_t sampleRate) {
    sampleRate_ = sampleRate;

    // Propagate sample rate to all effects so they can recalculate
    // their internal coefficients and pre-allocate buffers.
    for (auto& effect : effects_) {
        effect->setSampleRate(sampleRate);
    }

    // Pre-allocate the dry buffer with generous headroom.
    // Oboe may deliver more frames than the burst size, so use 4x to be safe.
    // This ensures no allocation happens on the audio thread.
    const int maxExpectedSamples = 8192;
    dryBuffer_.resize(maxExpectedSamples, 0.0f);
    dryBufferSize_ = maxExpectedSamples;
}

void SignalChain::resetAll() {
    for (auto& effect : effects_) {
        effect->reset();
    }
}

int SignalChain::getEffectCount() const {
    return static_cast<int>(effects_.size());
}

Effect* SignalChain::getEffect(int index) {
    if (index < 0 || index >= static_cast<int>(effects_.size())) {
        return nullptr;
    }
    return effects_[index].get();
}

const Effect* SignalChain::getEffect(int index) const {
    if (index < 0 || index >= static_cast<int>(effects_.size())) {
        return nullptr;
    }
    return effects_[index].get();
}

void SignalChain::createDefaultChain() {
    // Clear any existing effects
    effects_.clear();

    // Build the default guitar signal chain (32 effects) in pedalboard order:
    //
    //  0. Noise Gate    -- Silences hiss/hum before it gets amplified downstream
    //  1. Compressor    -- Evens out dynamics before distortion for consistent saturation
    //  2. Wah           -- Filter effect (auto-wah), placed pre-distortion for classic tone
    //  3. Boost         -- Clean gain stage to drive overdrive/amp harder
    //  4. Overdrive     -- Waveshaping distortion, the core "tone" generator
    //  5. Amp Model     -- Neural amp modeling (tube amp simulation)
    //  6. Cabinet Sim   -- Speaker cabinet impulse response convolution
    //  7. Parametric EQ -- Tone sculpting after amp/cab (cut mud, boost presence)
    //  8. Chorus        -- Thickening/doubling effect
    //  9. Vibrato       -- Pitch modulation via delay line (no dry blend)
    // 10. Phaser        -- Sweeping notch filter effect
    // 11. Flanger       -- Jet/whoosh comb filter effect
    // 12. Delay         -- Echoes placed after modulation so echoes are clean copies
    // 13. Reverb        -- Spatial ambience
    // 14. Tuner         -- Pitch analysis only, passes audio through unchanged
    // 15. Tremolo       -- FAUST-generated amplitude modulation (Fender-style post-reverb)
    //
    // This order matches conventional pedalboard wisdom:
    //   - Dynamics & filters before dirt
    //   - Boost before overdrive (drives it harder, like a Tube Screamer)
    //   - Amp model + cabinet after overdrive (models the amp and speaker)
    //   - Modulation after amp/cab
    //   - Time-based effects at the end
    //   - Tuner is fed separately with clean DI input (see feedTuner())

    // Default fresh-install tone: EP-3 Boost → Univibe → mild Reverb.
    // All other previously-default effects (NoiseGate, Compressor, Overdrive,
    // EQ, Chorus, Delay) are disabled to avoid the washy "atmospheric"
    // default that made the app sound generic on first launch.

    // Index 0: Noise Gate (disabled by default — user prefers raw tone)
    effects_.push_back(std::make_unique<NoiseGate>());
    effects_[0]->setEnabled(false);

    // Index 1: Compressor (disabled by default)
    effects_.push_back(std::make_unique<Compressor>());
    effects_[1]->setEnabled(false);

    // Index 2: Wah (disabled by default -- enable when needed)
    effects_.push_back(std::make_unique<Wah>());
    effects_[2]->setEnabled(false);

    // Index 3: Boost — ENABLED by default in EP-3 Echoplex preamp mode.
    // EP-3 mode adds the signature Maestro Echoplex preamp sweetener:
    // gentle low-cut, presence shelf boost at ~2kHz, and JFET harmonic
    // saturation. paramId 0 = level (dB), paramId 1 = mode (0=clean, 1=EP3).
    effects_.push_back(std::make_unique<Boost>());
    effects_[3]->setEnabled(true);
    effects_[3]->setParameter(1, 1.0f);  // Mode: EP-3 Echoplex preamp
    effects_[3]->setParameter(0, 3.0f);  // Level: +3 dB (modest, "always-on" boost)

    // Index 4: Overdrive (disabled by default)
    effects_.push_back(std::make_unique<Overdrive>());
    effects_[4]->setEnabled(false);

    // Index 5: Amp Model (disabled by default -- enable when an amp model is selected)
    effects_.push_back(std::make_unique<AmpModel>());
    effects_[5]->setEnabled(false);

    // Index 6: Cabinet Sim (disabled by default -- enable with amp model)
    effects_.push_back(std::make_unique<CabinetSim>());
    effects_[6]->setEnabled(false);

    // Index 7: Parametric EQ (disabled by default — all bands flat anyway)
    effects_.push_back(std::make_unique<ParametricEQ>());
    effects_[7]->setEnabled(false);

    // Index 8: Chorus (disabled by default)
    effects_.push_back(std::make_unique<Chorus>());
    effects_[8]->setEnabled(false);

    // Index 9: Vibrato (disabled by default)
    effects_.push_back(std::make_unique<Vibrato>());
    effects_[9]->setEnabled(false);

    // Index 10: Phaser (disabled by default)
    effects_.push_back(std::make_unique<Phaser>());
    effects_[10]->setEnabled(false);

    // Index 11: Flanger (disabled by default)
    effects_.push_back(std::make_unique<Flanger>());
    effects_[11]->setEnabled(false);

    // Index 12: Delay (disabled by default)
    effects_.push_back(std::make_unique<Delay>());
    effects_[12]->setEnabled(false);

    // Index 13: Reverb — ENABLED by default, mild settings.
    // Default params: decay=0.5 (~2s), damping=0.5, size=0.5 — these are
    // already moderate. The 30% wet mix (set below) keeps it subtle.
    effects_.push_back(std::make_unique<Reverb>());
    effects_[13]->setEnabled(true);

    // Index 14: Tuner (analysis only, always enabled, no audio modification)
    // Must be explicitly enabled since Effect base class defaults to disabled.
    // The tuner is a passthrough analyzer -- it reads audio but never modifies it.
    effects_.push_back(std::make_unique<Tuner>());
    effects_[14]->setEnabled(true);

    // Index 15: Tremolo (dual-mode: Standard FAUST + Vintage Optical VT-X)
    // Placed after reverb intentionally: this mirrors classic Fender amplifiers
    // where the tremolo circuit sits after the reverb tank, causing the tremolo
    // to modulate the entire reverb tail ("surf rock" tremolo sound).
    // Mode 0 (default): FAUST sine/square AM tremolo (backward compatible).
    // Mode 1: Circuit-accurate Guyatone VT-X optical tremolo with tube gain
    //         stage, neon lamp, and CdS LDR modeling.
    effects_.push_back(std::make_unique<Tremolo>());
    effects_[15]->setEnabled(false);

    // Index 16: Boss DS-1 Distortion (circuit-accurate WDF emulation, disabled by default)
    effects_.push_back(std::make_unique<BossDS1>());
    effects_[16]->setEnabled(false);

    // Index 17: ProCo RAT Distortion (circuit-accurate WDF emulation, disabled by default)
    effects_.push_back(std::make_unique<ProCoRAT>());
    effects_[17]->setEnabled(false);

    // Index 18: Boss DS-2 Turbo Distortion (circuit-accurate WDF emulation, disabled by default)
    effects_.push_back(std::make_unique<BossDS2>());
    effects_[18]->setEnabled(false);

    // Index 19: Boss HM-2 Heavy Metal (circuit-accurate WDF emulation, disabled by default)
    effects_.push_back(std::make_unique<BossHM2>());
    effects_[19]->setEnabled(false);

    // Index 20: Univibe — ENABLED by default (classic Hendrix-style tone).
    // Default params: speed=0.3 (~1.3Hz, slow sweep), intensity=0.7 (deep),
    // mode=0 (chorus). The asymmetric amplitude modulation from the DAFx
    // non-ideal allpass model produces the signature throbbing character.
    effects_.push_back(std::make_unique<Univibe>());
    effects_[20]->setEnabled(true);

    // Index 21: Fuzz Face (WDF germanium 2-transistor fuzz, disabled by default)
    effects_.push_back(std::make_unique<FuzzFace>());
    effects_[21]->setEnabled(false);

    // Index 22: Rangemaster (WDF germanium treble booster, disabled by default)
    effects_.push_back(std::make_unique<Rangemaster>());
    effects_[22]->setEnabled(false);

    // Index 23: Big Muff Pi (WDF 4-stage cascaded fuzz, disabled by default)
    effects_.push_back(std::make_unique<BigMuff>());
    effects_[23]->setEnabled(false);

    // Index 24: Octavia (octave-up rectifier fuzz, disabled by default)
    effects_.push_back(std::make_unique<Octavia>());
    effects_[24]->setEnabled(false);

    // Index 25: Rotary Speaker (Leslie cabinet doppler/amplitude modulation, disabled by default)
    effects_.push_back(std::make_unique<RotarySpeaker>());
    effects_[25]->setEnabled(false);

    // Index 26: Fuzzrite (Mosrite Fuzzrite circuit-accurate emulation, disabled by default)
    effects_.push_back(std::make_unique<Fuzzrite>());
    effects_[26]->setEnabled(false);

    // Index 27: MF-102 Ring Modulator (OTA four-quadrant multiplier, disabled by default)
    effects_.push_back(std::make_unique<MF102RingMod>());
    effects_[27]->setEnabled(false);

    // Index 28: Space Echo (Roland RE-201 tape echo + spring reverb, disabled by default)
    effects_.push_back(std::make_unique<SpaceEcho>());
    effects_[28]->setEnabled(false);

    // Index 29: DL4 Delay Modeler (16 delay algorithms, disabled by default)
    effects_.push_back(std::make_unique<DL4Delay>());
    effects_[29]->setEnabled(false);

    // Index 30: Boss BD-2 Blues Driver (discrete JFET overdrive, disabled by default)
    effects_.push_back(std::make_unique<BossBD2>());
    effects_[30]->setEnabled(false);

    // Index 31: Tube Screamer (TS-808/TS9 circuit-accurate overdrive, disabled by default)
    effects_.push_back(std::make_unique<TubeScreamer>());
    effects_[31]->setEnabled(false);

    // Set wet/dry mix defaults for effects where parallel blending is typical.
    effects_[8]->setWetDryMix(0.5f);   // Chorus: 50% wet (thicken, not replace)
    effects_[12]->setWetDryMix(0.5f);  // Delay: 50% wet (hear note + echoes)
    effects_[13]->setWetDryMix(0.3f);  // Reverb: 30% wet (ambient, not washed out)

    // Reset processing order to identity [0, 1, 2, ..., N-1]
    const int n = static_cast<int>(effects_.size());
    for (int i = 0; i < kMaxEffects; ++i) {
        const int val = (i < n) ? i : 0;
        orderBuf_[0][i] = val;
        orderBuf_[1][i] = val;
    }
    activeOrder_.store(0, std::memory_order_release);

    // Apply sample rate to all newly created effects if we already know it
    if (sampleRate_ > 0) {
        for (auto& effect : effects_) {
            effect->setSampleRate(sampleRate_);
        }
    }
}

bool SignalChain::setEffectOrder(const int* order, int count) {
    const int effectCount = static_cast<int>(effects_.size());
    if (count != effectCount) {
        return false;
    }

    // Validate all indices are in range before committing
    for (int i = 0; i < count; ++i) {
        if (order[i] < 0 || order[i] >= effectCount) {
            return false;
        }
    }

    // Validate no duplicate indices (must be a valid permutation)
    bool seen[kMaxEffects] = {};
    for (int i = 0; i < count; ++i) {
        if (seen[order[i]]) return false;
        seen[order[i]] = true;
    }

    // Write to the inactive buffer, then atomically swap
    const int inactive = 1 - activeOrder_.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i) {
        orderBuf_[inactive][i] = order[i];
    }

    // Publish the new order to the audio thread
    activeOrder_.store(inactive, std::memory_order_release);
    return true;
}

void SignalChain::getEffectOrder(int* out, int count) const {
    const int effectCount = static_cast<int>(effects_.size());
    const int copyCount = std::min(count, effectCount);
    const auto& order = orderBuf_[activeOrder_.load(std::memory_order_acquire)];
    for (int i = 0; i < copyCount; ++i) {
        out[i] = order[i];
    }
}

void SignalChain::prepareTunerForActivation() {
    if (kTunerIndex < static_cast<int>(effects_.size())) {
        static_cast<Tuner*>(effects_[kTunerIndex].get())->prepareForActivation();
    }
}

void SignalChain::feedTuner(float* buffer, int numFrames) {
    if (kTunerIndex < static_cast<int>(effects_.size())) {
        // Always feed the tuner regardless of enabled state so that
        // pitch data is available to the UI whenever the engine runs.
        effects_[kTunerIndex]->process(buffer, numFrames);
    }
}

void SignalChain::setEffectParameter(int effectIndex, int paramId, float value) {
    if (effectIndex < 0 || effectIndex >= static_cast<int>(effects_.size())) {
        return;
    }
    effects_[effectIndex]->setParameter(paramId, value);
}

float SignalChain::getEffectParameter(int effectIndex, int paramId) const {
    if (effectIndex < 0 || effectIndex >= static_cast<int>(effects_.size())) {
        return 0.0f;
    }
    return effects_[effectIndex]->getParameter(paramId);
}

// =============================================================================
// Private helpers
// =============================================================================

// =============================================================================
// Neural amp model management
// =============================================================================

// AmpModel is always at index 5 in the default chain.
static constexpr int kAmpModelIndex = 5;

bool SignalChain::loadNeuralModel(const std::string& path) {
    if (kAmpModelIndex >= static_cast<int>(effects_.size())) {
        return false;
    }
    auto* ampModel = dynamic_cast<AmpModel*>(effects_[kAmpModelIndex].get());
    if (!ampModel) {
        return false;
    }
    return ampModel->loadNeuralModel(path);
}

void SignalChain::clearNeuralModel() {
    if (kAmpModelIndex >= static_cast<int>(effects_.size())) {
        return;
    }
    auto* ampModel = dynamic_cast<AmpModel*>(effects_[kAmpModelIndex].get());
    if (ampModel) {
        ampModel->clearNeuralModel();
    }
}

bool SignalChain::isNeuralModelLoaded() const {
    if (kAmpModelIndex >= static_cast<int>(effects_.size())) {
        return false;
    }
    const auto* ampModel =
        dynamic_cast<const AmpModel*>(effects_[kAmpModelIndex].get());
    if (ampModel) {
        return ampModel->isNeuralModelLoaded();
    }
    return false;
}

// =============================================================================
// Private helpers
// =============================================================================

void SignalChain::ensureDryBufferSize(int numSamples) {
    // The dry buffer is pre-allocated in setSampleRate() with generous headroom.
    // This is a safety check -- should never trigger in practice, but prevents
    // buffer overrun if Oboe delivers an unexpectedly large buffer.
    if (numSamples > dryBufferSize_ && dryBufferSize_ > 0) {
        // Cannot allocate on the audio thread. Clamp to available size.
        // The caller should use the return value or the member variable.
        // For safety, we just log this would be a problem, but since we can't
        // allocate, the process() method must check dryBufferSize_ directly.
    }
}
