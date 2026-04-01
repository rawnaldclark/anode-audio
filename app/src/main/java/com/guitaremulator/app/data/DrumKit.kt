package com.guitaremulator.app.data

/**
 * Voice configuration for a single track in a drum kit.
 *
 * A kit stores the synthesis voice settings without any pattern (step) data,
 * allowing the same sounds to be reused across different patterns.
 *
 * @property engineType Voice engine type matching drums::EngineType enum
 *     (0=FM, 1=Subtractive, 2=Metallic, 3=Noise, 4=Physical, 5=MultiLayer).
 * @property voiceParams Synthesis parameter values (up to 16 floats).
 * @property name Human-readable voice name for UI display (e.g., "808 Kick").
 */
data class DrumKitVoice(
    val engineType: Int = 0,
    val voiceParams: List<Float> = List(16) { 0f },
    val name: String = "Voice"
)

/**
 * A drum kit containing 8 voice configurations.
 *
 * Kits decouple sound design from step programming. Users can load a kit
 * onto any pattern to change all 8 voices without altering the step sequence.
 *
 * @property name   Display name for the kit (e.g., "808 Classic", "909 Dance").
 * @property voices 8 voice configurations, one per track.
 */
data class DrumKit(
    val name: String = "Default Kit",
    val voices: List<DrumKitVoice> = List(8) { DrumKitVoice() }
)
