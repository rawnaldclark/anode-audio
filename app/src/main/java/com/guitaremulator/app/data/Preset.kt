package com.guitaremulator.app.data

import java.util.UUID

/**
 * Represents a single effect's state within a preset.
 *
 * Maps directly to the C++ signal chain: each effect has a type string matching
 * its C++ class name, an enabled flag, a wet/dry mix level, and a map of
 * parameter IDs to their float values.
 *
 * @property effectType String identifier matching the C++ effect class name.
 *     One of: "NoiseGate", "Compressor", "Wah", "Boost", "Overdrive",
 *     "AmpModel", "CabinetSim", "ParametricEQ", "Chorus", "Vibrato",
 *     "Phaser", "Flanger", "Delay", "Reverb", "Tuner", "Tremolo",
 *     "BossDS1", "ProCoRAT", "BossDS2", "BossHM2", "Univibe",
 *     "FuzzFace", "Rangemaster", "BigMuff", "Octavia", "RotarySpeaker",
 *     "Fuzzrite", "MF102RingMod", "SpaceEcho", "DL4Delay", "BossBD2",
 *     "TubeScreamer".
 * @property enabled Whether this effect is active in the signal chain.
 * @property wetDryMix Blend between dry (0.0) and wet (1.0) signal.
 * @property parameters Map of parameter ID (Int) to value (Float).
 *     Parameter IDs are effect-specific and match the C++ setParameter API.
 */
data class EffectState(
    val effectType: String,
    val enabled: Boolean,
    val wetDryMix: Float = 1.0f,
    val parameters: Map<Int, Float> = emptyMap()
)

/**
 * Category for organizing presets in the UI.
 *
 * Each category maps to a common guitar tone type, making it easy for users
 * to browse and find presets by musical style.
 */
enum class PresetCategory {
    CLEAN,
    CRUNCH,
    HEAVY,
    AMBIENT,
    ACOUSTIC,
    CUSTOM
}

/**
 * Complete preset containing the full state of the signal chain.
 *
 * Presets are persisted as JSON files in the app's internal storage.
 * Factory presets are bundled as assets and loaded on first run.
 *
 * @property id Unique identifier (UUID string). Generated at creation time.
 * @property name User-facing display name for the preset.
 * @property author Creator of the preset ("Factory" for built-in presets).
 * @property category Classification for browsing/filtering.
 * @property createdAt Unix timestamp (millis) of when the preset was created.
 * @property effects Ordered list of [EffectState] matching the signal chain.
 *     Older presets may have fewer entries; missing effects use defaults.
 * @property isFactory True for built-in presets that cannot be deleted.
 * @property inputGain Master input gain (0.0..4.0, default 1.0 = unity).
 *     Older presets without this field default to 1.0.
 * @property outputGain Master output gain (0.0..4.0, default 1.0 = unity).
 *     Older presets without this field default to 1.0.
 */
data class Preset(
    val id: String = UUID.randomUUID().toString(),
    val name: String,
    val author: String = "User",
    val category: PresetCategory = PresetCategory.CUSTOM,
    val createdAt: Long = System.currentTimeMillis(),
    val effects: List<EffectState>,
    val isFactory: Boolean = false,
    val inputGain: Float = 1.0f,
    val outputGain: Float = 1.0f,
    val effectOrder: List<Int>? = null,
    val isFavorite: Boolean = false
)

/**
 * Metadata describing each effect in the signal chain.
 *
 * Used by the UI to render parameter editors with correct names, ranges,
 * and units. Also provides the mapping from chain index to effect type string.
 */
object EffectRegistry {

    /**
     * Description of a single parameter on an effect.
     *
     * @property id Parameter ID matching the C++ setParameter API.
     * @property name Human-readable parameter name.
     * @property min Minimum allowed value.
     * @property max Maximum allowed value.
     * @property defaultValue Default value when no preset overrides it.
     * @property unit Display unit suffix (e.g., "dB", "ms", "%").
     */
    data class ParamInfo(
        val id: Int,
        val name: String,
        val min: Float,
        val max: Float,
        val defaultValue: Float,
        val unit: String = "",
        val valueLabels: Map<Int, String> = emptyMap()
    )

    /**
     * Description of an effect in the signal chain.
     *
     * @property index Position in the chain (0..31).
     * @property type C++ class name string.
     * @property displayName User-facing name.
     * @property enabledByDefault Whether the effect is on in a default chain.
     * @property params List of parameter descriptors.
     * @property isReadOnly True for effects that only expose read-only data (Tuner).
     */
    data class EffectInfo(
        val index: Int,
        val type: String,
        val displayName: String,
        val enabledByDefault: Boolean = true,
        val params: List<ParamInfo> = emptyList(),
        val isReadOnly: Boolean = false
    )

    /** Total number of effects in the signal chain. */
    const val EFFECT_COUNT = 32

    /**
     * Complete registry of all effects in signal chain order.
     *
     * Each entry documents the effect's index, type string, display name,
     * default enabled state, and all available parameters with their ranges.
     */
    val effects: List<EffectInfo> = listOf(
        // Index 0: Noise Gate
        EffectInfo(
            index = 0,
            type = "NoiseGate",
            displayName = "Noise Gate",
            params = listOf(
                ParamInfo(0, "Threshold", -80f, 0f, -40f, "dB"),
                ParamInfo(1, "Hysteresis", 0f, 20f, 6f, "dB"),
                ParamInfo(2, "Attack", 0.1f, 50f, 1f, "ms"),
                ParamInfo(3, "Release", 5f, 500f, 50f, "ms")
            )
        ),
        // Index 1: Compressor
        EffectInfo(
            index = 1,
            type = "Compressor",
            displayName = "Compressor",
            params = listOf(
                ParamInfo(0, "Threshold", -60f, 0f, -20f, "dB"),
                ParamInfo(1, "Ratio", 1f, 20f, 4f, ":1"),
                ParamInfo(2, "Attack", 0.1f, 100f, 10f, "ms"),
                ParamInfo(3, "Release", 10f, 1000f, 100f, "ms"),
                ParamInfo(4, "Makeup Gain", 0f, 30f, 0f, "dB"),
                ParamInfo(5, "Knee Width", 0f, 12f, 6f, "dB"),
                ParamInfo(6, "Character", 0f, 1f, 0f, valueLabels = mapOf(0 to "VCA", 1 to "OTA"))
            )
        ),
        // Index 2: Wah
        EffectInfo(
            index = 2,
            type = "Wah",
            displayName = "Wah",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Sensitivity", 0f, 1f, 0.5f),
                ParamInfo(1, "Frequency", 200f, 4000f, 800f, "Hz"),
                ParamInfo(2, "Resonance", 0.5f, 10f, 3f),
                ParamInfo(3, "Mode", 0f, 1f, 0f, valueLabels = mapOf(0 to "Auto", 1 to "Manual")),
                ParamInfo(4, "Wah Type", 0f, 2f, 0f, valueLabels = mapOf(0 to "Auto", 1 to "Cry Baby", 2 to "WH-10"))
            )
        ),
        // Index 3: Boost
        EffectInfo(
            index = 3,
            type = "Boost",
            displayName = "Boost",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Level", 0f, 24f, 0f, "dB"),
                ParamInfo(1, "Mode", 0f, 1f, 0f, valueLabels = mapOf(0 to "Clean", 1 to "EP-3"))
            )
        ),
        // Index 4: Overdrive
        EffectInfo(
            index = 4,
            type = "Overdrive",
            displayName = "Overdrive",
            params = listOf(
                ParamInfo(0, "Drive", 0f, 1f, 0.3f),
                ParamInfo(1, "Tone", 0f, 1f, 0.5f),
                ParamInfo(2, "Level", 0f, 1f, 0.7f)
            )
        ),
        // Index 5: Amp Model
        EffectInfo(
            index = 5,
            type = "AmpModel",
            displayName = "Amp Model",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Input Gain", 0f, 2f, 1.0f),
                ParamInfo(1, "Output Level", 0f, 1f, 0.7f),
                ParamInfo(2, "Model Type", 0f, 8f, 0f, valueLabels = mapOf(
                    0 to "Clean", 1 to "Crunch", 2 to "Hi-Gain",
                    3 to "Plexi", 4 to "Hiwatt", 5 to "Silver Jubilee",
                    6 to "AC30", 7 to "Twin Reverb", 8 to "Silvertone"
                )),
                ParamInfo(3, "Variac", 0f, 1f, 0f)
            )
        ),
        // Index 6: Cabinet Sim
        // Cabinet Type: 0=1x12 Combo, 1=4x12 British, 2=2x12 Open Back, 3=User IR,
        //   4=Greenback, 5=G12H-30, 6=G12-65, 7=Fane Crescendo, 8=Alnico Blue, 9=Jensen C10Q
        EffectInfo(
            index = 6,
            type = "CabinetSim",
            displayName = "Cabinet Sim",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Cabinet Type", 0f, 9f, 0f, valueLabels = mapOf(
                    0 to "1x12 Combo", 1 to "4x12 British", 2 to "2x12 Open",
                    3 to "User IR", 4 to "Greenback", 5 to "G12H-30",
                    6 to "G12-65", 7 to "Fane Crescendo", 8 to "Alnico Blue",
                    9 to "Jensen C10Q"
                )),
                ParamInfo(1, "Mix", 0f, 1f, 1f)
            )
        ),
        // Index 7: Graphic EQ (10-band MXR M108S style + Level)
        EffectInfo(
            index = 7,
            type = "ParametricEQ",
            displayName = "Graphic EQ",
            params = listOf(
                ParamInfo(0, "31.25 Hz", -12f, 12f, 0f, unit = "dB"),
                ParamInfo(1, "62.5 Hz", -12f, 12f, 0f, unit = "dB"),
                ParamInfo(2, "125 Hz", -12f, 12f, 0f, unit = "dB"),
                ParamInfo(3, "250 Hz", -12f, 12f, 0f, unit = "dB"),
                ParamInfo(4, "500 Hz", -12f, 12f, 0f, unit = "dB"),
                ParamInfo(5, "1 kHz", -12f, 12f, 0f, unit = "dB"),
                ParamInfo(6, "2 kHz", -12f, 12f, 0f, unit = "dB"),
                ParamInfo(7, "4 kHz", -12f, 12f, 0f, unit = "dB"),
                ParamInfo(8, "8 kHz", -12f, 12f, 0f, unit = "dB"),
                ParamInfo(9, "16 kHz", -12f, 12f, 0f, unit = "dB"),
                ParamInfo(10, "Level", -12f, 12f, 0f, unit = "dB")
            )
        ),
        // Index 8: Chorus
        EffectInfo(
            index = 8,
            type = "Chorus",
            displayName = "Chorus",
            params = listOf(
                ParamInfo(0, "Rate", 0.1f, 10f, 1f, "Hz"),
                ParamInfo(1, "Depth", 0f, 1f, 0.5f),
                ParamInfo(2, "Mode", 0f, 1f, 0f, valueLabels = mapOf(0 to "Standard", 1 to "CE-1"))
            )
        ),
        // Index 9: Vibrato
        EffectInfo(
            index = 9,
            type = "Vibrato",
            displayName = "Vibrato",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Rate", 0.5f, 10f, 4f, "Hz"),
                ParamInfo(1, "Depth", 0f, 1f, 0.3f)
            )
        ),
        // Index 10: Phaser
        EffectInfo(
            index = 10,
            type = "Phaser",
            displayName = "Phaser",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Rate", 0.1f, 5f, 0.5f, "Hz"),
                ParamInfo(1, "Depth", 0f, 1f, 0.5f),
                ParamInfo(2, "Feedback", 0f, 0.9f, 0.5f),
                ParamInfo(3, "Stages", 2f, 8f, 6f),
                ParamInfo(4, "Mode", 0f, 2f, 0f, valueLabels = mapOf(0 to "Standard", 1 to "Phase 90 Script", 2 to "Phase 90 Block"))
            )
        ),
        // Index 11: Flanger
        EffectInfo(
            index = 11,
            type = "Flanger",
            displayName = "Flanger",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Rate", 0.1f, 10f, 0.5f, "Hz"),
                ParamInfo(1, "Depth", 0f, 1f, 0.5f),
                ParamInfo(2, "Feedback", -0.9f, 0.9f, 0.5f)
            )
        ),
        // Index 12: Delay
        EffectInfo(
            index = 12,
            type = "Delay",
            displayName = "Delay",
            params = listOf(
                ParamInfo(0, "Delay Time", 1f, 2000f, 300f, "ms"),
                ParamInfo(1, "Feedback", 0f, 0.95f, 0.4f)
            )
        ),
        // Index 13: Reverb (Hall of Fame 2 layout)
        // Digital mode: Dattorro plate with Type presets (Room/Hall/Church/Plate/Lo-Fi/Mod)
        // Spring mode: chirp allpass dispersion tank with Dwell drive
        // Param visibility is mode-dependent (see RackModule.kt reverb logic)
        // UI shows: Digital -> Type + Decay + Tone, Spring -> Dwell + Tone
        // Internal params (1=Damping, 2=PreDelay, 3=Size, 7=SpringDecay)
        // are auto-configured by Type presets and hidden from UI.
        EffectInfo(
            index = 13,
            type = "Reverb",
            displayName = "Reverb",
            params = listOf(
                ParamInfo(0, "Decay", 0f, 1f, 0.5f),
                ParamInfo(4, "Mode", 0f, 2f, 0f, valueLabels = mapOf(
                    0 to "Digital", 1 to "Spring 2", 2 to "Spring 3"
                )),
                ParamInfo(5, "Dwell", 0f, 1f, 0.5f),
                ParamInfo(6, "Tone", 0f, 1f, 0.5f),
                ParamInfo(8, "Type", 0f, 5f, 1f, valueLabels = mapOf(
                    0 to "Room", 1 to "Hall", 2 to "Church",
                    3 to "Plate", 4 to "Lo-Fi", 5 to "Mod"
                ))
            )
        ),
        // Index 14: Tuner (read-only)
        EffectInfo(
            index = 14,
            type = "Tuner",
            displayName = "Tuner",
            isReadOnly = true,
            params = listOf(
                ParamInfo(0, "Detected Freq", 20f, 5000f, 0f, "Hz"),
                ParamInfo(1, "Detected Note", 0f, 11f, 0f),
                ParamInfo(2, "Cents Deviation", -50f, 50f, 0f, "cents"),
                ParamInfo(3, "Confidence", 0f, 1f, 0f)
            )
        ),
        // Index 15: Tremolo (dual-mode: Standard FAUST + Vintage Optical VT-X)
        // Mode 0 (Standard): Sine/square blend AM tremolo (backward compatible)
        // Mode 1 (Vintage Optical): Guyatone VT-X tube + neon + LDR circuit model
        EffectInfo(
            index = 15,
            type = "Tremolo",
            displayName = "Tremolo",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Rate", 0.5f, 20f, 4f, "Hz"),
                ParamInfo(1, "Depth", 0f, 1f, 0.5f),
                ParamInfo(2, "Shape", 0f, 1f, 0f, valueLabels = mapOf(0 to "Sine", 1 to "Square")),
                ParamInfo(3, "Mode", 0f, 1f, 0f, valueLabels = mapOf(0 to "Standard", 1 to "Vintage Optical")),
                ParamInfo(4, "Tone", 0f, 1f, 0.75f),
                ParamInfo(5, "Intensity", 0f, 1f, 0.7f),
                ParamInfo(6, "Phase Flip", 0f, 1f, 0f, valueLabels = mapOf(0 to "Normal", 1 to "Inverted"))
            )
        ),
        // Index 16: Boss DS-1 Distortion (circuit-accurate WDF emulation)
        EffectInfo(
            index = 16,
            type = "BossDS1",
            displayName = "Boss DS-1",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Dist", 0f, 1f, 0.5f),
                ParamInfo(1, "Tone", 0f, 1f, 0.5f),
                ParamInfo(2, "Level", 0f, 1f, 0.5f)
            )
        ),
        // Index 17: ProCo RAT Distortion (circuit-accurate WDF emulation)
        EffectInfo(
            index = 17,
            type = "ProCoRAT",
            displayName = "ProCo RAT",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Distortion", 0f, 1f, 0.5f),
                ParamInfo(1, "Filter", 0f, 1f, 0.5f),
                ParamInfo(2, "Volume", 0f, 1f, 0.5f)
            )
        ),
        // Index 18: Boss DS-2 Turbo Distortion (circuit-accurate WDF emulation)
        EffectInfo(
            index = 18,
            type = "BossDS2",
            displayName = "Boss DS-2",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Dist", 0f, 1f, 0.5f),
                ParamInfo(1, "Tone", 0f, 1f, 0.5f),
                ParamInfo(2, "Level", 0f, 1f, 0.5f),
                ParamInfo(3, "Mode", 0f, 1f, 0f, valueLabels = mapOf(0 to "Mode I", 1 to "Mode II"))
            )
        ),
        // Index 19: Boss HM-2 Heavy Metal (circuit-accurate WDF emulation)
        EffectInfo(
            index = 19,
            type = "BossHM2",
            displayName = "Boss HM-2",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Dist", 0f, 1f, 0.7f),
                ParamInfo(1, "Low", 0f, 1f, 0.5f),
                ParamInfo(2, "High", 0f, 1f, 0.5f),
                ParamInfo(3, "Level", 0f, 1f, 0.5f)
            )
        ),
        // Index 20: Univibe (thermal LDR modulation emulation)
        EffectInfo(
            index = 20,
            type = "Univibe",
            displayName = "Uni-Vibe",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Speed", 0f, 1f, 0.3f),
                ParamInfo(1, "Intensity", 0f, 1f, 0.7f),
                ParamInfo(2, "Mode", 0f, 1f, 0f, valueLabels = mapOf(0 to "Chorus", 1 to "Vibrato"))
            )
        ),
        // Index 21: Fuzz Face (WDF germanium 2-transistor fuzz)
        EffectInfo(
            index = 21,
            type = "FuzzFace",
            displayName = "Fuzz Face",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Fuzz", 0f, 1f, 0.7f),
                ParamInfo(1, "Volume", 0f, 1f, 0.5f),
                ParamInfo(2, "Guitar Volume", 0f, 1f, 1f)
            )
        ),
        // Index 22: Rangemaster (WDF germanium treble booster)
        EffectInfo(
            index = 22,
            type = "Rangemaster",
            displayName = "Rangemaster",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Range", 0f, 1f, 0.5f),
                ParamInfo(1, "Volume", 0f, 1f, 0.5f)
            )
        ),
        // Index 23: Big Muff Pi (WDF 4-stage cascaded fuzz)
        EffectInfo(
            index = 23,
            type = "BigMuff",
            displayName = "Big Muff Pi",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Sustain", 0f, 1f, 0.7f),
                ParamInfo(1, "Tone", 0f, 1f, 0.5f),
                ParamInfo(2, "Volume", 0f, 1f, 0.5f),
                ParamInfo(3, "Variant", 0f, 3f, 0f, valueLabels = mapOf(
                    0 to "NYC",
                    1 to "Ram's Head",
                    2 to "Green Russian",
                    3 to "Triangle"
                ))
            )
        ),
        // Index 24: Octavia (octave-up rectifier fuzz)
        EffectInfo(
            index = 24,
            type = "Octavia",
            displayName = "Octavia",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Fuzz", 0f, 1f, 0.7f),
                ParamInfo(1, "Level", 0f, 1f, 0.5f)
            )
        ),
        // Index 25: Rotary Speaker (Leslie cabinet emulation)
        EffectInfo(
            index = 25,
            type = "RotarySpeaker",
            displayName = "Rotary Speaker",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Speed", 0f, 1f, 0f, valueLabels = mapOf(0 to "Chorale", 1 to "Tremolo")),
                ParamInfo(1, "Mix", 0f, 1f, 1f),
                ParamInfo(2, "Depth", 0f, 1f, 0.7f)
            )
        ),
        // Index 26: Fuzzrite (Mosrite Fuzzrite circuit-accurate emulation)
        EffectInfo(
            index = 26,
            type = "Fuzzrite",
            displayName = "Fuzzrite",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Depth", 0f, 1f, 0.5f),
                ParamInfo(1, "Volume", 0f, 1f, 0.7f),
                ParamInfo(2, "Variant", 0f, 1f, 0f, valueLabels = mapOf(0 to "Germanium", 1 to "Silicon"))
            )
        ),
        // Index 27: MF-102 Ring Modulator (Moog OTA four-quadrant multiplier)
        EffectInfo(
            index = 27,
            type = "MF102RingMod",
            displayName = "MF-102 Ring Mod",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Mix", 0f, 1f, 1f),
                ParamInfo(1, "Frequency", 0f, 1f, 0.5f),
                ParamInfo(2, "LFO Rate", 0f, 1f, 0.3f),
                ParamInfo(3, "LFO Amount", 0f, 1f, 0f),
                ParamInfo(4, "Drive", 0f, 1f, 0.3f),
                ParamInfo(5, "Range", 0f, 1f, 1f, valueLabels = mapOf(0 to "LO", 1 to "HI")),
                ParamInfo(6, "LFO Wave", 0f, 1f, 0f, valueLabels = mapOf(0 to "Sine", 1 to "Square"))
            )
        ),
        // Index 28: Space Echo (Roland RE-201 tape echo + spring reverb emulation)
        EffectInfo(
            index = 28,
            type = "SpaceEcho",
            displayName = "Space Echo",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Repeat Rate", 0f, 1f, 0.5f),
                ParamInfo(1, "Echo Volume", 0f, 1f, 0.5f),
                ParamInfo(2, "Intensity", 0f, 1f, 0.4f),
                ParamInfo(3, "Mode", 0f, 11f, 0f, valueLabels = mapOf(
                    0 to "Mode 1", 1 to "Mode 2", 2 to "Mode 3",
                    3 to "Mode 4", 4 to "Mode 5", 5 to "Mode 6",
                    6 to "Mode 7", 7 to "Mode 8", 8 to "Mode 9",
                    9 to "Mode 10", 10 to "Mode 11", 11 to "Mode 12"
                )),
                ParamInfo(4, "Bass", 0f, 1f, 0.5f),
                ParamInfo(5, "Treble", 0f, 1f, 0.5f),
                ParamInfo(6, "Reverb Volume", 0f, 1f, 0.3f),
                ParamInfo(7, "Wow/Flutter", 0f, 1f, 0.5f),
                ParamInfo(8, "Tape Condition", 0f, 1f, 0.3f)
            )
        ),
        // Index 29: DL4 Delay Modeler (Line 6 DL4, 16 delay algorithms)
        EffectInfo(
            index = 29,
            type = "DL4Delay",
            displayName = "DL4 Delay Modeler",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Model", 0f, 15f, 0f, valueLabels = mapOf(
                    0 to "Tube Echo",
                    1 to "Tape Echo",
                    2 to "Multi-Head",
                    3 to "Sweep Echo",
                    4 to "Analog Echo",
                    5 to "Analog w/ Mod",
                    6 to "Lo Res Delay",
                    7 to "Digital Delay",
                    8 to "Digital w/ Mod",
                    9 to "Rhythm Delay",
                    10 to "Stereo Delays",
                    11 to "Ping Pong",
                    12 to "Reverse",
                    13 to "Dynamic Delay",
                    14 to "Auto-Volume",
                    15 to "Loop Sampler"
                )),
                ParamInfo(1, "Delay Time", 0f, 1f, 0.5f),
                ParamInfo(2, "Repeats", 0f, 1f, 0.4f),
                ParamInfo(3, "Tweak", 0f, 1f, 0.5f),
                ParamInfo(4, "Tweez", 0f, 1f, 0.5f),
                ParamInfo(5, "Mix", 0f, 1f, 0.5f)
            )
        ),
        // Index 30: Boss BD-2 Blues Driver (discrete JFET overdrive)
        EffectInfo(
            index = 30,
            type = "BossBD2",
            displayName = "Boss BD-2",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Gain", 0f, 1f, 0.5f),
                ParamInfo(1, "Tone", 0f, 1f, 0.5f),
                ParamInfo(2, "Level", 0f, 1f, 0.5f)
            )
        ),
        // Index 31: Tube Screamer (TS-808/TS9 circuit-accurate overdrive)
        EffectInfo(
            index = 31,
            type = "TubeScreamer",
            displayName = "Tube Screamer",
            enabledByDefault = false,
            params = listOf(
                ParamInfo(0, "Drive", 0f, 1f, 0.5f),
                ParamInfo(1, "Tone", 0f, 1f, 0.5f),
                ParamInfo(2, "Level", 0f, 1f, 0.5f),
                ParamInfo(3, "Mode", 0f, 1f, 0f, valueLabels = mapOf(
                    0 to "TS-808",
                    1 to "TS9"
                ))
            )
        )
    )

    /**
     * Look up an effect's info by its chain index.
     * @param index Chain index (0..31).
     * @return The [EffectInfo] or null if the index is out of range.
     */
    fun getEffectInfo(index: Int): EffectInfo? = effects.getOrNull(index)

    /**
     * Get the index of an effect by its type string.
     * @param type C++ class name (e.g., "Overdrive").
     * @return The chain index or -1 if not found.
     */
    fun getIndexForType(type: String): Int =
        effects.indexOfFirst { it.type == type }
}
