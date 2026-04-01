# Re-Amp Preset Restore -- Design Document

## Problem Statement

When the user opens a saved loop session and taps "Try Different Tones" (re-amp),
the dry recording immediately plays through WHATEVER preset/effects chain is
currently active on the pedalboard. This can be completely different from what the
loop was recorded with, potentially causing extremely loud or harsh output.

The correct behavior:
1. Load the SAVED effect chain (the one active when the loop was recorded) first
2. THEN start the dry playback through that restored chain
3. The user hears their loop as it originally sounded
4. Only when the user explicitly changes effects/knobs/presets should the tone change

## Root Cause Analysis

There are TWO independent bugs:

### Bug 1: Save Side -- Preset metadata never passed

In `MainActivity.kt` line 432-434:
```kotlin
onLooperSaveSession = { name ->
    audioViewModel.looperDelegate.exportAndSaveSession(name)
},
```

`exportAndSaveSession` accepts optional `presetId` and `presetName` parameters,
but the call site passes only the session name. The preset metadata defaults to
`null` and is never saved.

### Bug 2: Load Side -- No preset restoration before re-amp

In `LooperDelegate.kt` line 677-688, `startReamp(session)` does:
```kotlin
fun startReamp(session: LooperSession) {
    ensureInitialized()
    val loaded = audioEngine.loadDryForReamp(session.dryFilePath)
    audioEngine.startReamp()
    _isReamping.value = true
}
```

There is zero preset restoration. The dry recording plays through the current
chain unconditionally.

## Design Decision: Why Full Chain Snapshot (Not Just Preset ID)

Three approaches were evaluated:

| Approach | Correctness | Robustness | Complexity |
|----------|-------------|------------|------------|
| A: Preset ID only | Fails if preset modified/deleted/never loaded | Low | Low |
| B: Full chain snapshot only | Always correct | High | Medium |
| C: Snapshot + ID (with fallback) | Always correct + user-friendly display | High | Medium |

**Selected: Approach C** -- Store a full effect chain snapshot AND the preset
ID/name. Restoration uses the snapshot (always correct). Preset name provides
display context in the session library UI.

Scenarios where preset ID alone fails:
- User modifies preset after recording (overwrite or knob tweaks without saving)
- User deletes the preset
- User was tweaking knobs without any preset loaded (preset ID is empty)
- Factory preset gets updated in an app version

The snapshot captures the EXACT state at recording time, immune to all of these.

### Known Limitation

Custom cabinet IRs and neural amp model files are loaded from file paths. If the
user deletes those files after recording, the snapshot can restore all parameters
but cannot restore the missing file data. The effect will fall back to its default
cabinet/model. This is an acceptable edge case -- the session still plays with
reasonable defaults.

## Implementation Plan

### 1. LooperSession.kt -- Add effectChainSnapshot field

Add a new nullable property to the data class:

```kotlin
data class LooperSession(
    // ... existing properties ...
    val effectChainSnapshot: String? = null  // NEW: Full chain state as JSON string
)
```

Update `toJson()`:
```kotlin
if (effectChainSnapshot != null) json.put("effectChainSnapshot", effectChainSnapshot)
```

Update `fromJson()`:
```kotlin
effectChainSnapshot = json.optString("effectChainSnapshot", null)
```

**Backward compatibility**: Old sessions without this field get `null` from
`optString`. Restoration falls through to preset ID lookup or current chain.
New sessions on an old app version: the `effectChainSnapshot` key is ignored
by `optString` -- harmlessly skipped.

**Storage impact**: ~8-15 KB JSON per session. Session WAV files are ~45 MB.
Overhead < 0.05%. Negligible.

### 2. LooperDelegate.kt -- Accept snapshot in exportAndSaveSession

Update the method signature to accept the snapshot:

```kotlin
fun exportAndSaveSession(
    name: String,
    presetId: String? = null,
    presetName: String? = null,
    effectChainSnapshot: String? = null  // NEW
) {
    // ... existing validation ...

    val session = LooperSession(
        // ... existing fields ...
        effectChainSnapshot = effectChainSnapshot,  // NEW: pass through
        // ... rest unchanged ...
    )
    // ... rest of method unchanged ...
}
```

No other changes to LooperDelegate. The `startReamp` method stays as-is because
preset restoration is handled at the AudioViewModel level (see below).

### 3. PresetManager.kt -- Add snapshot deserialization helper

```kotlin
/**
 * Deserialize an effect chain snapshot JSON string into a Preset.
 *
 * Unlike deserializePreset (used for file import), this does NOT assign
 * a new UUID or modify the preset -- it restores exactly what was saved.
 * Used by the re-amp feature to restore the exact effect chain state.
 *
 * @param snapshotJson The JSON string from serializePreset.
 * @return The deserialized Preset or null if malformed.
 */
fun deserializeSnapshotPreset(snapshotJson: String): Preset? {
    return try {
        val json = JSONObject(snapshotJson)
        jsonToPreset(json)
    } catch (e: Exception) {
        Log.e(TAG, "Failed to deserialize chain snapshot", e)
        null
    }
}
```

### 4. AudioViewModel.kt -- Two new coordinating methods

#### 4a. Save with snapshot capture

```kotlin
/**
 * Save a looper session with a complete effect chain snapshot.
 *
 * Captures the current signal chain state (all 26 effects, gains, order)
 * as a JSON string embedded in the session metadata. This enables exact
 * restoration during re-amp playback regardless of preset modifications.
 *
 * @param name User-facing display name for the session.
 */
fun saveLooperSessionWithSnapshot(name: String) {
    val snapshotPreset = Preset(
        name = "snapshot",
        effects = _effectStates.value.map { uiState ->
            EffectState(
                effectType = uiState.type,
                enabled = uiState.enabled,
                wetDryMix = uiState.wetDryMix,
                parameters = uiState.parameters
            )
        },
        inputGain = _inputGain.value,
        outputGain = _outputGain.value,
        effectOrder = _effectOrder.value.let { order ->
            val identity = (0 until EffectRegistry.EFFECT_COUNT).toList()
            if (order != identity) order else null
        }
    )
    val snapshotJson = presetManager.serializePreset(snapshotPreset)

    looperDelegate.exportAndSaveSession(
        name = name,
        presetId = presetDelegate.currentPresetId.value.ifEmpty { null },
        presetName = presetDelegate.currentPresetName.value.ifEmpty { null },
        effectChainSnapshot = snapshotJson
    )
}
```

#### 4b. Re-amp with preset restoration

```kotlin
/**
 * Start re-amping a saved session, restoring the recorded effect chain first.
 *
 * Restoration priority:
 *   1. Full effect chain snapshot (always correct)
 *   2. Preset ID lookup (fallback for legacy sessions)
 *   3. Current chain (if both are missing)
 *
 * @param session The session to re-amp.
 */
fun startReampWithPresetRestore(session: LooperSession) {
    var restored = false

    // Priority 1: Full chain snapshot
    if (!restored && session.effectChainSnapshot != null) {
        try {
            val preset = presetManager.deserializeSnapshotPreset(session.effectChainSnapshot)
            if (preset != null) {
                applyRestoredPreset(preset)
                restored = true
            }
        } catch (e: Exception) {
            android.util.Log.w("AudioViewModel",
                "Failed to restore chain snapshot, trying preset ID", e)
        }
    }

    // Priority 2: Preset ID lookup (fallback for legacy sessions)
    if (!restored && session.presetId != null) {
        val preset = presetManager.loadPreset(session.presetId)
        if (preset != null) {
            applyRestoredPreset(preset)
            restored = true
        } else {
            android.util.Log.w("AudioViewModel",
                "Saved preset not found: ${session.presetId}, using current chain")
        }
    }

    // Update preset display in UI
    if (restored) {
        presetDelegate.setCurrentPreset(
            name = session.presetName ?: "Recorded Settings",
            id = session.presetId ?: ""
        )
    }
    // Priority 3: no restoration possible -- use current chain silently

    // Now start re-amp playback
    looperDelegate.startReamp(session)
}

/**
 * Apply a restored preset to both the native engine and UI state.
 * Shared helper for snapshot and preset-ID restoration paths.
 */
private fun applyRestoredPreset(preset: Preset) {
    if (_isRunning.value) {
        presetManager.applyPreset(preset, audioEngine)
    }
    updateEffectStatesFromPreset(preset)
    _inputGain.value = preset.inputGain
    _outputGain.value = preset.outputGain
    if (_isRunning.value) {
        audioEngine.setInputGain(preset.inputGain)
        audioEngine.setOutputGain(preset.outputGain)
    }
}
```

Note: `presetDelegate` is a private field on AudioViewModel. `setCurrentPreset`
is `internal` visibility (same Kotlin module). Both are accessible.

### 5. MainActivity.kt -- Rewire callbacks

#### Save callback (line 432-434):

FROM:
```kotlin
onLooperSaveSession = { name ->
    audioViewModel.looperDelegate.exportAndSaveSession(name)
},
```

TO:
```kotlin
onLooperSaveSession = { name ->
    audioViewModel.saveLooperSessionWithSnapshot(name)
},
```

#### Re-amp callback (line 443):

FROM:
```kotlin
onReampSession = { session -> audioViewModel.looperDelegate.startReamp(session) },
```

TO:
```kotlin
onReampSession = { session -> audioViewModel.startReampWithPresetRestore(session) },
```

### 6. SessionLibrary.kt -- No changes needed

The `onReamp: (LooperSession) -> Unit` callback signature is already correct.
The `LooperSession` object passed through contains whatever metadata was saved,
including the new `effectChainSnapshot` field. No UI changes required.

## Sequence Diagram

```
User taps "TRY DIFFERENT TONES" on session card
  |
  v
MainActivity.onReampSession(session)
  |
  v
AudioViewModel.startReampWithPresetRestore(session)
  |
  +-- 1. session.effectChainSnapshot != null ?
  |     YES --> deserializeSnapshotPreset(json)
  |             --> applyRestoredPreset(preset)
  |                 --> presetManager.applyPreset() [native engine]
  |                 --> updateEffectStatesFromPreset() [UI state]
  |                 --> set input/output gains
  |             --> restored = true
  |     NO  --> fall through
  |
  +-- 2. session.presetId != null ?
  |     YES --> presetManager.loadPreset(id)
  |             --> if found: applyRestoredPreset() + restored = true
  |             --> if missing: log warning, fall through
  |     NO  --> fall through
  |
  +-- 3. Update preset display (if restored)
  |       presetDelegate.setCurrentPreset(name, id)
  |
  +-- 4. looperDelegate.startReamp(session)
  |       --> loadDryForReamp(dryFilePath)
  |       --> audioEngine.startReamp()
  |       --> _isReamping = true
  |
  v
User hears dry recording through RESTORED effect chain
User can now tweak knobs/presets to explore different tones
```

## Error Handling Matrix

| Scenario | Behavior |
|----------|----------|
| Snapshot valid | Exact chain restored, re-amp starts |
| Snapshot corrupt | Fall through to preset ID |
| Preset ID valid, file exists | Preset restored, re-amp starts |
| Preset ID valid, file deleted | Log warning, current chain used |
| No snapshot, no preset ID (legacy session) | Current chain used silently |
| Dry WAV missing | startReamp logs error, returns without playing |
| Engine not running | Preset applied to UI state only (will sync on next start) |

## Testing Checklist

1. Save session WITH a preset loaded --> verify effectChainSnapshot is non-null in metadata.json
2. Save session WITHOUT a preset (knobs tweaked manually) --> verify snapshot captured, presetId is null
3. Re-amp a new session --> verify chain restores exactly (check all 26 effect states)
4. Re-amp a legacy session (no snapshot field) --> verify falls back to preset ID or current chain
5. Re-amp a session whose preset was deleted --> verify fallback to current chain with log warning
6. Re-amp after modifying the preset --> verify snapshot restores the ORIGINAL state (not modified)
7. Verify input/output gains restore correctly
8. Verify effect ordering restores correctly
9. Verify session library UI still shows preset name correctly

## Files Modified Summary

| File | Change | ~Lines |
|------|--------|--------|
| LooperSession.kt | +effectChainSnapshot property, +toJson/fromJson | +6 |
| LooperDelegate.kt | +effectChainSnapshot param on exportAndSaveSession | +3 |
| PresetManager.kt | +deserializeSnapshotPreset() method | +12 |
| AudioViewModel.kt | +saveLooperSessionWithSnapshot(), +startReampWithPresetRestore(), +applyRestoredPreset() | +55 |
| MainActivity.kt | Rewire 2 callbacks | +4 (net) |
| **Total** | | **~80 lines** |

No new files. No new dependencies. No JNI changes. No C++ changes.
