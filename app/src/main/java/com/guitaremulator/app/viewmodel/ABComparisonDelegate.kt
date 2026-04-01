package com.guitaremulator.app.viewmodel

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * Delegate handling A/B comparison mode for the audio signal chain.
 *
 * A/B mode lets the user capture the current audio configuration as
 * "Snapshot A", make changes, then toggle between A and B to compare.
 * On exit, the user chooses which slot to keep.
 *
 * This delegate does NOT directly access ViewModel state. Instead, it
 * receives lambdas for capturing and applying snapshots, keeping it
 * decoupled from the ViewModel's internal mutable state.
 *
 * @property captureState Lambda that captures the current audio configuration
 *     as an immutable [AudioSnapshot]. Called when entering A/B mode and
 *     when switching slots (to persist changes to the active slot).
 * @property applyState Lambda that applies an [AudioSnapshot] to the engine
 *     and ViewModel state (gains, effects, bypass, processing order).
 */
class ABComparisonDelegate(
    private val captureState: () -> AudioSnapshot,
    private val applyState: (AudioSnapshot) -> Unit
) {

    // =========================================================================
    // Observable State
    // =========================================================================

    /** Whether A/B comparison mode is active. */
    private val _abMode = MutableStateFlow(false)
    val abMode: StateFlow<Boolean> = _abMode.asStateFlow()

    /** Which slot is currently live ('A' or 'B'). */
    private val _activeSlot = MutableStateFlow('A')
    val activeSlot: StateFlow<Char> = _activeSlot.asStateFlow()

    // =========================================================================
    // Internal State
    // =========================================================================

    /** Snapshot A (captured on enter, represents the "before" state). */
    private var snapshotA: AudioSnapshot? = null

    /** Snapshot B (initially a copy of A, modified by user changes). */
    private var snapshotB: AudioSnapshot? = null

    // =========================================================================
    // Public API
    // =========================================================================

    /**
     * Enter A/B comparison mode.
     *
     * Captures the current audio state as Snapshot A (the "before" state).
     * Snapshot B is initialized as a copy of A so the user starts from the
     * same point and can make changes to compare.
     */
    fun enterAbMode() {
        if (_abMode.value) return
        val snapshot = captureState()
        snapshotA = snapshot
        snapshotB = snapshot.copy()
        _activeSlot.value = 'A'
        _abMode.value = true
    }

    /**
     * Toggle between A and B slots.
     *
     * Before switching, captures the current state into the active slot's
     * snapshot (so any changes the user made are preserved), then applies
     * the other slot's snapshot to the engine.
     */
    fun toggleAB() {
        if (!_abMode.value) return

        // Save current state into the active slot
        if (_activeSlot.value == 'A') {
            snapshotA = captureState()
            _activeSlot.value = 'B'
            snapshotB?.let { applyState(it) }
        } else {
            snapshotB = captureState()
            _activeSlot.value = 'A'
            snapshotA?.let { applyState(it) }
        }
    }

    /**
     * Exit A/B mode and keep the chosen slot's settings.
     *
     * @param keep 'A' to keep slot A's settings, 'B' to keep slot B's settings.
     */
    fun exitAbMode(keep: Char) {
        if (!_abMode.value) return

        // Save the current active slot before deciding which to keep
        if (_activeSlot.value == 'A') {
            snapshotA = captureState()
        } else {
            snapshotB = captureState()
        }

        // Apply the chosen slot's snapshot
        val chosen = if (keep == 'A') snapshotA else snapshotB
        chosen?.let { applyState(it) }

        // Clean up A/B state
        snapshotA = null
        snapshotB = null
        _activeSlot.value = 'A'
        _abMode.value = false
    }
}
