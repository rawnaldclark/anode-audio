#ifndef GUITAR_ENGINE_FAUST_DSP_H
#define GUITAR_ENGINE_FAUST_DSP_H

/**
 * Minimal FAUST compatibility layer for pre-generated C++ DSP headers.
 *
 * When FAUST compiles .dsp files with the -i (inline) flag, it embeds
 * its own definitions of these types. Without -i, the generated code
 * expects these definitions from the FAUST SDK headers. This file
 * provides standalone definitions so pre-generated FAUST headers can
 * compile without the FAUST SDK installed.
 *
 * The types here are API-compatible with FAUST 2.x generated code:
 *   - FAUSTFLOAT: the sample/parameter precision type
 *   - dsp: abstract base class for all FAUST-generated DSP classes
 *   - Meta: interface for DSP metadata (name, author, etc.)
 *   - UI: interface for parameter discovery and control
 *   - MapUI: concrete UI implementation that maps string paths to pointers
 *
 * Reference: https://faustdoc.grame.fr/manual/architectures/
 */

#include <string>
#include <map>
#include <cmath>

// FAUST uses FAUSTFLOAT for all audio buffers and parameter values.
// Default to float (single precision) for mobile performance.
#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif

// =============================================================================
// Meta interface - receives DSP metadata key/value pairs
// =============================================================================

/**
 * Interface for FAUST DSP metadata.
 * FAUST-generated classes call metadata(Meta*) to report their
 * name, author, version, license, and other information.
 */
struct Meta {
    virtual ~Meta() = default;
    virtual void declare(const char* key, const char* value) = 0;
};

// =============================================================================
// UI interface - discovers parameters and their ranges
// =============================================================================

/**
 * Interface for FAUST parameter discovery and control.
 *
 * FAUST-generated classes call buildUserInterface(UI*) to register
 * all their controllable parameters. Each parameter has:
 *   - A hierarchical label path (e.g., "Tremolo/Rate")
 *   - A pointer to the FAUSTFLOAT variable controlling it
 *   - Min/max/step bounds
 *   - An initial value
 *
 * The UI groups parameters into vertical/horizontal/tab boxes for
 * visual layout. Our adapter ignores the layout and only captures
 * the parameter addresses.
 */
struct UI {
    virtual ~UI() = default;

    // Layout grouping (ignored by our headless adapter)
    virtual void openVerticalBox(const char* label) = 0;
    virtual void openHorizontalBox(const char* label) = 0;
    virtual void openTabBox(const char* label) = 0;
    virtual void closeBox() = 0;

    // Active widgets (user-controllable parameters)
    virtual void addHorizontalSlider(const char* label, FAUSTFLOAT* zone,
        FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) = 0;
    virtual void addVerticalSlider(const char* label, FAUSTFLOAT* zone,
        FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) = 0;
    virtual void addNumEntry(const char* label, FAUSTFLOAT* zone,
        FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) = 0;
    virtual void addButton(const char* label, FAUSTFLOAT* zone) = 0;
    virtual void addCheckButton(const char* label, FAUSTFLOAT* zone) = 0;

    // Passive widgets (read-only displays)
    virtual void addHorizontalBargraph(const char* label, FAUSTFLOAT* zone,
        FAUSTFLOAT min, FAUSTFLOAT max) = 0;
    virtual void addVerticalBargraph(const char* label, FAUSTFLOAT* zone,
        FAUSTFLOAT min, FAUSTFLOAT max) = 0;
};

// =============================================================================
// dsp base class - abstract base for all FAUST-generated DSP classes
// =============================================================================

/**
 * Abstract base class for FAUST-generated DSP processors.
 *
 * FAUST generates a concrete subclass with:
 *   - init(sample_rate): initializes constants and state
 *   - compute(count, inputs, outputs): processes one block of audio
 *   - buildUserInterface(UI*): registers parameters
 *   - metadata(Meta*): reports metadata
 *   - getNumInputs()/getNumOutputs(): channel counts
 */
class dsp {
public:
    virtual ~dsp() = default;

    virtual int getNumInputs() = 0;
    virtual int getNumOutputs() = 0;

    virtual void buildUserInterface(UI* ui_interface) = 0;

    virtual int getSampleRate() = 0;

    virtual void init(int sample_rate) = 0;
    virtual void instanceInit(int sample_rate) = 0;
    virtual void instanceConstants(int sample_rate) = 0;
    virtual void instanceResetUserInterface() = 0;
    virtual void instanceClear() = 0;

    virtual dsp* clone() = 0;

    virtual void metadata(Meta* m) = 0;

    virtual void compute(int count, FAUSTFLOAT** inputs, FAUSTFLOAT** outputs) = 0;
};

// =============================================================================
// MapUI - maps FAUST parameter paths to their memory locations
// =============================================================================

/**
 * Concrete UI implementation that captures parameter addresses by path.
 *
 * When a FAUST DSP calls buildUserInterface(&mapUI), this class records
 * every parameter's address and its hierarchical path. Parameters can then
 * be set/read by path string:
 *
 *   MapUI mapUI;
 *   myDsp.buildUserInterface(&mapUI);
 *   mapUI.setParamValue("/Tremolo/Rate", 6.0f);
 *
 * The path is built from nested box labels separated by "/".
 */
class MapUI : public UI {
public:
    MapUI() = default;

    // Layout grouping: track the current path prefix
    void openVerticalBox(const char* label) override {
        pushLabel(label);
    }
    void openHorizontalBox(const char* label) override {
        pushLabel(label);
    }
    void openTabBox(const char* label) override {
        pushLabel(label);
    }
    void closeBox() override {
        if (!pathStack_.empty()) {
            pathStack_.pop_back();
        }
    }

    // Active widgets: register the parameter zone
    void addHorizontalSlider(const char* label, FAUSTFLOAT* zone,
            FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) override {
        addParameter(label, zone, init, min, max, step);
    }
    void addVerticalSlider(const char* label, FAUSTFLOAT* zone,
            FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) override {
        addParameter(label, zone, init, min, max, step);
    }
    void addNumEntry(const char* label, FAUSTFLOAT* zone,
            FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) override {
        addParameter(label, zone, init, min, max, step);
    }
    void addButton(const char* label, FAUSTFLOAT* zone) override {
        addParameter(label, zone, 0.0f, 0.0f, 1.0f, 1.0f);
    }
    void addCheckButton(const char* label, FAUSTFLOAT* zone) override {
        addParameter(label, zone, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    // Passive widgets: register but these are read-only outputs
    void addHorizontalBargraph(const char* label, FAUSTFLOAT* zone,
            FAUSTFLOAT min, FAUSTFLOAT max) override {
        (void)label; (void)zone; (void)min; (void)max;
    }
    void addVerticalBargraph(const char* label, FAUSTFLOAT* zone,
            FAUSTFLOAT min, FAUSTFLOAT max) override {
        (void)label; (void)zone; (void)min; (void)max;
    }

    // ---- Parameter access API ----

    /** Set a parameter by its full path (e.g., "/Tremolo/Rate"). */
    void setParamValue(const std::string& path, FAUSTFLOAT value) {
        auto it = paramMap_.find(path);
        if (it != paramMap_.end()) {
            *(it->second.zone) = value;
        }
    }

    /** Get a parameter value by its full path. Returns 0 if not found. */
    FAUSTFLOAT getParamValue(const std::string& path) const {
        auto it = paramMap_.find(path);
        if (it != paramMap_.end()) {
            return *(it->second.zone);
        }
        return 0.0f;
    }

    /** Get the number of registered parameters. */
    int getParamsCount() const {
        return static_cast<int>(orderedPaths_.size());
    }

    /** Get the path of a parameter by its registration order (0-based index). */
    const std::string& getParamPath(int index) const {
        static const std::string empty;
        if (index < 0 || index >= static_cast<int>(orderedPaths_.size())) {
            return empty;
        }
        return orderedPaths_[index];
    }

    /** Get the minimum value for a parameter by path. */
    FAUSTFLOAT getParamMin(const std::string& path) const {
        auto it = paramMap_.find(path);
        return (it != paramMap_.end()) ? it->second.min : 0.0f;
    }

    /** Get the maximum value for a parameter by path. */
    FAUSTFLOAT getParamMax(const std::string& path) const {
        auto it = paramMap_.find(path);
        return (it != paramMap_.end()) ? it->second.max : 1.0f;
    }

    /** Get the default value for a parameter by path. */
    FAUSTFLOAT getParamInit(const std::string& path) const {
        auto it = paramMap_.find(path);
        return (it != paramMap_.end()) ? it->second.init : 0.0f;
    }

private:
    struct ParamInfo {
        FAUSTFLOAT* zone;
        FAUSTFLOAT init;
        FAUSTFLOAT min;
        FAUSTFLOAT max;
        FAUSTFLOAT step;
    };

    void pushLabel(const char* label) {
        if (label && label[0] != '\0') {
            pathStack_.push_back(label);
        }
    }

    std::string buildPath(const char* label) const {
        std::string path = "/";
        for (const auto& seg : pathStack_) {
            path += seg + "/";
        }
        path += label;
        return path;
    }

    void addParameter(const char* label, FAUSTFLOAT* zone,
                      FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) {
        std::string path = buildPath(label);
        paramMap_[path] = {zone, init, min, max, step};
        orderedPaths_.push_back(path);
    }

    std::map<std::string, ParamInfo> paramMap_;
    std::vector<std::string> orderedPaths_;
    std::vector<std::string> pathStack_;
};

#endif // GUITAR_ENGINE_FAUST_DSP_H
