# Drum Machine Synthesizer — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a real-time drum machine synthesizer with 6 voice engines, 8-track step sequencer, and hybrid pad+sequencer UI to the Guitar Emulator Android app.

**Architecture:** Parallel drum bus mixed after looper in the Oboe callback. Modular C++ headers (drum_engine → sequencer_clock → drum_track → drum_voice subclasses → drum_bus). Kotlin ViewModel + Compose UI following existing patterns. All custom header-only voice implementations (DaisySP was evaluated but dropped in favor of custom code to avoid external dependency management and ensure full control over DSP — the voices are simple enough that custom implementations are straightforward).

**Tech Stack:** C++17 (header-only, Oboe audio callback), Kotlin (JNI bridge, ViewModel, Jetpack DataStore), Jetpack Compose (skeuomorphic UI).

**Spec:** `docs/superpowers/specs/2026-03-22-drum-machine-synth-design.md`

---

## Phase 1: C++ Core Engine (Tasks 1-6) — PARALLELIZABLE

These tasks build the C++ drum engine. Tasks 1-5 are independent voice implementations that can run in parallel. Task 6 integrates them.

### Task 1: Sequencer Clock + Drum Engine Skeleton

**Files:**
- Create: `app/src/main/cpp/drums/sequencer_clock.h`
- Create: `app/src/main/cpp/drums/drum_engine.h`
- Create: `app/src/main/cpp/drums/drum_voice.h`
- Create: `app/src/main/cpp/drums/drum_track.h`
- Create: `app/src/main/cpp/drums/drum_types.h`

**Context:** This is the foundation. `drum_types.h` defines Step, Track, Pattern, ParamLock, TrigCondition structs (see spec section 4.1). `sequencer_clock.h` implements the sample-accurate timing engine. `drum_voice.h` is the abstract base class. `drum_track.h` owns one voice + step array + trigger/choke logic. `drum_engine.h` owns 8 tracks + the clock + renders a stereo block.

- [ ] **Step 1:** Create `drums/drum_types.h` with all data structures from spec section 4.1. Include `ParamLock` (paramId uint8_t + value float), `TrigCondition` enum, `Step` struct (trigger, velocity, probability, retrigCount, retrigDecay, microTiming, paramLocks[8], condition, condA, condB), `Track` struct, `Pattern` struct, `DrumKit` struct. Use `#pragma pack` or explicit sizing. Add static_asserts for expected sizes.

- [ ] **Step 2:** Create `drums/sequencer_clock.h`. Implement:
  - `double samplesPerStep_` computed from `(sampleRate * 60.0) / (bpm * 4.0)` (16th notes)
  - `double accumulator_` — incremented per sample, triggers step advance when >= samplesPerStep_
  - `void setTempo(float bpm)` — recalculates samplesPerStep_ using atomic store
  - `void setSwing(float percent)` — stores swing 50.0-75.0
  - `int advanceAndCheck(int sampleIndex)` — returns step number if trigger, -1 otherwise
  - Swing: for odd steps (0-indexed), add `(swing - 50.0) / 50.0 * samplesPerStep_` to accumulator threshold
  - `void setSampleRate(int32_t sr)` — recalculates samplesPerStep_
  - `void reset()` — resets accumulator and currentStep to 0
  - All tempo/swing reads use `std::atomic<float>` for thread safety

- [ ] **Step 3:** Create `drums/drum_voice.h` — abstract base class:
  ```cpp
  class DrumVoice {
  public:
      virtual ~DrumVoice() = default;
      virtual void trigger(float velocity) = 0;
      virtual float process() = 0;  // returns one sample
      virtual void setParam(int paramId, float value) = 0;
      virtual float getParam(int paramId) const = 0;
      virtual void setSampleRate(int32_t sr) = 0;
      virtual void reset() = 0;
      bool isActive() const { return active_; }
      void fadeOut(int fadeSamples = 96) { /* 2ms fade */ fadeCounter_ = fadeSamples; }
  protected:
      bool active_ = false;
      int fadeCounter_ = 0;
      float applyFade(float sample);  // multiplies by fade ramp if fading
  };
  ```

- [ ] **Step 4:** Create `drums/drum_track.h` — owns one DrumVoice + step data:
  - `std::unique_ptr<DrumVoice> voice_`
  - `std::atomic<float> volume_{1.0f}, pan_{0.0f}`
  - `std::atomic<bool> muted_{false}`
  - `int8_t chokeGroup_ = -1`
  - `void triggerStep(const Step& step)` — evaluates probability (rand() % 100 < step.probability), triggers voice with velocity, handles retrigs
  - `void processBlock(float* outL, float* outR, int numFrames)` — calls voice->process() per sample, applies volume + pan, sums into L/R buffers
  - `void choke()` — calls voice->fadeOut()
  - `void setVoice(std::unique_ptr<DrumVoice> v)` — swap voice type

- [ ] **Step 5:** Create `drums/drum_engine.h` — top-level engine:
  - Owns `SequencerClock clock_`, `DrumTrack tracks_[8]`, stereo output buffers
  - `void renderBlock(float* outL, float* outR, int numFrames)`:
    1. Clear outL/outR to zero
    2. For each sample i: check `clock_.advanceAndCheck(i)` → if step triggered, iterate tracks, evaluate choke groups, call `track.triggerStep()`
    3. For each track: `track.processBlock(outL, outR, numFrames)`
  - Transport: `std::atomic<bool> playing_{false}`
  - Pattern: double-buffered `Pattern` with `std::atomic<Pattern*> activePattern_`
  - `void setPattern(const Pattern& p)` — copy to inactive buffer, swap pointer
  - `void setSampleRate(int32_t sr)` — propagate to clock + all voices
  - `void triggerPad(int trackIndex, float velocity)` — immediate voice trigger from UI
  - Fill mode: `std::atomic<bool> fillActive_{false}`, stores secondary fill pattern
  - Tap tempo: rolling window of 8 taps, median calculation
  - Half-time/double-time: `std::atomic<float> tempoMultiplier_{1.0f}` (0.5 = half, 2.0 = double), applied in clock
  - Pattern queuing: `std::atomic<int> queuedPattern_{-1}`, switches on next downbeat (step 0)
  - `bool hasActiveVoices() const` — returns true if any track's voice `isActive()` (needed to hear drum tails after stop)
  - Per-voice post-processing in `DrumTrack::processBlock()`: after `voice->process()`, apply common drive (fast_tanh) + SVF filter + pan. This ensures consistent post-processing across all voice types.
  - Drum bus: `DrumBus drumBus_` member, called at end of `renderBlock()` after all tracks sum: `drumBus_.processStereo(outL, outR, numFrames)`

- [ ] **Step 6:** Verify the skeleton compiles by adding a temporary `#include "drums/drum_engine.h"` to `audio_engine.h` (do not wire up the callback yet — that's Task 7).

---

### Task 2: FM Voice

**Files:**
- Create: `app/src/main/cpp/drums/voices/fm_voice.h`

**Context:** 2-operator FM synthesis with pitch envelope. Carrier sine + modulator sine. Use wavetable lookup for sine (1024-point table, linear interpolation). Pitch envelope: exponential decay from startFreq to baseFreq. Mod index envelope: exponential decay. Amp envelope: exponential decay. SVF filter (TPT form) on output. Drive via `fast_math::fast_tanh`. All `std::atomic<float>` for params set from UI thread.

- [ ] **Step 1:** Create `fm_voice.h` implementing `DrumVoice`. Params:
  - 0: freq (40-400 Hz), 1: modRatio (0.5-8.0), 2: modDepth (0-10), 3: pitchEnvDepth (0-8 octaves), 4: pitchDecay (5-500 ms), 5: ampDecay (20-2000 ms), 6: filterCutoff (100-20000 Hz), 7: filterRes (0-1), 8: drive (0-5)
  - `trigger()`: reset phases to 0 (carrier phase to pi/2 for 808 character), set envelopes to 1.0, active_=true
  - `process()`: compute mod signal, FM carrier, apply pitch env to freq, apply filter, apply drive, apply amp env. Deactivate when ampEnv < 0.0001f
  - Envelope coefficient: `coeff = exp(-1.0 / (decaySeconds * sampleRate))`
  - Pre-compute coefficients in `setParam()` / `setSampleRate()`, not per-sample

- [ ] **Step 2:** Add a static 1024-point sine wavetable (shared across all FM voices via a static array or namespace-scoped constant). Linear interpolation lookup:
  ```cpp
  inline float sineLookup(float phase) {
      float idx = phase * 1024.0f / (2.0f * M_PI);
      int i0 = static_cast<int>(idx) & 1023;
      int i1 = (i0 + 1) & 1023;
      float frac = idx - std::floor(idx);
      return sineTable[i0] + frac * (sineTable[i1] - sineTable[i0]);
  }
  ```

---

### Task 3: Subtractive Voice

**Files:**
- Create: `app/src/main/cpp/drums/voices/subtractive_voice.h`

**Context:** Tone oscillator (sine/triangle selectable) + white noise generator, mixed, through SVF filter with filter envelope, through amp envelope. For snares, claps, rimshots.

- [ ] **Step 1:** Create `subtractive_voice.h` implementing `DrumVoice`. Params:
  - 0: toneFreq (80-800 Hz), 1: toneDecay (10-500 ms), 2: noiseDecay (10-500 ms), 3: noiseMix (0-1, blend of tone vs noise), 4: filterFreq (200-16000 Hz), 5: filterRes (0-1), 6: filterDecay (5-500 ms), 7: filterEnvDepth (0-8 octaves), 8: drive (0-5)
  - Tone osc: sine wavetable lookup (reuse from FM voice) with pitch envelope
  - Noise: xorshift32 PRNG (fast, no state dependency issues)
  - SVF filter (TPT form): LP mode, cutoff swept by filter envelope
  - Two separate amp envelopes: one for tone, one for noise, then mixed

---

### Task 4: Metallic Voice (808 Hat/Cymbal)

**Files:**
- Create: `app/src/main/cpp/drums/voices/metallic_voice.h`

**Context:** 6 square oscillators at 808 frequencies (205.3, 304.4, 369.6, 522.7, 540, 800 Hz), summed through 2 bandpass filters (3440 Hz, 7100 Hz), mixed, through HP filter, through amp envelope. The "Color" param detunes all 6 frequencies from their base values.

- [ ] **Step 1:** Create `metallic_voice.h` implementing `DrumVoice`. Params:
  - 0: color (0-1, scales detuning of 6 oscillators from nominal 808 freqs), 1: tone (0-1, blend between BP1 and BP2 output), 2: decay (5-2000 ms), 3: hpCutoff (2000-12000 Hz)
  - 6 PolyBLEP square oscillators (avoid aliasing — use `fast_math` or simple PolyBLEP correction)
  - 2 SVF bandpass filters at 3440 Hz and 7100 Hz
  - 1 SVF highpass filter
  - Choke group support: when choked, fast exponential fade instead of abrupt cutoff

---

### Task 5: Noise Voice + Physical Voice + Multi-Layer Voice

**Files:**
- Create: `app/src/main/cpp/drums/voices/noise_voice.h`
- Create: `app/src/main/cpp/drums/voices/physical_voice.h`
- Create: `app/src/main/cpp/drums/voices/multilayer_voice.h`

**Context:** Noise voice is the simplest: white/pink noise through SVF with envelope. Physical voice has two sub-modes: Karplus-Strong (delay line + LP averaging filter + feedback) and Modal (N biquad resonators summed).

- [ ] **Step 1:** Create `noise_voice.h`. Params:
  - 0: noiseType (0=white, 1=pink via Voss-McCartney 3-stage), 1: filterMode (0=LP, 1=HP, 2=BP), 2: filterFreq (100-16000), 3: filterRes (0-1), 4: decay (5-2000 ms), 5: sweep (0-1, filter envelope depth)
  - xorshift32 for white noise, 3-stage Voss-McCartney for pink
  - SVF filter, amp envelope

- [ ] **Step 2:** Create `physical_voice.h`. Params:
  - 0: modelType (0=Karplus-Strong, 1=Modal), 1: pitch (MIDI note 20-120), 2: decay (0-1, maps to feedback/damping), 3: brightness (0-1, LP filter in feedback), 4: material (0-1, blend factor for KS sign-flipping), 5: damping (0-1)
  - Karplus-Strong: pre-allocated delay line (max 4800 samples for ~10Hz at 48kHz), noise burst excitation (p samples), LP average filter `y = 0.5*(x[n-p] + x[n-p-1])`
  - Modal: up to 12 biquad resonators, frequencies from Bessel zeros for membrane or harmonic series for bars. Impulse excitation.

- [ ] **Step 3:** Create `multilayer_voice.h` implementing `DrumVoice`. Params:
  - 0: voiceAType (0-4, one of the 5 non-Multi engine types), 1: voiceBType (0-4), 2: mix (0-1, crossfade A↔B), 3: drive (0-5, post-mix saturation)
  - Params 4-9: forwarded to voice A, params 10-15: forwarded to voice B
  - Owns two `std::unique_ptr<DrumVoice>` sub-voices (created via factory function based on type)
  - `trigger()`: triggers both sub-voices
  - `process()`: `return fast_tanh(drive * (voiceA.process() * (1-mix) + voiceB.process() * mix))`
  - Constraint: sub-voices cannot be MultiLayerVoice (enforced in factory function)

---

### Task 6: Drum Bus Effects

**Files:**
- Create: `app/src/main/cpp/drums/drum_bus.h`

**Context:** Simple bus processing chain: compressor → 3-band EQ → reverb send. Lightweight versions — not reusing the full guitar effects.

- [ ] **Step 1:** Create `drum_bus.h` with:
  - **Compressor**: RMS detector (simple IIR smoothing), threshold, ratio, attack/release envelopes, makeup gain. ~20 lines of DSP.
  - **3-band EQ**: Low shelf (~200 Hz), parametric mid (~1 kHz), high shelf (~8 kHz). Each is a biquad. Coefficients pre-computed on param change.
  - **Reverb send**: Simple Schroeder reverb (4 comb filters + 2 allpass), or reuse a simplified version of the existing Dattorro plate. Wet/dry mix control.
  - `void processStereo(float* bufL, float* bufR, int numFrames)` — processes in-place
  - All params via `std::atomic<float>`

---

## Phase 2: Audio Engine Integration (Task 7) — SEQUENTIAL, depends on Phase 1

### Task 7: Wire Drum Engine into Audio Callback + CMake

**Files:**
- Modify: `app/src/main/cpp/audio_engine.h` (add DrumEngine member, stereo buffers)
- Modify: `app/src/main/cpp/audio_engine.cpp` (add drum rendering + stereo mix after looper)
- Modify: `app/src/main/cpp/CMakeLists.txt` (add drums/ include path)

**Context:** Per spec section 5.1 (revised): drums mix AFTER looper, output is stereo. The existing mono→stereo expansion step changes to: guitar mono duplicated to L/R, then stereo drum mix summed on top, then final stereo soft limiter.

- [ ] **Step 1:** In `audio_engine.h`:
  - Add `#include "drums/drum_engine.h"`
  - Add member: `DrumEngine drumEngine_;`
  - Add members: `std::vector<float> drumBufferL_, drumBufferR_;` (pre-allocated in start())
  - Add: `std::atomic<float> drumMixLevel_{0.8f};`
  - Add public methods: `DrumEngine& getDrumEngine() { return drumEngine_; }`
  - Add: `void setDrumMixLevel(float level)`

- [ ] **Step 2:** In `audio_engine.cpp` `start()`:
  - After `signalChain_.setSampleRate(sampleRate_)`, add: `drumEngine_.setSampleRate(sampleRate_);`
  - Pre-allocate drum buffers: `drumBufferL_.resize(maxFrames); drumBufferR_.resize(maxFrames);`

- [ ] **Step 3:** In `audio_engine.cpp` `onAudioReady()`, after the looper section (after the `looperEngine_.processAudioBlock()` block) and before the output level meter, render drum audio into separate stereo buffers (do NOT sum into outputBuffer yet):
  ```cpp
  // ---- Drum Machine: render stereo drum mix into separate buffers ----
  const bool drumsActive = drumEngine_.isPlaying() || drumEngine_.hasActiveVoices();
  if (drumsActive) {
      const int32_t drumFrames = std::min(numFrames,
          static_cast<int32_t>(drumBufferL_.size()));
      std::memset(drumBufferL_.data(), 0, drumFrames * sizeof(float));
      std::memset(drumBufferR_.data(), 0, drumFrames * sizeof(float));
      drumEngine_.renderBlock(drumBufferL_.data(), drumBufferR_.data(), drumFrames);
  }
  ```

- [ ] **Step 4:** Replace the existing stereo expansion section (the `if (outputChannelCount_ > 1)` block that duplicates mono to stereo) with a new version that sums stereo drums:
  ```cpp
  // ---- Stereo expansion + drum mix ----
  const float mixLvl = drumMixLevel_.load(std::memory_order_relaxed);
  if (outputChannelCount_ > 1) {
      // Iterate backwards so in-place expansion doesn't overwrite unread mono samples
      for (int32_t i = numFrames - 1; i >= 0; --i) {
          const float monoGuitar = outputBuffer[i];
          float left = monoGuitar;
          float right = monoGuitar;
          if (drumsActive) {
              left += drumBufferL_[i] * mixLvl;
              right += drumBufferR_[i] * mixLvl;
          }
          outputBuffer[i * outputChannelCount_] = fast_math::softLimit(left);
          outputBuffer[i * outputChannelCount_ + 1] = fast_math::softLimit(right);
      }
  } else {
      // Mono output: sum drum L+R to mono
      if (drumsActive) {
          for (int32_t i = 0; i < numFrames; ++i) {
              outputBuffer[i] += (drumBufferL_[i] + drumBufferR_[i]) * 0.5f * mixLvl;
              outputBuffer[i] = fast_math::softLimit(outputBuffer[i]);
          }
      }
  }
  ```
  IMPORTANT: The `drumsActive` bool must be declared before this block (in Step 3) so it's visible here.

- [ ] **Step 5:** In `CMakeLists.txt`, add include path:
  ```cmake
  target_include_directories(guitar_engine PRIVATE
      ...existing paths...
      ${CMAKE_CURRENT_SOURCE_DIR}/drums
      ${CMAKE_CURRENT_SOURCE_DIR}/drums/voices
  )
  ```

- [ ] **Step 6:** Build all 3 architectures to verify compilation:
  ```bash
  ./gradlew.bat assembleDebug
  ```

---

## Phase 3: JNI Bridge + Kotlin Layer (Tasks 8-9) — SEQUENTIAL after Phase 2

Task 9 depends on Task 8 (JNI signatures must be authoritative before Kotlin external fun declarations).

### Task 8: JNI Bridge for Drum Engine

**Files:**
- Modify: `app/src/main/cpp/jni_bridge.cpp` (add ~25 drum JNI functions)

**Context:** Follow existing pattern in jni_bridge.cpp. All functions check `if (!gEngine) return;`. Use the existing `extern "C" JNIEXPORT` pattern. Package: `com.guitaremulator.app.audio`.

- [ ] **Step 1:** Add transport JNI functions:
  ```cpp
  JNIEXPORT void JNICALL Java_com_guitaremulator_app_audio_DrumEngine_nativeSetPlaying(JNIEnv*, jobject, jboolean playing);
  JNIEXPORT void JNICALL Java_com_guitaremulator_app_audio_DrumEngine_nativeSetTempo(JNIEnv*, jobject, jfloat bpm);
  JNIEXPORT jfloat JNICALL Java_com_guitaremulator_app_audio_DrumEngine_nativeTapTempo(JNIEnv*, jobject);
  JNIEXPORT void JNICALL Java_com_guitaremulator_app_audio_DrumEngine_nativeSetSwing(JNIEnv*, jobject, jfloat swing);
  JNIEXPORT void JNICALL Java_com_guitaremulator_app_audio_DrumEngine_nativeSetPattern(JNIEnv*, jobject, jint index);
  JNIEXPORT void JNICALL Java_com_guitaremulator_app_audio_DrumEngine_nativeSetMixLevel(JNIEnv*, jobject, jfloat level);
  JNIEXPORT void JNICALL Java_com_guitaremulator_app_audio_DrumEngine_nativeTriggerFill(JNIEnv*, jobject);
  JNIEXPORT jboolean JNICALL Java_com_guitaremulator_app_audio_DrumEngine_nativeIsPlaying(JNIEnv*, jobject);
  JNIEXPORT jfloat JNICALL Java_com_guitaremulator_app_audio_DrumEngine_nativeGetTempo(JNIEnv*, jobject);
  JNIEXPORT jint JNICALL Java_com_guitaremulator_app_audio_DrumEngine_nativeGetCurrentStep(JNIEnv*, jobject);
  ```

- [ ] **Step 2:** Add track control JNI functions:
  ```cpp
  nativeSetTrackMute(int track, boolean muted)
  nativeSetTrackVolume(int track, float volume)
  nativeSetTrackPan(int track, float pan)
  nativeSetTrackEngine(int track, int engineType)
  nativeSetVoiceParam(int track, int paramId, float value)
  nativeTriggerPad(int track, float velocity)
  nativeGetTrackMute(int track) -> boolean
  nativeSetHalfTime(boolean enabled)   // tempo *= 0.5
  nativeSetDoubleTime(boolean enabled) // tempo *= 2.0
  nativeQueuePattern(int index)        // queue next pattern, switches on downbeat
  nativeGetCurrentPattern() -> int
  ```
  Note: Solo is implemented in the ViewModel by toggling mute on all other tracks (no dedicated C++ logic needed).

- [ ] **Step 3:** Add pattern editing JNI functions:
  ```cpp
  nativeSetStep(int pattern, int track, int step, int velocity)  // velocity 0 = off
  nativeSetStepProb(int pattern, int track, int step, int percent)
  nativeSetStepRetrig(int pattern, int track, int step, int count)
  nativeSetPatternLength(int pattern, int length)
  nativeLoadPattern(String json)  // bulk load from JSON
  nativeGetPattern() -> String    // bulk export to JSON
  nativeLoadKit(String json)
  nativeGetStateSnapshot() -> String  // bulk JSON snapshot for process death recovery
  ```

- [ ] **Step 4:** Build and verify JNI signatures match by running `./gradlew.bat assembleDebug`.

---

### Task 9: Kotlin Data Layer + ViewModel

**Files:**
- Create: `app/src/main/java/com/guitaremulator/app/audio/DrumEngine.kt`
- Create: `app/src/main/java/com/guitaremulator/app/data/DrumPattern.kt`
- Create: `app/src/main/java/com/guitaremulator/app/data/DrumKit.kt`
- Create: `app/src/main/java/com/guitaremulator/app/data/DrumRepository.kt`
- Create: `app/src/main/java/com/guitaremulator/app/viewmodel/DrumViewModel.kt`

**Context:** Follow existing patterns. `DrumEngine.kt` mirrors `AudioEngine.kt` (external fun declarations). Data classes use `kotlinx.serialization` or Gson for JSON. Repository handles factory presets + user patterns. ViewModel exposes StateFlow for UI.

- [ ] **Step 1:** Create `DrumEngine.kt` with all `external fun` declarations matching the JNI functions from Task 8. Add `companion object { init { System.loadLibrary("guitar_engine") } }` — actually, the library is already loaded by AudioEngine, so just declare the external funs.

- [ ] **Step 2:** Create `DrumPattern.kt` with Kotlin data classes mirroring C++ structs:
  ```kotlin
  data class DrumStep(
      val trigger: Boolean = false,
      val velocity: Int = 100,
      val probability: Int = 100,
      val retrigCount: Int = 0,
      val retrigDecay: Int = 0
  )
  data class DrumTrackData(
      val steps: List<DrumStep> = List(64) { DrumStep() },
      val length: Int = 16,
      val engineType: Int = 0,
      val voiceParams: List<Float> = List(16) { 0.5f },
      val volume: Float = 0.8f,
      val pan: Float = 0.0f,
      val muted: Boolean = false,
      val chokeGroup: Int = -1
  )
  data class DrumPatternData(
      val version: Int = 1,
      val tracks: List<DrumTrackData> = List(8) { DrumTrackData() },
      val length: Int = 16,
      val swing: Float = 50f,
      val bpm: Float = 120f,
      val name: String = "New Pattern"
  )
  ```

- [ ] **Step 3:** Create `DrumKit.kt`:
  ```kotlin
  data class DrumKitVoice(val engineType: Int, val voiceParams: List<Float>, val name: String)
  data class DrumKit(val name: String, val voices: List<DrumKitVoice>)
  ```

- [ ] **Step 4:** Create `DrumRepository.kt`:
  - Load/save patterns as JSON to `context.filesDir/drum_patterns/`
  - Load/save kits as JSON to `context.filesDir/drum_kits/`
  - `loadFactoryPresets()` — copy from assets on first launch (or overwrite on update, preserve favorites)
  - `getPatternList()`, `savePattern()`, `loadPattern()`, `deletePattern()`

- [ ] **Step 5:** Create `DrumViewModel.kt`:
  - StateFlows: `currentPattern`, `isPlaying`, `tempo`, `swing`, `selectedTrack`, `currentStep`, `trackMutes`
  - Methods: `play()`, `stop()`, `setTempo()`, `tapTempo()`, `toggleStep()`, `setVelocity()`, `muteTrack()`, `soloTrack()`, `selectTrack()`, `triggerPad()`, `loadPattern()`, `savePattern()`, `triggerFill()`
  - Polling loop for `currentStep` (for step cursor animation): `viewModelScope.launch` with 16ms delay, reads `drumEngine.nativeGetCurrentStep()`
  - Persist: current pattern index + tempo + playing state to DataStore preferences

---

## Phase 4: Factory Presets (Task 10) — PARALLELIZABLE with Phase 3

### Task 10: Factory Drum Presets

**Files:**
- Create: `app/src/main/assets/drum_presets/` (JSON files)

**Context:** ~18 factory presets across 8 genres. Each preset is a JSON file containing a DrumPatternData + DrumKit. Voice params must match the parameter IDs defined in each voice type (Task 2-5). See spec section 7 for preset list.

- [ ] **Step 1:** Create preset JSON files. Each file: `{ "kit": { ... }, "pattern": { ... } }`. Presets to create:
  - `rock_straight.json`, `rock_driving.json`, `rock_halftime.json`
  - `blues_shuffle.json`, `blues_slow.json`
  - `funk_tight.json`, `funk_breakbeat.json`
  - `metal_thrash.json`, `metal_blast.json`
  - `jazz_ride.json`, `jazz_bossa.json`
  - `electronic_808.json`, `electronic_4otf.json`, `electronic_synthwave.json`
  - `lofi_chill.json`, `lofi_boombap.json`
  - `ambient_sparse.json`, `ambient_postrock.json`

- [ ] **Step 2:** For each preset, define:
  - Kit: which engine type per track (0=FM, 1=Subtractive, 2=Metallic, 3=Noise, 4=Physical), voice params
  - Pattern: step triggers, velocities, probability values, pattern length, BPM, swing
  - Verify BPM and swing values match spec section 7

---

## Phase 5: UI (Tasks 11-15) — PARALLELIZABLE after Phase 3

### Task 11: Drum Pad Grid Component

**Files:**
- Create: `app/src/main/java/com/guitaremulator/app/ui/drums/DrumPadGrid.kt`

**Context:** 4x4 grid of velocity-sensitive drum pads. Canvas-drawn with DesignSystem tokens. Color-coded by category (red=kick, amber=snare, green=hats, blue=perc). Jewel LED per pad. Touch triggers sound immediately via `drumEngine.nativeTriggerPad()`. Tap also selects track for step editing.

- [ ] **Step 1:** Create `DrumPadGrid.kt` composable:
  - `@Composable fun DrumPadGrid(pads: List<DrumPadInfo>, selectedTrack: Int, onPadTap: (Int, Float) -> Unit, onPadSelect: (Int) -> Unit)`
  - `data class DrumPadInfo(val name: String, val categoryColor: Color, val hasActiveSteps: Boolean)`
  - 4x4 `LazyVerticalGrid` or manual `Column`/`Row` layout
  - Each pad: Canvas-drawn with radial gradient background (category-tinted on `DesignSystem.ModuleSurface`), 2px border in category color, pad label in monospace CreamWhite
  - Touch handling: `pointerInput` for immediate trigger (no slop), velocity from touch Y-position within pad (top=soft, bottom=hard) — or fixed velocity for v1 simplicity
  - Haptic feedback on touch (`HapticFeedbackType.LightImpact`)
  - Jewel LED: small `LedIndicator` in top-right corner, active when track has steps in current pattern

---

### Task 12: Step Sequencer Row Component

**Files:**
- Create: `app/src/main/java/com/guitaremulator/app/ui/drums/StepSequencerRow.kt`

**Context:** 16 step buttons in a row showing the pattern for the selected track. Tap to toggle steps. Velocity shown as brightness + mini bar below. Green cursor shows playback position. Beat markers on steps 1/5/9/13.

- [ ] **Step 1:** Create `StepSequencerRow.kt` composable:
  - `@Composable fun StepSequencerRow(steps: List<DrumStep>, currentStep: Int, patternLength: Int, onToggleStep: (Int) -> Unit, onSetVelocity: (Int, Int) -> Unit)`
  - Row of 16 Canvas-drawn step cells
  - Active step: amber fill (`DesignSystem.VUAmber`), brightness varies with velocity (alpha 0.4-1.0)
  - Inactive step: dark (`DesignSystem.MeterBg`)
  - Current step cursor: green top border (`DesignSystem.JewelGreen`)
  - Beat markers: slightly brighter bottom border on steps 0, 4, 8, 12
  - Below each step: mini velocity bar (height proportional to velocity/127)
  - Tap: toggle step on/off (if on, default velocity 100; if off, clear)
  - Long-press: open velocity/probability/retrig popup (simple dialog or bottom sheet)

---

### Task 13: Transport Bar + Pattern Selector

**Files:**
- Create: `app/src/main/java/com/guitaremulator/app/ui/drums/DrumTransportBar.kt`
- Create: `app/src/main/java/com/guitaremulator/app/ui/drums/PatternSelector.kt`

**Context:** Transport: Play/Stop buttons (StompSwitch style), BPM display (recessed panel), FILL button, Swing readout, TAP tempo. Pattern selector: 16 slots per bank.

- [ ] **Step 1:** Create `DrumTransportBar.kt`:
  - Play/Stop: reuse `StompSwitch` component with `JewelGreen` for play
  - BPM display: recessed panel (`DesignSystem.MeterBg` background, `DesignSystem.VUAmber` text, monospace)
  - FILL button: `StompSwitch` style, held = active
  - Swing: `RotaryKnob` mini or text display
  - TAP: `StompSwitch` that calls `viewModel.tapTempo()` on each press

- [ ] **Step 2:** Create `PatternSelector.kt`:
  - Row of 16 small boxes, active pattern glows amber
  - Tap to queue (switches on next downbeat)
  - Bank label ("BANK A") with left/right arrows to switch banks A-H
  - Long-press for copy/paste/clear context menu

---

### Task 14: Track Mixer Strip + Kit Editor

**Files:**
- Create: `app/src/main/java/com/guitaremulator/app/ui/drums/DrumMixerStrip.kt`
- Create: `app/src/main/java/com/guitaremulator/app/ui/drums/DrumKitEditor.kt`

**Context:** Mixer strip shows quick controls for selected track (Vol, Pan, Tone, Decay knobs + Mute/Solo). Kit editor is a full overlay (like EffectEditorSheet) with all voice params as RotaryKnobs.

- [ ] **Step 1:** Create `DrumMixerStrip.kt`:
  - Horizontal row: engine badge (text chip showing "FM"/"SUB"/"MTL"/"NOI"/"PHY"), 4 mini `RotaryKnob`s (Vol, Pan, Tone, Decay), Mute/Solo buttons, "EDIT →" button
  - Engine badge: `DesignSystem.ModuleSurface` background, blue border

- [ ] **Step 2:** Create `DrumKitEditor.kt`:
  - Full-screen overlay (BottomSheet or similar, matching `EffectEditorSheet` pattern)
  - Shows all params for the selected track's engine type as `RotaryKnob`s
  - Engine type selector at top (dropdown or segmented control)
  - Param labels match the param names defined in each voice type

---

### Task 15: Main Drum Machine Screen + Navigation

**Files:**
- Create: `app/src/main/java/com/guitaremulator/app/ui/drums/DrumMachineScreen.kt`
- Modify: `app/src/main/java/com/guitaremulator/app/ui/PedalboardScreen.kt` (add drum entry point)
- Modify: `app/src/main/java/com/guitaremulator/app/MainActivity.kt` (add navigation route)

**Context:** Assembles all drum UI components into the hybrid layout. Adds navigation from the pedalboard screen (e.g., drum icon in the top bar).

- [ ] **Step 1:** Create `DrumMachineScreen.kt`:
  - Composable that assembles: TopBar → TransportBar → PatternSelector → DrumPadGrid → TrackSelectStrip → StepSequencerRow → MixerStrip
  - Bottom nav: KITS / PLAY / EDIT / MIXER / PRESETS tabs (can be simplified for v1)
  - Connects to `DrumViewModel` via `viewModel()`
  - Track select strip: Row of 8 chip buttons matching pad colors

- [ ] **Step 2:** Add navigation:
  - In `PedalboardScreen.kt`: add a drum machine icon button in the `TopBar` (next to tuner/settings icons)
  - In `MainActivity.kt`: add `composable("drums") { DrumMachineScreen() }` route
  - Icon: use a simple grid/drum icon from Material Icons or a custom drawable

- [ ] **Step 3:** Build and verify the full app compiles and navigates to the drum screen:
  ```bash
  ./gradlew.bat assembleDebug
  ```

---

## Phase 6: Integration Testing + Polish (Task 16) — SEQUENTIAL, after all phases

### Task 16: End-to-End Testing + Build Verification

**Files:**
- Create: `app/src/test/java/com/guitaremulator/app/DrumPatternSerializationTest.kt`
- Run: full build on all 3 architectures

- [ ] **Step 1:** Create `DrumPatternSerializationTest.kt`:
  - Round-trip test: create DrumPatternData → serialize to JSON → deserialize → assertEquals
  - Test default values
  - Test backward compat: deserialize JSON missing v2 fields → defaults applied
  - Test factory preset loading: verify all 18 preset files parse without error

- [ ] **Step 2:** Run full debug build:
  ```bash
  ./gradlew.bat assembleDebug
  ```
  Verify 0 errors across arm64-v8a, armeabi-v7a, x86_64.

- [ ] **Step 3:** Run existing unit tests to verify no regressions:
  ```bash
  ./gradlew.bat testDebugUnitTest
  ```
  All 228+ existing tests must pass.

- [ ] **Step 4:** Commit all work with a descriptive message.

---

## Task Dependency Graph

```
Phase 1 (parallel after Task 1):
  Task 1 (skeleton) ─┬─→ Task 2 (FM voice)              ─┐
                      ├─→ Task 3 (subtractive)            ─┤
                      ├─→ Task 4 (metallic)               ─┤
                      ├─→ Task 5 (noise+physical+multi)   ─┤
                      └─→ Task 6 (drum bus)               ─┤
                                                            │
Phase 2 (sequential):                                       │
  Task 7 (audio callback integration) ←────────────────────┘
           │
Phase 3 (sequential):
  Task 8 (JNI bridge) ──→ Task 9 (Kotlin data + ViewModel)
           │
Phase 4 (parallel, after Phase 1):
  Task 10 (factory presets — needs voice param IDs from Tasks 2-5)

Phase 5 (parallel, after Phase 3):
  Task 11 (pad grid)
  Task 12 (step sequencer row)
  Task 13 (transport + pattern selector)
  Task 14 (mixer strip + kit editor)
  Task 15 (main screen + navigation) — depends on 11-14

Phase 6 (sequential, after all):
  Task 16 (integration testing + build)
```

## Design System Note

The drum UI uses a blue category color (`#4088E0`) for percussion pads. The existing `DesignSystem.kt` has `JewelRed`, `JewelGreen`, `JewelAmber` but no blue. Add `val JewelBlue = Color(0xFF4088E0)` to `DesignSystem.kt` during the UI tasks (Task 11-15).

---

## Execution Notes for Agents

- **Read the spec first:** `docs/superpowers/specs/2026-03-22-drum-machine-synth-design.md`
- **Follow existing patterns:** Look at how `looper/looper_engine.h` integrates with `audio_engine.h` for the drum engine pattern. Look at `effects/effect.h` for the base class pattern. Look at `jni_bridge.cpp` for JNI naming conventions. Look at `ui/components/StompSwitch.kt` and `RotaryKnob.kt` for UI component patterns.
- **Use existing utilities:** `effects/fast_math.h` has `fast_tanh`, `softLimit`, and trig approximations. Reuse these in voice implementations.
- **Real-time safety:** No allocations, no locks, no logging in any code called from `onAudioReady()`. Use `std::atomic` for all cross-thread communication.
- **Build command:** `./gradlew.bat assembleDebug` (sets JAVA_HOME automatically via build_helper.bat)
- **Test command:** `$env:JAVA_HOME='C:\Program Files\Android\Android Studio\jbr'; .\gradlew.bat testDebugUnitTest`
