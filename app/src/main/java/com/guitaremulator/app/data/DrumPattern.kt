package com.guitaremulator.app.data

import java.util.UUID

/**
 * Single step in a drum sequencer track.
 *
 * Maps to the C++ drums::Step struct. Steps carry trigger, velocity,
 * probability, and retrig data for Elektron-style sequencing.
 *
 * @property trigger   Whether this step fires a note.
 * @property velocity  MIDI-style velocity 0-127 (100 = default).
 * @property probability Chance of firing 1-100 (100 = always).
 * @property retrigCount Sub-triggers per step (0=off, 2, 3, 4, 6, 8).
 * @property retrigDecay Velocity curve: 0=flat, 1=ramp-up, 2=ramp-down.
 */
data class DrumStep(
    val trigger: Boolean = false,
    val velocity: Int = 100,
    val probability: Int = 100,
    val retrigCount: Int = 0,
    val retrigDecay: Int = 0
)

/**
 * One track's data within a drum pattern.
 *
 * Contains the step sequence plus voice configuration and mix settings.
 * Maps to the C++ drums::Track struct for pattern serialization.
 *
 * @property steps       Step data (up to 64 steps, default 16).
 * @property length      Per-track length for polymetric patterns (1-64).
 * @property engineType  Voice engine type (0=FM, 1=Subtractive, 2=Metallic, 3=Noise, 4=Physical, 5=MultiLayer).
 * @property voiceParams Synthesis parameters for the track's voice (up to 16).
 * @property volume      Track output level (0.0-1.0).
 * @property pan         Stereo pan position (-1.0=L, 0.0=center, 1.0=R).
 * @property muted       Whether this track is muted.
 * @property chokeGroup  Choke group ID (-1=none, 0=hat group, 1-3=user).
 * @property drive       Drive saturation amount (0.0-5.0).
 * @property filterCutoff SVF filter cutoff in Hz (20-16000).
 * @property filterRes   SVF filter resonance (0.0-1.0).
 */
data class DrumTrackData(
    val steps: List<DrumStep> = List(16) { DrumStep() },
    val length: Int = 16,
    val engineType: Int = 0,
    val voiceParams: List<Float> = List(16) { 0f },
    val volume: Float = 0.8f,
    val pan: Float = 0.0f,
    val muted: Boolean = false,
    val chokeGroup: Int = -1,
    val drive: Float = 0.0f,
    val filterCutoff: Float = 16000f,
    val filterRes: Float = 0.0f
)

/**
 * Complete drum pattern with 8 tracks, tempo, swing, and metadata.
 *
 * Patterns are serialized to JSON using org.json (matching the existing
 * preset system's approach) and stored in the app's internal storage.
 *
 * @property id      Unique identifier (UUID string).
 * @property version Schema version for forward compatibility.
 * @property tracks  8 drum tracks with step data and voice config.
 * @property length  Global pattern length in steps (1-64).
 * @property swing   Swing percentage (50=straight, 75=heavy shuffle).
 * @property bpm     Tempo in beats per minute (20-300).
 * @property name    User-facing display name.
 * @property isFactory True for built-in patterns that cannot be deleted.
 */
data class DrumPatternData(
    val id: String = UUID.randomUUID().toString(),
    val version: Int = 1,
    val tracks: List<DrumTrackData> = List(8) { DrumTrackData() },
    val length: Int = 16,
    val swing: Float = 50f,
    val bpm: Float = 120f,
    val name: String = "New Pattern",
    val isFactory: Boolean = false
)
