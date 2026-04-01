package com.guitaremulator.app.data

import android.content.Context
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito
import java.io.File

/**
 * Unit tests for MidiMappingManager JSON serialization, deserialization,
 * validation, and CC lookup logic.
 *
 * Pure JVM tests (no Robolectric). Uses returnDefaultValues = true in
 * testOptions so android.util.Log returns default values. Context is
 * mocked with Mockito; a real temp directory is used for file I/O tests.
 */
class MidiMappingManagerTest {

    private lateinit var manager: MidiMappingManager
    private lateinit var tempDir: File

    @Before
    fun setUp() {
        tempDir = createTempDir("midi_test")
        val mockContext = Mockito.mock(Context::class.java)
        Mockito.`when`(mockContext.filesDir).thenReturn(tempDir)
        manager = MidiMappingManager(mockContext)
    }

    // =========================================================================
    // 1. Round-trip serialization
    // =========================================================================

    @Test
    fun `mapping serializes and deserializes correctly`() {
        val mapping = MidiMapping(
            id = "test-mapping-001",
            ccNumber = 7,
            midiChannel = 1,
            targetEffectIndex = 4,
            targetParamId = 0,
            minValue = 0.0f,
            maxValue = 1.0f,
            curve = MappingCurve.LOGARITHMIC
        )

        val json = manager.mappingToJson(mapping)
        val restored = manager.jsonToMapping(json)

        assertNotNull(restored)
        assertEquals(mapping.id, restored!!.id)
        assertEquals(mapping.ccNumber, restored.ccNumber)
        assertEquals(mapping.midiChannel, restored.midiChannel)
        assertEquals(mapping.targetEffectIndex, restored.targetEffectIndex)
        assertEquals(mapping.targetParamId, restored.targetParamId)
        assertEquals(mapping.minValue, restored.minValue, 0.001f)
        assertEquals(mapping.maxValue, restored.maxValue, 0.001f)
        assertEquals(mapping.curve, restored.curve)
    }

    // =========================================================================
    // 2. Invalid CC number rejected
    // =========================================================================

    @Test
    fun `invalid ccNumber rejected by validation`() {
        // CC number below range
        val mappingLow = MidiMapping(
            ccNumber = -1,
            midiChannel = 0,
            targetEffectIndex = 0,
            targetParamId = 0,
            minValue = -80f,
            maxValue = 0f
        )
        assertFalse(manager.addMapping(mappingLow))

        // CC number above range
        val mappingHigh = MidiMapping(
            ccNumber = 128,
            midiChannel = 0,
            targetEffectIndex = 0,
            targetParamId = 0,
            minValue = -80f,
            maxValue = 0f
        )
        assertFalse(manager.addMapping(mappingHigh))

        // Verify nothing was persisted
        assertTrue(manager.loadMappings().isEmpty())
    }

    // =========================================================================
    // 3. Invalid channel rejected
    // =========================================================================

    @Test
    fun `invalid channel rejected by validation`() {
        // Channel below range
        val mappingLow = MidiMapping(
            ccNumber = 7,
            midiChannel = -1,
            targetEffectIndex = 0,
            targetParamId = 0,
            minValue = -80f,
            maxValue = 0f
        )
        assertFalse(manager.addMapping(mappingLow))

        // Channel above range
        val mappingHigh = MidiMapping(
            ccNumber = 7,
            midiChannel = 16,
            targetEffectIndex = 0,
            targetParamId = 0,
            minValue = -80f,
            maxValue = 0f
        )
        assertFalse(manager.addMapping(mappingHigh))

        // Verify nothing was persisted
        assertTrue(manager.loadMappings().isEmpty())
    }

    // =========================================================================
    // 4. Invalid effect index rejected
    // =========================================================================

    @Test
    fun `invalid effectIndex rejected by validation`() {
        // Effect index below range
        val mappingLow = MidiMapping(
            ccNumber = 7,
            midiChannel = 0,
            targetEffectIndex = -1,
            targetParamId = 0,
            minValue = 0f,
            maxValue = 1f
        )
        assertFalse(manager.addMapping(mappingLow))

        // Effect index above range
        val mappingHigh = MidiMapping(
            ccNumber = 7,
            midiChannel = 0,
            targetEffectIndex = 21,
            targetParamId = 0,
            minValue = 0f,
            maxValue = 1f
        )
        assertFalse(manager.addMapping(mappingHigh))

        // Verify nothing was persisted
        assertTrue(manager.loadMappings().isEmpty())
    }

    // =========================================================================
    // 5. getMappingForCC returns correct mapping
    // =========================================================================

    @Test
    fun `getMappingForCC returns correct mapping`() {
        // Mapping 1: CC 7 on channel 1 -> Overdrive Drive
        val mapping1 = MidiMapping(
            id = "map-cc7",
            ccNumber = 7,
            midiChannel = 1,
            targetEffectIndex = 4, // Overdrive
            targetParamId = 0,     // Drive
            minValue = 0f,
            maxValue = 1f
        )
        // Mapping 2: CC 11 on channel 1 -> Overdrive Tone
        val mapping2 = MidiMapping(
            id = "map-cc11",
            ccNumber = 11,
            midiChannel = 1,
            targetEffectIndex = 4, // Overdrive
            targetParamId = 1,     // Tone
            minValue = 0f,
            maxValue = 1f
        )

        assertTrue(manager.addMapping(mapping1))
        assertTrue(manager.addMapping(mapping2))

        // Look up CC 7 on channel 1 -> should find mapping1
        val found7 = manager.getMappingForCC(7, 1)
        assertNotNull(found7)
        assertEquals("map-cc7", found7!!.id)

        // Look up CC 11 on channel 1 -> should find mapping2
        val found11 = manager.getMappingForCC(11, 1)
        assertNotNull(found11)
        assertEquals("map-cc11", found11!!.id)

        // Look up CC 99 -> should find nothing
        val foundNone = manager.getMappingForCC(99, 1)
        assertNull(foundNone)
    }

    // =========================================================================
    // 6. Omni channel matching
    // =========================================================================

    @Test
    fun `getMappingForCC with omni channel matches any channel`() {
        // Mapping with omni channel (0) should match any incoming channel
        val omniMapping = MidiMapping(
            id = "map-omni",
            ccNumber = 1,
            midiChannel = 0, // Omni
            targetEffectIndex = 4, // Overdrive
            targetParamId = 0,     // Drive
            minValue = 0f,
            maxValue = 1f
        )
        assertTrue(manager.addMapping(omniMapping))

        // Should match channel 5
        val found = manager.getMappingForCC(1, 5)
        assertNotNull(found)
        assertEquals("map-omni", found!!.id)

        // Should match channel 10
        val found10 = manager.getMappingForCC(1, 10)
        assertNotNull(found10)
        assertEquals("map-omni", found10!!.id)

        // Should match channel 1
        val found1 = manager.getMappingForCC(1, 1)
        assertNotNull(found1)
        assertEquals("map-omni", found1!!.id)
    }

    // =========================================================================
    // 7. Multiple mappings round-trip through JSON string
    // =========================================================================

    @Test
    fun `multiple mappings round-trip through JSON string`() {
        val mappings = listOf(
            MidiMapping(
                id = "m1",
                ccNumber = 1,
                midiChannel = 0,
                targetEffectIndex = 0, // NoiseGate
                targetParamId = 0,     // Threshold
                minValue = -80f,
                maxValue = 0f,
                curve = MappingCurve.LINEAR
            ),
            MidiMapping(
                id = "m2",
                ccNumber = 7,
                midiChannel = 1,
                targetEffectIndex = 4, // Overdrive
                targetParamId = 0,     // Drive
                minValue = 0f,
                maxValue = 1f,
                curve = MappingCurve.EXPONENTIAL
            ),
            MidiMapping(
                id = "m3",
                ccNumber = 11,
                midiChannel = 15,
                targetEffectIndex = 13, // Reverb
                targetParamId = 0,      // Decay
                minValue = 0.1f,
                maxValue = 10f,
                curve = MappingCurve.LOGARITHMIC
            )
        )

        val jsonString = manager.mappingsToJsonString(mappings)
        val restored = manager.jsonStringToMappings(jsonString)

        assertEquals(mappings.size, restored.size)
        for (i in mappings.indices) {
            assertEquals(mappings[i].id, restored[i].id)
            assertEquals(mappings[i].ccNumber, restored[i].ccNumber)
            assertEquals(mappings[i].midiChannel, restored[i].midiChannel)
            assertEquals(mappings[i].targetEffectIndex, restored[i].targetEffectIndex)
            assertEquals(mappings[i].targetParamId, restored[i].targetParamId)
            assertEquals(mappings[i].minValue, restored[i].minValue, 0.001f)
            assertEquals(mappings[i].maxValue, restored[i].maxValue, 0.001f)
            assertEquals(mappings[i].curve, restored[i].curve)
        }
    }

    // =========================================================================
    // 8. MappingCurve enum serializes as string
    // =========================================================================

    @Test
    fun `MappingCurve enum serializes as string`() {
        for (curve in MappingCurve.entries) {
            val mapping = MidiMapping(
                id = "curve-test-${curve.name}",
                ccNumber = 1,
                midiChannel = 0,
                targetEffectIndex = 4, // Overdrive
                targetParamId = 0,     // Drive
                minValue = 0f,
                maxValue = 1f,
                curve = curve
            )

            val json = manager.mappingToJson(mapping)

            // Verify the JSON contains the curve name as a plain string
            assertEquals(curve.name, json.getString("curve"))

            // Verify round-trip preserves the curve
            val restored = manager.jsonToMapping(json)
            assertNotNull(restored)
            assertEquals(curve, restored!!.curve)
        }
    }
}
