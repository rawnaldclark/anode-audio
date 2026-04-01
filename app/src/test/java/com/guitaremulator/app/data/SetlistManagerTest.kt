package com.guitaremulator.app.data

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito

/**
 * Unit tests for SetlistManager JSON serialization and deserialization.
 *
 * Tests the round-trip fidelity of setlistToJson / jsonToSetlist, edge cases
 * for missing fields, and invalid input handling.
 *
 * Uses returnDefaultValues = true in testOptions so android.util.Log
 * returns default values without Robolectric. Context is mocked with Mockito.
 */
class SetlistManagerTest {

    private lateinit var manager: SetlistManager

    @Before
    fun setUp() {
        val mockContext = Mockito.mock(Context::class.java)
        manager = SetlistManager(mockContext)
    }

    // =========================================================================
    // Round-trip serialization
    // =========================================================================

    @Test
    fun `setlist serializes and deserializes correctly`() {
        val setlist = Setlist(
            id = "setlist-abc-123",
            name = "Friday Night Gig",
            presetIds = listOf("preset-1", "preset-2", "preset-3"),
            createdAt = 1700000000000L
        )

        val json = manager.setlistToJson(setlist)
        val restored = manager.jsonToSetlist(json)

        assertNotNull(restored)
        assertEquals(setlist.id, restored!!.id)
        assertEquals(setlist.name, restored.name)
        assertEquals(setlist.presetIds, restored.presetIds)
        assertEquals(setlist.createdAt, restored.createdAt)
    }

    @Test
    fun `empty preset list serializes correctly`() {
        val setlist = Setlist(
            id = "setlist-empty",
            name = "Empty Set",
            presetIds = emptyList(),
            createdAt = 1700000000000L
        )

        val json = manager.setlistToJson(setlist)
        val restored = manager.jsonToSetlist(json)

        assertNotNull(restored)
        assertTrue(restored!!.presetIds.isEmpty())
    }

    @Test
    fun `setlist with multiple presets preserves order`() {
        val orderedIds = listOf("third", "first", "second", "fourth", "fifth")
        val setlist = Setlist(
            id = "setlist-order",
            name = "Order Test",
            presetIds = orderedIds,
            createdAt = 1700000000000L
        )

        val json = manager.setlistToJson(setlist)
        val restored = manager.jsonToSetlist(json)

        assertNotNull(restored)
        assertEquals(orderedIds.size, restored!!.presetIds.size)
        for (i in orderedIds.indices) {
            assertEquals(
                "Preset ID at index $i should match",
                orderedIds[i],
                restored.presetIds[i]
            )
        }
    }

    // =========================================================================
    // Missing / optional fields
    // =========================================================================

    @Test
    fun `missing presetIds key defaults to empty list`() {
        val json = JSONObject()
        json.put("id", "setlist-no-presets")
        json.put("name", "No Presets Key")
        json.put("createdAt", 1700000000000L)
        // Deliberately omit "presetIds"

        val restored = manager.jsonToSetlist(json.toString())

        assertNotNull(restored)
        assertTrue(restored!!.presetIds.isEmpty())
        assertEquals("setlist-no-presets", restored.id)
        assertEquals("No Presets Key", restored.name)
    }

    // =========================================================================
    // Invalid input
    // =========================================================================

    @Test
    fun `invalid JSON returns null`() {
        val restored = manager.jsonToSetlist("this is not valid json {{{")
        assertNull(restored)
    }

    // =========================================================================
    // Field preservation
    // =========================================================================

    @Test
    fun `setlist name is preserved in round trip`() {
        val names = listOf(
            "Simple Name",
            "Name With Special Ch@rs!",
            "    Leading Spaces",
            "",
            "A".repeat(200)
        )

        for (name in names) {
            val setlist = Setlist(
                id = "name-test",
                name = name,
                createdAt = 1700000000000L
            )

            val json = manager.setlistToJson(setlist)
            val restored = manager.jsonToSetlist(json)

            assertNotNull("Setlist with name '$name' should deserialize", restored)
            assertEquals(
                "Name should be preserved for: '$name'",
                name,
                restored!!.name
            )
        }
    }

    @Test
    fun `setlist id is preserved in round trip`() {
        val ids = listOf(
            "simple-id",
            "550e8400-e29b-41d4-a716-446655440000",
            "id-with-special_chars.v2"
        )

        for (id in ids) {
            val setlist = Setlist(
                id = id,
                name = "ID Test",
                createdAt = 1700000000000L
            )

            val json = manager.setlistToJson(setlist)
            val restored = manager.jsonToSetlist(json)

            assertNotNull("Setlist with id '$id' should deserialize", restored)
            assertEquals("ID should be preserved for: '$id'", id, restored!!.id)
        }
    }

    @Test
    fun `createdAt timestamp is preserved`() {
        val timestamps = listOf(0L, 1L, 1700000000000L, Long.MAX_VALUE)

        for (ts in timestamps) {
            val setlist = Setlist(
                id = "ts-test",
                name = "Timestamp Test",
                presetIds = emptyList(),
                createdAt = ts
            )

            val json = manager.setlistToJson(setlist)
            val restored = manager.jsonToSetlist(json)

            assertNotNull("Setlist with createdAt=$ts should deserialize", restored)
            assertEquals(
                "Timestamp $ts should be preserved",
                ts,
                restored!!.createdAt
            )
        }
    }
}
