#ifndef GUITAR_ENGINE_VINTAGE_TREMOLO_H
#define GUITAR_ENGINE_VINTAGE_TREMOLO_H

#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>
#include <cstdint>

/**
 * Circuit-accurate Guyatone VT-X Vintage Tremolo optical tremolo model.
 *
 * Emulates the full signal path of the Guyatone Flip VT-X pedal:
 *
 * 1. V1B triode gain stage (12AX7WA at ~100-120V B+):
 *    - Input coupling cap HPF (~15Hz, 22nF + 470K)
 *    - ~35x voltage gain with asymmetric soft clipping (fast_tanh)
 *    - Adds 2nd-harmonic warmth characteristic of single-ended triode
 *    - Responsible for the VT-X's +6dB volume boost when engaged
 *
 * 2. Tone control (single-knob RC low-pass):
 *    - Variable cutoff: 800Hz (dark) to 20kHz (bright)
 *    - Unusual for tremolo pedals -- gives tonal versatility
 *
 * 3. LFO with neon lamp threshold model (NE-2 type):
 *    - Phase-shift oscillator output mildly overdriven (fast_tanh * 1.5)
 *    - Neon strike at ~0.7 (normalized), extinction at ~0.45
 *    - Hysteresis state machine creates choppy modulation at high intensity
 *    - At low intensity, LFO never reaches strike voltage -> smoother
 *
 * 4. CdS photoresistor (LDR) model:
 *    - Asymmetric response: fast attack ~15ms, slow release ~100ms
 *    - Logarithmic resistance mapping (dark ~2M ohm, lit ~600 ohm)
 *    - Pre-computed 256-point VCA lookup table avoids per-sample exp()
 *
 * 5. Voltage-divider VCA:
 *    - R_series (220K) + R_ldr forms shunt-to-ground attenuator
 *    - Never reaches full silence (min ~-51dB), preventing harsh chop
 *    - Slight brightness modulation from LDR/capacitance interaction
 *
 * 6. Output coupling HPF (~10Hz) removes DC offset from tube stage.
 *
 * CPU cost: ~30-40 ops/sample (no oversampling needed -- tube soft-clip
 * via tanh is smooth and well-bandlimited).
 *
 * Reference: docs/guyatone-vtx-tremolo-analysis.md
 */
class VintageTremolo {
public:
    VintageTremolo() {
        buildVcaTable();
    }

    /**
     * Initialize sample-rate-dependent coefficients.
     *
     * Pre-computes LDR attack/release envelope coefficients, LFO phase
     * increment constant, and input/output coupling filter coefficients.
     * Must be called before process() and whenever the sample rate changes.
     *
     * @param sampleRate Device native sample rate in Hz.
     */
    void setSampleRate(int32_t sampleRate) {
        sampleRate_ = sampleRate;
        invSampleRate_ = 1.0f / static_cast<float>(sampleRate);

        // Pre-compute LDR envelope follower coefficients.
        // Attack: LDR resistance drops quickly when illuminated (~15ms)
        // Release: LDR resistance rises slowly when light removed (~100ms)
        // These asymmetric time constants are the hallmark of optical tremolo.
        ldrAttackCoeff_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * kLdrRiseTime));
        ldrReleaseCoeff_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * kLdrFallTime));

        // Input coupling HPF: 22nF + 470K = ~15.3Hz cutoff
        // Models the DC-blocking capacitor at the V1B triode grid input.
        // R = exp(-2*pi*fc/fs) -- sample-rate adaptive (MEMORY.md lesson)
        inputHpfCoeff_ = std::exp(-2.0f * kPi * kInputHpfFreq / static_cast<float>(sampleRate));

        // Output coupling HPF: ~10Hz -- removes DC offset from tube stage
        outputHpfCoeff_ = std::exp(-2.0f * kPi * kOutputHpfFreq / static_cast<float>(sampleRate));

        // Tone filter coefficient will be recomputed per-block via updateToneCoeff()
        updateToneCoeff(tone_.load(std::memory_order_relaxed));
    }

    /**
     * Process a mono audio buffer in-place through the vintage tremolo.
     *
     * Reads all parameters once at block start (control rate), then
     * processes sample-by-sample. The LFO phase, neon lamp state, and
     * LDR envelope are updated per-sample for smooth modulation.
     *
     * @param buffer  Mono float samples, modified in-place.
     * @param numFrames Number of frames (mono, so frames == samples).
     */
    void process(float* buffer, int numFrames) {
        // Read parameters once per block (avoid atomic reads in inner loop)
        const float rate = rate_.load(std::memory_order_relaxed);
        const float depth = depth_.load(std::memory_order_relaxed);
        const float intensity = intensity_.load(std::memory_order_relaxed);
        const float toneParam = tone_.load(std::memory_order_relaxed);

        // Recompute tone filter coefficient if parameter changed
        // (cheap comparison, avoids per-sample trig)
        if (toneParam != cachedToneParam_) {
            updateToneCoeff(toneParam);
        }

        // LFO phase increment for this block
        const float phaseInc = rate * invSampleRate_;

        for (int i = 0; i < numFrames; ++i) {
            float sample = buffer[i];

            // ================================================================
            // STAGE 1: Input coupling HPF (~15Hz)
            // Models the DC-blocking capacitor at the 12AX7 grid input.
            // Standard 1-pole HPF: y[n] = R * (y[n-1] + x[n] - x[n-1])
            // ================================================================
            float hpfOut = inputHpfCoeff_ * (inputHpfPrevOut_ + sample - inputHpfPrevIn_);
            inputHpfPrevIn_ = sample;
            inputHpfPrevOut_ = hpfOut;

            // ================================================================
            // STAGE 2: V1B triode gain stage
            // 12AX7 at ~100-120V B+ provides ~35x gain with soft clipping.
            // Asymmetric characteristic: positive peaks clip earlier due to
            // grid current limiting. Modeled with fast_tanh (Pade [3/3])
            // scaled for the correct drive/output ratio.
            //
            // The tube gain explains the VT-X's +6dB volume boost.
            // ================================================================
            float tubeIn = hpfOut * kTubeGain;
            float tubeOut = fast_math::fast_tanh(tubeIn * kTubeDriveScale) * kTubeOutputScale;

            // Add subtle 2nd harmonic (even-order distortion from single-ended triode).
            // In a real 12AX7 at low plate voltage, 2nd harmonic is ~1-3% of fundamental.
            // tubeOut^2 is always positive, shifting the waveform asymmetrically.
            tubeOut += kSecondHarmonicAmount * tubeOut * tubeOut;

            // ================================================================
            // STAGE 3: Tone filter (1-pole LPF, variable 800Hz-20kHz)
            // Simple RC filter between tube output and optical VCA.
            // ================================================================
            toneState_ += toneAlpha_ * (tubeOut - toneState_);
            float toned = toneState_;

            // ================================================================
            // STAGE 4: LFO with mild oscillator overdrive
            // Phase-shift oscillator naturally overdrives slightly (gain > 29),
            // flattening peaks. Modeled with fast_tanh at 1.5x overdrive.
            // ================================================================
            lfoPhase_ += phaseInc;
            if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;
            float lfoSine = fast_math::sin2pi(lfoPhase_);
            float lfoClipped = fast_math::fast_tanh(lfoSine * kLfoOverdrive);
            // Normalize to [0, 1] for lamp drive
            float lfoNorm = (lfoClipped + 1.0f) * 0.5f;

            // ================================================================
            // STAGE 5: Neon lamp model (NE-2 type threshold device)
            // The neon lamp does NOT linearly convert voltage to light.
            // It is a threshold device with hysteresis:
            //   - Strikes (turns on) above ~70V (normalized: 0.7)
            //   - Extinguishes below ~45V (normalized: 0.45)
            //   - Hysteresis creates different on/off thresholds
            //
            // Intensity parameter scales the LFO drive into the lamp.
            // At low intensity, LFO never reaches strike voltage -> no chop.
            // At high intensity, full neon switching -> vintage choppy tremolo.
            // ================================================================
            float lampDrive = lfoNorm * intensity;
            float neonBrightness;

            if (!neonLit_ && lampDrive > kStrikeThreshold) {
                neonLit_ = true;
            } else if (neonLit_ && lampDrive < kExtinctThreshold) {
                neonLit_ = false;
            }

            if (neonLit_) {
                // Brightness proportional to current above sustaining voltage
                neonBrightness = (lampDrive - kExtinctThreshold) * kNeonBrightnessScale;
                neonBrightness = std::min(neonBrightness, 1.0f);
            } else {
                neonBrightness = 0.0f;
            }

            // ================================================================
            // STAGE 6: CdS LDR model (asymmetric envelope follower)
            // The LDR's key characteristic: fast attack (resistance drops
            // quickly when illuminated, ~15ms) but slow release (resistance
            // rises slowly when light removed, ~100ms). This asymmetry is
            // THE defining sound of optical tremolo.
            // ================================================================
            float coeff = (neonBrightness > ldrState_) ? ldrAttackCoeff_ : ldrReleaseCoeff_;
            ldrState_ += coeff * (neonBrightness - ldrState_);
            ldrState_ = fast_math::denormal_guard(ldrState_);

            // ================================================================
            // STAGE 7: Voltage-divider VCA via pre-computed lookup table
            // Real circuit: R_ldr shunts signal to ground through R_series.
            // VCA gain = R_ldr / (R_series + R_ldr)
            // R_ldr varies logarithmically from ~2M (dark) to ~600 ohm (lit).
            //
            // The 256-point table maps ldrState [0,1] -> VCA gain [0,1],
            // avoiding per-sample exp() calls. Linear interpolation gives
            // smooth output with only ~4 ops per sample.
            // ================================================================
            float vcaGain = lookupVca(ldrState_);

            // Apply depth control: blend between full VCA modulation and unity.
            // depth=0: vcaGain=1.0 (no modulation), depth=1: full VCA effect.
            float effectiveGain = 1.0f - depth * (1.0f - vcaGain);

            float modulated = toned * effectiveGain;

            // ================================================================
            // STAGE 8: Output coupling HPF (~10Hz)
            // Removes DC offset introduced by the tube stage's asymmetric
            // clipping and 2nd harmonic content. Models the output coupling
            // capacitor in the real circuit.
            // ================================================================
            float outHpf = outputHpfCoeff_ * (outputHpfPrevOut_ + modulated - outputHpfPrevIn_);
            outputHpfPrevIn_ = modulated;
            outputHpfPrevOut_ = outHpf;

            buffer[i] = outHpf;
        }
    }

    /**
     * Reset all internal state (filter history, LFO phase, LDR envelope).
     * Called on stream restart or when the effect is re-enabled.
     */
    void reset() {
        inputHpfPrevIn_ = 0.0f;
        inputHpfPrevOut_ = 0.0f;
        outputHpfPrevIn_ = 0.0f;
        outputHpfPrevOut_ = 0.0f;
        toneState_ = 0.0f;
        lfoPhase_ = 0.0f;
        neonLit_ = false;
        ldrState_ = 0.0f;
    }

    // =========================================================================
    // Parameter setters (called from UI thread via setParameter dispatch)
    // =========================================================================

    /** Set LFO rate in Hz. Range: 0.5-20.0 Hz (uses existing tremolo range). */
    void setRate(float hz) {
        rate_.store(std::max(0.5f, std::min(20.0f, hz)), std::memory_order_relaxed);
    }

    /** Set modulation depth. Range: 0.0 (none) to 1.0 (full). */
    void setDepth(float d) {
        depth_.store(std::max(0.0f, std::min(1.0f, d)), std::memory_order_relaxed);
    }

    /** Set tone filter cutoff. Range: 0.0 (dark, 800Hz) to 1.0 (bright, 20kHz). */
    void setTone(float t) {
        tone_.store(std::max(0.0f, std::min(1.0f, t)), std::memory_order_relaxed);
    }

    /**
     * Set neon lamp drive intensity. Range: 0.0 to 1.0.
     *
     * Controls how hard the LFO drives the neon lamp:
     *   - Low (0.0-0.4): LFO never reaches strike voltage, smooth modulation
     *   - Mid (0.4-0.7): Partial neon switching, vintage pulsing character
     *   - High (0.7-1.0): Full neon on/off switching, choppy tremolo
     *
     * This creates the depth-dependent character that makes the VT-X special:
     * subtle settings produce smooth optical tremolo, while cranked settings
     * produce the aggressive "chopped" Fender-style tremolo.
     */
    void setIntensity(float i) {
        intensity_.store(std::max(0.0f, std::min(1.0f, i)), std::memory_order_relaxed);
    }

    float getRate() const { return rate_.load(std::memory_order_relaxed); }
    float getDepth() const { return depth_.load(std::memory_order_relaxed); }
    float getTone() const { return tone_.load(std::memory_order_relaxed); }
    float getIntensity() const { return intensity_.load(std::memory_order_relaxed); }

private:
    // =========================================================================
    // Constants
    // =========================================================================

    static constexpr float kPi = 3.14159265358979323846f;

    // Tube gain stage
    static constexpr float kTubeGain = 35.0f;          // ~30dB voltage gain (12AX7 at 100-120V B+)
    static constexpr float kTubeDriveScale = 0.5f;     // Scale into tanh for smooth clipping
    static constexpr float kTubeOutputScale = 2.0f;     // Compensate tanh [-1,1] range
    static constexpr float kSecondHarmonicAmount = 0.02f; // 2% 2nd harmonic distortion

    // Input/output coupling filters
    static constexpr float kInputHpfFreq = 15.3f;      // 22nF + 470K = 15.3Hz
    static constexpr float kOutputHpfFreq = 10.0f;      // Output coupling cap

    // LFO oscillator overdrive (tube gain > minimum 29 required for oscillation)
    static constexpr float kLfoOverdrive = 1.5f;

    // Neon lamp NE-2 thresholds (normalized to 0-1 range)
    // Strike: lamp ignites when LFO exceeds ~70V (normalized ~0.7)
    // Extinct: lamp extinguishes below ~45V (normalized ~0.45)
    // The ~0.25 hysteresis gap creates the choppy modulation character
    static constexpr float kStrikeThreshold = 0.7f;
    static constexpr float kExtinctThreshold = 0.45f;
    // Pre-computed: 1.0 / (1.0 - kExtinctThreshold)
    static constexpr float kNeonBrightnessScale = 1.0f / (1.0f - kExtinctThreshold);

    // LDR (CdS photoresistor) time constants
    static constexpr float kLdrRiseTime = 0.015f;       // 15ms attack (light on -> R drops fast)
    static constexpr float kLdrFallTime = 0.100f;       // 100ms release (light off -> R rises slow)

    // LDR resistance range (for VCA table computation)
    static constexpr float kRseries = 220000.0f;        // 220K series resistor in voltage divider
    static constexpr float kRdark = 2000000.0f;          // 2M ohm dark resistance
    static constexpr float kRlit = 600.0f;               // 600 ohm illuminated resistance

    // VCA lookup table size (256 points = smooth enough for audio, fits in L1 cache)
    static constexpr int kVcaTableSize = 256;

    // =========================================================================
    // Pre-computed VCA lookup table
    // =========================================================================

    /**
     * Build the VCA lookup table at construction time.
     *
     * Maps LDR light state [0, 1] to voltage divider attenuation.
     * Entry i corresponds to ldrState = i / 255.
     *
     * The logarithmic resistance mapping:
     *   log(R) = log(R_dark) + ldrState * (log(R_lit) - log(R_dark))
     *   R_ldr = exp(log(R))
     *   VCA = R_ldr / (R_series + R_ldr)
     *
     * This avoids per-sample exp() calls -- just linear interpolation in process().
     */
    void buildVcaTable() {
        const float logRdark = std::log(kRdark);
        const float logRlit = std::log(kRlit);
        const float logRange = logRlit - logRdark;

        for (int i = 0; i < kVcaTableSize; ++i) {
            float ldrState = static_cast<float>(i) / static_cast<float>(kVcaTableSize - 1);
            float logR = logRdark + ldrState * logRange;
            float R_ldr = std::exp(logR);
            float vca = R_ldr / (kRseries + R_ldr);
            vcaTable_[i] = vca;
        }
    }

    /**
     * Linearly interpolate the VCA table for a given LDR state.
     *
     * @param ldrState Current LDR light level in [0, 1].
     * @return VCA attenuation factor in [~0.003, ~0.9].
     */
    float lookupVca(float ldrState) const {
        float idx = ldrState * static_cast<float>(kVcaTableSize - 1);
        int i0 = static_cast<int>(idx);
        if (i0 >= kVcaTableSize - 1) return vcaTable_[kVcaTableSize - 1];
        if (i0 < 0) return vcaTable_[0];
        float frac = idx - static_cast<float>(i0);
        return vcaTable_[i0] + frac * (vcaTable_[i0 + 1] - vcaTable_[i0]);
    }

    /**
     * Recompute the tone filter coefficient from the tone parameter.
     *
     * Cutoff sweeps logarithmically from 800Hz (dark) to 20kHz (bright):
     *   freq = 800 * 25^tone  (800Hz at tone=0, 20kHz at tone=1)
     *
     * Uses a 1-pole LPF: alpha = w / (1 + w) where w = 2*pi*fc/fs.
     * This is a simple, efficient model of the VT-X's RC tone control.
     */
    void updateToneCoeff(float toneParam) {
        cachedToneParam_ = toneParam;
        // 800Hz * 25^tone = 800Hz to 20kHz logarithmic sweep
        float freq = 800.0f * std::pow(25.0f, toneParam);
        float w = 2.0f * kPi * freq / static_cast<float>(sampleRate_);
        toneAlpha_ = w / (1.0f + w);
    }

    // =========================================================================
    // State variables
    // =========================================================================

    // Parameters (atomic for lock-free UI thread -> audio thread communication)
    std::atomic<float> rate_{4.0f};        // LFO rate in Hz
    std::atomic<float> depth_{0.5f};       // Modulation depth
    std::atomic<float> tone_{0.75f};       // Tone filter (0=dark, 1=bright)
    std::atomic<float> intensity_{0.7f};   // Neon lamp drive intensity

    // Sample rate
    int32_t sampleRate_ = 44100;
    float invSampleRate_ = 1.0f / 44100.0f;

    // Input coupling HPF state (~15Hz, models grid coupling capacitor)
    float inputHpfCoeff_ = 0.0f;
    float inputHpfPrevIn_ = 0.0f;
    float inputHpfPrevOut_ = 0.0f;

    // Output coupling HPF state (~10Hz, models output coupling capacitor)
    float outputHpfCoeff_ = 0.0f;
    float outputHpfPrevIn_ = 0.0f;
    float outputHpfPrevOut_ = 0.0f;

    // Tone filter state (1-pole LPF, variable 800Hz-20kHz)
    float toneAlpha_ = 0.0f;
    float toneState_ = 0.0f;
    float cachedToneParam_ = -1.0f;   // Force recompute on first block

    // LFO state
    float lfoPhase_ = 0.0f;

    // Neon lamp state (hysteresis: separate strike/extinct thresholds)
    bool neonLit_ = false;

    // LDR envelope follower state (asymmetric attack/release)
    float ldrState_ = 0.0f;
    float ldrAttackCoeff_ = 0.0f;
    float ldrReleaseCoeff_ = 0.0f;

    // Pre-computed VCA lookup table (ldrState -> voltage divider attenuation)
    float vcaTable_[kVcaTableSize] = {};
};

#endif // GUITAR_ENGINE_VINTAGE_TREMOLO_H
