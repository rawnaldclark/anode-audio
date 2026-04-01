#ifndef GUITAR_ENGINE_FAUST_EFFECT_H
#define GUITAR_ENGINE_FAUST_EFFECT_H

#include "effects/effect.h"
#include "faust/faust_dsp.h"

#include <vector>
#include <string>
#include <cstring>

/**
 * Generic adapter that bridges a FAUST-generated DSP class to our Effect
 * interface, enabling FAUST .dsp files to be used as drop-in effects in
 * the signal chain.
 *
 * Template parameter T must be a class derived from dsp (the FAUST base class)
 * with the standard FAUST-generated interface: init(), compute(),
 * buildUserInterface(), instanceClear().
 *
 * This adapter handles:
 *   1. Audio I/O translation: Effect::process(float*, int) is mono in-place,
 *      while FAUST::compute() uses float** arrays. The adapter wraps the
 *      mono buffer into the pointer-of-pointer format FAUST expects.
 *   2. Parameter mapping: FAUST uses hierarchical string paths for parameters
 *      (e.g., "/Tremolo/Rate"), while our Effect uses integer paramIds.
 *      MapUI auto-discovers parameters, and the adapter maps paramId N to
 *      the Nth parameter registered during buildUserInterface().
 *   3. Lifecycle: setSampleRate() calls FAUST's init(), reset() calls
 *      instanceClear().
 *
 * Usage:
 *   // FaustTremolo is a FAUST-generated class derived from dsp
 *   auto tremolo = std::make_unique<FaustEffect<FaustTremolo>>();
 *   tremolo->setSampleRate(48000);
 *   tremolo->setParameter(0, 6.0f);  // Rate = 6 Hz
 *   tremolo->process(buffer, 256);
 *
 * Thread safety:
 *   Parameters are stored as FAUSTFLOAT member variables inside the FAUST
 *   class (written via setParameter, read in compute). FAUST reads parameters
 *   once at the start of compute() into local variables, so there are no
 *   torn reads mid-buffer. Single-word float writes are atomic on ARM and
 *   x86, making this safe for the UI-thread-writes / audio-thread-reads
 *   pattern used by the rest of the signal chain.
 */
template <typename T>
class FaustEffect : public Effect {
public:
    FaustEffect() {
        // Build the parameter map. FAUST's buildUserInterface() calls
        // methods on MapUI to register each controllable parameter.
        // After this, mapUI_ knows every parameter's path and pointer.
        faustDsp_.buildUserInterface(&mapUI_);
    }

    // =========================================================================
    // Effect interface implementation
    // =========================================================================

    /**
     * Process audio through the FAUST DSP.
     *
     * Wraps the mono in-place buffer into FAUST's float** format and
     * delegates to T::compute(). The FAUST-generated compute() reads
     * parameters once per block and processes sample-by-sample.
     *
     * For mono effects (1 input, 1 output), the same buffer pointer is
     * used for both input and output. FAUST code generated with -inpl
     * (in-place) or -scal (scalar) mode supports this safely.
     */
    void process(float* buffer, int numFrames) override {
        // FAUST uses float** (array-of-channel-pointers) for I/O.
        // For mono: inputs[0] and outputs[0] both point to our buffer.
        FAUSTFLOAT* inputs[1] = { buffer };
        FAUSTFLOAT* outputs[1] = { buffer };
        faustDsp_.compute(numFrames, inputs, outputs);
    }

    /**
     * Set the sample rate and initialize the FAUST DSP.
     *
     * FAUST's init() computes all sample-rate-dependent constants
     * (filter coefficients, delay line lengths, etc.) and resets state.
     * Since init() calls instanceResetUserInterface() which overwrites
     * parameter zones to defaults, we save and restore parameter values
     * to survive mid-session sample rate changes (e.g., Oboe stream restart).
     */
    void setSampleRate(int32_t sampleRate) override {
        // Save current parameter values before init() resets them
        const int paramCount = mapUI_.getParamsCount();
        std::vector<float> savedValues(paramCount);
        for (int i = 0; i < paramCount; ++i) {
            savedValues[i] = mapUI_.getParamValue(mapUI_.getParamPath(i));
        }

        faustDsp_.init(sampleRate);

        // Restore parameter values that were overwritten by init()
        for (int i = 0; i < paramCount; ++i) {
            mapUI_.setParamValue(mapUI_.getParamPath(i), savedValues[i]);
        }
    }

    /**
     * Reset internal DSP state without changing sample rate or parameters.
     *
     * FAUST's instanceClear() zeros all delay lines, filter history,
     * and recursive state variables, but preserves the current sample
     * rate and parameter values.
     */
    void reset() override {
        faustDsp_.instanceClear();
    }

    /**
     * Set a parameter by integer index.
     *
     * Maps paramId to the Nth FAUST parameter (in registration order)
     * and writes the value directly to the FAUST zone pointer. The value
     * is clamped to the parameter's declared [min, max] range.
     *
     * @param paramId Zero-based parameter index (registration order).
     * @param value   New parameter value (clamped to declared range).
     */
    void setParameter(int paramId, float value) override {
        if (paramId < 0 || paramId >= mapUI_.getParamsCount()) return;

        const std::string& path = mapUI_.getParamPath(paramId);
        float minVal = mapUI_.getParamMin(path);
        float maxVal = mapUI_.getParamMax(path);
        float clamped = std::max(minVal, std::min(maxVal, value));
        mapUI_.setParamValue(path, clamped);
    }

    /**
     * Get a parameter by integer index.
     *
     * @param paramId Zero-based parameter index (registration order).
     * @return Current parameter value, or 0.0f if paramId is out of range.
     */
    float getParameter(int paramId) const override {
        if (paramId < 0 || paramId >= mapUI_.getParamsCount()) return 0.0f;

        const std::string& path = mapUI_.getParamPath(paramId);
        return mapUI_.getParamValue(path);
    }

    /** Get the number of FAUST parameters discovered. */
    int getParameterCount() const {
        return mapUI_.getParamsCount();
    }

    /** Get the FAUST path of a parameter by index. */
    const std::string& getParameterPath(int paramId) const {
        return mapUI_.getParamPath(paramId);
    }

private:
    T faustDsp_;
    MapUI mapUI_;
};

#endif // GUITAR_ENGINE_FAUST_EFFECT_H
