# Loop Crop/Trim Feature -- Technical Design Document

## Overview

This document specifies the complete technical design for loop crop/trim functionality
in the LooperEngine. The feature allows users to adjust loop start and end boundaries
after recording, removing unwanted audio at the head and/or tail of a loop to fix
timing issues.

**Terminology convention throughout this document:**
- **Trim**: Remove samples from the head (start) and/or tail (end) of the loop.
- **Crop**: Synonym for trim (keep a sub-region, discard the rest).
- **Head trim**: Remove N samples from the start of the loop.
- **Tail trim**: Remove N samples from the end of the loop.
- **Trim region**: The samples that SURVIVE the crop (between trim start and trim end).

---

## 1. Sample-Accurate Trimming Algorithm

### 1.1 Decision: Offset Pointers with Deferred Compaction

**Recommendation: Option (b) -- Offset pointers -- for preview, with Option (a) -- in-place
memmove -- as a one-time commit operation.**

This is a two-phase design:

**Phase 1 -- Preview (real-time safe):**
Set `trimStartOffset_` and `trimEndOffset_` atomics. The audio thread reads these on
every block and adjusts its playback window without touching the buffer data. This gives
instant, glitch-free preview of any trim position while the user drags the handles.

**Phase 2 -- Commit (non-real-time):**
When the user commits the trim, perform an in-place `memmove` to compact the buffers.
This must happen either:
  (a) While the looper is STOPPED (preferred -- simplest), or
  (b) Via a double-buffer swap on the audio thread (described below).

**Why not pure offset pointers permanently?**
Permanent offsets create complexity in every path that reads the buffer: overdub, undo,
export, waveform generation, re-amp loading. Every function would need `+ startOffset`
arithmetic and `min(length, endOffset)` bounds checks. Compacting once on commit keeps
all existing code paths unchanged.

**Why not copy to a new region within the pre-allocated buffer?**
The pre-allocated buffer is a single contiguous vector. Copying to a "new region" within
it would mean writing to samples AFTER the current loop length, requiring coordination
with the write position. It also wastes the head region (bytes 0..trimStart are dead
space until the next clear). Memmove is simpler and reclaims the space.

### 1.2 Data Flow

```
  User drags trim handles on waveform UI
       |
       v
  [Kotlin] setTrimPreview(startSample, endSample)  // JNI call
       |
       v
  [C++] trimPreviewStart_.store(startSample)        // atomic<int32_t>
        trimPreviewEnd_.store(endSample)             // atomic<int32_t>
       |
       v
  [Audio thread] processPlayback() reads trimPreviewStart_ / trimPreviewEnd_
                 Maps playPos_ to trimmed region
                 Crossfades at virtual boundaries
       |
       |  (User taps "Confirm")
       v
  [Kotlin] commitTrim()                              // JNI call
       |
       v
  [C++] commitTrim() -- pauses playback, memmoves buffers, updates lengths
       |
       v
  [Audio thread] resumes with compacted buffer, offsets reset to 0
```

### 1.3 Preview Playback Algorithm

During preview, the audio thread translates `playPos_` to the trimmed window:

```cpp
// In processPlayback(), when trim preview is active:
const int32_t tStart = trimPreviewStart_.load(std::memory_order_relaxed);
const int32_t tEnd   = trimPreviewEnd_.load(std::memory_order_relaxed);
const int32_t trimLen = tEnd - tStart;

if (trimLen <= 0) return; // degenerate: skip

for (int i = 0; i < numFrames; ++i) {
    // Map playPos_ into the trim window
    const int32_t bufPos = tStart + (playPos_ % trimLen);
    float sample = loopWetBuffer_[bufPos];

    // Apply crossfade at trimmed loop boundaries
    sample = applyTrimCrossfade(sample, playPos_ % trimLen, trimLen, fadeLen);
    sample = fast_math::denormal_guard(sample);

    outputBuffer[i] += sample * volume;

    playPos_++;
    if (playPos_ >= trimLen) {
        playPos_ = 0;
    }
}
```

The key insight: `playPos_` stays in `[0, trimLen)` and we add `tStart` to index
into the actual buffer. This means we never modify buffer data during preview.

### 1.4 Commit Operation (memmove)

```cpp
/**
 * Commit the current trim preview: compact buffers in-place.
 *
 * MUST be called from non-audio thread (JNI/UI). The looper should be
 * in IDLE or PLAYING state. This method briefly pauses playback via
 * an atomic flag, performs the memmove, then resumes.
 *
 * @return true if trim was applied successfully.
 */
bool commitTrim() {
    const int32_t tStart = trimPreviewStart_.load(std::memory_order_acquire);
    const int32_t tEnd   = trimPreviewEnd_.load(std::memory_order_acquire);
    const int32_t oldLen = loopLength_.load(std::memory_order_acquire);

    // Validate
    if (tStart < 0 || tEnd <= tStart || tEnd > oldLen) return false;
    if (tStart == 0 && tEnd == oldLen) return true; // no-op

    const int32_t newLen = tEnd - tStart;

    // --- Save pre-trim state for undo (if crop undo is enabled) ---
    preTrimLength_ = oldLen;
    preTrimStartOffset_ = tStart;
    trimUndoAvailable_.store(true, std::memory_order_release);

    // --- Signal audio thread to pause playback ---
    trimCommitInProgress_.store(true, std::memory_order_release);

    // Spin-wait until the audio thread acknowledges (it sets trimAcknowledged_)
    // Timeout after ~50ms (2400 spins at ~20us each) to prevent deadlock
    int spins = 0;
    while (!trimAcknowledged_.load(std::memory_order_acquire) && spins < 2400) {
        std::this_thread::sleep_for(std::chrono::microseconds(20));
        spins++;
    }

    // --- Compact all three buffers ---
    if (tStart > 0) {
        std::memmove(loopDryBuffer_.data(),
                     loopDryBuffer_.data() + tStart,
                     newLen * sizeof(float));
        std::memmove(loopWetBuffer_.data(),
                     loopWetBuffer_.data() + tStart,
                     newLen * sizeof(float));
        // Also compact undo buffer if it has valid data
        if (undoAvailable_.load(std::memory_order_relaxed)) {
            std::memmove(loopUndoBuffer_.data(),
                         loopUndoBuffer_.data() + tStart,
                         newLen * sizeof(float));
        }
    }
    // Tail trim: no memmove needed -- just reduce the length

    // --- Apply crossfade at new loop boundary ---
    applyBoundaryCrossfade(loopWetBuffer_.data(), newLen);
    applyBoundaryCrossfade(loopDryBuffer_.data(), newLen);
    if (undoAvailable_.load(std::memory_order_relaxed)) {
        applyBoundaryCrossfade(loopUndoBuffer_.data(), newLen);
    }

    // --- Update lengths ---
    loopLengthInternal_ = newLen;
    loopLength_.store(newLen, std::memory_order_release);

    // --- Reset trim preview ---
    trimPreviewStart_.store(0, std::memory_order_release);
    trimPreviewEnd_.store(newLen, std::memory_order_release);

    // --- Reset playback position (proportional mapping) ---
    // Map old position into new range to maintain musical context
    // playPos_ will be clamped by the audio thread on resume

    // --- Resume playback ---
    trimAcknowledged_.store(false, std::memory_order_release);
    trimCommitInProgress_.store(false, std::memory_order_release);

    return true;
}
```

### 1.5 Audio Thread Pause Protocol

The audio thread checks `trimCommitInProgress_` at the top of `processAudioBlock()`:

```cpp
// At the start of processAudioBlock():
if (trimCommitInProgress_.load(std::memory_order_acquire)) {
    // Signal acknowledgment to the committing thread
    trimAcknowledged_.store(true, std::memory_order_release);

    // Output silence for this block (don't touch buffers)
    // The live signal in outputBuffer passes through unchanged
    playbackPosition_.store(0, std::memory_order_release);
    return; // Skip all looper processing this block
}
```

**Worst-case silence duration**: One audio callback period. At 48kHz with a 256-sample
buffer, that is 5.3ms -- completely inaudible as a gap. At 512 samples, 10.7ms -- still
below perceptual threshold for a single dropout in a loop context.

**Why not lock-free?** We could use a triple-buffer scheme, but the memmove itself takes
<1ms for even 120s of audio (5.76M floats * 4 bytes = 23MB at ~20 GB/s memory bandwidth).
The pause protocol is simpler, more predictable, and the silence gap is negligible.

### 1.6 Timing Analysis

For a 120-second loop at 48kHz:
- Buffer size: 5,760,000 floats = 23,040,000 bytes = 22.0 MB
- memmove speed on modern ARM (Cortex-A76+): ~15-25 GB/s for large aligned copies
- **memmove time for 3 buffers**: 3 * 22 MB / 15 GB/s = ~4.4ms worst case
- **Total pause**: ~5-10ms (memmove + crossfade application)
- **Perceptual impact**: Inaudible. One loop iteration click at most.

---

## 2. Crossfade at New Boundaries

### 2.1 The Problem

The original recording has a crossfade baked into the boundary region by
`applyCrossfadeToRecording()`. After trimming:

- **Head trim**: The old crossfade zone at the start (samples 0..fadeLen) is gone.
  The new start is an arbitrary point in the waveform.
- **Tail trim**: The old crossfade zone at the end (samples len-fadeLen..len) is gone.
  The new end is an arbitrary point in the waveform.
- The new start and new end samples are almost certainly NOT at the same amplitude,
  so looping them directly will produce a click.

### 2.2 Solution: Re-apply Hann Crossfade at New Boundary

After memmove compaction, apply the same crossfade technique used in
`applyCrossfadeToRecording()`:

```cpp
/**
 * Apply Hann crossfade at loop boundary for a compacted buffer.
 * Blends the last fadeLen samples with the first fadeLen samples.
 *
 * @param buffer   Buffer to process in-place.
 * @param length   New loop length after trim.
 */
void applyBoundaryCrossfade(float* buffer, int32_t length) {
    const int32_t fadeLen = getEffectiveFadeLength(length);
    if (fadeLen <= 0 || length < fadeLen * 2) return;

    const int32_t effectiveFade = std::min(fadeLen,
        static_cast<int32_t>(fadeInTable_.size()));

    for (int32_t i = 0; i < effectiveFade; ++i) {
        const int32_t endIdx = length - effectiveFade + i;
        const float head = buffer[i];
        const float tail = buffer[endIdx];

        // Blend: start of loop fades in, end of loop fades out
        buffer[i] = head * fadeInTable_[i] + tail * fadeOutTable_[i];
    }
    // Note: The tail region (endIdx samples) is now "used up" by the crossfade.
    // The playback crossfade in processPlayback() handles this during playback
    // by reading from the head region when in the tail crossfade zone.
}
```

### 2.3 Crossfade During Preview

During preview (before commit), the audio thread applies a VIRTUAL crossfade at the
trimmed boundaries using the same fade tables, but WITHOUT modifying buffer data:

```cpp
float applyTrimCrossfade(float sample, int32_t posInTrim, int32_t trimLen,
                         int32_t fadeLen) const {
    const int32_t fadeStart = trimLen - fadeLen;
    if (posInTrim < fadeStart || fadeLen <= 0) return sample;

    const int32_t fadeIdx = posInTrim - fadeStart;
    if (fadeIdx >= fadeLen) return sample;

    // Fade out the tail, fade in the corresponding head sample
    const float fadeOut = fadeOutTable_[fadeIdx];
    const float fadeIn  = fadeInTable_[fadeIdx];

    // Head sample from the trimmed region start
    const int32_t tStart = trimPreviewStart_.load(std::memory_order_relaxed);
    const float headSample = loopWetBuffer_[tStart + fadeIdx];

    return sample * fadeOut + headSample * fadeIn;
}
```

### 2.4 Should We Use Fade-In/Fade-Out Instead of Crossfade?

**No. Use crossfade (blend start and end), not independent fades.**

Rationale:
- Independent fade-in/fade-out creates a perceptible volume dip at the boundary.
  A 5ms linear fade can drop perceived level by 3-6dB momentarily.
- Crossfade (sum of overlapping Hann windows = unity gain) maintains constant
  perceived loudness across the boundary.
- The existing `applyCrossfadeToRecording()` already uses this approach, so the
  trim crossfade is consistent with the recording crossfade.

### 2.5 Edge Case: Very Short Loops After Trim

If the trimmed region is shorter than `2 * fadeLen`, the crossfade would overlap
with itself. The existing `getEffectiveFadeLength()` handles this by capping the
fade at 2% of loop length, with a floor of `kMinCrossfadeSamples` (48 samples = 1ms).

For the absolute minimum: if `trimLen < 96 samples` (2ms at 48kHz), we skip
crossfade entirely and accept a potential click. This is acceptable because a loop
shorter than 2ms is not musically useful and the user is clearly doing something
experimental.

---

## 3. Preserving Overdub/Undo Integrity

### 3.1 Decision: Crop the Undo Buffer Identically

**Recommendation: Crop the undo buffer with the same start/end offsets as the
dry and wet buffers.**

Rationale:
- The undo buffer is a snapshot of the wet buffer from before the most recent overdub.
  Its sample indices are 1:1 aligned with the wet buffer.
- If we crop wet[100..5000] to wet[0..4900], we must also crop undo[100..5000]
  to undo[0..4900], or a subsequent undo would restore samples that no longer
  align with the loop length.
- This maintains the invariant: `undo[i]` is the wet signal at position `i`
  before the last overdub, for all `i` in `[0, loopLength)`.

### 3.2 Implementation

In `commitTrim()`, the undo buffer memmove happens alongside the dry and wet buffers
(see section 1.4 pseudocode). The `undoAvailable_` flag remains valid because the
undo buffer still represents a valid pre-overdub state for the trimmed region.

### 3.3 Alternative Considered: Invalidate Undo on Crop

We could simply set `undoAvailable_ = false` after a crop, which is simpler but
loses the user's undo history. This is a worse user experience because:
1. The user may have done a careful overdub, then noticed the loop boundaries
   need adjustment. They would lose their undo safety net.
2. Cropping is a boundary operation; it does not modify sample content within the
   surviving region (except for the crossfade zone at the edges).

**Conclusion: Crop the undo buffer. It is one extra memmove call, negligible cost.**

### 3.4 Crop During Active Overdub

**Disallow crop/trim while in OVERDUBBING state.** The incremental undo copy
(`undoCopyDone_` flag) assumes the undo buffer and wet buffer have the same length
and index space. Changing the loop length mid-overdub would corrupt the incremental
copy logic.

The UI should grey out the crop/trim controls when the looper state is OVERDUBBING
or RECORDING. Crop is only available in PLAYING or IDLE states.

---

## 4. Real-Time Preview

### 4.1 Decision: Yes, Preview BEFORE Commit

**Recommendation: Full real-time preview of the trimmed loop.**

The user must hear the trimmed loop playing continuously while dragging the trim
handles. Without preview, trimming is guesswork -- the user has no way to judge
whether the trim points sound correct until they commit, and if they're wrong,
they have to undo and try again.

### 4.2 Implementation Strategy

As described in section 1.3, preview works by:
1. The UI sends trim start/end sample positions via atomic stores.
2. The audio thread reads these atomics and maps `playPos_` into the trimmed region.
3. The audio thread applies a virtual crossfade at the trimmed boundaries.
4. No buffer data is modified during preview.

### 4.3 Activating Preview Mode

A dedicated atomic flag `trimPreviewActive_` gates the preview logic:

```cpp
// UI thread (via JNI):
void setTrimPreview(int32_t startSample, int32_t endSample) {
    const int32_t len = loopLength_.load(std::memory_order_acquire);
    // Validate and clamp
    int32_t s = std::clamp(startSample, 0, len);
    int32_t e = std::clamp(endSample, 0, len);
    if (e - s < kMinCrossfadeSamples * 2) {
        // Too short -- snap end to minimum length
        e = std::min(s + kMinCrossfadeSamples * 2, len);
    }
    trimPreviewStart_.store(s, std::memory_order_release);
    trimPreviewEnd_.store(e, std::memory_order_release);
    trimPreviewActive_.store(true, std::memory_order_release);
}

void cancelTrimPreview() {
    trimPreviewActive_.store(false, std::memory_order_release);
    // Reset playback to full loop -- playPos_ is clamped automatically
}
```

### 4.4 Preview During Playback vs. Stopped

- **PLAYING**: Preview plays the trimmed sub-region at normal speed. The user hears
  the loop boundary in real time and can judge if it clicks.
- **IDLE**: No audio plays. The waveform display shows the trim handles but there
  is no auditory preview. The user must press play to hear it.
- **Recommendation**: When the user opens the trim editor, auto-start playback if
  the looper is IDLE and a loop exists. This gives immediate auditory feedback.

### 4.5 Performance Impact of Preview

The preview adds exactly 2 atomic loads per audio block (`trimPreviewStart_`,
`trimPreviewEnd_`) and one modulo operation per sample. On ARM64, relaxed atomic
loads compile to plain `ldr` instructions -- zero overhead. The modulo compiles to
a comparison and conditional subtraction (not a division) because the compiler
knows the divisor is positive and the dividend is incrementing by 1.

**Total additional CPU per sample**: ~2 cycles. Negligible.

---

## 5. Snap-to-Zero-Crossing

### 5.1 Decision: Yes, Offer as an Option

**Recommendation: Provide snap-to-zero-crossing as an optional "snap" mode that the
user can toggle. Default: OFF.**

Rationale:
- Zero-crossing snap eliminates clicks at trim points BEFORE the crossfade is applied,
  giving the cleanest possible result.
- However, zero crossings may not align with musically meaningful boundaries (beat
  boundaries, note attacks). Forcing snap-to-zero can shift the trim point by up to
  half a wavelength of the lowest frequency present, which at 80Hz (low E string)
  is 6.25ms -- audible as a timing shift.
- Making it optional lets the user choose: precise timing (snap off) vs. clean
  boundaries (snap on). The crossfade handles the remaining discontinuity either way.

### 5.2 Algorithm: Nearest Zero-Crossing Search

```cpp
/**
 * Find the nearest zero-crossing to the given sample position.
 * Searches both forward and backward within a window.
 *
 * A zero-crossing is defined as a sign change between adjacent samples:
 *   buffer[n] * buffer[n+1] <= 0
 *
 * When multiple zero-crossings are equidistant, prefer the one where the
 * signal is rising (positive slope) for head trim, and falling (negative
 * slope) for tail trim. This places the trim point at the natural start
 * of a waveform cycle.
 *
 * @param buffer     The audio buffer to search.
 * @param pos        Desired trim position (sample index).
 * @param length     Total buffer length.
 * @param maxSearch  Maximum samples to search in each direction.
 * @param preferRising  True for head trim (prefer rising ZC), false for tail.
 * @return Snapped position (unchanged if no ZC found within window).
 */
int32_t findNearestZeroCrossing(const float* buffer, int32_t pos,
                                 int32_t length, int32_t maxSearch,
                                 bool preferRising) const {
    // Search window: recommended maxSearch = sampleRate_ / 80
    // (half wavelength of lowest guitar string, ~6ms at 48kHz = 300 samples)

    int32_t bestPos = pos;
    int32_t bestDist = maxSearch + 1;
    bool bestRising = false;

    for (int32_t offset = -maxSearch; offset <= maxSearch; ++offset) {
        const int32_t idx = pos + offset;
        if (idx < 0 || idx + 1 >= length) continue;

        const float s0 = buffer[idx];
        const float s1 = buffer[idx + 1];

        // Check for zero-crossing (sign change or exact zero)
        if (s0 * s1 <= 0.0f) {
            const int32_t dist = std::abs(offset);
            const bool rising = (s1 > s0);

            // Prefer closer crossings; break ties by slope preference
            if (dist < bestDist ||
                (dist == bestDist && rising == preferRising && !bestRising)) {
                bestPos = idx;
                bestDist = dist;
                bestRising = rising;
            }
        }
    }

    return bestPos;
}
```

### 5.3 Search Window Size

The search window should be large enough to find at least one zero-crossing for the
lowest frequency present in guitar audio:

- Low E standard tuning: 82.4 Hz -> half period = 6.07ms = 291 samples at 48kHz
- Drop D: 73.4 Hz -> half period = 6.81ms = 327 samples
- Drop C: 65.4 Hz -> half period = 7.65ms = 367 samples

**Recommended `maxSearch`**: `static_cast<int32_t>(sampleRate_ / 60.0f)` = 800 samples
at 48kHz. This covers drop tunings down to ~60Hz with margin.

### 5.4 Which Buffer to Search

Search the **wet** buffer (post-effects), not the dry buffer. The user hears the wet
signal during loop playback, so zero-crossings in the wet signal determine whether the
trim point clicks. Effects like distortion, compression, and EQ shift zero-crossing
positions relative to the dry signal.

### 5.5 Performance

The search examines at most `2 * maxSearch` = 1600 samples, performing one multiply and
one comparison per sample. At 48kHz this is ~33us of buffer data, computed once per
trim handle movement (not per audio block). Negligible.

---

## 6. Snap-to-Beat

### 6.1 Decision: Yes, in Quantized Mode

**Recommendation: When quantized mode is active, trim handles snap to beat boundaries.
Sub-beat snapping (1/8, 1/16 note divisions) available via a resolution selector.**

### 6.2 Algorithm: Beat Boundary Calculation

```cpp
/**
 * Snap a sample position to the nearest beat boundary.
 *
 * @param pos            Desired position in samples.
 * @param bpm            Current tempo.
 * @param subdivisions   Snap resolution: 1 = quarter note, 2 = eighth, 4 = sixteenth.
 * @return Snapped position in samples.
 */
int32_t snapToBeat(int32_t pos, float bpm, int subdivisions = 1) const {
    const double samplesPerBeat = (60.0 / static_cast<double>(bpm))
                                   * static_cast<double>(sampleRate_);
    const double samplesPerSnap = samplesPerBeat / static_cast<double>(subdivisions);

    // Round to nearest snap point
    const double snapIndex = std::round(static_cast<double>(pos) / samplesPerSnap);
    const int32_t snapped = static_cast<int32_t>(snapIndex * samplesPerSnap);

    return std::clamp(snapped, 0, loopLengthInternal_);
}
```

### 6.3 Snap Resolution Options

Present to the user as a toggle or dropdown:
- **1 bar** (coarsest)
- **1 beat** (default in quantized mode)
- **1/2 beat** (eighth note)
- **1/4 beat** (sixteenth note)
- **Free** (no snapping, or zero-crossing snap if enabled)

### 6.4 Combining Beat Snap with Zero-Crossing Snap

If both are enabled, beat snap takes priority (it represents musical intent).
Zero-crossing snap is only applied within the "free" resolution mode.

The UI should make this clear: the snap mode selector replaces zero-crossing snap
when a beat subdivision is selected.

---

## 7. Waveform Display for Trim UI

### 7.1 Decision: Min/Max Pairs Per Display Column

**Recommendation: Generate a downsampled waveform overview using min/max sample pairs
per horizontal pixel column. This is the standard technique used by every DAW.**

### 7.2 Algorithm: Waveform Summary Generation

```cpp
/**
 * Generate a waveform summary for display, computing min/max sample values
 * per pixel column.
 *
 * Called from JNI thread (NOT audio thread). Reads from the wet buffer
 * while the audio thread may be writing (overdubbing). This is safe because:
 * - We read float values that were previously written (no partial writes on ARM64
 *   for aligned 32-bit values).
 * - Minor visual inconsistency during active overdub is acceptable (waveform
 *   refreshes every ~100ms from the UI poll timer).
 *
 * @param outMin       Output array for minimum values per column (caller-allocated).
 * @param outMax       Output array for maximum values per column (caller-allocated).
 * @param numColumns   Number of display columns (typically screen width in dp * density).
 * @param startSample  First sample to include (0 for full loop, or trimPreviewStart).
 * @param endSample    Last sample (exclusive) to include.
 * @return Number of columns actually written (may be < numColumns if loop is short).
 */
int32_t generateWaveformSummary(float* outMin, float* outMax,
                                 int32_t numColumns,
                                 int32_t startSample,
                                 int32_t endSample) const {
    const int32_t len = loopLength_.load(std::memory_order_acquire);
    if (len <= 0 || numColumns <= 0) return 0;

    // Clamp range
    const int32_t s = std::clamp(startSample, 0, len);
    const int32_t e = std::clamp(endSample, 0, len);
    const int32_t rangeLen = e - s;
    if (rangeLen <= 0) return 0;

    const int32_t actualColumns = std::min(numColumns, rangeLen);
    const double samplesPerColumn = static_cast<double>(rangeLen)
                                    / static_cast<double>(actualColumns);

    for (int32_t col = 0; col < actualColumns; ++col) {
        const int32_t colStart = s + static_cast<int32_t>(col * samplesPerColumn);
        const int32_t colEnd   = s + static_cast<int32_t>((col + 1) * samplesPerColumn);
        const int32_t safeEnd  = std::min(colEnd, e);

        float minVal =  1.0f;
        float maxVal = -1.0f;

        for (int32_t j = colStart; j < safeEnd; ++j) {
            const float v = loopWetBuffer_[j];
            if (v < minVal) minVal = v;
            if (v > maxVal) maxVal = v;
        }

        outMin[col] = minVal;
        outMax[col] = maxVal;
    }

    return actualColumns;
}
```

### 7.3 Resolution Recommendations

| Scenario | Display width | Columns | Samples/column (10s loop, 48kHz) |
|----------|--------------|---------|----------------------------------|
| Phone portrait | ~360dp = ~1080px | 1080 | 444 |
| Phone landscape | ~780dp = ~2340px | 2340 | 205 |
| Tablet | ~1200dp = ~2400px | 2400 | 200 |

At 200-400 samples per column, the min/max representation captures all significant
waveform features. Individual cycles of a 100Hz guitar signal (480 samples/cycle)
span 1-2 columns, which is correct -- you see the envelope, not individual cycles.

### 7.4 Refresh Strategy

- **Initial load**: Generate full waveform on first trim editor open (~2ms for 1080 columns).
- **During recording/overdub**: Refresh every 100ms from a Kotlin coroutine that calls
  the JNI waveform generation function. This matches typical UI refresh rates (10 fps
  for waveform, 60 fps for cursor position).
- **During trim handle drag**: Do NOT regenerate the waveform. The waveform data is
  static once recorded. Only the trim markers and crossfade preview region change.
  Those are drawn as overlays by the Compose Canvas.

### 7.5 JNI Transfer

The min/max arrays are `FloatArray` in Kotlin. Use `SetFloatArrayRegion` for efficient
bulk copy across the JNI boundary:

```kotlin
// Kotlin side
external fun looperGetWaveformSummary(
    numColumns: Int,
    startSample: Int,
    endSample: Int
): FloatArray?  // Interleaved [min0, max0, min1, max1, ...]
```

Interleaved min/max in a single array avoids two JNI calls and two array allocations.
The Kotlin side de-interleaves for drawing.

### 7.6 Drawing the Waveform in Compose

```kotlin
Canvas(modifier = Modifier.fillMaxSize()) {
    val columnWidth = size.width / waveformData.size * 2  // pairs
    val centerY = size.height / 2f

    for (i in waveformData.indices step 2) {
        val minVal = waveformData[i]
        val maxVal = waveformData[i + 1]
        val x = (i / 2) * columnWidth

        // Map [-1, 1] to [height, 0]
        val yMin = centerY - maxVal * centerY  // maxVal draws ABOVE center
        val yMax = centerY - minVal * centerY  // minVal draws BELOW center

        drawLine(
            color = waveformColor,
            start = Offset(x, yMin),
            end = Offset(x, yMax),
            strokeWidth = columnWidth.coerceAtLeast(1f)
        )
    }

    // Draw trim handles as draggable vertical lines
    // Draw crossfade regions as semi-transparent overlays
    // Draw playback cursor as animated vertical line
}
```

---

## 8. Undo for Crop Operation

### 8.1 Decision: Yes, One-Level Crop Undo

**Recommendation: The crop operation is undoable. Keep the original buffer content
and length for one-level undo of the crop itself.**

### 8.2 Implementation Strategy

The crop undo is separate from the overdub undo (which uses `loopUndoBuffer_`).
It works by exploiting the fact that **memmove only moves data forward in the buffer;
it does not erase the original tail data**:

After `memmove(buf, buf + trimStart, newLen * sizeof(float))`:
- `buf[0..newLen-1]` contains the trimmed data (correct)
- `buf[newLen..oldLen-1]` still contains OLD data (stale but intact -- the original
  samples from positions `trimStart+newLen` to `oldLen-1`)
- `buf[trimStart..trimStart+newLen-1]` was overwritten by the memmove (original head
  data at positions `0..trimStart-1` is destroyed if trimStart > 0)

**Problem**: memmove overwrites the source region. After a head trim, the original
samples at positions `0..trimStart-1` are destroyed.

**Solution**: Before the memmove in `commitTrim()`, save the entire pre-trim loop to
a dedicated **crop undo buffer**. Since we already have pre-allocated buffers at full
capacity (`bufferCapacity_` samples), we can reuse the undo buffer's extra capacity:

```
bufferCapacity_ = sampleRate * 120s = 5,760,000 samples
loopLength = current loop (e.g. 480,000 = 10s)
undo buffer capacity = 5,760,000 samples (same pre-allocation)
```

However, the undo buffer is already used for overdub undo. We need a separate mechanism.

### 8.3 Crop Undo via Saved State

Store the pre-trim state as three values:
- `preTrimLength_`: The loop length before the crop
- `preTrimStartOffset_`: The head trim amount
- `cropUndoDrySnapshot_` / `cropUndoWetSnapshot_`: Full copies of the buffers

**Memory concern**: Storing full snapshots doubles the memory usage. For a 120s loop,
that is an additional 44 MB (dry + wet). On a 4GB device this is marginal.

**Better approach: Use the undo buffer for crop undo too, with mutual exclusion.**

The crop undo and overdub undo are mutually exclusive operations:
- Overdub undo is available only after an overdub (PLAYING state)
- Crop is available only in PLAYING or IDLE state
- When a crop is committed, the overdub undo is no longer meaningful (the buffer
  boundaries have changed)

Therefore:

```
Before crop:
  loopWetBuffer_:  [--- loop data (len=N) ---][--- unused capacity ---]
  loopUndoBuffer_: [--- pre-overdub snapshot (len=N) ---][--- unused ---]

Crop operation (commit):
  1. Copy loopWetBuffer_[0..N-1] -> loopUndoBuffer_[0..N-1]  // save for crop undo
  2. Copy loopDryBuffer_[0..N-1] -> loopUndoBuffer_[N..2N-1] // save dry too (uses undo capacity)
  3. memmove loopWetBuffer_ and loopDryBuffer_ for the trim
  4. Set cropUndoAvailable_ = true, undoAvailable_ = false
  5. Store preTrimLength_ and preTrimStartOffset_

Crop undo:
  1. Copy loopUndoBuffer_[0..preTrimLength_-1] -> loopWetBuffer_
  2. Copy loopUndoBuffer_[preTrimLength_..2*preTrimLength_-1] -> loopDryBuffer_
  3. Set loopLengthInternal_ = preTrimLength_
  4. Set cropUndoAvailable_ = false
```

**Capacity check**: The undo buffer has `bufferCapacity_` samples. For crop undo we
need `2 * preTrimLength_` samples (wet + dry). This fits as long as
`preTrimLength_ <= bufferCapacity_ / 2`, which is true for any loop up to 60s at 48kHz.
For loops longer than 60s, we can either:
  (a) Not support crop undo (set `cropUndoAvailable_ = false`), or
  (b) Only save the wet buffer (not dry), since the user typically only listens to wet.

**Recommendation**: Save both wet and dry up to 60s. For loops >60s, save only wet and
mark dry re-amp data as lost on undo (with a UI warning).

### 8.4 Crop Undo Command

Add a new command `UNDO_CROP` to the LooperCommand enum, or reuse the existing UNDO
command with context detection:

```cpp
LooperState handleUndo(LooperState state) {
    if (state != LooperState::PLAYING && state != LooperState::IDLE) return state;

    // Priority: crop undo first (more destructive, user expects it to undo crop)
    if (cropUndoAvailable_.load(std::memory_order_relaxed)) {
        return handleCropUndo(state);
    }

    // Fall back to overdub undo
    if (state == LooperState::PLAYING &&
        undoAvailable_.load(std::memory_order_relaxed)) {
        loopWetBuffer_.swap(loopUndoBuffer_);
        undoAvailable_.store(false, std::memory_order_release);
    }

    return state;
}
```

**UX consideration**: The UI should show "Undo Crop" as a distinct button in the trim
editor, separate from the "Undo Overdub" button in the main looper controls. This
avoids ambiguity about what will be undone.

---

## 9. New Member Variables

```cpp
// ---- Trim preview (cross-thread atomics) ----
std::atomic<bool> trimPreviewActive_{false};
std::atomic<int32_t> trimPreviewStart_{0};
std::atomic<int32_t> trimPreviewEnd_{0};

// ---- Trim commit synchronization ----
std::atomic<bool> trimCommitInProgress_{false};
std::atomic<bool> trimAcknowledged_{false};

// ---- Crop undo state ----
std::atomic<bool> cropUndoAvailable_{false};
int32_t preTrimLength_ = 0;           // Audio-thread-only after commit
int32_t preTrimStartOffset_ = 0;      // Audio-thread-only after commit

// ---- Zero-crossing snap ----
std::atomic<bool> zeroCrossingSnapEnabled_{false};
```

---

## 10. New JNI Functions

```cpp
// Trim preview
void looperSetTrimPreview(int startSample, int endSample);
void looperCancelTrimPreview();

// Trim commit
boolean looperCommitTrim();

// Crop undo
boolean looperUndoCrop();
boolean looperIsCropUndoAvailable();

// Zero-crossing snap
int looperFindNearestZeroCrossing(int position, boolean isHeadTrim);
void looperSetZeroCrossingSnap(boolean enabled);

// Beat snap
int looperSnapToBeat(int position, int subdivisions);

// Waveform
float[] looperGetWaveformSummary(int numColumns, int startSample, int endSample);
```

Total: 8 new JNI functions.

---

## 11. Edge Cases and Pitfalls

### 11.1 Trim to Zero Length
- **Guard**: Minimum trimmed length = `2 * kMinCrossfadeSamples` = 96 samples (2ms).
- The UI should prevent the trim handles from overlapping beyond this minimum.
- `setTrimPreview()` clamps to enforce this minimum.

### 11.2 Trim During State Transitions
- **Guard**: Only allow trim operations in PLAYING or IDLE state.
- If the user is recording or overdubbing, the trim editor is disabled.
- `commitTrim()` checks state and returns false if not PLAYING/IDLE.

### 11.3 Trim Preview + Overdub Start
- **Guard**: If trim preview is active and the user starts overdubbing, cancel the
  trim preview first. Overdubbing with offset pointers would corrupt the data.
- In `handleOverdub()`: `trimPreviewActive_.store(false)`.

### 11.4 Trim Preview + Undo
- If the user presses undo while trim preview is active, cancel the preview and
  perform the normal undo. The preview is a non-destructive visual/audio overlay;
  canceling it has no data loss.

### 11.5 Export After Trim (But Before Commit)
- `exportLoop()` reads `loopLength_` and the raw buffer from index 0. If trim preview
  is active but not committed, the export produces the ORIGINAL (untrimmed) loop.
  This is correct behavior -- the user explicitly chose not to commit yet.

### 11.6 Sample Rate Change Between Recording and Trimming
- Not a concern: the sample rate is fixed for the lifetime of the LooperEngine (set in
  `init()`). It cannot change while a loop exists.

### 11.7 Concurrent commitTrim() Calls
- **Guard**: The spin-wait in `commitTrim()` and the `trimCommitInProgress_` flag
  prevent concurrent execution. If called twice simultaneously (e.g., double-tap),
  the second call will see `trimCommitInProgress_ == true` and should fail fast:
  ```cpp
  if (trimCommitInProgress_.load(std::memory_order_acquire)) return false;
  ```
  Use `compare_exchange_strong` to atomically claim the commit:
  ```cpp
  bool expected = false;
  if (!trimCommitInProgress_.compare_exchange_strong(expected, true,
      std::memory_order_acq_rel)) return false;
  ```

### 11.8 Playback Position After Commit
- After `commitTrim()`, `playPos_` must be reset to 0 (start of new trimmed loop).
  If we tried to proportionally map the old position, we might land in the crossfade
  zone or outside the new range. Resetting to 0 is the safest and most predictable
  behavior.

### 11.9 Crossfade Table Size vs. Effective Fade Length
- The pre-computed `fadeInTable_` / `fadeOutTable_` have `crossfadeSamples_` entries
  (240 at 48kHz). `getEffectiveFadeLength()` may return a SMALLER value for short loops.
  All crossfade code must use the effective fade length, not the table size, as the
  loop index. The existing code already does this correctly.

### 11.10 BPM Change During Trim Preview
- If the user changes BPM while trim preview is active with beat-snap enabled, the
  snap points shift. The UI should re-snap the handles to the new beat grid when
  BPM changes. This is a UI-layer concern, not an audio-thread concern.

### 11.11 Denormal Accumulation in Trimmed Loop
- The existing `denormal_guard()` in `processPlayback()` handles this. No additional
  action needed for trimmed loops.

---

## 12. Complete API Surface

### 12.1 C++ (LooperEngine public methods to add)

```cpp
// === Trim Preview ===
void setTrimPreview(int32_t startSample, int32_t endSample);
void cancelTrimPreview();
bool isTrimPreviewActive() const;

// === Trim Commit ===
bool commitTrim();
bool undoCrop();
bool isCropUndoAvailable() const;

// === Snap Helpers (called from JNI thread, NOT audio thread) ===
int32_t findNearestZeroCrossing(int32_t pos, bool isHeadTrim) const;
int32_t snapToBeat(int32_t pos, int subdivisions = 1) const;
void setZeroCrossingSnapEnabled(bool enabled);
bool isZeroCrossingSnapEnabled() const;

// === Waveform Summary ===
int32_t generateWaveformSummary(float* outMinMax, int32_t numColumns,
                                 int32_t startSample, int32_t endSample) const;
```

### 12.2 Kotlin (AudioEngine JNI externals to add)

```kotlin
// Trim
external fun looperSetTrimPreview(startSample: Int, endSample: Int)
external fun looperCancelTrimPreview()
external fun looperCommitTrim(): Boolean
external fun looperUndoCrop(): Boolean
external fun looperIsCropUndoAvailable(): Boolean

// Snap
external fun looperFindNearestZeroCrossing(position: Int, isHeadTrim: Boolean): Int
external fun looperSnapToBeat(position: Int, subdivisions: Int): Int
external fun looperSetZeroCrossingSnap(enabled: Boolean)

// Waveform
external fun looperGetWaveformSummary(
    numColumns: Int, startSample: Int, endSample: Int
): FloatArray?
```

### 12.3 Kotlin ViewModel Functions

```kotlin
class LooperViewModel {
    // Trim state
    val trimPreviewActive: StateFlow<Boolean>
    val trimStartSample: MutableStateFlow<Int>
    val trimEndSample: MutableStateFlow<Int>
    val cropUndoAvailable: StateFlow<Boolean>
    val snapMode: MutableStateFlow<SnapMode>  // FREE, ZERO_CROSSING, BEAT, BEAT_HALF, BEAT_QUARTER

    // Waveform
    val waveformData: StateFlow<FloatArray?>

    // Actions
    fun setTrimRange(start: Int, end: Int)
    fun commitTrim()
    fun undoCrop()
    fun setSnapMode(mode: SnapMode)
    fun refreshWaveform(numColumns: Int)
}
```

---

## 13. Implementation Phases

### Phase 1: Core Trim Engine (C++)
- Add trim preview atomics and `setTrimPreview()` / `cancelTrimPreview()`
- Modify `processPlayback()` to respect trim preview offsets
- Add `applyTrimCrossfade()` for virtual crossfade during preview
- Add `commitTrim()` with memmove and crossfade re-application
- Add trim commit synchronization protocol
- Add `applyBoundaryCrossfade()` helper
- **Tests**: Unit test memmove correctness, crossfade unity gain, edge cases

### Phase 2: Waveform Summary (C++)
- Add `generateWaveformSummary()` method
- **Tests**: Verify min/max accuracy, boundary conditions

### Phase 3: Snap Algorithms (C++)
- Add `findNearestZeroCrossing()`
- Add `snapToBeat()`
- **Tests**: Verify snap accuracy with known waveforms

### Phase 4: Crop Undo (C++)
- Add crop undo buffer management
- Add `undoCrop()` method
- Ensure mutual exclusion with overdub undo
- **Tests**: Verify round-trip crop->undo restores original data

### Phase 5: JNI Bridge
- Add 8-9 new JNI functions
- Wire to LooperEngine methods

### Phase 6: Kotlin/Compose UI
- LooperTrimEditor composable with waveform display
- Draggable trim handles with touch gesture detection
- Snap mode selector
- Confirm/Cancel/Undo buttons
- Integration with LooperViewModel

**Estimated total effort**: ~800-1200 lines C++, ~200 lines JNI, ~600 lines Kotlin/Compose.

---

## 14. Summary of Recommendations

| Question | Recommendation | Key Rationale |
|----------|---------------|---------------|
| 1. Trimming algorithm | Offset pointers (preview) + memmove (commit) | Preview is real-time safe; memmove keeps code simple |
| 2. Crossfade at new boundaries | Hann crossfade, same as recording | Constant gain; consistent with existing code |
| 3. Overdub/undo integrity | Crop undo buffer identically | Maintains index alignment invariant |
| 4. Real-time preview | Yes, via atomic offsets in audio thread | Essential for usability |
| 5. Zero-crossing snap | Optional, default OFF | Clean boundaries but may shift timing |
| 6. Beat snap | Yes, in quantized mode with subdivision options | Musical precision for quantized workflows |
| 7. Waveform display | Min/max pairs per pixel column | Industry standard, efficient, accurate |
| 8. Crop undo | Yes, one-level, reuse undo buffer capacity | Low cost, high user value |
