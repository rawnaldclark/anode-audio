#ifndef GUITAR_ENGINE_ROTARY_SPEAKER_H
#define GUITAR_ENGINE_ROTARY_SPEAKER_H

#include "effect.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * Leslie-type rotary speaker effect.
 *
 * Emulates the mechanical rotating speaker cabinet originally designed by
 * Don Leslie in 1941. The real Leslie 122/147 contains two rotating elements:
 *
 *   1. **Horn rotor** (treble): A two-mouthed horn driven by a compression
 *      driver, spinning above a crossover point (~800Hz). The horn's rotation
 *      creates both Doppler pitch shift (frequency modulation) and amplitude
 *      modulation as the sound source moves toward and away from the listener.
 *
 *   2. **Drum rotor** (bass): A rotating baffle around a stationary 15" woofer,
 *      spinning below the crossover point. The drum is heavier and accelerates/
 *      decelerates more slowly than the horn, creating the characteristic
 *      "Leslie lurch" when switching speeds.
 *
 * The Leslie has two speed settings controlled by a switch:
 *   - **Chorale** (slow): Horn ~0.83Hz, Drum ~0.67Hz -- gentle, subtle motion
 *   - **Tremolo** (fast): Horn ~6.75Hz, Drum ~5.83Hz -- intense, swirling sound
 *
 * The signature Leslie character comes from the speed TRANSITION: both rotors
 * use different acceleration/deceleration rates (horn is lighter, responds
 * faster), so during a speed change the two rotors are temporarily at different
 * speeds, creating complex intermodulation.
 *
 * Signal flow:
 *   Input -> 1st-order crossover (800Hz, -6dB/oct)
 *         -> Low band  -> Drum delay line (Doppler + AM) -+
 *         -> High band -> Horn delay line (Doppler + AM) -+-> Cabinet LP -> Mix -> Output
 *
 * DSP implementation:
 *   - Crossover: 1st-order LP/HP (~6dB/oct slopes) for wide band overlap
 *     matching the simple passive crossover in a real Leslie 122/147
 *   - Doppler: Modulated delay line with linear interpolation (same pattern as
 *     chorus.h) models the distance change as the rotor sweeps toward/away
 *   - AM: Asymmetric sinusoidal amplitude variation with 90-degree phase
 *     offset from Doppler, modeling the directional radiation pattern of
 *     the rotating horn/drum openings
 *   - Speed ramp: Per-sample exponential approach to target speed, with
 *     different time constants for acceleration vs deceleration
 *   - Cabinet coloration: One-pole LP at ~4kHz models the wooden cabinet
 *
 * Parameter IDs for JNI access:
 *   0 = speed  (0.0 to 1.0) -- 0=Chorale(slow), 1=Tremolo(fast), threshold 0.5
 *   1 = mix    (0.0 to 1.0) -- internal wet/dry blend (0=dry, 1=full rotary)
 *   2 = depth  (0.0 to 1.0) -- modulation intensity multiplier
 */
class RotarySpeaker : public Effect {
public:
    RotarySpeaker() = default;

    /**
     * Process audio through the rotary speaker simulation. Real-time safe.
     *
     * Per-sample algorithm:
     *   1. Split input through 1st-order crossover into low/high bands
     *   2. Ramp horn and drum rotor speeds toward target (exponential approach)
     *   3. Advance horn and drum LFO phases at current (ramped) speed
     *   4. Write band signals into respective delay buffers
     *   5. Read from delay buffers at modulated positions (Doppler)
     *   6. Apply asymmetric amplitude modulation to each band (90deg offset)
     *   7. Recombine bands, apply cabinet LP, apply mix, clamp output
     *
     * @param buffer   Mono audio samples in [-1.0, 1.0].
     * @param numFrames Number of frames in the buffer.
     */
    void process(float* buffer, int numFrames) override {
        if (hornBufSize_ == 0) return;  // setSampleRate() not yet called

        // Snapshot parameters at block boundaries (lock-free atomic reads)
        const float speedParam = speed_.load(std::memory_order_relaxed);
        const float mix = mix_.load(std::memory_order_relaxed);
        const float depth = depth_.load(std::memory_order_relaxed);

        // Determine target speeds based on speed switch position
        const bool fast = (speedParam >= 0.5f);
        const float hornTarget = fast ? kHornFastHz : kHornSlowHz;
        const float drumTarget = fast ? kDrumFastHz : kDrumSlowHz;

        // Select ramp coefficients based on acceleration vs deceleration.
        // The horn is lighter and responds faster than the heavy drum baffle.
        // Acceleration (slow->fast) is quicker than deceleration (fast->slow)
        // for both rotors, matching the mechanical inertia asymmetry.
        const float hornRamp = (hornTarget > hornSpeed_)
            ? hornAccelCoeff_ : hornDecelCoeff_;
        const float drumRamp = (drumTarget > drumSpeed_)
            ? drumAccelCoeff_ : drumDecelCoeff_;

        // Pre-compute Doppler depth in samples
        const float hornDopplerDepth = depth * kHornDopplerDepthSec
                                     * static_cast<float>(sampleRate_);
        const float drumDopplerDepth = depth * kDrumDopplerDepthSec
                                     * static_cast<float>(sampleRate_);

        const float invSampleRate = 1.0f / static_cast<float>(sampleRate_);

        for (int i = 0; i < numFrames; ++i) {
            const float input = buffer[i];

            // =================================================================
            // Stage 1: 1st-order crossover filter (800Hz, -6dB/oct slopes)
            // =================================================================
            // Real Leslie used a simple passive crossover creating wide overlap
            // between horn and drum bands -- this blend is essential to the sound.
            lpState1_ += static_cast<float>(lpCoeff_) * (input - lpState1_);
            lpState1_ = fast_math::denormal_guard(lpState1_);
            float lowBand = lpState1_;
            float highBand = input - lowBand;

            // =================================================================
            // Stage 2: Exponential speed ramping (per-sample)
            // =================================================================
            // Each sample, the current speed moves toward the target by a
            // fraction determined by the ramp coefficient:
            //   speed += (target - speed) * coeff
            // This creates an exponential approach curve that models the
            // mechanical inertia of the rotating masses.
            hornSpeed_ += (hornTarget - hornSpeed_) * hornRamp;
            drumSpeed_ += (drumTarget - drumSpeed_) * drumRamp;

            // =================================================================
            // Stage 3: Advance LFO phases at current rotor speeds
            // =================================================================
            // Phase increments use the current (ramped) speed, so during a
            // speed transition the LFO smoothly accelerates/decelerates.
            hornPhase_ += hornSpeed_ * invSampleRate;
            if (hornPhase_ >= 1.0f) hornPhase_ -= 1.0f;

            drumPhase_ += drumSpeed_ * invSampleRate;
            if (drumPhase_ >= 1.0f) drumPhase_ -= 1.0f;

            // Doppler uses sin(phase) - modulates delay position
            // Two-mouthed horn geometry: 12% 2nd harmonic creates subtle double-bump
            const float hornLfo = fast_math::sin2pi(hornPhase_)
                                + 0.12f * fast_math::sin2pi(2.0f * hornPhase_);
            const float drumLfo = fast_math::sin2pi(drumPhase_);

            // AM uses sin(phase + 0.25) = cos(phase) -- 90 degrees offset from Doppler.
            // In reality, AM peaks when source is closest (position), while
            // Doppler peaks when velocity is maximum (90 degrees ahead of position).
            const float hornAMLfo = fast_math::sin2pi(hornPhase_ + 0.25f);
            const float drumAMLfo = fast_math::sin2pi(drumPhase_ + 0.25f);

            // =================================================================
            // Stage 4: Horn rotor (treble band) - Doppler delay + AM
            // =================================================================
            // Write high band to horn delay buffer
            hornBuffer_[hornWritePos_] = highBand;

            // Modulated delay read position (Doppler effect)
            // Center delay ~1.5ms, modulated by +/- hornDopplerDepth
            float hornDelaySamples = hornCenterDelay_
                                   + hornLfo * hornDopplerDepth;
            hornDelaySamples = std::max(1.0f, hornDelaySamples);

            // Linear interpolation readback from horn delay buffer
            float hornReadF = static_cast<float>(hornWritePos_)
                            - hornDelaySamples;
            if (hornReadF < 0.0f)
                hornReadF += static_cast<float>(hornBufSize_);
            int hornReadInt = static_cast<int>(hornReadF);
            float hornFrac = hornReadF - static_cast<float>(hornReadInt);
            int hornIdx0 = hornReadInt & hornBufMask_;
            int hornIdx1 = (hornReadInt + 1) & hornBufMask_;
            float hornWet = hornBuffer_[hornIdx0] * (1.0f - hornFrac)
                          + hornBuffer_[hornIdx1] * hornFrac;

            // Asymmetric AM: lerp between 1.0 (no mod) and full range [0.15, 1.0]
            // Horn mouth is highly directional - much louder facing toward listener
            // than away. hornAMLfo is 90deg offset from Doppler for realism.
            float hornAMFull = kHornAMMid + hornAMLfo * kHornAMSwing;
            hornWet *= 1.0f + depth * (hornAMFull - 1.0f);

            hornWritePos_ = (hornWritePos_ + 1) & hornBufMask_;

            // =================================================================
            // Stage 5: Drum rotor (bass band) - Doppler delay + AM
            // =================================================================
            // Write low band to drum delay buffer
            drumBuffer_[drumWritePos_] = lowBand;

            // Modulated delay read position (Doppler effect)
            // Center delay ~2.0ms (longer than horn -- larger physical element)
            float drumDelaySamples = drumCenterDelay_
                                   + drumLfo * drumDopplerDepth;
            drumDelaySamples = std::max(1.0f, drumDelaySamples);

            // Linear interpolation readback from drum delay buffer
            float drumReadF = static_cast<float>(drumWritePos_)
                            - drumDelaySamples;
            if (drumReadF < 0.0f)
                drumReadF += static_cast<float>(drumBufSize_);
            int drumReadInt = static_cast<int>(drumReadF);
            float drumFrac = drumReadF - static_cast<float>(drumReadInt);
            int drumIdx0 = drumReadInt & drumBufMask_;
            int drumIdx1 = (drumReadInt + 1) & drumBufMask_;
            float drumWet = drumBuffer_[drumIdx0] * (1.0f - drumFrac)
                          + drumBuffer_[drumIdx1] * drumFrac;

            // Asymmetric AM for drum rotor. Drum baffle is less directional
            // than the horn, so the AM range is narrower [0.5, 1.0].
            float drumAMFull = kDrumAMMid + drumAMLfo * kDrumAMSwing;
            drumWet *= 1.0f + depth * (drumAMFull - 1.0f);

            drumWritePos_ = (drumWritePos_ + 1) & drumBufMask_;

            // =================================================================
            // Stage 6: Recombine bands, cabinet coloration, and apply mix
            // =================================================================
            // Sum the processed horn and drum bands back together.
            float wet = hornWet + drumWet;

            // Cabinet coloration: gentle HF rolloff models wooden Leslie cabinet
            cabinetLpState_ += static_cast<float>(cabinetLpCoeff_) * (wet - cabinetLpState_);
            cabinetLpState_ = fast_math::denormal_guard(cabinetLpState_);
            const float wetFiltered = cabinetLpState_;

            // Apply internal wet/dry mix (separate from the base class
            // wet/dry mix handled by SignalChain).
            const float output = input * (1.0f - mix) + wetFiltered * mix;

            buffer[i] = clamp(output, -1.0f, 1.0f);
        }
    }

    /**
     * Configure sample rate and pre-compute all coefficients.
     *
     * Pre-computes:
     *   - 1st-order crossover LP coefficient (800Hz)
     *   - Horn and drum center delay times in samples
     *   - Speed ramp coefficients for accel/decel per rotor
     *   - Delay buffer allocations (power-of-two sized)
     *   - Cabinet coloration LP coefficient (4kHz)
     *
     * This is the ONLY place where allocations and expensive math occur.
     * Called once during stream setup, before any process() calls.
     *
     * @param sampleRate Device native sample rate in Hz.
     */
    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;
        const double fs = static_cast<double>(sampleRate);

        // =================================================================
        // Crossover filter coefficient (1st-order at 800Hz)
        // =================================================================
        // A single one-pole LP/HP pair giving -6dB/oct slopes and wide
        // overlap between horn and drum bands, matching the simple passive
        // crossover in a real Leslie 122/147.
        // Coefficient: alpha = 1 - exp(-2 * pi * fc / fs)
        lpCoeff_ = 1.0 - std::exp(-2.0 * kPi * kCrossoverFreqHz / fs);

        // =================================================================
        // Delay buffer allocation
        // =================================================================
        // Horn: center 1.5ms + depth 0.3ms + margin = ~5ms buffer
        // Drum: center 2.0ms + depth 0.15ms + margin = ~10ms buffer
        // Generous sizing prevents any possibility of read-before-write.
        int hornMinSize = static_cast<int>(0.005f * sampleRate) + 1;
        hornBufSize_ = nextPowerOfTwo(hornMinSize);
        hornBufMask_ = hornBufSize_ - 1;
        hornBuffer_.assign(hornBufSize_, 0.0f);
        hornWritePos_ = 0;

        int drumMinSize = static_cast<int>(0.010f * sampleRate) + 1;
        drumBufSize_ = nextPowerOfTwo(drumMinSize);
        drumBufMask_ = drumBufSize_ - 1;
        drumBuffer_.assign(drumBufSize_, 0.0f);
        drumWritePos_ = 0;

        // =================================================================
        // Center delay times in samples
        // =================================================================
        hornCenterDelay_ = kHornCenterDelaySec * static_cast<float>(sampleRate);
        drumCenterDelay_ = kDrumCenterDelaySec * static_cast<float>(sampleRate);

        // =================================================================
        // Speed ramp coefficients (exponential approach per sample)
        // =================================================================
        // For a time constant T (seconds), the per-sample coefficient is:
        //   alpha = 1 - exp(-1 / (T * fs))
        // This gives ~63% of the transition in T seconds, ~95% in 3T.
        //
        // Horn (lighter, faster response):
        //   Accel (slow->fast): tau = 0.7s  -> ~2.1s to 95% (3 * 0.7)
        //   Decel (fast->slow): tau = 1.3s  -> ~3.9s to 95% (3 * 1.3)
        //
        // Drum (massive baffle, very slow response):
        //   Accel (slow->fast): tau = 4.5s  -> ~13.5s to 95% (3 * 4.5)
        //   Decel (fast->slow): tau = 5.5s  -> ~16.5s to 95% (3 * 5.5)
        hornAccelCoeff_ = static_cast<float>(
            1.0 - std::exp(-1.0 / (kHornAccelTau * fs)));
        hornDecelCoeff_ = static_cast<float>(
            1.0 - std::exp(-1.0 / (kHornDecelTau * fs)));
        drumAccelCoeff_ = static_cast<float>(
            1.0 - std::exp(-1.0 / (kDrumAccelTau * fs)));
        drumDecelCoeff_ = static_cast<float>(
            1.0 - std::exp(-1.0 / (kDrumDecelTau * fs)));

        // =================================================================
        // Cabinet coloration LP coefficient (4kHz)
        // =================================================================
        cabinetLpCoeff_ = 1.0 - std::exp(-2.0 * kPi * kCabinetLPFreqHz / fs);

        // Initialize rotor speeds to slow (Chorale) position
        hornSpeed_ = kHornSlowHz;
        drumSpeed_ = kDrumSlowHz;
    }

    /**
     * Reset all internal state to silence.
     * Clears delay buffers, filter state, and LFO phases.
     * Called when the stream restarts to prevent artifacts from stale state.
     */
    void reset() override {
        std::fill(hornBuffer_.begin(), hornBuffer_.end(), 0.0f);
        std::fill(drumBuffer_.begin(), drumBuffer_.end(), 0.0f);
        hornWritePos_ = 0;
        drumWritePos_ = 0;

        // Reset crossover filter state
        lpState1_ = 0.0f;

        // Reset cabinet LP state
        cabinetLpState_ = 0.0f;

        // Reset LFO phases (horn and drum start at different phases to
        // avoid correlated modulation which would sound unnatural)
        hornPhase_ = 0.0f;
        drumPhase_ = 0.25f;   // 90 degrees offset from horn

        // Reset rotor speeds to slow (Chorale)
        hornSpeed_ = kHornSlowHz;
        drumSpeed_ = kDrumSlowHz;
    }

    /**
     * Set a parameter by ID. Thread-safe (atomic store).
     * @param paramId  0=speed, 1=mix, 2=depth
     * @param value    Parameter value (clamped to valid range).
     */
    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0: speed_.store(clamp(value, 0.0f, 1.0f),
                                 std::memory_order_relaxed); break;
            case 1: mix_.store(clamp(value, 0.0f, 1.0f),
                               std::memory_order_relaxed); break;
            case 2: depth_.store(clamp(value, 0.0f, 1.0f),
                                 std::memory_order_relaxed); break;
            default: break;
        }
    }

    /**
     * Get a parameter by ID. Thread-safe (atomic load).
     * @param paramId  0=speed, 1=mix, 2=depth
     * @return Current parameter value, or 0.0f if paramId is unknown.
     */
    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return speed_.load(std::memory_order_relaxed);
            case 1: return mix_.load(std::memory_order_relaxed);
            case 2: return depth_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    // =========================================================================
    // Physical constants modeled from Leslie 122/147 measurements
    // =========================================================================

    static constexpr double kPi = 3.14159265358979323846;

    /** Crossover frequency between horn and drum (Hz). */
    static constexpr double kCrossoverFreqHz = 800.0;

    /** Cabinet coloration: gentle HF rolloff frequency (Hz). */
    static constexpr double kCabinetLPFreqHz = 4000.0;

    // -- Horn rotor speeds (Hz) -----------------------------------------------
    // The horn is lighter than the drum and spins faster at both speed settings.
    /** Chorale (slow) horn rotation speed. */
    static constexpr float kHornSlowHz = 0.83f;
    /** Tremolo (fast) horn rotation speed. */
    static constexpr float kHornFastHz = 6.75f;

    // -- Drum rotor speeds (Hz) -----------------------------------------------
    // The drum baffle is massive and spins slower than the horn.
    /** Chorale (slow) drum rotation speed. */
    static constexpr float kDrumSlowHz = 0.67f;
    /** Tremolo (fast) drum rotation speed. */
    static constexpr float kDrumFastHz = 5.83f;

    // -- Speed ramp time constants (seconds) -----------------------------------
    // These are the tau values for the exponential ramp. 3*tau gives ~95%.
    // The drum baffle is massive and takes 4-5 seconds to reach full speed.
    /** Horn acceleration tau: ~2.1s to 95% (3 * 0.7). */
    static constexpr double kHornAccelTau = 0.7;
    /** Horn deceleration tau: ~3.9s to 95% (3 * 1.3). */
    static constexpr double kHornDecelTau = 1.3;
    /** Drum acceleration tau: ~13.5s to 95% (3 * 4.5). */
    static constexpr double kDrumAccelTau = 4.5;
    /** Drum deceleration tau: ~16.5s to 95% (3 * 5.5). */
    static constexpr double kDrumDecelTau = 5.5;

    // -- Doppler delay parameters (seconds) ------------------------------------
    /** Horn center delay time: ~1.5ms. */
    static constexpr float kHornCenterDelaySec = 0.0015f;
    /** Horn Doppler depth at full intensity: +/- 0.3ms. */
    static constexpr float kHornDopplerDepthSec = 0.00035f;
    /** Drum center delay time: ~2.0ms. */
    static constexpr float kDrumCenterDelaySec = 0.002f;
    /** Drum Doppler depth at full intensity: +/- 0.15ms (drum barely moves). */
    static constexpr float kDrumDopplerDepthSec = 0.00015f;

    // -- Amplitude modulation (asymmetric) ------------------------------------
    // Real Leslie AM is NOT symmetric around 1.0. The horn mouth directionality
    // means the signal drops much more when facing away than it exceeds when
    // facing toward the listener.

    // Horn AM: asymmetric [0.15, 1.0] at full depth - horn mouth is highly directional
    static constexpr float kHornAMMid = 0.575f;    // (0.15 + 1.0) / 2
    static constexpr float kHornAMSwing = 0.425f;   // (1.0 - 0.15) / 2

    // Drum AM: asymmetric [0.5, 1.0] at full depth - drum baffle is less directional
    static constexpr float kDrumAMMid = 0.75f;     // (0.5 + 1.0) / 2
    static constexpr float kDrumAMSwing = 0.25f;    // (1.0 - 0.5) / 2

    // =========================================================================
    // Internal helpers
    // =========================================================================

    /**
     * Round up to the next power of two.
     * Used to size circular buffers for efficient bitmask wrapping.
     * @param n Minimum required size.
     * @return Smallest power of two >= n.
     */
    static int nextPowerOfTwo(int n) {
        int p = 1; while (p < n) p <<= 1; return p;
    }

    /**
     * Clamp a value to a range.
     * @param v   Value to clamp.
     * @param lo  Minimum allowed value.
     * @param hi  Maximum allowed value.
     * @return Clamped value in [lo, hi].
     */
    static float clamp(float v, float lo, float hi) {
        return std::max(lo, std::min(hi, v));
    }

    // =========================================================================
    // Parameters (written from UI thread, read from audio thread)
    // =========================================================================

    /** Speed switch: 0.0=Chorale(slow), 1.0=Tremolo(fast), threshold 0.5. */
    std::atomic<float> speed_{0.0f};

    /** Internal wet/dry mix: 0.0=fully dry, 1.0=full rotary effect. */
    std::atomic<float> mix_{1.0f};

    /** Modulation intensity multiplier: 0.0=no modulation, 1.0=full depth. */
    std::atomic<float> depth_{0.7f};

    // =========================================================================
    // Audio thread state (only accessed from the audio callback)
    // =========================================================================

    int32_t sampleRate_ = 44100;

    // -- Crossover filter state -----------------------------------------------
    // 1st-order LP/HP crossover (-6dB/oct). lpCoeff_ is the one-pole LP
    // coefficient, pre-computed in setSampleRate().
    double lpCoeff_ = 0.0;
    float lpState1_ = 0.0f;   // 1st-order LP section state

    // -- Cabinet coloration LP state ------------------------------------------
    float cabinetLpState_ = 0.0f;
    double cabinetLpCoeff_ = 0.0;

    // -- Horn rotor state -----------------------------------------------------
    std::vector<float> hornBuffer_;
    int hornBufSize_ = 0;
    int hornBufMask_ = 0;
    int hornWritePos_ = 0;
    float hornCenterDelay_ = 0.0f;   // Center delay in samples
    float hornPhase_ = 0.0f;         // LFO phase [0, 1)
    float hornSpeed_ = kHornSlowHz;  // Current (ramped) rotation speed (Hz)

    // -- Drum rotor state -----------------------------------------------------
    std::vector<float> drumBuffer_;
    int drumBufSize_ = 0;
    int drumBufMask_ = 0;
    int drumWritePos_ = 0;
    float drumCenterDelay_ = 0.0f;   // Center delay in samples
    float drumPhase_ = 0.25f;        // LFO phase [0, 1), offset from horn
    float drumSpeed_ = kDrumSlowHz;  // Current (ramped) rotation speed (Hz)

    // -- Speed ramp coefficients (pre-computed in setSampleRate) ---------------
    float hornAccelCoeff_ = 0.0f;
    float hornDecelCoeff_ = 0.0f;
    float drumAccelCoeff_ = 0.0f;
    float drumDecelCoeff_ = 0.0f;
};

#endif // GUITAR_ENGINE_ROTARY_SPEAKER_H
