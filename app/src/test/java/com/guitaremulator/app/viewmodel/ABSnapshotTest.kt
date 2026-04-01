package com.guitaremulator.app.viewmodel

import com.guitaremulator.app.data.EffectState
import org.junit.Assert.*
import org.junit.Test

/**
 * Unit tests for [AudioSnapshot] data class semantics.
 *
 * Validates structural equality, copy behavior, independence after copy,
 * and nullable field handling for A/B comparison snapshots.
 */
class ABSnapshotTest {

    // =========================================================================
    // Helpers
    // =========================================================================

    /** Build a representative snapshot with two effects for reuse across tests. */
    private fun buildSnapshot(
        inputGain: Float = 1.0f,
        outputGain: Float = 1.0f,
        globalBypass: Boolean = false,
        effectOrder: List<Int>? = null,
        effects: List<EffectState> = listOf(
            EffectState("NoiseGate", enabled = true, wetDryMix = 1.0f, parameters = mapOf(0 to -40f)),
            EffectState("Overdrive", enabled = false, wetDryMix = 0.5f, parameters = mapOf(0 to 0.3f, 1 to 0.7f))
        )
    ): AudioSnapshot = AudioSnapshot(
        effectStates = effects,
        effectOrder = effectOrder,
        inputGain = inputGain,
        outputGain = outputGain,
        globalBypass = globalBypass
    )

    // =========================================================================
    // Equality tests
    // =========================================================================

    @Test
    fun `snapshot data class equality`() {
        val a = buildSnapshot()
        val b = buildSnapshot()
        assertEquals(a, b)
        assertEquals(a.hashCode(), b.hashCode())
    }

    @Test
    fun `snapshot data class inequality on inputGain`() {
        val a = buildSnapshot(inputGain = 1.0f)
        val b = buildSnapshot(inputGain = 2.0f)
        assertNotEquals(a, b)
    }

    @Test
    fun `snapshot data class inequality on effects`() {
        val a = buildSnapshot(effects = listOf(
            EffectState("NoiseGate", enabled = true)
        ))
        val b = buildSnapshot(effects = listOf(
            EffectState("Overdrive", enabled = false)
        ))
        assertNotEquals(a, b)
    }

    // =========================================================================
    // Copy tests
    // =========================================================================

    @Test
    fun `snapshot copy preserves all fields`() {
        val original = buildSnapshot(
            inputGain = 1.5f,
            outputGain = 0.8f,
            globalBypass = true,
            effectOrder = listOf(1, 0)
        )
        val copied = original.copy()
        assertEquals(original, copied)
    }

    @Test
    fun `snapshot copy can override inputGain`() {
        val original = buildSnapshot(inputGain = 1.0f, outputGain = 0.8f, globalBypass = false)
        val modified = original.copy(inputGain = 2.0f)

        assertEquals(2.0f, modified.inputGain, 0.001f)
        // All other fields remain unchanged
        assertEquals(original.outputGain, modified.outputGain, 0.001f)
        assertEquals(original.globalBypass, modified.globalBypass)
        assertEquals(original.effectStates, modified.effectStates)
        assertEquals(original.effectOrder, modified.effectOrder)
    }

    @Test
    fun `snapshot copy can override outputGain`() {
        val original = buildSnapshot(outputGain = 1.0f)
        val modified = original.copy(outputGain = 3.5f)

        assertEquals(3.5f, modified.outputGain, 0.001f)
        assertEquals(original.inputGain, modified.inputGain, 0.001f)
        assertEquals(original.globalBypass, modified.globalBypass)
        assertEquals(original.effectStates, modified.effectStates)
    }

    @Test
    fun `snapshot copy can override globalBypass`() {
        val original = buildSnapshot(globalBypass = false)
        val modified = original.copy(globalBypass = true)

        assertTrue(modified.globalBypass)
        assertFalse(original.globalBypass)
        assertEquals(original.inputGain, modified.inputGain, 0.001f)
        assertEquals(original.outputGain, modified.outputGain, 0.001f)
        assertEquals(original.effectStates, modified.effectStates)
    }

    // =========================================================================
    // Nullable effectOrder
    // =========================================================================

    @Test
    fun `snapshot with null effectOrder`() {
        val snapshot = buildSnapshot(effectOrder = null)
        assertNull(snapshot.effectOrder)
    }

    @Test
    fun `snapshot with non-null effectOrder`() {
        val order = listOf(4, 3, 2, 1, 0)
        val snapshot = buildSnapshot(effectOrder = order)
        assertNotNull(snapshot.effectOrder)
        assertEquals(order, snapshot.effectOrder)
    }

    // =========================================================================
    // Independence after copy
    // =========================================================================

    @Test
    fun `snapshots are independent after copy`() {
        val effectList = mutableListOf(
            EffectState("NoiseGate", enabled = true),
            EffectState("Overdrive", enabled = false)
        )
        val orderList = mutableListOf(0, 1)

        val original = AudioSnapshot(
            effectStates = effectList.toList(),
            effectOrder = orderList.toList(),
            inputGain = 1.0f,
            outputGain = 1.0f,
            globalBypass = false
        )

        val copied = original.copy()

        // Mutating the original mutable source lists does NOT affect either
        // snapshot, because toList() created defensive copies.
        effectList.add(EffectState("Reverb", enabled = true))
        orderList.add(2)

        // Both snapshots retain their original size
        assertEquals(2, original.effectStates.size)
        assertEquals(2, copied.effectStates.size)
        assertEquals(2, original.effectOrder!!.size)
        assertEquals(2, copied.effectOrder!!.size)

        // The two snapshots are still equal to each other
        assertEquals(original, copied)
    }
}
