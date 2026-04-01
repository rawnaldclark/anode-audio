package com.guitaremulator.app.data

import java.util.UUID

/**
 * Ordered collection of preset IDs for live performance.
 *
 * A setlist references presets by their UUID strings, allowing the user
 * to arrange presets in a specific order for a gig or practice session.
 *
 * @property id Unique identifier (UUID string).
 * @property name User-facing display name.
 * @property presetIds Ordered list of preset UUIDs in this setlist.
 * @property createdAt Unix timestamp (millis) of creation time.
 */
data class Setlist(
    val id: String = UUID.randomUUID().toString(),
    val name: String,
    val presetIds: List<String> = emptyList(),
    val createdAt: Long = System.currentTimeMillis()
)
