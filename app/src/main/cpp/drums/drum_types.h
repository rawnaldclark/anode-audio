#ifndef GUITAR_ENGINE_DRUM_TYPES_H
#define GUITAR_ENGINE_DRUM_TYPES_H

#include <cstdint>
#include <cstring>

/**
 * Core data structures for the drum machine sequencer.
 *
 * These structs define the pattern/track/step hierarchy used by the
 * sequencer clock and drum engine. Layout follows the Elektron-inspired
 * design spec (section 4.1) with v2 fields included in storage but
 * not exposed in v1 UI.
 *
 * Thread safety:
 *   - Pattern data is double-buffered in DrumEngine. The UI thread writes
 *     to the inactive buffer and atomically swaps the pointer. The audio
 *     thread reads only from the active buffer.
 *   - Individual Step fields are small enough to be read/written atomically
 *     on 32/64-bit ARM (naturally aligned loads/stores are atomic).
 */

namespace drums {

/**
 * Per-step parameter override (Elektron-style "parameter lock").
 * Allows any synthesis parameter to be overridden on a specific step.
 * v2 feature -- data structure stored from day one, UI deferred.
 */
struct ParamLock {
    uint8_t paramId;    ///< 0-31: which voice parameter is overridden
    uint8_t pad_[3];    ///< Alignment padding to 4-byte boundary
    float   value;      ///< Normalized parameter value 0.0-1.0
};
static_assert(sizeof(ParamLock) == 8, "ParamLock must be 8 bytes");

/**
 * Conditional trigger types (v2 feature, data stored from day one).
 *
 * Controls whether a step fires based on sequencer state rather than
 * simple probability. Inspired by Elektron Digitakt conditional trigs.
 */
enum class TrigCondition : uint8_t {
    NONE = 0,       ///< Always fire (falls through to probability check)
    FILL,           ///< Fire only when fill mode is active
    NOT_FILL,       ///< Fire only when fill mode is inactive
    PRE,            ///< Fire if previous conditional trig on this track was true
    NOT_PRE,        ///< Fire if previous conditional trig on this track was false
    FIRST,          ///< Fire on first loop iteration only
    NOT_FIRST,      ///< Fire on all iterations except the first
    LAST,           ///< Fire on last loop before pattern change
    NOT_LAST,       ///< Fire on all loops except the last before change
    A_B             ///< Cycle-count: fire on cycle condA of every condB cycles
};

/**
 * Single sequencer step within a track.
 *
 * Contains the trigger flag, velocity, probability, retrig settings,
 * and v2-deferred fields (micro-timing, parameter locks, conditions).
 * Steps are stored in fixed-size arrays within Track for cache locality.
 */
struct Step {
    bool     trigger;           ///< Note on (true) or off (false)
    uint8_t  velocity;          ///< 0-127 (MIDI-style)
    uint8_t  probability;       ///< 1-100 (percentage, 100 = always fire)
    uint8_t  retrigCount;       ///< 0=off, 2, 3, 4, 6, 8 sub-triggers per step
    uint8_t  retrigDecay;       ///< Velocity curve: 0=flat, 1=ramp-up, 2=ramp-down

    // --- v2 fields (stored, not exposed in v1 UI) ---
    int8_t        microTiming;      ///< -23 to +23 (384ths of step nudge)
    uint8_t       numParamLocks;    ///< 0-8 active parameter locks
    uint8_t       pad_;             ///< Alignment padding
    ParamLock     paramLocks[8];    ///< Per-step parameter overrides
    TrigCondition condition;        ///< Conditional trigger type
    uint8_t       condA;            ///< A in A:B cycle count (1-8)
    uint8_t       condB;            ///< B in A:B cycle count (2-8, A <= B)
    uint8_t       pad2_;            ///< Final alignment padding

    /** Initialize step to safe defaults (off, full velocity, 100% probability). */
    void clear() {
        trigger = false;
        velocity = 100;
        probability = 100;
        retrigCount = 0;
        retrigDecay = 0;
        microTiming = 0;
        numParamLocks = 0;
        pad_ = 0;
        std::memset(paramLocks, 0, sizeof(paramLocks));
        condition = TrigCondition::NONE;
        condA = 1;
        condB = 1;
        pad2_ = 0;
    }
};

/**
 * Voice engine types available for each drum track.
 *
 * Each track selects one engine type which determines the synthesis
 * algorithm and which parameters are exposed via macro controls.
 */
enum class EngineType : uint8_t {
    FM = 0,             ///< 2-op FM synthesis (kicks, toms, zaps)
    SUBTRACTIVE,        ///< Analog subtractive (snares, claps, rimshots)
    METALLIC,           ///< 808-style 6-osc metallic (hats, cymbals)
    NOISE,              ///< Filtered noise shaper (shakers, brushes)
    PHYSICAL,           ///< Karplus-Strong / modal (clave, woodblock)
    MULTI_LAYER,        ///< Two sub-voices layered (hybrid sounds)
    ENGINE_TYPE_COUNT
};

/**
 * One track in a pattern. Owns step data + voice configuration.
 *
 * Stores up to 64 steps (supporting polymetric lengths), the voice
 * engine type and its parameters, and mix settings (volume/pan/mute).
 */
struct Track {
    Step     steps[64];         ///< Max 64 steps per track
    uint8_t  length;            ///< 1-64 (track-specific length for polymetric)
    uint8_t  engineType;        ///< EngineType cast to uint8_t
    uint8_t  pad_[2];           ///< Alignment padding
    float    voiceParams[16];   ///< Synthesis parameters for this track's voice
    float    volume;            ///< 0.0-1.0 track output level
    float    pan;               ///< -1.0 (hard L) to 1.0 (hard R)
    bool     muted;             ///< Muted tracks produce no output
    int8_t   chokeGroup;        ///< -1=none, 0=hat group, 1-3=user choke groups
    uint8_t  pad2_[2];          ///< Final alignment padding

    /** Initialize track to safe defaults. */
    void clear() {
        for (int i = 0; i < 64; ++i) {
            steps[i].clear();
        }
        length = 16;
        engineType = static_cast<uint8_t>(EngineType::FM);
        pad_[0] = pad_[1] = 0;
        std::memset(voiceParams, 0, sizeof(voiceParams));
        volume = 0.8f;
        pan = 0.0f;
        muted = false;
        chokeGroup = -1;
        pad2_[0] = pad2_[1] = 0;
    }
};

/**
 * Complete sequencer pattern containing up to 8 tracks.
 *
 * Patterns are double-buffered in DrumEngine for lock-free swapping
 * between UI and audio threads. Memory footprint is ~42KB per pattern.
 */
struct Pattern {
    Track    tracks[8];         ///< 8 drum tracks
    uint8_t  length;            ///< Global pattern length 1-64 steps
    uint8_t  pad_[3];           ///< Alignment padding
    float    swing;             ///< 50.0 (straight) to 75.0 (heavy shuffle)
    float    bpm;               ///< 20.0-300.0 beats per minute
    char     name[16];          ///< Null-terminated pattern name

    /** Initialize pattern to safe defaults. */
    void clear() {
        for (int i = 0; i < 8; ++i) {
            tracks[i].clear();
        }
        length = 16;
        pad_[0] = pad_[1] = pad_[2] = 0;
        swing = 50.0f;
        bpm = 120.0f;
        std::memset(name, 0, sizeof(name));
        std::strncpy(name, "Init", sizeof(name) - 1);
    }
};

/**
 * A drum kit stores voice engine types and parameters without pattern data.
 * Allows mix-and-match: load a kit onto any pattern to change sounds
 * without altering the step sequence.
 */
struct DrumKit {
    char name[32];              ///< Null-terminated kit name
    struct TrackVoice {
        uint8_t engineType;     ///< EngineType cast to uint8_t
        uint8_t pad_[3];        ///< Alignment padding
        float   voiceParams[16]; ///< Synthesis parameters
    } tracks[8];

    /** Initialize kit to safe defaults. */
    void clear() {
        std::memset(name, 0, sizeof(name));
        std::strncpy(name, "Init Kit", sizeof(name) - 1);
        for (int i = 0; i < 8; ++i) {
            tracks[i].engineType = static_cast<uint8_t>(EngineType::FM);
            tracks[i].pad_[0] = tracks[i].pad_[1] = tracks[i].pad_[2] = 0;
            std::memset(tracks[i].voiceParams, 0, sizeof(tracks[i].voiceParams));
        }
    }
};

} // namespace drums

#endif // GUITAR_ENGINE_DRUM_TYPES_H
