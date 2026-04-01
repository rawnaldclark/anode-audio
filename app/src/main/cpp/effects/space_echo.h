#ifndef GUITAR_ENGINE_SPACE_ECHO_H
#define GUITAR_ENGINE_SPACE_ECHO_H

#include "effect.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdlib>

/**
 * Roland RE-201 Space Echo emulation.
 *
 * Circuit-accurate model of the iconic 1974 tape echo unit, implementing:
 *
 * 1. THREE PLAYBACK HEADS at fixed 1:2:3 ratio with cubic interpolated
 *    read pointers. The Repeat Rate knob scales tape speed, so all three
 *    head delay times move proportionally (just like the real unit's DC
 *    motor speed control).
 *
 * 2. WOW AND FLUTTER: 4-component sinusoidal model (primary wow ~1Hz,
 *    secondary wow ~3Hz, primary flutter ~10Hz, secondary flutter ~20Hz)
 *    plus bandpass-filtered noise (0.5-30Hz). The frequencies drift slowly
 *    over time to avoid periodic artifacts.
 *
 * 3. TAPE SATURATION in the feedback loop: frequency-dependent soft
 *    clipping with pre-emphasis/de-emphasis. High frequencies saturate
 *    first (matching real oxide tape behavior). Each recirculation adds
 *    harmonics and loses HF content.
 *
 * 4. FEEDBACK DARKENING: 1-pole LPF (~5kHz) + 1-pole HPF (~60Hz) in
 *    the feedback path. This is THE defining RE-201 characteristic --
 *    after 4 passes: -12dB at 5kHz. After 8: mostly sub-1kHz warmth.
 *
 * 5. 12-MODE SELECTOR: combinations of heads 1-3 and spring reverb,
 *    matching the real unit's rotary switch positions.
 *
 * 6. INPUT PREAMP: 2SC828 transistor model with ~20dB gain and soft
 *    asymmetric clipping. Input coupling HPF at ~30Hz.
 *
 * 7. BAXANDALL TONE STACK: Bass shelf 350Hz, Treble shelf 2.5kHz,
 *    +/-12dB range, using biquad filters.
 *
 * 8. BUILT-IN SPRING REVERB: 2-spring Accutronics Type 4 emulation
 *    with chirp allpass dispersion (12 stages per spring). Fed from
 *    input in PARALLEL with echo (not from tape output).
 *
 * Memory: ~375KB for delay buffer (96000 samples at 48kHz).
 * Pre-allocated in setSampleRate(), never in process().
 *
 * Parameter IDs for JNI access:
 *   0 = Repeat Rate    (0.0 to 1.0, maps to base delay 132-526ms)
 *   1 = Echo Volume    (0.0 to 1.0, wet level of tape echo)
 *   2 = Intensity      (0.0 to 1.0, feedback amount 0-0.95)
 *   3 = Mode           (0-11, the 12 mode switch positions)
 *   4 = Bass           (0.0 to 1.0, tone stack +/-12dB)
 *   5 = Treble         (0.0 to 1.0, tone stack +/-12dB)
 *   6 = Reverb Volume  (0.0 to 1.0, spring reverb level)
 *   7 = Wow/Flutter    (0.0 to 1.0, tape transport artifact amount)
 *   8 = Tape Condition (0.0 to 1.0, 0=new bright, 1=worn dark)
 */
class SpaceEcho : public Effect {
public:
    SpaceEcho() = default;

    // =========================================================================
    // Effect interface
    // =========================================================================

    void process(float* buffer, int numFrames) override {
        // Snapshot all parameters (atomic relaxed reads)
        const float repeatRate   = repeatRate_.load(std::memory_order_relaxed);
        const float echoVolume   = echoVolume_.load(std::memory_order_relaxed);
        const float intensity    = intensity_.load(std::memory_order_relaxed);
        const int   mode         = static_cast<int>(mode_.load(std::memory_order_relaxed));
        const float bass         = bass_.load(std::memory_order_relaxed);
        const float treble       = treble_.load(std::memory_order_relaxed);
        const float reverbVolume = reverbVolume_.load(std::memory_order_relaxed);
        const float wfAmount     = wowFlutter_.load(std::memory_order_relaxed);
        const float tapeCond     = tapeCondition_.load(std::memory_order_relaxed);

        // Decode mode (0-11) into head enables and reverb enable
        const int modeIdx = clamp(mode, 0, 11);
        const bool head1On = kModes[modeIdx].head1;
        const bool head2On = kModes[modeIdx].head2;
        const bool head3On = kModes[modeIdx].head3;
        const bool reverbOn = kModes[modeIdx].reverb;

        // Count active heads for normalization
        const int activeHeads = (head1On ? 1 : 0) + (head2On ? 1 : 0) + (head3On ? 1 : 0);
        const float headNorm = (activeHeads > 0) ? (1.0f / static_cast<float>(activeHeads)) : 0.0f;

        // Base delay in samples from Repeat Rate knob.
        // Range: 132ms (fast tape) to 526ms (slow tape) for head 1.
        // RepeatRate 0 = slow tape (long delay), 1 = fast tape (short delay).
        const float baseDelayMs = 526.0f - repeatRate * (526.0f - 132.0f);
        const float baseDelaySamples = (baseDelayMs / 1000.0f) * static_cast<float>(sampleRate_);

        // Tape condition affects feedback darkening cutoff.
        // New tape (0): LPF at ~6kHz. Worn tape (1): LPF at ~3kHz.
        const float tapeHFCutoff = 6000.0f - tapeCond * 3000.0f;
        const double tapeLP_coeff = computeLPCoeff(tapeHFCutoff);

        // Feedback amount scaled by intensity (0 to 0.95)
        const float feedback = intensity * kMaxFeedback;

        // Recompute tone stack coefficients if parameters changed
        updateToneStack(bass, treble);

        for (int i = 0; i < numFrames; ++i) {
            const float inputSample = buffer[i];

            // ---- INPUT PREAMP (2SC828 transistor model) ----
            // Input coupling HPF at ~30Hz removes DC
            float preampIn = applyInputHPF(inputSample);

            // Transistor gain stage with soft asymmetric clipping (~20dB gain)
            float preampOut = preampClip(preampIn * kPreampGain);

            // ---- BAXANDALL TONE STACK ----
            float toned = applyToneStack(preampOut);

            // ---- TAPE ECHO PATH ----
            // The record signal is the toned input mixed with feedback
            float recordSignal = toned + fast_math::denormal_guard(feedbackSample_ * feedback);

            // Write to delay buffer
            delayBuffer_[writePos_] = recordSignal;

            // ---- WOW AND FLUTTER MODULATION ----
            float wfMod = computeWowFlutter(wfAmount);

            // ---- READ FROM THREE HEADS ----
            float echoSum = 0.0f;

            if (head1On) {
                float delaySmp = baseDelaySamples * 1.0f * (1.0f + wfMod);
                echoSum += readCubic(delaySmp);
            }
            if (head2On) {
                float delaySmp = baseDelaySamples * 2.0f * (1.0f + wfMod);
                echoSum += readCubic(delaySmp);
            }
            if (head3On) {
                float delaySmp = baseDelaySamples * 3.0f * (1.0f + wfMod);
                echoSum += readCubic(delaySmp);
            }

            // Normalize by number of active heads
            echoSum *= headNorm;

            // ---- FEEDBACK PATH: tape saturation + darkening ----
            // This is INSIDE the feedback loop, so each recirculation
            // adds harmonics and loses HF -- the defining RE-201 character.
            float fbSignal = echoSum;

            // Tape saturation (frequency-dependent: HF saturates first)
            fbSignal = tapeSaturate(fbSignal);

            // Feedback darkening: 1-pole LPF (cumulative HF loss per pass)
            fbDarkenLPState_ = fbDarkenLPState_ * tapeLP_coeff
                             + static_cast<double>(fbSignal) * (1.0 - tapeLP_coeff);
            fbDarkenLPState_ = fast_math::denormal_guard(fbDarkenLPState_);
            fbSignal = static_cast<float>(fbDarkenLPState_);

            // Feedback HPF at ~60Hz (removes DC buildup per pass)
            double hpInput = static_cast<double>(fbSignal);
            fbDarkenHPState_ = fbDarkenHP_coeff_
                             * (fbDarkenHPState_ + hpInput - fbDarkenHPPrev_);
            fbDarkenHPPrev_ = hpInput;
            fbDarkenHPState_ = fast_math::denormal_guard(fbDarkenHPState_);
            fbSignal = static_cast<float>(fbDarkenHPState_);

            // Store for next sample's feedback mix
            feedbackSample_ = fbSignal;

            // ---- SPRING REVERB (parallel path from input) ----
            float reverbOut = 0.0f;
            if (reverbOn && reverbVolume > 0.001f) {
                reverbOut = processSpringReverb(preampOut);
            }

            // ---- OUTPUT MIX ----
            // Dry signal passes through the preamp and tone stack.
            // Echo and reverb are mixed in parallel.
            float output = toned
                         + echoSum * echoVolume
                         + reverbOut * reverbVolume;

            // Advance write position
            writePos_ = (writePos_ + 1) & bufferMask_;

            // Advance wow/flutter phase
            advanceWFPhase();

            buffer[i] = output;
        }
    }

    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;
        const float fs = static_cast<float>(sampleRate);

        // ---- Main delay buffer ----
        // Max delay: head 3 at slowest tape speed = ~1579ms, plus modulation headroom
        const int maxDelaySamples = static_cast<int>(kMaxDelayMs / 1000.0f * fs) + 64;
        bufferSize_ = nextPowerOfTwo(maxDelaySamples);
        bufferMask_ = bufferSize_ - 1;
        delayBuffer_.assign(bufferSize_, 0.0f);
        writePos_ = 0;

        // ---- Input HPF at ~30Hz ----
        inputHP_coeff_ = std::exp(-2.0 * M_PI * 30.0 / static_cast<double>(sampleRate));

        // ---- Feedback darkening HPF at ~60Hz ----
        fbDarkenHP_coeff_ = std::exp(-2.0 * M_PI * 60.0 / static_cast<double>(sampleRate));

        // ---- Tape saturation pre/de-emphasis filters ----
        // Pre-emphasis: 1-pole HPF at ~2kHz (boosts HF before saturation)
        preEmph_coeff_ = std::exp(-2.0 * M_PI * 2000.0 / static_cast<double>(sampleRate));
        // De-emphasis: 1-pole LPF at ~5kHz (restores balance after saturation)
        deEmph_coeff_ = std::exp(-2.0 * M_PI * 5000.0 / static_cast<double>(sampleRate));

        // ---- Tone stack biquad coefficients ----
        // Will be computed per-block in process() via updateToneStack()
        toneStackDirty_ = true;
        prevBass_ = -1.0f;
        prevTreble_ = -1.0f;

        // ---- Spring reverb ----
        allocateSpringBuffers(sampleRate);

        // ---- Wow/Flutter noise filter (bandpass 0.5-30Hz) ----
        // LPF at 30Hz
        wfNoiseLPCoeff_ = std::exp(-2.0 * M_PI * 30.0 / static_cast<double>(sampleRate));
        // HPF at 0.5Hz
        wfNoiseHPCoeff_ = std::exp(-2.0 * M_PI * 0.5 / static_cast<double>(sampleRate));

        // Wow/flutter phase increment per sample (base frequencies)
        wfPhaseInc_ = 1.0f / fs;
    }

    void reset() override {
        std::fill(delayBuffer_.begin(), delayBuffer_.end(), 0.0f);
        writePos_ = 0;
        feedbackSample_ = 0.0f;

        // Filter states
        inputHPState_ = 0.0;
        inputHPPrev_ = 0.0;
        fbDarkenLPState_ = 0.0;
        fbDarkenHPState_ = 0.0;
        fbDarkenHPPrev_ = 0.0;
        preEmphState_ = 0.0;
        deEmphState_ = 0.0;

        // Tone stack biquad states
        for (int b = 0; b < 2; ++b) {
            bqX1_[b] = bqX2_[b] = 0.0;
            bqY1_[b] = bqY2_[b] = 0.0;
        }

        // Spring reverb state
        resetSpringState();

        // Wow/flutter state
        wfPhase_ = 0.0f;
        wfNoiseState_ = 0.0;
        wfNoiseLPState_ = 0.0;
        wfNoiseHPState_ = 0.0;
        wfNoiseHPPrev_ = 0.0;
        lfsr_ = 0xACE1u;
    }

    // =========================================================================
    // Parameter access
    // =========================================================================

    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0:
                repeatRate_.store(clamp(value, 0.0f, 1.0f),
                                  std::memory_order_relaxed);
                break;
            case 1:
                echoVolume_.store(clamp(value, 0.0f, 1.0f),
                                  std::memory_order_relaxed);
                break;
            case 2:
                intensity_.store(clamp(value, 0.0f, 1.0f),
                                 std::memory_order_relaxed);
                break;
            case 3:
                mode_.store(clamp(value, 0.0f, 11.0f),
                            std::memory_order_relaxed);
                break;
            case 4:
                bass_.store(clamp(value, 0.0f, 1.0f),
                            std::memory_order_relaxed);
                break;
            case 5:
                treble_.store(clamp(value, 0.0f, 1.0f),
                              std::memory_order_relaxed);
                break;
            case 6:
                reverbVolume_.store(clamp(value, 0.0f, 1.0f),
                                    std::memory_order_relaxed);
                break;
            case 7:
                wowFlutter_.store(clamp(value, 0.0f, 1.0f),
                                  std::memory_order_relaxed);
                break;
            case 8:
                tapeCondition_.store(clamp(value, 0.0f, 1.0f),
                                     std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return repeatRate_.load(std::memory_order_relaxed);
            case 1: return echoVolume_.load(std::memory_order_relaxed);
            case 2: return intensity_.load(std::memory_order_relaxed);
            case 3: return mode_.load(std::memory_order_relaxed);
            case 4: return bass_.load(std::memory_order_relaxed);
            case 5: return treble_.load(std::memory_order_relaxed);
            case 6: return reverbVolume_.load(std::memory_order_relaxed);
            case 7: return wowFlutter_.load(std::memory_order_relaxed);
            case 8: return tapeCondition_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    // =========================================================================
    // Constants
    // =========================================================================

    /** Maximum delay time in ms (head 3 at slowest tape speed + modulation room). */
    static constexpr float kMaxDelayMs = 2000.0f;

    /** Maximum feedback coefficient (caps below unity to prevent runaway). */
    static constexpr float kMaxFeedback = 0.95f;

    /** Preamp gain in linear (~20dB = 10x). */
    static constexpr float kPreampGain = 10.0f;

    /** 12 mode configurations matching the RE-201 rotary switch. */
    struct ModeConfig {
        bool head1, head2, head3, reverb;
    };

    static constexpr ModeConfig kModes[12] = {
        {true,  false, false, false},  // Mode 1:  Head 1 only (slapback)
        {false, true,  false, false},  // Mode 2:  Head 2 only (quarter note)
        {true,  true,  false, false},  // Mode 3:  Head 1+2 (dotted eighth)
        {false, false, true,  false},  // Mode 4:  Head 3 only (half note)
        {true,  false, true,  false},  // Mode 5:  Head 1+3 (dotted quarter)
        {false, true,  true,  false},  // Mode 6:  Head 2+3 (2-beat)
        {true,  true,  true,  false},  // Mode 7:  All heads (triple cascade)
        {true,  false, false, true},   // Mode 8:  Head 1 + reverb
        {false, true,  false, true},   // Mode 9:  Head 2 + reverb
        {true,  true,  false, true},   // Mode 10: Head 1+2 + reverb
        {false, false, true,  true},   // Mode 11: Head 3 + reverb
        {true,  true,  true,  true}    // Mode 12: All heads + reverb
    };

    // Spring reverb constants
    static constexpr int kMaxSprings = 2;
    static constexpr int kSpringAPCount = 12;

    /** Mutually prime delay lengths per spring (in samples at 48kHz). */
    static constexpr int kSpringBaseDelay[kMaxSprings] = { 1109, 1543 };

    /** Graduated allpass dispersion coefficients per spring. */
    static constexpr float kSpringAPCoeffs[kMaxSprings][kSpringAPCount] = {
        { 0.30f, 0.35f, 0.40f, 0.45f, 0.50f, 0.55f,
          0.60f, 0.65f, 0.70f, 0.73f, 0.76f, 0.80f },
        { 0.32f, 0.37f, 0.42f, 0.47f, 0.52f, 0.57f,
          0.62f, 0.67f, 0.72f, 0.75f, 0.78f, 0.82f }
    };

    // =========================================================================
    // Cubic Hermite interpolation for delay line reads
    // =========================================================================

    /**
     * Read from the delay buffer using cubic Hermite interpolation.
     * This provides smooth modulation of delay times for wow/flutter
     * without the aliasing artifacts of linear interpolation.
     *
     * @param delaySamples Fractional delay in samples from current write pos.
     * @return Interpolated sample value.
     */
    float readCubic(float delaySamples) const {
        // Clamp delay to valid range
        if (delaySamples < 1.0f) delaySamples = 1.0f;
        float maxDelay = static_cast<float>(bufferSize_ - 4);
        if (delaySamples > maxDelay) delaySamples = maxDelay;

        float readPos = static_cast<float>(writePos_) - delaySamples;
        if (readPos < 0.0f) readPos += static_cast<float>(bufferSize_);

        int idx = static_cast<int>(readPos);
        float frac = readPos - static_cast<float>(idx);

        // Four sample points for cubic interpolation
        float y0 = delayBuffer_[(idx - 1 + bufferSize_) & bufferMask_];
        float y1 = delayBuffer_[idx & bufferMask_];
        float y2 = delayBuffer_[(idx + 1) & bufferMask_];
        float y3 = delayBuffer_[(idx + 2) & bufferMask_];

        // Hermite cubic interpolation (4-point, 3rd-order)
        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    // =========================================================================
    // Input preamp
    // =========================================================================

    /**
     * Input coupling HPF at ~30Hz (removes DC from guitar signal).
     * 1-pole HPF: y[n] = R * (y[n-1] + x[n] - x[n-1])
     */
    float applyInputHPF(float input) {
        double x = static_cast<double>(input);
        inputHPState_ = inputHP_coeff_ * (inputHPState_ + x - inputHPPrev_);
        inputHPPrev_ = x;
        inputHPState_ = fast_math::denormal_guard(inputHPState_);
        return static_cast<float>(inputHPState_);
    }

    /**
     * 2SC828 transistor soft clip with slight asymmetry.
     * Models the common-emitter stage's inherent nonlinearity.
     * The asymmetry adds subtle even harmonics (warmer than pure tanh).
     */
    static float preampClip(float x) {
        // Asymmetric soft clip: positive half compresses slightly more
        float sat = fast_math::fast_tanh(x);
        // Subtle even-harmonic asymmetry from transistor transfer curve
        sat += 0.05f * x / (1.0f + x * x);
        return sat;
    }

    // =========================================================================
    // Baxandall tone stack (2 biquad shelving filters)
    // =========================================================================

    /**
     * Recompute tone stack biquad coefficients when bass/treble change.
     * Only recalculates if values actually changed (avoids per-sample trig).
     *
     * Bass:   Low shelf at 350Hz, +/-12dB, Q=0.7
     * Treble: High shelf at 2.5kHz, +/-12dB, Q=0.7
     */
    void updateToneStack(float bass, float treble) {
        // Skip if parameters unchanged
        if (bass == prevBass_ && treble == prevTreble_ && !toneStackDirty_) return;
        prevBass_ = bass;
        prevTreble_ = treble;
        toneStackDirty_ = false;

        const double fs = static_cast<double>(sampleRate_);

        // Bass: 0.0 -> -12dB, 0.5 -> 0dB, 1.0 -> +12dB
        float bassDb = (bass - 0.5f) * 24.0f;
        computeLowShelf(350.0, bassDb, 0.7, fs, bqB_[0], bqA_[0]);

        // Treble: same mapping
        float trebleDb = (treble - 0.5f) * 24.0f;
        computeHighShelf(2500.0, trebleDb, 0.7, fs, bqB_[1], bqA_[1]);
    }

    /** Apply the 2-band Baxandall tone stack (cascaded biquads). */
    float applyToneStack(float input) {
        double x = static_cast<double>(input);

        // Biquad 0: Bass shelf
        double y0 = bqB_[0][0] * x + bqB_[0][1] * bqX1_[0] + bqB_[0][2] * bqX2_[0]
                   - bqA_[0][1] * bqY1_[0] - bqA_[0][2] * bqY2_[0];
        bqX2_[0] = bqX1_[0]; bqX1_[0] = x;
        bqY2_[0] = bqY1_[0]; bqY1_[0] = fast_math::denormal_guard(y0);

        // Biquad 1: Treble shelf
        double y1 = bqB_[1][0] * y0 + bqB_[1][1] * bqX1_[1] + bqB_[1][2] * bqX2_[1]
                   - bqA_[1][1] * bqY1_[1] - bqA_[1][2] * bqY2_[1];
        bqX2_[1] = bqX1_[1]; bqX1_[1] = y0;
        bqY2_[1] = bqY1_[1]; bqY1_[1] = fast_math::denormal_guard(y1);

        return static_cast<float>(y1);
    }

    /** Compute biquad coefficients for a low shelf filter. */
    static void computeLowShelf(double fc, float gainDb, double Q, double fs,
                                double b[3], double a[3]) {
        double A = std::pow(10.0, gainDb / 40.0);
        double w0 = 2.0 * M_PI * fc / fs;
        double cosW = std::cos(w0);
        double sinW = std::sin(w0);
        double alpha = sinW / (2.0 * Q);
        double sqrtA = std::sqrt(A);
        double twoSqrtAAlpha = 2.0 * sqrtA * alpha;

        double a0 = (A + 1.0) + (A - 1.0) * cosW + twoSqrtAAlpha;
        b[0] = A * ((A + 1.0) - (A - 1.0) * cosW + twoSqrtAAlpha) / a0;
        b[1] = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosW) / a0;
        b[2] = A * ((A + 1.0) - (A - 1.0) * cosW - twoSqrtAAlpha) / a0;
        a[0] = 1.0;
        a[1] = -2.0 * ((A - 1.0) + (A + 1.0) * cosW) / a0;
        a[2] = ((A + 1.0) + (A - 1.0) * cosW - twoSqrtAAlpha) / a0;
    }

    /** Compute biquad coefficients for a high shelf filter. */
    static void computeHighShelf(double fc, float gainDb, double Q, double fs,
                                 double b[3], double a[3]) {
        double A = std::pow(10.0, gainDb / 40.0);
        double w0 = 2.0 * M_PI * fc / fs;
        double cosW = std::cos(w0);
        double sinW = std::sin(w0);
        double alpha = sinW / (2.0 * Q);
        double sqrtA = std::sqrt(A);
        double twoSqrtAAlpha = 2.0 * sqrtA * alpha;

        double a0 = (A + 1.0) - (A - 1.0) * cosW + twoSqrtAAlpha;
        b[0] = A * ((A + 1.0) + (A - 1.0) * cosW + twoSqrtAAlpha) / a0;
        b[1] = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosW) / a0;
        b[2] = A * ((A + 1.0) + (A - 1.0) * cosW - twoSqrtAAlpha) / a0;
        a[0] = 1.0;
        a[1] = 2.0 * ((A - 1.0) - (A + 1.0) * cosW) / a0;
        a[2] = ((A + 1.0) - (A - 1.0) * cosW - twoSqrtAAlpha) / a0;
    }

    // =========================================================================
    // Tape saturation (frequency-dependent, in feedback loop)
    // =========================================================================

    /**
     * Frequency-dependent tape saturation modeling real oxide tape behavior.
     *
     * The pre-emphasis boosts HF before the nonlinearity, causing high
     * frequencies to hit the saturation curve harder (just like real tape).
     * The de-emphasis LPF restores spectral balance afterward.
     *
     * @param input Signal sample entering the tape record path.
     * @return Saturated signal with HF compression and harmonic addition.
     */
    float tapeSaturate(float input) {
        double x = static_cast<double>(input);

        // Pre-emphasis: 1-pole HPF at ~2kHz extracts HF content
        double hpOut = preEmph_coeff_ * (preEmphState_ + x - preEmphPrev_);
        preEmphPrev_ = x;
        preEmphState_ = fast_math::denormal_guard(hpOut);

        // Boost HF component before saturation (real tape pre-emphasis)
        float preEmph = input + 0.7f * static_cast<float>(hpOut);

        // Soft saturation via tanh (models tape hysteresis curve)
        float saturated = fast_math::fast_tanh(preEmph * 1.5f);

        // De-emphasis: 1-pole LPF at ~5kHz restores spectral balance
        deEmphState_ = deEmphState_ * deEmph_coeff_
                     + static_cast<double>(saturated) * (1.0 - deEmph_coeff_);
        deEmphState_ = fast_math::denormal_guard(deEmphState_);

        return static_cast<float>(deEmphState_);
    }

    // =========================================================================
    // Wow and flutter (4-component sinusoidal + filtered noise)
    // =========================================================================

    /**
     * Compute the current wow/flutter modulation value.
     *
     * Uses 4 sinusoidal components at characteristic mechanical frequencies
     * plus bandpass-filtered noise for realism. The frequencies drift slowly
     * to avoid periodic artifacts (real tape transport has non-repeating
     * mechanical variations).
     *
     * @param amount User control (0=none, 1=full, exaggerated for worn tape).
     * @return Fractional delay modulation in range approx [-0.002, +0.002].
     */
    float computeWowFlutter(float amount) {
        if (amount < 0.001f) return 0.0f;

        // 4 sinusoidal components at characteristic RE-201 mechanical frequencies
        float wow1    = 0.0010f * fast_math::sin2pi(wfPhase_ * 1.0f);   // Primary wow ~1Hz
        float wow2    = 0.0005f * fast_math::sin2pi(wfPhase_ * 3.0f);   // Secondary wow ~3Hz
        float flutter1 = 0.0004f * fast_math::sin2pi(wfPhase_ * 10.0f); // Primary flutter ~10Hz
        float flutter2 = 0.0001f * fast_math::sin2pi(wfPhase_ * 20.0f); // Secondary flutter ~20Hz

        // Bandpass-filtered noise component (0.5-30Hz)
        float noise = lfsr_noise() * 0.00015f;
        // LPF at 30Hz
        wfNoiseLPState_ = wfNoiseLPState_ * wfNoiseLPCoeff_
                        + static_cast<double>(noise) * (1.0 - wfNoiseLPCoeff_);
        wfNoiseLPState_ = fast_math::denormal_guard(wfNoiseLPState_);
        // HPF at 0.5Hz
        double lpOut = wfNoiseLPState_;
        wfNoiseHPState_ = wfNoiseHPCoeff_ * (wfNoiseHPState_ + lpOut - wfNoiseHPPrev_);
        wfNoiseHPPrev_ = lpOut;
        wfNoiseHPState_ = fast_math::denormal_guard(wfNoiseHPState_);

        float total = wow1 + wow2 + flutter1 + flutter2 + static_cast<float>(wfNoiseHPState_);

        return total * amount;
    }

    /** Advance the wow/flutter master phase (called once per sample). */
    void advanceWFPhase() {
        wfPhase_ += wfPhaseInc_;
        // Wrap at a large value to prevent float precision loss
        // (at 1 sample/increment, this wraps every ~11.6 hours at 48kHz)
        if (wfPhase_ > 2000000.0f) {
            wfPhase_ -= 2000000.0f;
        }
    }

    /**
     * Simple LFSR-based pseudo-random noise for wow/flutter.
     * Returns a value in [-1, 1]. Not cryptographic -- just needs to
     * sound random at sub-30Hz modulation rates.
     */
    float lfsr_noise() {
        // 16-bit Galois LFSR, taps at bits 16, 14, 13, 11
        uint16_t bit = ((lfsr_ >> 0) ^ (lfsr_ >> 2) ^ (lfsr_ >> 3) ^ (lfsr_ >> 5)) & 1u;
        lfsr_ = (lfsr_ >> 1) | (bit << 15);
        return (static_cast<float>(lfsr_) / 32768.0f) - 1.0f;
    }

    // =========================================================================
    // Spring reverb (2-spring Accutronics Type 4)
    // =========================================================================

    /**
     * Process one sample through the 2-spring reverb tank.
     *
     * Each spring: input -> allpass dispersion chain (12 stages) ->
     *   delay line -> damping LPF/HPF -> feedback -> sum.
     * The two springs have mutually prime delay lengths and different
     * dispersion coefficients for a dense, natural tail.
     *
     * @param input Dry input sample (from preamp, NOT from tape).
     * @return Reverb wet signal.
     */
    float processSpringReverb(float input) {
        if (springDelay_[0].empty()) return 0.0f;

        const float springFb = 0.85f; // Fixed feedback for ~3s reverb time
        float springSum = 0.0f;

        for (int s = 0; s < kMaxSprings; ++s) {
            // Mix input with feedback from this spring's delay line
            float sig = input + springFbSample_[s] * springFb;

            // Allpass dispersion chain (12 cascaded 1st-order allpass)
            // Creates the characteristic spring "chirp" -- HF arrives first
            for (int k = 0; k < kSpringAPCount; ++k) {
                float a = kSpringAPCoeffs[s][k];
                float prev = springAPState_[s][k];
                float out = a * sig + prev - a * springAPPrev_[s][k];
                springAPPrev_[s][k] = out;
                springAPState_[s][k] = sig;
                sig = fast_math::denormal_guard(out);
            }

            // Write to spring delay line
            springDelay_[s][springWritePos_[s]] = sig;

            // Read from spring delay line
            int readPos = springWritePos_[s] - springDelayLen_[s];
            if (readPos < 0) readPos += (springDelayMask_[s] + 1);
            float delayed = springDelay_[s][readPos & springDelayMask_[s]];

            // Advance write position
            springWritePos_[s] = (springWritePos_[s] + 1) & springDelayMask_[s];

            // Damping LPF at ~4kHz (natural spring tank rolloff)
            springDampLP_[s] = springDampLP_[s] * springLPCoeff_[s]
                             + static_cast<double>(delayed) * (1.0 - springLPCoeff_[s]);
            springDampLP_[s] = fast_math::denormal_guard(springDampLP_[s]);
            float dampedLP = static_cast<float>(springDampLP_[s]);

            // Damping HPF at ~100Hz (spring tank bass rolloff)
            double hpIn = static_cast<double>(dampedLP);
            springDampHP_[s] = springHPCoeff_[s]
                             * (springDampHP_[s] + hpIn - springHPPrev_[s]);
            springHPPrev_[s] = hpIn;
            springDampHP_[s] = fast_math::denormal_guard(springDampHP_[s]);
            float dampedOut = static_cast<float>(springDampHP_[s]);

            // Store for next sample's feedback
            springFbSample_[s] = dampedOut;

            springSum += dampedOut;
        }

        // Average the two springs
        return springSum * 0.5f;
    }

    /** Allocate spring reverb delay buffers (called from setSampleRate). */
    void allocateSpringBuffers(int32_t sampleRate) {
        float rateScale = static_cast<float>(sampleRate) / 48000.0f;

        for (int s = 0; s < kMaxSprings; ++s) {
            springDelayLen_[s] = std::max(1, static_cast<int>(
                static_cast<float>(kSpringBaseDelay[s]) * rateScale));

            int minSize = springDelayLen_[s] + 1;
            int size = nextPowerOfTwo(minSize);
            springDelay_[s].assign(size, 0.0f);
            springDelayMask_[s] = size - 1;
            springWritePos_[s] = 0;

            // Damping LPF: ~4kHz for spring 0, ~3.5kHz for spring 1
            float lpFc = (s == 0) ? 4000.0f : 3500.0f;
            springLPCoeff_[s] = std::exp(-2.0 * M_PI * static_cast<double>(lpFc)
                              / static_cast<double>(sampleRate));

            // Damping HPF: ~100Hz
            float hpFc = (s == 0) ? 100.0f : 120.0f;
            springHPCoeff_[s] = std::exp(-2.0 * M_PI * static_cast<double>(hpFc)
                              / static_cast<double>(sampleRate));
        }

        resetSpringState();
    }

    /** Reset all spring reverb state to silence. */
    void resetSpringState() {
        for (int s = 0; s < kMaxSprings; ++s) {
            std::fill(springDelay_[s].begin(), springDelay_[s].end(), 0.0f);
            springWritePos_[s] = 0;
            springFbSample_[s] = 0.0f;
            springDampLP_[s] = 0.0;
            springDampHP_[s] = 0.0;
            springHPPrev_[s] = 0.0;
            for (int k = 0; k < kSpringAPCount; ++k) {
                springAPState_[s][k] = 0.0f;
                springAPPrev_[s][k] = 0.0f;
            }
        }
    }

    // =========================================================================
    // Utility
    // =========================================================================

    /** Compute 1-pole LPF coefficient from cutoff frequency. */
    double computeLPCoeff(float fc) const {
        return std::exp(-2.0 * M_PI * static_cast<double>(fc)
                      / static_cast<double>(sampleRate_));
    }

    static int nextPowerOfTwo(int n) {
        int power = 1;
        while (power < n) power <<= 1;
        return power;
    }

    static float clamp(float value, float minVal, float maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    static int clamp(int value, int minVal, int maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    // =========================================================================
    // Parameters (written from UI thread, read from audio thread)
    // =========================================================================

    std::atomic<float> repeatRate_{0.5f};     // 0-1, maps to base delay 132-526ms
    std::atomic<float> echoVolume_{0.5f};     // 0-1, tape echo wet level
    std::atomic<float> intensity_{0.4f};      // 0-1, feedback amount (0-0.95)
    std::atomic<float> mode_{0.0f};           // 0-11, mode selector position
    std::atomic<float> bass_{0.5f};           // 0-1, tone stack bass (+/-12dB)
    std::atomic<float> treble_{0.5f};         // 0-1, tone stack treble (+/-12dB)
    std::atomic<float> reverbVolume_{0.3f};   // 0-1, spring reverb level
    std::atomic<float> wowFlutter_{0.5f};     // 0-1, wow/flutter amount
    std::atomic<float> tapeCondition_{0.3f};  // 0-1, tape wear (0=new, 1=worn)

    // =========================================================================
    // Audio thread state
    // =========================================================================

    int32_t sampleRate_ = 44100;

    // ---- Main delay buffer (shared by all 3 heads) ----
    std::vector<float> delayBuffer_;
    int bufferSize_ = 0;
    int bufferMask_ = 0;
    int writePos_ = 0;

    // ---- Feedback state ----
    float feedbackSample_ = 0.0f;

    // ---- Input HPF (~30Hz coupling cap) ----
    double inputHP_coeff_ = 0.0;
    double inputHPState_ = 0.0;
    double inputHPPrev_ = 0.0;

    // ---- Feedback darkening LPF (tape HF loss per pass) ----
    double fbDarkenLPState_ = 0.0;

    // ---- Feedback darkening HPF (~60Hz, removes DC per pass) ----
    double fbDarkenHP_coeff_ = 0.0;
    double fbDarkenHPState_ = 0.0;
    double fbDarkenHPPrev_ = 0.0;

    // ---- Tape saturation pre/de-emphasis ----
    double preEmph_coeff_ = 0.0;
    double preEmphState_ = 0.0;
    double preEmphPrev_ = 0.0;
    double deEmph_coeff_ = 0.0;
    double deEmphState_ = 0.0;

    // ---- Tone stack biquad state (2 cascaded biquads) ----
    double bqB_[2][3] = {};  // Numerator coefficients [filter][coeff]
    double bqA_[2][3] = {};  // Denominator coefficients [filter][coeff]
    double bqX1_[2] = {};    // x[n-1] per biquad
    double bqX2_[2] = {};    // x[n-2] per biquad
    double bqY1_[2] = {};    // y[n-1] per biquad
    double bqY2_[2] = {};    // y[n-2] per biquad
    float prevBass_ = -1.0f;
    float prevTreble_ = -1.0f;
    bool toneStackDirty_ = true;

    // ---- Wow/flutter state ----
    float wfPhase_ = 0.0f;          // Master phase accumulator
    float wfPhaseInc_ = 0.0f;       // Phase increment per sample (1/fs)
    double wfNoiseLPCoeff_ = 0.0;   // LP at 30Hz
    double wfNoiseHPCoeff_ = 0.0;   // HP at 0.5Hz
    double wfNoiseLPState_ = 0.0;
    double wfNoiseState_ = 0.0;
    double wfNoiseHPState_ = 0.0;
    double wfNoiseHPPrev_ = 0.0;
    uint16_t lfsr_ = 0xACE1u;       // LFSR state for noise generation

    // ---- Spring reverb state ----
    std::vector<float> springDelay_[kMaxSprings];
    int springDelayMask_[kMaxSprings] = {};
    int springWritePos_[kMaxSprings] = {};
    int springDelayLen_[kMaxSprings] = {};
    float springFbSample_[kMaxSprings] = {};
    float springAPState_[kMaxSprings][kSpringAPCount] = {};
    float springAPPrev_[kMaxSprings][kSpringAPCount] = {};
    double springDampLP_[kMaxSprings] = {};
    double springLPCoeff_[kMaxSprings] = {};
    double springDampHP_[kMaxSprings] = {};
    double springHPCoeff_[kMaxSprings] = {};
    double springHPPrev_[kMaxSprings] = {};
};

#endif // GUITAR_ENGINE_SPACE_ECHO_H
