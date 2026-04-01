package com.guitaremulator.app.data

import android.content.Context
import android.util.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/**
 * Manages drum pattern persistence, loading, and serialization.
 *
 * Patterns are stored as individual JSON files in the app's internal storage
 * at [filesDir]/drum_patterns/. Factory patterns are generated on first run
 * if the directory is empty.
 *
 * Uses [org.json] for serialization, matching the existing PresetManager
 * approach (no external dependencies).
 *
 * All file I/O operations are synchronous and should be called from a
 * background thread or coroutine.
 *
 * @param context Application context for accessing file storage.
 */
class DrumRepository(private val context: Context) {

    companion object {
        private const val TAG = "DrumRepository"
        private const val PATTERNS_DIR = "drum_patterns"
        private const val JSON_EXTENSION = ".json"
    }

    /** Directory where drum patterns are stored. */
    private val patternsDir: File by lazy {
        File(context.filesDir, PATTERNS_DIR).also { dir ->
            if (!dir.exists()) {
                dir.mkdirs()
            }
        }
    }

    // =========================================================================
    // Public API
    // =========================================================================

    /**
     * Save a drum pattern to internal storage as a JSON file.
     *
     * @param pattern The pattern to save.
     * @return True if saved successfully, false on error.
     */
    fun savePattern(pattern: DrumPatternData): Boolean {
        return try {
            val json = patternToJson(pattern)
            val file = File(patternsDir, pattern.id + JSON_EXTENSION)
            file.writeText(json.toString(2))
            Log.d(TAG, "Saved drum pattern: ${pattern.name} (${pattern.id})")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to save drum pattern: ${pattern.name}", e)
            false
        }
    }

    /**
     * Load a drum pattern by its unique ID.
     *
     * @param id UUID string of the pattern.
     * @return The loaded [DrumPatternData] or null if not found or corrupted.
     */
    fun loadPattern(id: String): DrumPatternData? {
        return try {
            val file = File(patternsDir, id + JSON_EXTENSION)
            if (!file.exists()) {
                Log.w(TAG, "Drum pattern file not found: $id")
                return null
            }
            val json = JSONObject(file.readText())
            jsonToPattern(json)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load drum pattern: $id", e)
            null
        }
    }

    /**
     * Delete a drum pattern by its unique ID.
     * Factory patterns cannot be deleted.
     *
     * @param id UUID string of the pattern to delete.
     * @return True if deleted successfully.
     */
    fun deletePattern(id: String): Boolean {
        val file = File(patternsDir, id + JSON_EXTENSION)
        if (!file.exists()) return false

        // Protect factory patterns
        try {
            val json = JSONObject(file.readText())
            if (json.optBoolean("isFactory", false)) {
                Log.w(TAG, "Cannot delete factory drum pattern: $id")
                return false
            }
        } catch (_: Exception) {
            // If parse fails, allow deletion
        }

        return file.delete().also { success ->
            if (success) Log.d(TAG, "Deleted drum pattern: $id")
            else Log.w(TAG, "Failed to delete drum pattern file: $id")
        }
    }

    /**
     * List all saved drum patterns, sorted by name.
     *
     * @return List of all patterns in storage. Returns empty list on error.
     */
    fun getPatternList(): List<DrumPatternData> {
        val patterns = mutableListOf<DrumPatternData>()
        val files = patternsDir.listFiles { file ->
            file.extension == "json"
        } ?: return emptyList()

        for (file in files) {
            try {
                val json = JSONObject(file.readText())
                jsonToPattern(json)?.let { patterns.add(it) }
            } catch (e: Exception) {
                Log.w(TAG, "Skipping corrupted drum pattern file: ${file.name}", e)
            }
        }

        // Factory patterns first, then by name
        return patterns.sortedWith(compareByDescending<DrumPatternData> { it.isFactory }
            .thenBy { it.name })
    }

    /**
     * Load factory drum patterns into storage.
     *
     * Called on every app launch to ensure factory patterns reflect the
     * latest definitions. Overwrites existing factory files.
     *
     * @return Number of factory patterns loaded.
     */
    fun loadFactoryPresets(): Int {
        val factoryPatterns = createFactoryPatterns()
        var loaded = 0

        for (pattern in factoryPatterns) {
            if (savePattern(pattern)) loaded++
        }

        Log.d(TAG, "Loaded $loaded factory drum patterns")
        return loaded
    }

    // =========================================================================
    // JSON Serialization
    // =========================================================================

    /**
     * Serialize a [DrumPatternData] to a [JSONObject].
     */
    internal fun patternToJson(pattern: DrumPatternData): JSONObject {
        val json = JSONObject()
        json.put("id", pattern.id)
        json.put("version", pattern.version)
        json.put("name", pattern.name)
        json.put("length", pattern.length)
        json.put("swing", pattern.swing.toDouble())
        json.put("bpm", pattern.bpm.toDouble())
        json.put("isFactory", pattern.isFactory)

        val tracksArray = JSONArray()
        for (track in pattern.tracks) {
            val trackJson = JSONObject()
            trackJson.put("length", track.length)
            trackJson.put("engineType", track.engineType)
            trackJson.put("volume", track.volume.toDouble())
            trackJson.put("pan", track.pan.toDouble())
            trackJson.put("muted", track.muted)
            trackJson.put("chokeGroup", track.chokeGroup)
            trackJson.put("drive", track.drive.toDouble())
            trackJson.put("filterCutoff", track.filterCutoff.toDouble())
            trackJson.put("filterRes", track.filterRes.toDouble())

            // Voice params
            val paramsArray = JSONArray()
            for (p in track.voiceParams) {
                paramsArray.put(p.toDouble())
            }
            trackJson.put("voiceParams", paramsArray)

            // Steps
            val stepsArray = JSONArray()
            for (step in track.steps) {
                val stepJson = JSONObject()
                stepJson.put("trigger", step.trigger)
                stepJson.put("velocity", step.velocity)
                stepJson.put("probability", step.probability)
                stepJson.put("retrigCount", step.retrigCount)
                stepJson.put("retrigDecay", step.retrigDecay)
                stepsArray.put(stepJson)
            }
            trackJson.put("steps", stepsArray)

            tracksArray.put(trackJson)
        }
        json.put("tracks", tracksArray)

        return json
    }

    /**
     * Deserialize a [JSONObject] into a [DrumPatternData].
     *
     * @param json The JSON object to parse.
     * @return The deserialized pattern or null if malformed.
     */
    internal fun jsonToPattern(json: JSONObject): DrumPatternData? {
        return try {
            val tracksArray = json.getJSONArray("tracks")
            val tracks = mutableListOf<DrumTrackData>()

            for (t in 0 until tracksArray.length()) {
                val trackJson = tracksArray.getJSONObject(t)

                // Parse voice params
                val paramsArray = trackJson.optJSONArray("voiceParams")
                val voiceParams = if (paramsArray != null) {
                    (0 until paramsArray.length()).map { paramsArray.getDouble(it).toFloat() }
                } else {
                    List(16) { 0f }
                }

                // Parse steps
                val stepsArray = trackJson.optJSONArray("steps")
                val steps = if (stepsArray != null) {
                    (0 until stepsArray.length()).map { i ->
                        val stepJson = stepsArray.getJSONObject(i)
                        DrumStep(
                            trigger = stepJson.optBoolean("trigger", false),
                            velocity = stepJson.optInt("velocity", 100),
                            probability = stepJson.optInt("probability", 100),
                            retrigCount = stepJson.optInt("retrigCount", 0),
                            retrigDecay = stepJson.optInt("retrigDecay", 0)
                        )
                    }
                } else {
                    List(16) { DrumStep() }
                }

                tracks.add(
                    DrumTrackData(
                        steps = steps,
                        length = trackJson.optInt("length", 16),
                        engineType = trackJson.optInt("engineType", 0),
                        voiceParams = voiceParams,
                        volume = trackJson.optDouble("volume", 0.8).toFloat(),
                        pan = trackJson.optDouble("pan", 0.0).toFloat(),
                        muted = trackJson.optBoolean("muted", false),
                        chokeGroup = trackJson.optInt("chokeGroup", -1),
                        drive = trackJson.optDouble("drive", 0.0).toFloat(),
                        filterCutoff = trackJson.optDouble("filterCutoff", 16000.0).toFloat(),
                        filterRes = trackJson.optDouble("filterRes", 0.0).toFloat()
                    )
                )
            }

            DrumPatternData(
                id = json.getString("id"),
                version = json.optInt("version", 1),
                tracks = tracks,
                length = json.optInt("length", 16),
                swing = json.optDouble("swing", 50.0).toFloat(),
                bpm = json.optDouble("bpm", 120.0).toFloat(),
                name = json.optString("name", "Untitled"),
                isFactory = json.optBoolean("isFactory", false)
            )
        } catch (e: Exception) {
            Log.e(TAG, "Failed to parse drum pattern JSON", e)
            null
        }
    }

    // =========================================================================
    // Factory patterns -- GROOVE-PROGRAMMED presets
    // =========================================================================
    //
    // 16 genre-authentic presets + 1 empty = 17 total.
    // Each preset has real groove: ghost notes, velocity curves, proper swing,
    // and genre-specific kit voicing.
    //
    // 16th-note grid mapping (4/4 time, 16 steps per bar):
    //   Beat 1: step 0 (1), step 1 (e), step  2 (&), step  3 (a)
    //   Beat 2: step 4 (2), step 5 (e), step  6 (&), step  7 (a)
    //   Beat 3: step 8 (3), step 9 (e), step 10 (&), step 11 (a)
    //   Beat 4: step 12(4), step 13(e), step 14 (&), step 15 (a)
    //
    // Track layout: 0=Kick, 1=Snare, 2=Closed Hat, 3=Open Hat,
    //               4=Clap, 5=Rim, 6=Shaker, 7=Cowbell
    //
    // Voice parameter indices by engine type:
    //   FM (0): 0=freq, 1=modRatio, 2=modDepth, 3=pitchEnvDepth,
    //           4=pitchDecay(ms), 5=ampDecay(ms), 6=filterCutoff,
    //           7=filterRes, 8=drive
    //   Subtractive (1): 0=toneFreq, 1=toneDecay(ms), 2=noiseDecay(ms),
    //           3=noiseMix, 4=filterFreq, 5=filterRes, 6=filterDecay(ms),
    //           7=filterEnvDepth, 8=drive
    //   Metallic (2): 0=color, 1=tone, 2=decay(ms), 3=hpCutoff
    //   Noise (3): 0=noiseType, 1=filterMode, 2=filterFreq, 3=filterRes,
    //           4=decay(ms), 5=sweep
    //   Physical (4): 0=modelType, 1=pitch(MIDI), 2=decay, 3=brightness,
    //           4=material, 5=damping
    // =========================================================================

    /**
     * Create built-in factory drum patterns with full voice parameters
     * and step data for each genre. IDs are deterministic so factory
     * patterns are overwritten on app updates.
     *
     * 8 genres x 2 patterns each = 16, plus 1 empty = 17 presets.
     */
    private fun createFactoryPatterns(): List<DrumPatternData> {
        return listOf(
            // Rock (2)
            createRockSteady(),
            createDrivingRock(),
            // Funk (2)
            createTightFunk(),
            createPFunkGroove(),
            // Hip-Hop / Lo-Fi (2)
            createBoomBap(),
            createLoFiChill(),
            // Blues (2)
            createBluesShuffle(),
            createSlowBlues(),
            // Jazz (2)
            createJazzRide(),
            createBossaNova(),
            // Electronic (2)
            createFourOnTheFloor(),
            createTrap(),
            // Metal (2)
            createThrash(),
            createHalfTimeMetal(),
            // Empty
            createEmptyPattern()
        )
    }

    // =====================================================================
    // Shared voice param builders (pad to 16 floats)
    // =====================================================================

    /** Build a 16-element voice param list from the given values, zero-padded. */
    private fun voiceParams(vararg values: Float): List<Float> {
        return List(16) { i -> if (i < values.size) values[i] else 0f }
    }

    // =====================================================================
    // Genre-specific voice parameters -- EVERY voice is unique per genre.
    //
    // Engine type key: 0=FM, 1=Subtractive, 2=Metallic, 3=Noise, 4=Physical
    //
    // FM params:   freq, modRatio, modDepth, pitchEnvDepth, pitchDecay(ms),
    //              ampDecay(ms), filterCutoff, filterRes, drive
    // SUB params:  toneFreq, toneDecay(ms), noiseDecay(ms), noiseMix,
    //              filterFreq, filterRes, filterDecay(ms), filterEnvDepth, drive
    // METAL params: color, tone, decay(ms), hpCutoff
    // NOISE params: noiseType(0=white,1=pink), filterMode(0=LP,1=HP,2=BP),
    //              filterFreq, filterRes, decay(ms), sweep
    // PHYS params: modelType(0=KS,1=Modal), pitch(MIDI), decay, brightness,
    //              material, damping
    //
    // Per-track post-processing (drive, filterCutoff, filterRes) creates the
    // genre's overall timbral character on TOP of voice synthesis:
    //   Lo-fi: heavy drive + low cutoff = dusty warmth
    //   Jazz: zero drive + wide open filter = pristine clarity
    //   Metal: moderate drive + high cutoff = bright aggression
    //   Trap: mild drive + varied per-voice = modern punch
    // =====================================================================

    // =====================================================================
    // 808 TRAP -- sub-bass boom, crispy hats, modern production
    // =====================================================================

    /** Trap kit: 808 sub-bass kick, crunchy snare, crispy hats. */
    private fun trapKit(): List<DrumTrackData> = listOf(
        // KICK: FM, 40Hz sub-bass territory, huge pitch sweep, LONG 800ms boom
        // filterCutoff=800 removes all highs for pure sub-bass rumble
        DrumTrackData(engineType = 0, volume = 0.95f,
            drive = 0.5f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(
                40f, 1.0f, 2.5f, 5.0f, 45f, 800f, 800f, 0f, 0.5f)),
        // SNARE: SUB, 180Hz body, noise-heavy (0.7), crunchy drive=1.5
        DrumTrackData(engineType = 1, volume = 0.85f,
            drive = 0.5f, filterCutoff = 8000f, filterRes = 0f,
            voiceParams = voiceParams(
                180f, 60f, 250f, 0.7f, 4000f, 0.3f, 80f, 2.5f, 1.5f)),
        // CLOSED HAT: METAL, ultra-tight 25ms, crispy bright hpCutoff=10000
        DrumTrackData(engineType = 2, volume = 0.7f, chokeGroup = 0,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0.4f, 0.7f, 25f, 10000f)),
        // OPEN HAT: METAL, 400ms ring, bright hpCutoff=8000
        DrumTrackData(engineType = 2, volume = 0.7f, chokeGroup = 0,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0.3f, 0.6f, 400f, 8000f)),
        // CLAP: SUB, almost pure noise (0.95), 200ms decay, resonant filter
        DrumTrackData(engineType = 1, volume = 0.75f,
            drive = 0.3f, filterCutoff = 10000f, filterRes = 0f,
            voiceParams = voiceParams(
                150f, 10f, 200f, 0.95f, 2500f, 0.6f, 60f, 2.0f, 0.5f)),
        // RIM: PHYS KS, sharp click, MIDI 80, very short, max brightness
        DrumTrackData(engineType = 4, volume = 0.7f,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0f, 80f, 0.1f, 1.0f, 0.2f, 0.5f)),
        // SHAKER: NOISE pink, LP 5000Hz, short 40ms
        DrumTrackData(engineType = 3, volume = 0.5f,
            drive = 0f, filterCutoff = 12000f, filterRes = 0f,
            voiceParams = voiceParams(1f, 0f, 5000f, 0.2f, 40f, 0.2f)),
        // COWBELL: PHYS Modal, MIDI 74, short decay
        DrumTrackData(engineType = 4, volume = 0.6f,
            drive = 0f, filterCutoff = 14000f, filterRes = 0f,
            voiceParams = voiceParams(1f, 74f, 0.2f, 0.7f, 0f, 0.3f))
    )

    // =====================================================================
    // LO-FI HIP HOP -- warm, saturated, dark, imperfect (dusty sampler)
    // =====================================================================

    /** Lo-fi kit: EVERYTHING dark (low cutoffs), saturated (high drive), shorter decays. */
    private fun lofiKit(): List<DrumTrackData> = listOf(
        // KICK: FM, 55Hz (warmer than trap's 40Hz), heavy drive=3.0, dark filter=1200
        // The saturation + low-pass is the "lo-fi warmth" character
        DrumTrackData(engineType = 0, volume = 0.9f,
            drive = 2.0f, filterCutoff = 4000f, filterRes = 0.1f,
            voiceParams = voiceParams(
                55f, 1.0f, 1.5f, 3.0f, 30f, 400f, 1200f, 0.3f, 3.0f)),
        // SNARE: SUB, 220Hz, balanced mix (0.5), VERY dark filter=2500, saturated drive=2.5
        DrumTrackData(engineType = 1, volume = 0.8f,
            drive = 2.5f, filterCutoff = 3500f, filterRes = 0.1f,
            voiceParams = voiceParams(
                220f, 60f, 120f, 0.5f, 2500f, 0.2f, 50f, 1.0f, 2.5f)),
        // CLOSED HAT: METAL, 35ms, dark hpCutoff=5000 (NOT bright -- lo-fi hats are muffled)
        DrumTrackData(engineType = 2, volume = 0.6f, chokeGroup = 0,
            drive = 1.5f, filterCutoff = 5000f, filterRes = 0f,
            voiceParams = voiceParams(0.5f, 0.3f, 35f, 5000f)),
        // OPEN HAT: METAL, 200ms, very dark hpCutoff=4000
        DrumTrackData(engineType = 2, volume = 0.6f, chokeGroup = 0,
            drive = 1.5f, filterCutoff = 5000f, filterRes = 0f,
            voiceParams = voiceParams(0.4f, 0.25f, 200f, 4000f)),
        // CLAP: SUB, noise-heavy (0.8), short 100ms, very dark filter=2000, saturated
        DrumTrackData(engineType = 1, volume = 0.7f,
            drive = 2.0f, filterCutoff = 3000f, filterRes = 0f,
            voiceParams = voiceParams(
                140f, 15f, 100f, 0.8f, 2000f, 0.3f, 40f, 1.5f, 2.0f)),
        // RIM: PHYS KS, dull (brightness=0.4), lower pitch MIDI 70
        DrumTrackData(engineType = 4, volume = 0.6f,
            drive = 1.0f, filterCutoff = 4000f, filterRes = 0f,
            voiceParams = voiceParams(0f, 70f, 0.2f, 0.4f, 0.1f, 0.4f)),
        // SHAKER: NOISE pink, very dark LP 3000Hz, 50ms, slight sweep
        DrumTrackData(engineType = 3, volume = 0.45f,
            drive = 1.5f, filterCutoff = 3000f, filterRes = 0f,
            voiceParams = voiceParams(1f, 0f, 3000f, 0.2f, 50f, 0.2f)),
        // COWBELL: PHYS Modal, MIDI 72, dull brightness=0.4
        DrumTrackData(engineType = 4, volume = 0.55f,
            drive = 1.0f, filterCutoff = 4000f, filterRes = 0f,
            voiceParams = voiceParams(1f, 72f, 0.25f, 0.4f, 0f, 0.4f))
    )

    // =====================================================================
    // ROCK -- punchy, bright, in-your-face, clear attack
    // =====================================================================

    /** Rock kit: punchy kick click, snappy bright snare, clear hats. */
    private fun rockKit(): List<DrumTrackData> = listOf(
        // KICK: FM, 60Hz, fast 8ms pitch decay (punchy beater click), 250ms body
        // Bright filter=5000 lets the attack through, moderate drive=0.8
        DrumTrackData(engineType = 0, volume = 0.9f,
            drive = 0.5f, filterCutoff = 12000f, filterRes = 0f,
            voiceParams = voiceParams(
                60f, 1.2f, 2.0f, 3.5f, 8f, 250f, 5000f, 0.1f, 0.8f)),
        // SNARE: SUB, 200Hz, balanced tone/noise (0.55), bright filter=6000
        DrumTrackData(engineType = 1, volume = 0.85f,
            drive = 0.3f, filterCutoff = 14000f, filterRes = 0f,
            voiceParams = voiceParams(
                200f, 50f, 180f, 0.55f, 6000f, 0.2f, 55f, 3.0f, 0.5f)),
        // CLOSED HAT: METAL, 40ms, bright hpCutoff=7000
        DrumTrackData(engineType = 2, volume = 0.7f, chokeGroup = 0,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0.3f, 0.6f, 40f, 7000f)),
        // OPEN HAT: METAL, 350ms, hpCutoff=5500
        DrumTrackData(engineType = 2, volume = 0.7f, chokeGroup = 0,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0.35f, 0.5f, 350f, 5500f)),
        // CLAP: SUB, layered crack, moderate noise
        DrumTrackData(engineType = 1, volume = 0.7f,
            drive = 0.3f, filterCutoff = 12000f, filterRes = 0f,
            voiceParams = voiceParams(
                170f, 20f, 140f, 0.7f, 5000f, 0.3f, 60f, 2.5f, 0.5f)),
        // RIM: PHYS KS, bright click MIDI 76
        DrumTrackData(engineType = 4, volume = 0.7f,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0f, 76f, 0.15f, 0.8f, 0.2f, 0.3f)),
        // SHAKER: NOISE white, LP 7000Hz (bright), 60ms
        DrumTrackData(engineType = 3, volume = 0.55f,
            drive = 0f, filterCutoff = 14000f, filterRes = 0f,
            voiceParams = voiceParams(0f, 0f, 7000f, 0.2f, 60f, 0.3f)),
        // COWBELL: PHYS Modal, MIDI 74
        DrumTrackData(engineType = 4, volume = 0.6f,
            drive = 0f, filterCutoff = 14000f, filterRes = 0f,
            voiceParams = voiceParams(1f, 74f, 0.3f, 0.7f, 0f, 0.3f))
    )

    // =====================================================================
    // FUNK -- tight, snappy, high dynamics, clean articulation
    // =====================================================================

    /** Funk kit: tight 200ms kick, snappy bright snare, clean hats for ghost notes. */
    private fun funkKit(): List<DrumTrackData> = listOf(
        // KICK: FM, 55Hz, tight 200ms decay, moderate drive=0.6, open filter=4000
        DrumTrackData(engineType = 0, volume = 0.9f,
            drive = 0.3f, filterCutoff = 14000f, filterRes = 0f,
            voiceParams = voiceParams(
                55f, 1.0f, 2.0f, 3.0f, 10f, 200f, 4000f, 0.1f, 0.6f)),
        // SNARE: SUB, 210Hz, snappy bright filter=7000, low drive=0.3 for dynamics
        DrumTrackData(engineType = 1, volume = 0.85f,
            drive = 0.2f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(
                210f, 40f, 130f, 0.5f, 7000f, 0.2f, 45f, 3.0f, 0.3f)),
        // CLOSED HAT: METAL, tight 30ms, bright hpCutoff=8000
        DrumTrackData(engineType = 2, volume = 0.7f, chokeGroup = 0,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0.25f, 0.7f, 30f, 8000f)),
        // OPEN HAT: METAL, 250ms, hpCutoff=6000
        DrumTrackData(engineType = 2, volume = 0.65f, chokeGroup = 0,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0.3f, 0.55f, 250f, 6000f)),
        // CLAP: SUB, sharp and short (80ms), noise-heavy (0.9)
        DrumTrackData(engineType = 1, volume = 0.7f,
            drive = 0f, filterCutoff = 14000f, filterRes = 0f,
            voiceParams = voiceParams(
                160f, 10f, 80f, 0.9f, 4000f, 0.3f, 30f, 2.0f, 0.2f)),
        // RIM: PHYS KS, snappy MIDI 78, bright, very short
        DrumTrackData(engineType = 4, volume = 0.7f,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0f, 78f, 0.12f, 0.9f, 0.15f, 0.3f)),
        // SHAKER: NOISE white, brighter LP 8000Hz, 50ms
        DrumTrackData(engineType = 3, volume = 0.55f,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0f, 0f, 8000f, 0.15f, 50f, 0.3f)),
        // COWBELL: PHYS Modal, MIDI 76
        DrumTrackData(engineType = 4, volume = 0.6f,
            drive = 0f, filterCutoff = 14000f, filterRes = 0f,
            voiceParams = voiceParams(1f, 76f, 0.3f, 0.8f, 0f, 0.25f))
    )

    // =====================================================================
    // JAZZ -- light, airy, acoustic-feeling, pristine clean
    // =====================================================================

    /** Jazz kit: feather-light clean kick, brush snare, ride cymbal, zero drive. */
    private fun jazzKit(): List<DrumTrackData> = listOf(
        // KICK: FM, 70Hz (higher, less sub), VERY short 100ms, NO drive, clean
        // Subtle modDepth=0.5, clean filter=3000 -- like a felt beater on a jazz kit
        DrumTrackData(engineType = 0, volume = 0.75f,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(
                70f, 1.0f, 0.5f, 2.0f, 5f, 100f, 3000f, 0f, 0f)),
        // SNARE: SUB, 250Hz (higher), more tone than noise (0.35) for brush character
        // NO drive, clean filter=4000 -- open, natural
        DrumTrackData(engineType = 1, volume = 0.75f,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(
                250f, 30f, 80f, 0.35f, 4000f, 0.15f, 40f, 1.5f, 0f)),
        // RIDE (in CHat slot): METAL, LONG 300ms ring, lower hpCutoff=4000
        // No choke group -- ride should ring freely
        DrumTrackData(engineType = 2, volume = 0.75f, chokeGroup = -1,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0.15f, 0.4f, 300f, 4000f)),
        // CRASH/OPEN HAT: METAL, 600ms (crash-like), lower hpCutoff=3500
        DrumTrackData(engineType = 2, volume = 0.6f, chokeGroup = -1,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0.2f, 0.35f, 600f, 3500f)),
        // CLAP: not prominent in jazz -- very quiet default
        DrumTrackData(engineType = 1, volume = 0.4f,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(
                160f, 15f, 60f, 0.6f, 3000f, 0.15f, 30f, 1.0f, 0f)),
        // RIM (cross-stick): PHYS KS, MIDI 72, very short 0.08, clean brightness=0.7
        DrumTrackData(engineType = 4, volume = 0.7f,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0f, 72f, 0.08f, 0.7f, 0.1f, 0.3f)),
        // BRUSH: NOISE pink, LP 4000Hz, longer 100ms (sweepy brush feel)
        DrumTrackData(engineType = 3, volume = 0.5f,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(1f, 0f, 4000f, 0.15f, 100f, 0.4f)),
        // COWBELL: PHYS Modal, MIDI 70 (lower pitch)
        DrumTrackData(engineType = 4, volume = 0.55f,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(1f, 70f, 0.25f, 0.5f, 0f, 0.35f))
    )

    // =====================================================================
    // BLUES -- warm, shuffly, organic, moderate saturation
    // =====================================================================

    /** Blues kit: warm round kick, tone-heavy snare, relaxed hats. */
    private fun bluesKit(): List<DrumTrackData> = listOf(
        // KICK: FM, 58Hz, moderate 300ms, warm drive=0.4, filter=3500
        DrumTrackData(engineType = 0, volume = 0.85f,
            drive = 0.3f, filterCutoff = 10000f, filterRes = 0f,
            voiceParams = voiceParams(
                58f, 1.0f, 1.5f, 3.0f, 12f, 300f, 3500f, 0.1f, 0.4f)),
        // SNARE: SUB, 190Hz, more tone than noise (0.45), warm filter=4500
        DrumTrackData(engineType = 1, volume = 0.8f,
            drive = 0.2f, filterCutoff = 10000f, filterRes = 0f,
            voiceParams = voiceParams(
                190f, 60f, 160f, 0.45f, 4500f, 0.2f, 60f, 2.0f, 0.3f)),
        // CLOSED HAT: METAL, 45ms, moderate hpCutoff=6000
        DrumTrackData(engineType = 2, volume = 0.65f, chokeGroup = 0,
            drive = 0f, filterCutoff = 12000f, filterRes = 0f,
            voiceParams = voiceParams(0.3f, 0.5f, 45f, 6000f)),
        // OPEN HAT: METAL, 300ms, hpCutoff=5000
        DrumTrackData(engineType = 2, volume = 0.6f, chokeGroup = 0,
            drive = 0f, filterCutoff = 12000f, filterRes = 0f,
            voiceParams = voiceParams(0.3f, 0.45f, 300f, 5000f)),
        // CLAP: not prominent in blues
        DrumTrackData(engineType = 1, volume = 0.55f,
            drive = 0.2f, filterCutoff = 8000f, filterRes = 0f,
            voiceParams = voiceParams(
                150f, 15f, 100f, 0.65f, 3500f, 0.2f, 50f, 1.5f, 0.2f)),
        // RIM: PHYS KS, MIDI 74, moderate brightness=0.75
        DrumTrackData(engineType = 4, volume = 0.7f,
            drive = 0f, filterCutoff = 12000f, filterRes = 0f,
            voiceParams = voiceParams(0f, 74f, 0.15f, 0.75f, 0.15f, 0.3f)),
        // SHAKER: NOISE pink, LP 5000Hz, 70ms
        DrumTrackData(engineType = 3, volume = 0.5f,
            drive = 0f, filterCutoff = 10000f, filterRes = 0f,
            voiceParams = voiceParams(1f, 0f, 5000f, 0.2f, 70f, 0.25f)),
        // COWBELL: PHYS Modal, MIDI 74
        DrumTrackData(engineType = 4, volume = 0.55f,
            drive = 0f, filterCutoff = 12000f, filterRes = 0f,
            voiceParams = voiceParams(1f, 74f, 0.3f, 0.6f, 0f, 0.3f))
    )

    // =====================================================================
    // ELECTRONIC (House / Four-on-the-Floor) -- clean, punchy, modern
    // =====================================================================

    /** Electronic kit: punchy FM kick, synthetic clap, bright clean hats. */
    private fun electroKit(): List<DrumTrackData> = listOf(
        // KICK: FM, 48Hz, big pitch sweep (4 oct), 350ms, punchy drive=1.0
        DrumTrackData(engineType = 0, volume = 0.92f,
            drive = 0.5f, filterCutoff = 14000f, filterRes = 0f,
            voiceParams = voiceParams(
                48f, 1.0f, 2.5f, 4.0f, 15f, 350f, 2000f, 0.1f, 1.0f)),
        // SNARE (as clap): SUB, noise-heavy (0.9), 150ms, resonant filter
        DrumTrackData(engineType = 1, volume = 0.8f,
            drive = 0.5f, filterCutoff = 14000f, filterRes = 0f,
            voiceParams = voiceParams(
                160f, 15f, 150f, 0.9f, 3500f, 0.5f, 60f, 3.0f, 0.8f)),
        // CLOSED HAT: METAL, 30ms, very bright hpCutoff=9000
        DrumTrackData(engineType = 2, volume = 0.7f, chokeGroup = 0,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0.3f, 0.8f, 30f, 9000f)),
        // OPEN HAT: METAL, 250ms, bright hpCutoff=7000
        DrumTrackData(engineType = 2, volume = 0.7f, chokeGroup = 0,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0.25f, 0.65f, 250f, 7000f)),
        // CLAP: SUB, almost pure noise (0.95), 180ms, resonant
        DrumTrackData(engineType = 1, volume = 0.8f,
            drive = 0.3f, filterCutoff = 14000f, filterRes = 0f,
            voiceParams = voiceParams(
                155f, 10f, 180f, 0.95f, 3000f, 0.4f, 70f, 3.5f, 1.0f)),
        // RIM: PHYS KS, higher MIDI 82 (clicky), short
        DrumTrackData(engineType = 4, volume = 0.65f,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0f, 82f, 0.1f, 0.85f, 0.2f, 0.3f)),
        // SHAKER: NOISE white, HP 6000Hz (high-passed shaker), 40ms
        DrumTrackData(engineType = 3, volume = 0.5f,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0f, 1f, 6000f, 0.2f, 40f, 0.3f)),
        // COWBELL: PHYS Modal, MIDI 76
        DrumTrackData(engineType = 4, volume = 0.6f,
            drive = 0f, filterCutoff = 14000f, filterRes = 0f,
            voiceParams = voiceParams(1f, 76f, 0.3f, 0.7f, 0f, 0.25f))
    )

    // =====================================================================
    // METAL -- tight, aggressive, attack-focused, bright
    // =====================================================================

    /** Metal kit: ultra-tight kick, aggressive bright snare, tight hats. */
    private fun metalKit(): List<DrumTrackData> = listOf(
        // KICK: FM, 65Hz, ultra-tight 4ms pitch decay (click attack), short 150ms
        // Bright filter=6000 for beater attack, more FM harmonics (modDepth=3.0)
        DrumTrackData(engineType = 0, volume = 0.92f,
            drive = 1.0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(
                65f, 1.5f, 3.0f, 3.5f, 4f, 150f, 6000f, 0.1f, 1.5f)),
        // SNARE: SUB, 180Hz, noise-heavy (0.6), very bright filter=8000, drive=1.0
        DrumTrackData(engineType = 1, volume = 0.9f,
            drive = 1.0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(
                180f, 40f, 200f, 0.6f, 8000f, 0.25f, 35f, 4.0f, 1.0f)),
        // CLOSED HAT: METAL, ultra-tight 25ms, very bright hpCutoff=9000
        DrumTrackData(engineType = 2, volume = 0.7f, chokeGroup = 0,
            drive = 0.5f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0.35f, 0.7f, 25f, 9000f)),
        // CRASH (in OHat slot): METAL, 200ms, bright hpCutoff=7000
        DrumTrackData(engineType = 2, volume = 0.7f, chokeGroup = 0,
            drive = 0.3f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0.4f, 0.5f, 200f, 7000f)),
        // CLAP: not typically used in metal
        DrumTrackData(engineType = 1, volume = 0.5f,
            drive = 0.5f, filterCutoff = 14000f, filterRes = 0f,
            voiceParams = voiceParams(
                170f, 15f, 120f, 0.7f, 6000f, 0.3f, 40f, 3.0f, 0.8f)),
        // RIM: PHYS KS, not prominent in metal
        DrumTrackData(engineType = 4, volume = 0.5f,
            drive = 0.3f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0f, 76f, 0.12f, 0.8f, 0.2f, 0.3f)),
        // SHAKER: not used in metal
        DrumTrackData(engineType = 3, volume = 0.3f,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(0f, 0f, 8000f, 0.2f, 40f, 0.2f)),
        // COWBELL: not used in metal
        DrumTrackData(engineType = 4, volume = 0.35f,
            drive = 0f, filterCutoff = 16000f, filterRes = 0f,
            voiceParams = voiceParams(1f, 76f, 0.2f, 0.7f, 0f, 0.3f))
    )

    // =====================================================================
    // DEFAULT kit (for empty pattern -- general purpose)
    // =====================================================================

    /** Default kit: balanced general-purpose sounds for building from scratch. */
    private fun defaultKit(): List<DrumTrackData> = listOf(
        DrumTrackData(engineType = 0, volume = 0.9f,
            voiceParams = voiceParams(50f, 1.0f, 2.0f, 4.0f, 40f, 500f, 2000f, 0f, 1.5f)),
        DrumTrackData(engineType = 1, volume = 0.85f,
            voiceParams = voiceParams(200f, 80f, 150f, 0.6f, 5000f, 0.3f, 60f, 3.0f, 0.5f)),
        DrumTrackData(engineType = 2, volume = 0.7f, chokeGroup = 0,
            voiceParams = voiceParams(0.3f, 0.6f, 40f, 8000f)),
        DrumTrackData(engineType = 2, volume = 0.7f, chokeGroup = 0,
            voiceParams = voiceParams(0.3f, 0.5f, 300f, 6000f)),
        DrumTrackData(engineType = 1, volume = 0.75f,
            voiceParams = voiceParams(150f, 20f, 120f, 0.8f, 3000f, 0.4f, 100f, 3.0f, 1.0f)),
        DrumTrackData(engineType = 4, volume = 0.7f,
            voiceParams = voiceParams(0f, 72f, 0.3f, 0.8f, 0.2f, 0.3f)),
        DrumTrackData(engineType = 3, volume = 0.6f,
            voiceParams = voiceParams(1f, 0f, 8000f, 0.2f, 80f, 0.3f)),
        DrumTrackData(engineType = 4, volume = 0.65f,
            voiceParams = voiceParams(1f, 80f, 0.4f, 0.6f, 0f, 0.3f))
    )

    // =====================================================================
    // Convenience: inactive step (used to fill non-triggered positions)
    // =====================================================================
    private val off = DrumStep()

    // =====================================================================
    // ROCK (2 presets)
    // =====================================================================

    /**
     * Rock Steady -- 120 BPM, swing 50% (straight).
     *
     * Classic rock pocket: kick on 1 & 3, snare on 2 & 4 with a subtle
     * ghost note on the "and" of 3. Closed hats on 8th notes with
     * accented downbeats and softer upbeats for breathing dynamics.
     */
    private fun createRockSteady(): DrumPatternData {
        val kit = rockKit().toMutableList()

        // Kick: beat 1 (step 0) vel 127, beat 3 (step 8) vel 127
        kit[0] = kit[0].copy(steps = listOf(
            DrumStep(trigger = true, velocity = 127), off, off, off,      // beat 1
            off, off, off, off,                                            // beat 2
            DrumStep(trigger = true, velocity = 127), off, off, off,      // beat 3
            off, off, off, off                                             // beat 4
        ))

        // Snare: beat 2 (step 4) vel 120, beat 4 (step 12) vel 120,
        //        ghost on &3 (step 10) vel 35
        kit[1] = kit[1].copy(steps = listOf(
            off, off, off, off,                                            // beat 1
            DrumStep(trigger = true, velocity = 120), off, off, off,      // beat 2
            off, off, DrumStep(trigger = true, velocity = 35), off,       // beat 3 + ghost &3
            DrumStep(trigger = true, velocity = 120), off, off, off       // beat 4
        ))

        // Closed hat: 8th notes (every even step), downbeats vel 100,
        // upbeats vel 70. No open hat.
        kit[2] = kit[2].copy(steps = List(16) { i ->
            if (i % 2 == 0) {
                DrumStep(trigger = true, velocity = if (i % 4 == 0) 100 else 70)
            } else {
                off
            }
        })

        return DrumPatternData(
            id = "factory-drum-rock-steady",
            name = "Rock Steady",
            bpm = 120f,
            swing = 50f,
            isFactory = true,
            tracks = kit
        )
    }

    /**
     * Driving Rock -- 135 BPM, swing 50% (straight).
     *
     * Energetic syncopated pattern: kick on 1, &2, 3, &4 with velocity
     * shaping. Snare on 2 & 4 full blast with a ghost on "e" of 2.
     * 16th-note hats alternating loud/soft. Crash on beat 1 via open
     * hat track for fill-launching feel.
     */
    private fun createDrivingRock(): DrumPatternData {
        val kit = rockKit().toMutableList()

        // Kick: 1 (vel 127), &2 (step 6, vel 90), 3 (vel 120), &4 (step 14, vel 85)
        kit[0] = kit[0].copy(steps = listOf(
            DrumStep(trigger = true, velocity = 127), off, off, off,      // beat 1
            off, off, DrumStep(trigger = true, velocity = 90), off,       // &2
            DrumStep(trigger = true, velocity = 120), off, off, off,      // beat 3
            off, off, DrumStep(trigger = true, velocity = 85), off        // &4
        ))

        // Snare: beat 2 (vel 127), e2 ghost (step 5, vel 30), beat 4 (vel 127)
        kit[1] = kit[1].copy(steps = listOf(
            off, off, off, off,                                            // beat 1
            DrumStep(trigger = true, velocity = 127),                      // beat 2
            DrumStep(trigger = true, velocity = 30),                       // e2 ghost
            off, off,
            off, off, off, off,                                            // beat 3
            DrumStep(trigger = true, velocity = 127), off, off, off       // beat 4
        ))

        // Closed hat: 16ths, accented on 8th-note positions (even steps)
        // vel 95 on 8ths, vel 60 on in-between 16ths
        kit[2] = kit[2].copy(steps = List(16) { i ->
            DrumStep(trigger = true, velocity = if (i % 2 == 0) 95 else 60)
        })

        // Open hat / Crash: beat 1 only (vel 100) for section-launching feel
        kit[3] = kit[3].copy(
            voiceParams = voiceParams(0.4f, 0.3f, 800f, 5000f), // crash: bright, long decay
            steps = listOf(
                DrumStep(trigger = true, velocity = 100), off, off, off,
                off, off, off, off,
                off, off, off, off,
                off, off, off, off
            )
        )

        return DrumPatternData(
            id = "factory-drum-driving-rock",
            name = "Driving Rock",
            bpm = 135f,
            swing = 50f,
            isFactory = true,
            tracks = kit
        )
    }

    // =====================================================================
    // FUNK (2 presets)
    // =====================================================================

    /**
     * Tight Funk -- 100 BPM, swing 56%.
     *
     * Syncopated kick that LEAVES beat 3 empty (the space is the groove).
     * Kick on 1, &2, &3. Snare on 2 & 4 with ghost notes on e1, a2, e3,
     * a4. 16th-note hats with the classic funk velocity curve:
     * [90,50,70,50, 95,50,70,50, 90,50,70,50, 95,50,70,50].
     * Open hat on &4 for the turnaround lift.
     */
    private fun createTightFunk(): DrumPatternData {
        val kit = funkKit().toMutableList()

        // Kick: 1 (vel 127), &2 (step 6, vel 100), &3 (step 10, vel 95)
        // Beat 3 (step 8) deliberately empty -- the pocket breathes here
        kit[0] = kit[0].copy(steps = listOf(
            DrumStep(trigger = true, velocity = 127), off, off, off,      // beat 1
            off, off, DrumStep(trigger = true, velocity = 100), off,      // &2
            off, off, DrumStep(trigger = true, velocity = 95), off,       // &3 (beat 3 empty!)
            off, off, off, off                                             // beat 4
        ))

        // Snare: 2 (vel 120), 4 (vel 120), ghosts on e1/a2/e3/a4
        kit[1] = kit[1].copy(steps = listOf(
            off,                                                           // 1
            DrumStep(trigger = true, velocity = 35),                       // e1 ghost
            off, off,
            DrumStep(trigger = true, velocity = 120), off, off,           // beat 2
            DrumStep(trigger = true, velocity = 40),                       // a2 ghost
            off,                                                           // beat 3
            DrumStep(trigger = true, velocity = 30),                       // e3 ghost
            off, off,
            DrumStep(trigger = true, velocity = 120), off, off,           // beat 4
            DrumStep(trigger = true, velocity = 38)                        // a4 ghost
        ))

        // Closed hat: 16ths with funk velocity curve
        val hatVelocities = listOf(90, 50, 70, 50, 95, 50, 70, 50, 90, 50, 70, 50, 95, 50, 70, 50)
        kit[2] = kit[2].copy(steps = List(16) { i ->
            DrumStep(trigger = true, velocity = hatVelocities[i])
        })

        // Open hat: &4 (step 14, vel 80)
        kit[3] = kit[3].copy(steps = listOf(
            off, off, off, off, off, off, off, off,
            off, off, off, off, off, off,
            DrumStep(trigger = true, velocity = 80), off
        ))

        return DrumPatternData(
            id = "factory-drum-tight-funk",
            name = "Tight Funk",
            bpm = 100f,
            swing = 56f,
            isFactory = true,
            tracks = kit
        )
    }

    /**
     * P-Funk Groove -- 108 BPM, swing 58%.
     *
     * Parliament/Funkadelic-inspired pattern. Kick on 1, a1, 3, a3
     * with velocity shaping. Snare on 2 & 4 with ghosts on a1 and e3.
     * 16th-note hats with heavy velocity variation.
     * Clap layered with snare on 2 & 4 for extra crack.
     */
    private fun createPFunkGroove(): DrumPatternData {
        val kit = funkKit().toMutableList()

        // Kick: 1 (vel 127), a1 (step 3, vel 75), 3 (vel 120), a3 (step 11, vel 70)
        kit[0] = kit[0].copy(steps = listOf(
            DrumStep(trigger = true, velocity = 127), off, off,           // beat 1
            DrumStep(trigger = true, velocity = 75),                       // a1
            off, off, off, off,                                            // beat 2
            DrumStep(trigger = true, velocity = 120), off, off,           // beat 3
            DrumStep(trigger = true, velocity = 70),                       // a3
            off, off, off, off                                             // beat 4
        ))

        // Snare: 2 (vel 127), 4 (vel 127), ghost on a1 (step 3, vel 35)
        // and e3 (step 9, vel 35)
        kit[1] = kit[1].copy(steps = listOf(
            off, off, off,
            DrumStep(trigger = true, velocity = 35),                       // a1 ghost
            DrumStep(trigger = true, velocity = 127), off, off, off,      // beat 2
            off,
            DrumStep(trigger = true, velocity = 35),                       // e3 ghost
            off, off,
            DrumStep(trigger = true, velocity = 127), off, off, off       // beat 4
        ))

        // Closed hat: 16ths with heavy velocity variation (P-Funk pump)
        val hatVel = listOf(95, 40, 75, 45, 100, 35, 70, 50, 95, 40, 75, 45, 100, 40, 65, 50)
        kit[2] = kit[2].copy(steps = List(16) { i ->
            DrumStep(trigger = true, velocity = hatVel[i])
        })

        // Clap: layered with snare on 2 and 4 (vel 80)
        kit[4] = kit[4].copy(steps = listOf(
            off, off, off, off,
            DrumStep(trigger = true, velocity = 80), off, off, off,       // beat 2
            off, off, off, off,
            DrumStep(trigger = true, velocity = 80), off, off, off        // beat 4
        ))

        return DrumPatternData(
            id = "factory-drum-pfunk-groove",
            name = "P-Funk Groove",
            bpm = 108f,
            swing = 58f,
            isFactory = true,
            tracks = kit
        )
    }

    // =====================================================================
    // HIP-HOP / LO-FI (2 presets)
    // =====================================================================

    /**
     * Boom Bap -- 90 BPM, swing 62%.
     *
     * Classic golden-era hip-hop. Kick on 1, &2, a3 -- the boom-bap
     * signature placement. HALF-TIME snare on beat 3 only (vel 127).
     * Soft 8th-note hats with probability variation for humanized feel.
     * Open hat on &4 for the turnaround.
     */
    private fun createBoomBap(): DrumPatternData {
        val kit = trapKit().toMutableList()

        // Kick: 1 (vel 127), &2 (step 6, vel 100), a3 (step 11, vel 110)
        kit[0] = kit[0].copy(steps = listOf(
            DrumStep(trigger = true, velocity = 127), off, off, off,      // beat 1
            off, off, DrumStep(trigger = true, velocity = 100), off,      // &2
            off, off, off,                                                 // beat 3
            DrumStep(trigger = true, velocity = 110),                      // a3
            off, off, off, off                                             // beat 4
        ))

        // Snare: beat 3 only (vel 127) -- half-time feel
        kit[1] = kit[1].copy(steps = listOf(
            off, off, off, off, off, off, off, off,
            DrumStep(trigger = true, velocity = 127), off, off, off,      // beat 3
            off, off, off, off
        ))

        // Closed hat: 8ths, soft (vel 60-75), probability 90% for humanized feel
        kit[2] = kit[2].copy(steps = List(16) { i ->
            if (i % 2 == 0) {
                DrumStep(
                    trigger = true,
                    velocity = if (i % 4 == 0) 75 else 60,
                    probability = 90
                )
            } else {
                off
            }
        })

        // Open hat: &4 (step 14, vel 70)
        kit[3] = kit[3].copy(steps = listOf(
            off, off, off, off, off, off, off, off,
            off, off, off, off, off, off,
            DrumStep(trigger = true, velocity = 70), off
        ))

        return DrumPatternData(
            id = "factory-drum-boom-bap",
            name = "Boom Bap",
            bpm = 90f,
            swing = 62f,
            isFactory = true,
            tracks = kit
        )
    }

    /**
     * Lo-Fi Chill -- 80 BPM, swing 65%.
     *
     * Sparse, lazy, bedroom-producer vibe. Kick on 1 and a2 only --
     * minimalist. Snare on beat 3 at relaxed velocity (not hammered).
     * Soft 8th-note hats with random probability (85%). Very quiet
     * 16th-note shaker underneath for subtle texture.
     */
    private fun createLoFiChill(): DrumPatternData {
        val kit = lofiKit().toMutableList()

        // Kick: 1 (vel 110), a2 (step 7, vel 85) -- sparse, lazy
        kit[0] = kit[0].copy(steps = listOf(
            DrumStep(trigger = true, velocity = 110), off, off, off,      // beat 1
            off, off, off,
            DrumStep(trigger = true, velocity = 85),                       // a2
            off, off, off, off,                                            // beat 3
            off, off, off, off                                             // beat 4
        ))

        // Snare: beat 3 (vel 100) -- relaxed, not aggressive
        kit[1] = kit[1].copy(steps = listOf(
            off, off, off, off, off, off, off, off,
            DrumStep(trigger = true, velocity = 100), off, off, off,      // beat 3
            off, off, off, off
        ))

        // Closed hat: 8ths, very soft (vel 45-60), random probability 85%
        kit[2] = kit[2].copy(steps = List(16) { i ->
            if (i % 2 == 0) {
                DrumStep(
                    trigger = true,
                    velocity = if (i % 4 == 0) 60 else 45,
                    probability = 85
                )
            } else {
                off
            }
        })

        // Shaker: 16ths, very quiet (vel 25-35) -- subtle texture layer
        kit[6] = kit[6].copy(
            volume = 0.4f,
            steps = List(16) { i ->
                DrumStep(
                    trigger = true,
                    velocity = if (i % 4 == 0) 35 else 25,
                    probability = 85
                )
            }
        )

        return DrumPatternData(
            id = "factory-drum-lofi-chill",
            name = "Lo-Fi Chill",
            bpm = 80f,
            swing = 65f,
            isFactory = true,
            tracks = kit
        )
    }

    // =====================================================================
    // BLUES (2 presets)
    // =====================================================================

    /**
     * Blues Shuffle -- 95 BPM, swing 66% (triplet).
     *
     * Classic 12/8 shuffle feel. Kick on 1 & 3 with solid foundation.
     * Snare on 2 & 4 with ghost notes on &1 and &3 for swing pocket.
     * Shuffle hat pattern: beats plus swung upbeats, accented on
     * downbeats (vel 90) with swung notes softer (vel 55).
     * Rimshot on &2 and &4 for subtle cross-stick accent.
     */
    private fun createBluesShuffle(): DrumPatternData {
        val kit = bluesKit().toMutableList()

        // Kick: 1 (vel 115), 3 (vel 110) -- simple, solid foundation
        kit[0] = kit[0].copy(steps = listOf(
            DrumStep(trigger = true, velocity = 115), off, off, off,      // beat 1
            off, off, off, off,                                            // beat 2
            DrumStep(trigger = true, velocity = 110), off, off, off,      // beat 3
            off, off, off, off                                             // beat 4
        ))

        // Snare: 2 (vel 100), 4 (vel 100), ghost on &1 (step 2, vel 30),
        // ghost on &3 (step 10, vel 30)
        kit[1] = kit[1].copy(steps = listOf(
            off, off, DrumStep(trigger = true, velocity = 30), off,       // &1 ghost
            DrumStep(trigger = true, velocity = 100), off, off, off,      // beat 2
            off, off, DrumStep(trigger = true, velocity = 30), off,       // &3 ghost
            DrumStep(trigger = true, velocity = 100), off, off, off       // beat 4
        ))

        // Closed hat: shuffle pattern -- beats and swung upbeats
        // Downbeats (steps 0,4,8,12) vel 90, swung (&) notes (steps 2,6,10,14) vel 55
        kit[2] = kit[2].copy(steps = List(16) { i ->
            when {
                i % 4 == 0 -> DrumStep(trigger = true, velocity = 90)     // downbeats
                i % 4 == 2 -> DrumStep(trigger = true, velocity = 55)     // swung upbeats
                else -> off
            }
        })

        // Rim: &2 (step 6, vel 50), &4 (step 14, vel 50) -- subtle cross-stick
        kit[5] = kit[5].copy(steps = listOf(
            off, off, off, off, off, off,
            DrumStep(trigger = true, velocity = 50), off,                  // &2
            off, off, off, off, off, off,
            DrumStep(trigger = true, velocity = 50), off                   // &4
        ))

        return DrumPatternData(
            id = "factory-drum-blues-shuffle",
            name = "Blues Shuffle",
            bpm = 95f,
            swing = 66f,
            isFactory = true,
            tracks = kit
        )
    }

    /**
     * Slow Blues -- 70 BPM, swing 66% (triplet).
     *
     * Laid-back slow blues with space. Kick on 1, a2, 3 with the a2
     * giving a lazy push. Snare on 2 & 4 at moderate velocity (not
     * aggressive). Soft triplet-feel hats. Open hat on &4 for the
     * turnaround swell.
     */
    private fun createSlowBlues(): DrumPatternData {
        val kit = bluesKit().toMutableList()

        // Kick: 1 (vel 120), a2 (step 7, vel 80), 3 (vel 110)
        kit[0] = kit[0].copy(steps = listOf(
            DrumStep(trigger = true, velocity = 120), off, off, off,      // beat 1
            off, off, off,
            DrumStep(trigger = true, velocity = 80),                       // a2
            DrumStep(trigger = true, velocity = 110), off, off, off,      // beat 3
            off, off, off, off                                             // beat 4
        ))

        // Snare: 2 (vel 95), 4 (vel 95) -- laid back, not aggressive
        kit[1] = kit[1].copy(steps = listOf(
            off, off, off, off,
            DrumStep(trigger = true, velocity = 95), off, off, off,       // beat 2
            off, off, off, off,
            DrumStep(trigger = true, velocity = 95), off, off, off        // beat 4
        ))

        // Closed hat: triplet feel, soft (vel 50-65)
        // Beats (vel 65) + swung upbeats (vel 50)
        kit[2] = kit[2].copy(steps = List(16) { i ->
            when {
                i % 4 == 0 -> DrumStep(trigger = true, velocity = 65)     // downbeats
                i % 4 == 2 -> DrumStep(trigger = true, velocity = 50)     // swung upbeats
                else -> off
            }
        })

        // Open hat: &4 (step 14, vel 60) -- turnaround swell
        kit[3] = kit[3].copy(steps = listOf(
            off, off, off, off, off, off, off, off,
            off, off, off, off, off, off,
            DrumStep(trigger = true, velocity = 60), off
        ))

        return DrumPatternData(
            id = "factory-drum-slow-blues",
            name = "Slow Blues",
            bpm = 70f,
            swing = 66f,
            isFactory = true,
            tracks = kit
        )
    }

    // =====================================================================
    // JAZZ (2 presets)
    // =====================================================================

    /**
     * Jazz Ride -- 140 BPM, swing 66% (triplet).
     *
     * Traditional jazz ride pattern. Sparse kick for comping (beat 1 and
     * &3). Snare is ALL ghost notes -- no backbeat. Ride cymbal (in the
     * closed hat slot) plays the classic swing pattern with accent on 2
     * and 4. Rim cross-stick on 2 & 4 for the traditional hi-hat foot.
     */
    private fun createJazzRide(): DrumPatternData {
        val kit = jazzKit().toMutableList()

        // Kick: 1 (vel 80), &3 (step 10, vel 55) -- sparse comping
        kit[0] = kit[0].copy(steps = listOf(
            DrumStep(trigger = true, velocity = 80), off, off, off,       // beat 1
            off, off, off, off,
            off, off, DrumStep(trigger = true, velocity = 55), off,       // &3
            off, off, off, off
        ))

        // Snare: ghost notes only -- no backbeat
        // e2 (step 5, vel 30), &3 (step 10, vel 25), e4 (step 13, vel 35)
        kit[1] = kit[1].copy(steps = listOf(
            off, off, off, off,
            off, DrumStep(trigger = true, velocity = 30), off, off,       // e2 ghost
            off, off, DrumStep(trigger = true, velocity = 25), off,       // &3 ghost
            off, DrumStep(trigger = true, velocity = 35), off, off        // e4 ghost
        ))

        // Ride (in closed hat slot): swing pattern
        // All beats (vel 85) + swung upbeats (vel 65)
        // Accent on 2 and 4 (vel 95)
        kit[2] = kit[2].copy(steps = List(16) { i ->
            when {
                i == 4 || i == 12 -> DrumStep(trigger = true, velocity = 95) // accent 2 & 4
                i % 4 == 0 -> DrumStep(trigger = true, velocity = 85)        // other downbeats
                i % 4 == 2 -> DrumStep(trigger = true, velocity = 65)        // swung upbeats
                else -> off
            }
        })

        // Open hat not used (avoid choke with ride)
        kit[3] = kit[3].copy(
            chokeGroup = -1,
            steps = List(16) { off }
        )

        // Rim: cross-stick on 2 and 4 (vel 40) -- hi-hat foot feel
        kit[5] = kit[5].copy(steps = listOf(
            off, off, off, off,
            DrumStep(trigger = true, velocity = 40), off, off, off,       // beat 2
            off, off, off, off,
            DrumStep(trigger = true, velocity = 40), off, off, off        // beat 4
        ))

        return DrumPatternData(
            id = "factory-drum-jazz-ride",
            name = "Jazz Ride",
            bpm = 140f,
            swing = 66f,
            isFactory = true,
            tracks = kit
        )
    }

    /**
     * Bossa Nova -- 120 BPM, swing 50% (straight).
     *
     * Brazilian bossa nova with straight 8th feel. Kick plays the
     * characteristic bossa bass rhythm: 1, &2, &3. Rim plays the bossa
     * clave pattern. Light 8th-note hats. Very soft 16th-note shaker
     * underneath for the samba-derived texture.
     */
    private fun createBossaNova(): DrumPatternData {
        val kit = jazzKit().toMutableList()

        // Kick: 1 (vel 100), &2 (step 6, vel 75), &3 (step 10, vel 70)
        // Classic bossa bass drum pattern
        kit[0] = kit[0].copy(steps = listOf(
            DrumStep(trigger = true, velocity = 100), off, off, off,      // beat 1
            off, off, DrumStep(trigger = true, velocity = 75), off,       // &2
            off, off, DrumStep(trigger = true, velocity = 70), off,       // &3
            off, off, off, off                                             // beat 4
        ))

        // Rim: bossa clave pattern
        // Steps: 1 (0), &1 (2), &2 (6), 3 (8), a3 (11), &4 (14)
        kit[5] = kit[5].copy(steps = listOf(
            DrumStep(trigger = true, velocity = 75), off,                  // 1
            DrumStep(trigger = true, velocity = 55), off,                  // &1
            off, off,
            DrumStep(trigger = true, velocity = 65), off,                  // &2
            DrumStep(trigger = true, velocity = 70), off, off,            // 3
            DrumStep(trigger = true, velocity = 50),                       // a3
            off, off,
            DrumStep(trigger = true, velocity = 60), off                   // &4
        ))

        // Closed hat: 8ths, light (vel 55)
        kit[2] = kit[2].copy(steps = List(16) { i ->
            if (i % 2 == 0) {
                DrumStep(trigger = true, velocity = 55)
            } else {
                off
            }
        })

        // Shaker: 16ths, very soft (vel 20-30) -- samba texture
        kit[6] = kit[6].copy(
            volume = 0.45f,
            steps = List(16) { i ->
                DrumStep(
                    trigger = true,
                    velocity = if (i % 4 == 0) 30 else if (i % 2 == 0) 25 else 20
                )
            }
        )

        return DrumPatternData(
            id = "factory-drum-bossa-nova",
            name = "Bossa Nova",
            bpm = 120f,
            swing = 50f,
            isFactory = true,
            tracks = kit
        )
    }

    // =====================================================================
    // ELECTRONIC (2 presets)
    // =====================================================================

    /**
     * Four on the Floor -- 128 BPM, swing 50% (straight).
     *
     * Classic house/dance pattern. Kick on every beat with slight
     * velocity variation for a pumping feel. Clap (not snare) on 2 & 4.
     * 16th-note hats with pumping velocity curve. Open hat on &2 and &4
     * for the classic house lift.
     */
    private fun createFourOnTheFloor(): DrumPatternData {
        val kit = electroKit().toMutableList()

        // Kick: every beat with velocity variation for pump
        kit[0] = kit[0].copy(steps = listOf(
            DrumStep(trigger = true, velocity = 127), off, off, off,      // beat 1
            DrumStep(trigger = true, velocity = 120), off, off, off,      // beat 2
            DrumStep(trigger = true, velocity = 125), off, off, off,      // beat 3
            DrumStep(trigger = true, velocity = 120), off, off, off       // beat 4
        ))

        // Clap: 2 and 4 (vel 110) -- house uses clap, not snare
        kit[4] = kit[4].copy(steps = listOf(
            off, off, off, off,
            DrumStep(trigger = true, velocity = 110), off, off, off,      // beat 2
            off, off, off, off,
            DrumStep(trigger = true, velocity = 110), off, off, off       // beat 4
        ))

        // Closed hat: 16ths with pumping velocity [85,40,65,40...]
        kit[2] = kit[2].copy(steps = List(16) { i ->
            DrumStep(trigger = true, velocity = when (i % 4) {
                0 -> 85
                1 -> 40
                2 -> 65
                else -> 40
            })
        })

        // Open hat: &2 (step 6, vel 90), &4 (step 14, vel 90)
        kit[3] = kit[3].copy(steps = listOf(
            off, off, off, off, off, off,
            DrumStep(trigger = true, velocity = 90), off,                  // &2
            off, off, off, off, off, off,
            DrumStep(trigger = true, velocity = 90), off                   // &4
        ))

        return DrumPatternData(
            id = "factory-drum-four-on-floor",
            name = "Four on the Floor",
            bpm = 128f,
            swing = 50f,
            isFactory = true,
            tracks = kit
        )
    }

    /**
     * Trap -- 145 BPM, swing 50% (straight).
     *
     * Modern trap pattern. Heavy sub kick on 1, &2, a3. Half-time snare
     * on beat 3 only. 16th-note hats with retrig rolls on steps 6-7
     * and 14-15 (the signature trap hat rolls). Open hat on &4.
     */
    private fun createTrap(): DrumPatternData {
        val kit = trapKit().toMutableList()

        // Kick: 808 sub-bass boom -- 1, &2, a3
        kit[0] = kit[0].copy(
            steps = listOf(
                DrumStep(trigger = true, velocity = 127), off, off, off,  // beat 1
                off, off, DrumStep(trigger = true, velocity = 110), off,  // &2
                off, off, off,
                DrumStep(trigger = true, velocity = 120),                  // a3
                off, off, off, off
            )
        )

        // Snare: beat 3 only (vel 127) -- half-time
        kit[1] = kit[1].copy(steps = listOf(
            off, off, off, off, off, off, off, off,
            DrumStep(trigger = true, velocity = 127), off, off, off,      // beat 3
            off, off, off, off
        ))

        // Closed hat: 16ths with retrig rolls on steps 6,7 and 14,15
        // (trapKit() already has crispy 25ms/10kHz hats)
        kit[2] = kit[2].copy(
            steps = List(16) { i ->
                DrumStep(
                    trigger = true,
                    velocity = when {
                        i % 4 == 0 -> 85
                        i % 2 == 0 -> 70
                        else -> 55
                    },
                    // Trap hat rolls: retrig on steps 6,7 (before beat 3)
                    // and steps 14,15 (before beat 1 of next bar)
                    retrigCount = if (i == 6 || i == 7 || i == 14 || i == 15) 3 else 0
                )
            }
        )

        // Open hat: &4 (step 14, vel 85)
        kit[3] = kit[3].copy(steps = listOf(
            off, off, off, off, off, off, off, off,
            off, off, off, off, off, off,
            DrumStep(trigger = true, velocity = 85), off
        ))

        return DrumPatternData(
            id = "factory-drum-trap",
            name = "Trap",
            bpm = 145f,
            swing = 50f,
            isFactory = true,
            tracks = kit
        )
    }

    // =====================================================================
    // METAL (2 presets)
    // =====================================================================

    /**
     * Thrash -- 180 BPM, swing 50% (straight).
     *
     * Relentless double-bass 16th-note kick pattern at constant high
     * velocity. Hard snare on 2 & 4. 8th-note hats cutting through.
     * Crash on beat 1 via open hat track.
     */
    private fun createThrash(): DrumPatternData {
        val kit = metalKit().toMutableList()

        // Kick: all 16ths, constant vel 120 -- double bass assault
        kit[0] = kit[0].copy(steps = List(16) {
            DrumStep(trigger = true, velocity = 120)
        })

        // Snare: 2 (vel 127), 4 (vel 127) -- hard, no ghost notes
        kit[1] = kit[1].copy(steps = listOf(
            off, off, off, off,
            DrumStep(trigger = true, velocity = 127), off, off, off,      // beat 2
            off, off, off, off,
            DrumStep(trigger = true, velocity = 127), off, off, off       // beat 4
        ))

        // Closed hat: 8ths (vel 95)
        kit[2] = kit[2].copy(steps = List(16) { i ->
            if (i % 2 == 0) DrumStep(trigger = true, velocity = 95)
            else off
        })

        // Crash on beat 1 (vel 110) -- every bar start
        kit[3] = kit[3].copy(steps = listOf(
            DrumStep(trigger = true, velocity = 110), off, off, off,
            off, off, off, off,
            off, off, off, off,
            off, off, off, off
        ))

        return DrumPatternData(
            id = "factory-drum-thrash",
            name = "Thrash",
            bpm = 180f,
            swing = 50f,
            isFactory = true,
            tracks = kit
        )
    }

    /**
     * Half-Time Metal -- 140 BPM, swing 50% (straight).
     *
     * Heavy half-time groove. Kick on 1, &1, 3, &3 with velocity
     * shaping. Snare on beat 3 only for the massive half-time hit.
     * 8th-note hats. Open hat on &4 for release.
     */
    private fun createHalfTimeMetal(): DrumPatternData {
        val kit = metalKit().toMutableList()

        // Kick: 1 (vel 127), &1 (step 2, vel 110), 3 (vel 125), &3 (step 10, vel 105)
        kit[0] = kit[0].copy(steps = listOf(
            DrumStep(trigger = true, velocity = 127), off,                // beat 1
            DrumStep(trigger = true, velocity = 110), off,                // &1
            off, off, off, off,                                            // beat 2
            DrumStep(trigger = true, velocity = 125), off,                // beat 3
            DrumStep(trigger = true, velocity = 105), off,                // &3
            off, off, off, off                                             // beat 4
        ))

        // Snare: beat 3 only (vel 127) -- heavy half-time
        kit[1] = kit[1].copy(steps = listOf(
            off, off, off, off, off, off, off, off,
            DrumStep(trigger = true, velocity = 127), off, off, off,      // beat 3
            off, off, off, off
        ))

        // Closed hat: 8ths (vel 85)
        kit[2] = kit[2].copy(steps = List(16) { i ->
            if (i % 2 == 0) DrumStep(trigger = true, velocity = 85)
            else off
        })

        // Open hat: &4 (step 14, vel 95) -- release point
        kit[3] = kit[3].copy(steps = listOf(
            off, off, off, off, off, off, off, off,
            off, off, off, off, off, off,
            DrumStep(trigger = true, velocity = 95), off
        ))

        return DrumPatternData(
            id = "factory-drum-half-time-metal",
            name = "Half-Time Metal",
            bpm = 140f,
            swing = 50f,
            isFactory = true,
            tracks = kit
        )
    }

    // =====================================================================
    // EMPTY
    // =====================================================================

    /** Empty pattern with default kit sounds for building from scratch. */
    private fun createEmptyPattern(): DrumPatternData {
        return DrumPatternData(
            id = "factory-drum-empty",
            name = "Empty",
            bpm = 120f,
            isFactory = true,
            tracks = defaultKit()
        )
    }
}
