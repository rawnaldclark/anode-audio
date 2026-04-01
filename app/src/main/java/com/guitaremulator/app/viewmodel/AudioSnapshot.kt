package com.guitaremulator.app.viewmodel

import com.guitaremulator.app.data.EffectState

/**
 * Immutable snapshot of the current audio configuration for A/B comparison.
 *
 * Captures all tone-defining state: effect parameters, processing order,
 * master gains, and bypass. Two snapshots can be compared by instantly
 * toggling between them in the ViewModel.
 *
 * @property effectStates Complete state of all 21 effects in the signal chain.
 * @property effectOrder Processing order (null = identity/default order).
 * @property inputGain Master input gain (0.0..4.0).
 * @property outputGain Master output gain (0.0..4.0).
 * @property globalBypass Whether global bypass was active.
 */
data class AudioSnapshot(
    val effectStates: List<EffectState>,
    val effectOrder: List<Int>?,
    val inputGain: Float,
    val outputGain: Float,
    val globalBypass: Boolean
)
