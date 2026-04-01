package com.guitaremulator.app.data

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito

/**
 * Integration tests for PresetManager's JSON serialization layer.
 *
 * Focuses on round-trip fidelity through both [PresetManager.presetToJson] and
 * [PresetManager.jsonToPreset], backward compatibility for missing JSON keys,
 * edge cases (special characters, unknown keys, NaN values), and
 * [PresetManager.buildEffectChain] completeness.
 *
 * These tests complement [PresetSerializationTest] by exercising integration
 * scenarios: multi-effect round-trips, effect ordering persistence, and
 * schema evolution where older preset JSON lacks newer fields.
 *
 * Uses a mock [Context] (never accessed at runtime because we only call
 * internal serialization methods). The `returnDefaultValues = true` testOption
 * in build.gradle.kts ensures [android.util.Log] calls return defaults.
 */
class PresetManagerIntegrationTest {

    private lateinit var manager: PresetManager

    @Before
    fun setUp() {
        val mockContext = Mockito.mock(Context::class.java)
        manager = PresetManager(mockContext)
    }

    // =========================================================================
    // 1. Full round-trip: all fields
    // =========================================================================

    @Test
    fun `presetToJson then jsonToPreset round-trip preserves all fields`() {
        val original = Preset(
            id = "integration-001",
            name = "Full Round Trip",
            author = "IntegrationBot",
            category = PresetCategory.HEAVY,
            createdAt = 1710000000000L,
            effects = listOf(
                EffectState(
                    effectType = "NoiseGate",
                    enabled = true,
                    wetDryMix = 0.9f,
                    parameters = mapOf(0 to -45f, 1 to 8f, 2 to 1.5f, 3 to 40f)
                ),
                EffectState(
                    effectType = "Overdrive",
                    enabled = true,
                    wetDryMix = 1.0f,
                    parameters = mapOf(0 to 0.7f, 1 to 0.4f, 2 to 0.6f)
                ),
                EffectState(
                    effectType = "Reverb",
                    enabled = false,
                    wetDryMix = 0.35f,
                    parameters = mapOf(0 to 2.5f, 1 to 0.4f, 2 to 15f, 3 to 0.6f)
                )
            ),
            isFactory = true,
            isFavorite = true,
            inputGain = 1.8f,
            outputGain = 0.6f,
            effectOrder = (0 until EffectRegistry.EFFECT_COUNT).reversed().toList()
        )

        val json = manager.presetToJson(original)
        val restored = manager.jsonToPreset(json)

        assertNotNull("Round-trip should not return null", restored)
        assertEquals(original.id, restored!!.id)
        assertEquals(original.name, restored.name)
        assertEquals(original.author, restored.author)
        assertEquals(original.category, restored.category)
        assertEquals(original.createdAt, restored.createdAt)
        assertEquals(original.isFactory, restored.isFactory)
        assertEquals(original.isFavorite, restored.isFavorite)
        assertEquals(original.inputGain, restored.inputGain, 0.001f)
        assertEquals(original.outputGain, restored.outputGain, 0.001f)
        assertEquals(original.effects.size, restored.effects.size)
        assertEquals(original.effectOrder, restored.effectOrder)
    }

    // =========================================================================
    // 2. Round-trip with multiple effects and all parameters
    // =========================================================================

    @Test
    fun `round-trip preserves effect states with all parameters`() {
        val effects = listOf(
            EffectState(
                effectType = "Compressor",
                enabled = true,
                wetDryMix = 1.0f,
                parameters = mapOf(0 to -20f, 1 to 4f, 2 to 10f, 3 to 100f, 4 to 0f, 5 to 6f)
            ),
            EffectState(
                effectType = "ParametricEQ",
                enabled = true,
                wetDryMix = 1.0f,
                parameters = mapOf(
                    0 to -3f, 1 to -1f, 2 to 0f, 3 to 2f, 4 to 1f,
                    5 to 3f, 6 to 4f, 7 to 2f, 8 to -1f, 9 to 0f, 10 to 0f
                )
            ),
            EffectState(
                effectType = "Phaser",
                enabled = false,
                wetDryMix = 0.4f,
                parameters = mapOf(0 to 0.5f, 1 to 0.5f, 2 to 0.5f, 3 to 6f)
            ),
            EffectState(
                effectType = "BossHM2",
                enabled = true,
                wetDryMix = 1.0f,
                parameters = mapOf(0 to 0.7f, 1 to 0.5f, 2 to 0.5f, 3 to 0.5f)
            ),
            EffectState(
                effectType = "Univibe",
                enabled = true,
                wetDryMix = 0.8f,
                parameters = mapOf(0 to 0.3f, 1 to 0.7f, 2 to 0f)
            )
        )

        val preset = Preset(
            name = "Multi-Effect Integration",
            effects = effects,
            inputGain = 2.0f,
            outputGain = 0.5f
        )

        val json = manager.presetToJson(preset)
        val restored = manager.jsonToPreset(json)

        assertNotNull(restored)
        assertEquals(effects.size, restored!!.effects.size)

        for (i in effects.indices) {
            val orig = effects[i]
            val rest = restored.effects[i]
            assertEquals("Effect $i type mismatch", orig.effectType, rest.effectType)
            assertEquals("Effect $i enabled mismatch", orig.enabled, rest.enabled)
            assertEquals("Effect $i wetDryMix mismatch", orig.wetDryMix, rest.wetDryMix, 0.001f)
            assertEquals(
                "Effect $i param count mismatch",
                orig.parameters.size,
                rest.parameters.size
            )
            for ((paramId, value) in orig.parameters) {
                assertEquals(
                    "Effect $i param $paramId mismatch",
                    value,
                    rest.parameters[paramId]!!,
                    0.001f
                )
            }
        }
    }

    // =========================================================================
    // 3. Round-trip preserves effect order
    // =========================================================================

    @Test
    fun `round-trip preserves effect order`() {
        // A non-identity permutation of 0..EFFECT_COUNT-1
        val n = EffectRegistry.EFFECT_COUNT
        val customOrder = (n - 1 downTo 0).toList()
        val preset = Preset(
            name = "Custom Order",
            effects = emptyList(),
            effectOrder = customOrder
        )

        val json = manager.presetToJson(preset)
        val restored = manager.jsonToPreset(json)

        assertNotNull(restored)
        assertNotNull("effectOrder should survive round-trip", restored!!.effectOrder)
        assertEquals(customOrder, restored.effectOrder)
    }

    // =========================================================================
    // 4. Round-trip preserves isFavorite = true
    // =========================================================================

    @Test
    fun `round-trip preserves isFavorite true`() {
        val preset = Preset(
            name = "My Favorite",
            effects = emptyList(),
            isFavorite = true
        )

        val json = manager.presetToJson(preset)
        val restored = manager.jsonToPreset(json)

        assertNotNull(restored)
        assertTrue("isFavorite should be true after round-trip", restored!!.isFavorite)
    }

    // =========================================================================
    // 5. Missing isFavorite key defaults to false
    // =========================================================================

    @Test
    fun `missing isFavorite key defaults to false`() {
        val json = buildMinimalPresetJson()
        // Explicitly ensure isFavorite is absent (simulates an older preset file)
        assertFalse(
            "Test precondition: isFavorite should not be in the JSON",
            json.has("isFavorite")
        )

        val preset = manager.jsonToPreset(json)

        assertNotNull(preset)
        assertFalse("Missing isFavorite should default to false", preset!!.isFavorite)
    }

    // =========================================================================
    // 6. Missing effectOrder key defaults to null
    // =========================================================================

    @Test
    fun `missing effectOrder key defaults to null`() {
        val json = buildMinimalPresetJson()
        // Do not add effectOrder — simulates a preset from before effectOrder was added
        assertFalse(
            "Test precondition: effectOrder should not be in the JSON",
            json.has("effectOrder")
        )

        val preset = manager.jsonToPreset(json)

        assertNotNull(preset)
        assertNull(
            "Missing effectOrder should default to null for backward compat",
            preset!!.effectOrder
        )
    }

    // =========================================================================
    // 7. Missing inputGain and outputGain default to 1.0
    // =========================================================================

    @Test
    fun `missing inputGain and outputGain default to 1_0`() {
        val json = buildMinimalPresetJson()
        // Do not add inputGain or outputGain — simulates an older preset file
        assertFalse(json.has("inputGain"))
        assertFalse(json.has("outputGain"))

        val preset = manager.jsonToPreset(json)

        assertNotNull(preset)
        assertEquals(
            "Missing inputGain should default to 1.0",
            1.0f, preset!!.inputGain, 0.001f
        )
        assertEquals(
            "Missing outputGain should default to 1.0",
            1.0f, preset.outputGain, 0.001f
        )
    }

    // =========================================================================
    // 8. jsonToPreset returns null for empty string
    // =========================================================================

    @Test
    fun `jsonToPreset returns null for empty string`() {
        // JSONObject("") throws, so wrapping it reproduces the empty-input scenario.
        // jsonToPreset takes a JSONObject, so we test with a JSONObject that has
        // no required fields, which will fail on json.getJSONArray("effects").
        val emptyJson = JSONObject()
        val result = manager.jsonToPreset(emptyJson)

        assertNull("Empty JSON object (no fields) should return null", result)
    }

    // =========================================================================
    // 9. jsonToPreset returns null for invalid JSON
    // =========================================================================

    @Test
    fun `jsonToPreset returns null for invalid JSON`() {
        // Construct a JSONObject that has the wrong type for effects (string instead of array).
        // This will cause getJSONArray("effects") to throw, resulting in null.
        val badJson = JSONObject()
        badJson.put("id", "bad-id")
        badJson.put("name", "Bad")
        badJson.put("effects", "not-an-array")

        val result = manager.jsonToPreset(badJson)

        assertNull("JSON with effects as a string should return null", result)
    }

    // =========================================================================
    // 10. jsonToPreset handles missing effects array gracefully
    // =========================================================================

    @Test
    fun `jsonToPreset handles missing effects array gracefully`() {
        val json = JSONObject()
        json.put("id", "no-effects-id")
        json.put("name", "No Effects")
        json.put("author", "Test")
        json.put("category", "CUSTOM")
        json.put("createdAt", 1700000000000L)
        // Intentionally omit "effects" entirely

        val result = manager.jsonToPreset(json)

        assertNull(
            "Missing effects array should cause jsonToPreset to return null",
            result
        )
    }

    // =========================================================================
    // 11. buildEffectChain fills missing effects with defaults
    // =========================================================================

    @Test
    fun `buildEffectChain fills missing effects with defaults`() {
        // Build a chain with only a few overrides -- the chain should still
        // contain exactly EFFECT_COUNT entries (all 25 effects).
        val chain = manager.buildEffectChain(
            enabledMap = mapOf(0 to true, 4 to true),
            paramOverrides = mapOf(Pair(4, 0) to 0.9f),
            wetDryOverrides = mapOf(13 to 0.5f)
        )

        assertEquals(
            "Chain must contain all registered effects",
            EffectRegistry.EFFECT_COUNT,
            chain.size
        )

        // Verify each chain entry has the correct effectType matching the registry
        for (i in chain.indices) {
            val expected = EffectRegistry.effects[i]
            assertEquals(
                "Effect at index $i should match registry type",
                expected.type,
                chain[i].effectType
            )
        }

        // Verify the overridden values
        assertTrue("NoiseGate (index 0) should be enabled", chain[0].enabled)
        assertTrue("Overdrive (index 4) should be enabled", chain[4].enabled)
        assertEquals(
            "Overdrive drive param should be overridden",
            0.9f, chain[4].parameters[0]!!, 0.001f
        )
        assertEquals(
            "Reverb wet/dry should be overridden",
            0.5f, chain[13].wetDryMix, 0.001f
        )

        // Verify non-overridden effects use their registry defaults
        val wahInfo = EffectRegistry.effects[2] // Wah, enabledByDefault = false
        assertFalse(
            "Wah should use default enabled state (false)",
            chain[2].enabled
        )
        assertEquals(
            "Non-overridden wet/dry should default to 1.0",
            1.0f, chain[0].wetDryMix, 0.001f
        )

        // Verify default parameter values for a non-overridden effect
        val noiseGateInfo = EffectRegistry.effects[0]
        for (paramInfo in noiseGateInfo.params) {
            assertEquals(
                "NoiseGate param ${paramInfo.name} should use default",
                paramInfo.defaultValue,
                chain[0].parameters[paramInfo.id]!!,
                0.001f
            )
        }
    }

    // =========================================================================
    // 12. buildEffectChain matches by effectType not position
    // =========================================================================

    @Test
    fun `buildEffectChain matches by effectType not position`() {
        // Build the full chain, then serialize and deserialize a subset.
        // This verifies that after round-trip, we can find effects by type.
        val fullChain = manager.buildEffectChain()

        // Verify that each entry's effectType corresponds to the registry index
        for (i in fullChain.indices) {
            val info = EffectRegistry.effects[i]
            assertEquals(
                "Chain position $i should have type ${info.type}",
                info.type,
                fullChain[i].effectType
            )
            // Verify the registry can look up the correct index from the type
            assertEquals(
                "getIndexForType should return $i for ${info.type}",
                i,
                EffectRegistry.getIndexForType(fullChain[i].effectType)
            )
        }

        // Serialize a preset with a partial chain (only 3 effects, out of order)
        val partialEffects = listOf(
            fullChain[13], // Reverb
            fullChain[0],  // NoiseGate
            fullChain[20]  // Univibe
        )
        val preset = Preset(name = "Partial", effects = partialEffects)
        val json = manager.presetToJson(preset)
        val restored = manager.jsonToPreset(json)

        assertNotNull(restored)
        assertEquals(3, restored!!.effects.size)

        // Each restored effect should be findable by type in the registry
        for (effect in restored.effects) {
            val idx = EffectRegistry.getIndexForType(effect.effectType)
            assertTrue(
                "Effect type ${effect.effectType} should have a valid registry index",
                idx >= 0
            )
            assertTrue(
                "Effect type ${effect.effectType} index should be within range",
                idx < EffectRegistry.EFFECT_COUNT
            )
        }

        // Verify the types survived round-trip in the same order
        assertEquals("Reverb", restored.effects[0].effectType)
        assertEquals("NoiseGate", restored.effects[1].effectType)
        assertEquals("Univibe", restored.effects[2].effectType)
    }

    // =========================================================================
    // 13. JSON with extra unknown keys is ignored
    // =========================================================================

    @Test
    fun `JSON with extra unknown keys is ignored`() {
        val json = buildMinimalPresetJson()
        json.put("foo", "bar")
        json.put("extraNumber", 42)
        json.put("nestedGarbage", JSONObject().apply { put("a", 1) })

        val preset = manager.jsonToPreset(json)

        assertNotNull("Unknown keys should not break parsing", preset)
        assertEquals("test-id", preset!!.id)
        assertEquals("Test", preset.name)
    }

    // =========================================================================
    // 14. JSON with NaN parameter values are handled
    // =========================================================================

    @Test
    fun `JSON with NaN parameter values are handled`() {
        // org.json on JVM rejects NaN in JSONObject.put(), so we test the gain
        // paths which use optDouble. When a gain key is absent, optDouble returns
        // the fallback (1.0). We also test that string "NaN" is handled.
        //
        // For parameter values, if org.json parses a value as NaN (Android
        // behavior), toFloat() preserves it. The jsonToPreset method does not
        // sanitize individual effect parameters (that is done by
        // deserializePreset). So we verify the gain-level NaN protection here.

        // Test 1: Missing gain keys (optDouble returns 1.0 fallback)
        val json1 = buildMinimalPresetJson()
        // Don't set inputGain or outputGain
        val preset1 = manager.jsonToPreset(json1)
        assertNotNull(preset1)
        assertFalse("inputGain should not be NaN", preset1!!.inputGain.isNaN())
        assertFalse("outputGain should not be NaN", preset1.outputGain.isNaN())
        assertEquals(1.0f, preset1.inputGain, 0.001f)
        assertEquals(1.0f, preset1.outputGain, 0.001f)

        // Test 2: String "NaN" as gain value -- optDouble returns fallback for
        // non-numeric strings on JVM org.json
        val json2 = buildMinimalPresetJson()
        json2.put("inputGain", "NaN")
        json2.put("outputGain", "NaN")
        val preset2 = manager.jsonToPreset(json2)
        assertNotNull(preset2)
        assertFalse("String 'NaN' inputGain should be sanitized", preset2!!.inputGain.isNaN())
        assertFalse("String 'NaN' outputGain should be sanitized", preset2.outputGain.isNaN())

        // Test 3: Infinity as string -- similarly handled by optDouble fallback
        val json3 = buildMinimalPresetJson()
        json3.put("inputGain", "Infinity")
        json3.put("outputGain", "-Infinity")
        val preset3 = manager.jsonToPreset(json3)
        assertNotNull(preset3)
        assertFalse("String 'Infinity' inputGain should be sanitized", preset3!!.inputGain.isInfinite())
        assertFalse("String '-Infinity' outputGain should be sanitized", preset3.outputGain.isInfinite())
    }

    // =========================================================================
    // 15. Preset name with special characters survives round-trip
    // =========================================================================

    @Test
    fun `preset name with special characters survives round-trip`() {
        // Test unicode, quotes, backslashes, newlines, and emoji
        val specialNames = listOf(
            "Preset with \"quotes\"",
            "Backslash\\Path",
            "Unicode: \u00e9\u00e0\u00fc\u00f1",
            "CJK: \u4e2d\u6587\u65e5\u672c\u8a9e",
            "Newline\nTab\tCarriage\r",
            "Emoji: \uD83C\uDFB8\uD83D\uDD25",
            "Mixed: O'Brien's \"Amp\" @ 100% <gain> & more"
        )

        for (name in specialNames) {
            val preset = Preset(name = name, effects = emptyList())
            val json = manager.presetToJson(preset)
            val restored = manager.jsonToPreset(json)

            assertNotNull("Preset with name '$name' should survive round-trip", restored)
            assertEquals(
                "Name should be preserved exactly",
                name,
                restored!!.name
            )
        }
    }

    // =========================================================================
    // Additional integration edge cases
    // =========================================================================

    @Test
    fun `invalid effectOrder permutation falls back to null`() {
        // effectOrder must be a valid permutation of [0, EFFECT_COUNT).
        // An invalid permutation (duplicates, wrong size) should be discarded.
        val json = buildMinimalPresetJson()

        // Wrong size: only 3 elements instead of 21
        val shortOrder = JSONArray()
        shortOrder.put(0)
        shortOrder.put(1)
        shortOrder.put(2)
        json.put("effectOrder", shortOrder)

        val preset = manager.jsonToPreset(json)

        assertNotNull(preset)
        assertNull(
            "Invalid effectOrder (wrong size) should fall back to null",
            preset!!.effectOrder
        )
    }

    @Test
    fun `effectOrder with duplicates falls back to null`() {
        val json = buildMinimalPresetJson()

        // Correct size (21) but with duplicates
        val dupOrder = JSONArray()
        for (i in 0 until EffectRegistry.EFFECT_COUNT) {
            dupOrder.put(0) // All zeros -- invalid permutation
        }
        json.put("effectOrder", dupOrder)

        val preset = manager.jsonToPreset(json)

        assertNotNull(preset)
        assertNull(
            "effectOrder with duplicates should fall back to null",
            preset!!.effectOrder
        )
    }

    @Test
    fun `effectOrder with out-of-range index falls back to null`() {
        val json = buildMinimalPresetJson()

        val badOrder = JSONArray()
        for (i in 0 until EffectRegistry.EFFECT_COUNT) {
            badOrder.put(i + 100) // All out-of-range
        }
        json.put("effectOrder", badOrder)

        val preset = manager.jsonToPreset(json)

        assertNotNull(preset)
        assertNull(
            "effectOrder with out-of-range values should fall back to null",
            preset!!.effectOrder
        )
    }

    @Test
    fun `valid identity effectOrder is preserved`() {
        val json = buildMinimalPresetJson()

        // Identity order: 0, 1, 2, ..., 20
        val identityOrder = JSONArray()
        for (i in 0 until EffectRegistry.EFFECT_COUNT) {
            identityOrder.put(i)
        }
        json.put("effectOrder", identityOrder)

        val preset = manager.jsonToPreset(json)

        assertNotNull(preset)
        assertNotNull(
            "Valid identity effectOrder should be preserved (not nulled out)",
            preset!!.effectOrder
        )
        assertEquals(
            (0 until EffectRegistry.EFFECT_COUNT).toList(),
            preset.effectOrder
        )
    }

    @Test
    fun `gain values at extreme valid boundaries are preserved`() {
        val json = buildMinimalPresetJson()
        json.put("inputGain", 0.0)
        json.put("outputGain", 4.0)

        val preset = manager.jsonToPreset(json)

        assertNotNull(preset)
        assertEquals(0.0f, preset!!.inputGain, 0.001f)
        assertEquals(4.0f, preset.outputGain, 0.001f)

        // Round-trip the boundary values through presetToJson -> jsonToPreset
        val json2 = manager.presetToJson(preset)
        val restored = manager.jsonToPreset(json2)

        assertNotNull(restored)
        assertEquals(0.0f, restored!!.inputGain, 0.001f)
        assertEquals(4.0f, restored.outputGain, 0.001f)
    }

    // =========================================================================
    // Helpers
    // =========================================================================

    /**
     * Build a minimal valid JSON object representing a preset with no optional
     * fields. Uses only the required keys: id, name, author, category,
     * createdAt, isFactory, effects.
     *
     * Does NOT include: isFavorite, inputGain, outputGain, effectOrder.
     * This simulates an "older" preset file for backward compatibility testing.
     */
    private fun buildMinimalPresetJson(
        effectsJson: String = "[]"
    ): JSONObject {
        val json = JSONObject()
        json.put("id", "test-id")
        json.put("name", "Test")
        json.put("author", "Tester")
        json.put("category", "CUSTOM")
        json.put("createdAt", 1700000000000L)
        json.put("isFactory", false)
        json.put("effects", JSONArray(effectsJson))
        return json
    }
}
