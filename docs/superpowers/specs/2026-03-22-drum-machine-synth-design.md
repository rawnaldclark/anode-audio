# Drum Machine Synthesizer — Design Spec

**Date:** 2026-03-22
**Status:** Approved (brainstorming)
**Scope:** Real-time drum machine synthesizer module for the Guitar Emulator Android app

---

## 1. Overview

A built-in drum machine with synthesized drum sounds for jamming on guitar. Inspired by the Elektron Model:Cycles (FM synthesis), Novation Circuit Tracks (sample + synth hybrid), and Teenage Engineering PO-16 (subtractive/wavetable).

**Design principle:** Instant jamming first, deep programming available.

**Primary use cases:**
- A) Set-and-forget: pick a preset pattern, hit play, jam on guitar
- B) Semi-interactive: tweak patterns live — mute/unmute, trigger fills, change tempo
- C) Full programming: build patterns from scratch, deep sound design per voice

---

## 2. Key Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Track count | Variable per preset, up to 8 | Flexible — some presets need 4, some need 8 |
| Synthesis | Multi-engine (6 types per track) | Different drums need different engines |
| Signal routing | Parallel drum bus, mixed at final output | Drums should NOT go through guitar distortion/amp |
| Presets | ~16-20 genre presets + full step editor | Balanced: instant jamming + build your own |
| Genres | Rock, Blues, Funk, Metal, Jazz, Electronic, Lo-fi, Ambient | 8 genres x ~2 patterns each |
| Sequencer depth | Essential + flavor (v1), Elektron-deep (v2+) | Ship fast, grow deep. Data structures support v2 from day one |
| UI layout | Hybrid: 4x4 drum pads + 16-step sequencer row | Finger drumming + step editing on one screen |
| Architecture | Modular component (B) | Matches existing header-only pattern, testable, extensible |
| Library | DaisySP (MIT) + custom header-only | Battle-tested core voices + full flexibility |

---

## 3. Synthesis Engine

### 3.1 Voice Types (6 Engines)

Each of the 8 tracks selects one engine type. The engine determines which parameters are exposed via macro controls.

**FM Drum**
- Signal flow: `[Mod Osc] → FM → [Carrier Osc] → [SVF] → [Amp Env] → out`
- Pitch envelope + modulation index envelope
- Best for: Kicks (808/909), toms, laser/zap FX, bass tones
- Params: Freq, ModRatio, ModDepth, PitchDecay, AmpDecay, FilterCutoff, Drive
- CPU: ~25 FLOPs/sample

**Analog Subtractive**
- Signal flow: `[Tone Osc] + [Noise Gen] → [Mixer] → [SVF] → [Amp Env] → out`
- Filter envelope shapes body vs. click
- Best for: Snares, claps, rimshots, toms
- Params: ToneFreq, ToneDecay, NoiseDecay, NoiseMix, FilterFreq, FilterRes, FilterDecay
- CPU: ~20 FLOPs/sample

**Metallic (808 Cymbal)**
- Signal flow: 6 detuned square oscillators → 2 bandpass filters → HP → envelope
- Frequencies: 205.3, 304.4, 369.6, 522.7, 540, 800 Hz (deliberately inharmonic)
- Best for: Hi-hats (open/closed), cymbals, rides, cowbell
- Params: Color (detune), Tone, Decay, HPCutoff
- CPU: ~40 FLOPs/sample

**Noise Shaper**
- Signal flow: `[White/Pink Noise] → [SVF (LP/HP/BP)] → [Amp Env] → out`
- Best for: Shakers, brushes, white noise sweeps, risers, lo-fi textures
- Params: NoiseType, FilterMode, FilterFreq, FilterRes, Decay, Sweep
- CPU: ~12 FLOPs/sample

**Physical Model**
- Karplus-Strong: `[Noise Burst] → [Delay Line] → [LP Avg] → [Feedback] → out`
- Modal: `[Impulse] → [Mode1 + Mode2 + ... + ModeN] → out`
- Best for: Clave, woodblock, cowbell, marimba, metallic plinks
- Params: ModelType, Pitch, Decay, Brightness, Material, Damping
- CPU: ~15-60 FLOPs/sample (modal cost scales with mode count)

**Multi-Layer**
- Signal flow: `[Sub-Voice A] + [Sub-Voice B] → [Crossfade] → [Drive] → out`
- Combines any two of the 5 non-Multi engine types (no recursive nesting)
- Best for: Layered kicks (FM body + noise click), complex snares, hybrid sounds
- Params: VoiceA type+params, VoiceB type+params, Mix, Drive
- CPU: 2x sub-voice cost

### 3.2 Per-Voice Processing Chain

After the synthesis engine, every voice gets:
```
[Synth Engine] → [Drive/Saturation] → [SVF Filter] → [Amp Envelope] → [Pan] → Track Bus
```
- Drive uses existing `fast_math::fast_tanh` Pade approximant
- SVF filter is TPT form for stability at high cutoff frequencies

### 3.3 Drum Bus Processing

```
[Track 1-8 summed] → [Bus Compressor] → [Bus EQ (3-band)] → [Bus Reverb] → [Master Volume] → Mix with Guitar
```
- Compressor: simple RMS detector + gain reduction
- EQ: 3-band (low/mid/high shelving)
- Reverb: lightweight Dattorro plate (shared algorithm with guitar reverb)

### 3.4 Library Strategy

- **DaisySP** (MIT license, Mutable Instruments Plaits lineage)
  - Pin to latest stable release tag (or specific commit hash at implementation time)
  - Copy only required source files into `app/src/main/cpp/drums/daisysp/`:
    - `Source/Drums/analogbassdrum.h/.cpp`
    - `Source/Drums/analogsnaredrum.h/.cpp`
    - `Source/Drums/syntheticbassdrum.h/.cpp`
    - `Source/Drums/syntheticsnaredrum.h/.cpp`
    - `Source/Drums/hihat.h/.cpp`
    - `Source/Utility/dsp.h` (shared math utilities)
    - `Source/Filters/svf.h/.cpp` (if not using our own SVF)
  - DaisySP uses `float` internally — matches our 32-bit float I/O convention
  - DaisySP calls `Init(sampleRate)` per voice — compatible with runtime-queried rate
  - No DaisySP build system artifacts needed — just source files added to CMakeLists.txt
- Custom header-only implementations for: Noise Shaper, Physical Model, Multi-Layer
- Uses existing `fast_math` utilities from the guitar effects codebase

---

## 4. Sequencer Architecture

### 4.1 Data Structures

```cpp
struct ParamLock {
    uint8_t  paramId;           // 0-31: which parameter is overridden
    float    value;             // normalized 0.0-1.0
};
// sizeof(ParamLock) = 8 bytes (with alignment padding)

enum class TrigCondition : uint8_t {
    NONE = 0,       // always fire (use probability field)
    FILL,           // fire when fill mode active
    NOT_FILL,       // fire when fill mode inactive
    PRE,            // fire if prev conditional on this track was true
    NOT_PRE,
    FIRST,          // first loop only
    NOT_FIRST,
    LAST,           // last loop before pattern change
    NOT_LAST,
    A_B             // cycle-count: fire on cycle condA of condB
};

struct Step {
    bool     trigger;           // note on/off
    uint8_t  velocity;          // 0-127
    uint8_t  probability;       // 1-100% (100 = always fire)
    uint8_t  retrigCount;       // valid v1 values: 0(off), 2, 3, 4, 6, 8
    uint8_t  retrigDecay;       // velocity curve: 0=flat, 1=ramp-up, 2=ramp-down
    // v2 fields (stored, not exposed in UI yet):
    int8_t   microTiming;       // -23 to +23 (384ths of step)
    uint8_t  numParamLocks;     // 0-8
    ParamLock paramLocks[8];    // per-step param overrides
    TrigCondition condition;    // v2: A:B cycle, FILL, PRE, etc.
    uint8_t  condA;             // v2: A in A:B (1-8)
    uint8_t  condB;             // v2: B in A:B (2-8, A <= B)
};
// sizeof(Step) = ~80 bytes (with alignment)

struct Track {
    Step     steps[64];         // max 64 steps per track
    uint8_t  length;            // 1-64 (variable per track for future polymetric)
    uint8_t  engineType;        // FM, Subtractive, Metallic, Noise, Physical, Multi
    float    voiceParams[16];   // synthesis parameters for this track's voice
    float    volume;            // 0.0-1.0
    float    pan;               // -1.0 (L) to 1.0 (R)
    bool     muted;
    int8_t   chokeGroup;        // -1=none, 0=hat group, 1-3=user groups
};

struct Pattern {
    Track    tracks[8];
    uint8_t  length;            // global pattern length (1-64 steps)
    float    swing;             // 50.0 (straight) to 75.0 (heavy shuffle)
    float    bpm;               // 20-300 BPM
    char     name[16];
};

struct DrumKit {
    char     name[32];
    struct { uint8_t engineType; float voiceParams[16]; } tracks[8];
};
```

Memory: Step ~80 bytes, Track ~5.2 KB (64 steps + params), Pattern ~42 KB (8 tracks + metadata). 128 patterns = ~5.4 MB. Acceptable for mobile.

### 4.2 Timing Engine

Sample-accurate clock inside the existing Oboe `onAudioReady()` callback:

```
samplesPerStep = (sampleRate * 60.0) / (bpm * stepsPerBeat)
```

- Double-precision accumulator prevents long-term drift
- Subtract (don't reset) accumulator to preserve fractional remainder
- Triggers fire at exact sample offset within the buffer (~0.02ms accuracy)

### 4.3 Swing

MPC-style: even-numbered 16th notes delayed by a percentage of step duration.
```
offset = (swingPercent - 50.0) / 50.0 * samplesPerStep
```
- 50% = straight, 54-57% = house, 66% = triplet shuffle, 70%+ = jazz/soul

### 4.4 Choke Groups

When any member of a choke group triggers, all other members receive a fast fade-out (~2ms exponential decay). Default group 0 = hi-hat (closed hat chokes open hat).

### 4.5 Tap Tempo

Rolling window of last 8 taps, median inter-tap interval (rejects outliers), 2-second auto-reset timeout.

### 4.6 v1 Features

- Step triggers (on/off per step)
- Velocity (0-127 per step)
- Pattern length (1-64 steps)
- Swing (50-75%, MPC-style)
- Probability (1-100% per step)
- Retrigs (2x/3x/4x/6x/8x per step — flams and rolls)
- Fill mode (hold for one-shot fill pattern)
- Mute/solo per track
- Choke groups (open/closed hi-hat linking)
- Tap tempo
- Pattern bank (8 banks x 16 patterns = 128 total)
- Pattern queuing (next pattern starts on downbeat)
- Half-time / double-time toggle

### 4.7 v2+ Deferred Features (data structures ready)

- Parameter locks (per-step knob automation)
- Conditional trigs (A:B cycle counts, FILL, PRE, 1ST/LST)
- Micro-timing (sub-step nudge +/-23/384)
- Per-track LFO (assignable modulation)
- Polymetric (different length per track)
- Pattern chaining (song arrangement mode)
- Mutation/randomization (Fisher-Yates shuffle of active positions)
- MIDI clock output (24 PPQN)
- MIDI note input (finger drumming from external pads)

---

## 5. Integration Architecture

### 5.1 Audio Callback Flow

```
AudioEngine::onAudioReady(numFrames):
  1. Read guitar input → inputBuffer
  2. signalChain_.process(inputBuffer, outputBuffer, numFrames)   // EXISTING
  3. outputBuffer *= outputGain_                                   // EXISTING
  4. postProcess(outputBuffer) — DC blocker + limiter + NaN guard  // EXISTING
  5. looperEngine_.process(outputBuffer)                           // EXISTING
  6. NEW: drumEngine_.renderBlock(drumBufferL, drumBufferR, numFrames)  // stereo
  7. NEW: Stereo expansion: outputL[i] = outputBuffer[i] + drumBufferL[i] * drumMixLevel_
                            outputR[i] = outputBuffer[i] + drumBufferR[i] * drumMixLevel_
  8. Final soft limiter on stereo output                           // NEW
```

DrumEngine is a peer of SignalChain, not inside it.

**Critical design decisions:**
- **Drums are mixed AFTER the looper** so loop recordings contain only guitar audio. This lets users change drum patterns during loop playback without baking drums into recordings.
- **Drum engine outputs stereo** (two buffers: L/R) because per-voice pan is fundamental to drum mixing. The existing mono guitar signal is duplicated to both channels, then the stereo drum mix is summed on top. This changes the final output expansion step from simple channel duplication to a proper stereo sum.
- A final soft limiter runs on the stereo output to catch any clipping from the guitar+drum sum.

### 5.2 New C++ Files (~10 headers)

```
app/src/main/cpp/drums/
  drum_engine.h          - Top-level: owns sequencer + tracks + bus FX
  sequencer_clock.h      - Sample-accurate clock, swing, tempo
  drum_track.h           - Owns one DrumVoice + step data, trigger/choke/retrig logic
  drum_voice.h           - Abstract base: virtual trigger(), process(), setParam()
  drum_bus.h             - Bus compressor + 3-band EQ + reverb send
  voices/
    fm_voice.h           - FM drum synthesis
    subtractive_voice.h  - Analog subtractive
    metallic_voice.h     - 808-style 6-oscillator metallic
    noise_voice.h        - Filtered noise
    physical_voice.h     - Karplus-Strong + modal synthesis
```

### 5.3 JNI Bridge (~25 new functions)

**Transport (set):** drumSetPlaying, drumSetTempo, drumTapTempo, drumSetSwing, drumSetPattern, drumSetMixLevel, drumTriggerFill

**Transport (get):** drumIsPlaying, drumGetTempo, drumGetCurrentStep, drumGetCurrentPattern

**Track Control (set):** drumSetTrackMute, drumSetTrackVolume, drumSetTrackPan, drumSetTrackEngine, drumSetVoiceParam, drumTriggerPad

**Track Control (get):** drumGetTrackMute, drumGetTrackVolume, drumGetVoiceParam

**Pattern Editing:** drumSetStep, drumSetStepProb, drumSetStepRetrig, drumSetPatternLength, drumGetPattern (returns full JSON snapshot), drumLoadKit, drumLoadPattern

**Bulk State:** drumGetStateSnapshot (returns JSON of all transport + track + pattern state — used on screen rotation / process death recovery)

### 5.4 New Kotlin Files (~10)

```
audio/DrumEngine.kt            - JNI external fun declarations
viewmodel/DrumViewModel.kt     - UI state + business logic
data/DrumPattern.kt            - Pattern/Track/Step data classes
data/DrumKit.kt                - Kit preset data classes
data/DrumRepository.kt         - JSON load/save + factory presets
ui/drums/DrumMachineScreen.kt  - Main drum screen (hybrid layout)
ui/drums/DrumPadGrid.kt        - 4x4 velocity-sensitive pads
ui/drums/StepSequencerRow.kt   - 16-step editor strip
ui/drums/DrumMixerPanel.kt     - Per-track volume/pan/mute
ui/drums/DrumKitEditor.kt      - Voice parameter knobs (overlay)
```

### 5.5 Modified Files (~7)

- `cpp/audio_engine.h/cpp` — Add DrumEngine member + renderBlock call
- `cpp/jni_bridge.cpp` — Add ~20 drum JNI functions
- `cpp/CMakeLists.txt` — Add drums/ directory
- `kotlin/AudioEngine.kt` — Add drum JNI externals
- `kotlin/AudioViewModel.kt` — Drum state coordination
- `kotlin/MainActivity.kt` — Navigation to drum screen
- `kotlin/ui/PedalboardScreen.kt` — Add drum machine entry point

---

## 6. UI Design

### 6.1 Layout: Hybrid Pad + Sequencer

The drum machine screen uses a hybrid layout inspired by Circuit Tracks (pad grid) and Model:Cycles (step row):

**Top to bottom:**
1. **Top Bar** — brushed metal gradient, "DRUM MACHINE" inset panel, kit name in gold
2. **Transport Bar** — Play/Stop (chrome stomp buttons), BPM display (recessed), FILL button, Swing readout, TAP tempo
3. **Pattern Bank** — 16 slots per bank (8 banks = 128 total), amber glow on active, tap to queue next
4. **4x4 Drum Pad Grid** — velocity-sensitive pads, color-coded by category:
   - Red (#E04040): Kick, Low Tom, Mid Tom
   - Amber (#E8B040): Snare, Clap, Hi Tom
   - Green (#40C848): Closed Hat, Open Hat, Crash, Ride
   - Blue (#4088E0): Rim, Shaker, Cowbell, Clave, Perc 1-2
   - Jewel LED on each pad shows active steps
   - Tap to finger drum live; also selects track for step editing
5. **Track Select Strip** — 8 track chips matching pad colors, selects step sequencer target
6. **16-Step Sequencer Row** — tap to toggle steps, velocity as brightness + mini bar graph, beat markers on 1/5/9/13, green playback cursor, long-press for probability/retrig
7. **Track Mixer Strip** — Vol/Pan/Tone/Decay knobs, engine badge (FM/SUB/MTL/NOI/PHY), Mute/Solo, "EDIT →" for full voice editor
8. **Bottom Nav** — KITS / PLAY / EDIT / MIXER / PRESETS

### 6.2 Interaction Model

- **Default (PLAY mode):** Pads trigger sounds, transport runs. Tapping a pad also selects it for the step sequencer below. No mode switches needed for basic use.
- **EDIT mode (bottom nav):** Unlocks per-step probability, retrig count, retrig velocity curve editing. Long-press steps to access.
- **KITS mode:** Full voice parameter editor per track (EffectEditorSheet-style overlay with RotaryKnobs for all synthesis params).
- **MIXER mode:** All 8 tracks visible with LevelMeters, RotaryKnobs for volume/pan, Mute/Solo buttons. Bus compressor/EQ/reverb controls.
- **PRESETS mode:** Save/load full drum presets (kit + patterns). Factory preset browser with genre categories.

### 6.3 Design System Integration

All components use existing DesignSystem.kt tokens:
- Pad backgrounds: category-tinted gradients on `ModuleSurface`
- LEDs: `LedIndicator` jewel style with `JewelRed/Green/Amber` + Blue
- Knobs: `RotaryKnob` with numbered scale ring
- Transport buttons: `StompSwitch` chrome style
- Step grid: `LedIndicator`-inspired cells
- Panel borders: `drawPanelBorder()` extension
- Typography: monospace `CreamWhite` headers, `TextSecondary` labels

---

## 7. Factory Presets (~16-20)

### Rock (3)
- **Straight Rock** — Kick on 1/3, snare on 2/4, closed hat 8ths. 120 BPM.
- **Driving Rock** — Kick pattern with anticipations, crash on 1. 130 BPM.
- **Half-Time Rock** — Snare on 3 only, open hat on upbeats. 80 BPM.

### Blues (2)
- **Blues Shuffle** — 12/8 feel, 66% swing, brush snare, sparse kick. 95 BPM.
- **Slow Blues** — Triplet hi-hat, ghost notes on snare. 70 BPM.

### Funk (2)
- **Tight Funk** — Syncopated 16th kick, ghost notes, tight closed hat. 100 BPM.
- **Breakbeat Funk** — Amen-style breakbeat pattern. 110 BPM.

### Metal (2)
- **Thrash Metal** — Double kick 16ths, snare on 2/4, fast hat. 180 BPM.
- **Blast Beat** — Alternating kick/snare 16ths, ride bell. 200 BPM.

### Jazz (2)
- **Jazz Ride** — Ride pattern with swing, kick comping, brush snare ghost notes. 140 BPM.
- **Bossa Jazz** — Ride + rim click pattern, sparse kick. 120 BPM.

### Electronic (3)
- **808 Boom Bap** — Classic 808 kit, kick/snare/hat, 58% swing. 90 BPM.
- **Four on the Floor** — 909-style kick on every beat, open hat on upbeats. 128 BPM.
- **Synthwave** — Gated reverb snare, LinnDrum-style kit. 110 BPM.

### Lo-fi (2)
- **Lo-fi Chill** — Lazy swing (70%), soft velocity, dusty hat. 80 BPM.
- **Boom Bap** — Hard kick/snare, probability on hat, ghost rim. 85 BPM.

### Ambient (2)
- **Sparse Texture** — Minimal kick/rim, long decay metallic, probability everywhere. 75 BPM.
- **Post-Rock Build** — Ride swells, kick on 1 only, shaker 16ths building. 100 BPM.

---

## 8. CPU Budget

Based on research (SynthMark benchmarks, Oboe performance data):

- 8 FM drum voices: ~11.5 MFLOPS = **<1% of a single ARM core** (worst case: Cortex-A55 little core at 1.8 GHz = ~14.4 GFLOPS theoretical, 11.5M/14.4G = 0.08%)
- Drum bus effects (comp + EQ + reverb): ~5% additional
- Guitar chain (heavy preset): ~50-70% of callback budget
- **Total with drums: ~55-75%** — well within budget with 25% headroom

Drums are computationally trivial compared to the guitar chain. The main engineering effort is UI and sequencer logic, not DSP.

---

## 9. Estimated Scope

- **~10 new C++ headers** (~3,000-4,000 lines)
- **~10 new Kotlin files** (~2,000-3,000 lines)
- **~7 modified files**
- **~25 new JNI functions**
- **No new dependencies** except DaisySP source files (MIT, copied in)
- **Storage**: ~50 KB for 64 patterns as JSON, ~100-200 KB for factory presets in APK

---

## 10. Thread Safety

The sequencer clock and voice rendering run on the **audio thread** (Oboe callback). Pattern editing and transport control are called from the **UI thread** via JNI.

**Approach: Atomic fields + double-buffered pattern swap**

- **Transport controls** (play/stop, tempo, swing): Use `std::atomic<float>` / `std::atomic<bool>`. Audio thread reads atomically. Same pattern as existing effect parameters.
- **Single-step edits** (toggle trigger, set velocity, set probability): Pack step fields into a 64-bit struct and use `std::atomic<uint64_t>` for lock-free step writes. Audio thread reads the atomic value each time it processes that step.
- **Bulk pattern loads** (drumLoadPattern, drumLoadKit): Double-buffered swap. Write new pattern into inactive buffer, then atomically swap the pointer. Audio thread always reads from the active buffer. Same pattern as the existing `setEffectOrder` double-buffered swap in SignalChain.
- **Voice parameter changes** (drumSetVoiceParam): `std::atomic<float>` per parameter, read by audio thread at block boundaries.

---

## 11. Error Handling

- **Voice allocation failure**: DrumEngine pre-allocates all 8 track voices at startup (fixed, no dynamic allocation). If DaisySP Init() fails, that track falls back to a silent passthrough voice and logs a warning via the existing engine error callback.
- **Corrupted pattern JSON**: DrumRepository validates JSON on load. Malformed files are skipped with a warning; the pattern slot falls back to an empty pattern. Same approach as existing preset loading.
- **CPU budget exceeded**: If the audio callback exceeds its deadline, the existing Oboe error recovery (exponential backoff restart) handles the xrun. The drum engine adds negligible CPU (<1%), so this is extremely unlikely to be caused by drums. If it occurs, drums continue running after the restart — no special handling needed.
- **NaN/Inf in drum output**: The final soft limiter (step 8 in callback flow) catches any NaN/Inf from drum synthesis, same as the existing NaN guard for guitar output.

---

## 12. Persistence

- **Pattern storage**: JSON files in app internal storage at `files/drum_patterns/`. One file per pattern: `pattern_A01.json` through `pattern_H16.json`. Same directory convention as guitar presets.
- **Kit storage**: JSON files at `files/drum_kits/`. Kit files store voice params only (no pattern data), allowing mix-and-match.
- **Factory presets**: Shipped in APK `assets/drum_presets/`. Loaded on first launch (or overwritten on app update, preserving user favorites — same logic as existing `loadFactoryPresets()`).
- **Session state**: Current pattern index, transport state (playing/stopped), tempo, and mix level persisted in Jetpack DataStore Preferences alongside existing guitar preferences (last preset ID, input source, buffer multiplier).
- **Backward compatibility**: Pattern JSON includes a `version` field. Future v2 fields (paramLocks, conditions) are stored as optional JSON fields. v1 reader ignores unknown fields. v2 reader fills missing fields with defaults.

---

## 13. Testing Strategy

- **Sequencer clock accuracy**: Unit test that verifies trigger sample offsets match expected positions for known BPM/swing values over 1000+ steps. Test non-integer samplesPerStep accumulation for drift.
- **Pattern serialization**: Round-trip test: create pattern → serialize to JSON → deserialize → compare all fields. Test backward compat with v1/v2 version fields.
- **Choke group behavior**: Unit test: trigger open hat, then closed hat, verify open hat envelope decays to zero within 2ms.
- **Swing math**: Parametric test with swing values 50/58/66/75%, verify even-step offsets match expected sample counts.
- **Voice synthesis**: Smoke tests for each engine type: trigger, process N samples, verify output is non-zero and within [-1, 1] range, verify envelope decays to silence.
- **JNI smoke tests**: Kotlin instrumented tests calling each JNI function to verify no crashes or JNI signature mismatches.
- **Thread safety**: Stress test: UI thread rapidly sets steps while audio thread reads. Verify no crashes, no corrupted data (using TSan if available).
- **UI tests**: Compose preview tests for DrumPadGrid, StepSequencerRow to verify rendering without crashes.

---

## 14. Out of Scope (v1)

- MIDI clock output/input
- External footswitch integration (future: Bluetooth MIDI)
- Sample import/playback engine
- Song arrangement mode
- Parameter locks UI
- Conditional trigs UI
- Per-track LFO
- Audio export of drum patterns
