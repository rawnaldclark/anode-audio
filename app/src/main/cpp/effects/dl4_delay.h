#ifndef GUITAR_ENGINE_DL4_DELAY_H
#define GUITAR_ENGINE_DL4_DELAY_H

#include "effect.h"
#include "fast_math.h"
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstring>

/**
 * Line 6 DL4 Delay Modeler emulation -- 16 delay algorithms in one effect.
 *
 * The DL4 (1999) was Line 6's iconic green delay pedal, containing models of
 * classic tape echoes (Maestro EP-1/EP-3, Roland RE-201), analog BBD delays
 * (Boss DM-2, EHX Deluxe Memory Man), and creative digital delay algorithms.
 * It became synonymous with ambient guitar (The Edge / U2) and remains one of
 * the most influential delay pedals ever made.
 *
 * Architecture:
 *   All 16 models share a common delay infrastructure:
 *   - Circular delay buffer with cubic Hermite interpolation (max ~2.5s)
 *   - Configurable feedback path with LP/HP filters + optional saturation
 *   - Sine LFO for wow/flutter/chorus modulation of read position
 *   - Bit crusher / sample rate reducer for Lo Res model
 *   - Reverse buffer with crossfade for Reverse model
 *   - Dual delay lines for Stereo/Ping Pong models (summed to mono)
 *   - Envelope follower for Dynamic Delay ducking
 *   - Volume swell envelope for Auto-Volume Echo
 *
 *   Each model configures which features are active and maps the Tweak/Tweez
 *   knobs to different internal parameters.
 *
 * The 16 Models:
 *    0: Tube Echo      (Maestro EP-1) -- tube warmth + tape flutter + drive
 *    1: Tape Echo      (Maestro EP-3) -- tape transport + bass/treble EQ
 *    2: Multi-Head     (Roland RE-101) -- 3 taps at 1:2:3 ratio + flutter
 *    3: Sweep Echo     (Line 6 original) -- resonant filter sweep in feedback
 *    4: Analog Echo    (Boss DM-2) -- BBD warmth + bass/treble EQ
 *    5: Analog w/ Mod  (EHX Memory Man) -- BBD + chorus modulation
 *    6: Lo Res Delay   (8-bit era) -- bit crush + sample rate reduction
 *    7: Digital Delay  (clean DD-style) -- pristine + bass/treble EQ
 *    8: Digital w/ Mod (Line 6 original) -- clean + pitch vibrato
 *    9: Rhythm Delay   (Line 6 original) -- multi-tap subdivision patterns
 *   10: Stereo Delays  (Line 6 original) -- dual independent taps (mono sum)
 *   11: Ping Pong      (Line 6 original) -- alternating L/R cross-feed
 *   12: Reverse        (tape reverse) -- granular reverse buffer
 *   13: Dynamic Delay  (TC 2290) -- envelope-ducked echo
 *   14: Auto-Volume    (Line 6 original) -- volume swell + tape echo
 *   15: Loop Sampler   -- passthrough (we have a separate looper)
 *
 * Parameter IDs for JNI access:
 *   0 = Model       (0-15, selects delay algorithm)
 *   1 = Delay Time  (0-1, maps to delay time; range depends on model)
 *   2 = Repeats     (0-1, feedback amount)
 *   3 = Tweak       (0-1, model-specific parameter)
 *   4 = Tweez       (0-1, model-specific parameter)
 *   5 = Mix         (0-1, wet/dry blend -- handled internally per model)
 *
 * Memory: ~960KB for delay buffers (2x 96000 samples + reverse buffers)
 * Pre-allocated in setSampleRate(), never in process().
 */
class DL4Delay : public Effect {
public:
    DL4Delay() = default;

    // =========================================================================
    // Effect interface
    // =========================================================================

    void process(float* buffer, int numFrames) override {
        // Snapshot all parameters atomically (relaxed loads for audio thread)
        const int model = static_cast<int>(model_.load(std::memory_order_relaxed));
        const float delayKnob = delayTime_.load(std::memory_order_relaxed);
        const float repeats = repeats_.load(std::memory_order_relaxed);
        const float tweak = tweak_.load(std::memory_order_relaxed);
        const float tweez = tweez_.load(std::memory_order_relaxed);
        const float mix = mix_.load(std::memory_order_relaxed);

        // Clamp model to valid range
        const int modelIdx = clamp(model, 0, 15);

        // Dispatch to model-specific processing.
        // Each model uses the shared infrastructure (delay buffer, filters,
        // LFO, etc.) but configures them differently.
        switch (modelIdx) {
            case 0:  processTubeEcho(buffer, numFrames, delayKnob, repeats, tweak, tweez, mix); break;
            case 1:  processTapeEcho(buffer, numFrames, delayKnob, repeats, tweak, tweez, mix); break;
            case 2:  processMultiHead(buffer, numFrames, delayKnob, repeats, tweak, tweez, mix); break;
            case 3:  processSweepEcho(buffer, numFrames, delayKnob, repeats, tweak, tweez, mix); break;
            case 4:  processAnalogEcho(buffer, numFrames, delayKnob, repeats, tweak, tweez, mix); break;
            case 5:  processAnalogMod(buffer, numFrames, delayKnob, repeats, tweak, tweez, mix); break;
            case 6:  processLoRes(buffer, numFrames, delayKnob, repeats, tweak, tweez, mix); break;
            case 7:  processDigital(buffer, numFrames, delayKnob, repeats, tweak, tweez, mix); break;
            case 8:  processDigitalMod(buffer, numFrames, delayKnob, repeats, tweak, tweez, mix); break;
            case 9:  processRhythm(buffer, numFrames, delayKnob, repeats, tweak, tweez, mix); break;
            case 10: processStereoDelays(buffer, numFrames, delayKnob, repeats, tweak, tweez, mix); break;
            case 11: processPingPong(buffer, numFrames, delayKnob, repeats, tweak, tweez, mix); break;
            case 12: processReverse(buffer, numFrames, delayKnob, repeats, tweak, tweez, mix); break;
            case 13: processDynamic(buffer, numFrames, delayKnob, repeats, tweak, tweez, mix); break;
            case 14: processAutoVolume(buffer, numFrames, delayKnob, repeats, tweak, tweez, mix); break;
            case 15: processLoopSampler(buffer, numFrames, mix); break;
        }
    }

    void setSampleRate(int32_t sampleRate) override {
        sampleRate_ = sampleRate;
        const float fs = static_cast<float>(sampleRate);

        // ---- Main delay buffer (2.5 seconds max) ----
        const int maxDelaySamples = static_cast<int>(kMaxDelayMs / 1000.0f * fs) + 64;
        bufferSize_ = nextPowerOfTwo(maxDelaySamples);
        bufferMask_ = bufferSize_ - 1;
        delayBuffer_.assign(bufferSize_, 0.0f);
        writePos_ = 0;

        // ---- Secondary delay buffer for stereo/ping-pong models ----
        delayBuffer2_.assign(bufferSize_, 0.0f);
        writePos2_ = 0;

        // ---- Reverse buffer (two halves for double-buffering) ----
        // Max reverse length is 1.25 seconds
        reverseLen_ = static_cast<int>(1250.0f / 1000.0f * fs);
        reverseBuffer_.assign(reverseLen_ * 2, 0.0f);
        reverseWritePos_ = 0;
        reverseReadPos_ = 0;
        reversePhase_ = 0;

        // ---- Pre-compute filter coefficients ----
        // Feedback darkening LPF at 3kHz (tape/tube models)
        fbLP3k_coeff_ = computeLPCoeff(3000.0f);
        // Feedback darkening LPF at 3.5kHz (BBD/analog models)
        fbLP3_5k_coeff_ = computeLPCoeff(3500.0f);
        // Feedback HPF at 60Hz (DC removal in feedback)
        fbHP60_coeff_ = std::exp(-2.0 * M_PI * 60.0 / static_cast<double>(sampleRate));

        // ---- Sweep filter coefficients (state-variable filter) ----
        sweepLfoPhase_ = 0.0f;

        // ---- Envelope follower for Dynamic Delay ----
        envAttackCoeff_ = std::exp(-1.0 / (0.010 * static_cast<double>(sampleRate)));  // 10ms attack
        envReleaseCoeff_ = std::exp(-1.0 / (0.200 * static_cast<double>(sampleRate))); // 200ms release

        // ---- Volume swell for Auto-Volume Echo ----
        swellEnv_ = 0.0f;
        swellGate_ = 0.0f;
        prevAbsInput_ = 0.0f;

        // ---- Bit crusher state ----
        decimatorCounter_ = 0.0f;
        decimatorHold_ = 0.0f;

        // ---- LFO phase increment ----
        lfoPhaseInc_ = 1.0f / fs;
    }

    void reset() override {
        std::fill(delayBuffer_.begin(), delayBuffer_.end(), 0.0f);
        std::fill(delayBuffer2_.begin(), delayBuffer2_.end(), 0.0f);
        std::fill(reverseBuffer_.begin(), reverseBuffer_.end(), 0.0f);
        writePos_ = 0;
        writePos2_ = 0;
        reverseWritePos_ = 0;
        reverseReadPos_ = 0;
        reversePhase_ = 0;

        // Filter states
        fbLPState_ = 0.0;
        fbLPState2_ = 0.0;
        fbHPState_ = 0.0;
        fbHPPrev_ = 0.0;
        feedbackSample_ = 0.0f;
        feedbackSample2_ = 0.0f;

        // Sweep filter state
        sweepBP_ = 0.0;
        sweepLP_ = 0.0;
        sweepLfoPhase_ = 0.0f;

        // LFO phase
        lfoPhase_ = 0.0f;

        // Envelope follower
        envLevel_ = 0.0;

        // Volume swell
        swellEnv_ = 0.0f;
        swellGate_ = 0.0f;
        prevAbsInput_ = 0.0f;

        // Bit crusher
        decimatorCounter_ = 0.0f;
        decimatorHold_ = 0.0f;

        // Shelving EQ states
        for (int b = 0; b < 2; ++b) {
            bqX1_[b] = bqX2_[b] = 0.0;
            bqY1_[b] = bqY2_[b] = 0.0;
        }
        prevTweakEQ_ = -1.0f;
        prevTweezEQ_ = -1.0f;
    }

    // =========================================================================
    // Parameter access
    // =========================================================================

    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case 0: {
                float clamped = clampf(value, 0.0f, 15.0f);
                int newModel = static_cast<int>(clamped + 0.5f);
                int oldModel = static_cast<int>(model_.load(std::memory_order_relaxed) + 0.5f);
                model_.store(static_cast<float>(clamp(newModel, 0, 15)),
                             std::memory_order_relaxed);
                // Reset filter state when model changes to prevent clicks
                if (newModel != oldModel) {
                    resetFilterState();
                }
                break;
            }
            case 1:
                delayTime_.store(clampf(value, 0.0f, 1.0f), std::memory_order_relaxed);
                break;
            case 2:
                repeats_.store(clampf(value, 0.0f, 1.0f), std::memory_order_relaxed);
                break;
            case 3:
                tweak_.store(clampf(value, 0.0f, 1.0f), std::memory_order_relaxed);
                break;
            case 4:
                tweez_.store(clampf(value, 0.0f, 1.0f), std::memory_order_relaxed);
                break;
            case 5:
                mix_.store(clampf(value, 0.0f, 1.0f), std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    float getParameter(int paramId) const override {
        switch (paramId) {
            case 0: return model_.load(std::memory_order_relaxed);
            case 1: return delayTime_.load(std::memory_order_relaxed);
            case 2: return repeats_.load(std::memory_order_relaxed);
            case 3: return tweak_.load(std::memory_order_relaxed);
            case 4: return tweez_.load(std::memory_order_relaxed);
            case 5: return mix_.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

private:
    // =========================================================================
    // Constants
    // =========================================================================

    /** Maximum delay time in ms (2.5 seconds, matching DL4 hardware). */
    static constexpr float kMaxDelayMs = 2500.0f;

    /** Maximum feedback coefficient (capped to prevent runaway). */
    static constexpr float kMaxFeedback = 0.97f;

    /** Self-oscillation threshold: feedback above this creates infinite repeats. */
    static constexpr float kSelfOscFeedback = 0.95f;

    // =========================================================================
    // Utility helpers
    // =========================================================================

    static int clamp(int v, int lo, int hi) {
        return (v < lo) ? lo : (v > hi) ? hi : v;
    }

    static float clampf(float v, float lo, float hi) {
        return std::max(lo, std::min(hi, v));
    }

    static int nextPowerOfTwo(int n) {
        int power = 1;
        while (power < n) power <<= 1;
        return power;
    }

    /** Compute 1-pole LPF coefficient for a given cutoff frequency. */
    double computeLPCoeff(float freq) const {
        return std::exp(-2.0 * M_PI * static_cast<double>(freq)
                        / static_cast<double>(sampleRate_));
    }

    /**
     * Map the 0-1 delay knob to delay time in samples.
     * Exponential mapping for musical feel (short delays are more sensitive).
     *
     * @param knob Normalized 0-1 knob position.
     * @param maxMs Maximum delay in milliseconds for this model.
     * @return Delay in fractional samples.
     */
    float delayKnobToSamples(float knob, float maxMs) const {
        // Exponential mapping: 10ms to maxMs
        const float minMs = 10.0f;
        float ms = minMs * fast_math::exp2_approx(knob * fast_math::fastLog2(maxMs / minMs));
        return (ms / 1000.0f) * static_cast<float>(sampleRate_);
    }

    /**
     * Map repeats knob (0-1) to feedback coefficient.
     * At 1.0 = self-oscillation (feedback >= 0.97).
     */
    static float repeatsToFeedback(float repeats) {
        return repeats * kMaxFeedback;
    }

    // =========================================================================
    // Cubic Hermite interpolation for delay line reads
    // =========================================================================

    /**
     * Read from the primary delay buffer with cubic Hermite interpolation.
     * Same algorithm as SpaceEcho -- smooth modulation without aliasing.
     *
     * @param delaySamples Fractional delay in samples from current write pos.
     * @return Interpolated sample value.
     */
    float readCubic(float delaySamples) const {
        if (delaySamples < 1.0f) delaySamples = 1.0f;
        float maxDelay = static_cast<float>(bufferSize_ - 4);
        if (delaySamples > maxDelay) delaySamples = maxDelay;

        float readPos = static_cast<float>(writePos_) - delaySamples;
        if (readPos < 0.0f) readPos += static_cast<float>(bufferSize_);

        int idx = static_cast<int>(readPos);
        float frac = readPos - static_cast<float>(idx);

        float y0 = delayBuffer_[(idx - 1 + bufferSize_) & bufferMask_];
        float y1 = delayBuffer_[idx & bufferMask_];
        float y2 = delayBuffer_[(idx + 1) & bufferMask_];
        float y3 = delayBuffer_[(idx + 2) & bufferMask_];

        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    /**
     * Read from the secondary delay buffer with cubic Hermite interpolation.
     * Used by Stereo Delays and Ping Pong models.
     */
    float readCubic2(float delaySamples) const {
        if (delaySamples < 1.0f) delaySamples = 1.0f;
        float maxDelay = static_cast<float>(bufferSize_ - 4);
        if (delaySamples > maxDelay) delaySamples = maxDelay;

        float readPos = static_cast<float>(writePos2_) - delaySamples;
        if (readPos < 0.0f) readPos += static_cast<float>(bufferSize_);

        int idx = static_cast<int>(readPos);
        float frac = readPos - static_cast<float>(idx);

        float y0 = delayBuffer2_[(idx - 1 + bufferSize_) & bufferMask_];
        float y1 = delayBuffer2_[idx & bufferMask_];
        float y2 = delayBuffer2_[(idx + 1) & bufferMask_];
        float y3 = delayBuffer2_[(idx + 2) & bufferMask_];

        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    // =========================================================================
    // Shared DSP building blocks
    // =========================================================================

    /**
     * Apply 1-pole LPF in the feedback path (cumulative darkening per repeat).
     * Uses 64-bit state to prevent denormal and precision issues.
     *
     * @param input Sample entering the feedback path.
     * @param coeff Pre-computed LP coefficient (from computeLPCoeff).
     * @param state Reference to the filter's state variable (double precision).
     * @return Filtered sample.
     */
    static float applyFeedbackLP(float input, double coeff, double& state) {
        state = state * coeff + static_cast<double>(input) * (1.0 - coeff);
        state = fast_math::denormal_guard(state);
        return static_cast<float>(state);
    }

    /**
     * Apply 1-pole HPF at 60Hz to remove DC buildup in feedback path.
     * y[n] = R * (y[n-1] + x[n] - x[n-1])
     */
    float applyFeedbackHP(float input) {
        double x = static_cast<double>(input);
        fbHPState_ = fbHP60_coeff_ * (fbHPState_ + x - fbHPPrev_);
        fbHPPrev_ = x;
        fbHPState_ = fast_math::denormal_guard(fbHPState_);
        return static_cast<float>(fbHPState_);
    }

    /**
     * Tube-style soft saturation (asymmetric for even harmonics).
     * Models the 12AX7 tube in the Maestro EP-1 preamp.
     *
     * @param x Input sample.
     * @param drive Saturation amount (0-1 from Tweez knob).
     * @return Saturated sample.
     */
    static float tubeSaturate(float x, float drive) {
        float gain = 1.0f + drive * 3.0f;  // 1x to 4x gain into saturation
        float driven = x * gain;
        // Asymmetric soft clip: positive side compresses more (tube character)
        float sat = fast_math::fast_tanh(driven);
        // Even harmonic addition from asymmetric transfer curve
        sat += 0.08f * drive * driven / (1.0f + driven * driven);
        return sat;
    }

    /**
     * Tape-style symmetric soft saturation (tanh waveshaper).
     * Models the magnetic oxide saturation curve of recording tape.
     *
     * @param x Input sample.
     * @param amount Saturation intensity (0 = clean, 1 = heavy).
     * @return Saturated sample.
     */
    static float tapeSaturate(float x, float amount) {
        float gain = 1.0f + amount * 2.0f;
        return fast_math::fast_tanh(x * gain);
    }

    /**
     * BBD-style gentle compression and soft saturation.
     * Models the charge-transfer imperfections of bucket-brigade devices.
     * Lighter than tape saturation, with subtle compressive character.
     */
    static float bbdSaturate(float x) {
        // Gentle cubic soft clip: x - x^3/3 (no sharp knee)
        float clamped = clampf(x, -1.5f, 1.5f);
        return clamped - (clamped * clamped * clamped) / 9.0f;
    }

    /**
     * Sine LFO with phase accumulation.
     * Returns current LFO value and advances phase.
     *
     * @param rate LFO frequency in Hz.
     * @return LFO value in [-1, 1].
     */
    float advanceLFO(float rate) {
        float val = fast_math::sin2pi(lfoPhase_);
        lfoPhase_ += rate * lfoPhaseInc_;
        // Wrap to [0, 1) to prevent float precision loss over time
        lfoPhase_ -= static_cast<int>(lfoPhase_);
        if (lfoPhase_ < 0.0f) lfoPhase_ += 1.0f;
        return val;
    }

    /**
     * Bit-crush a sample to the specified bit depth.
     * Quantizes to N bits and scales back, creating staircase waveforms.
     *
     * @param input Sample in [-1, 1].
     * @param bits Bit depth (6-24). At 24 bits, output is effectively unchanged.
     * @return Quantized sample.
     */
    static float bitCrush(float input, float bits) {
        if (bits >= 23.5f) return input;  // Transparent at full bit depth
        float levels = fast_math::exp2_approx(bits);
        float halfLevels = levels * 0.5f;
        return std::round(input * halfLevels) / halfLevels;
    }

    /**
     * Sample-rate decimation (hold samples for N cycles).
     * Combined with bit crushing for the Lo Res model.
     *
     * @param input Current input sample.
     * @param factor Decimation factor (1.0 = no decimation, 16.0 = /16).
     * @return Decimated sample (held value).
     */
    float sampleDecimate(float input, float factor) {
        decimatorCounter_ += 1.0f;
        if (decimatorCounter_ >= factor) {
            decimatorCounter_ -= factor;
            decimatorHold_ = input;
        }
        return decimatorHold_;
    }

    /**
     * Update shelving EQ biquad coefficients for bass/treble controls.
     * Only recomputes when parameter values change (avoids per-sample trig).
     *
     * @param bass Normalized bass control (0-1, 0.5 = flat).
     * @param treble Normalized treble control (0-1, 0.5 = flat).
     */
    void updateEQ(float bass, float treble) {
        if (bass == prevTweakEQ_ && treble == prevTweezEQ_) return;
        prevTweakEQ_ = bass;
        prevTweezEQ_ = treble;

        const double fs = static_cast<double>(sampleRate_);

        // Bass: 0.0 -> -12dB, 0.5 -> 0dB, 1.0 -> +12dB
        float bassDb = (bass - 0.5f) * 24.0f;
        computeLowShelf(200.0, bassDb, 0.7, fs, bqB_[0], bqA_[0]);

        // Treble: same mapping
        float trebleDb = (treble - 0.5f) * 24.0f;
        computeHighShelf(3500.0, trebleDb, 0.7, fs, bqB_[1], bqA_[1]);
    }

    /** Apply the 2-band shelving EQ (cascaded biquads). */
    float applyEQ(float input) {
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

    /** Reset filter states when model changes to prevent clicks/artifacts. */
    void resetFilterState() {
        fbLPState_ = 0.0;
        fbLPState2_ = 0.0;
        fbHPState_ = 0.0;
        fbHPPrev_ = 0.0;
        feedbackSample_ = 0.0f;
        feedbackSample2_ = 0.0f;
        sweepBP_ = 0.0;
        sweepLP_ = 0.0;
        envLevel_ = 0.0;
        swellEnv_ = 0.0f;
        swellGate_ = 0.0f;
        decimatorCounter_ = 0.0f;
        decimatorHold_ = 0.0f;
        for (int b = 0; b < 2; ++b) {
            bqX1_[b] = bqX2_[b] = 0.0;
            bqY1_[b] = bqY2_[b] = 0.0;
        }
        prevTweakEQ_ = -1.0f;
        prevTweezEQ_ = -1.0f;
    }

    // =========================================================================
    // Model 0: Tube Echo (Maestro EP-1)
    //
    // Tweak: Flutter amount (wow/flutter modulation depth)
    // Tweez: Drive (tube saturation + tape saturation amount)
    //
    // Character: Warm, dark repeats with progressive degradation.
    // Tube preamp adds 2nd/3rd harmonics; tape flutter creates pitch wobble.
    // At high Drive, echoes become increasingly saturated and compressed.
    // Self-oscillation at max Repeats produces warm, organ-like feedback.
    // =========================================================================

    void processTubeEcho(float* buffer, int numFrames,
                         float delayKnob, float repeats,
                         float flutter, float drive, float mix) {
        const float delaySamples = delayKnobToSamples(delayKnob, kMaxDelayMs);
        const float feedback = repeatsToFeedback(repeats);
        // Flutter: composite wow (1Hz) + flutter (8Hz), depth scaled by Tweak
        const float flutterRate1 = 1.0f;   // Primary wow
        const float flutterRate2 = 8.5f;   // Secondary flutter
        const float flutterDepth = flutter * 0.003f;  // Max +-0.3% pitch deviation

        for (int i = 0; i < numFrames; ++i) {
            const float dry = buffer[i];

            // Tube preamp coloration on input (subtle warmth)
            float input = tubeSaturate(dry, drive * 0.3f);

            // Write input + feedback to delay line
            delayBuffer_[writePos_] = input
                + fast_math::denormal_guard(feedbackSample_ * feedback);

            // Modulated read position (wow/flutter)
            float wfMod = flutterDepth * (
                0.7f * fast_math::sin2pi(lfoPhase_ * flutterRate1) +
                0.3f * fast_math::sin2pi(lfoPhase_ * flutterRate2)
            );
            float modDelay = delaySamples * (1.0f + wfMod);
            float delayed = readCubic(modDelay);

            // Feedback path: LP filter (repeats get darker) + tape saturation
            float fbSignal = applyFeedbackLP(delayed, fbLP3k_coeff_, fbLPState_);
            fbSignal = tapeSaturate(fbSignal, drive * 0.5f);
            fbSignal = applyFeedbackHP(fbSignal);
            feedbackSample_ = fbSignal;

            // Advance LFO
            lfoPhase_ += lfoPhaseInc_;
            lfoPhase_ -= static_cast<int>(lfoPhase_);

            // Advance write position
            writePos_ = (writePos_ + 1) & bufferMask_;

            // Wet/dry mix
            buffer[i] = dry * (1.0f - mix) + delayed * mix;
        }
    }

    // =========================================================================
    // Model 1: Tape Echo (Maestro EP-3, solid-state)
    //
    // Tweak: Bass EQ (low shelf in feedback path)
    // Tweez: Treble EQ (high shelf in feedback path)
    //
    // Character: Cleaner than Tube Echo but still has tape transport artifacts.
    // Subtle wow/flutter, lighter saturation (JFET vs tube).
    // Bass/Treble controls shape echo tone independently of dry signal.
    // =========================================================================

    void processTapeEcho(float* buffer, int numFrames,
                         float delayKnob, float repeats,
                         float bass, float treble, float mix) {
        const float delaySamples = delayKnobToSamples(delayKnob, kMaxDelayMs);
        const float feedback = repeatsToFeedback(repeats);
        updateEQ(bass, treble);

        // Fixed subtle flutter (EP-3 has less flutter than EP-1)
        const float flutterDepth = 0.001f;

        for (int i = 0; i < numFrames; ++i) {
            const float dry = buffer[i];

            // Write input + feedback
            delayBuffer_[writePos_] = dry
                + fast_math::denormal_guard(feedbackSample_ * feedback);

            // Subtle flutter modulation
            float wfMod = flutterDepth * fast_math::sin2pi(lfoPhase_ * 1.2f);
            float modDelay = delaySamples * (1.0f + wfMod);
            float delayed = readCubic(modDelay);

            // Feedback path: shelving EQ + mild tape saturation
            float fbSignal = applyEQ(delayed);
            fbSignal = tapeSaturate(fbSignal, 0.15f);
            fbSignal = applyFeedbackHP(fbSignal);
            feedbackSample_ = fbSignal;

            lfoPhase_ += lfoPhaseInc_;
            lfoPhase_ -= static_cast<int>(lfoPhase_);
            writePos_ = (writePos_ + 1) & bufferMask_;

            buffer[i] = dry * (1.0f - mix) + delayed * mix;
        }
    }

    // =========================================================================
    // Model 2: Multi-Head (Roland RE-101 Space Echo)
    //
    // Tweak: Head spacing ratio (controls relative position of taps)
    // Tweez: Flutter amount
    //
    // Character: 3 delay taps at configurable ratios create complex rhythmic
    // patterns. Each tap has tape character (LP filter + saturation).
    // At nominal settings, taps at 1:2:3 ratio like the real RE-101.
    // Tweak morphs the spacing from tight (all taps close) to wide.
    // =========================================================================

    void processMultiHead(float* buffer, int numFrames,
                          float delayKnob, float repeats,
                          float headSpacing, float flutter, float mix) {
        const float baseDelay = delayKnobToSamples(delayKnob, kMaxDelayMs);
        const float feedback = repeatsToFeedback(repeats);
        const float flutterDepth = flutter * 0.003f;

        // Head spacing: Tweak controls the ratio between taps.
        // At 0.5 (noon): classic 1:2:3 ratios.
        // At 0.0: all heads close together (1:1.1:1.2).
        // At 1.0: wide spacing (1:3:5).
        float ratio2 = 1.0f + headSpacing * 2.0f;  // 1.0 to 3.0
        float ratio3 = 1.0f + headSpacing * 4.0f;  // 1.0 to 5.0

        for (int i = 0; i < numFrames; ++i) {
            const float dry = buffer[i];

            delayBuffer_[writePos_] = dry
                + fast_math::denormal_guard(feedbackSample_ * feedback);

            // Flutter modulation (shared across all heads like real tape)
            float wfMod = flutterDepth * (
                0.7f * fast_math::sin2pi(lfoPhase_ * 1.0f) +
                0.3f * fast_math::sin2pi(lfoPhase_ * 9.0f)
            );

            // Read three heads
            float head1 = readCubic(baseDelay * 1.0f * (1.0f + wfMod));
            float head2 = readCubic(baseDelay * ratio2 * (1.0f + wfMod));
            float head3 = readCubic(baseDelay * ratio3 * (1.0f + wfMod));

            // Mix heads equally
            float delayed = (head1 + head2 + head3) * 0.333f;

            // Feedback: LP darkening + tape saturation
            float fbSignal = applyFeedbackLP(delayed, fbLP3k_coeff_, fbLPState_);
            fbSignal = tapeSaturate(fbSignal, 0.2f);
            fbSignal = applyFeedbackHP(fbSignal);
            feedbackSample_ = fbSignal;

            lfoPhase_ += lfoPhaseInc_;
            lfoPhase_ -= static_cast<int>(lfoPhase_);
            writePos_ = (writePos_ + 1) & bufferMask_;

            buffer[i] = dry * (1.0f - mix) + delayed * mix;
        }
    }

    // =========================================================================
    // Model 3: Sweep Echo
    //
    // Tweak: Sweep speed (LFO rate for the resonant filter)
    // Tweez: Sweep depth/resonance
    //
    // Character: Tape-colored echoes with a slowly sweeping resonant bandpass
    // filter in the feedback path. Each repeat passes through the sweeping
    // filter, creating progressive tonal morphing -- like an auto-wah on
    // the echoes. At zero depth, behaves like a standard tape echo.
    // =========================================================================

    void processSweepEcho(float* buffer, int numFrames,
                          float delayKnob, float repeats,
                          float sweepSpeed, float sweepDepth, float mix) {
        const float delaySamples = delayKnobToSamples(delayKnob, kMaxDelayMs);
        const float feedback = repeatsToFeedback(repeats);

        // Sweep LFO rate: 0.05Hz (barely perceptible) to 5Hz (fast wobble)
        const float sweepRate = 0.05f + sweepSpeed * 4.95f;
        // Resonance: Q of 1.5 (mild) to 6.0 (very resonant)
        const float Q = 1.5f + sweepDepth * 4.5f;
        // Frequency sweep range: 200Hz to 4kHz
        const float minFreq = 200.0f;
        const float maxFreq = 4000.0f;

        for (int i = 0; i < numFrames; ++i) {
            const float dry = buffer[i];

            delayBuffer_[writePos_] = dry
                + fast_math::denormal_guard(feedbackSample_ * feedback);

            float delayed = readCubic(delaySamples);

            // Compute sweep filter center frequency from LFO
            float lfoVal = fast_math::sin2pi(sweepLfoPhase_);
            float lfoNorm = (lfoVal + 1.0f) * 0.5f;  // [0, 1]
            float centerFreq = minFreq * fast_math::exp2_approx(
                lfoNorm * fast_math::fastLog2(maxFreq / minFreq));

            // State-variable bandpass filter on the feedback signal.
            // SVF: f = 2*sin(pi*fc/fs), q = 1/Q
            float f = 2.0f * fast_math::fast_tanh(
                static_cast<float>(M_PI) * centerFreq / static_cast<float>(sampleRate_) * 0.5f);
            // Using fast_tanh as sin approximation for small angles -- accurate enough
            // Correct: f = 2*sin(pi*fc/fs), but tan_approx works for fc << fs
            f = 2.0f * static_cast<float>(
                fast_math::tan_approx(M_PI * static_cast<double>(centerFreq) / static_cast<double>(sampleRate_)));
            float q = 1.0f / Q;

            // SVF iteration
            double input_d = static_cast<double>(delayed);
            sweepLP_ += f * sweepBP_;
            double hp = input_d - sweepLP_ - q * sweepBP_;
            sweepBP_ += f * hp;
            sweepLP_ = fast_math::denormal_guard(sweepLP_);
            sweepBP_ = fast_math::denormal_guard(sweepBP_);

            // Mix: crossfade between unfiltered and bandpass-filtered based on depth
            float fbSignal = delayed * (1.0f - sweepDepth)
                           + static_cast<float>(sweepBP_) * sweepDepth;

            // Mild tape saturation
            fbSignal = tapeSaturate(fbSignal, 0.15f);
            fbSignal = applyFeedbackHP(fbSignal);
            feedbackSample_ = fbSignal;

            sweepLfoPhase_ += sweepRate * lfoPhaseInc_;
            sweepLfoPhase_ -= static_cast<int>(sweepLfoPhase_);
            if (sweepLfoPhase_ < 0.0f) sweepLfoPhase_ += 1.0f;

            writePos_ = (writePos_ + 1) & bufferMask_;

            buffer[i] = dry * (1.0f - mix) + delayed * mix;
        }
    }

    // =========================================================================
    // Model 4: Analog Echo (Boss DM-2 / BBD)
    //
    // Tweak: Bass EQ
    // Tweez: Treble EQ
    //
    // Character: Dark, warm repeats from BBD bandwidth limitation (~3.5kHz).
    // Gentle compression and saturation from charge-transfer imperfections.
    // Each repeat loses more treble, creating progressively darker echoes.
    // Self-oscillation is warm and smooth.
    // =========================================================================

    void processAnalogEcho(float* buffer, int numFrames,
                           float delayKnob, float repeats,
                           float bass, float treble, float mix) {
        const float delaySamples = delayKnobToSamples(delayKnob, kMaxDelayMs);
        const float feedback = repeatsToFeedback(repeats);
        updateEQ(bass, treble);

        for (int i = 0; i < numFrames; ++i) {
            const float dry = buffer[i];

            delayBuffer_[writePos_] = dry
                + fast_math::denormal_guard(feedbackSample_ * feedback);

            float delayed = readCubic(delaySamples);

            // Feedback path: BBD bandwidth LP + shelving EQ + BBD saturation
            float fbSignal = applyFeedbackLP(delayed, fbLP3_5k_coeff_, fbLPState_);
            fbSignal = applyEQ(fbSignal);
            fbSignal = bbdSaturate(fbSignal);
            fbSignal = applyFeedbackHP(fbSignal);
            feedbackSample_ = fbSignal;

            writePos_ = (writePos_ + 1) & bufferMask_;

            buffer[i] = dry * (1.0f - mix) + delayed * mix;
        }
    }

    // =========================================================================
    // Model 5: Analog w/ Mod (EHX Deluxe Memory Man)
    //
    // Tweak: Modulation speed (chorus LFO rate)
    // Tweez: Modulation depth (chorus pitch deviation)
    //
    // Character: Same warm BBD as Analog Echo, plus chorus modulation on the
    // delay read. Each successive repeat accumulates more modulation, creating
    // increasingly "smeared" echoes. The combination of BBD warmth + chorus
    // is uniquely lush.
    // =========================================================================

    void processAnalogMod(float* buffer, int numFrames,
                          float delayKnob, float repeats,
                          float modSpeed, float modDepth, float mix) {
        const float delaySamples = delayKnobToSamples(delayKnob, kMaxDelayMs);
        const float feedback = repeatsToFeedback(repeats);

        // Chorus modulation: rate 0.1Hz to 8Hz, depth 0 to 3ms
        const float chorusRate = 0.1f + modSpeed * 7.9f;
        const float chorusDepthSamples = modDepth * 0.003f * static_cast<float>(sampleRate_);

        for (int i = 0; i < numFrames; ++i) {
            const float dry = buffer[i];

            delayBuffer_[writePos_] = dry
                + fast_math::denormal_guard(feedbackSample_ * feedback);

            // Modulated read (chorus on delay)
            float lfoVal = advanceLFO(chorusRate);
            float modOffset = lfoVal * chorusDepthSamples;
            float delayed = readCubic(delaySamples + modOffset);

            // Feedback: BBD LP + BBD saturation
            float fbSignal = applyFeedbackLP(delayed, fbLP3_5k_coeff_, fbLPState_);
            fbSignal = bbdSaturate(fbSignal);
            fbSignal = applyFeedbackHP(fbSignal);
            feedbackSample_ = fbSignal;

            writePos_ = (writePos_ + 1) & bufferMask_;

            buffer[i] = dry * (1.0f - mix) + delayed * mix;
        }
    }

    // =========================================================================
    // Model 6: Lo Res Delay
    //
    // Tweak: Tone (LP filter cutoff on wet signal)
    // Tweez: Resolution (bit depth from 24-bit clean to 6-bit crunchy)
    //
    // Character: Emulates early 1980s digital delays with primitive ADC.
    // Low bit depth creates quantization noise and staircase waveforms.
    // Sample rate reduction adds aliasing artifacts. At 24-bit: transparent.
    // At 6-bit: extreme digital grunge. Each repeat crushes further.
    // =========================================================================

    void processLoRes(float* buffer, int numFrames,
                      float delayKnob, float repeats,
                      float tone, float resolution, float mix) {
        const float delaySamples = delayKnobToSamples(delayKnob, kMaxDelayMs);
        const float feedback = repeatsToFeedback(repeats);

        // Resolution: 6 bits (Tweez=0) to 24 bits (Tweez=1)
        const float bits = 6.0f + resolution * 18.0f;
        // Sample rate decimation: factor 1 (clean) to 16 (grungy)
        const float decimation = 1.0f + (1.0f - resolution) * 15.0f;
        // Tone LP: cutoff from 1kHz (tone=0) to 12kHz (tone=1)
        const double toneLP = computeLPCoeff(1000.0f + tone * 11000.0f);

        for (int i = 0; i < numFrames; ++i) {
            const float dry = buffer[i];

            delayBuffer_[writePos_] = dry
                + fast_math::denormal_guard(feedbackSample_ * feedback);

            float delayed = readCubic(delaySamples);

            // Apply bit crushing and decimation to the delayed signal
            float crushed = bitCrush(delayed, bits);
            crushed = sampleDecimate(crushed, decimation);

            // Tone filter on output
            float toned = applyFeedbackLP(crushed, toneLP, fbLPState_);

            // Feedback: the bit-crushed signal feeds back (further degradation per repeat)
            float fbSignal = applyFeedbackHP(toned);
            feedbackSample_ = fbSignal;

            writePos_ = (writePos_ + 1) & bufferMask_;

            buffer[i] = dry * (1.0f - mix) + toned * mix;
        }
    }

    // =========================================================================
    // Model 7: Digital Delay (clean, Boss DD-style)
    //
    // Tweak: Bass EQ
    // Tweez: Treble EQ
    //
    // Character: Crystal-clear, uncolored repeats. No saturation, no
    // modulation, no pitch artifacts. Pure digital delay. Bass/Treble are
    // in the feedback path, so each repeat is progressively shaped.
    // The model for precise rhythmic delay playing.
    // =========================================================================

    void processDigital(float* buffer, int numFrames,
                        float delayKnob, float repeats,
                        float bass, float treble, float mix) {
        const float delaySamples = delayKnobToSamples(delayKnob, kMaxDelayMs);
        const float feedback = repeatsToFeedback(repeats);
        updateEQ(bass, treble);

        for (int i = 0; i < numFrames; ++i) {
            const float dry = buffer[i];

            delayBuffer_[writePos_] = dry
                + fast_math::denormal_guard(feedbackSample_ * feedback);

            float delayed = readCubic(delaySamples);

            // Feedback path: shelving EQ only (no saturation, no LP darkening)
            float fbSignal = applyEQ(delayed);
            fbSignal = applyFeedbackHP(fbSignal);
            feedbackSample_ = fbSignal;

            writePos_ = (writePos_ + 1) & bufferMask_;

            buffer[i] = dry * (1.0f - mix) + delayed * mix;
        }
    }

    // =========================================================================
    // Model 8: Digital w/ Mod
    //
    // Tweak: Mod speed (chorus/vibrato LFO rate)
    // Tweez: Mod depth (pitch deviation amount)
    //
    // Character: Clean digital repeats with added pitch vibrato modulation.
    // Unlike Analog w/ Mod, the base tone is bright and clear (no BBD).
    // Each successive repeat accumulates more pitch deviation.
    // =========================================================================

    void processDigitalMod(float* buffer, int numFrames,
                           float delayKnob, float repeats,
                           float modSpeed, float modDepth, float mix) {
        const float delaySamples = delayKnobToSamples(delayKnob, kMaxDelayMs);
        const float feedback = repeatsToFeedback(repeats);

        // Vibrato modulation: rate 0.1Hz to 8Hz, depth 0 to 3ms
        const float vibratoRate = 0.1f + modSpeed * 7.9f;
        const float vibratoDepthSamples = modDepth * 0.003f * static_cast<float>(sampleRate_);

        for (int i = 0; i < numFrames; ++i) {
            const float dry = buffer[i];

            delayBuffer_[writePos_] = dry
                + fast_math::denormal_guard(feedbackSample_ * feedback);

            // Modulated read (pitch vibrato)
            float lfoVal = advanceLFO(vibratoRate);
            float modOffset = lfoVal * vibratoDepthSamples;
            float delayed = readCubic(delaySamples + modOffset);

            // Clean feedback (no saturation, no LP)
            float fbSignal = applyFeedbackHP(delayed);
            feedbackSample_ = fbSignal;

            writePos_ = (writePos_ + 1) & bufferMask_;

            buffer[i] = dry * (1.0f - mix) + delayed * mix;
        }
    }

    // =========================================================================
    // Model 9: Rhythm Delay
    //
    // Tweak: Rhythm subdivision pattern (quantized to 6 positions)
    // Tweez: Offset amount (time offset applied to the subdivision)
    //
    // Character: Multi-tap rhythmic patterns based on musical subdivisions
    // of the delay time. The Delay Time knob sets the base tempo (quarter
    // note period). Tweak selects the subdivision. Tweez shifts the timing.
    // Adds chorus-like modulation for musicality.
    // =========================================================================

    void processRhythm(float* buffer, int numFrames,
                       float delayKnob, float repeats,
                       float subdivision, float offset, float mix) {
        const float baseDelay = delayKnobToSamples(delayKnob, kMaxDelayMs);
        const float feedback = repeatsToFeedback(repeats);

        // Subdivision ratios: quarter, dotted 8th, 8th, 8th triplet, 16th, dotted 16th
        static constexpr float kSubdivisions[6] = {
            1.0f, 0.75f, 0.5f, 0.333f, 0.25f, 0.1875f
        };

        // Quantize Tweak to 6 positions
        int subdivIdx = static_cast<int>(subdivision * 5.99f);
        subdivIdx = clamp(subdivIdx, 0, 5);
        float ratio = kSubdivisions[subdivIdx];

        // Offset: shift delay time by +/- 20%
        float timeOffset = 1.0f + (offset - 0.5f) * 0.4f;

        float delaySamples = baseDelay * ratio * timeOffset;

        for (int i = 0; i < numFrames; ++i) {
            const float dry = buffer[i];

            delayBuffer_[writePos_] = dry
                + fast_math::denormal_guard(feedbackSample_ * feedback);

            float delayed = readCubic(delaySamples);

            // Clean feedback with mild modulation for musicality
            float fbSignal = applyFeedbackHP(delayed);
            feedbackSample_ = fbSignal;

            writePos_ = (writePos_ + 1) & bufferMask_;

            buffer[i] = dry * (1.0f - mix) + delayed * mix;
        }
    }

    // =========================================================================
    // Model 10: Stereo Delays (summed to mono for our app)
    //
    // Tweak: Right delay time (0-1, same range as main Delay Time)
    // Tweez: Right repeats (feedback for the right delay)
    //
    // Character: Two independent delay lines with separate times and feedback.
    // In our mono app, both are summed. Different L/R times create complex
    // polyrhythmic echo patterns. Clean digital character.
    // =========================================================================

    void processStereoDelays(float* buffer, int numFrames,
                             float delayKnob, float repeats,
                             float rightDelay, float rightRepeats, float mix) {
        const float leftDelaySamples = delayKnobToSamples(delayKnob, kMaxDelayMs);
        const float rightDelaySamples = delayKnobToSamples(rightDelay, kMaxDelayMs);
        const float leftFeedback = repeatsToFeedback(repeats);
        const float rightFeedback = repeatsToFeedback(rightRepeats);

        for (int i = 0; i < numFrames; ++i) {
            const float dry = buffer[i];

            // Write to both delay lines (independent, no cross-feed)
            delayBuffer_[writePos_] = dry
                + fast_math::denormal_guard(feedbackSample_ * leftFeedback);
            delayBuffer2_[writePos2_] = dry
                + fast_math::denormal_guard(feedbackSample2_ * rightFeedback);

            // Read from both
            float leftDelayed = readCubic(leftDelaySamples);
            float rightDelayed = readCubic2(rightDelaySamples);

            // Feedback paths (clean digital)
            feedbackSample_ = fast_math::denormal_guard(leftDelayed);
            feedbackSample2_ = fast_math::denormal_guard(rightDelayed);

            writePos_ = (writePos_ + 1) & bufferMask_;
            writePos2_ = (writePos2_ + 1) & bufferMask_;

            // Sum both delays for mono output
            float wet = (leftDelayed + rightDelayed) * 0.5f;
            buffer[i] = dry * (1.0f - mix) + wet * mix;
        }
    }

    // =========================================================================
    // Model 11: Ping Pong
    //
    // Tweak: Right delay ratio (0-1 maps to 0.25x-2.0x of left delay)
    // Tweez: Stereo spread (0 = mono, 1 = full L/R; summed to mono in our app)
    //
    // Character: Alternating L/R echoes via cross-feedback between two delay
    // lines. Creates bouncing echo patterns. Tweak controls the relative
    // timing of the right channel. Clean digital character.
    // =========================================================================

    void processPingPong(float* buffer, int numFrames,
                         float delayKnob, float repeats,
                         float rightRatio, float spread, float mix) {
        const float leftDelaySamples = delayKnobToSamples(delayKnob, kMaxDelayMs);
        // Right delay: 0.25x to 2.0x of left. At Tweak=0.5 (noon): 1.0x (equal).
        const float ratioMapped = 0.25f + rightRatio * 1.75f;
        const float rightDelaySamples = leftDelaySamples * ratioMapped;
        const float feedback = repeatsToFeedback(repeats);

        for (int i = 0; i < numFrames; ++i) {
            const float dry = buffer[i];

            // Cross-feedback: left input gets right's output, right gets left's
            delayBuffer_[writePos_] = dry
                + fast_math::denormal_guard(feedbackSample2_ * feedback);
            delayBuffer2_[writePos2_] = fast_math::denormal_guard(feedbackSample_ * feedback);

            float leftDelayed = readCubic(leftDelaySamples);
            float rightDelayed = readCubic2(rightDelaySamples);

            feedbackSample_ = fast_math::denormal_guard(leftDelayed);
            feedbackSample2_ = fast_math::denormal_guard(rightDelayed);

            writePos_ = (writePos_ + 1) & bufferMask_;
            writePos2_ = (writePos2_ + 1) & bufferMask_;

            // Sum to mono (spread parameter has no audible effect in mono,
            // but we include it for parameter completeness and future stereo)
            float wet = (leftDelayed + rightDelayed) * 0.5f;
            buffer[i] = dry * (1.0f - mix) + wet * mix;
        }
    }

    // =========================================================================
    // Model 12: Reverse
    //
    // Tweak: Mod speed (LFO rate on reversed echoes)
    // Tweez: Mod depth (pitch modulation amount)
    //
    // Character: Captures audio chunks and plays them back reversed.
    // Creates inherent swelling/crescendo effect (attack becomes decay).
    // Uses double-buffering: one buffer captures while the other plays
    // back reversed, with crossfade at boundaries to avoid clicks.
    // Maximum delay is 1.25 seconds (needs buffer for capture + playback).
    // =========================================================================

    void processReverse(float* buffer, int numFrames,
                        float delayKnob, float repeats,
                        float modSpeed, float modDepth, float mix) {
        // Reverse max is 1.25 seconds
        const float maxReverseMs = 1250.0f;
        const float reverseMs = 10.0f + delayKnob * (maxReverseMs - 10.0f);
        const int chunkLen = clamp(
            static_cast<int>(reverseMs / 1000.0f * static_cast<float>(sampleRate_)),
            64, reverseLen_);
        const float feedback = repeatsToFeedback(repeats) * 0.8f; // Slightly less feedback for stability

        // Modulation parameters
        const float modRate = 0.1f + modSpeed * 4.9f;
        const float modDepthSamples = modDepth * 0.002f * static_cast<float>(sampleRate_);

        // Crossfade length (~5ms for smooth transitions)
        const int xfadeLen = std::min(static_cast<int>(0.005f * static_cast<float>(sampleRate_)), chunkLen / 4);

        for (int i = 0; i < numFrames; ++i) {
            const float dry = buffer[i];

            // Determine which half of the double buffer we're writing to
            int writeHalf = reversePhase_;      // 0 or 1
            int readHalf = 1 - reversePhase_;   // opposite half

            // Write input + feedback to capture buffer
            int writeIdx = writeHalf * reverseLen_ + reverseWritePos_;
            if (writeIdx < static_cast<int>(reverseBuffer_.size())) {
                reverseBuffer_[writeIdx] = dry
                    + fast_math::denormal_guard(feedbackSample_ * feedback);
            }

            // Read reversed from the other half
            int readIdx = readHalf * reverseLen_ + (chunkLen - 1 - reverseReadPos_);
            float reversed = 0.0f;
            if (readIdx >= 0 && readIdx < static_cast<int>(reverseBuffer_.size())) {
                reversed = reverseBuffer_[readIdx];
            }

            // Apply modulation to reversed signal
            if (modDepth > 0.001f) {
                float lfoVal = advanceLFO(modRate);
                reversed *= (1.0f + lfoVal * modDepth * 0.3f);
            }

            // Crossfade at boundaries
            float xfadeGain = 1.0f;
            if (reverseReadPos_ < xfadeLen) {
                xfadeGain = static_cast<float>(reverseReadPos_) / static_cast<float>(xfadeLen);
            } else if (reverseReadPos_ > chunkLen - xfadeLen) {
                xfadeGain = static_cast<float>(chunkLen - reverseReadPos_)
                          / static_cast<float>(xfadeLen);
            }
            reversed *= xfadeGain;

            feedbackSample_ = reversed;

            // Advance positions
            reverseWritePos_++;
            reverseReadPos_++;

            // When chunk is complete, swap buffers
            if (reverseWritePos_ >= chunkLen) {
                reverseWritePos_ = 0;
                reverseReadPos_ = 0;
                reversePhase_ = 1 - reversePhase_;
            }

            buffer[i] = dry * (1.0f - mix) + reversed * mix;
        }
    }

    // =========================================================================
    // Model 13: Dynamic Delay (TC Electronic 2290)
    //
    // Tweak: Sensitivity threshold (ducking trigger level)
    // Tweez: Duck amount (how much echoes are attenuated while playing)
    //
    // Character: Envelope-ducked delay -- echoes are quiet while you play
    // (above threshold) and rise to full level when you stop or play softly.
    // Creates a "professional" delay that fills the gaps without cluttering.
    // Clean digital delay character in the delay path itself.
    // =========================================================================

    void processDynamic(float* buffer, int numFrames,
                        float delayKnob, float repeats,
                        float threshold, float duckAmount, float mix) {
        const float delaySamples = delayKnobToSamples(delayKnob, kMaxDelayMs);
        const float feedback = repeatsToFeedback(repeats);

        // Threshold: maps 0-1 to a useful guitar level range
        // At threshold=0: very sensitive (ducks on quiet playing)
        // At threshold=1: only ducks on loud playing
        const float threshLin = 0.001f + threshold * 0.5f;  // Linear amplitude threshold

        // Duck attenuation: 0dB (no duck) to -30dB (deep duck)
        const float duckGain = 1.0f - duckAmount * 0.97f;  // 1.0 to 0.03

        for (int i = 0; i < numFrames; ++i) {
            const float dry = buffer[i];

            delayBuffer_[writePos_] = dry
                + fast_math::denormal_guard(feedbackSample_ * feedback);

            float delayed = readCubic(delaySamples);

            // Envelope follower on input signal
            float absInput = std::abs(dry);
            if (absInput > envLevel_) {
                envLevel_ = envLevel_ * envAttackCoeff_ + absInput * (1.0 - envAttackCoeff_);
            } else {
                envLevel_ = envLevel_ * envReleaseCoeff_ + absInput * (1.0 - envReleaseCoeff_);
            }
            envLevel_ = fast_math::denormal_guard(envLevel_);

            // Compute ducking gain: when envelope > threshold, attenuate wet signal
            float envLin = static_cast<float>(envLevel_);
            float duck = 1.0f;
            if (envLin > threshLin) {
                // Smooth ducking: the more above threshold, the more attenuation
                float excess = (envLin - threshLin) / (1.0f - threshLin + 0.001f);
                excess = clampf(excess, 0.0f, 1.0f);
                duck = 1.0f - excess * (1.0f - duckGain);
            }

            // Apply ducking to the wet signal
            float duckedWet = delayed * duck;

            // Clean feedback (no saturation)
            feedbackSample_ = fast_math::denormal_guard(delayed);

            writePos_ = (writePos_ + 1) & bufferMask_;

            buffer[i] = dry * (1.0f - mix) + duckedWet * mix;
        }
    }

    // =========================================================================
    // Model 14: Auto-Volume Echo
    //
    // Tweak: Mod depth (wow/flutter amount on the echo)
    // Tweez: Swell/ramp time (attack time for the auto-volume swell)
    //
    // Character: Volume swell removes the pick attack from each note,
    // creating a bowed/pad-like quality. Combined with tape-style echo
    // for ethereal textures. The swell is applied BEFORE the delay, so
    // both dry and wet signals fade in.
    // =========================================================================

    void processAutoVolume(float* buffer, int numFrames,
                           float delayKnob, float repeats,
                           float modDepth, float swellTime, float mix) {
        const float delaySamples = delayKnobToSamples(delayKnob, kMaxDelayMs);
        const float feedback = repeatsToFeedback(repeats);

        // Swell ramp time: 5ms (very quick) to 2 seconds (dramatic)
        const float rampMs = 5.0f + swellTime * 1995.0f;
        const float rampSamples = rampMs / 1000.0f * static_cast<float>(sampleRate_);
        const float rampInc = 1.0f / rampSamples;

        // Flutter modulation depth
        const float flutterDepth = modDepth * 0.003f;

        // Onset detection threshold
        const float onsetThresh = 0.01f;

        for (int i = 0; i < numFrames; ++i) {
            const float dry = buffer[i];
            const float absDry = std::abs(dry);

            // Simple onset detection: rising edge above threshold
            if (absDry > onsetThresh && prevAbsInput_ <= onsetThresh) {
                // New note detected -- reset swell envelope
                swellEnv_ = 0.0f;
            }
            prevAbsInput_ = absDry;

            // Advance swell envelope toward 1.0
            if (swellEnv_ < 1.0f) {
                swellEnv_ += rampInc;
                if (swellEnv_ > 1.0f) swellEnv_ = 1.0f;
            }

            // Apply swell to input (removes pick attack)
            float swelled = dry * swellEnv_;

            // Write swelled signal + feedback to delay
            delayBuffer_[writePos_] = swelled
                + fast_math::denormal_guard(feedbackSample_ * feedback);

            // Modulated read (tape-style flutter)
            float wfMod = flutterDepth * fast_math::sin2pi(lfoPhase_ * 1.0f);
            float modDelay = delaySamples * (1.0f + wfMod);
            float delayed = readCubic(modDelay);

            // Feedback: tape darkening + mild saturation
            float fbSignal = applyFeedbackLP(delayed, fbLP3k_coeff_, fbLPState_);
            fbSignal = tapeSaturate(fbSignal, 0.1f);
            fbSignal = applyFeedbackHP(fbSignal);
            feedbackSample_ = fbSignal;

            lfoPhase_ += lfoPhaseInc_;
            lfoPhase_ -= static_cast<int>(lfoPhase_);
            writePos_ = (writePos_ + 1) & bufferMask_;

            // Output: swelled dry + delayed wet
            buffer[i] = swelled * (1.0f - mix) + delayed * mix;
        }
    }

    // =========================================================================
    // Model 15: Loop Sampler -- SKIP (passthrough)
    //
    // We have a dedicated LooperEngine. This model passes audio through
    // unchanged so the DL4 effect can be set to model 15 without any
    // processing. The Mix parameter is ignored.
    // =========================================================================

    void processLoopSampler(float* buffer, int numFrames, float /*mix*/) {
        // Passthrough -- no processing. The separate LooperEngine handles looping.
        (void)buffer;
        (void)numFrames;
    }

    // =========================================================================
    // Parameters (written from UI thread, read from audio thread)
    // =========================================================================

    std::atomic<float> model_{0.0f};        // 0-15, selects delay algorithm
    std::atomic<float> delayTime_{0.5f};    // 0-1, normalized delay time
    std::atomic<float> repeats_{0.4f};      // 0-1, feedback amount
    std::atomic<float> tweak_{0.5f};        // 0-1, model-specific
    std::atomic<float> tweez_{0.5f};        // 0-1, model-specific
    std::atomic<float> mix_{0.5f};          // 0-1, wet/dry blend

    // =========================================================================
    // Audio thread state
    // =========================================================================

    int32_t sampleRate_ = 44100;

    // ---- Primary delay buffer ----
    std::vector<float> delayBuffer_;
    int bufferSize_ = 0;
    int bufferMask_ = 0;
    int writePos_ = 0;

    // ---- Secondary delay buffer (stereo/ping-pong models) ----
    std::vector<float> delayBuffer2_;
    int writePos2_ = 0;

    // ---- Reverse buffer (double-buffered for model 12) ----
    std::vector<float> reverseBuffer_;
    int reverseLen_ = 0;
    int reverseWritePos_ = 0;
    int reverseReadPos_ = 0;
    int reversePhase_ = 0;  // 0 or 1, which half is being written

    // ---- Feedback path filter states (double precision) ----
    double fbLPState_ = 0.0;    // Primary feedback LP state
    double fbLPState2_ = 0.0;   // Secondary feedback LP state
    double fbHPState_ = 0.0;    // Feedback HP state
    double fbHPPrev_ = 0.0;     // HP previous input

    float feedbackSample_ = 0.0f;   // Primary feedback sample
    float feedbackSample2_ = 0.0f;  // Secondary feedback sample

    // ---- Filter coefficients (pre-computed in setSampleRate) ----
    double fbLP3k_coeff_ = 0.0;     // LP at 3kHz (tape/tube models)
    double fbLP3_5k_coeff_ = 0.0;   // LP at 3.5kHz (BBD/analog models)
    double fbHP60_coeff_ = 0.0;     // HP at 60Hz (DC removal)

    // ---- Sweep filter state (model 3: Sweep Echo) ----
    double sweepBP_ = 0.0;
    double sweepLP_ = 0.0;
    float sweepLfoPhase_ = 0.0f;

    // ---- LFO state ----
    float lfoPhase_ = 0.0f;
    float lfoPhaseInc_ = 0.0f;  // 1/sampleRate

    // ---- Envelope follower (model 13: Dynamic Delay) ----
    double envLevel_ = 0.0;
    double envAttackCoeff_ = 0.0;
    double envReleaseCoeff_ = 0.0;

    // ---- Volume swell (model 14: Auto-Volume Echo) ----
    float swellEnv_ = 0.0f;
    float swellGate_ = 0.0f;
    float prevAbsInput_ = 0.0f;

    // ---- Bit crusher state (model 6: Lo Res) ----
    float decimatorCounter_ = 0.0f;
    float decimatorHold_ = 0.0f;

    // ---- Shelving EQ biquad state (multiple models) ----
    double bqB_[2][3] = {};  // Biquad numerator coefficients [bass, treble]
    double bqA_[2][3] = {};  // Biquad denominator coefficients
    double bqX1_[2] = {};    // Biquad input delay line z^-1
    double bqX2_[2] = {};    // Biquad input delay line z^-2
    double bqY1_[2] = {};    // Biquad output delay line z^-1
    double bqY2_[2] = {};    // Biquad output delay line z^-2
    float prevTweakEQ_ = -1.0f;  // Previous bass value (for dirty check)
    float prevTweezEQ_ = -1.0f;  // Previous treble value
};

#endif // GUITAR_ENGINE_DL4_DELAY_H
