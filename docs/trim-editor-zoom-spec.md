# Trim Editor Waveform Zoom -- Design Specification

## Problem

The current `LooperTrimEditor` always shows the entire loop at 1x zoom. For a
10-second loop at 48kHz (480,000 samples) rendered into ~300 display columns,
each column represents ~1,600 samples (~33ms). Moving a trim handle by a single
pixel jumps 1,600 samples. At normal phone widths (~360dp usable inside
24dp horizontal padding = ~312dp), the finest adjustment possible is roughly
33ms per touch pixel. For precise editing -- placing a trim point at a specific
transient, between notes, or at a zero crossing -- this resolution is
inadequate. Musicians need sub-millisecond precision for clean loop points.

## Approach Evaluation

### Approach A: Pinch-to-Zoom with Pan
- **Pros**: Industry standard for touch, natural, allows continuous zoom levels
- **Cons**: Gesture conflict with handle drag (both are single-finger). Must
  carefully disambiguate "dragging a handle" from "panning the viewport". Two
  simultaneous touch interaction models on same surface.
- **Risk**: Medium. Solvable with clear gesture routing rules.

### Approach B: Zoom Slider / Buttons
- **Pros**: Zero gesture conflict. Simple implementation. Always discoverable.
- **Cons**: Not intuitive for musicians accustomed to pinch-to-zoom on every
  other app. Requires extra UI chrome eating vertical space. Cannot zoom and
  pan simultaneously.
- **Risk**: Low implementation risk, moderate UX risk (feels dated).

### Approach C: Double-Tap to Zoom
- **Pros**: Zero gesture conflict. No extra UI elements.
- **Cons**: Limited zoom levels (usually 2-3 discrete levels). Cannot center
  zoom on arbitrary point easily. Slow to navigate. No DAW or audio editor
  uses this as primary zoom mechanism.
- **Risk**: Low, but insufficient for the precision editing use case.

### Approach D: Overview + Detail (Minimap)
- **Pros**: Gold standard in professional DAWs (Ableton, Logic, Audacity).
  User always has spatial context. No gesture conflicts because overview and
  detail are separate surfaces. Overview can show full loop with viewport
  rectangle; detail shows zoomed region. Handles exist only in detail view.
- **Cons**: Requires more vertical space (two waveform views). More complex
  layout. Overview waveform must be drawn separately.
- **Risk**: Medium (layout complexity), but eliminates all gesture conflicts.

### Selected Approach: D + A Hybrid (Overview Minimap + Pinch-to-Zoom on Detail)

**Rationale**: The minimap pattern is the proven solution in professional audio
editing, and it solves the hardest UX problem (spatial context when zoomed). By
combining it with pinch-to-zoom on the detail view, we get the best of both
worlds: discoverable zoom via the minimap viewport drag, and fast/intuitive
zoom via pinch gesture for users who expect it.

The key insight that resolves the gesture conflict is: **trim handles are
single-finger drag, while pinch is inherently two-finger**. Pan (scrolling
when zoomed) is the conflict point. We solve this by:
1. Making the minimap viewport draggable for coarse navigation (eliminates
   need for single-finger pan on the detail view in most cases).
2. Two-finger pan on detail view when zoomed (pinch already requires two
   fingers; sliding two fingers without pinching = pan).
3. Single-finger touch on detail view ONLY triggers handle drag (within hit
   radius) or does nothing (outside hit radius). Never pan.

This means: single finger = handle manipulation only, two fingers = zoom/pan.
Clean separation, zero ambiguity.

---

## Interaction Model

### Gesture Routing (Detail Waveform Canvas)

| Gesture              | Zoom = 1x        | Zoom > 1x             |
|----------------------|-------------------|-----------------------|
| 1-finger near handle | Drag handle       | Drag handle            |
| 1-finger elsewhere   | No-op             | No-op (not pan!)       |
| 2-finger pinch       | Zoom in           | Zoom in/out            |
| 2-finger slide       | No-op (at 1x)     | Pan viewport           |
| 2-finger pinch+slide | Zoom + pan (both) | Zoom + pan (both)      |

### Gesture Routing (Overview Minimap)

| Gesture              | Action                                   |
|----------------------|------------------------------------------|
| 1-finger drag        | Drag viewport rectangle (pan detail)     |
| 1-finger tap         | Center viewport on tap position          |
| No pinch/zoom        | Overview always shows full loop           |

### Zoom Range

- **Minimum**: 1.0x (entire loop visible, current behavior)
- **Maximum**: 64.0x (at 48kHz, 300 columns, this gives ~2.5 samples per
  column = ~0.05ms precision, well beyond what any human needs)
- **Practical sweet spot**: Most users will zoom 4x-16x. At 8x zoom, each
  pixel represents ~4ms -- tight enough to see individual waveform cycles
  at guitar frequencies.
- **Zoom steps**: Continuous (not discrete). Pinch gesture maps linearly to
  log2(zoomLevel) for perceptually uniform scaling.

### Zoom Anchor Point

Pinch-to-zoom always zooms centered on the midpoint between the two fingers.
This is the standard behavior users expect from maps, photos, etc. The viewport
adjusts so that the sample under the pinch center remains at the same pixel
position throughout the zoom.

### Auto-scroll on Handle Drag

When a handle is being dragged toward the edge of the zoomed viewport:
- If the handle pixel position is within 32dp of the left or right edge of
  the detail canvas, the viewport auto-scrolls in that direction.
- Scroll speed ramps linearly: 0 at 32dp from edge, maximum at 0dp from edge.
- Maximum scroll speed: 5% of visible range per frame (at 60fps).
- This allows the user to drag a handle across the entire loop without
  lifting their finger, even when zoomed in.

### Handle Visibility

When the user pinch-zooms and the active (most recently touched) handle
scrolls out of view:
- A small indicator arrow appears at the left or right edge of the detail
  canvas showing the direction and distance to the off-screen handle.
- The indicator uses the handle's color (SafeGreen for start, WarningAmber
  for end).
- Tapping the indicator animates the viewport to center on that handle.

---

## State Variables

### New State in TrimWaveformCanvas

```kotlin
// Zoom level: 1.0 = entire loop visible, higher = zoomed in
var zoomLevel by remember { mutableFloatStateOf(1f) }

// Viewport start in samples (first visible sample)
var viewportStartSample by remember { mutableIntStateOf(0) }

// Derived: viewport end sample
val viewportSampleSpan = (loopLength / zoomLevel).roundToInt().coerceAtLeast(MIN_HANDLE_GAP_SAMPLES)
val viewportEndSample = (viewportStartSample + viewportSampleSpan).coerceAtMost(loopLength)
```

### Constants

```kotlin
/** Maximum zoom level (64x = ~0.05ms per pixel at typical config). */
private const val MAX_ZOOM_LEVEL = 64f

/** Minimum zoom level (1x = entire loop visible). */
private const val MIN_ZOOM_LEVEL = 1f

/** Auto-scroll trigger zone in dp (handles near edge trigger viewport scroll). */
private val AUTO_SCROLL_EDGE_ZONE = 32.dp

/** Overview minimap height in dp. */
private val OVERVIEW_HEIGHT = 40.dp

/** Detail waveform height in dp (same as current 140dp). */
private val DETAIL_HEIGHT = 140.dp
```

---

## Coordinate Conversion Formulas

### Current (Zoom = 1x)

```
sampleToPixel(sample) = sample / loopLength * canvasWidth
pixelToSample(pixel)  = pixel / canvasWidth * loopLength
```

### Zoomed

```
// In detail view, only [viewportStartSample, viewportEndSample] is visible
sampleToPixel(sample) = (sample - viewportStartSample) / viewportSampleSpan * canvasWidth
pixelToSample(pixel)  = pixel / canvasWidth * viewportSampleSpan + viewportStartSample

// In overview, always full loop
overviewSampleToPixel(sample) = sample / loopLength * overviewCanvasWidth
```

### Viewport Rectangle in Overview

```
val vpLeftPx  = viewportStartSample / loopLength * overviewCanvasWidth
val vpRightPx = viewportEndSample / loopLength * overviewCanvasWidth
```

### Pinch-to-Zoom Math

When a pinch gesture reports `centroidPx` (pixel x-coordinate of pinch center)
and `zoomDelta` (>1 = zooming in, <1 = zooming out):

```kotlin
// Sample under the pinch centroid (must remain fixed)
val anchorSample = pixelToSample(centroidPx)

// Apply zoom
val newZoom = (zoomLevel * zoomDelta).coerceIn(MIN_ZOOM_LEVEL, MAX_ZOOM_LEVEL)
val newSpan = (loopLength / newZoom).roundToInt()

// Recompute viewportStart so anchorSample stays at centroidPx
val anchorFraction = centroidPx / canvasWidth
val newStart = (anchorSample - anchorFraction * newSpan).roundToInt()
    .coerceIn(0, (loopLength - newSpan).coerceAtLeast(0))

zoomLevel = newZoom
viewportStartSample = newStart
```

---

## Gesture Detection Implementation

### Compose API: transformable() vs Custom

The `Modifier.transformable()` modifier provides pinch-to-zoom and pan, but it
consumes ALL gestures including single-finger drag. We cannot use it directly
because single-finger must be reserved for handle drag.

**Solution: Custom `awaitEachGesture` block with pointer count routing.**

```kotlin
.pointerInput(loopLength) {
    awaitEachGesture {
        val firstDown = awaitFirstDown(requireUnconsumed = false)
        firstDown.consume()

        // Wait briefly to see if a second finger arrives
        val secondPointer = withTimeoutOrNull(40L) {
            // Keep consuming firstDown's moves while waiting
            do {
                val event = awaitPointerEvent()
                event.changes.forEach { it.consume() }
                if (event.changes.size >= 2) return@withTimeoutOrNull event
            } while (event.changes.all { it.pressed })
            null
        }

        if (secondPointer != null && secondPointer.changes.size >= 2) {
            // === TWO-FINGER: Pinch-to-zoom and/or pan ===
            handlePinchAndPan(firstDown, secondPointer)
        } else {
            // === SINGLE-FINGER: Handle drag (existing logic) ===
            handleSingleFingerDrag(firstDown)
        }
    }
}
```

**Critical detail: the 40ms timeout.** This is the window to detect a second
finger. If a second finger arrives within 40ms of the first, it's a pinch
gesture. If not, it's a single-finger handle drag. 40ms is fast enough that
the user perceives no delay, but long enough to catch simultaneous two-finger
touches that land a few ms apart.

**Rejected alternative**: Using `detectTransformGestures` alongside
`pointerInput` for handle drag. Compose does not support layering multiple
gesture detectors on the same composable that both consume events cleanly.
A single `awaitEachGesture` with routing is the correct pattern.

### Pinch-and-Pan Handler (Pseudo-code)

```kotlin
private suspend fun AwaitPointerEventScope.handlePinchAndPan(
    firstDown: PointerInputChange,
    initialEvent: PointerEvent
) {
    var prevSpan = calculateSpan(initialEvent.changes)
    var prevCentroid = calculateCentroid(initialEvent.changes)

    do {
        val event = awaitPointerEvent()
        val activeChanges = event.changes.filter { it.pressed }
        if (activeChanges.size < 2) break

        val currentSpan = calculateSpan(activeChanges)
        val currentCentroid = calculateCentroid(activeChanges)

        // Zoom delta from span change
        if (prevSpan > 0f) {
            val zoomDelta = currentSpan / prevSpan
            applyZoom(currentCentroid.x, zoomDelta)
        }

        // Pan delta from centroid movement
        val panDeltaPx = currentCentroid.x - prevCentroid.x
        applyPan(panDeltaPx)

        prevSpan = currentSpan
        prevCentroid = currentCentroid
        activeChanges.forEach { it.consume() }
    } while (true)
}

private fun calculateSpan(changes: List<PointerInputChange>): Float {
    if (changes.size < 2) return 0f
    return abs(changes[0].position.x - changes[1].position.x)
}

private fun calculateCentroid(changes: List<PointerInputChange>): Offset {
    val x = changes.map { it.position.x }.average().toFloat()
    val y = changes.map { it.position.y }.average().toFloat()
    return Offset(x, y)
}
```

---

## Canvas Drawing Changes

### Overview Minimap Drawing

A new `OverviewMinimap` composable sits directly above the detail waveform.
Height: 40dp. Shares the same `waveformData` (which covers the full loop).

Drawing layers:
1. Dark background (DesignSystem.Background at 100% alpha).
2. Waveform bars spanning full loop (thin, muted green/white).
3. Semi-transparent dark overlay OUTSIDE the viewport rectangle.
4. Viewport rectangle outline (1.5dp stroke, DesignSystem.SafeGreen at 60%).
5. Trim handle position markers (thin vertical lines: green for start, amber
   for end).

The overview does NOT have its own trim handles. It only shows their positions
as indicators. All handle interaction happens in the detail view.

### Detail Waveform Drawing Changes

Currently the canvas renders `pairCount` bars where:
```
pairCount = waveformData.size / 2
barWidth = canvasWidth / pairCount
```

And bar `i` represents sample range `[i * samplesPerBar, (i+1) * samplesPerBar]`
across the full loop.

**With zoom, two approaches exist:**

#### Option 1: Re-fetch waveform data for visible range

Call `getWaveformSummary(numColumns=300, startSample=viewportStartSample,
endSample=viewportEndSample)` whenever the viewport changes. This gives
maximum resolution at any zoom level because the C++ side iterates only the
visible sample range and distributes it across all 300 columns.

- **Pro**: Perfect resolution at all zoom levels. At 16x zoom viewing 30,000
  samples across 300 columns = 100 samples/column (~2ms), versus 300
  columns at full loop = 1600 samples/column.
- **Con**: JNI call on every zoom/pan gesture frame could cause jank.
  `getWaveformSummary` iterates up to `numColumns * samplesPerColumn` samples.
  At 16x zoom, that is 300 * 100 = 30,000 iterations -- fast. At 1x, it is
  300 * 1600 = 480,000 -- still fast (sub-millisecond on any modern phone).
  The concern is JNI overhead of crossing the boundary each frame.

#### Option 2: Pre-fetch at multiple LODs (level-of-detail)

Pre-compute several resolutions of waveform summary (e.g., 300, 1200, 4800
columns) when the trim editor opens. Use the appropriate LOD based on zoom
level. No JNI calls during gesture.

- **Pro**: Zero runtime JNI cost. Instant rendering at any zoom.
- **Con**: More memory (4800 columns * 2 floats * 4 bytes = ~38KB, trivial).
  Initial load time slightly longer. Fixed resolution levels cannot match
  arbitrary zoom -- at between-LOD zoom levels, bars are either too coarse
  or sub-pixel.

#### Recommended: Option 1 with Debouncing

Re-fetch the waveform data for the visible range, but debounce the JNI call
to avoid calling it every frame during a pinch gesture:

1. During active pinch/pan, use the EXISTING full-loop waveformData and
   index into it to render the visible portion (fast, no JNI, but coarse
   resolution at high zoom -- acceptable for the ~200ms of active gesture).
2. When the gesture ENDS (finger up), fire a single debounced JNI call:
   `getWaveformSummary(300, viewportStartSample, viewportEndSample)`.
3. Store the result in a `zoomedWaveformData` state variable.
4. Canvas draws `zoomedWaveformData` when available, falls back to
   slicing the full-loop data when not.

This gives the best of both worlds: smooth 60fps during gesture (using
approximate data), and pixel-perfect resolution once the gesture settles.

**LooperDelegate changes:**
```kotlin
// New function in LooperDelegate:
fun refreshZoomedWaveformData(numColumns: Int, startSample: Int, endSample: Int) {
    val data = audioEngine.getWaveformSummary(numColumns, startSample, endSample)
    _zoomedWaveformData.value = data
}

// New state:
private val _zoomedWaveformData = MutableStateFlow<FloatArray?>(null)
val zoomedWaveformData: StateFlow<FloatArray?> = _zoomedWaveformData.asStateFlow()
```

### Fallback Rendering (During Gesture)

When rendering from the full-loop `waveformData` at zoom > 1x:

```kotlin
// Full-loop data has pairCount bars. Each bar represents:
val samplesPerBar = loopLength.toFloat() / pairCount

// Bars that fall within the viewport:
val firstBar = ((viewportStartSample / samplesPerBar).toInt()).coerceIn(0, pairCount - 1)
val lastBar = ((viewportEndSample / samplesPerBar).toInt()).coerceIn(0, pairCount - 1)

// Render only bars [firstBar, lastBar] stretched across full canvas width
for (i in firstBar..lastBar) {
    val barSampleStart = i * samplesPerBar
    val barSampleEnd = (i + 1) * samplesPerBar
    val barLeftPx = sampleToPixel(barSampleStart)  // uses zoomed formula
    val barRightPx = sampleToPixel(barSampleEnd)
    // ... draw rect from barLeftPx to barRightPx ...
}
```

This renders correctly but with lower resolution (bars get wider as you zoom
in, showing the same coarse min/max data stretched over more pixels). The
debounced high-res fetch replaces this within ~200ms of gesture end.

---

## Layout Structure

### Before (Current)

```
Column {
    TrimEditorHeader(...)
    Spacer(12dp)
    TrimWaveformCanvas(140dp)  // Full loop, no zoom
    Spacer(12dp)
    TrimTimeDisplay(...)
    Spacer(16dp)
    SnapModeSelector(...)
    Spacer(20dp)
    ApplyTrimButton(...)
}
```

### After (With Zoom)

```
Column {
    TrimEditorHeader(...)
    Spacer(8dp)
    ZoomLevelIndicator(...)              // "4.0x" text, tap to reset to 1x
    Spacer(4dp)
    OverviewMinimap(40dp)                // Always shows full loop + viewport rect
    Spacer(4dp)
    TrimWaveformCanvas(140dp)            // Zoomed detail view
    Spacer(8dp)
    TrimTimeDisplay(...)
    Spacer(12dp)
    SnapModeSelector(...)
    Spacer(16dp)
    ApplyTrimButton(...)
}
```

Total added vertical space: 40dp (minimap) + 4dp (spacing) + 4dp (spacing) +
~20dp (zoom indicator) = ~68dp. The panel was already generous with spacing;
reducing 3 spacers by 4dp each saves 12dp, net increase ~56dp. Acceptable --
the panel does not need to fit in one screen (it could scroll if needed, though
it currently fits on most phones).

### ZoomLevelIndicator

A small centered text showing the current zoom level, tappable to reset:

```
Row(center) {
    Text("4.0x", 11sp, Monospace, TextMuted)  // or "1.0x" at default
    if (zoomLevel > 1f) {
        Spacer(4dp)
        Text("TAP TO RESET", 9sp, TextMuted, clickable -> reset zoom to 1x)
    }
}
```

At 1x zoom, shows "1.0x" with no reset label (already at minimum).

---

## Overview Minimap Composable Spec

### OverviewMinimap Parameters

```kotlin
@Composable
private fun OverviewMinimap(
    waveformData: FloatArray?,
    loopLength: Int,
    viewportStartSample: Int,
    viewportEndSample: Int,
    trimStart: Int,
    trimEnd: Int,
    onViewportPan: (newCenterSample: Int) -> Unit,
    modifier: Modifier = Modifier
)
```

### Drawing Spec

1. **Background**: `DesignSystem.Background` full rect.

2. **Waveform bars**: Same algorithm as current `TrimWaveformCanvas` but:
   - Height scaled to 40dp.
   - Amplitude scaling: `centerY * 0.75f` (slightly less headroom since it
     is a compact view).
   - Bar color: `DesignSystem.TextMuted.copy(alpha = 0.4f)` for bars outside
     trim range, `DesignSystem.SafeGreen.copy(alpha = 0.4f)` for bars inside.

3. **Dark overlay outside viewport**: `DesignSystem.Background.copy(alpha = 0.6f)`
   over the left and right regions outside `[vpLeftPx, vpRightPx]`.

4. **Viewport rectangle**:
   - Stroke: `DesignSystem.SafeGreen.copy(alpha = 0.6f)`, 1.5dp width.
   - Fill: `DesignSystem.SafeGreen.copy(alpha = 0.05f)` (barely visible tint).
   - Corner radius: 2dp.

5. **Trim handle markers**: Vertical lines at trimStart and trimEnd pixel
   positions. 1dp stroke, same colors as detail handles (SafeGreen/WarningAmber)
   at 50% alpha.

### Minimap Gesture Handler

Single `pointerInput` block:
- On finger down: determine if inside viewport rect (drag to pan) or outside
  (tap to center viewport on touch position).
- Drag: convert pixel delta to sample delta, update `viewportStartSample`.
- Tap: center viewport on the tapped sample position.

```kotlin
.pointerInput(loopLength, viewportStartSample, viewportEndSample) {
    awaitEachGesture {
        val down = awaitFirstDown(requireUnconsumed = false)
        down.consume()
        val tapSample = (down.position.x / size.width.toFloat() * loopLength).roundToInt()

        // Check if touch is inside viewport rectangle
        val vpLeft = viewportStartSample.toFloat() / loopLength * size.width
        val vpRight = viewportEndSample.toFloat() / loopLength * size.width
        val isInsideViewport = down.position.x in vpLeft..vpRight

        if (isInsideViewport) {
            // Drag the viewport
            val grabOffset = down.position.x - (vpLeft + vpRight) / 2f
            do {
                val event = awaitPointerEvent()
                val change = event.changes.firstOrNull() ?: break
                if (change.pressed) {
                    change.consume()
                    val centerPx = change.position.x - grabOffset
                    val centerSample = (centerPx / size.width * loopLength).roundToInt()
                    onViewportPan(centerSample)
                } else break
            } while (true)
        } else {
            // Tap outside viewport -> center on tap
            onViewportPan(tapSample)
        }
    }
}
```

**NOTE on pointerInput keys**: The minimap's `pointerInput` uses
`viewportStartSample` and `viewportEndSample` as keys because the minimap is
NOT being actively dragged by the user (it receives updates from the detail
view's pinch/pan). This is different from the detail view where we learned
(documented in MEMORY.md) that rapidly-changing state as keys causes gesture
drops. For the minimap, the viewport changes infrequently (only on detail
view gesture end), so key-based restart is acceptable. However, if we later
find this causes issues, we should switch to `rememberUpdatedState` here too.

---

## Auto-Scroll During Handle Drag

When a trim handle is being dragged in the detail view and the finger approaches
the edge of the canvas:

```kotlin
// Inside the handle drag loop, after computing new samplePos:
if (zoomLevel > 1f) {
    val edgeZonePx = AUTO_SCROLL_EDGE_ZONE.toPx()
    val handlePx = sampleToPixel(samplePos)

    val scrollSpeed = when {
        handlePx < edgeZonePx -> {
            // Near left edge: scroll left (negative)
            -((edgeZonePx - handlePx) / edgeZonePx)
        }
        handlePx > canvasWidth - edgeZonePx -> {
            // Near right edge: scroll right (positive)
            ((handlePx - (canvasWidth - edgeZonePx)) / edgeZonePx)
        }
        else -> 0f
    }

    if (scrollSpeed != 0f) {
        val maxScrollSamples = (viewportSampleSpan * 0.05f).roundToInt() // 5% per frame
        val scrollDelta = (scrollSpeed * maxScrollSamples).roundToInt()
        viewportStartSample = (viewportStartSample + scrollDelta)
            .coerceIn(0, (loopLength - viewportSampleSpan).coerceAtLeast(0))
    }
}
```

---

## Performance Analysis

### JNI Call Cost
- `getWaveformSummary(300, start, end)` iterates `min(300 * samplesPerColumn,
  viewportSampleSpan)` samples. At 64x zoom on a 480,000-sample loop, the
  visible span is 7,500 samples. Iterating 7,500 floats with min/max is ~15us.
  With JNI overhead, ~50us total. Negligible.
- At 1x zoom, iterating 480,000 samples is ~1ms. Still fast, but we only call
  this on gesture end (debounced), not per-frame.

### Canvas Draw Cost
- Drawing 300 bars (rects + color selection) is the same cost as today.
  No regression.
- Overview minimap adds 300 more bars at 40dp height. Trivial.
- Total canvas draw time remains well under 2ms.

### Memory
- `waveformData` (full loop): 300 * 2 * 4 = 2,400 bytes.
- `zoomedWaveformData` (viewport): 300 * 2 * 4 = 2,400 bytes.
- Total additional: 2.4KB. Negligible.

### Frame Rate
- During pinch gesture: no JNI call, just re-rendering canvas with shifted
  viewport indices into existing data. Zero allocation, zero JNI. Solid 60fps.
- On gesture end: one async JNI call (~50us), state update triggers recompose.
  One extra frame to swap from coarse to fine data. Imperceptible.

---

## Edge Cases

### 1. Zoom causes handle to be off-screen
If the user zooms into a region between the two handles, both handles are
outside the viewport. The handle indicators (arrows at canvas edges) appear.
The user can:
- Pinch out to see both handles
- Drag the minimap viewport to navigate to a handle
- Tap a handle indicator to auto-scroll

### 2. Handle drag pushes viewport beyond loop boundary
`viewportStartSample` is always clamped to `[0, loopLength - viewportSampleSpan]`.
If auto-scroll tries to push past the boundary, it stops. The handle can still
be moved to sample 0 or loopLength because the handle position is clamped
independently of the viewport.

### 3. Loop length changes while zoomed (unlikely but possible)
If `loopLength` changes (e.g., undo crop), reset zoom to 1x and clear
`zoomedWaveformData`. The `pointerInput(loopLength)` key will restart the
gesture coroutine, which is correct behavior.

### 4. Very short loop (< 1000 samples)
At 1x zoom, 300 columns for 1000 samples = 3.3 samples per column. Already
high resolution. Max zoom is pointless but harmless (MIN_HANDLE_GAP_SAMPLES
prevents degenerate states). The minimap still renders correctly.

### 5. Pinch gesture starts on a handle
If one finger lands on a handle and a second finger arrives within 40ms, the
gesture is classified as pinch (not handle drag). This is correct: if the user
placed two fingers, they intended to zoom, not drag a handle.

### 6. Zoom level indicator text at extreme zoom
At 64x, text shows "64.0x". At fractional levels like 3.7x, shows one
decimal place. Format: `"%.1fx".format(zoomLevel)`.

---

## Accessibility

- Zoom level is announced via `semantics { stateDescription = "Zoom ${zoomLevel}x" }`.
- Overview minimap has `contentDescription = "Waveform overview showing viewport
  position"`.
- Handle off-screen indicators have `contentDescription = "Start handle off-screen
  to the left, tap to scroll"`.

---

## Visual Constants Summary

| Token                       | Value                                        |
|-----------------------------|----------------------------------------------|
| Overview bg                 | `DesignSystem.Background` (100%)             |
| Overview waveform (in trim) | `DesignSystem.SafeGreen` (40%)               |
| Overview waveform (outside) | `DesignSystem.TextMuted` (40%)               |
| Overview dark overlay       | `DesignSystem.Background` (60%)              |
| Viewport stroke             | `DesignSystem.SafeGreen` (60%), 1.5dp        |
| Viewport fill               | `DesignSystem.SafeGreen` (5%)                |
| Trim markers in overview    | SafeGreen / WarningAmber (50%), 1dp          |
| Zoom indicator text         | `DesignSystem.TextMuted`, 11sp, Monospace    |
| Reset label text            | `DesignSystem.TextMuted`, 9sp                |
| Off-screen handle indicator | SafeGreen / WarningAmber (80%), 8dp triangle |
| Auto-scroll edge zone       | 32dp                                         |

---

## Implementation Phases

### Phase 1: Core Zoom + Minimap (MVP)
1. Add `zoomLevel`, `viewportStartSample` state to `TrimWaveformCanvas`.
2. Implement coordinate conversion formulas.
3. Modify canvas drawing to render only visible range from full-loop data.
4. Add `OverviewMinimap` composable with viewport rectangle.
5. Wire minimap drag to update `viewportStartSample`.
6. Add zoom level indicator with tap-to-reset.
7. Implement two-finger pinch-to-zoom with anchor-point math.
8. Implement two-finger pan.

### Phase 2: High-Resolution Zoom Data
1. Add `refreshZoomedWaveformData(numColumns, start, end)` to LooperDelegate.
2. Add `zoomedWaveformData` StateFlow.
3. Debounce JNI call on gesture end.
4. Canvas switches to `zoomedWaveformData` when available.
5. Clear `zoomedWaveformData` on viewport change (until next debounced fetch).

### Phase 3: Polish
1. Auto-scroll during handle drag near edges.
2. Off-screen handle indicators with tap-to-navigate.
3. Smooth animated transitions when tapping minimap (animate viewport shift).
4. Accessibility semantics.

---

## Files Modified

| File | Change |
|------|--------|
| `LooperTrimEditor.kt` | Add zoom state, minimap composable, modify gesture routing, modify canvas draw, add zoom indicator |
| `LooperDelegate.kt` | Add `refreshZoomedWaveformData()`, `_zoomedWaveformData` StateFlow |
| `MainActivity.kt` | Collect and pass `zoomedWaveformData` to LooperTrimEditor |
| `PedalboardScreen.kt` | Pass `zoomedWaveformData` through to LooperTrimEditor |
| No C++ changes | `getWaveformSummary` already supports `startSample`/`endSample` |

---

## Self-Critique and Mitigations

### Critique 1: The 40ms two-finger detection timeout adds latency to handle drag
**Severity**: Low. 40ms is one frame at 24fps. At 60fps it is 2.4 frames. The
user will not perceive this delay because:
- The handle does not need to move in the first 40ms (the finger hasn't moved
  much yet, it is still within touch slop).
- Visual feedback (activeHandle state change, haptic) fires immediately on
  first touch, before the 40ms timeout. Only the drag tracking is delayed.

**Mitigation**: If testing reveals perceptible lag, reduce timeout to 20ms or
implement "optimistic single-finger" mode that starts handle drag immediately
and cancels it if a second finger arrives. This is more complex but eliminates
all latency.

### Critique 2: Minimap adds 40dp of vertical space to an already tall panel
**Severity**: Low-Medium. On a 5.5" phone (800dp viewport), the panel currently
uses ~460dp. Adding 56dp (net) brings it to ~516dp, still comfortably fitting
with room for the status bar. On very short phones (640dp), it might clip.

**Mitigation**: The minimap only renders when `zoomLevel > 1f`. At 1x zoom, the
minimap is not needed (the detail view shows everything) and can be hidden via
`AnimatedVisibility`. This recovers the 56dp at default zoom.

### Critique 3: Two-finger pan might conflict with system gestures on some Android launchers
**Severity**: Very low. Android's system gesture zones (edge swipe for back) are
at the screen edges, while our canvas is inset by 24dp horizontal padding. The
canvas touch area does not overlap with system gesture zones.

### Critique 4: `zoomedWaveformData` creates a second source of truth for waveform rendering
**Severity**: Low. The fallback path (rendering from `waveformData` with viewport
clipping) is always correct, just lower resolution. `zoomedWaveformData` is a
pure optimization. If it is stale or null, the canvas renders correctly from
the full-loop data. There is no data inconsistency risk.

### Critique 5: The minimap pointerInput uses viewport state as keys
**Severity**: Medium. As documented in MEMORY.md, rapidly-changing state as
`pointerInput` keys causes coroutine restarts that drop gestures. The minimap
viewport changes during detail view pinch/pan, which could restart the minimap's
gesture coroutine.

**Mitigation**: Use `rememberUpdatedState` for `viewportStartSample` and
`viewportEndSample` in the minimap's `pointerInput`, and key it only on
`loopLength`. This matches the pattern proven to work in the detail view's
handle drag. Updated spec for minimap gesture handler:

```kotlin
.pointerInput(loopLength) {
    awaitEachGesture {
        val currentVpStart by rememberUpdatedState(viewportStartSample)
        val currentVpEnd by rememberUpdatedState(viewportEndSample)
        // ... use currentVpStart/currentVpEnd inside gesture loop ...
    }
}
```

(Note: `rememberUpdatedState` must be declared at the composable level, not
inside the gesture lambda. The gesture lambda captures the State delegate.)
