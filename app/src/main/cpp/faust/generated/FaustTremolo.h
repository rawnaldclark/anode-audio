/**
 * Pre-generated C++ implementation of the Tremolo effect.
 *
 * This file follows the FAUST-generated class structure and can be replaced
 * by actual FAUST output when the FAUST compiler is available. To regenerate:
 *
 *   faust -lang cpp -cn FaustTremolo -i -single -inpl -nvi \
 *         tremolo.dsp -o FaustTremolo.h
 *
 * Source: faust/dsp/tremolo.dsp
 *
 * Algorithm:
 *   Amplitude modulation using an LFO that blends between sine and square
 *   waveforms. The LFO output is mapped to [0, 1] and used to modulate
 *   the signal amplitude: out = in * (1 - depth * lfo).
 *
 * Parameters (in registration order, matching paramId indices):
 *   0: Rate  - LFO frequency in Hz     [0.5, 20.0], default 4.0
 *   1: Depth - Modulation depth         [0.0, 1.0],  default 0.5
 *   2: Shape - Sine/square blend        [0.0, 1.0],  default 0.0
 */

#ifndef FAUST_TREMOLO_H
#define FAUST_TREMOLO_H

#include "faust/faust_dsp.h"
#include <cmath>
#include <algorithm>

class FaustTremolo : public dsp {
private:
    // Sample rate
    int fSampleRate;

    // Pre-computed constant: 2 * PI / sampleRate (phase increment per Hz)
    float fConst0;

    // Quadrature oscillator state for efficient sine generation.
    // Avoids per-sample trig calls by using a 2D rotation:
    //   sinState[n] = sinState[n-1] * cos(w) - cosState[n-1] * sin(w)
    //   cosState[n] = sinState[n-1] * sin(w) + cosState[n-1] * cos(w)
    // where w = 2*PI*freq/sampleRate
    float sinState_;  // sine component of oscillator
    float cosState_;  // cosine component of oscillator

    // Square wave phase accumulator
    float fSquarePhase;

    // UI parameter zones (written by MapUI, read in compute)
    FAUSTFLOAT fHslider0; // Rate
    FAUSTFLOAT fHslider1; // Depth
    FAUSTFLOAT fHslider2; // Shape

public:
    FaustTremolo()
        : fSampleRate(0)
        , fConst0(0.0f)
        , fSquarePhase(0.0f)
        , sinState_(0.0f)
        , cosState_(1.0f)
        , fHslider0(4.0f)
        , fHslider1(0.5f)
        , fHslider2(0.0f) {
    }

    int getNumInputs() override { return 1; }
    int getNumOutputs() override { return 1; }

    void metadata(Meta* m) override {
        m->declare("name", "Tremolo");
        m->declare("author", "GuitarEmulator");
        m->declare("version", "1.0");
    }

    static void classInit(int /*sample_rate*/) {}

    void instanceConstants(int sample_rate) override {
        fSampleRate = sample_rate;
        // Phase increment constant: 2*PI / sampleRate
        fConst0 = 6.2831853071795862f / static_cast<float>(sample_rate);
    }

    void instanceResetUserInterface() override {
        fHslider0 = 4.0f;  // Rate default
        fHslider1 = 0.5f;  // Depth default
        fHslider2 = 0.0f;  // Shape default
    }

    void instanceClear() override {
        sinState_ = 0.0f;
        cosState_ = 1.0f;
        fSquarePhase = 0.0f;
    }

    void instanceInit(int sample_rate) override {
        instanceConstants(sample_rate);
        instanceResetUserInterface();
        instanceClear();
    }

    void init(int sample_rate) override {
        classInit(sample_rate);
        instanceInit(sample_rate);

        // Initialize quadrature oscillator to produce sine starting at 0.
        // sinState = sin(0) = 0, cosState = cos(0) = 1.
        sinState_ = 0.0f;
        cosState_ = 1.0f;
    }

    int getSampleRate() override { return fSampleRate; }

    dsp* clone() override { return new FaustTremolo(); }

    void buildUserInterface(UI* ui_interface) override {
        ui_interface->openVerticalBox("Tremolo");
        // Parameter registration order determines paramId indices:
        //   0 = Rate, 1 = Depth, 2 = Shape
        ui_interface->addHorizontalSlider("Rate", &fHslider0,
            4.0f, 0.5f, 20.0f, 0.1f);
        ui_interface->addHorizontalSlider("Depth", &fHslider1,
            0.5f, 0.0f, 1.0f, 0.01f);
        ui_interface->addHorizontalSlider("Shape", &fHslider2,
            0.0f, 0.0f, 1.0f, 0.01f);
        ui_interface->closeBox();
    }

    /**
     * Process one block of audio.
     *
     * Reads parameters once per block (control rate), then processes
     * sample-by-sample. Uses a quadrature oscillator for the sine LFO
     * (no per-sample trig calls) and a phase accumulator for square.
     */
    void compute(int count, FAUSTFLOAT** inputs, FAUSTFLOAT** outputs) override {
        FAUSTFLOAT* input0 = inputs[0];
        FAUSTFLOAT* output0 = outputs[0];

        // Read parameters once per block (FAUST convention)
        const float rate = static_cast<float>(fHslider0);
        const float depth = static_cast<float>(fHslider1);
        const float shape = static_cast<float>(fHslider2);

        // Pre-compute angular frequency for this block
        const float omega = fConst0 * rate;
        const float cosW = std::cos(omega);
        const float sinW = std::sin(omega);

        // Phase increment for square wave
        const float phaseInc = rate / static_cast<float>(fSampleRate);

        for (int i = 0; i < count; i++) {
            // ---- Quadrature sine oscillator ----
            // Rotation matrix: [cos -sin; sin cos] applied to [sin, cos]
            const float s = sinState_;
            const float c = cosState_;
            sinState_ = s * cosW - c * sinW;
            cosState_ = s * sinW + c * cosW;

            // Sine LFO normalized to [0, 1]
            const float sineLfo = (sinState_ + 1.0f) * 0.5f;

            // ---- Square LFO ----
            // Phase accumulator wraps at 1.0, output is +1/-1
            fSquarePhase += phaseInc;
            if (fSquarePhase >= 1.0f) {
                fSquarePhase -= 1.0f;
            }
            const float squareVal = (fSquarePhase < 0.5f) ? 1.0f : -1.0f;
            const float squareLfo = (squareVal + 1.0f) * 0.5f;

            // ---- Blend sine and square ----
            const float lfo = sineLfo * (1.0f - shape) + squareLfo * shape;

            // ---- Apply tremolo ----
            // Modulate amplitude: 1.0 at lfo=0, (1-depth) at lfo=1
            output0[i] = input0[i] * (1.0f - depth * lfo);
        }

        // Renormalize quadrature oscillator every block to prevent drift.
        // The rotation matrix accumulates floating-point error over time,
        // causing the oscillator amplitude to slowly grow or decay.
        // Renormalization keeps it on the unit circle.
        // Use epsilon (not 0.0f) to guard against denormals and to prevent
        // -ffinite-math-only from optimizing away the check.
        const float mag = std::sqrt(sinState_ * sinState_ + cosState_ * cosState_);
        if (mag > 1e-20f) {
            const float invMag = 1.0f / mag;
            sinState_ *= invMag;
            cosState_ *= invMag;
        } else {
            // Oscillator collapsed -- reinitialize to unit circle
            sinState_ = 0.0f;
            cosState_ = 1.0f;
        }
    }
};

#endif // FAUST_TREMOLO_H
