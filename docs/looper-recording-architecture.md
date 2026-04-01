# Looper & Recording Architecture Design

## Executive Summary

This document specifies the architecture for a **looper and recording subsystem** that sits
alongside the existing signal chain in the guitar effects app. It is NOT an Effect subclass
in the signal chain -- it is a parallel recording/playback/overdub engine that taps audio
from the existing callback in `AudioEngine::onAudioReady()`.

The design covers: dual-track (dry+wet) recording, free-form looping with overdub and undo,
BPM-quantized looping, tap tempo, retrospective re-amping, layer management, and audio
thread safety. All with concrete sample counts, buffer sizes, and memory estimates.

---

## 1. Dual-Track Recording Architecture

### 1.1 Where to Tap the Signal

The existing `onAudioReady()` callback in `audio_engine.cpp` (lines 276-475) has a clean
linear flow that provides two natural tap points:

```
INPUT READ (line 329-349)
  --> mono extraction into outputBuffer
  --> inputLevel_ metering
  --> inputGain scaling
  --> tuner feed (pre-effects)
                                       <--- DRY TAP POINT (line ~379)
  --> signalChain_.process()
  --> outputGain scaling
  --> DC blocker
  --> NaN guard + soft limiter
                                       <--- WET TAP POINT (line ~434)
  --> spectrum analyzer feed
  --> stereo expansion
```

**Dry tap**: After inputGain scaling, before `signalChain_.process()`. This is the
"DI signal" -- the guitar signal exactly as the musician played it, with only the
master input gain applied. This is what you would feed into a re-amping session.

**Wet tap**: After the soft limiter and NaN guard, before stereo expansion and
spectrum analyzer. This is the fully processed signal with all effects, DC blocking,
and limiting applied -- exactly what the user hears.

**Sample alignment**: Both taps execute within the same callback invocation on the
same buffer of `numFrames` samples. They are inherently sample-aligned with zero
offset. The dry tap copies the buffer BEFORE `signalChain_.process()` overwrites it;
the wet tap copies the buffer AFTER all processing completes.

### 1.2 Buffer Design: Ring Buffer with Pre-Allocated Backing

**Ring buffer, not linear buffer.** A ring buffer is the correct choice because:
1. Recording length is not known in advance (user starts/stops freely)
2. For looping, we need continuous circular read/write without reallocation
3. Maximum recording length can be bounded to limit memory usage

**Structure:**

```cpp
struct RecordBuffer {
    float* data;           // Pre-allocated backing array
    int32_t capacity;      // Total samples allocated
    int32_t length;        // Samples actually recorded (0 until first pass complete)
    int32_t writePos;      // Current write head (wraps at capacity for max-length recording)
    int32_t readPos;       // Current read head (for playback)
};
```

**Pre-allocation strategy:**

All recording buffers must be allocated during `LooperEngine::init()`, called from
the setup thread before audio processing begins. The audio thread must NEVER call
malloc/new/realloc.

Maximum practical loop length calculation at 48kHz, 32-bit float:

| Duration | Samples      | Bytes (1 track) | Bytes (dry+wet) |
|----------|-------------|------------------|-----------------|
| 30s      | 1,440,000   | 5.49 MB          | 10.99 MB        |
| 60s      | 2,880,000   | 10.99 MB         | 21.97 MB        |
| 120s     | 5,760,000   | 21.97 MB         | 43.95 MB        |
| 300s     | 14,400,000  | 54.93 MB         | 109.86 MB       |

**Recommendation: 120 seconds maximum for looping, 300 seconds for linear recording.**

For the looper (circular overdub), 120s at dual-track = ~44 MB. This is practical on
devices with 4+ GB RAM (the minimum for any device running Android 12+).

For linear recording (export to file), we can stream to disk and support much longer
durations (see section 1.5).

### 1.3 Memory Layout

Pre-allocate four buffers at init:

```cpp
class LooperEngine {
    // Recording/looping buffers (pre-allocated, audio-thread safe)
    static constexpr int32_t kMaxLoopSamples = 120 * 48000;  // 120s at 48kHz = 5,760,000 samples
    static constexpr int32_t kMaxRecordSamples = 300 * 48000; // 300s for linear recording

    // Looper buffers (circular)
    std::vector<float> loopDryBuffer_;     // 5,760,000 * 4 = 21.97 MB
    std::vector<float> loopWetBuffer_;     // 5,760,000 * 4 = 21.97 MB
    std::vector<float> loopUndoBuffer_;    // 5,760,000 * 4 = 21.97 MB  (for undo last overdub)

    // Linear recording buffers (for export, streamed to disk)
    // These use a smaller ring buffer that gets flushed to disk by a writer thread
    std::vector<float> recordRingDry_;     // 96,000 * 4 = 375 KB  (2 second ring for disk flush)
    std::vector<float> recordRingWet_;     // 96,000 * 4 = 375 KB

    // Total looper memory: ~66 MB at 48kHz
    // Total recording ring memory: ~750 KB (tiny, disk-backed)
};
```

**CRITICAL**: Call `resize()` or `assign()` only during init, never on the audio thread.
The `std::vector` owns the heap allocation; on the audio thread we access only
`data()` pointers and pre-known sizes.

### 1.4 Sample Rate Adaptation

Buffers must be sized based on the ACTUAL device sample rate, not a hardcoded 48kHz.
The audio engine discovers the native rate at runtime in `openOutputStream()` (line 223):

```cpp
void LooperEngine::init(int32_t sampleRate) {
    sampleRate_ = sampleRate;
    const int32_t maxLoopSamples = 120 * sampleRate;  // 120s at native rate
    loopDryBuffer_.assign(maxLoopSamples, 0.0f);
    loopWetBuffer_.assign(maxLoopSamples, 0.0f);
    loopUndoBuffer_.assign(maxLoopSamples, 0.0f);
    // ... recording ring buffers ...
}
```

At 44100 Hz: maxLoopSamples = 5,292,000 (60.5 MB total for 3 loop buffers)
At 48000 Hz: maxLoopSamples = 5,760,000 (65.9 MB total)
At 96000 Hz: maxLoopSamples = 11,520,000 (131.8 MB) -- consider reducing max to 60s

### 1.5 File Format for Storage

**Recording (linear export):** WAV 32-bit float (IEEE 754). Rationale:
- No quality loss (lossless, no quantization beyond the 32-bit float processing format)
- Simple header (44 bytes), trivial to write incrementally
- Universally compatible (every DAW reads 32-bit float WAV)
- File size: 48000 Hz * 4 bytes * 60 seconds = 11.5 MB/minute per track
- For a 5-minute recording: 57.5 MB per track, 115 MB for dry+wet pair

**Compressed export (optional):** FLAC for sharing/archiving:
- Lossless, ~50-60% compression ratio for guitar audio
- 5-minute recording: ~50 MB for dry+wet pair
- BUT: FLAC encoding is CPU-intensive and must happen on a background thread,
  never during real-time recording
- Offer as a "Save Compressed" option in UI after recording completes

**Loop state persistence:** When saving a looper session, write:
1. `loop_dry.wav` (32-bit float, full loop length)
2. `loop_wet.wav` (32-bit float, full loop length)
3. `loop_meta.json` (BPM, time signature, bar count, loop length in samples, sample rate)

### 1.6 Disk I/O Architecture for Linear Recording

The audio thread cannot perform file I/O. Use a **producer-consumer ring buffer** with
a dedicated writer thread:

```
Audio Thread                    Writer Thread
    |                               |
    |-- write to recordRingDry_ --> |
    |-- write to recordRingWet_ --> |
    |-- advance writePos_      --> |
    |                               |-- read from ring when half full
    |                               |-- fwrite() to disk
    |                               |-- advance readPos_
```

Ring buffer sizing: 2 seconds = 96,000 samples at 48kHz = 375 KB per track.
The writer thread wakes every 1 second (or when ring is >50% full) to flush.
At 48kHz, the audio thread writes 48,000 samples/second = 187.5 KB/s per track.
A single `fwrite()` of 187.5 KB takes <1ms on any modern flash storage.

The writer thread uses a **condition variable** or a **futex** (NOT on the audio thread).
The audio thread signals via an atomic flag or a lock-free semaphore.

Actually, simpler: the writer thread polls at a fixed interval (e.g., 500ms). The ring
buffer has 2 seconds of capacity, so even if the writer thread misses one wake-up, there
is still 1.5 seconds of headroom before overflow.

---

## 2. Free-Form Looping

### 2.1 State Machine

The looper operates as a finite state machine with these states:

```
IDLE --> RECORDING --> PLAYING --> OVERDUBBING --> PLAYING
  |                      |            |              |
  |                      v            v              v
  |                    IDLE         PLAYING        IDLE
  |                  (clear)       (undo)        (clear)
```

States:
- **IDLE**: No loop recorded. Footswitch starts RECORDING.
- **RECORDING**: First pass. Audio writes to loop buffer. Footswitch stops recording,
  sets loop length, transitions to PLAYING.
- **PLAYING**: Loop plays back. Footswitch starts OVERDUBBING. Double-tap or long-press
  stops and transitions to IDLE (with confirmation dialog in UI).
- **OVERDUBBING**: New audio is summed onto existing loop content. Footswitch stops
  overdub and returns to PLAYING.

```cpp
enum class LooperState : int {
    IDLE = 0,
    RECORDING = 1,
    PLAYING = 2,
    OVERDUBBING = 3
};
```

### 2.2 Zero-Latency Punch-In/Punch-Out

When the user presses Record or Stop, the command must take effect on the VERY NEXT
sample. The architecture:

1. UI thread sets `std::atomic<LooperCommand> pendingCommand_` to the desired action
2. Audio thread checks `pendingCommand_` at the START of each callback
3. When a command is detected, the state transition happens at sample 0 of that callback
4. No "scheduling" or "waiting for next buffer boundary" -- the transition is immediate

The latency from button press to effect is exactly one audio callback period:
- At 48kHz with 192-sample burst (4ms), worst case = 4ms
- At 48kHz with 128-sample burst (2.67ms), worst case = 2.67ms

This is below the ~10ms threshold of human perception for rhythmic onset.

```cpp
enum class LooperCommand : int {
    NONE = 0,
    RECORD = 1,     // IDLE -> RECORDING
    STOP = 2,       // RECORDING -> PLAYING, OVERDUBBING -> PLAYING
    OVERDUB = 3,    // PLAYING -> OVERDUBBING
    CLEAR = 4,      // any -> IDLE
    UNDO = 5        // Undo last overdub layer
};

std::atomic<LooperCommand> pendingCommand_{LooperCommand::NONE};
```

### 2.3 Loop Length Determination

The first recording pass determines the loop length:

```cpp
// During RECORDING state:
if (state_ == LooperState::RECORDING) {
    for (int i = 0; i < numFrames; ++i) {
        loopWetBuffer_[recordWritePos_] = wetSample[i];
        loopDryBuffer_[recordWritePos_] = drySample[i];
        recordWritePos_++;
    }
}

// On STOP command during RECORDING:
loopLengthSamples_ = recordWritePos_;
playbackPos_ = 0;
state_ = LooperState::PLAYING;
```

### 2.4 Overdub: Mixing New Audio onto Existing Loop

During OVERDUBBING, the new input is SUMMED with the existing loop content:

```cpp
// During OVERDUBBING state:
for (int i = 0; i < numFrames; ++i) {
    int pos = (playbackPos_ + i) % loopLengthSamples_;

    // Save current content to undo buffer BEFORE modifying
    // (only on the first overdub frame after entering OVERDUBBING state)
    if (justEnteredOverdub_) {
        // Copy entire loop to undo buffer (happens incrementally as we play through)
        loopUndoBuffer_[pos] = loopWetBuffer_[pos];
    }

    // Sum new wet audio onto existing loop content
    float existing = loopWetBuffer_[pos];
    float newAudio = wetSample[i];
    loopWetBuffer_[pos] = existing + newAudio;

    // Also sum dry for potential re-amping
    float existingDry = loopDryBuffer_[pos];
    float newDry = drySample[i];
    loopDryBuffer_[pos] = existingDry + newDry;

    // Output what the user hears: existing loop + new input (already mixed in output)
    // The loop playback is mixed into the output buffer
}
```

**Gain normalization during overdub**: After multiple overdub passes, the summed signal
can exceed [-1.0, 1.0]. Options:

1. **No normalization (recommended for recording)**: Let the existing DC blocker + soft
   limiter in `onAudioReady()` handle it. The soft limiter at threshold 0.8 will gently
   compress peaks. This is what hardware loopers do (Boss RC-5, EHX 720).

2. **Headroom reduction**: Apply 0.85x gain to the sum: `loopBuffer[pos] = (existing + new) * 0.85f`.
   This prevents buildup but reduces overall level. A 3rd overdub would be at 0.85^2 = 0.72x.

3. **Per-sample soft clip**: `loopBuffer[pos] = fast_math::fast_tanh(existing + new)`.
   This is what the EHX 45000 does internally.

**Recommendation**: Option 1 (no normalization). Let the existing output limiter handle
it. The user can lower the output gain or overdub level if buildup becomes excessive. This
matches real hardware looper behavior and preserves dynamics.

### 2.5 Undo Last Overdub Layer

The undo buffer stores the state of `loopWetBuffer_` BEFORE the most recent overdub:

```cpp
void processUndoCommand() {
    if (hasUndoData_) {
        // Swap loop buffer and undo buffer
        // (swap, don't copy -- so undo is "redo"-able by pressing undo again)
        std::swap(loopWetBuffer_, loopUndoBuffer_);
        std::swap(loopDryBuffer_, loopUndoDryBuffer_); // Need 4th buffer for dry undo
    }
}
```

**Memory cost of undo**: One additional buffer per track = +21.97 MB for dry undo.
Total with undo: loopDry (22MB) + loopWet (22MB) + undoDry (22MB) + undoWet (22MB) = 88 MB.

**IMPORTANT**: We need 4 buffers total (dry, wet, undoDry, undoWet) for full re-amping undo.
If re-amping undo is not needed, we can drop undoDry and save 22 MB.

**Alternative: Undo only the wet track** (save 22 MB). The dry track accumulates all layers
permanently. This means re-amping after undo would include all dry layers including the
undone one. This is an acceptable trade-off. Recommendation: skip undoDry to save memory.
Total with undo (wet only): 66 MB.

### 2.6 Crossfade at Loop Boundaries

When the loop wraps from end to start, a discontinuity between the last sample and the
first sample causes an audible click. The solution is a short crossfade:

**Crossfade duration: 5ms** (240 samples at 48kHz). This is long enough to prevent clicks
but short enough to preserve transients. For reference:
- Boss RC-5: ~3ms crossfade
- EHX 720: ~5ms crossfade
- TC Ditto: ~10ms crossfade (more noticeable on rhythmic material)

**Implementation: Raised cosine (Hann) crossfade at the loop boundary.**

```cpp
static constexpr int kCrossfadeSamples = 240; // 5ms at 48kHz (recompute from sampleRate)

// Pre-compute crossfade coefficients in init():
// fadeOut[i] = 0.5 * (1 + cos(pi * i / N))  -- goes from 1.0 to 0.0
// fadeIn[i]  = 0.5 * (1 - cos(pi * i / N))  -- goes from 0.0 to 1.0
// Note: fadeIn[i] + fadeOut[i] = 1.0 (constant power NOT needed for click prevention)

// During playback, when approaching the loop end:
for (int i = 0; i < numFrames; ++i) {
    int pos = (playbackPos_ + i) % loopLengthSamples_;
    float sample = loopWetBuffer_[pos];

    // Check if we are within the crossfade zone at the END of the loop
    int distFromEnd = loopLengthSamples_ - pos;
    if (distFromEnd <= crossfadeSamples_ && distFromEnd > 0) {
        int fadeIdx = crossfadeSamples_ - distFromEnd; // 0 at start of fade, N at end
        float fadeOut = crossfadeFadeOut_[fadeIdx];
        float fadeIn  = crossfadeFadeIn_[fadeIdx];

        // Blend: fade out the end, fade in the beginning
        int startPos = pos - loopLengthSamples_ + crossfadeSamples_; // wraps to beginning
        // Actually simpler: blend the "end" sample with the "start" sample
        float startSample = loopWetBuffer_[fadeIdx]; // sample from loop beginning
        sample = sample * fadeOut + startSample * fadeIn;
    }

    outputBuffer[i] += sample * loopPlaybackGain_;
}
```

**Note on crossfade during RECORDING**: The crossfade samples at the start of the loop
should be captured during recording. When recording stops, the first `kCrossfadeSamples`
of the loop are blended with the last `kCrossfadeSamples` in-place. This is a one-time
operation (< 1ms CPU for 240 samples).

### 2.7 Maximum Practical Loop Length on Mobile

| Device RAM | Recommended Max Loop | Memory Usage |
|-----------|---------------------|-------------|
| 4 GB      | 60 seconds          | ~33 MB (wet+dry+undo) |
| 6 GB      | 120 seconds         | ~66 MB |
| 8+ GB     | 120 seconds         | ~66 MB (no reason to go higher) |

Android's per-app memory limit varies by device but is typically 256-512 MB for
foreground apps. 66 MB for the looper is 13-26% of this budget, which is acceptable
given that the signal chain effects use relatively little memory (~20 MB for all
26 effects including cabinet IRs and delay buffers).

**Detection at runtime**: Query `ActivityManager.getMemoryClass()` in Kotlin and pass
to C++ via JNI. If memoryClass < 256 MB, reduce max loop to 60 seconds.

---

## 3. BPM-Quantized Looping

### 3.1 Loop Buffer Size from BPM

```
samplesPerBeat = (60.0 / BPM) * sampleRate
samplesPerBar  = samplesPerBeat * beatsPerBar
loopSamples    = samplesPerBar * numBars
```

Examples at 48kHz:

| BPM | Time Sig | Bars | Samples     | Duration | Memory (dry+wet) |
|-----|----------|------|-------------|----------|-----------------|
| 60  | 4/4      | 4    | 768,000     | 16.0s    | 5.86 MB         |
| 90  | 4/4      | 4    | 512,000     | 10.67s   | 3.91 MB         |
| 120 | 4/4      | 4    | 384,000     | 8.0s     | 2.93 MB         |
| 120 | 4/4      | 8    | 768,000     | 16.0s    | 5.86 MB         |
| 120 | 3/4      | 4    | 288,000     | 6.0s     | 2.20 MB         |
| 140 | 4/4      | 4    | 329,143     | 6.86s    | 2.51 MB         |
| 180 | 4/4      | 4    | 256,000     | 5.33s    | 1.95 MB         |

**IMPORTANT: Non-integer sample counts.**
At 120 BPM / 48kHz: samplesPerBeat = 24,000.000 (exact integer).
At 130 BPM / 48kHz: samplesPerBeat = 22,153.846... (NOT integer).

For quantized loops, we MUST round to the nearest integer sample count. The accumulated
timing error over 4 bars is:

```
fractional error per beat = 0.846 samples
error over 16 beats (4 bars of 4/4) = 13.5 samples = 0.28ms
```

0.28ms is imperceptible. We round `loopLengthSamples` to the nearest integer:

```cpp
int32_t computeQuantizedLoopLength(float bpm, int beatsPerBar, int numBars, int32_t sampleRate) {
    double samplesPerBeat = (60.0 / static_cast<double>(bpm)) * sampleRate;
    double totalBeats = static_cast<double>(beatsPerBar) * numBars;
    return static_cast<int32_t>(std::round(samplesPerBeat * totalBeats));
}
```

### 3.2 Beat Grid Alignment and Quantized Start/Stop

When BPM is set, the loop records in a pre-determined buffer of exactly
`samplesPerBar * numBars` samples. The user flow:

1. Set BPM (via tap tempo or manual entry) and bar count (e.g., 4 bars of 4/4)
2. Press Record: loop buffer length is pre-calculated. A count-in click plays (optional:
   1 bar of clicks before recording starts)
3. Recording fills exactly `loopLengthSamples_` samples, then automatically transitions
   to PLAYING. No stop button needed.
4. If the user presses Stop early: **quantize to the nearest bar boundary.**

**Quantization behavior for early/late stop:**

```cpp
void handleQuantizedStop() {
    int32_t samplesPerBar = computeQuantizedLoopLength(bpm_, beatsPerBar_, 1, sampleRate_);
    int32_t currentSamples = recordWritePos_;

    // Find nearest bar boundary
    int32_t barsRecorded = (currentSamples + samplesPerBar / 2) / samplesPerBar;
    barsRecorded = std::max(barsRecorded, 1); // At least 1 bar

    int32_t quantizedLength = barsRecorded * samplesPerBar;

    if (quantizedLength > currentSamples) {
        // User stopped slightly BEFORE bar boundary: continue recording
        // silently (or with the input) until the boundary
        // Set a "finishing" flag that completes recording at the boundary
        quantizedTargetLength_ = quantizedLength;
        state_ = LooperState::FINISHING; // Keeps recording until target reached
    } else {
        // User stopped slightly AFTER bar boundary: truncate to boundary
        loopLengthSamples_ = quantizedLength;
        playbackPos_ = 0;
        state_ = LooperState::PLAYING;
    }
}
```

**Tolerance window**: If the user stops within +/- 50% of a bar from a boundary,
snap to that boundary. Beyond 50%, snap to the next boundary. This is standard
quantize-to-nearest behavior.

### 3.3 Time Signature Support

Supported time signatures and their beat groupings:

| Time Sig | beatsPerBar | beatUnit | Notes |
|----------|------------|----------|-------|
| 4/4      | 4          | quarter  | Standard rock/pop |
| 3/4      | 3          | quarter  | Waltz |
| 6/8      | 6          | eighth   | Compound duple (2 groups of 3) |
| 2/4      | 2          | quarter  | March |
| 5/4      | 5          | quarter  | Take Five |
| 7/8      | 7          | eighth   | Progressive |

For non-quarter-note denominators (6/8, 7/8), the beat unit affects click track accent
pattern but NOT the loop length calculation, because we express BPM in terms of the
denominator note. For 6/8 at 120 BPM (where BPM = eighth note tempo):

```
samplesPerBeat = (60 / 120) * 48000 = 24000 samples per eighth note
samplesPerBar = 24000 * 6 = 144000 samples
```

If the user thinks in "dotted quarter = 80 BPM" (compound meter convention), the UI
should convert: `eighthNoteBPM = dottedQuarterBPM * 3 / 2`. So "80 BPM in 6/8" becomes
120 eighth-note BPM internally.

### 3.4 Click Track / Metronome

**The metronome MUST bypass the signal chain.** If the click goes through the signal
chain (distortion, delay, reverb), it becomes mushy, delayed, and rhythmically useless.

Implementation: Generate click samples in the audio callback and sum them directly
into the output buffer AFTER all signal chain processing:

```cpp
// In onAudioReady(), after signalChain_.process() and after soft limiter:
if (looperEngine_.isMetronomeActive()) {
    looperEngine_.generateMetronomeClick(outputBuffer, numFrames);
}
```

**Click sound generation**: Use a short (3-5ms) sine wave burst:
- Downbeat (beat 1): 1200 Hz, 3ms duration, amplitude 0.3 (not too loud)
- Other beats: 800 Hz, 2ms duration, amplitude 0.2
- 3ms at 48kHz = 144 samples. Pre-compute the click waveforms in init().

```cpp
// Pre-computed click waveform: 3ms * 48kHz = 144 samples
// Amplitude envelope: linear fade from 0.3 to 0 over 144 samples
// Frequency: 1200 Hz downbeat, 800 Hz other beats
static constexpr int kClickDurationSamples = 144; // recompute from sampleRate

void generateClickWaveform(float* out, int numSamples, float freq, float amplitude) {
    for (int i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(sampleRate_);
        float envelope = 1.0f - static_cast<float>(i) / static_cast<float>(numSamples);
        out[i] = amplitude * envelope * std::sin(2.0f * M_PI * freq * t);
    }
}
```

**Click timing**: The looper tracks a `samplesSinceLastBeat_` counter. When it reaches
`samplesPerBeat`, a click is triggered:

```cpp
void processMetronome(float* outputBuffer, int numFrames) {
    for (int i = 0; i < numFrames; ++i) {
        if (clickSamplesRemaining_ > 0) {
            int clickIdx = clickDuration_ - clickSamplesRemaining_;
            outputBuffer[i] += clickWaveform_[clickIdx];
            clickSamplesRemaining_--;
        }

        beatSampleCounter_++;
        if (beatSampleCounter_ >= samplesPerBeat_) {
            beatSampleCounter_ = 0;
            currentBeat_ = (currentBeat_ + 1) % beatsPerBar_;
            clickSamplesRemaining_ = clickDuration_;
            // Select downbeat or regular click waveform based on currentBeat_
            clickWaveform_ = (currentBeat_ == 0) ? downbeatClick_ : regularClick_;
        }
    }
}
```

---

## 4. Tap Tempo Algorithm

### 4.1 Algorithm: Trimmed Mean with Outlier Rejection

The standard approach used by most pedal manufacturers (Boss, TC Electronic, Eventide):

```cpp
class TapTempo {
    static constexpr int kMaxTaps = 8;
    static constexpr int kMinTaps = 2;        // Need at least 2 taps for 1 interval
    static constexpr float kMinBPM = 30.0f;   // 2000ms interval
    static constexpr float kMaxBPM = 300.0f;  // 200ms interval
    static constexpr float kTimeoutMs = 3000.0f; // Reset after 3s of no taps
    static constexpr float kOutlierThreshold = 0.35f; // Reject intervals >35% from median

    std::array<int64_t, kMaxTaps> tapTimestamps_;
    int tapCount_ = 0;
    float currentBPM_ = 120.0f;

    void onTap(int64_t timestampMs) {
        // Reset if too long since last tap
        if (tapCount_ > 0 && (timestampMs - tapTimestamps_[tapCount_ - 1]) > kTimeoutMs) {
            tapCount_ = 0;
        }

        // Store timestamp
        if (tapCount_ < kMaxTaps) {
            tapTimestamps_[tapCount_++] = timestampMs;
        } else {
            // Shift left, discard oldest
            for (int i = 0; i < kMaxTaps - 1; ++i) {
                tapTimestamps_[i] = tapTimestamps_[i + 1];
            }
            tapTimestamps_[kMaxTaps - 1] = timestampMs;
        }

        if (tapCount_ < kMinTaps) return; // Not enough taps yet

        // Compute intervals
        int numIntervals = tapCount_ - 1;
        float intervals[kMaxTaps - 1];
        for (int i = 0; i < numIntervals; ++i) {
            intervals[i] = static_cast<float>(tapTimestamps_[i + 1] - tapTimestamps_[i]);
        }

        // Find median interval (for outlier detection)
        float sorted[kMaxTaps - 1];
        std::copy(intervals, intervals + numIntervals, sorted);
        std::sort(sorted, sorted + numIntervals);
        float median = sorted[numIntervals / 2];

        // Reject outliers (missed taps = ~2x interval, double-taps = ~0.5x interval)
        float sum = 0.0f;
        int validCount = 0;
        for (int i = 0; i < numIntervals; ++i) {
            float deviation = std::abs(intervals[i] - median) / median;
            if (deviation <= kOutlierThreshold) {
                sum += intervals[i];
                validCount++;
            }
        }

        if (validCount == 0) {
            // All intervals rejected; use median as fallback
            sum = median;
            validCount = 1;
        }

        float avgIntervalMs = sum / static_cast<float>(validCount);
        float bpm = 60000.0f / avgIntervalMs;
        currentBPM_ = std::clamp(bpm, kMinBPM, kMaxBPM);
    }
};
```

### 4.2 How Many Taps for Reliable BPM

| Taps | Intervals | Reliability | Use Case |
|------|-----------|-------------|----------|
| 2    | 1         | Poor        | Rough estimate, update immediately |
| 3    | 2         | Moderate    | Outlier rejection becomes possible |
| 4    | 3         | Good        | Sufficient for most users |
| 5-8  | 4-7       | Excellent   | Averaged, very stable |

**Recommendation**: Display BPM after 2 taps but flag it as "uncertain" in the UI (e.g.,
show "~120 BPM" vs "120 BPM"). After 4+ taps, display with confidence.

### 4.3 BPM Range Constraints

| BPM   | Interval (ms) | Use Case |
|-------|--------------|----------|
| 30    | 2000         | Very slow ambient |
| 40    | 1500         | Slow ballad |
| 60    | 1000         | Slow rock |
| 120   | 500          | Standard rock |
| 180   | 333          | Fast punk/metal |
| 240   | 250          | Very fast (double-time feel) |
| 300   | 200          | Extreme (approaching tap reliability limit) |

Above 300 BPM (200ms intervals), human tap accuracy degrades significantly. Below 30 BPM,
3-second inter-tap waits exceed the patience threshold. Clamp to [30, 300].

### 4.4 Double-Tap Rejection

A "double-tap" is when the user accidentally taps twice in rapid succession. Detect and
reject any interval shorter than 200ms (300 BPM). This is handled by the kMaxBPM clamp:
if the interval is < 200ms, it is classified as an outlier by the median-based rejection.

### 4.5 Tap Tempo Integration with Quantized Looper

When tap tempo sets a new BPM while a quantized loop is playing:
- The loop length does NOT change (that would pitch-shift the audio)
- The metronome click rate updates to the new BPM
- The next recording pass will use the new BPM for quantization
- A "Re-quantize" UI action could be offered to adjust loop length to new BPM (with
  time-stretching, but this is a v2 feature)

---

## 5. Retrospective Re-Amping from Dry Recording

### 5.1 Architecture

Re-amping plays a previously recorded dry signal through the current signal chain, as if
the guitarist were playing live but with different effect settings. This is the killer
feature of dual-track recording.

**Two modes:**

**A. Real-time re-amping (monitoring)**: The dry recording plays back through the signal
chain at 1x speed. The user hears the re-amped output in real time while tweaking knobs.
This is the primary mode.

**B. Offline re-amping (bounce/export)**: The dry recording is processed through the
signal chain as fast as the CPU allows, writing the wet output to a new file. This is
faster than real-time on modern devices but requires careful state management.

### 5.2 Real-Time Re-Amping Implementation

During re-amping, the audio callback REPLACES the live input with samples from the dry
recording buffer:

```cpp
// In onAudioReady(), replace the input read section:
if (reampState_ == ReampState::PLAYING) {
    // Read from dry recording instead of live input
    int32_t samplesToRead = std::min(numFrames, reampRemaingSamples_);
    std::memcpy(outputBuffer, reampDryBuffer_ + reampReadPos_, samplesToRead * sizeof(float));
    reampReadPos_ += samplesToRead;
    reampRemaingSamples_ -= samplesToRead;

    // Zero-fill if recording ended mid-buffer
    if (samplesToRead < numFrames) {
        std::memset(outputBuffer + samplesToRead, 0, (numFrames - samplesToRead) * sizeof(float));
    }

    // Apply input gain (same as live input path)
    if (inputGain != 1.0f) {
        for (int32_t i = 0; i < numFrames; ++i) {
            outputBuffer[i] *= inputGain;
        }
    }

    // Process through signal chain (same as live path)
    if (!bypassed) {
        signalChain_.process(outputBuffer, numFrames);
    }

    // ... rest of callback (output gain, DC blocker, limiter, etc.) is unchanged
}
```

**Input mixing during re-amping**: The user might want to play along with the re-amped
recording. Offer a "Monitor + Re-amp" mode that sums the live input with the dry playback:

```cpp
// Sum live input and dry playback
for (int i = 0; i < numFrames; ++i) {
    outputBuffer[i] = liveInput[i] * liveInputGain + dryPlayback[i] * reampGain;
}
```

### 5.3 Sample Rate Mismatch Handling

If a dry recording was made at 48kHz but the device now runs at 44100Hz (e.g., different
USB interface connected), sample rate conversion (SRC) is needed.

**Recommendation: Perform SRC at load time, not in the audio callback.**

When the user loads a dry recording for re-amping:
1. Read the WAV file header to determine the recorded sample rate
2. If it differs from the current device sample rate, resample the entire recording
   using a high-quality offline SRC (sinc interpolation, 64-point kernel)
3. Store the resampled version in the reamp buffer
4. All subsequent real-time playback uses the resampled data (no per-sample SRC needed)

**Resampling ratio**: `outputSamples = inputSamples * (targetRate / sourceRate)`
- 48000 -> 44100: ratio = 0.91875, a 60s recording goes from 2,880,000 to 2,646,000 samples
- 44100 -> 48000: ratio = 1.08844, a 60s recording goes from 2,646,000 to 2,880,000 samples

Use a quality library for offline SRC. Options:
- **libsamplerate** (BSD, well-tested, used in Audacity)
- **r8brain** (MIT, very high quality)
- **Simple polyphase FIR**: For integer ratios (48000/44100 = 160/147), a polyphase filter
  bank provides artifact-free conversion.

**Performance**: Offline SRC of 5 minutes of audio at high quality takes ~200ms on a
Snapdragon 8 Gen 1. This is a one-time cost at load time, imperceptible to the user.

### 5.4 Offline Re-Amping (Bounce)

For "Save Re-amped" export, process the dry recording through the signal chain in blocks,
writing output to a WAV file:

```cpp
void offlineReamp(const float* dryInput, int32_t totalSamples,
                  const std::string& outputPath) {
    // Process in blocks of 512 samples (matching typical callback size)
    constexpr int kBlockSize = 512;
    float block[kBlockSize];
    WavWriter writer(outputPath, sampleRate_, 1, 32); // mono, 32-bit float

    signalChain_.resetAll(); // Clear all effect state

    for (int32_t pos = 0; pos < totalSamples; pos += kBlockSize) {
        int32_t framesToProcess = std::min(kBlockSize, totalSamples - pos);
        std::memcpy(block, dryInput + pos, framesToProcess * sizeof(float));

        signalChain_.process(block, framesToProcess);

        // Apply same post-processing as onAudioReady():
        // DC blocker, NaN guard, soft limiter
        applyPostProcessing(block, framesToProcess);

        writer.writeSamples(block, framesToProcess);
    }
}
```

**CRITICAL**: Offline re-amping MUST run on a background thread, NOT the audio thread.
The signal chain must be either:
a) Duplicated (create a second SignalChain instance with identical parameters), or
b) Run while audio is stopped (simpler but means no monitoring during export)

Option (b) is simpler and recommended for v1. The UI shows a progress bar: "Re-amping...
45%". At 48kHz with 26 effects, the signal chain processes ~10x faster than real-time,
so a 5-minute recording bounces in ~30 seconds.

### 5.5 UI/UX for Re-Amping

1. User records a performance (dry + wet saved automatically)
2. Later, user opens "Recordings" screen, selects a recording
3. Taps "Re-Amp" button
4. App loads the dry recording into the reamp buffer
5. Playback starts through current signal chain settings
6. User tweaks effects in real-time, hearing the result
7. When satisfied, taps "Save" to bounce the final re-amped version
8. Original dry recording is NEVER modified (non-destructive workflow)

---

## 6. Layer/Track Management

### 6.1 How Many Overdub Layers Before Performance Degrades?

Overdub layers are CUMULATIVE, not stacked. Each overdub modifies the loop buffer in-place
(summing new audio onto existing content). This means:

- **CPU cost**: Zero per additional layer. The buffer contains the SUM of all previous
  layers. Playback reads one sample per frame regardless of how many overdubs occurred.
- **Memory cost**: Zero per additional layer (in-place modification). Only the undo buffer
  adds memory (one extra copy of the loop).

This is the standard architecture for all hardware loopers (Boss RC-series, EHX 720/22500,
TC Ditto). It has no practical layer limit.

**If we wanted separate, mixable layers** (like a multitrack recorder):

| Layers | Memory per layer (120s @ 48kHz) | Total Memory |
|--------|-------------------------------|-------------|
| 2      | 21.97 MB                       | 43.95 MB    |
| 4      | 21.97 MB                       | 87.89 MB    |
| 8      | 21.97 MB                       | 175.78 MB   |
| 16     | 21.97 MB                       | 351.56 MB   | (exceeds most per-app limits)

**Recommendation for v1**: In-place overdub (unlimited layers, zero memory growth).
This matches user expectations from hardware loopers.

**For v2**: Offer a "multitrack" mode with 4 separate layers, each with independent
volume faders. This would need 4 * 22 MB = 88 MB for wet layers alone, plus 4 * 22 MB
for dry layers if re-amping is supported per layer. 176 MB total is feasible on 8+ GB
devices.

### 6.2 Per-Layer Dry+Wet Storage

For the in-place overdub architecture:
- **One dry buffer, one wet buffer**: Both accumulate all layers. Re-amping the dry buffer
  gives you ALL layers re-amped together. You cannot isolate layer 3's dry guitar.
- **This is acceptable** because re-amping applies a new tone to the entire performance,
  not individual layers. If the user wants per-layer re-amping, they need multitrack mode.

### 6.3 Mixing During Playback

```cpp
// Loop playback is summed into the output buffer with a gain control:
for (int i = 0; i < numFrames; ++i) {
    int pos = (playbackPos_ + i) % loopLengthSamples_;
    outputBuffer[i] += loopWetBuffer_[pos] * loopVolume_;
}
// loopVolume_ is an atomic<float> controlled from the UI (0.0 to 1.0)
```

The live input passes through the signal chain as normal and appears in `outputBuffer`
already. The loop playback is ADDED to the output. This means the user hears:

`output = processedLiveInput + loopPlayback * loopVolume`

This is correct: the loop is a backing track that the musician plays over.

---

## 7. Audio Thread Safety

### 7.1 Command Communication: Lock-Free State Machine

The existing codebase uses `std::atomic<T>` with acquire/release ordering for all
UI-to-audio communication (see `atomic_params.h` lines 1-72, `AudioParams` struct).
The looper follows the same pattern:

```cpp
// Commands from UI thread to audio thread
std::atomic<LooperCommand> pendingCommand_{LooperCommand::NONE};

// State readable by UI thread (for display)
std::atomic<LooperState> currentState_{LooperState::IDLE};
std::atomic<int32_t> playbackPosition_{0};    // For progress bar
std::atomic<int32_t> loopLength_{0};          // Total loop length in samples
std::atomic<int32_t> currentBeat_{0};         // For beat indicator LED
std::atomic<float> loopVolume_{1.0f};         // Playback volume
std::atomic<bool> metronomeActive_{false};    // Click track on/off
std::atomic<float> bpm_{120.0f};             // Current BPM

// Re-amp state
std::atomic<ReampState> reampState_{ReampState::IDLE};
```

**Command dispatch on audio thread:**

```cpp
void LooperEngine::processCommand() {
    LooperCommand cmd = pendingCommand_.exchange(LooperCommand::NONE,
                                                  std::memory_order_acq_rel);
    if (cmd == LooperCommand::NONE) return;

    switch (cmd) {
        case LooperCommand::RECORD:
            if (state_ == LooperState::IDLE) {
                state_ = LooperState::RECORDING;
                recordWritePos_ = 0;
            }
            break;
        case LooperCommand::STOP:
            if (state_ == LooperState::RECORDING) {
                loopLengthSamples_ = recordWritePos_;
                applyCrossfade(); // Blend loop start/end
                playbackPos_ = 0;
                state_ = LooperState::PLAYING;
            } else if (state_ == LooperState::OVERDUBBING) {
                state_ = LooperState::PLAYING;
            }
            break;
        // ... etc
    }
    currentState_.store(static_cast<LooperState>(state_), std::memory_order_release);
}
```

**CRITICAL**: Use `exchange()` (not `load()` then `store()`) to consume the command. This
prevents the command from being processed twice if the audio callback runs faster than
expected. This is the same pattern used for `running_` in `onErrorAfterClose()` (line 560
of audio_engine.cpp).

### 7.2 Recording State Machine: No Locks, No Allocations

The entire looper state machine runs within `processAudioBlock()`, called from the audio
callback:

```cpp
void LooperEngine::processAudioBlock(const float* dryInput, const float* wetInput,
                                      float* outputBuffer, int numFrames) {
    // 1. Check for pending command
    processCommand();

    // 2. Execute current state
    switch (state_) {
        case LooperState::IDLE:
            // Nothing to do
            break;

        case LooperState::RECORDING:
            // Write dry and wet to loop buffers
            for (int i = 0; i < numFrames; ++i) {
                if (recordWritePos_ < maxLoopSamples_) {
                    loopDryBuffer_[recordWritePos_] = dryInput[i];
                    loopWetBuffer_[recordWritePos_] = wetInput[i];
                    recordWritePos_++;
                } else {
                    // Max loop length reached; auto-stop
                    loopLengthSamples_ = maxLoopSamples_;
                    applyCrossfade();
                    playbackPos_ = 0;
                    state_ = LooperState::PLAYING;
                    break; // Exit inner loop, fall through to PLAYING
                }
            }
            break;

        case LooperState::PLAYING:
            // Read from loop buffer and add to output
            for (int i = 0; i < numFrames; ++i) {
                int pos = playbackPos_ % loopLengthSamples_;
                outputBuffer[i] += loopWetBuffer_[pos] * loopVolume;
                playbackPos_++;
                if (playbackPos_ >= loopLengthSamples_) {
                    playbackPos_ = 0;
                }
            }
            break;

        case LooperState::OVERDUBBING:
            for (int i = 0; i < numFrames; ++i) {
                int pos = playbackPos_ % loopLengthSamples_;

                // Save to undo buffer (if first pass through since entering overdub)
                if (!undoSaved_[pos]) {
                    loopUndoBuffer_[pos] = loopWetBuffer_[pos];
                    undoSaved_[pos] = true; // This is a bitfield, pre-allocated
                }

                // Sum new audio onto existing
                loopWetBuffer_[pos] += wetInput[i];
                loopDryBuffer_[pos] += dryInput[i];

                // Play back the combined result
                outputBuffer[i] += loopWetBuffer_[pos] * loopVolume;

                playbackPos_++;
                if (playbackPos_ >= loopLengthSamples_) {
                    playbackPos_ = 0;
                }
            }
            break;
    }

    // 3. Update UI-readable state
    playbackPosition_.store(playbackPos_, std::memory_order_release);

    // 4. Process metronome (if active, adds click to output)
    if (metronomeActive_.load(std::memory_order_acquire)) {
        processMetronome(outputBuffer, numFrames);
    }
}
```

### 7.3 File I/O Without Blocking the Audio Thread

**For linear recording to disk**, the audio thread writes to a small ring buffer,
and a separate writer thread flushes to disk:

```cpp
// Audio thread: write to ring (lock-free SPSC)
class SPSCRingBuffer {
    std::vector<float> buffer_;
    std::atomic<int32_t> writePos_{0};
    std::atomic<int32_t> readPos_{0};
    int32_t capacity_;

    // Audio thread calls this (producer)
    bool write(const float* data, int count) {
        int32_t w = writePos_.load(std::memory_order_relaxed);
        int32_t r = readPos_.load(std::memory_order_acquire);

        int32_t available = capacity_ - (w - r); // space available
        if (available < count) return false; // Ring full! Data lost.

        for (int i = 0; i < count; ++i) {
            buffer_[(w + i) % capacity_] = data[i];
        }
        writePos_.store(w + count, std::memory_order_release);
        return true;
    }

    // Writer thread calls this (consumer)
    int read(float* data, int maxCount) {
        int32_t r = readPos_.load(std::memory_order_relaxed);
        int32_t w = writePos_.load(std::memory_order_acquire);

        int32_t available = w - r;
        int32_t toRead = std::min(available, static_cast<int32_t>(maxCount));

        for (int i = 0; i < toRead; ++i) {
            data[i] = buffer_[(r + i) % capacity_];
        }
        readPos_.store(r + toRead, std::memory_order_release);
        return toRead;
    }
};
```

**Writer thread lifecycle:**

```cpp
void writerThreadFunc() {
    float scratchBuffer[4096]; // Stack-allocated read buffer
    while (writerRunning_.load(std::memory_order_acquire)) {
        int dryRead = dryRing_.read(scratchBuffer, 4096);
        if (dryRead > 0) {
            fwrite(scratchBuffer, sizeof(float), dryRead, dryFile_);
        }

        int wetRead = wetRing_.read(scratchBuffer, 4096);
        if (wetRead > 0) {
            fwrite(scratchBuffer, sizeof(float), wetRead, wetFile_);
        }

        // Sleep 100ms if nothing was available (reduce CPU when idle)
        if (dryRead == 0 && wetRead == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    // Flush remaining data and close files
    // ... (final drain loop)
}
```

### 7.4 Integration into Existing onAudioReady()

The looper integrates into the existing callback with minimal changes:

```cpp
oboe::DataCallbackResult AudioEngine::onAudioReady(...) {
    // ... existing input read and mono extraction (lines 329-349) ...
    // ... inputGain scaling (lines 375-379) ...

    // === NEW: Capture dry signal for looper ===
    // dryCapture_ is a pre-allocated buffer (same size as inputBuffer_)
    if (looperEngine_.isActive()) {
        std::memcpy(dryCapture_.data(), outputBuffer, numFrames * sizeof(float));
    }

    // ... tuner feed (lines 389-391) ...
    // ... signalChain_.process() (lines 394-396) ...
    // ... outputGain, DC blocker, NaN guard, soft limiter (lines 399-426) ...

    // === NEW: Capture wet signal and process looper ===
    if (looperEngine_.isActive()) {
        // wetCapture is outputBuffer at this point (post-processing)
        looperEngine_.processAudioBlock(dryCapture_.data(), outputBuffer,
                                         outputBuffer, numFrames);
    }

    // ... output level meter (lines 429-431) ...
    // ... spectrum analyzer (line 434) ...
    // ... stereo expansion (lines 443-450) ...
}
```

**Memory for dryCapture_**: Add to `AudioEngine` member variables:
```cpp
std::vector<float> dryCapture_; // Pre-allocated in start(), same size as inputBuffer_
```

---

## 8. Failure Modes and Critique

### 8.1 Memory Pressure on Low-End Devices

**Risk**: 66 MB for looper buffers may cause OOM on devices with 3-4 GB RAM where the
per-app memory limit is 192-256 MB.

**Mitigation**:
1. Query `ActivityManager.getMemoryClass()` at app startup
2. If < 256 MB: reduce max loop to 30 seconds (16.5 MB)
3. If < 192 MB: disable looper entirely (show "Not enough memory" in UI)
4. Allocate buffers lazily (only when user opens looper screen, not at app startup)
5. Release buffers when user navigates away from looper screen

**Detection**: `std::vector::assign()` can throw `std::bad_alloc`. Catch this in init()
and report failure to Kotlin via JNI return value.

### 8.2 Audio Thread Overrun During Complex Overdub

**Risk**: During overdub, the audio callback must:
1. Read from loop buffer
2. Write to loop buffer (sum)
3. Copy to undo buffer (conditional)
4. Process metronome (if active)
5. All of this ON TOP of the existing 26-effect signal chain

**Measurement**: Additional per-sample operations for overdub:
- 2 reads + 2 writes + 1 conditional write + 1 addition = ~7 operations/sample
- At 48kHz with 192-sample buffer: 7 * 192 = 1,344 extra operations per callback
- This is approximately 0.5 microseconds of additional work (trivial)

The signal chain (especially WDF pedals at 2x oversampling + cabinet FFT convolution)
dominates the callback time at ~300-500 microseconds. The looper adds <1% CPU overhead.

**NOT a real risk** for CPU. The real risk is memory bandwidth -- the loop buffer may
not be in L2 cache, causing cache misses. At 5.76 million samples (22 MB), the loop
buffer far exceeds any mobile L2 cache (typically 1-4 MB). Each playback sample is a
cache miss.

**Mitigation**: The loop playback reads sequentially, which the hardware prefetcher
handles well. Random access would be a problem; sequential access is fine. At 48kHz,
we read 48K samples/second = 187 KB/s -- well within DRAM bandwidth limits (>10 GB/s
even on low-end Snapdragons).

### 8.3 Recording Buffer Overflow (Linear Recording)

**Risk**: If the writer thread stalls (storage full, I/O contention), the SPSC ring buffer
fills up and the audio thread's writes are dropped, causing gaps in the recording.

**Mitigation**:
1. Size the ring buffer for 2 seconds of headroom (96K samples = 375 KB)
2. Monitor ring buffer fill level from the writer thread
3. If fill > 75%, log a warning and increase writer thread priority
4. If fill = 100%, set an atomic error flag that the UI thread polls
5. Display "Recording interrupted -- storage too slow" in the UI

**Android-specific**: eMMC/UFS write speeds on modern devices are 100-500 MB/s sequential.
Writing 375 KB/s (48kHz * 4 bytes * 2 tracks) requires <0.001% of available bandwidth.
Stalls only occur if:
- Internal storage is >95% full (Android throttles writes)
- A background app triggers heavy I/O (e.g., photo sync, app update)

### 8.4 Latency During Loop Playback

The user hears the loop playback with the same output latency as the live input:
approximately 4-10ms depending on buffer settings. This is NOT additional latency
on top of the live signal -- both the loop and live signal go through the same output
path. They are time-aligned in the callback.

However: the loop was RECORDED with the full round-trip latency. If round-trip latency
is 10ms, then what the user played was heard 10ms after they intended it. When the loop
plays back, it is 10ms "late" relative to a perfect metronome.

**This is inherent to all real-time loopers** and is not something we can fix in software.
The user compensates by playing 10ms early, which they do naturally after a few bars.
Hardware loopers (Boss RC-5, etc.) have the same property.

**For quantized loops**: The quantization logic should NOT try to compensate for latency.
The user's taps are already latency-adjusted (they tap when they HEAR the beat, which is
already delayed). Subtracting latency would make the loop EARLY.

### 8.5 Crossfade Artifacts at Very Fast Tempos

At 300 BPM, one beat = 200ms = 9600 samples. A 5ms crossfade is 2.5% of a beat -- negligible.

But: at 300 BPM with 1/4 bar loops (a "beat repeat" use case), the loop length is 9600
samples and the 240-sample crossfade is very close to the start. This could cause the
crossfade to overlap with musical content.

**Mitigation**: Scale crossfade length to never exceed 2% of loop length:
```cpp
int crossfadeSamples = std::min(kDefaultCrossfadeSamples,
                                 loopLengthSamples_ / 50);
crossfadeSamples = std::max(crossfadeSamples, 48); // Minimum 1ms
```

### 8.6 Undo Buffer Race Condition

**Risk**: User presses Undo while OVERDUBBING state is active. The undo buffer is being
written to (saving pre-overdub state) at the same time it would be swapped.

**Mitigation**: Undo command is only valid in PLAYING state. If state is OVERDUBBING,
the command is ignored (or automatically stops overdub first, then undoes).

```cpp
case LooperCommand::UNDO:
    if (state_ == LooperState::PLAYING && hasUndoData_) {
        std::swap(loopWetBuffer_, loopUndoBuffer_);
        hasUndoData_ = false; // Can't undo twice without a new overdub
    }
    break;
```

### 8.7 App Backgrounding During Recording

When Android moves the app to the background, the foreground service keeps the audio
callback running (the app already has `AudioProcessingService` with PARTIAL_WAKE_LOCK).
Recording continues seamlessly.

**Risk**: If the user force-kills the app during recording, unsaved data is lost.

**Mitigation**: The SPSC ring buffer + writer thread ensures data is flushed to disk
every 500ms. Maximum data loss from a force-kill: 500ms of audio (24K samples). The WAV
header is written with a placeholder length at the start of recording, then updated with
the correct length when recording stops normally. If the app crashes, the WAV file has an
incorrect length header but the audio data is intact. The UI should detect and repair
truncated WAV files on next launch.

### 8.8 Sample Rate Change Mid-Loop

**Risk**: USB audio interface disconnected during loop playback. Engine restarts at a
different sample rate (e.g., 48kHz -> 44.1kHz). Loop buffer contains 48kHz audio but
playback is at 44.1kHz, causing pitch shift.

**Mitigation**: On engine restart (`AudioEngine::start()`), if the sample rate changed
and a loop is loaded:
1. Detect mismatch: `if (looperEngine_.getSampleRate() != sampleRate_)`
2. Option A: Clear the loop and notify the user ("Loop cleared due to sample rate change")
3. Option B: Resample the loop buffer offline (takes ~100ms for a 120s loop)

Recommendation: Option A for v1 (simple, safe). Option B for v2.

---

## 9. Complete Integration Summary

### 9.1 New Files Required

**C++ (header-only, following project convention):**
- `app/src/main/cpp/looper/looper_engine.h` -- State machine, buffer management, overdub
- `app/src/main/cpp/looper/tap_tempo.h` -- BPM detection from tap intervals
- `app/src/main/cpp/looper/metronome.h` -- Click track generation
- `app/src/main/cpp/looper/spsc_ring_buffer.h` -- Lock-free ring for disk I/O
- `app/src/main/cpp/looper/wav_writer.h` -- WAV file writing (header + float data)

**Kotlin:**
- `LooperViewModel.kt` -- UI state, command dispatch, progress tracking
- `LooperScreen.kt` -- UI for transport controls, waveform, BPM display
- `LooperRepository.kt` -- File management for saved loops/recordings

### 9.2 JNI Functions Required (~15 new functions)

```
// Looper transport
looperSetCommand(command: Int)          // Record, Stop, Overdub, Clear, Undo
looperGetState(): Int                   // IDLE, RECORDING, PLAYING, OVERDUBBING
looperGetPlaybackPosition(): Int        // Current position in samples
looperGetLoopLength(): Int              // Total loop length in samples
looperSetVolume(volume: Float)          // Loop playback volume

// Metronome
setMetronomeActive(active: Boolean)
setMetronomeBPM(bpm: Float)
setTimeSignature(beatsPerBar: Int, beatUnit: Int)
getCurrentBeat(): Int                   // For beat indicator UI

// Tap tempo
onTapTempo(timestampMs: Long)           // Called from UI on each tap
getTapTempoBPM(): Float                 // Current detected BPM

// BPM quantized mode
setQuantizedMode(enabled: Boolean)
setQuantizedBars(numBars: Int)

// Re-amping
loadDryRecordingForReamp(path: String): Boolean
startReamp()
stopReamp()

// Linear recording
startRecording(outputDir: String): Boolean
stopRecording(): String                 // Returns path to saved WAV files
```

### 9.3 Memory Budget Summary

| Component | Allocation | Size @ 48kHz |
|-----------|-----------|-------------|
| loopDryBuffer_ (120s) | init() | 21.97 MB |
| loopWetBuffer_ (120s) | init() | 21.97 MB |
| loopUndoBuffer_ (120s) | init() | 21.97 MB |
| SPSC ring dry (2s) | init() | 0.375 MB |
| SPSC ring wet (2s) | init() | 0.375 MB |
| dryCapture_ (4x burst) | start() | ~3 KB |
| Metronome click waveforms | init() | ~2.3 KB |
| Crossfade coefficients | init() | ~1.9 KB |
| **Total** | | **~66.7 MB** |

### 9.4 CPU Budget per Callback

| Operation | Cost per callback (192 frames) |
|-----------|-------------------------------|
| Dry signal copy (memcpy) | ~0.2 us |
| Loop read + sum (overdub) | ~0.5 us |
| Undo buffer write (conditional) | ~0.3 us |
| Metronome generation | ~0.1 us |
| Crossfade (when at boundary) | ~0.1 us |
| SPSC ring write (if recording) | ~0.3 us |
| **Total looper overhead** | **~1.5 us** |
| **Existing signal chain** | **~300-500 us** |
| **Looper as % of total** | **~0.3-0.5%** |

The looper adds negligible CPU overhead.

### 9.5 Recommended Implementation Order

**Phase 1 (Core Looper):**
1. LooperEngine class with IDLE/RECORDING/PLAYING state machine
2. Dual-track (dry+wet) capture in onAudioReady()
3. Loop playback with crossfade
4. JNI bridge for transport commands
5. Basic Kotlin UI (record/stop/play buttons, waveform display)

**Phase 2 (Overdub + Undo):**
6. OVERDUBBING state with in-place sum
7. Undo buffer and swap mechanism
8. Loop volume control

**Phase 3 (Metronome + Quantization):**
9. Tap tempo algorithm
10. Metronome click generation (bypass signal chain)
11. BPM-quantized loop length
12. Beat grid alignment on stop

**Phase 4 (Recording + Re-amping):**
13. SPSC ring buffer for disk streaming
14. Writer thread and WAV file output
15. Re-amping mode (load dry, play through chain)
16. Offline bounce/export

**Phase 5 (Polish):**
17. Memory class detection and dynamic buffer sizing
18. Sample rate change handling
19. Truncated WAV repair on launch
20. Recording list UI with playback/delete/share
