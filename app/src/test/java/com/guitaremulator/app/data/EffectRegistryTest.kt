package com.guitaremulator.app.data

import org.junit.Assert.*
import org.junit.Test

/**
 * Unit tests for [EffectRegistry].
 *
 * Validates structural integrity of the effect registry: correct indices,
 * unique types, sensible parameter ranges, and lookup functions.
 */
class EffectRegistryTest {

    // =========================================================================
    // Structural integrity
    // =========================================================================

    @Test
    fun `registry has exactly 32 effects`() {
        assertEquals(EffectRegistry.EFFECT_COUNT, EffectRegistry.effects.size)
        assertEquals(32, EffectRegistry.EFFECT_COUNT)
    }

    @Test
    fun `effect indices are sequential 0 through 31`() {
        for ((i, info) in EffectRegistry.effects.withIndex()) {
            assertEquals("Effect at position $i should have index $i", i, info.index)
        }
    }

    @Test
    fun `all effect types are unique`() {
        val types = EffectRegistry.effects.map { it.type }
        assertEquals(
            "Duplicate type strings found: ${types.groupBy { it }.filter { it.value.size > 1 }.keys}",
            types.size,
            types.toSet().size
        )
    }

    @Test
    fun `all effect display names are unique`() {
        val names = EffectRegistry.effects.map { it.displayName }
        assertEquals(
            "Duplicate display names: ${names.groupBy { it }.filter { it.value.size > 1 }.keys}",
            names.size,
            names.toSet().size
        )
    }

    @Test
    fun `no effect type is empty`() {
        for (info in EffectRegistry.effects) {
            assertTrue("Effect ${info.index} has empty type", info.type.isNotEmpty())
            assertTrue("Effect ${info.index} has empty displayName", info.displayName.isNotEmpty())
        }
    }

    // =========================================================================
    // Parameter validation
    // =========================================================================

    @Test
    fun `all parameter IDs within an effect are unique`() {
        for (info in EffectRegistry.effects) {
            val ids = info.params.map { it.id }
            assertEquals(
                "${info.type}: duplicate param IDs ${ids.groupBy { it }.filter { it.value.size > 1 }.keys}",
                ids.size,
                ids.toSet().size
            )
        }
    }

    @Test
    fun `all parameter min is less than or equal to max`() {
        for (info in EffectRegistry.effects) {
            for (param in info.params) {
                assertTrue(
                    "${info.type}.${param.name}: min (${param.min}) > max (${param.max})",
                    param.min <= param.max
                )
            }
        }
    }

    @Test
    fun `all parameter defaults are within min-max range`() {
        for (info in EffectRegistry.effects) {
            // Skip read-only effects (Tuner) — their defaults represent
            // "no signal" state (e.g., 0 Hz detected) which is intentionally
            // below the parameter's displayable minimum.
            if (info.isReadOnly) continue

            for (param in info.params) {
                assertTrue(
                    "${info.type}.${param.name}: default (${param.defaultValue}) < min (${param.min})",
                    param.defaultValue >= param.min
                )
                assertTrue(
                    "${info.type}.${param.name}: default (${param.defaultValue}) > max (${param.max})",
                    param.defaultValue <= param.max
                )
            }
        }
    }

    @Test
    fun `no parameter has NaN or Infinity in range`() {
        for (info in EffectRegistry.effects) {
            for (param in info.params) {
                assertFalse("${info.type}.${param.name}: min is NaN", param.min.isNaN())
                assertFalse("${info.type}.${param.name}: max is NaN", param.max.isNaN())
                assertFalse("${info.type}.${param.name}: default is NaN", param.defaultValue.isNaN())
                assertFalse("${info.type}.${param.name}: min is Infinite", param.min.isInfinite())
                assertFalse("${info.type}.${param.name}: max is Infinite", param.max.isInfinite())
                assertFalse("${info.type}.${param.name}: default is Infinite", param.defaultValue.isInfinite())
            }
        }
    }

    // =========================================================================
    // Known effect type strings match signal chain index reference
    // =========================================================================

    @Test
    fun `signal chain order matches expected types`() {
        val expected = listOf(
            "NoiseGate", "Compressor", "Wah", "Boost", "Overdrive",
            "AmpModel", "CabinetSim", "ParametricEQ", "Chorus", "Vibrato",
            "Phaser", "Flanger", "Delay", "Reverb", "Tuner", "Tremolo",
            "BossDS1", "ProCoRAT", "BossDS2", "BossHM2", "Univibe",
            "FuzzFace", "Rangemaster",
            "BigMuff", "Octavia",
            "RotarySpeaker", "Fuzzrite", "MF102RingMod"
        )
        for ((i, type) in expected.withIndex()) {
            assertEquals("Index $i should be $type", type, EffectRegistry.effects[i].type)
        }
    }

    // =========================================================================
    // Lookup functions
    // =========================================================================

    @Test
    fun `getEffectInfo returns correct info for valid index`() {
        val info = EffectRegistry.getEffectInfo(0)
        assertNotNull(info)
        assertEquals("NoiseGate", info!!.type)
    }

    @Test
    fun `getEffectInfo returns null for negative index`() {
        assertNull(EffectRegistry.getEffectInfo(-1))
    }

    @Test
    fun `getEffectInfo returns null for out-of-bounds index`() {
        assertNull(EffectRegistry.getEffectInfo(EffectRegistry.EFFECT_COUNT))
        assertNull(EffectRegistry.getEffectInfo(100))
    }

    @Test
    fun `getIndexForType returns correct index for known type`() {
        assertEquals(0, EffectRegistry.getIndexForType("NoiseGate"))
        assertEquals(4, EffectRegistry.getIndexForType("Overdrive"))
        assertEquals(13, EffectRegistry.getIndexForType("Reverb"))
        assertEquals(20, EffectRegistry.getIndexForType("Univibe"))
    }

    @Test
    fun `getIndexForType returns -1 for unknown type`() {
        assertEquals(-1, EffectRegistry.getIndexForType("NonExistent"))
        assertEquals(-1, EffectRegistry.getIndexForType(""))
    }

    @Test
    fun `getIndexForType is case-sensitive`() {
        assertEquals(-1, EffectRegistry.getIndexForType("noisegate"))
        assertEquals(-1, EffectRegistry.getIndexForType("NOISEGATE"))
    }

    // =========================================================================
    // Specific effect properties
    // =========================================================================

    @Test
    fun `Tuner is read-only`() {
        val tuner = EffectRegistry.getEffectInfo(14)
        assertNotNull(tuner)
        assertTrue(tuner!!.isReadOnly)
    }

    @Test
    fun `only Tuner is read-only`() {
        for (info in EffectRegistry.effects) {
            if (info.type == "Tuner") {
                assertTrue(info.isReadOnly)
            } else {
                assertFalse("${info.type} should not be read-only", info.isReadOnly)
            }
        }
    }

    @Test
    fun `all pedal effects are disabled by default`() {
        val pedals = listOf(16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27) // DS1, RAT, DS2, HM2, Univibe, FuzzFace, Rangemaster, BigMuff, Octavia, RotarySpeaker, Fuzzrite, MF102RingMod
        for (idx in pedals) {
            val info = EffectRegistry.getEffectInfo(idx)
            assertNotNull(info)
            assertFalse("${info!!.type} should be disabled by default", info.enabledByDefault)
        }
    }

    @Test
    fun `core effects are enabled by default`() {
        val core = listOf(0, 1, 4, 7, 8, 12, 13) // Gate, Comp, OD, EQ, Chorus, Delay, Reverb
        for (idx in core) {
            val info = EffectRegistry.getEffectInfo(idx)
            assertNotNull(info)
            assertTrue("${info!!.type} should be enabled by default", info.enabledByDefault)
        }
    }

    // =========================================================================
    // Parameter count sanity checks
    // =========================================================================

    @Test
    fun `NoiseGate has 4 parameters`() {
        assertEquals(4, EffectRegistry.getEffectInfo(0)!!.params.size)
    }

    @Test
    fun `Compressor has 7 parameters`() {
        assertEquals(7, EffectRegistry.getEffectInfo(1)!!.params.size)
    }

    @Test
    fun `Overdrive has 3 parameters`() {
        assertEquals(3, EffectRegistry.getEffectInfo(4)!!.params.size)
    }

    @Test
    fun `Reverb has 5 parameters`() {
        assertEquals(5, EffectRegistry.getEffectInfo(13)!!.params.size)
    }

    @Test
    fun `Tuner has 4 read-only parameters`() {
        val tuner = EffectRegistry.getEffectInfo(14)!!
        assertEquals(4, tuner.params.size)
        assertTrue(tuner.isReadOnly)
    }

    @Test
    fun `RotarySpeaker has 3 parameters`() {
        val rotary = EffectRegistry.getEffectInfo(25)!!
        assertEquals("RotarySpeaker", rotary.type)
        assertEquals(3, rotary.params.size)
    }

    // =========================================================================
    // Permutation validation helpers (mirrors Kotlin-side validation in PresetManager)
    // =========================================================================

    private fun isValidPermutation(order: List<Int>, effectCount: Int): Boolean {
        return order.size == effectCount &&
               order.all { it in 0 until effectCount } &&
               order.toSet().size == order.size
    }

    @Test
    fun `valid identity permutation`() {
        val n = EffectRegistry.EFFECT_COUNT
        val identity = (0 until n).toList()
        assertTrue(isValidPermutation(identity, n))
    }

    @Test
    fun `valid reversed permutation`() {
        val n = EffectRegistry.EFFECT_COUNT
        val reversed = (n - 1 downTo 0).toList()
        assertTrue(isValidPermutation(reversed, n))
    }

    @Test
    fun `invalid permutation with duplicates`() {
        val n = EffectRegistry.EFFECT_COUNT
        val dups = listOf(0, 0) + (2 until n).toList()
        assertFalse(isValidPermutation(dups, n))
    }

    @Test
    fun `invalid permutation with out of range index`() {
        val n = EffectRegistry.EFFECT_COUNT
        val outOfRange = (0 until n - 1).toList() + listOf(n)
        assertFalse(isValidPermutation(outOfRange, n))
    }

    @Test
    fun `invalid permutation with negative index`() {
        val n = EffectRegistry.EFFECT_COUNT
        val neg = listOf(-1) + (1 until n).toList()
        assertFalse(isValidPermutation(neg, n))
    }

    @Test
    fun `invalid permutation wrong size too few`() {
        val n = EffectRegistry.EFFECT_COUNT
        val tooFew = (0 until n - 1).toList()
        assertFalse(isValidPermutation(tooFew, n))
    }

    @Test
    fun `invalid permutation wrong size too many`() {
        val n = EffectRegistry.EFFECT_COUNT
        val tooMany = (0 until n + 1).toList()
        assertFalse(isValidPermutation(tooMany, n))
    }

    @Test
    fun `empty permutation for zero effects`() {
        assertTrue(isValidPermutation(emptyList(), 0))
    }
}
