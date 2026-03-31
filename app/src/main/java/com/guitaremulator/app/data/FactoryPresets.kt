package com.guitaremulator.app.data

/**
 * Factory preset definitions and effect chain builder.
 *
 * Contains all built-in presets that ship with the app, plus the
 * [buildEffectChain] helper used to construct effect state lists
 * from override maps.
 */
internal object FactoryPresets {

    /**
     * Build a default effect state list where all effects use their
     * default parameter values and the specified enabled states.
     *
     * @param enabledMap Map of effect index to enabled state. Effects not in
     *     the map use the registry's default enabled state.
     * @param paramOverrides Map of (effectIndex, paramId) to override value.
     * @param wetDryOverrides Map of effect index to wet/dry mix override.
     * @return List of [EffectState] entries (one per registered effect).
     */
    fun buildEffectChain(
        enabledMap: Map<Int, Boolean> = emptyMap(),
        paramOverrides: Map<Pair<Int, Int>, Float> = emptyMap(),
        wetDryOverrides: Map<Int, Float> = emptyMap()
    ): List<EffectState> {
        return EffectRegistry.effects.map { info ->
            val params = mutableMapOf<Int, Float>()
            for (paramInfo in info.params) {
                val key = Pair(info.index, paramInfo.id)
                params[paramInfo.id] = paramOverrides[key] ?: paramInfo.defaultValue
            }

            EffectState(
                effectType = info.type,
                enabled = enabledMap[info.index] ?: info.enabledByDefault,
                wetDryMix = wetDryOverrides[info.index] ?: 1.0f,
                parameters = params
            )
        }
    }

    /**
     * Create the list of all built-in factory presets.
     *
     * Uses deterministic UUIDs so that factory presets have stable IDs
     * across installations and updates.
     *
     * @return List of 53 factory presets with realistic parameter values.
     */
    fun getAll(): List<Preset> {
        return listOf(
            createCleanSparkle(),
            createClassicCrunch(),
            createHeavyMetal(),
            createAmbientWash(),
            createAcousticSim(),
            createDS1Crunch(),
            createRATMetal(),
            createVibeWash(),
            createClaptonWomanTone(),
            createJackWhite7NA(),
            createFruscianteUTB(),
            createVoxChime(),
            createHendrixPurpleHaze(),
            createHendrixVoodooChild(),
            createGilmourComfortablyNumb(),
            createOctaviaFuzz(),
            createEVHBrownSound(),
            createFruscianteDaniCalifornia(),
            createGilmourLeslie(),
            createHendrixMachineGunRhythm(),
            createHendrixMachineGunLead(),
            createHendrixMachineGunNeckSolo(),
            createHendrixChangesBOG(),
            createFruscianteLaCigale(),
            createFruscianteSlane(),
            createGreenManalishi(),
            createGreenAlbatross(),
            createGreenBlues(),
            createLovelessWash(),
            createChickenPickin(),
            createReggaeSkank(),
            createDottedDelay(),
            createIronButterflyIAGDV(),
            createIronButterflyTermination(),
            createFruscianteFuzzrite(),
            // MF-102 Ring Modulator presets
            createRingModWarmShimmer(),
            createRingModHarmonicTremolo(),
            createRingModBellTones(),
            createRingModRobotFactory(),
            createRingModRadioTransmission(),
            createRingModFunkClav(),
            createRingModDeepSpace(),
            createRingModOctaveUp(),
            createRingModBrokenAmp(),
            // Tube Screamer presets
            createTSClassicGreen(),
            createTSEdgeOfBreakup(),
            createTSTexasFlood(),
            createTSMidBoost(),
            createTS9Bright(),
            createTS9Lead(),
            createTSStack(),
            createTSWarmNeck(),
            createTSFrusciante()
        )
    }

    // =====================================================================
    // Individual factory preset definitions
    // =====================================================================

    /**
     * Factory Preset 1: "Clean Sparkle"
     * NoiseGate + light compression + clean amp model + reverb.
     * A pristine, open tone with shimmer from the reverb tail.
     */
    private fun createCleanSparkle(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to true,   // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (clean)
            6 to false,  // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Noise Gate: moderate threshold, fast attack/release
            Pair(0, 0) to -45f,   // threshold
            Pair(0, 1) to 6f,     // hysteresis
            Pair(0, 2) to 1f,     // attack
            Pair(0, 3) to 30f,    // release
            // Compressor: gentle squeeze
            Pair(1, 0) to -18f,   // threshold
            Pair(1, 1) to 3f,     // ratio
            Pair(1, 2) to 15f,    // attack
            Pair(1, 3) to 150f,   // release
            Pair(1, 4) to 3f,     // makeupGain
            Pair(1, 5) to 6f,     // kneeWidth
            // AmpModel: clean, moderate gain
            Pair(5, 0) to 0.3f,   // inputGain
            Pair(5, 1) to 0.6f,   // outputLevel
            Pair(5, 2) to 0f,     // modelType (clean)
            Pair(5, 3) to 0f,     // variac (off)
            // Reverb: Hall type, medium decay
            Pair(13, 8) to 1f,    // type (Hall)
            Pair(13, 0) to 0.70f, // decay (normalized)
            Pair(13, 6) to 0.6f   // tone (moderately bright)
        )

        val wetDry = mapOf(13 to 0.35f) // Reverb slightly blended

        return Preset(
            id = "factory-clean-sparkle",
            name = "Clean Sparkle",
            author = "Factory",
            category = PresetCategory.CLEAN,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * Factory Preset 2: "Classic Crunch"
     * NoiseGate + compressor + overdrive (mid drive) + delay + reverb.
     * A warm, crunchy rhythm tone reminiscent of classic rock.
     */
    private fun createClassicCrunch(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to true,   // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to true,   // Overdrive
            5 to false,  // AmpModel
            6 to false,  // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to true,  // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Noise Gate
            Pair(0, 0) to -42f,
            Pair(0, 1) to 6f,
            Pair(0, 2) to 1f,
            Pair(0, 3) to 40f,
            // Compressor: tighter squeeze for sustain
            Pair(1, 0) to -22f,
            Pair(1, 1) to 5f,
            Pair(1, 2) to 8f,
            Pair(1, 3) to 120f,
            Pair(1, 4) to 4f,
            Pair(1, 5) to 4f,
            // Overdrive: medium crunch
            Pair(4, 0) to 0.45f,  // drive
            Pair(4, 1) to 0.55f,  // tone
            Pair(4, 2) to 0.65f,  // level
            // Delay: slapback
            Pair(12, 0) to 280f,  // delayTimeMs
            Pair(12, 1) to 0.25f, // feedback
            // Reverb: Room type, medium decay
            Pair(13, 8) to 0f,    // type (Room)
            Pair(13, 0) to 0.59f, // decay (normalized)
            Pair(13, 6) to 0.5f   // tone
        )

        val wetDry = mapOf(
            12 to 0.3f,  // Delay blended
            13 to 0.25f  // Reverb subtle
        )

        return Preset(
            id = "factory-classic-crunch",
            name = "Classic Crunch",
            author = "Factory",
            category = PresetCategory.CRUNCH,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * Factory Preset 3: "Heavy Metal"
     * NoiseGate + compressor + overdrive (high) + amp (high-gain) + cabinet + delay + reverb.
     * A tight, aggressive high-gain tone for metal.
     */
    private fun createHeavyMetal(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate (essential for high gain)
            1 to true,   // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to true,   // Overdrive (boosted)
            5 to true,   // AmpModel (high gain)
            6 to true,   // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to true,  // Delay (subtle)
            13 to true,  // Reverb (tight)
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Noise Gate: tight gate for high gain
            Pair(0, 0) to -35f,   // threshold (higher to clamp noise)
            Pair(0, 1) to 8f,     // hysteresis
            Pair(0, 2) to 0.5f,   // attack (fast)
            Pair(0, 3) to 25f,    // release (fast)
            // Compressor: aggressive
            Pair(1, 0) to -25f,
            Pair(1, 1) to 6f,
            Pair(1, 2) to 5f,
            Pair(1, 3) to 80f,
            Pair(1, 4) to 6f,
            Pair(1, 5) to 3f,
            // Overdrive: high gain boost
            Pair(4, 0) to 0.85f,  // drive (high)
            Pair(4, 1) to 0.4f,   // tone (slightly dark)
            Pair(4, 2) to 0.6f,   // level
            // AmpModel: high-gain model
            Pair(5, 0) to 0.8f,   // inputGain
            Pair(5, 1) to 0.55f,  // outputLevel
            Pair(5, 2) to 2f,     // modelType (high-gain)
            Pair(5, 3) to 0f,     // variac (off)
            // CabinetSim: 4x12 British (type 1)
            Pair(6, 0) to 1f,     // cabinetType (1 = 4x12 British)
            Pair(6, 1) to 1f,     // mix (full)
            // Delay: short, subtle
            Pair(12, 0) to 150f,
            Pair(12, 1) to 0.15f,
            // Reverb: Room type
            Pair(13, 8) to 0f,    // type (Room)
            Pair(13, 0) to 0.45f, // decay (normalized)
            Pair(13, 6) to 0.3f   // tone
        )

        val wetDry = mapOf(
            12 to 0.2f,
            13 to 0.15f
        )

        return Preset(
            id = "factory-heavy-metal",
            name = "Heavy Metal",
            author = "Factory",
            category = PresetCategory.HEAVY,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * Factory Preset 4: "Ambient Wash"
     * Chorus + phaser + long delay + heavy reverb.
     * Ethereal, spacious soundscape for ambient guitar.
     */
    private fun createAmbientWash(): Preset {
        val enabled = mapOf(
            0 to false,  // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to false,  // AmpModel
            6 to false,  // CabinetSim
            7 to false,  // ParametricEQ
            8 to true,   // Chorus
            9 to false,  // Vibrato
            10 to true,  // Phaser
            11 to false, // Flanger
            12 to true,  // Delay (long)
            13 to true,  // Reverb (heavy)
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Chorus: slow, deep modulation
            Pair(8, 0) to 0.5f,   // rate
            Pair(8, 1) to 0.7f,   // depth
            // Phaser: slow sweep
            Pair(10, 0) to 0.3f,  // rate
            Pair(10, 1) to 0.6f,  // depth
            Pair(10, 2) to 0.4f,  // feedback
            Pair(10, 3) to 6f,    // stages
            // Delay: long atmospheric delay
            Pair(12, 0) to 750f,  // delayTimeMs
            Pair(12, 1) to 0.55f, // feedback (lots of repeats)
            // Reverb: Church type
            Pair(13, 8) to 2f,    // type (Church)
            Pair(13, 0) to 0.89f, // decay (normalized)
            Pair(13, 6) to 0.7f   // tone
        )

        val wetDry = mapOf(
            8 to 0.5f,   // Chorus blended
            10 to 0.4f,  // Phaser subtle
            12 to 0.5f,  // Delay prominent
            13 to 0.6f   // Reverb heavy
        )

        return Preset(
            id = "factory-ambient-wash",
            name = "Ambient Wash",
            author = "Factory",
            category = PresetCategory.AMBIENT,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * Factory Preset 5: "Acoustic Sim"
     * Compressor + EQ (cut lows, boost highs) + light reverb.
     * Simulates a bright acoustic guitar tone from an electric.
     */
    private fun createAcousticSim(): Preset {
        val enabled = mapOf(
            0 to false,  // NoiseGate
            1 to true,   // Compressor (even dynamics)
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to false,  // AmpModel
            6 to false,  // CabinetSim
            7 to true,   // ParametricEQ (shaping)
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb (room)
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Compressor: smooth, even response
            Pair(1, 0) to -15f,   // threshold
            Pair(1, 1) to 4f,     // ratio
            Pair(1, 2) to 12f,    // attack
            Pair(1, 3) to 200f,   // release
            Pair(1, 4) to 3f,     // makeupGain
            Pair(1, 5) to 6f,     // kneeWidth
            // Graphic EQ: cut lows, slight mid scoop, boost highs
            Pair(7, 2) to -6f,    // 125Hz (cut)
            Pair(7, 3) to -3f,    // 250Hz (taper)
            Pair(7, 4) to 0f,     // 500Hz (flat)
            Pair(7, 5) to -2f,    // 1kHz (slight scoop)
            Pair(7, 6) to 0f,     // 2kHz (flat)
            Pair(7, 7) to 3f,     // 4kHz (presence)
            Pair(7, 8) to 5f,     // 8kHz (brightness)
            // Reverb: Room type
            Pair(13, 8) to 0f,    // type (Room)
            Pair(13, 0) to 0.54f, // decay (normalized)
            Pair(13, 6) to 0.4f   // tone
        )

        val wetDry = mapOf(
            13 to 0.2f // Reverb subtle
        )

        return Preset(
            id = "factory-acoustic-sim",
            name = "Acoustic Sim",
            author = "Factory",
            category = PresetCategory.ACOUSTIC,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * Factory Preset 6: "DS-1 Crunch"
     * Boss DS-1 distortion + noise gate + reverb.
     * Classic mid-range crunch from the iconic orange pedal.
     */
    private fun createDS1Crunch(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to false,  // AmpModel
            6 to false,  // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to true,  // Boss DS-1 (featured)
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Noise Gate: moderate for distortion
            Pair(0, 0) to -40f,
            Pair(0, 1) to 6f,
            Pair(0, 2) to 1f,
            Pair(0, 3) to 35f,
            // Boss DS-1: classic rock crunch
            Pair(16, 0) to 0.55f,  // Dist (medium-high)
            Pair(16, 1) to 0.6f,   // Tone (slightly bright)
            Pair(16, 2) to 0.65f,  // Level
            // Reverb: Room type
            Pair(13, 8) to 0f,    // type (Room)
            Pair(13, 0) to 0.54f, // decay (normalized)
            Pair(13, 6) to 0.5f   // tone
        )

        val wetDry = mapOf(13 to 0.2f)

        return Preset(
            id = "factory-ds1-crunch",
            name = "DS-1 Crunch",
            author = "Factory",
            category = PresetCategory.CRUNCH,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * Factory Preset 7: "RAT Metal"
     * ProCo RAT into tight noise gate. Classic aggressive distortion.
     * Filter rolled back for thick, saturated tone.
     */
    private fun createRATMetal(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate (tight)
            1 to true,   // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to false,  // AmpModel
            6 to false,  // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to true,  // Delay (subtle slapback)
            13 to true,  // Reverb (tight)
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to true,  // ProCo RAT (featured)
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Noise Gate: tight for high gain
            Pair(0, 0) to -35f,
            Pair(0, 1) to 8f,
            Pair(0, 2) to 0.5f,
            Pair(0, 3) to 20f,
            // Compressor: sustain boost
            Pair(1, 0) to -22f,
            Pair(1, 1) to 5f,
            Pair(1, 2) to 5f,
            Pair(1, 3) to 80f,
            Pair(1, 4) to 4f,
            Pair(1, 5) to 4f,
            // ProCo RAT: aggressive, filter rolled back
            Pair(17, 0) to 0.75f,  // Distortion (high)
            Pair(17, 1) to 0.35f,  // Filter (rolled back = thick)
            Pair(17, 2) to 0.5f,   // Volume (0.5 = unity with linear 2x taper)
            // Delay: tight slapback
            Pair(12, 0) to 120f,
            Pair(12, 1) to 0.15f,
            // Reverb: Room type
            Pair(13, 8) to 0f,    // type (Room)
            Pair(13, 0) to 0.42f, // decay (normalized)
            Pair(13, 6) to 0.3f   // tone
        )

        val wetDry = mapOf(
            12 to 0.2f,
            13 to 0.15f
        )

        return Preset(
            id = "factory-rat-metal",
            name = "RAT Metal",
            author = "Factory",
            category = PresetCategory.HEAVY,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * Factory Preset 8: "Vibe Wash"
     * Uni-Vibe modulation with chorus and reverb.
     * Psychedelic, swirling tone inspired by Hendrix.
     */
    private fun createVibeWash(): Preset {
        val enabled = mapOf(
            0 to false,  // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to true,   // Overdrive (light)
            5 to false,  // AmpModel
            6 to false,  // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to true,  // Delay (atmospheric)
            13 to true,  // Reverb (lush)
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to true,  // Univibe (featured)
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Overdrive: light breakup
            Pair(4, 0) to 0.25f,
            Pair(4, 1) to 0.5f,
            Pair(4, 2) to 0.7f,
            // Univibe: slow, deep modulation
            Pair(20, 0) to 0.25f,  // Speed (slow)
            Pair(20, 1) to 0.8f,   // Intensity (deep)
            Pair(20, 2) to 0f,     // Mode (chorus)
            // Delay: atmospheric
            Pair(12, 0) to 450f,
            Pair(12, 1) to 0.35f,
            // Reverb: Hall type
            Pair(13, 8) to 1f,    // type (Hall)
            Pair(13, 0) to 0.77f, // decay (normalized)
            Pair(13, 6) to 0.65f   // tone
        )

        val wetDry = mapOf(
            12 to 0.35f,
            13 to 0.4f
        )

        return Preset(
            id = "factory-vibe-wash",
            name = "Vibe Wash",
            author = "Factory",
            category = PresetCategory.AMBIENT,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * Factory Preset 9: "Clapton Woman Tone"
     * EQ (treble rolled off) + AmpModel (Plexi) + CabinetSim (Greenback).
     * Smooth, singing sustain with the treble rolled way off, inspired by
     * Clapton's late-'60s "woman tone" on the neck pickup.
     */
    private fun createClaptonWomanTone(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Plexi)
            6 to true,   // CabinetSim (Greenback)
            7 to true,   // ParametricEQ (treble rolloff)
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to false, // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Noise Gate: moderate threshold
            Pair(0, 0) to -42f,   // threshold
            Pair(0, 1) to 6f,     // hysteresis
            Pair(0, 2) to 1f,     // attack
            Pair(0, 3) to 35f,    // release
            // Graphic EQ: treble rolled way off, slight mid boost
            Pair(7, 2) to 0f,     // 125Hz (flat)
            Pair(7, 3) to 0f,     // 250Hz (flat)
            Pair(7, 4) to 0f,     // 500Hz (flat)
            Pair(7, 5) to 2f,     // 1kHz (+2dB mid presence)
            Pair(7, 6) to 0f,     // 2kHz (flat)
            Pair(7, 7) to -8f,    // 4kHz (treble rolloff)
            Pair(7, 8) to -10f,   // 8kHz (heavy treble rolloff)
            // AmpModel: Plexi — closer to Clapton's cranked Marshall
            // Plexi preGain=5.0 with 2 cascaded stages; 0.9 keeps dynamics intact
            Pair(5, 0) to 0.9f,   // inputGain (moderate, preserves pick dynamics)
            Pair(5, 1) to 0.6f,   // outputLevel
            Pair(5, 2) to 3f,     // modelType (Plexi — Clapton's cranked Marshall)
            Pair(5, 3) to 0f,     // variac
            // CabinetSim: Greenback (type 4)
            Pair(6, 0) to 4f,     // cabinetType (Greenback)
            Pair(6, 1) to 1f      // mix (full)
        )

        return Preset(
            id = "factory-clapton-woman-tone",
            name = "Clapton Woman Tone",
            author = "Factory",
            category = PresetCategory.CRUNCH,
            effects = buildEffectChain(enabled, params),
            isFactory = true
        )
    }

    /**
     * Factory Preset 10: "Jack White Seven Nation Army"
     * Boost + AmpModel (Silvertone) + CabinetSim (Jensen C10Q).
     * Raw, lo-fi garage rock tone through the iconic plastic-cased amp.
     */
    private fun createJackWhite7NA(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to true,   // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Silvertone)
            6 to true,   // CabinetSim (Jensen C10Q)
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to false, // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Noise Gate: slightly higher threshold for raw signal
            Pair(0, 0) to -38f,   // threshold
            Pair(0, 1) to 6f,     // hysteresis
            Pair(0, 2) to 1f,     // attack
            Pair(0, 3) to 35f,    // release
            // Boost: moderate level push
            Pair(3, 0) to 6f,     // level (6dB boost)
            // AmpModel: Silvertone with moderate gain
            Pair(5, 0) to 1.2f,   // inputGain
            Pair(5, 1) to 0.65f,  // outputLevel
            Pair(5, 2) to 8f,     // modelType (Silvertone)
            Pair(5, 3) to 0f,     // variac
            // CabinetSim: Jensen C10Q (type 9)
            Pair(6, 0) to 9f,     // cabinetType (Jensen C10Q)
            Pair(6, 1) to 1f      // mix (full)
        )

        return Preset(
            id = "factory-jack-white-7na",
            name = "Jack White Seven Nation Army",
            author = "Factory",
            category = PresetCategory.CRUNCH,
            effects = buildEffectChain(enabled, params),
            isFactory = true
        )
    }

    /**
     * Factory Preset 11: "Frusciante Under the Bridge"
     * Compressor + AmpModel (Clean) + Chorus + Reverb.
     * Lush, clean tone with gentle chorus shimmer and deep reverb,
     * inspired by the intro arpeggios.
     */
    private fun createFruscianteUTB(): Preset {
        val enabled = mapOf(
            0 to false,  // NoiseGate
            1 to true,   // Compressor (gentle)
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Clean)
            6 to false,  // CabinetSim
            7 to false,  // ParametricEQ
            8 to true,   // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb (lush)
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Compressor: gentle, even dynamics
            Pair(1, 0) to -15f,   // threshold
            Pair(1, 1) to 3f,     // ratio
            Pair(1, 2) to 12f,    // attack
            Pair(1, 3) to 150f,   // release
            Pair(1, 4) to 2f,     // makeupGain
            Pair(1, 5) to 6f,     // kneeWidth
            // AmpModel: Clean with low gain for pristine tone
            Pair(5, 0) to 0.4f,   // inputGain (low, clean headroom)
            Pair(5, 1) to 0.7f,   // outputLevel
            Pair(5, 2) to 0f,     // modelType (Clean)
            Pair(5, 3) to 0f,     // variac
            // Chorus: CE-1 mode, slow, moderate depth
            Pair(8, 0) to 0.7f,   // rate (0.7Hz)
            Pair(8, 1) to 0.6f,   // depth
            Pair(8, 2) to 1f,     // mode (CE-1 Chorus Ensemble)
            // Reverb: Hall type
            Pair(13, 8) to 1f,    // type (Hall)
            Pair(13, 0) to 0.74f, // decay (normalized)
            Pair(13, 6) to 0.7f   // tone
        )

        val wetDry = mapOf(
            8 to 0.45f,  // Chorus blended
            13 to 0.4f   // Reverb prominent
        )

        return Preset(
            id = "factory-frusciante-utb",
            name = "Frusciante Under the Bridge",
            author = "Factory",
            category = PresetCategory.CLEAN,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * Factory Preset 12: "Vox Chime"
     * AmpModel (AC30) + CabinetSim (Alnico Blue) + Tremolo + Reverb.
     * Chimey, jangly British tone with gentle tremolo pulse,
     * inspired by Beatles/Brian May-era Vox tones.
     */
    private fun createVoxChime(): Preset {
        val enabled = mapOf(
            0 to false,  // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (AC30)
            6 to true,   // CabinetSim (Alnico Blue)
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to true,  // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // AmpModel: AC30, moderate gain for chime
            Pair(5, 0) to 0.8f,   // inputGain
            Pair(5, 1) to 0.65f,  // outputLevel
            Pair(5, 2) to 6f,     // modelType (AC30)
            Pair(5, 3) to 0f,     // variac
            // CabinetSim: Alnico Blue (type 8)
            Pair(6, 0) to 8f,     // cabinetType (Alnico Blue)
            Pair(6, 1) to 1f,     // mix (full)
            // Tremolo: gentle sine pulse
            Pair(15, 0) to 4f,    // rate (4Hz)
            Pair(15, 1) to 0.4f,  // depth (subtle)
            Pair(15, 2) to 0f,    // shape (sine)
            // Reverb: Hall type
            Pair(13, 8) to 1f,    // type (Hall)
            Pair(13, 0) to 0.65f, // decay (normalized)
            Pair(13, 6) to 0.6f   // tone
        )

        val wetDry = mapOf(
            13 to 0.3f  // Reverb blended
        )

        return Preset(
            id = "factory-vox-chime",
            name = "Vox Chime",
            author = "Factory",
            category = PresetCategory.CLEAN,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * Factory Preset 13: "Hendrix Purple Haze"
     * Fuzz Face + AmpModel (Plexi) + CabinetSim (G12H-30).
     * Thick, sustaining fuzz tone with germanium warmth,
     * inspired by Hendrix's iconic psychedelic anthem.
     */
    private fun createHendrixPurpleHaze(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Plexi)
            6 to true,   // CabinetSim (G12H-30)
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to false, // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to true,  // Fuzz Face (featured)
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Noise Gate: moderate for fuzz
            Pair(0, 0) to -38f,   // threshold
            Pair(0, 1) to 6f,     // hysteresis
            Pair(0, 2) to 1f,     // attack
            Pair(0, 3) to 35f,    // release
            // Fuzz Face: high fuzz for thick sustain
            // Param order: 0=fuzz, 1=volume, 2=guitarVolume
            Pair(21, 0) to 0.8f,  // Fuzz (high)
            Pair(21, 1) to 0.7f,  // Volume
            Pair(21, 2) to 1.0f,  // Guitar Volume (full)
            // AmpModel: Plexi — post-fuzz needs lower inputGain to preserve dynamics
            // Plexi preGain=5.0; at 0.6 total drive=3.0x, keeps fuzz character intact
            Pair(5, 0) to 0.6f,   // inputGain (moderate post-fuzz)
            Pair(5, 1) to 0.6f,   // outputLevel
            Pair(5, 2) to 3f,     // modelType (Plexi)
            Pair(5, 3) to 0f,     // variac
            // CabinetSim: G12H-30 (type 5) — Hendrix's preferred Celestion
            Pair(6, 0) to 5f,     // cabinetType (G12H-30)
            Pair(6, 1) to 1f      // mix (full)
        )

        return Preset(
            id = "factory-hendrix-purple-haze",
            name = "Hendrix Purple Haze",
            author = "Factory",
            category = PresetCategory.HEAVY,
            effects = buildEffectChain(enabled, params),
            isFactory = true
        )
    }

    /**
     * Factory Preset 14: "Hendrix Voodoo Child"
     * Wah (CryBaby, auto) + Fuzz Face + AmpModel (Plexi) + CabinetSim (G12H-30).
     * Wah before fuzz is the key — the "talking" quality of the auto-wah
     * interacts with germanium fuzz for that classic Hendrix expression.
     */
    private fun createHendrixVoodooChild(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to true,   // Wah (auto-wah for the "talking" quality)
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Plexi)
            6 to true,   // CabinetSim (G12H-30)
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay (disabled — dry rig, no delay)
            13 to false, // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to true,  // Fuzz Face (featured)
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Noise Gate: moderate for fuzz
            Pair(0, 0) to -38f,
            Pair(0, 1) to 6f,
            Pair(0, 2) to 1f,
            Pair(0, 3) to 35f,
            // Wah: CryBaby auto-wah with high sensitivity
            Pair(2, 0) to 0.7f,   // Sensitivity (high for expression)
            Pair(2, 1) to 800f,   // Frequency
            Pair(2, 2) to 4f,     // Resonance (punchy)
            Pair(2, 3) to 0f,     // Mode (Auto)
            Pair(2, 4) to 1f,     // wahType (CryBaby)
            // Fuzz Face: moderate fuzz, slightly cleaner
            Pair(21, 0) to 0.7f,  // Fuzz
            Pair(21, 1) to 0.65f, // Volume
            Pair(21, 2) to 1.0f,  // Guitar Volume (full)
            // AmpModel: Plexi — post-fuzz needs lower inputGain to preserve dynamics
            // Plexi preGain=5.0; at 0.6 total drive=3.0x
            Pair(5, 0) to 0.6f,   // inputGain (moderate post-fuzz)
            Pair(5, 1) to 0.6f,   // outputLevel
            Pair(5, 2) to 3f,     // modelType (Plexi)
            Pair(5, 3) to 0f,     // variac
            // CabinetSim: G12H-30 (type 5) — Hendrix's preferred Celestion
            Pair(6, 0) to 5f,     // cabinetType (G12H-30)
            Pair(6, 1) to 1f      // mix
        )

        return Preset(
            id = "factory-hendrix-voodoo-child",
            name = "Hendrix Voodoo Child",
            author = "Factory",
            category = PresetCategory.HEAVY,
            effects = buildEffectChain(enabled, params),
            isFactory = true
        )
    }

    /**
     * Factory Preset 15: "Gilmour Comfortably Numb"
     * Big Muff (Ram's Head) + AmpModel (Hiwatt) + CabinetSim (Fane Crescendo) + Delay + Reverb.
     * Soaring, sustaining lead tone with lush ambience,
     * inspired by Gilmour's iconic solos.
     */
    private fun createGilmourComfortablyNumb(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to true,   // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Hiwatt)
            6 to true,   // CabinetSim (Fane Crescendo)
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to true,  // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to true,  // Big Muff Pi (featured)
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Noise Gate: moderate
            Pair(0, 0) to -40f,   // threshold
            Pair(0, 1) to 6f,     // hysteresis
            Pair(0, 2) to 1f,     // attack
            Pair(0, 3) to 35f,    // release
            // Compressor: moderate sustain boost, OTA character for musical squash
            Pair(1, 0) to -20f,   // threshold
            Pair(1, 1) to 4f,     // ratio
            Pair(1, 2) to 10f,    // attack
            Pair(1, 3) to 200f,   // release
            Pair(1, 4) to 6f,     // makeupGain
            Pair(1, 5) to 0.5f,   // kneeWidth
            Pair(1, 6) to 1f,     // character (OTA — musical, Gilmour-era Ross style)
            // Big Muff Pi: Ram's Head variant, soaring sustain
            Pair(23, 0) to 0.7f,  // Sustain
            Pair(23, 1) to 0.6f,  // Tone
            Pair(23, 2) to 0.5f,  // Volume
            Pair(23, 3) to 1f,    // Variant (Ram's Head)
            // AmpModel: Hiwatt, moderate gain for clarity
            Pair(5, 0) to 0.9f,   // inputGain
            Pair(5, 1) to 0.65f,  // outputLevel
            Pair(5, 2) to 4f,     // modelType (Hiwatt)
            Pair(5, 3) to 0f,     // variac
            // CabinetSim: Fane Crescendo (type 7)
            Pair(6, 0) to 7f,     // cabinetType (Fane Crescendo)
            Pair(6, 1) to 1f,     // mix (full)
            // Delay: 430ms for spacious repeats
            Pair(12, 0) to 430f,  // delayTimeMs
            Pair(12, 1) to 0.3f,  // feedback
            // Reverb: Hall type
            Pair(13, 8) to 1f,    // type (Hall)
            Pair(13, 0) to 0.74f, // decay (normalized)
            Pair(13, 6) to 0.7f   // tone
        )

        val wetDry = mapOf(
            12 to 0.3f,  // Delay blended
            13 to 0.35f  // Reverb prominent
        )

        return Preset(
            id = "factory-gilmour-comfortably-numb",
            name = "Gilmour Comfortably Numb",
            author = "Factory",
            category = PresetCategory.HEAVY,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * Factory Preset 16: "Octavia Fuzz"
     * Octavia + AmpModel (Plexi) + CabinetSim (G12H-30).
     * Octave-up rectifier fuzz for wild, harmonically rich lead tones.
     */
    private fun createOctaviaFuzz(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Plexi)
            6 to true,   // CabinetSim (G12H-30)
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to false, // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to true,  // Octavia (featured)
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Noise Gate: moderate for fuzz
            Pair(0, 0) to -38f,   // threshold
            Pair(0, 1) to 6f,     // hysteresis
            Pair(0, 2) to 1f,     // attack
            Pair(0, 3) to 35f,    // release
            // Octavia: octave-up fuzz
            Pair(24, 0) to 0.7f,  // Fuzz
            Pair(24, 1) to 0.6f,  // Level
            // AmpModel: Plexi — post-fuzz needs lower inputGain to preserve dynamics
            // Plexi preGain=5.0; at 0.6 total drive=3.0x
            Pair(5, 0) to 0.6f,   // inputGain (moderate post-fuzz)
            Pair(5, 1) to 0.6f,   // outputLevel
            Pair(5, 2) to 3f,     // modelType (Plexi)
            Pair(5, 3) to 0f,     // variac
            // CabinetSim: G12H-30 (type 5) — Hendrix's preferred Celestion
            Pair(6, 0) to 5f,     // cabinetType (G12H-30)
            Pair(6, 1) to 1f      // mix (full)
        )

        return Preset(
            id = "factory-octavia-fuzz",
            name = "Octavia Fuzz",
            author = "Factory",
            category = PresetCategory.HEAVY,
            effects = buildEffectChain(enabled, params),
            isFactory = true
        )
    }

    /**
     * Factory Preset 17: "EVH Brown Sound"
     * Phaser (Phase 90 Script) + Boost (EP-3) + AmpModel (Plexi, Variac sag) + CabinetSim (G12-65) + NoiseGate.
     * The iconic late-'70s Van Halen tone: a hot-rodded Plexi with variac sag,
     * a Phase 90 Script mod for subtle swirl, and an EP-3 preamp boost coloring the signal.
     */
    private fun createEVHBrownSound(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to true,   // Boost (EP-3)
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Plexi + Variac)
            6 to true,   // CabinetSim (G12-65)
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to true,  // Phaser (Phase 90 Script)
            11 to false, // Flanger
            12 to false, // Delay
            13 to false, // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Noise Gate: moderate threshold for high-gain cleanup
            Pair(0, 0) to -38f,   // threshold
            Pair(0, 1) to 6f,     // hysteresis
            Pair(0, 2) to 1f,     // attack
            Pair(0, 3) to 35f,    // release
            // Boost: EP-3 preamp coloration at 8dB push
            Pair(3, 0) to 8f,     // level (8dB)
            Pair(3, 1) to 1f,     // mode (EP-3)
            // AmpModel: Plexi cranked with Variac sag for brown sound compression
            // Plexi preGain=5.0; at 0.7 total drive=3.5x, Variac reduces further
            Pair(5, 0) to 0.7f,   // inputGain (EP-3 boost provides extra drive)
            Pair(5, 1) to 0.6f,   // outputLevel
            Pair(5, 2) to 3f,     // modelType (Plexi)
            Pair(5, 3) to 0.5f,   // variac (50% for brown sound sag)
            // CabinetSim: G12-65 (type 6) — tight, neutral response for EVH clarity
            Pair(6, 0) to 6f,     // cabinetType (G12-65)
            Pair(6, 1) to 1f,     // mix (full)
            // Phaser: slow Phase 90 Script mod for subtle swirl
            Pair(10, 0) to 0.3f,  // rate (0.3Hz — barely perceptible swirl)
            Pair(10, 1) to 0.5f,  // depth
            Pair(10, 2) to 0.0f,  // feedback (ignored in Phase 90 mode)
            Pair(10, 3) to 4f,    // stages (ignored in Phase 90 mode)
            Pair(10, 4) to 1f     // mode (Phase 90 Script)
        )

        return Preset(
            id = "factory-evh-brown-sound",
            name = "EVH Brown Sound",
            author = "Factory",
            category = PresetCategory.HEAVY,
            effects = buildEffectChain(enabled, params),
            isFactory = true
        )
    }

    /**
     * Factory Preset 18: "Frusciante Dani California"
     * DS-2 (Mode II) + AmpModel (Silver Jubilee) + CabinetSim (Greenback) + Delay + NoiseGate.
     * Frusciante's aggressive lead tone: the DS-2 in Mode II provides the essential
     * 900Hz mid-boost that cuts through the mix, into a Silver Jubilee for British crunch.
     */
    private fun createFruscianteDaniCalifornia(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Silver Jubilee)
            6 to true,   // CabinetSim (Greenback)
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to true,  // Delay
            13 to false, // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to true,  // Boss DS-2 (featured, Mode II)
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Noise Gate: moderate threshold
            Pair(0, 0) to -38f,   // threshold
            Pair(0, 1) to 6f,     // hysteresis
            Pair(0, 2) to 1f,     // attack
            Pair(0, 3) to 35f,    // release
            // Boss DS-2: Mode II for essential mid-boost that cuts through
            Pair(18, 0) to 0.6f,  // Dist (60%)
            Pair(18, 1) to 0.7f,  // Tone (70%)
            Pair(18, 2) to 0.7f,  // Level (70%)
            Pair(18, 3) to 1f,    // Mode (Mode II)
            // AmpModel: Silver Jubilee for British crunch
            // Jubilee preGain=6.0 with cascaded stages; 0.5 keeps dynamics with DS-2 driving it
            Pair(5, 0) to 0.5f,   // inputGain (moderate, DS-2 provides main drive)
            Pair(5, 1) to 0.6f,   // outputLevel
            Pair(5, 2) to 5f,     // modelType (Silver Jubilee)
            Pair(5, 3) to 0f,     // variac (off)
            // CabinetSim: Greenback (type 4)
            Pair(6, 0) to 4f,     // cabinetType (Greenback)
            Pair(6, 1) to 1f,     // mix (full)
            // Delay: 300ms moderate feedback
            Pair(12, 0) to 300f,  // delayTimeMs (300ms)
            Pair(12, 1) to 0.2f   // feedback (moderate)
        )

        val wetDry = mapOf(
            12 to 0.25f  // Delay subtle blend
        )

        return Preset(
            id = "factory-frusciante-dani-california",
            name = "Frusciante Dani California",
            author = "Factory",
            category = PresetCategory.HEAVY,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * Factory Preset 19: "Gilmour Leslie"
     * Compressor + Big Muff (Ram's Head) + AmpModel (Hiwatt) + Cab (Fane) + Rotary Speaker + Delay + Reverb.
     * Soaring lead tone with lush rotary speaker modulation,
     * inspired by Gilmour's psychedelic era Leslie experiments.
     */
    private fun createGilmourLeslie(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to true,   // Compressor
            2 to false, 3 to false, 4 to false,
            5 to true,   // AmpModel (Hiwatt)
            6 to true,   // CabinetSim (Fane)
            7 to false, 8 to false, 9 to false,
            10 to false, 11 to false,
            12 to true,  // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, 16 to false, 17 to false, 18 to false, 19 to false, 20 to false,
            21 to false, 22 to false,
            23 to true,  // Big Muff (Ram's Head)
            24 to false,
            25 to true,  // Rotary Speaker (featured)
            26 to false  // Fuzzrite
        )
        val params = mapOf(
            // Noise Gate
            Pair(0, 0) to -40f, Pair(0, 1) to 6f, Pair(0, 2) to 1f, Pair(0, 3) to 35f,
            // Compressor (OTA character for musical Gilmour-era Ross style)
            Pair(1, 0) to -20f, Pair(1, 1) to 4f, Pair(1, 2) to 10f, Pair(1, 3) to 200f, Pair(1, 4) to 6f, Pair(1, 5) to 0.5f, Pair(1, 6) to 1f,
            // Big Muff: Ram's Head
            Pair(23, 0) to 0.7f, Pair(23, 1) to 0.6f, Pair(23, 2) to 0.5f, Pair(23, 3) to 1f,
            // AmpModel: Hiwatt
            Pair(5, 0) to 0.9f, Pair(5, 1) to 0.65f, Pair(5, 2) to 4f, Pair(5, 3) to 0f,
            // CabinetSim: Fane Crescendo
            Pair(6, 0) to 7f, Pair(6, 1) to 1f,
            // Rotary Speaker: slow mode, full mix, moderate depth
            Pair(25, 0) to 0f, Pair(25, 1) to 0.8f, Pair(25, 2) to 0.7f,
            // Delay: 430ms
            Pair(12, 0) to 430f, Pair(12, 1) to 0.3f,
            // Reverb: lush hall
            Pair(13, 8) to 1f, Pair(13, 0) to 0.74f, Pair(13, 6) to 0.7f  // Hall
        )
        val wetDry = mapOf(12 to 0.3f, 13 to 0.35f)
        return Preset(
            id = "factory-gilmour-leslie",
            name = "Gilmour Leslie",
            author = "Factory",
            category = PresetCategory.HEAVY,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    // =================================================================
    // Band of Gypsys signal chain order (Fillmore East, Jan 1 1970).
    //
    // Confirmed by photographic evidence and Roger Mayer's accounts:
    //   Wah(2) -> Octavia(24) -> FuzzFace(21) -> Univibe(20) ->
    //   AmpModel(5) -> CabinetSim(6) -> EQ(7)
    //
    // All other effects follow in default order (disabled).
    // =================================================================
    private val bogEffectOrder = listOf(
        0,  // NoiseGate
        2,  // Wah
        24, // Octavia
        21, // FuzzFace
        20, // Univibe
        5,  // AmpModel
        6,  // CabinetSim
        7,  // ParametricEQ
        14, // Tuner
        1, 3, 4, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18, 19, 22, 23, 25
    )

    /**
     * Factory Preset 20: "Hendrix Machine Gun Rhythm"
     *
     * The iconic stuttering riff from Band of Gypsys (Fillmore East, Jan 1 1970).
     * Recreates the rhythm/riff tone: FuzzFace at moderate guitar volume (simulating
     * vol knob at 7-8 for percussive articulation), Uni-Vibe in chorus mode pulsing
     * at riff tempo, CryBaby wah parked at ~900Hz for nasal mid emphasis.
     *
     * Signal chain: Wah(parked) -> FuzzFace -> Univibe -> Plexi -> G12-65 (flat, JBL-like).
     * No Octavia on the rhythm sections.
     *
     * Amp: Plexi voicing (Marshall 1959 Super Lead, West Coast Organ mods with
     * GE 6550A cold-biased near Class B, 600V plates, 33K/500pF tone stack).
     * No Variac — Hendrix ran HIGHER voltage, not lower.
     * Cabinet: G12-65 for neutral/flat response approximating JBL D120F speakers.
     */
    private fun createHendrixMachineGunRhythm(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to true,   // Wah (parked CryBaby for nasal mid coloring)
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Plexi)
            6 to true,   // CabinetSim (G12-65 flat response)
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to false, // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to true,  // Univibe (chorus mode, central to Machine Gun)
            21 to true,  // FuzzFace (Axis Fuzz approximation)
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia (off for rhythm sections)
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Noise Gate: sensitive threshold for fuzz cleanup
            Pair(0, 0) to -50f,   // threshold (low, let sustain through)
            Pair(0, 1) to 6f,     // hysteresis
            Pair(0, 2) to 1f,     // attack
            Pair(0, 3) to 40f,    // release
            // Wah: CryBaby parked at 900Hz for nasal mid emphasis
            // Manual mode = fixed frequency position (no envelope follow)
            Pair(2, 0) to 0.5f,   // sensitivity (irrelevant in manual mode)
            Pair(2, 1) to 900f,   // frequency (parked position ~900Hz)
            Pair(2, 2) to 4f,     // resonance (moderate Q for vocal quality)
            Pair(2, 3) to 1f,     // mode (Manual = parked wah)
            Pair(2, 4) to 1f,     // wahType (Cry Baby)
            // FuzzFace: moderate settings to simulate guitar vol at 7-8
            // The real rig used full fuzz with guitar vol rolled back;
            // guitarVolume param emulates this impedance interaction
            Pair(21, 0) to 0.8f,  // fuzz (high, Axis Fuzz was near max)
            Pair(21, 1) to 0.7f,  // volume (output level)
            Pair(21, 2) to 0.75f, // guitarVolume (7-8 out of 10 for percussive articulation)
            // Univibe: chorus mode, moderate-fast speed matched to riff tempo
            // The Uni-Vibe's pulsing modulation on the fuzzed signal creates
            // the stuttering "machine gun fire" effect
            Pair(20, 0) to 0.4f,  // speed (~3-4 Hz, matched to riff pulse)
            Pair(20, 1) to 0.75f, // intensity (deep, pronounced modulation)
            Pair(20, 2) to 0f,    // mode (Chorus = dry+wet blend)
            // AmpModel: Plexi cranked (simulating 1959 Super Lead with 6550 mods)
            // Post-fuzz needs lower inputGain; Plexi preGain=5.0, at 0.6 total=3.0x
            Pair(5, 0) to 0.6f,   // inputGain (moderate post-fuzz, preserves dynamics)
            Pair(5, 1) to 0.65f,  // outputLevel
            Pair(5, 2) to 3f,     // modelType (Plexi)
            Pair(5, 3) to 0f,     // variac (OFF — Hendrix ran higher voltage)
            // CabinetSim: G12-65 for neutral/flat response
            // Approximates JBL D120F — no speaker breakup, extended highs
            Pair(6, 0) to 6f,     // cabinetType (G12-65)
            Pair(6, 1) to 1f      // mix (full)
        )

        return Preset(
            id = "factory-hendrix-machine-gun-rhythm",
            name = "Hendrix Machine Gun Rhythm",
            author = "Factory",
            category = PresetCategory.HEAVY,
            effects = buildEffectChain(enabled, params),
            isFactory = true,
            effectOrder = bogEffectOrder
        )
    }

    /**
     * Factory Preset 21: "Hendrix Machine Gun Lead"
     *
     * Full-saturation solo tone from Machine Gun's lead sections (~4:00-7:20).
     * All four BOG effects engaged: CryBaby wah (auto-sweep for expressiveness),
     * Octavia (octave-up for upper-register screams), FuzzFace at full gain,
     * Uni-Vibe throbbing underneath.
     *
     * This covers: the sustained feedback note (~3:59), blues box solo,
     * whammy bar/sound effects sections, and the "war sounds" passages.
     *
     * Guitar volume full up (1.0) — maximum sustain and saturation.
     * The wah creates the "screaming/crying" vocal quality when swept.
     * The Octavia adds octave harmonics for upper-register "whistle" tone
     * and metallic ring-mod chaos on lower notes.
     */
    private fun createHendrixMachineGunLead(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to true,   // Wah (auto-sweep for expressive lead)
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Plexi)
            6 to true,   // CabinetSim (G12-65)
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to false, // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to true,  // Univibe (chorus mode, throbbing underneath leads)
            21 to true,  // FuzzFace (full saturation)
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to true,  // Octavia (octave-up for upper register screams)
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Noise Gate: sensitive, let sustain and feedback through
            Pair(0, 0) to -55f,   // threshold (very low for sustained lead)
            Pair(0, 1) to 6f,     // hysteresis
            Pair(0, 2) to 1f,     // attack
            Pair(0, 3) to 50f,    // release (longer for sustain)
            // Wah: CryBaby in auto mode for expressive sweep
            // High sensitivity tracks picking dynamics for "talking" quality
            Pair(2, 0) to 0.7f,   // sensitivity (high for expressiveness)
            Pair(2, 1) to 800f,   // frequency (center)
            Pair(2, 2) to 4.5f,   // resonance (vocal Q)
            Pair(2, 3) to 0f,     // mode (Auto — follows playing dynamics)
            Pair(2, 4) to 1f,     // wahType (Cry Baby)
            // Octavia: octave-up fuzz for screaming upper-register harmonics
            // Moderate fuzz (the FuzzFace handles most of the saturation)
            Pair(24, 0) to 0.55f, // fuzz (moderate — FuzzFace provides main dirt)
            Pair(24, 1) to 0.6f,  // level
            // FuzzFace: FULL saturation, guitar volume at 10
            // Maximum sustain for extended notes and feedback
            Pair(21, 0) to 0.9f,  // fuzz (near max, massive sustain)
            Pair(21, 1) to 0.75f, // volume
            Pair(21, 2) to 1.0f,  // guitarVolume (FULL for maximum saturation)
            // Univibe: chorus mode, slightly slower than rhythm for solo breathing
            Pair(20, 0) to 0.35f, // speed (~3 Hz, throbbing underneath leads)
            Pair(20, 1) to 0.8f,  // intensity (deep modulation)
            Pair(20, 2) to 0f,    // mode (Chorus)
            // AmpModel: Plexi — FuzzFace+Octavia already provide massive saturation
            // Plexi preGain=5.0; at 0.5 total drive=2.5x, lets fuzz character through
            Pair(5, 0) to 0.5f,   // inputGain (low, fuzz+octavia provide all the dirt)
            Pair(5, 1) to 0.65f,  // outputLevel
            Pair(5, 2) to 3f,     // modelType (Plexi)
            Pair(5, 3) to 0f,     // variac (OFF)
            // CabinetSim: G12-65 neutral response
            Pair(6, 0) to 6f,     // cabinetType (G12-65)
            Pair(6, 1) to 1f      // mix (full)
        )

        return Preset(
            id = "factory-hendrix-machine-gun-lead",
            name = "Hendrix Machine Gun Lead",
            author = "Factory",
            category = PresetCategory.HEAVY,
            effects = buildEffectChain(enabled, params),
            isFactory = true,
            effectOrder = bogEffectOrder
        )
    }

    /**
     * Factory Preset 22: "Hendrix Machine Gun Neck Solo"
     *
     * The warm, mournful legato section starting at ~7:20 where Hendrix
     * switches from bridge to neck pickup. The tone transforms from bright
     * and cutting to warm, vocal, and singing.
     *
     * ParametricEQ simulates the neck pickup switch: high frequencies rolled
     * off (-8dB above 3.5kHz), low-mids boosted (+3dB at 500Hz). This shifts
     * the FuzzFace from "buzzy/aggressive" (bridge) to "warm/thick/vocal" (neck),
     * and makes the Univibe's throbbing dramatically more pronounced in the
     * low-mid region where the mismatched allpass stages have deepest notches.
     *
     * No wah, no Octavia — just fuzz + Univibe + the neck pickup's warmth.
     * Dense legato runs, sustained bends, and the most emotionally powerful
     * section of the entire 12-minute performance.
     */
    private fun createHendrixMachineGunNeckSolo(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah (OFF for neck section)
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Plexi)
            6 to true,   // CabinetSim (G12-65)
            7 to true,   // ParametricEQ (neck pickup simulation)
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to false, // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to true,  // Univibe (most pronounced in this section)
            21 to true,  // FuzzFace (full saturation, warm through neck sim)
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia (OFF for warm legato)
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Noise Gate: gentle, preserve legato sustain
            Pair(0, 0) to -55f,   // threshold (very low for maximum sustain)
            Pair(0, 1) to 6f,     // hysteresis
            Pair(0, 2) to 1f,     // attack
            Pair(0, 3) to 60f,    // release (long for legato)
            // ParametricEQ: simulate neck pickup tonal shift
            // Neck pickup emphasizes fundamental + low harmonics, rolls off highs.
            // The reversed Strat's neck pickup has extra warmth from reversed
            // pole piece stagger.
            // Graphic EQ: neck pickup simulation (warm low-mids, treble rolloff)
            Pair(7, 2) to 0f,     // 125Hz (flat)
            Pair(7, 3) to 2f,     // 250Hz (+2dB warmth)
            Pair(7, 4) to 3f,     // 500Hz (+3dB low-mid body)
            Pair(7, 5) to 2f,     // 1kHz (+2dB vocal character)
            Pair(7, 6) to 0f,     // 2kHz (flat)
            Pair(7, 7) to -6f,    // 4kHz (treble rolloff)
            Pair(7, 8) to -8f,    // 8kHz (neck pickup sim rolloff)
            // FuzzFace: full saturation through warm neck sim
            // Same fuzz settings but the EQ-altered input spectrum transforms
            // the distortion character from "buzzy" to "warm/vocal"
            Pair(21, 0) to 0.85f, // fuzz (high sustain)
            Pair(21, 1) to 0.7f,  // volume
            Pair(21, 2) to 1.0f,  // guitarVolume (full for legato sustain)
            // Univibe: deepest settings — most pronounced in this section
            // With the neck-sim EQ concentrating energy in the 200-800Hz range,
            // the Univibe's 15nF/220nF allpass stages create their deepest
            // notches exactly where the signal energy peaks
            Pair(20, 0) to 0.3f,  // speed (slightly slower, mournful quality)
            Pair(20, 1) to 0.85f, // intensity (maximum depth for singing quality)
            Pair(20, 2) to 0f,    // mode (Chorus)
            // AmpModel: Plexi — post-fuzz needs lower inputGain
            // Plexi preGain=5.0; at 0.6 total drive=3.0x
            Pair(5, 0) to 0.6f,   // inputGain (moderate post-fuzz)
            Pair(5, 1) to 0.65f,  // outputLevel
            Pair(5, 2) to 3f,     // modelType (Plexi)
            Pair(5, 3) to 0f,     // variac (OFF)
            // CabinetSim: G12-65 neutral response
            Pair(6, 0) to 6f,     // cabinetType (G12-65)
            Pair(6, 1) to 1f      // mix (full)
        )

        return Preset(
            id = "factory-hendrix-machine-gun-neck-solo",
            name = "Hendrix Machine Gun Neck Solo",
            author = "Factory",
            category = PresetCategory.HEAVY,
            effects = buildEffectChain(enabled, params),
            isFactory = true,
            effectOrder = bogEffectOrder
        )
    }

    // =================================================================
    // Peter Green out-of-phase pickup simulation effect order
    //
    // Green's reversed-magnet middle pickup creates phase cancellation
    // with the bridge pickup, producing a hollow, nasal tone. We simulate
    // this by placing ParametricEQ(7) BEFORE AmpModel(5) in the chain:
    //   EQ(7) -> Rangemaster(22) -> AmpModel(5) -> CabinetSim(6)
    //
    // All other effects follow in default order (disabled).
    // =================================================================
    private val greenOutOfPhaseOrder = listOf(
        0,  // NoiseGate
        7,  // ParametricEQ (out-of-phase pickup sim - MUST be before amp)
        22, // Rangemaster (treble booster)
        5,  // AmpModel
        6,  // CabinetSim
        14, // Tuner
        1, 2, 3, 4, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18, 19, 20, 21, 23, 24, 25
    )

    /**
     * Factory Preset 23: "Hendrix Changes (BOG)"
     * Jimi Hendrix "Changes" from Band of Gypsys, Fillmore East, Jan 1 1970.
     * Mellow jazzy funk — same rig as Machine Gun but everything off except subtle Univibe.
     * Plexi at low inputGain simulates guitar volume rolled to ~5 for a clean, warm tone.
     */
    private fun createHendrixChangesBOG(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Plexi at low gain = clean)
            6 to true,   // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to false, // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to true,  // Univibe (subtle chorus)
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // NoiseGate: light gating for jazz passages
            Pair(0, 0) to -50f,   // threshold (low — preserve dynamics)
            Pair(0, 1) to 6f,     // hysteresis
            Pair(0, 2) to 1f,     // attack
            Pair(0, 3) to 40f,    // release
            // AmpModel: Plexi at very low inputGain simulates guitar vol at ~5
            // Plexi preGain=5.0; inputGain=0.35 → total drive=1.75x (clean)
            Pair(5, 0) to 0.35f,  // inputGain (critical — keeps Plexi clean)
            Pair(5, 1) to 0.65f,  // outputLevel
            Pair(5, 2) to 3f,     // modelType (Plexi)
            Pair(5, 3) to 0f,     // variac (off)
            // CabinetSim: G12-65 (flat, JBL-like response for jazz clarity)
            Pair(6, 0) to 6f,     // cabinetType (G12-65)
            Pair(6, 1) to 1f,     // mix (full)
            // Univibe: very slow chorus, barely perceptible
            // Real Univibe in chorus mode at low speed adds gentle movement
            Pair(20, 0) to 0.15f, // speed (~1.5Hz, very slow swirl)
            Pair(20, 1) to 0.35f, // intensity (subtle — just adds life)
            Pair(20, 2) to 0f     // mode (Chorus)
        )

        return Preset(
            id = "factory-hendrix-changes",
            name = "Hendrix Changes (BOG)",
            author = "Factory",
            category = PresetCategory.CLEAN,
            effects = buildEffectChain(enabled, params),
            isFactory = true,
            inputGain = 0.8f,
            outputGain = 0.85f,
            effectOrder = bogEffectOrder
        )
    }

    /**
     * Factory Preset 24: "Frusciante La Cigale Jam"
     * RHCP atmospheric intro jam at La Cigale, Paris 2006.
     * Spacey, delay-heavy, CE-1 chorus shimmer, Jubilee at edge of breakup.
     * OTA compressor approximates the CE-1 preamp compression character.
     */
    private fun createFruscianteLaCigale(): Preset {
        val enabled = mapOf(
            0 to false,  // NoiseGate
            1 to true,   // Compressor (OTA — CE-1 preamp character)
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Silver Jubilee)
            6 to true,   // CabinetSim (Greenback)
            7 to false,  // ParametricEQ
            8 to true,   // Chorus (CE-1 mode)
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to true,  // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Compressor: OTA character approximates CE-1 preamp compression
            Pair(1, 0) to -18f,   // threshold
            Pair(1, 1) to 3f,     // ratio
            Pair(1, 2) to 15f,    // attack
            Pair(1, 3) to 120f,   // release
            Pair(1, 4) to 2f,     // makeupGain
            Pair(1, 5) to 8f,     // kneeWidth (soft knee for natural feel)
            Pair(1, 6) to 1f,     // character (OTA)
            // AmpModel: Silver Jubilee at edge of breakup
            // Jubilee's cascaded gain stages respond beautifully to pick dynamics
            Pair(5, 0) to 0.35f,  // inputGain (edge of breakup)
            Pair(5, 1) to 0.65f,  // outputLevel
            Pair(5, 2) to 5f,     // modelType (Silver Jubilee)
            Pair(5, 3) to 0f,     // variac (off)
            // CabinetSim: Greenback — warm, focused midrange
            Pair(6, 0) to 4f,     // cabinetType (Greenback)
            Pair(6, 1) to 1f,     // mix (full)
            // Chorus: CE-1 mode — the defining Frusciante sound
            // Real CE-1 has MN3002 BBD, triangle LFO, 5ms center/±2ms swing
            Pair(8, 0) to 0.5f,   // rate
            Pair(8, 1) to 0.45f,  // depth
            Pair(8, 2) to 1f,     // mode (CE-1)
            // Delay: long ambient wash
            Pair(12, 0) to 450f,  // delayTime (ms)
            Pair(12, 1) to 0.45f, // feedback (several repeats)
            // Reverb: Hall type
            Pair(13, 8) to 1f,    // type (Hall)
            Pair(13, 0) to 0.7f, // decay (normalized)
            Pair(13, 6) to 0.6f   // tone
        )

        val wetDry = mapOf(
            8 to 0.35f,   // Chorus — enough to shimmer without overwhelming
            12 to 0.35f,  // Delay — ambient but not washy
            13 to 0.30f   // Reverb — space without mud
        )

        return Preset(
            id = "factory-frusciante-la-cigale",
            name = "Frusciante La Cigale Jam",
            author = "Factory",
            category = PresetCategory.AMBIENT,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true,
            inputGain = 0.85f,
            outputGain = 0.8f
        )
    }

    /**
     * Factory Preset 25: "Frusciante Slane Intro"
     * RHCP Californication extended intro at Slane Castle 2003.
     * Dynamic clean-to-overdrive with DS-2 Mode II for leads.
     * Jubilee amp provides the mid-heavy foundation; DS-2 Mode II's 900Hz
     * mid boost is THE defining Frusciante lead sound.
     */
    private fun createFruscianteSlane(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to true,   // Compressor (OTA — CE-1 preamp)
            2 to false,  // Wah
            3 to true,   // Boost (Clean — MXR Micro Amp approximation)
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Silver Jubilee)
            6 to true,   // CabinetSim (G12-65)
            7 to false,  // ParametricEQ
            8 to true,   // Chorus (CE-1 mode)
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to true,  // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to true,  // Boss DS-2 (Mode II — 900Hz mid boost)
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // NoiseGate: moderate threshold
            Pair(0, 0) to -48f,   // threshold
            Pair(0, 1) to 6f,     // hysteresis
            Pair(0, 2) to 1f,     // attack
            Pair(0, 3) to 35f,    // release
            // Compressor: OTA character, CE-1 preamp approximation
            Pair(1, 0) to -15f,   // threshold
            Pair(1, 1) to 2.5f,   // ratio
            Pair(1, 2) to 12f,    // attack
            Pair(1, 3) to 130f,   // release
            Pair(1, 4) to 1.5f,   // makeupGain
            Pair(1, 5) to 8f,     // kneeWidth
            Pair(1, 6) to 1f,     // character (OTA)
            // Boost: subtle clean push (MXR Micro Amp approximation)
            Pair(3, 0) to 3f,     // level (+3dB push)
            Pair(3, 1) to 0f,     // mode (Clean)
            // AmpModel: Silver Jubilee at moderate crunch
            Pair(5, 0) to 0.55f,  // inputGain (moderate crunch)
            Pair(5, 1) to 0.65f,  // outputLevel
            Pair(5, 2) to 5f,     // modelType (Silver Jubilee)
            Pair(5, 3) to 0f,     // variac (off)
            // CabinetSim: G12-65 (neutral, lets Jubilee character through)
            Pair(6, 0) to 6f,     // cabinetType (G12-65)
            Pair(6, 1) to 1f,     // mix (full)
            // Chorus: CE-1 mode, subtler than La Cigale
            Pair(8, 0) to 0.6f,   // rate
            Pair(8, 1) to 0.3f,   // depth (subtler)
            Pair(8, 2) to 1f,     // mode (CE-1)
            // Boss DS-2: Mode II — the 900Hz mid boost IS the Frusciante lead sound
            Pair(18, 0) to 0.55f, // dist
            Pair(18, 1) to 0.6f,  // tone
            Pair(18, 2) to 0.6f,  // level
            Pair(18, 3) to 1f,    // mode (Mode II)
            // Delay: moderate repeats
            Pair(12, 0) to 380f,  // delayTime (ms)
            Pair(12, 1) to 0.3f,  // feedback
            // Reverb: Plate type
            Pair(13, 8) to 3f,    // type (Plate)
            Pair(13, 0) to 0.65f, // decay (normalized)
            Pair(13, 6) to 0.55f   // tone
        )

        val wetDry = mapOf(
            8 to 0.25f,   // Chorus — subtle support
            12 to 0.25f,  // Delay — rhythmic repeats
            13 to 0.20f   // Reverb — space without clutter
        )

        return Preset(
            id = "factory-frusciante-slane",
            name = "Frusciante Slane Intro",
            author = "Factory",
            category = PresetCategory.CRUNCH,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true,
            inputGain = 0.9f,
            outputGain = 0.8f
        )
    }

    /**
     * Factory Preset 26: "Green Manalishi"
     * Peter Green's proto-metal tone (1970). Out-of-phase reversed-magnet pickup
     * simulated via ParametricEQ placed BEFORE the amp in the signal chain.
     * Rangemaster treble booster into cranked Plexi creates the snarling,
     * aggressive tone of the original recording.
     */
    private fun createGreenManalishi(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Plexi, cranked)
            6 to true,   // CabinetSim (Greenback)
            7 to true,   // ParametricEQ (out-of-phase pickup sim)
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to false, // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to true,  // Rangemaster (OC44 treble booster)
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // NoiseGate: moderate for high-gain context
            Pair(0, 0) to -42f,   // threshold
            Pair(0, 1) to 6f,     // hysteresis
            Pair(0, 2) to 1f,     // attack
            Pair(0, 3) to 35f,    // release
            // ParametricEQ: out-of-phase reversed-magnet pickup simulation
            // Green's bridge+middle with reversed middle magnet creates:
            // - Massive bass cancellation (the two pickups are 180° out of phase in lows)
            // - Nasal midrange peak (partial cancellation creates resonant honk)
            // - Treble rolloff (high-frequency phase interaction)
            // Graphic EQ: out-of-phase pickup simulation (bass cut, nasal mid peak, treble cut)
            Pair(7, 2) to -12f,   // 125Hz (massive bass cut — phase cancellation)
            Pair(7, 3) to -10f,   // 250Hz (bass cut)
            Pair(7, 4) to -4f,    // 500Hz (taper into mid peak)
            Pair(7, 5) to 8f,     // 1kHz (nasal honk peak)
            Pair(7, 6) to 2f,     // 2kHz (upper-mid presence)
            Pair(7, 7) to -4f,    // 4kHz (treble rolloff)
            Pair(7, 8) to -6f,    // 8kHz (treble rolloff from phase interaction)
            // Rangemaster: OC44 germanium treble booster
            // Green used a Rangemaster clone to push the Plexi front end
            Pair(22, 0) to 0.6f,  // range (treble emphasis)
            Pair(22, 1) to 0.65f, // volume
            // AmpModel: Plexi cranked — the phase-cancelled signal drives it into
            // aggressive breakup that emphasizes the nasal midrange character
            Pair(5, 0) to 0.9f,   // inputGain (cranked, breaking up hard)
            Pair(5, 1) to 0.65f,  // outputLevel
            Pair(5, 2) to 3f,     // modelType (Plexi)
            Pair(5, 3) to 0f,     // variac (off)
            // CabinetSim: Greenback — warm, mid-focused, pairs with Plexi
            Pair(6, 0) to 4f,     // cabinetType (Greenback)
            Pair(6, 1) to 1f      // mix (full)
        )

        return Preset(
            id = "factory-green-manalishi",
            name = "Green Manalishi",
            author = "Factory",
            category = PresetCategory.HEAVY,
            effects = buildEffectChain(enabled, params),
            isFactory = true,
            inputGain = 1.0f,
            outputGain = 0.75f,
            effectOrder = greenOutOfPhaseOrder
        )
    }

    /**
     * Factory Preset 27: "Green Albatross"
     * Peter Green's ethereal clean instrumental (1968). Warm, liquid, sustaining
     * clean tone through a Fender Twin. Neck pickup simulated via gentle EQ warmth
     * (NOT the out-of-phase sound — this is pure neck position).
     * OTA compressor provides the sustain that makes the melody sing.
     */
    private fun createGreenAlbatross(): Preset {
        val enabled = mapOf(
            0 to false,  // NoiseGate
            1 to true,   // Compressor (OTA for sustain)
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Twin Reverb, clean)
            6 to true,   // CabinetSim (1x12 Combo)
            7 to true,   // ParametricEQ (warm neck pickup sim)
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb (spacious)
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Compressor: OTA character for smooth, singing sustain
            Pair(1, 0) to -18f,   // threshold
            Pair(1, 1) to 3f,     // ratio
            Pair(1, 2) to 10f,    // attack
            Pair(1, 3) to 150f,   // release
            Pair(1, 4) to 3f,     // makeupGain (compensate for compression)
            Pair(1, 5) to 8f,     // kneeWidth (soft knee for natural feel)
            Pair(1, 6) to 1f,     // character (OTA)
            // ParametricEQ: warm neck pickup simulation (NOT out-of-phase)
            // Gentle warmth and treble rolloff to approximate neck position
            // Graphic EQ: warm neck position (slight warmth, mid body, treble rolloff)
            Pair(7, 2) to 2f,     // 125Hz (slight warmth)
            Pair(7, 3) to 1f,     // 250Hz (warmth taper)
            Pair(7, 4) to 0f,     // 500Hz (flat)
            Pair(7, 5) to 1f,     // 1kHz (gentle body)
            Pair(7, 6) to 1.5f,   // 2kHz (midrange body)
            Pair(7, 7) to -2f,    // 4kHz (gentle treble rolloff)
            Pair(7, 8) to -3f,    // 8kHz (treble rolloff)
            // AmpModel: Twin Reverb clean — Peter Green's Fender clean tone
            Pair(5, 0) to 0.35f,  // inputGain (dead clean)
            Pair(5, 1) to 0.7f,   // outputLevel
            Pair(5, 2) to 7f,     // modelType (Twin Reverb)
            Pair(5, 3) to 0f,     // variac (off)
            // CabinetSim: 1x12 Combo — open-back warmth
            Pair(6, 0) to 0f,     // cabinetType (1x12 Combo)
            Pair(6, 1) to 1f,     // mix (full)
            // Reverb: Hall type
            Pair(13, 8) to 1f,    // type (Hall)
            Pair(13, 0) to 0.77f, // decay (normalized)
            Pair(13, 6) to 0.65f   // tone
        )

        val wetDry = mapOf(
            13 to 0.30f  // Reverb — atmospheric but not overpowering
        )

        return Preset(
            id = "factory-green-albatross",
            name = "Green Albatross",
            author = "Factory",
            category = PresetCategory.CLEAN,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true,
            inputGain = 0.85f,
            outputGain = 0.85f
        )
    }

    /**
     * Factory Preset 28: "Green Blues"
     * Peter Green's emotional blues tone ("Need Your Love So Bad", "Man of the World").
     * Out-of-phase middle position but warmer variant than Manalishi — less extreme
     * bass cut, softer mid peak. Through a slightly crunchy amp that responds to
     * picking dynamics, creating the weeping, vocal quality Green was known for.
     */
    private fun createGreenBlues(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Crunch — dynamic response)
            6 to true,   // CabinetSim (Greenback)
            7 to true,   // ParametricEQ (warm out-of-phase sim)
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // NoiseGate: gentle, preserve sustain for emotive playing
            Pair(0, 0) to -45f,   // threshold
            Pair(0, 1) to 6f,     // hysteresis
            Pair(0, 2) to 1f,     // attack
            Pair(0, 3) to 40f,    // release
            // ParametricEQ: warmer out-of-phase variant (less extreme than Manalishi)
            // The blues tone uses the same reversed-magnet middle pickup, but
            // Green's dynamics and volume control soften the effect
            // Graphic EQ: warmer out-of-phase (less extreme bass cut, mid honk, treble cut)
            Pair(7, 2) to -6f,    // 125Hz (bass cut, less extreme)
            Pair(7, 3) to -8f,    // 250Hz (bass cut)
            Pair(7, 4) to -2f,    // 500Hz (taper)
            Pair(7, 5) to 6f,     // 1kHz (mid honk, less aggressive)
            Pair(7, 6) to 1f,     // 2kHz (slight presence)
            Pair(7, 7) to -3f,    // 4kHz (gentle treble rolloff)
            Pair(7, 8) to -4f,    // 8kHz (treble rolloff)
            // AmpModel: Crunch — responsive to dynamics for expressive blues
            Pair(5, 0) to 0.65f,  // inputGain (responsive to picking dynamics)
            Pair(5, 1) to 0.65f,  // outputLevel
            Pair(5, 2) to 1f,     // modelType (Crunch)
            Pair(5, 3) to 0f,     // variac (off)
            // CabinetSim: Greenback — warm midrange character
            Pair(6, 0) to 4f,     // cabinetType (Greenback)
            Pair(6, 1) to 1f,     // mix (full)
            // Reverb: Plate type
            Pair(13, 8) to 3f,    // type (Plate)
            Pair(13, 0) to 0.65f, // decay (normalized)
            Pair(13, 6) to 0.5f   // tone
        )

        val wetDry = mapOf(
            13 to 0.20f  // Reverb — subtle room, doesn't dilute emotion
        )

        return Preset(
            id = "factory-green-blues",
            name = "Green Blues",
            author = "Factory",
            category = PresetCategory.CRUNCH,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true,
            inputGain = 0.9f,
            outputGain = 0.8f,
            effectOrder = greenOutOfPhaseOrder
        )
    }

    /**
     * Factory Preset 29: "Loveless Wash"
     * Kevin Shields/My Bloody Valentine wall-of-sound shoegaze.
     * Big Muff (Ram's Head variant) into dense modulation stack with massive
     * reverb and delay. The Muff provides woolly sustain; chorus and flanger
     * create the swirling, disorienting texture; long delay and cavernous reverb
     * complete the sonic wash.
     */
    private fun createLovelessWash(): Preset {
        val enabled = mapOf(
            0 to false,  // NoiseGate (off — sustain is the point)
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Crunch with Variac sag)
            6 to true,   // CabinetSim (4x12 British)
            7 to false,  // ParametricEQ
            8 to true,   // Chorus (Standard — wide stereo wash)
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to true,  // Flanger (slow, subtle movement)
            12 to true,  // Delay (long, many repeats)
            13 to true,  // Reverb (cavernous)
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to true,  // Big Muff Pi (Ram's Head — woolly sustain)
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Big Muff: Ram's Head variant — woolly, dark sustain
            // Shields used a reversed Jazzmaster into distortion for the glide guitar
            Pair(23, 0) to 0.85f, // sustain (high — wall of fuzz)
            Pair(23, 1) to 0.3f,  // tone (dark, woolly — the MBV characteristic)
            Pair(23, 2) to 0.6f,  // volume
            Pair(23, 3) to 1f,    // variant (Ram's Head)
            // AmpModel: Crunch with slight Variac sag for amp compression
            Pair(5, 0) to 0.5f,   // inputGain
            Pair(5, 1) to 0.6f,   // outputLevel
            Pair(5, 2) to 1f,     // modelType (Crunch)
            Pair(5, 3) to 0.3f,   // variac (slight sag — compressed feel)
            // CabinetSim: 4x12 British — big, wide response
            Pair(6, 0) to 1f,     // cabinetType (4x12 British)
            Pair(6, 1) to 1f,     // mix (full)
            // Chorus: slow, deep — creates the swirling width
            Pair(8, 0) to 0.4f,   // rate (slow)
            Pair(8, 1) to 0.8f,   // depth (deep modulation)
            Pair(8, 2) to 0f,     // mode (Standard)
            // Flanger: very slow, subtle movement underneath the chorus
            Pair(11, 0) to 0.2f,  // rate (very slow)
            Pair(11, 1) to 0.4f,  // depth
            Pair(11, 2) to 0.3f,  // feedback (gentle)
            // Delay: long time, lots of repeats — builds the wash
            Pair(12, 0) to 450f,  // delayTime (ms)
            Pair(12, 1) to 0.7f,  // feedback (many repeats, near self-oscillation)
            // Reverb: Church type
            Pair(13, 8) to 2f,    // type (Church)
            Pair(13, 0) to 0.92f, // decay (normalized)
            Pair(13, 6) to 0.4f   // tone
        )

        val wetDry = mapOf(
            23 to 1.0f,   // Big Muff — full wet (it IS the sound)
            8 to 0.5f,    // Chorus — significant presence
            11 to 0.3f,   // Flanger — subtle underlying movement
            12 to 0.45f,  // Delay — major component of the wash
            13 to 0.60f   // Reverb — dominant atmospheric element
        )

        return Preset(
            id = "factory-loveless-wash",
            name = "Loveless Wash",
            author = "Factory",
            category = PresetCategory.AMBIENT,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true,
            inputGain = 0.85f,
            outputGain = 0.7f
        )
    }

    /**
     * Factory Preset 30: "Chicken Pickin'"
     * Brad Paisley/Brian Setzer country twang. Slow-attack OTA compressor
     * lets the pick transient snap through before clamping down, creating
     * that percussive "chicken pickin'" attack. Twin Reverb provides sparkle;
     * slapback delay adds rockabilly bounce; EQ boosts the twang frequencies.
     */
    private fun createChickenPickin(): Preset {
        val enabled = mapOf(
            0 to false,  // NoiseGate
            1 to true,   // Compressor (OTA — slow attack for snap)
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Twin Reverb)
            6 to true,   // CabinetSim (Jensen C10Q)
            7 to true,   // ParametricEQ (twang sculpting)
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to true,  // Delay (slapback)
            13 to true,  // Reverb (short spring)
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Compressor: OTA with SLOW attack — critical for chicken pickin'
            // The slow attack lets the initial pick transient pass through uncompressed,
            // creating the percussive snap; then compression evens out the sustain
            Pair(1, 0) to -18f,   // threshold
            Pair(1, 1) to 4f,     // ratio
            Pair(1, 2) to 30f,    // attack (SLOW — lets pick snap through)
            Pair(1, 3) to 200f,   // release
            Pair(1, 4) to 4f,     // makeupGain (compensate for heavy compression)
            Pair(1, 5) to 4f,     // kneeWidth (harder knee for obvious effect)
            Pair(1, 6) to 1f,     // character (OTA)
            // AmpModel: Twin Reverb at edge of breakup — the country standard
            Pair(5, 0) to 0.9f,   // inputGain (edge of breakup for sparkle)
            Pair(5, 1) to 0.7f,   // outputLevel
            Pair(5, 2) to 7f,     // modelType (Twin Reverb)
            Pair(5, 3) to 0f,     // variac (off)
            // CabinetSim: Jensen C10Q — bright, articulate, THE country speaker
            Pair(6, 0) to 9f,     // cabinetType (Jensen C10Q)
            Pair(6, 1) to 1f,     // mix (full)
            // Graphic EQ: sculpt for twang
            Pair(7, 2) to -2f,    // 125Hz (slight bass cut — keeps tight)
            Pair(7, 3) to 0f,     // 250Hz (flat)
            Pair(7, 4) to 0f,     // 500Hz (flat)
            Pair(7, 5) to 0f,     // 1kHz (flat)
            Pair(7, 6) to 2f,     // 2kHz (twang presence)
            Pair(7, 7) to 3f,     // 4kHz (twang boost)
            Pair(7, 8) to 2f,     // 8kHz (sparkle)
            // Delay: slapback — single short repeat for rockabilly bounce
            Pair(12, 0) to 120f,  // delayTime (120ms slapback)
            Pair(12, 1) to 0.1f,  // feedback (single repeat)
            // Reverb: Room type
            Pair(13, 8) to 0f,    // type (Room)
            Pair(13, 0) to 0.45f, // decay (normalized)
            Pair(13, 6) to 0.5f   // tone
        )

        val wetDry = mapOf(
            12 to 0.30f,  // Delay — audible slapback
            13 to 0.15f   // Reverb — subtle room
        )

        return Preset(
            id = "factory-chicken-pickin",
            name = "Chicken Pickin'",
            author = "Factory",
            category = PresetCategory.CLEAN,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true,
            inputGain = 1.0f,
            outputGain = 0.85f
        )
    }

    /**
     * Factory Preset 31: "Reggae Skank"
     * Bob Marley/dub rhythm guitar tone. Very fast VCA compressor squashes
     * transients for machine-like rhythmic consistency. Dead-clean Twin Reverb
     * with aggressive EQ sculpting: bass cut (stay out of bass guitar territory),
     * mid scoop, treble bite for the percussive "skank" rhythm.
     */
    private fun createReggaeSkank(): Preset {
        val enabled = mapOf(
            0 to false,  // NoiseGate
            1 to true,   // Compressor (VCA — fast attack squashes transients)
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Twin Reverb, dead clean)
            6 to true,   // CabinetSim (Jensen C10Q)
            7 to true,   // ParametricEQ (aggressive sculpting)
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb (bright spring)
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Compressor: VCA with VERY fast attack — squashes transients for
            // machine-like rhythmic evenness. This is the opposite of chicken pickin'.
            Pair(1, 0) to -22f,   // threshold
            Pair(1, 1) to 6f,     // ratio (heavy compression)
            Pair(1, 2) to 0.5f,   // attack (VERY fast — kills transients)
            Pair(1, 3) to 150f,   // release
            Pair(1, 4) to 3f,     // makeupGain
            Pair(1, 5) to 3f,     // kneeWidth
            Pair(1, 6) to 0f,     // character (VCA — transparent, mechanical)
            // AmpModel: Twin Reverb dead clean — no breakup
            Pair(5, 0) to 0.3f,   // inputGain (dead clean)
            Pair(5, 1) to 0.7f,   // outputLevel
            Pair(5, 2) to 7f,     // modelType (Twin Reverb)
            Pair(5, 3) to 0f,     // variac (off)
            // CabinetSim: Jensen C10Q — bright, articulate
            Pair(6, 0) to 9f,     // cabinetType (Jensen C10Q)
            Pair(6, 1) to 1f,     // mix (full)
            // Graphic EQ: aggressive reggae sculpting
            // Cut bass (stay out of bass guitar's frequency range),
            // scoop mids (thin the sound), boost treble (percussive bite)
            Pair(7, 2) to -8f,    // 125Hz (cut bass — bass guitar territory)
            Pair(7, 3) to -6f,    // 250Hz (bass cut taper)
            Pair(7, 4) to -2f,    // 500Hz (low-mid taper)
            Pair(7, 5) to -3f,    // 1kHz (scoop mids — thin, rhythmic)
            Pair(7, 6) to 0f,     // 2kHz (flat)
            Pair(7, 7) to 3f,     // 4kHz (treble bite)
            Pair(7, 8) to 4f,     // 8kHz (skank bite)
            // Reverb: Room type
            Pair(13, 8) to 0f,    // type (Room)
            Pair(13, 0) to 0.59f, // decay (normalized)
            Pair(13, 6) to 0.7f   // tone
        )

        val wetDry = mapOf(
            13 to 0.25f  // Reverb — present but not drowning the rhythm
        )

        return Preset(
            id = "factory-reggae-skank",
            name = "Reggae Skank",
            author = "Factory",
            category = PresetCategory.CLEAN,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true,
            inputGain = 0.85f,
            outputGain = 0.85f
        )
    }

    /**
     * Factory Preset 32: "Dotted Delay"
     * The Edge's rhythmic dotted-eighth delay from "Where The Streets Have No Name".
     * AC30 chimey breakup through Alnico Blue speakers with precisely timed delay
     * creating the cascading rhythmic patterns. Subtle CE-1 chorus adds shimmer;
     * OTA compressor mimics the MXR Dyna Comp for even sustain.
     */
    private fun createDottedDelay(): Preset {
        val enabled = mapOf(
            0 to false,  // NoiseGate
            1 to true,   // Compressor (OTA — Dyna Comp approximation)
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (AC30)
            6 to true,   // CabinetSim (Alnico Blue)
            7 to false,  // ParametricEQ
            8 to true,   // Chorus (CE-1 — subtle shimmer)
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to true,  // Delay (dotted eighth — THE sound)
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false  // Fuzzrite
        )

        val params = mapOf(
            // Compressor: OTA — mimics MXR Dyna Comp for even sustain
            Pair(1, 0) to -20f,   // threshold
            Pair(1, 1) to 4f,     // ratio
            Pair(1, 2) to 8f,     // attack
            Pair(1, 3) to 120f,   // release
            Pair(1, 4) to 2f,     // makeupGain
            Pair(1, 5) to 6f,     // kneeWidth
            Pair(1, 6) to 1f,     // character (OTA)
            // AmpModel: AC30 — THE Edge amp, chimey breakup
            Pair(5, 0) to 0.8f,   // inputGain (chimey breakup territory)
            Pair(5, 1) to 0.65f,  // outputLevel
            Pair(5, 2) to 6f,     // modelType (AC30)
            Pair(5, 3) to 0f,     // variac (off)
            // CabinetSim: Alnico Blue — THE AC30 speaker, bright and chimey
            Pair(6, 0) to 8f,     // cabinetType (Alnico Blue)
            Pair(6, 1) to 1f,     // mix (full)
            // Chorus: CE-1 — subtle shimmer underneath the delays
            Pair(8, 0) to 1.5f,   // rate
            Pair(8, 1) to 0.25f,  // depth (subtle — don't compete with delay)
            Pair(8, 2) to 1f,     // mode (CE-1)
            // Delay: dotted eighth at ~126 BPM
            // At 126 BPM: quarter note = 476ms, dotted eighth = 476 * 0.75 = 357ms
            // Using 360ms as a practical rounded value
            Pair(12, 0) to 360f,  // delayTime (dotted eighth at ~126 BPM)
            Pair(12, 1) to 0.55f, // feedback (4+ audible repeats — builds the rhythm)
            // Reverb: Plate type
            Pair(13, 8) to 3f,    // type (Plate)
            Pair(13, 0) to 0.65f, // decay (normalized)
            Pair(13, 6) to 0.55f   // tone
        )

        val wetDry = mapOf(
            8 to 0.20f,   // Chorus — subtle shimmer
            12 to 0.45f,  // Delay — the star of the show
            13 to 0.20f   // Reverb — supportive space
        )

        return Preset(
            id = "factory-dotted-delay",
            name = "Dotted Delay",
            author = "Factory",
            category = PresetCategory.AMBIENT,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true,
            inputGain = 0.9f,
            outputGain = 0.8f
        )
    }

    // =====================================================================
    // Fuzzrite factory presets
    // =====================================================================

    /**
     * Factory Preset: "Iron Butterfly IAGDV"
     * In-A-Gadda-Da-Vida main riff tone.
     * Erik Brann: Mosrite Ventures guitar + Ge Fuzzrite + Vox Super Beatle.
     * Twin voicing approximates the solid-state Vox clean amp.
     * Alnico cab for Celestion Alnico speakers in the V414 cabinet.
     */
    private fun createIronButterflyIAGDV(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Twin = solid-state Vox approximation)
            6 to true,   // CabinetSim (Alnico)
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to true   // Fuzzrite (Ge variant)
        )

        val params = mapOf(
            // NoiseGate: light for single-coil hum
            Pair(0, 0) to -45f,  // threshold
            // AmpModel: Twin voicing (closest to Vox Super Beatle solid-state clean)
            Pair(5, 0) to 0.5f,  // inputGain (moderate -- amp shouldn't add much dirt)
            Pair(5, 1) to 0.65f, // outputLevel
            Pair(5, 2) to 7f,    // modelType (Twin)
            Pair(5, 3) to 0f,    // variac off
            // CabinetSim: Alnico (Celestion Alnico in Vox V414 cab)
            Pair(6, 0) to 7f,    // cabinetType (Alnico)
            Pair(6, 1) to 1f,    // mix
            // Reverb: Plate type
            Pair(13, 8) to 3f,    // type (Plate)
            Pair(13, 0) to 0.63f, // decay (normalized)
            Pair(13, 6) to 0.5f,  // tone / size
            // Fuzzrite: Germanium, Depth at ~6.5/10, Volume at ~7.5/10
            Pair(26, 0) to 0.65f, // depth (not maxed -- retain note definition)
            Pair(26, 1) to 0.75f, // volume
            Pair(26, 2) to 0f     // variant (Germanium)
        )

        val wetDry = mapOf(
            13 to 0.25f  // Reverb -- subtle spring character
        )

        return Preset(
            id = "factory-iron-butterfly-iagdv",
            name = "Iron Butterfly IAGDV",
            author = "Factory",
            category = PresetCategory.HEAVY,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true,
            inputGain = 0.9f,
            outputGain = 0.75f
        )
    }

    /**
     * Factory Preset: "Iron Butterfly Termination"
     * Full buzz-saw Fuzzrite tone for aggressive passages.
     * Same amp/cab as IAGDV but with Depth maxed for full Q2 clipping.
     */
    private fun createIronButterflyTermination(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Twin)
            6 to true,   // CabinetSim (Alnico)
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to true   // Fuzzrite
        )

        val params = mapOf(
            Pair(0, 0) to -45f,
            Pair(5, 0) to 0.55f,
            Pair(5, 1) to 0.7f,
            Pair(5, 2) to 7f,     // Twin
            Pair(5, 3) to 0f,
            Pair(6, 0) to 7f,     // Alnico
            Pair(6, 1) to 1f,
            // Reverb: Room type
            Pair(13, 8) to 0f,    // type (Room)
            Pair(13, 0) to 0.59f, // decay (normalized)
            Pair(13, 6) to 0.5f,  // tone
            // Fuzzrite: MAXED Depth for full Q2 buzz-saw
            Pair(26, 0) to 1.0f,  // depth (FULL -- maximum buzz-saw)
            Pair(26, 1) to 0.8f,  // volume
            Pair(26, 2) to 0f     // variant (Germanium)
        )

        val wetDry = mapOf(13 to 0.2f)

        return Preset(
            id = "factory-iron-butterfly-termination",
            name = "Iron Butterfly Termination",
            author = "Factory",
            category = PresetCategory.HEAVY,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true,
            inputGain = 0.95f,
            outputGain = 0.7f
        )
    }

    /**
     * Factory Preset: "Frusciante Fuzzrite"
     * Stadium Arcadium tour / The Empyrean-era tone.
     * Frusciante's signature setting: Ge Fuzzrite with Depth at 0 (Q1 only,
     * no buzz-saw), Volume cranked. DS-2 before Fuzzrite in chain.
     * Marshall Silver Jubilee amp into G12H30 cab.
     */
    private fun createFruscianteFuzzrite(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to true,   // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Jubilee)
            6 to true,   // CabinetSim (G12H30)
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to true,  // Delay (subtle space)
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to true,  // Boss DS-2 (DS-2 before Fuzzrite in Frusciante's chain)
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to true   // Fuzzrite (Ge variant, Depth=0)
        )

        val params = mapOf(
            // NoiseGate
            Pair(0, 0) to -40f,
            // Compressor: light
            Pair(1, 0) to -20f,  // threshold
            Pair(1, 1) to 2.5f,  // ratio
            Pair(1, 2) to 10f,   // attack
            Pair(1, 3) to 100f,  // release
            Pair(1, 4) to 1.5f,  // makeupGain
            // AmpModel: Silver Jubilee
            Pair(5, 0) to 0.45f, // inputGain (moderate, Fuzzrite provides drive)
            Pair(5, 1) to 0.7f,  // outputLevel
            Pair(5, 2) to 5f,    // modelType (Jubilee)
            Pair(5, 3) to 0f,    // variac off
            // CabinetSim: G12H30 (Marshall 4x12)
            Pair(6, 0) to 4f,    // cabinetType (G12H-30)
            Pair(6, 1) to 1f,    // mix
            // DS-2: Mode I, moderate distortion (sits before Fuzzrite)
            Pair(18, 0) to 0.5f, // dist
            Pair(18, 1) to 0.6f, // tone
            Pair(18, 2) to 0.5f, // level
            Pair(18, 3) to 0f,   // mode (Mode I)
            // Delay: subtle space
            Pair(12, 0) to 350f, // delayTime
            Pair(12, 1) to 0.2f, // feedback
            // Reverb: Plate type
            Pair(13, 8) to 3f,    // type (Plate)
            Pair(13, 0) to 0.65f, // decay (normalized)
            Pair(13, 6) to 0.55f,  // tone / size
            // Fuzzrite: Germanium, Depth at 0 (Q1 only!), Volume high
            Pair(26, 0) to 0.0f, // depth (ZERO -- only Q1, Frusciante's signature setting)
            Pair(26, 1) to 0.9f, // volume (8-10/10)
            Pair(26, 2) to 0f    // variant (Germanium)
        )

        val wetDry = mapOf(
            12 to 0.15f, // Delay -- very subtle
            13 to 0.2f   // Reverb -- light
        )

        return Preset(
            id = "factory-frusciante-fuzzrite",
            name = "Frusciante Fuzzrite",
            author = "Factory",
            category = PresetCategory.HEAVY,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true,
            inputGain = 0.85f,
            outputGain = 0.75f
        )
    }

    // =====================================================================
    // MF-102 Ring Modulator presets
    // =====================================================================

    /**
     * Factory Preset: "Warm Shimmer"
     * Subtle always-on ring mod that adds harmonic shimmer to clean Strat tones.
     * Low mix keeps the dry signal dominant while the carrier adds a gentle
     * metallic sparkle. LFO slowly drifts the carrier frequency for organic
     * movement. The "small higher frequencies spinning off the sustain" effect
     * described by MF-102 owners on the Moog forum.
     *
     * Best with: Neck or middle pickup, clean amp, fingerpicking or arpeggios.
     */
    private fun createRingModWarmShimmer(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to true,   // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Clean)
            6 to true,   // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false, // Fuzzrite
            27 to true,  // MF-102 Ring Mod
            28 to false  // Space Echo
        )

        val params = mapOf(
            // NoiseGate: light
            Pair(0, 0) to -45f,
            // Compressor: gentle studio squeeze
            Pair(1, 0) to -18f,   // threshold
            Pair(1, 1) to 3f,     // ratio
            Pair(1, 2) to 15f,    // attack
            Pair(1, 3) to 150f,   // release
            Pair(1, 4) to 2f,     // makeupGain
            // AmpModel: Clean, low gain
            Pair(5, 0) to 0.3f,   // inputGain
            Pair(5, 1) to 0.6f,   // outputLevel
            Pair(5, 2) to 0f,     // modelType (Clean)
            Pair(5, 3) to 0f,     // variac
            // CabinetSim: Jensen (warm, scooped mids for Strat sparkle)
            Pair(6, 0) to 9f,     // cabinetType (Jensen C10Q)
            Pair(6, 1) to 1f,     // mix
            // Reverb: Hall, medium
            Pair(13, 8) to 1f,    // type (Hall)
            Pair(13, 0) to 0.55f, // decay
            Pair(13, 6) to 0.6f,  // tone
            // MF-102: Low mix, HI range ~500Hz carrier, slow sine LFO drifting ±0.3 oct
            // Freq 0.575 = 500Hz in HI range. Gentle drive for Strat single coils.
            Pair(27, 0) to 0.2f,  // mix (mostly dry -- the shimmer peeks through)
            Pair(27, 1) to 0.575f, // frequency (~500Hz carrier, upper harmonics territory)
            Pair(27, 2) to 0.199f, // lfoRate (~0.3Hz -- glacial drift)
            Pair(27, 3) to 0.2f,  // lfoAmount (~0.6 octaves total, subtle wobble)
            Pair(27, 4) to 0.35f, // drive (slightly above unity for Strat output)
            Pair(27, 5) to 1f,    // range (HI)
            Pair(27, 6) to 0f     // lfoWave (sine)
        )

        val wetDry = mapOf(
            13 to 0.25f  // Reverb -- moderate
        )

        return Preset(
            id = "factory-ringmod-warm-shimmer",
            name = "Warm Shimmer",
            author = "Factory",
            category = PresetCategory.CLEAN,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true,
            inputGain = 1.0f,
            outputGain = 0.85f
        )
    }

    /**
     * Factory Preset: "Harmonic Tremolo"
     * LO range carrier at ~7Hz creates a rich, frequency-dependent tremolo
     * that sounds nothing like a standard amplitude tremolo. The ring mod
     * alternately emphasizes and cancels different harmonics as the carrier
     * cycles, producing the "breathing" quality that Fender brownface amps
     * were known for. A touch of sine LFO adds organic drift.
     *
     * The #1 practical use case for ring mod with guitar on the Moog forum.
     * Best with: Position 2 or 4 (in-between), clean amp, rhythm playing.
     */
    private fun createRingModHarmonicTremolo(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Twin -- Fender clean)
            6 to true,   // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false, // Fuzzrite
            27 to true,  // MF-102 Ring Mod
            28 to false  // Space Echo
        )

        val params = mapOf(
            Pair(0, 0) to -45f,
            // AmpModel: Twin (clean Fender character)
            Pair(5, 0) to 0.35f,
            Pair(5, 1) to 0.65f,
            Pair(5, 2) to 7f,    // Twin
            Pair(5, 3) to 0f,
            // CabinetSim: Alnico Blue (chimey, suits Fender clean)
            Pair(6, 0) to 8f,    // Alnico Blue
            Pair(6, 1) to 1f,
            // Reverb: spring character
            Pair(13, 4) to 1f,   // mode (Spring 2)
            Pair(13, 5) to 0.45f, // dwell
            Pair(13, 6) to 0.5f, // tone
            // MF-102: LO range ~7Hz carrier, full wet, gentle LFO drift
            // Freq 0.502 = ~7Hz in LO range. This is the sweet spot where
            // ring mod becomes a rich harmonic tremolo -- the carrier is fast
            // enough to modulate audibly but slow enough to feel rhythmic.
            Pair(27, 0) to 0.85f, // mix (mostly wet -- this IS the tremolo)
            Pair(27, 1) to 0.502f, // frequency (~7Hz in LO -- sweet spot)
            Pair(27, 2) to 0.126f, // lfoRate (~0.2Hz -- very slow drift)
            Pair(27, 3) to 0.1f,  // lfoAmount (~0.3 oct, keeps it organic)
            Pair(27, 4) to 0.3f,  // drive (moderate for Strat)
            Pair(27, 5) to 0f,    // range (LO)
            Pair(27, 6) to 0f     // lfoWave (sine)
        )

        val wetDry = mapOf(
            13 to 0.2f  // Reverb -- touch of spring
        )

        return Preset(
            id = "factory-ringmod-harmonic-tremolo",
            name = "Harmonic Tremolo",
            author = "Factory",
            category = PresetCategory.CLEAN,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true,
            inputGain = 1.0f,
            outputGain = 0.85f
        )
    }

    /**
     * Factory Preset: "Bell Tones"
     * Classic musical ring mod sound. Carrier at ~220Hz (A3) creates bell-like
     * metallic harmonics when playing in the key of A. The sum and difference
     * tones produce harmonically related partials that ring like a gamelan
     * or tubular bells. Mix at 50% keeps the original note audible underneath.
     *
     * Tip from the Moog manual: tune the carrier to the root note of your key.
     * Best with: Neck pickup, clean amp, single-note melodies in key of A.
     */
    private fun createRingModBellTones(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to true,   // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Clean)
            6 to true,   // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to true,  // Delay (slapback for bell decay)
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false, // Fuzzrite
            27 to true,  // MF-102 Ring Mod
            28 to false  // Space Echo
        )

        val params = mapOf(
            Pair(0, 0) to -45f,
            // Compressor: sustain for bell-like decay
            Pair(1, 0) to -22f,   // threshold
            Pair(1, 1) to 4f,     // ratio
            Pair(1, 2) to 5f,     // attack (fast)
            Pair(1, 3) to 200f,   // release
            Pair(1, 4) to 4f,     // makeupGain
            // AmpModel: Clean
            Pair(5, 0) to 0.25f,
            Pair(5, 1) to 0.6f,
            Pair(5, 2) to 0f,     // Clean
            Pair(5, 3) to 0f,
            // CabinetSim: 1x12 Combo (balanced, not too dark)
            Pair(6, 0) to 0f,     // 1x12 Combo
            Pair(6, 1) to 1f,
            // Delay: short slapback to extend bell harmonics
            Pair(12, 0) to 180f,  // delayTime
            Pair(12, 1) to 0.15f, // feedback (just one repeat)
            // Reverb: Plate (nice shimmer for bells)
            Pair(13, 8) to 3f,    // type (Plate)
            Pair(13, 0) to 0.65f, // decay
            Pair(13, 6) to 0.55f, // tone
            // MF-102: HI range, carrier at ~220Hz (A3), no LFO, 50% mix
            // Freq 0.407 = 220Hz (A3). Playing in key of A, the root note
            // at 110Hz produces sum=330Hz (E4) and diff=110Hz (unison),
            // the 5th at 165Hz produces sum=385Hz and diff=55Hz -- all
            // harmonically related, creating stable bell partials.
            Pair(27, 0) to 0.5f,  // mix (50/50 -- hear both guitar and bells)
            Pair(27, 1) to 0.407f, // frequency (~220Hz, A3)
            Pair(27, 2) to 0f,    // lfoRate (unused)
            Pair(27, 3) to 0f,    // lfoAmount (none -- static carrier for tuned bells)
            Pair(27, 4) to 0.3f,  // drive (moderate for Strat)
            Pair(27, 5) to 1f,    // range (HI)
            Pair(27, 6) to 0f     // lfoWave (sine, unused)
        )

        val wetDry = mapOf(
            12 to 0.2f,  // Delay -- subtle
            13 to 0.3f   // Reverb -- moderate plate wash
        )

        return Preset(
            id = "factory-ringmod-bell-tones",
            name = "Bell Tones",
            author = "Factory",
            category = PresetCategory.AMBIENT,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true,
            inputGain = 1.0f,
            outputGain = 0.8f
        )
    }

    /**
     * Factory Preset: "Robot Factory"
     * Aggressive, robotic ring mod for angular riffs and noise textures.
     * Inspired by Tom Morello's use of ring modulation for mechanical,
     * industrial sounds. High carrier frequency with square LFO creates
     * stuttering, glitchy tones. High drive pushes the OTA into saturation.
     *
     * Best with: Bridge pickup, heavy palm muting, staccato single-note riffs.
     * Play power chords for maximum dissonance, or single notes for sci-fi leads.
     */
    private fun createRingModRobotFactory(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Crunch)
            6 to true,   // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to false, // Reverb (dry and in-your-face)
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false, // Fuzzrite
            27 to true,  // MF-102 Ring Mod
            28 to false  // Space Echo
        )

        val params = mapOf(
            Pair(0, 0) to -40f,
            // AmpModel: Crunch (some breakup to thicken the ring mod)
            Pair(5, 0) to 0.7f,
            Pair(5, 1) to 0.7f,
            Pair(5, 2) to 1f,     // Crunch
            Pair(5, 3) to 0f,
            // CabinetSim: 4x12 British (tight, aggressive)
            Pair(6, 0) to 1f,     // 4x12 British
            Pair(6, 1) to 1f,
            // MF-102: HI range, ~1kHz carrier (maximum metallic clash),
            // square LFO at ~3Hz for robotic stuttering, cranked drive.
            // Freq 0.717 = ~1000Hz. This is the most aggressive zone --
            // guitar fundamentals (80-400Hz) produce sum tones in the 1-1.4kHz
            // range and difference tones that are inharmonic and dissonant.
            Pair(27, 0) to 0.9f,  // mix (mostly wet -- commit to the robot)
            Pair(27, 1) to 0.717f, // frequency (~1000Hz -- aggressive metallic)
            Pair(27, 2) to 0.616f, // lfoRate (~3Hz -- stuttering rhythm)
            Pair(27, 3) to 0.5f,  // lfoAmount (1.5 octaves -- dramatic sweep)
            Pair(27, 4) to 0.7f,  // drive (high -- OTA saturation adds grit)
            Pair(27, 5) to 1f,    // range (HI)
            Pair(27, 6) to 1f     // lfoWave (SQUARE -- hard cuts, robotic)
        )

        return Preset(
            id = "factory-ringmod-robot-factory",
            name = "Robot Factory",
            author = "Factory",
            category = PresetCategory.HEAVY,
            effects = buildEffectChain(enabled, params),
            isFactory = true,
            inputGain = 1.0f,
            outputGain = 0.75f
        )
    }

    /**
     * Factory Preset: "Radio Transmission"
     * Eerie, atmospheric ring mod textures for ambient/post-rock passages.
     * Inspired by Jonny Greenwood's use of ring modulation and filtering
     * to create unsettling, otherworldly guitar textures on Kid A/Amnesiac.
     * Medium carrier with slow sine LFO creates drifting, alien harmonics.
     * Reverb and delay push it into a vast, decaying soundscape.
     *
     * Best with: Neck pickup, volume swells (roll guitar volume up slowly),
     * sustained chords, or harmonics above the 12th fret.
     */
    private fun createRingModRadioTransmission(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Clean)
            6 to true,   // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to true,  // Delay (long, washy)
            13 to true,  // Reverb (big hall)
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false, // Fuzzrite
            27 to true,  // MF-102 Ring Mod
            28 to false  // Space Echo
        )

        val params = mapOf(
            Pair(0, 0) to -50f,   // noise gate: very sensitive for swells
            // AmpModel: Clean, very low gain (let ring mod be the texture)
            Pair(5, 0) to 0.2f,
            Pair(5, 1) to 0.5f,
            Pair(5, 2) to 0f,     // Clean
            Pair(5, 3) to 0f,
            // CabinetSim: 2x12 Open (airy, open back for ambient)
            Pair(6, 0) to 2f,     // 2x12 Open
            Pair(6, 1) to 1f,
            // Delay: long, with feedback for cascading repeats
            Pair(12, 0) to 600f,  // delayTime (long)
            Pair(12, 1) to 0.55f, // feedback (building repeats)
            // Reverb: Church (huge space)
            Pair(13, 8) to 2f,    // type (Church)
            Pair(13, 0) to 0.8f,  // decay (very long)
            Pair(13, 6) to 0.4f,  // tone (slightly dark)
            // MF-102: HI range ~330Hz (E4), slow LFO for eerie drift
            // Freq 0.490 = ~330Hz. Mid-range carrier creates ghostly
            // sidebands that are close enough to musical intervals to
            // feel tonal but far enough to feel "wrong" -- the Radiohead zone.
            Pair(27, 0) to 0.65f, // mix (more wet than dry -- commit to weird)
            Pair(27, 1) to 0.490f, // frequency (~330Hz, E4)
            Pair(27, 2) to 0.199f, // lfoRate (~0.3Hz -- glacial)
            Pair(27, 3) to 0.35f, // lfoAmount (~1 octave total -- wide drift)
            Pair(27, 4) to 0.25f, // drive (low -- clean ring mod, less OTA grit)
            Pair(27, 5) to 1f,    // range (HI)
            Pair(27, 6) to 0f     // lfoWave (sine -- smooth, not choppy)
        )

        val wetDry = mapOf(
            12 to 0.35f, // Delay -- prominent
            13 to 0.45f  // Reverb -- big wash
        )

        return Preset(
            id = "factory-ringmod-radio-transmission",
            name = "Radio Transmission",
            author = "Factory",
            category = PresetCategory.AMBIENT,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true,
            inputGain = 0.9f,
            outputGain = 0.8f
        )
    }

    /**
     * Factory Preset: "Funk Clav"
     * Snappy, percussive ring mod that mimics the metallic bite of a
     * Clavinet through a ring modulator -- a classic funk/soul studio trick.
     * HI carrier at ~150Hz, no LFO (static), moderate mix. The low carrier
     * frequency adds thickness and bark without becoming fully atonal.
     * Works best with staccato 16th-note muted strumming.
     *
     * Best with: Bridge pickup, muted/staccato strumming, clean amp.
     * Roll tone knob to ~7 to tame bridge pickup ice-pick.
     */
    private fun createRingModFunkClav(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to true,   // Compressor (tight snap)
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Clean)
            6 to true,   // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to false, // Reverb (dry funk -- no reverb)
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false, // Fuzzrite
            27 to true,  // MF-102 Ring Mod
            28 to false  // Space Echo
        )

        val params = mapOf(
            Pair(0, 0) to -40f,
            // Compressor: tight, fast attack for percussive snap
            Pair(1, 0) to -15f,   // threshold (hit hard)
            Pair(1, 1) to 6f,     // ratio (squeeze)
            Pair(1, 2) to 2f,     // attack (very fast -- snap)
            Pair(1, 3) to 80f,    // release (quick recovery)
            Pair(1, 4) to 5f,     // makeupGain
            Pair(1, 5) to 3f,     // kneeWidth (hard)
            // AmpModel: Clean with a bit of edge
            Pair(5, 0) to 0.45f,
            Pair(5, 1) to 0.7f,
            Pair(5, 2) to 0f,     // Clean
            Pair(5, 3) to 0f,
            // CabinetSim: Jensen C10Q (tight, midrange-focused -- clav character)
            Pair(6, 0) to 9f,     // Jensen C10Q
            Pair(6, 1) to 1f,
            // MF-102: HI range ~150Hz static carrier, moderate mix, high drive
            // Freq 0.329 = ~150Hz. Low carrier in HI range creates thick,
            // bark-like harmonics that add the "wooden thwack" of a Clavinet.
            // No LFO -- the effect should be consistent and rhythmic.
            Pair(27, 0) to 0.4f,  // mix (blended -- clav bite + guitar body)
            Pair(27, 1) to 0.329f, // frequency (~150Hz -- thick, barky)
            Pair(27, 2) to 0f,    // lfoRate (unused)
            Pair(27, 3) to 0f,    // lfoAmount (none -- static and punchy)
            Pair(27, 4) to 0.5f,  // drive (moderate-high, Strat needs it)
            Pair(27, 5) to 1f,    // range (HI)
            Pair(27, 6) to 0f     // lfoWave (sine, unused)
        )

        return Preset(
            id = "factory-ringmod-funk-clav",
            name = "Funk Clav",
            author = "Factory",
            category = PresetCategory.CLEAN,
            effects = buildEffectChain(enabled, params),
            isFactory = true,
            inputGain = 1.1f,
            outputGain = 0.8f
        )
    }

    /**
     * Factory Preset: "Deep Space"
     * Slow, evolving ring mod soundscape for ambient guitar. LO range carrier
     * at ~3Hz creates a deep pulse while slow sine LFO sweeps it through
     * sub-audio territory, producing tidal-wave-like amplitude modulation.
     * Combined with long reverb and delay for a vast, cinematic texture.
     *
     * Best with: Neck pickup, volume swells, e-bow or sustained feedback.
     * Let notes ring and decay naturally -- the effect evolves over 10+ seconds.
     */
    private fun createRingModDeepSpace(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Clean)
            6 to true,   // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to true,  // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false, // Fuzzrite
            27 to true,  // MF-102 Ring Mod
            28 to false  // Space Echo
        )

        val params = mapOf(
            Pair(0, 0) to -50f,   // noise gate: very sensitive
            // AmpModel: Clean, warm
            Pair(5, 0) to 0.2f,
            Pair(5, 1) to 0.5f,
            Pair(5, 2) to 0f,     // Clean
            Pair(5, 3) to 0f,
            // CabinetSim: 2x12 Open (spacious)
            Pair(6, 0) to 2f,     // 2x12 Open
            Pair(6, 1) to 1f,
            // Delay: very long, high feedback for self-oscillation
            Pair(12, 0) to 900f,  // delayTime
            Pair(12, 1) to 0.6f,  // feedback (building cascades)
            // Reverb: Mod type (modulated tails for shimmer)
            Pair(13, 8) to 5f,    // type (Mod)
            Pair(13, 0) to 0.85f, // decay (very long)
            Pair(13, 6) to 0.45f, // tone (slightly dark)
            // MF-102: LO range ~3Hz, slow LFO sweeping through sub-audio
            // Freq 0.329 = ~3Hz in LO range. This deep pulsing creates
            // tidal breathing that interacts with the delay feedback
            // to produce evolving, generative textures.
            Pair(27, 0) to 0.75f, // mix (mostly wet)
            Pair(27, 1) to 0.329f, // frequency (~3Hz in LO -- deep pulse)
            Pair(27, 2) to 0.126f, // lfoRate (~0.2Hz -- tidal)
            Pair(27, 3) to 0.45f, // lfoAmount (~1.35 oct -- wide sweep through sub-audio)
            Pair(27, 4) to 0.2f,  // drive (low -- clean, spacious)
            Pair(27, 5) to 0f,    // range (LO)
            Pair(27, 6) to 0f     // lfoWave (sine -- smooth tides)
        )

        val wetDry = mapOf(
            12 to 0.4f,  // Delay -- prominent
            13 to 0.5f   // Reverb -- massive wash
        )

        return Preset(
            id = "factory-ringmod-deep-space",
            name = "Deep Space",
            author = "Factory",
            category = PresetCategory.AMBIENT,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true,
            inputGain = 0.85f,
            outputGain = 0.8f
        )
    }

    /**
     * Factory Preset: "Octave Up"
     * Ring mod tuned to produce an octave-up effect. When the carrier is set
     * to match the note being played, the sum frequency = 2x fundamental
     * (octave up) and the difference = 0 (DC, filtered out). This creates
     * a usable octave-up similar to an Octavia but with the Moog OTA warmth.
     *
     * Carrier set to ~440Hz (A4). Works best when playing around A2-A3
     * (open A string area). The further from the carrier's tuned note,
     * the more dissonant/bell-like it becomes -- use that musically.
     *
     * Best with: Neck pickup, single notes around the A string (open to 12th fret).
     * Slight overdrive helps thicken the octave effect.
     */
    private fun createRingModOctaveUp(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to true,   // Overdrive (light crunch to thicken)
            5 to true,   // AmpModel (Crunch)
            6 to true,   // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false, // Fuzzrite
            27 to true,  // MF-102 Ring Mod
            28 to false  // Space Echo
        )

        val params = mapOf(
            Pair(0, 0) to -42f,
            // Overdrive: light crunch
            Pair(4, 0) to 0.25f,  // drive (touch of grit)
            Pair(4, 1) to 0.55f,  // tone
            Pair(4, 2) to 0.7f,   // level
            // AmpModel: Crunch (slight breakup)
            Pair(5, 0) to 0.5f,
            Pair(5, 1) to 0.65f,
            Pair(5, 2) to 1f,     // Crunch
            Pair(5, 3) to 0f,
            // CabinetSim: Greenback (classic rock character)
            Pair(6, 0) to 4f,     // Greenback
            Pair(6, 1) to 1f,
            // Reverb: Room (tight, not washy)
            Pair(13, 8) to 0f,    // type (Room)
            Pair(13, 0) to 0.35f, // decay (short)
            Pair(13, 6) to 0.6f,  // tone
            // MF-102: HI range, carrier at ~440Hz (A4), no LFO, high mix
            // Freq 0.549 = ~440Hz. When playing A2 (110Hz):
            //   Sum = 110+440 = 550Hz (slightly sharp C#5)
            //   Diff = 440-110 = 330Hz (E4, the 5th)
            // When playing A3 (220Hz):
            //   Sum = 220+440 = 660Hz (E5, octave+5th above A3)
            //   Diff = 440-220 = 220Hz (unison A3)
            // The closer to carrier pitch, the more "octave up" it sounds.
            Pair(27, 0) to 0.7f,  // mix (mostly wet for clear octave)
            Pair(27, 1) to 0.549f, // frequency (~440Hz, A4)
            Pair(27, 2) to 0f,    // lfoRate (unused)
            Pair(27, 3) to 0f,    // lfoAmount (none -- static tuned carrier)
            Pair(27, 4) to 0.45f, // drive (moderate -- thickens octave)
            Pair(27, 5) to 1f,    // range (HI)
            Pair(27, 6) to 0f     // lfoWave (sine, unused)
        )

        val wetDry = mapOf(
            13 to 0.15f  // Reverb -- subtle room
        )

        return Preset(
            id = "factory-ringmod-octave-up",
            name = "Octave Up",
            author = "Factory",
            category = PresetCategory.CRUNCH,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true,
            inputGain = 1.0f,
            outputGain = 0.8f
        )
    }

    /**
     * Factory Preset: "Broken Amp"
     * Lo-fi warble that sounds like a tube amp with a dying power supply.
     * LO range carrier at ~20Hz creates a fast, ragged tremolo that is
     * subtly pitch-unstable due to the ring mod sidebands. The square LFO
     * adds periodic speed changes that mimic a failing component.
     * Think: a vintage amp found in a garage that barely works but sounds
     * incredible in its own broken way.
     *
     * Best with: Position 4 (neck+mid), light picking, blues licks.
     * Roll guitar volume to 7-8 for that "amp on the edge" feel.
     */
    private fun createRingModBrokenAmp(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (Clean -- let ring mod do the grit)
            6 to true,   // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb (short, lo-fi)
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false, // Fuzzrite
            27 to true,  // MF-102 Ring Mod
            28 to false  // Space Echo
        )

        val params = mapOf(
            Pair(0, 0) to -45f,
            // AmpModel: Clean (ring mod provides all the dirt)
            Pair(5, 0) to 0.4f,
            Pair(5, 1) to 0.6f,
            Pair(5, 2) to 0f,     // Clean
            Pair(5, 3) to 0f,
            // CabinetSim: 1x12 Combo (small, boxy -- lo-fi character)
            Pair(6, 0) to 0f,     // 1x12 Combo
            Pair(6, 1) to 1f,
            // Reverb: Lo-Fi type (gritty, degraded tails)
            Pair(13, 8) to 4f,    // type (Lo-Fi)
            Pair(13, 0) to 0.4f,  // decay (short-medium)
            Pair(13, 6) to 0.35f, // tone (dark)
            // MF-102: LO range ~20Hz (fast sub-audio tremolo), square LFO
            // Freq 0.717 = ~20Hz in LO. This is fast enough to create a
            // ragged, uneven tremolo with ring mod sidebands that make it
            // sound like a failing power supply. Square LFO at ~0.5Hz
            // alternates the carrier speed, creating the "something is
            // wrong with this amp" character.
            Pair(27, 0) to 0.6f,  // mix (blended -- enough dry for note definition)
            Pair(27, 1) to 0.717f, // frequency (~20Hz in LO -- fast warble)
            Pair(27, 2) to 0.291f, // lfoRate (~0.5Hz -- periodic instability)
            Pair(27, 3) to 0.3f,  // lfoAmount (~0.9 oct -- noticeable speed change)
            Pair(27, 4) to 0.4f,  // drive (moderate -- some OTA warmth)
            Pair(27, 5) to 0f,    // range (LO)
            Pair(27, 6) to 1f     // lfoWave (SQUARE -- abrupt speed jumps)
        )

        val wetDry = mapOf(
            13 to 0.2f  // Reverb -- touch of grungy room
        )

        return Preset(
            id = "factory-ringmod-broken-amp",
            name = "Broken Amp",
            author = "Factory",
            category = PresetCategory.CLEAN,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true,
            inputGain = 1.0f,
            outputGain = 0.85f
        )
    }

    // =====================================================================
    // Tube Screamer (TS-808 / TS9) factory presets
    // =====================================================================

    /**
     * TS Preset 1: "TS Classic Green"
     * The definitive TS-808 sound. Drive at 45%, tone centered slightly
     * above noon, level at unity. NoiseGate + compressor tighten the front
     * end. Light room reverb places the sound in a believable acoustic
     * space. Pairs well with a slightly broken-up amp; the TS mid-hump
     * pushes the amp into natural tube saturation.
     *
     * Best with: Bridge or neck-bridge position, moderate picking attack.
     */
    private fun createTSClassicGreen(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to true,   // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive (off -- TS is the dirt source)
            5 to true,   // AmpModel (crunch -- TS pushes it to breakup)
            6 to false,  // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb (subtle room)
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false, // Fuzzrite
            27 to false, // MF-102 Ring Mod
            28 to false, // Space Echo
            29 to false, // DL4 Delay
            30 to false, // Boss BD-2
            31 to true   // TubeScreamer
        )

        val params = mapOf(
            // NoiseGate: moderate threshold, natural release
            Pair(0, 0) to -42f,   // threshold
            Pair(0, 1) to 6f,     // hysteresis
            Pair(0, 2) to 1f,     // attack
            Pair(0, 3) to 35f,    // release
            // Compressor: gentle squeeze to even out attack
            Pair(1, 0) to -20f,   // threshold
            Pair(1, 1) to 3f,     // ratio
            Pair(1, 2) to 12f,    // attack
            Pair(1, 3) to 140f,   // release
            Pair(1, 4) to 3f,     // makeupGain
            Pair(1, 5) to 5f,     // kneeWidth
            // AmpModel: crunch (TS mid-hump drives it to natural breakup)
            Pair(5, 0) to 0.55f,  // inputGain (moderate -- TS does the work)
            Pair(5, 1) to 0.65f,  // outputLevel
            Pair(5, 2) to 1f,     // modelType (crunch)
            Pair(5, 3) to 0f,     // variac (off)
            // Reverb: Room type, short decay -- spatial but not splashy
            Pair(13, 8) to 0f,    // type (Room)
            Pair(13, 0) to 0.45f, // decay (short-medium)
            Pair(13, 6) to 0.55f, // tone (neutral)
            // TubeScreamer: TS-808 mode, iconic mid-gain sweet spot
            Pair(31, 0) to 0.45f, // Drive
            Pair(31, 1) to 0.55f, // Tone
            Pair(31, 2) to 0.50f, // Level
            Pair(31, 3) to 0.0f   // Mode (TS-808)
        )

        val wetDry = mapOf(
            13 to 0.25f  // Reverb -- subtle
        )

        return Preset(
            id = "factory-ts-classic-green",
            name = "TS Classic Green",
            author = "Factory",
            category = PresetCategory.CRUNCH,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * TS Preset 2: "TS Edge of Breakup"
     * Low-drive TS-808 as a clean boost into an amp on the edge of breakup.
     * Drive at 20% adds only the lightest clipping; the level at 65% pushes
     * the amp harder. The tone is slightly bright to cut through a mix when
     * using single coils. This is a touch-sensitive preset -- pick harder for
     * more grit, softer for almost clean.
     *
     * Best with: Strat neck or bridge, moderate to high amp gain.
     */
    private fun createTSEdgeOfBreakup(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor (off -- preserve pick dynamics)
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel (crunch -- TS tips it over)
            6 to false,  // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false, // Fuzzrite
            27 to false, // MF-102 Ring Mod
            28 to false, // Space Echo
            29 to false, // DL4 Delay
            30 to false, // Boss BD-2
            31 to true   // TubeScreamer
        )

        val params = mapOf(
            // NoiseGate: gentle -- preserve pick attack transients
            Pair(0, 0) to -50f,
            Pair(0, 1) to 5f,
            Pair(0, 2) to 2f,
            Pair(0, 3) to 50f,
            // AmpModel: crunch on the edge
            Pair(5, 0) to 0.65f,  // inputGain (amp near breakup)
            Pair(5, 1) to 0.7f,   // outputLevel
            Pair(5, 2) to 1f,     // modelType (crunch)
            Pair(5, 3) to 0f,
            // Reverb: Hall type, longer tail for bloom
            Pair(13, 8) to 1f,    // type (Hall)
            Pair(13, 0) to 0.55f, // decay (medium)
            Pair(13, 6) to 0.6f,  // tone (slightly bright)
            // TubeScreamer: TS-808 as transparent clean boost
            Pair(31, 0) to 0.20f, // Drive (very low -- barely clipping)
            Pair(31, 1) to 0.60f, // Tone (bright -- single-coil clarity)
            Pair(31, 2) to 0.65f, // Level (high -- amp-pushing)
            Pair(31, 3) to 0.0f   // Mode (TS-808)
        )

        val wetDry = mapOf(
            13 to 0.3f  // Reverb -- noticeable bloom
        )

        return Preset(
            id = "factory-ts-edge-of-breakup",
            name = "TS Edge of Breakup",
            author = "Factory",
            category = PresetCategory.CRUNCH,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * TS Preset 3: "TS Texas Flood"
     * High-drive TS-808 for thick, singing blues lead tone inspired by
     * Stevie Ray Vaughan's Strat-into-Tube-Screamer-into-Vibroverb approach.
     * Drive at 70% gives a thick, compressed saturation. Tone slightly dark
     * at 45% takes the edge off single-coil brightness. Compressor added
     * before the TS to even out string-to-string volume.
     *
     * Best with: Strat neck pickup, heavy-gauge strings, dig in hard.
     */
    private fun createTSTexasFlood(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to true,   // Compressor (levels string dynamics)
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel
            6 to false,  // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb (spring-style)
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false, // Fuzzrite
            27 to false, // MF-102 Ring Mod
            28 to false, // Space Echo
            29 to false, // DL4 Delay
            30 to false, // Boss BD-2
            31 to true   // TubeScreamer
        )

        val params = mapOf(
            // NoiseGate
            Pair(0, 0) to -40f,
            Pair(0, 1) to 6f,
            Pair(0, 2) to 1f,
            Pair(0, 3) to 40f,
            // Compressor: tighter for SRV consistency
            Pair(1, 0) to -24f,
            Pair(1, 1) to 4f,
            Pair(1, 2) to 10f,
            Pair(1, 3) to 120f,
            Pair(1, 4) to 5f,
            Pair(1, 5) to 4f,
            // AmpModel: crunch (Vibroverb-style natural breakup)
            Pair(5, 0) to 0.6f,
            Pair(5, 1) to 0.65f,
            Pair(5, 2) to 1f,     // crunch
            Pair(5, 3) to 0f,
            // Reverb: spring-like (2-spring mode on Hall of Fame)
            Pair(13, 8) to 6f,    // type (Spring 2)
            Pair(13, 0) to 0.50f, // decay (medium -- classic spring length)
            Pair(13, 6) to 0.5f,  // tone (neutral)
            // TubeScreamer: TS-808 thick blues drive
            Pair(31, 0) to 0.70f, // Drive (high -- fat saturation)
            Pair(31, 1) to 0.45f, // Tone (slightly dark -- warm blues)
            Pair(31, 2) to 0.45f, // Level (slightly below unity)
            Pair(31, 3) to 0.0f   // Mode (TS-808)
        )

        val wetDry = mapOf(
            13 to 0.3f  // Reverb -- spring splash
        )

        return Preset(
            id = "factory-ts-texas-flood",
            name = "TS Texas Flood",
            author = "Factory",
            category = PresetCategory.CRUNCH,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * TS Preset 4: "TS Mid Boost"
     * TS-808 dialed for maximum mid-hump impact -- low drive lets the EQ
     * coloration dominate rather than the clipping. Level at 70% hits the
     * amp hard with a scooped mid-spike that fills space in a band mix.
     * Useful for rhythm playing where the guitar needs presence without
     * occupying the upper-mid frequency space of vocals.
     *
     * Best with: Bridge pickup, rhythm chords, medium-clean amp.
     */
    private fun createTSMidBoost(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to true,   // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel
            6 to false,  // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false, // Fuzzrite
            27 to false, // MF-102 Ring Mod
            28 to false, // Space Echo
            29 to false, // DL4 Delay
            30 to false, // Boss BD-2
            31 to true   // TubeScreamer
        )

        val params = mapOf(
            // NoiseGate
            Pair(0, 0) to -44f,
            Pair(0, 1) to 6f,
            Pair(0, 2) to 1f,
            Pair(0, 3) to 35f,
            // Compressor: moderate, consistent for rhythm
            Pair(1, 0) to -20f,
            Pair(1, 1) to 4f,
            Pair(1, 2) to 8f,
            Pair(1, 3) to 110f,
            Pair(1, 4) to 4f,
            Pair(1, 5) to 5f,
            // AmpModel: clean-to-crunch (TS mid boost + high level tips it)
            Pair(5, 0) to 0.5f,
            Pair(5, 1) to 0.65f,
            Pair(5, 2) to 1f,     // crunch
            Pair(5, 3) to 0f,
            // Reverb: Room type, short -- tight rhythm sound
            Pair(13, 8) to 0f,    // type (Room)
            Pair(13, 0) to 0.40f, // decay (short)
            Pair(13, 6) to 0.5f,  // tone
            // TubeScreamer: TS-808 mid-bump dominant, EQ more than clip
            Pair(31, 0) to 0.30f, // Drive (low -- EQ coloring, not grit)
            Pair(31, 1) to 0.70f, // Tone (bright -- accentuates mid spike)
            Pair(31, 2) to 0.70f, // Level (high -- volume boost into amp)
            Pair(31, 3) to 0.0f   // Mode (TS-808)
        )

        val wetDry = mapOf(
            13 to 0.2f  // Reverb -- minimal, tight rhythm
        )

        return Preset(
            id = "factory-ts-mid-boost",
            name = "TS Mid Boost",
            author = "Factory",
            category = PresetCategory.CRUNCH,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * TS Preset 5: "TS9 Bright"
     * TS9 mode has a slightly lower input impedance and different clipping
     * characteristic compared to the TS-808 -- the op-amp clips harder and
     * the tone stack sits after the clipping stage, giving a more open,
     * glassy top end. Drive at 50%, tone at 65% brings out the extra
     * air without becoming harsh. Good for clean, slightly compressed tones
     * where a Strat's top end detail needs to survive.
     *
     * Best with: Strat neck or middle, single-note lines.
     */
    private fun createTS9Bright(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to true,   // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel
            6 to false,  // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false, // Fuzzrite
            27 to false, // MF-102 Ring Mod
            28 to false, // Space Echo
            29 to false, // DL4 Delay
            30 to false, // Boss BD-2
            31 to true   // TubeScreamer
        )

        val params = mapOf(
            // NoiseGate
            Pair(0, 0) to -42f,
            Pair(0, 1) to 5f,
            Pair(0, 2) to 1f,
            Pair(0, 3) to 40f,
            // Compressor: gentle for note detail
            Pair(1, 0) to -22f,
            Pair(1, 1) to 3f,
            Pair(1, 2) to 15f,
            Pair(1, 3) to 160f,
            Pair(1, 4) to 2f,
            Pair(1, 5) to 6f,
            // AmpModel: crunch -- TS9 openness complements natural breakup
            Pair(5, 0) to 0.55f,
            Pair(5, 1) to 0.65f,
            Pair(5, 2) to 1f,     // crunch
            Pair(5, 3) to 0f,
            // Reverb: Hall type -- open and airy to match TS9 top end
            Pair(13, 8) to 1f,    // type (Hall)
            Pair(13, 0) to 0.55f, // decay (medium)
            Pair(13, 6) to 0.65f, // tone (bright)
            // TubeScreamer: TS9 mode, slightly open and glassy
            Pair(31, 0) to 0.50f, // Drive (medium)
            Pair(31, 1) to 0.65f, // Tone (bright -- TS9 character)
            Pair(31, 2) to 0.50f, // Level (unity)
            Pair(31, 3) to 1.0f   // Mode (TS9)
        )

        val wetDry = mapOf(
            13 to 0.3f  // Reverb -- present and airy
        )

        return Preset(
            id = "factory-ts9-bright",
            name = "TS9 Bright",
            author = "Factory",
            category = PresetCategory.CRUNCH,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * TS Preset 6: "TS9 Lead"
     * TS9 pushed into lead territory with drive at 75%. The TS9's harder
     * clipping characteristic produces a more compressed, sustainy lead tone
     * compared to the TS-808's softer asymmetric saturation. Tone at 50%
     * (noon) keeps the sound balanced. Level at 55% lifts above rhythm.
     * Long reverb tail adds singing sustain during held notes.
     *
     * Best with: Neck pickup, single notes, slow vibrato by hand.
     */
    private fun createTS9Lead(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to true,   // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel
            6 to false,  // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to true,  // Delay (dotted-eighth slapback for leads)
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false, // Fuzzrite
            27 to false, // MF-102 Ring Mod
            28 to false, // Space Echo
            29 to false, // DL4 Delay
            30 to false, // Boss BD-2
            31 to true   // TubeScreamer
        )

        val params = mapOf(
            // NoiseGate: moderate -- TS9 high drive needs gating
            Pair(0, 0) to -40f,
            Pair(0, 1) to 7f,
            Pair(0, 2) to 1f,
            Pair(0, 3) to 30f,
            // Compressor: moderate for even sustain across strings
            Pair(1, 0) to -22f,
            Pair(1, 1) to 4f,
            Pair(1, 2) to 8f,
            Pair(1, 3) to 100f,
            Pair(1, 4) to 4f,
            Pair(1, 5) to 4f,
            // AmpModel: crunch pushed into lead territory
            Pair(5, 0) to 0.65f,
            Pair(5, 1) to 0.65f,
            Pair(5, 2) to 1f,     // crunch
            Pair(5, 3) to 0f,
            // Delay: 375ms, low feedback -- single repeat for depth
            Pair(12, 0) to 375f,  // delayTimeMs (dotted eighth at ~100bpm)
            Pair(12, 1) to 0.20f, // feedback (one echo)
            // Reverb: Hall type, longer decay -- singing sustain
            Pair(13, 8) to 1f,    // type (Hall)
            Pair(13, 0) to 0.70f, // decay (long)
            Pair(13, 6) to 0.5f,  // tone
            // TubeScreamer: TS9 compressed lead
            Pair(31, 0) to 0.75f, // Drive (high -- TS9 hard clip character)
            Pair(31, 1) to 0.50f, // Tone (noon -- balanced)
            Pair(31, 2) to 0.55f, // Level (slightly above unity)
            Pair(31, 3) to 1.0f   // Mode (TS9)
        )

        val wetDry = mapOf(
            12 to 0.3f,  // Delay -- moderate, supporting lead
            13 to 0.35f  // Reverb -- generous for sustain bloom
        )

        return Preset(
            id = "factory-ts9-lead",
            name = "TS9 Lead",
            author = "Factory",
            category = PresetCategory.CRUNCH,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * TS Preset 7: "TS Stack"
     * Designed for stacking before another drive pedal. Drive at 60%
     * provides harmonic content for the downstream stage to further shape.
     * Tone at 40% is darker than usual to compensate for treble added by
     * the downstream drive. Level at 40% keeps the signal chain from
     * clipping the next stage too aggressively. The Overdrive is enabled
     * at a low drive setting to simulate the second pedal in the chain.
     *
     * Best with: Use as a "third pedal" in front of another drive;
     * the stacked harmonics produce extra-dimensional overdrive.
     */
    private fun createTSStack(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to false,  // Compressor (off -- stack dynamics matter)
            2 to false,  // Wah
            3 to false,  // Boost
            4 to true,   // Overdrive (second stage in the stack)
            5 to true,   // AmpModel
            6 to false,  // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false, // Fuzzrite
            27 to false, // MF-102 Ring Mod
            28 to false, // Space Echo
            29 to false, // DL4 Delay
            30 to false, // Boss BD-2
            31 to true   // TubeScreamer (first stage of the stack)
        )

        val params = mapOf(
            // NoiseGate: fairly aggressive -- stacked drives add noise
            Pair(0, 0) to -38f,
            Pair(0, 1) to 7f,
            Pair(0, 2) to 1f,
            Pair(0, 3) to 25f,
            // Overdrive: second pedal in stack -- low drive, matches TS output
            Pair(4, 0) to 0.35f,  // drive (low second stage)
            Pair(4, 1) to 0.50f,  // tone (neutral)
            Pair(4, 2) to 0.55f,  // level (slight push)
            // AmpModel: crunch -- stack feeds into natural amp saturation
            Pair(5, 0) to 0.55f,
            Pair(5, 1) to 0.65f,
            Pair(5, 2) to 1f,     // crunch
            Pair(5, 3) to 0f,
            // Reverb: Room type, short -- stacked drives need tight reverb
            Pair(13, 8) to 0f,    // type (Room)
            Pair(13, 0) to 0.35f, // decay (short)
            Pair(13, 6) to 0.45f, // tone (slightly dark)
            // TubeScreamer: TS-808 as first stage, dark tone for stack balance
            Pair(31, 0) to 0.60f, // Drive (medium-high -- harmonic richness)
            Pair(31, 1) to 0.40f, // Tone (dark -- compensates downstream treble)
            Pair(31, 2) to 0.40f, // Level (lower -- won't slam next stage)
            Pair(31, 3) to 0.0f   // Mode (TS-808)
        )

        val wetDry = mapOf(
            13 to 0.2f  // Reverb -- subtle, tight
        )

        return Preset(
            id = "factory-ts-stack",
            name = "TS Stack",
            author = "Factory",
            category = PresetCategory.CRUNCH,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * TS Preset 8: "TS Warm Neck"
     * Tailored for humbuckers or Strat neck pickup where brightness is not
     * a priority. Tone at 35% is significantly below noon, rolling off high
     * frequencies to preserve the inherent warmth of the pickup. Drive at 35%
     * adds body without thinning out the low-mids. Level at 55% compensates
     * for the darker tone's perceived volume loss.
     *
     * Best with: Neck pickup, humbucker or single coil, jazz/blues leads.
     */
    private fun createTSWarmNeck(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to true,   // Compressor
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel
            6 to false,  // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to false, // Delay
            13 to true,  // Reverb
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false, // Fuzzrite
            27 to false, // MF-102 Ring Mod
            28 to false, // Space Echo
            29 to false, // DL4 Delay
            30 to false, // Boss BD-2
            31 to true   // TubeScreamer
        )

        val params = mapOf(
            // NoiseGate
            Pair(0, 0) to -44f,
            Pair(0, 1) to 5f,
            Pair(0, 2) to 1f,
            Pair(0, 3) to 45f,
            // Compressor: smooth for warm legato sustain
            Pair(1, 0) to -18f,
            Pair(1, 1) to 3f,
            Pair(1, 2) to 15f,
            Pair(1, 3) to 180f,
            Pair(1, 4) to 3f,
            Pair(1, 5) to 6f,
            // AmpModel: clean -- dark TS tone stays clean without more grit
            Pair(5, 0) to 0.5f,
            Pair(5, 1) to 0.65f,
            Pair(5, 2) to 0f,     // clean (keep warmth intact)
            Pair(5, 3) to 0f,
            // Reverb: Hall type, medium -- warm, spacious feel
            Pair(13, 8) to 1f,    // type (Hall)
            Pair(13, 0) to 0.60f, // decay (medium-long)
            Pair(13, 6) to 0.4f,  // tone (dark reverb to match)
            // TubeScreamer: TS-808 warm/dark for humbuckers and neck singles
            Pair(31, 0) to 0.35f, // Drive (moderate -- body without thin)
            Pair(31, 1) to 0.35f, // Tone (dark -- warmth preserved)
            Pair(31, 2) to 0.55f, // Level (slight volume comp for dark tone)
            Pair(31, 3) to 0.0f   // Mode (TS-808)
        )

        val wetDry = mapOf(
            13 to 0.35f  // Reverb -- warm bath of space
        )

        return Preset(
            id = "factory-ts-warm-neck",
            name = "TS Warm Neck",
            author = "Factory",
            category = PresetCategory.CRUNCH,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }

    /**
     * TS Preset 9: "TS Frusciante Lead"
     * TS-808 dialed for single-coil singing sustain in the style of
     * neck-pickup Strat leads. Drive and tone both at 50% (noon) is
     * the classic "set it and forget it" Tube Screamer position --
     * the mid-hump does the work of cutting through without added
     * brightness or darkness. A long Hall reverb tail with moderate
     * delay lets held notes bloom and sustain naturally. Light
     * compressor maintains consistency while preserving pick feel.
     *
     * Best with: Strat neck or neck-middle, single notes and
     * melodic phrases. Roll guitar volume slightly for dynamics.
     */
    private fun createTSFrusciante(): Preset {
        val enabled = mapOf(
            0 to true,   // NoiseGate
            1 to true,   // Compressor (light -- sustain without killing dynamics)
            2 to false,  // Wah
            3 to false,  // Boost
            4 to false,  // Overdrive
            5 to true,   // AmpModel
            6 to false,  // CabinetSim
            7 to false,  // ParametricEQ
            8 to false,  // Chorus
            9 to false,  // Vibrato
            10 to false, // Phaser
            11 to false, // Flanger
            12 to true,  // Delay (adds dimension to single notes)
            13 to true,  // Reverb (long bloom)
            14 to true,  // Tuner
            15 to false, // Tremolo
            16 to false, // Boss DS-1
            17 to false, // ProCo RAT
            18 to false, // Boss DS-2
            19 to false, // Boss HM-2
            20 to false, // Univibe
            21 to false, // Fuzz Face
            22 to false, // Rangemaster
            23 to false, // Big Muff Pi
            24 to false, // Octavia
            25 to false, // Rotary Speaker
            26 to false, // Fuzzrite
            27 to false, // MF-102 Ring Mod
            28 to false, // Space Echo
            29 to false, // DL4 Delay
            30 to false, // Boss BD-2
            31 to true   // TubeScreamer
        )

        val params = mapOf(
            // NoiseGate: moderate -- single coils need it but not too tight
            Pair(0, 0) to -44f,
            Pair(0, 1) to 5f,
            Pair(0, 2) to 2f,
            Pair(0, 3) to 50f,
            // Compressor: gentle for long sustain, preserve pick attack
            Pair(1, 0) to -22f,
            Pair(1, 1) to 3f,
            Pair(1, 2) to 15f,
            Pair(1, 3) to 200f,
            Pair(1, 4) to 3f,
            Pair(1, 5) to 6f,
            // AmpModel: crunch (TS mid-hump drives it to natural breakup)
            Pair(5, 0) to 0.6f,
            Pair(5, 1) to 0.65f,
            Pair(5, 2) to 1f,     // crunch
            Pair(5, 3) to 0f,
            // Delay: ~330ms at low feedback, single echo for dimension
            Pair(12, 0) to 330f,  // delayTimeMs
            Pair(12, 1) to 0.18f, // feedback (single warm repeat)
            // Reverb: Hall type, long decay -- note bloom and air
            Pair(13, 8) to 1f,    // type (Hall)
            Pair(13, 0) to 0.72f, // decay (long -- singing sustain)
            Pair(13, 6) to 0.55f, // tone (slightly bright -- single-coil air)
            // TubeScreamer: TS-808 noon settings, Strat single-coil sustain
            Pair(31, 0) to 0.55f, // Drive (just past noon -- singing sustain)
            Pair(31, 1) to 0.50f, // Tone (noon -- perfectly balanced)
            Pair(31, 2) to 0.50f, // Level (unity)
            Pair(31, 3) to 0.0f   // Mode (TS-808)
        )

        val wetDry = mapOf(
            12 to 0.28f, // Delay -- present but not dominant
            13 to 0.4f   // Reverb -- generous bloom for sustain
        )

        return Preset(
            id = "factory-ts-frusciante-lead",
            name = "TS Frusciante Lead",
            author = "Factory",
            category = PresetCategory.CRUNCH,
            effects = buildEffectChain(enabled, params, wetDry),
            isFactory = true
        )
    }
}
