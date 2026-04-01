package com.guitaremulator.app.data

import org.junit.Assert.*
import org.junit.Test
import kotlin.math.roundToInt

/**
 * Fuzz-style unit tests for [EffectRegistry] parameter definitions.
 *
 * Validates that every effect's parameters have consistent ranges, sequential
 * IDs, valid defaults, and correct enum labels. These tests catch configuration
 * errors that would cause sliders to misbehave, NaN propagation, or enum
 * display mismatches at runtime.
 */
class EffectParameterFuzzTest {

    // =========================================================================
    // Per-effect parameter range validation (min <= default <= max)
    // =========================================================================

    @Test
    fun `NoiseGate params in range`() = assertParamsInRange("NoiseGate")

    @Test
    fun `Compressor params in range`() = assertParamsInRange("Compressor")

    @Test
    fun `Wah params in range`() = assertParamsInRange("Wah")

    @Test
    fun `Boost params in range`() = assertParamsInRange("Boost")

    @Test
    fun `Overdrive params in range`() = assertParamsInRange("Overdrive")

    @Test
    fun `AmpModel params in range`() = assertParamsInRange("AmpModel")

    @Test
    fun `CabinetSim params in range`() = assertParamsInRange("CabinetSim")

    @Test
    fun `ParametricEQ params in range`() = assertParamsInRange("ParametricEQ")

    @Test
    fun `Chorus params in range`() = assertParamsInRange("Chorus")

    @Test
    fun `Vibrato params in range`() = assertParamsInRange("Vibrato")

    @Test
    fun `Phaser params in range`() = assertParamsInRange("Phaser")

    @Test
    fun `Flanger params in range`() = assertParamsInRange("Flanger")

    @Test
    fun `Delay params in range`() = assertParamsInRange("Delay")

    @Test
    fun `Reverb params in range`() = assertParamsInRange("Reverb")

    @Test
    fun `Tremolo params in range`() = assertParamsInRange("Tremolo")

    @Test
    fun `BossDS1 params in range`() = assertParamsInRange("BossDS1")

    @Test
    fun `ProCoRAT params in range`() = assertParamsInRange("ProCoRAT")

    @Test
    fun `BossDS2 params in range`() = assertParamsInRange("BossDS2")

    @Test
    fun `BossHM2 params in range`() = assertParamsInRange("BossHM2")

    @Test
    fun `Univibe params in range`() = assertParamsInRange("Univibe")

    // =========================================================================
    // No degenerate parameter ranges
    // =========================================================================

    @Test
    fun `no parameter has zero-width range`() {
        for (info in EffectRegistry.effects) {
            if (info.isReadOnly) continue
            for (param in info.params) {
                assertTrue(
                    "${info.type}.${param.name}: min (${param.min}) == max (${param.max}) is degenerate",
                    param.min < param.max
                )
            }
        }
    }

    // =========================================================================
    // Sequential parameter IDs
    // =========================================================================

    @Test
    fun `parameter IDs are sequential starting from 0`() {
        // Reverb (index 13) has non-sequential IDs because internal params
        // (Damping, PreDelay, Size, SpringDecay) are hidden from the registry
        // but still exist in the C++ engine. Skip sequential check for Reverb.
        for (info in EffectRegistry.effects) {
            if (info.type == "Reverb") {
                // Just verify IDs are unique and non-negative
                val ids = info.params.map { it.id }
                assertEquals("${info.type}: duplicate param IDs", ids.size, ids.toSet().size)
                assertTrue("${info.type}: negative param ID", ids.all { it >= 0 })
                continue
            }
            for ((i, param) in info.params.withIndex()) {
                assertEquals(
                    "${info.type}: param '${param.name}' at position $i should have id $i",
                    i,
                    param.id
                )
            }
        }
    }

    // =========================================================================
    // Enum parameter label validation
    // =========================================================================

    @Test
    fun `enum parameters have labels for all valid integer values`() {
        for (info in EffectRegistry.effects) {
            for (param in info.params) {
                if (param.valueLabels.isEmpty()) continue

                // Enum params have integer values from min to max
                val minInt = param.min.roundToInt()
                val maxInt = param.max.roundToInt()

                for (v in minInt..maxInt) {
                    assertTrue(
                        "${info.type}.${param.name}: missing label for value $v " +
                            "(has labels for ${param.valueLabels.keys})",
                        param.valueLabels.containsKey(v)
                    )
                }
            }
        }
    }

    @Test
    fun `enum parameters have no labels outside valid range`() {
        for (info in EffectRegistry.effects) {
            for (param in info.params) {
                if (param.valueLabels.isEmpty()) continue

                val minInt = param.min.roundToInt()
                val maxInt = param.max.roundToInt()

                for (key in param.valueLabels.keys) {
                    assertTrue(
                        "${info.type}.${param.name}: label key $key is outside range [$minInt, $maxInt]",
                        key in minInt..maxInt
                    )
                }
            }
        }
    }

    @Test
    fun `enum parameter labels are non-empty strings`() {
        for (info in EffectRegistry.effects) {
            for (param in info.params) {
                for ((key, label) in param.valueLabels) {
                    assertTrue(
                        "${info.type}.${param.name}: label for value $key is empty",
                        label.isNotEmpty()
                    )
                }
            }
        }
    }

    // =========================================================================
    // Known boundary value spot checks
    // =========================================================================

    @Test
    fun `Phaser stages range is 2 to 8`() {
        val phaser = EffectRegistry.getEffectInfo(10)!!
        val stages = phaser.params.find { it.name == "Stages" }!!
        assertEquals(2f, stages.min, 0.001f)
        assertEquals(8f, stages.max, 0.001f)
    }

    @Test
    fun `Delay time range is 1 to 2000 ms`() {
        val delay = EffectRegistry.getEffectInfo(12)!!
        val time = delay.params.find { it.name == "Delay Time" }!!
        assertEquals(1f, time.min, 0.001f)
        assertEquals(2000f, time.max, 0.001f)
    }

    @Test
    fun `Reverb decay range is 0 to 1 normalized`() {
        val reverb = EffectRegistry.getEffectInfo(13)!!
        val decay = reverb.params.find { it.name == "Decay" }!!
        assertEquals(0f, decay.min, 0.001f)
        assertEquals(1f, decay.max, 0.001f)
    }

    @Test
    fun `Flanger feedback allows negative values`() {
        val flanger = EffectRegistry.getEffectInfo(11)!!
        val feedback = flanger.params.find { it.name == "Feedback" }!!
        assertTrue("Flanger feedback min should be negative", feedback.min < 0f)
    }

    @Test
    fun `NoiseGate threshold is in dB with negative range`() {
        val gate = EffectRegistry.getEffectInfo(0)!!
        val threshold = gate.params.find { it.name == "Threshold" }!!
        assertTrue("Gate threshold min should be negative dB", threshold.min < 0f)
        assertEquals(0f, threshold.max, 0.001f)
        assertEquals("dB", threshold.unit)
    }

    // =========================================================================
    // Total parameter count sanity
    // =========================================================================

    @Test
    fun `total parameter count across all effects is at least 70`() {
        val totalParams = EffectRegistry.effects.sumOf { it.params.size }
        assertTrue(
            "Expected at least 70 parameters total, got $totalParams",
            totalParams >= 70
        )
    }

    // =========================================================================
    // Helper
    // =========================================================================

    private fun assertParamsInRange(effectType: String) {
        val info = EffectRegistry.effects.find { it.type == effectType }
        assertNotNull("Effect type '$effectType' not found in registry", info)
        val effect = info!!
        if (effect.isReadOnly) return

        assertTrue(
            "$effectType should have at least 1 parameter",
            effect.params.isNotEmpty()
        )

        for (param in effect.params) {
            assertTrue(
                "$effectType.${param.name}: default (${param.defaultValue}) < min (${param.min})",
                param.defaultValue >= param.min
            )
            assertTrue(
                "$effectType.${param.name}: default (${param.defaultValue}) > max (${param.max})",
                param.defaultValue <= param.max
            )
        }
    }
}
