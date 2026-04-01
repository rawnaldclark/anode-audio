package com.guitaremulator.app.audio

import org.junit.Assert.*
import org.junit.Test
import java.io.ByteArrayInputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Unit tests for [WavDecoder] WAV header parsing and sample decoding.
 *
 * Uses byte-array builders to construct minimal valid (and intentionally
 * malformed) WAV files in memory, then verifies that [WavDecoder.decode]
 * produces correct [WavData] or returns null for invalid inputs.
 */
class WavDecoderTest {

    // =========================================================================
    // WAV byte-array builder
    // =========================================================================

    /**
     * Build a minimal valid WAV file as a byte array.
     *
     * Constructs a 44-byte standard RIFF/WAV header followed by the supplied
     * audio data bytes. All multi-byte fields are little-endian per the RIFF
     * specification.
     *
     * @param sampleRate Audio sample rate in Hz.
     * @param channels Number of audio channels (1 = mono, 2 = stereo).
     * @param bitsPerSample Bits per sample (16, 24, or 32).
     * @param audioFormat WAV format code: 1 = PCM, 3 = IEEE float, 0xFFFE = Extensible.
     * @param dataBytes Raw audio sample data (already encoded in the target format).
     * @return Complete WAV file bytes ready to be wrapped in a ByteArrayInputStream.
     */
    private fun buildWavBytes(
        sampleRate: Int = 44100,
        channels: Short = 1,
        bitsPerSample: Short = 16,
        audioFormat: Short = 1,
        dataBytes: ByteArray = ByteArray(0)
    ): ByteArray {
        val byteRate = sampleRate * channels * (bitsPerSample / 8)
        val blockAlign = (channels * (bitsPerSample / 8)).toShort()
        val dataSize = dataBytes.size
        val fileSize = 36 + dataSize // total file size minus 8 bytes for "RIFF" + size field

        val buffer = ByteBuffer.allocate(44 + dataSize).order(ByteOrder.LITTLE_ENDIAN)

        // RIFF header
        buffer.put("RIFF".toByteArray(Charsets.US_ASCII)) // Bytes 0-3
        buffer.putInt(fileSize)                            // Bytes 4-7
        buffer.put("WAVE".toByteArray(Charsets.US_ASCII))  // Bytes 8-11

        // fmt sub-chunk
        buffer.put("fmt ".toByteArray(Charsets.US_ASCII))  // Bytes 12-15
        buffer.putInt(16)                                   // Bytes 16-19: fmt chunk size (PCM)
        buffer.putShort(audioFormat)                        // Bytes 20-21
        buffer.putShort(channels)                           // Bytes 22-23
        buffer.putInt(sampleRate)                           // Bytes 24-27
        buffer.putInt(byteRate)                             // Bytes 28-31
        buffer.putShort(blockAlign)                         // Bytes 32-33
        buffer.putShort(bitsPerSample)                      // Bytes 34-35

        // data sub-chunk
        buffer.put("data".toByteArray(Charsets.US_ASCII))  // Bytes 36-39
        buffer.putInt(dataSize)                             // Bytes 40-43

        // Audio data
        if (dataBytes.isNotEmpty()) {
            buffer.put(dataBytes)
        }

        return buffer.array()
    }

    /**
     * Build a WAV file with a corrupted signature at the specified offset.
     *
     * @param offset Byte offset of the 4-byte signature to corrupt.
     * @return WAV bytes with the signature replaced by "XXXX".
     */
    private fun buildWavWithBadSignature(offset: Int): ByteArray {
        val wav = buildWavBytes(
            dataBytes = shortSamplesToBytes(shortArrayOf(0, 100, -100))
        )
        val bad = "XXXX".toByteArray(Charsets.US_ASCII)
        System.arraycopy(bad, 0, wav, offset, 4)
        return wav
    }

    /**
     * Convert an array of 16-bit signed PCM samples to little-endian bytes.
     */
    private fun shortSamplesToBytes(samples: ShortArray): ByteArray {
        val buffer = ByteBuffer.allocate(samples.size * 2).order(ByteOrder.LITTLE_ENDIAN)
        for (s in samples) {
            buffer.putShort(s)
        }
        return buffer.array()
    }

    /**
     * Convert an array of 32-bit IEEE float samples to little-endian bytes.
     */
    private fun floatSamplesToBytes(samples: FloatArray): ByteArray {
        val buffer = ByteBuffer.allocate(samples.size * 4).order(ByteOrder.LITTLE_ENDIAN)
        for (s in samples) {
            buffer.putFloat(s)
        }
        return buffer.array()
    }

    /**
     * Convert an array of 24-bit signed PCM samples (as Ints) to little-endian bytes.
     * Each sample occupies 3 bytes.
     */
    private fun int24SamplesToBytes(samples: IntArray): ByteArray {
        val buffer = ByteBuffer.allocate(samples.size * 3).order(ByteOrder.LITTLE_ENDIAN)
        for (s in samples) {
            buffer.put((s and 0xFF).toByte())
            buffer.put(((s shr 8) and 0xFF).toByte())
            buffer.put(((s shr 16) and 0xFF).toByte())
        }
        return buffer.array()
    }

    // =========================================================================
    // Valid WAV decoding
    // =========================================================================

    @Test
    fun `decode valid 16-bit mono WAV`() {
        val rawSamples = shortArrayOf(0, 16384, -16384, 32767, -32768)
        val dataBytes = shortSamplesToBytes(rawSamples)
        val wavBytes = buildWavBytes(
            sampleRate = 44100,
            channels = 1,
            bitsPerSample = 16,
            audioFormat = 1,
            dataBytes = dataBytes
        )

        val result = WavDecoder.decode(ByteArrayInputStream(wavBytes))

        assertNotNull(result)
        assertEquals(44100, result!!.sampleRate)
        assertEquals(1, result.numChannels)
        assertEquals(rawSamples.size, result.samples.size)

        // Verify normalized sample values: sample / 32768.0
        assertEquals(0f, result.samples[0], 0.001f)
        assertEquals(16384f / 32768f, result.samples[1], 0.001f)
        assertEquals(-16384f / 32768f, result.samples[2], 0.001f)
        assertEquals(32767f / 32768f, result.samples[3], 0.001f)
        assertEquals(-1.0f, result.samples[4], 0.001f)
    }

    @Test
    fun `decode valid 16-bit stereo WAV`() {
        // Stereo: interleaved L, R, L, R ...
        // WavDecoder extracts only the left channel.
        val stereoSamples = shortArrayOf(
            1000, 2000,   // Frame 0: L=1000, R=2000
            3000, 4000,   // Frame 1: L=3000, R=4000
            5000, 6000    // Frame 2: L=5000, R=6000
        )
        val dataBytes = shortSamplesToBytes(stereoSamples)
        val wavBytes = buildWavBytes(
            sampleRate = 48000,
            channels = 2,
            bitsPerSample = 16,
            audioFormat = 1,
            dataBytes = dataBytes
        )

        val result = WavDecoder.decode(ByteArrayInputStream(wavBytes))

        assertNotNull(result)
        assertEquals(48000, result!!.sampleRate)
        assertEquals(2, result.numChannels)
        assertEquals(3, result.samples.size) // 3 frames

        // Only left channel values
        assertEquals(1000f / 32768f, result.samples[0], 0.001f)
        assertEquals(3000f / 32768f, result.samples[1], 0.001f)
        assertEquals(5000f / 32768f, result.samples[2], 0.001f)
    }

    @Test
    fun `decode valid 32-bit float WAV`() {
        val floatSamples = floatArrayOf(0.0f, 0.5f, -0.5f, 1.0f, -1.0f)
        val dataBytes = floatSamplesToBytes(floatSamples)
        val wavBytes = buildWavBytes(
            sampleRate = 96000,
            channels = 1,
            bitsPerSample = 32,
            audioFormat = 3, // IEEE float
            dataBytes = dataBytes
        )

        val result = WavDecoder.decode(ByteArrayInputStream(wavBytes))

        assertNotNull(result)
        assertEquals(96000, result!!.sampleRate)
        assertEquals(1, result.numChannels)
        assertEquals(floatSamples.size, result.samples.size)

        for (i in floatSamples.indices) {
            assertEquals(floatSamples[i], result.samples[i], 0.0001f)
        }
    }

    @Test
    fun `decode valid 24-bit WAV`() {
        // 24-bit signed PCM: range [-8388608, 8388607], normalized by 8388608.
        val samples24 = intArrayOf(0, 4194304, -4194304) // 0, 0.5 * 2^23, -0.5 * 2^23
        val dataBytes = int24SamplesToBytes(samples24)
        val wavBytes = buildWavBytes(
            sampleRate = 44100,
            channels = 1,
            bitsPerSample = 24,
            audioFormat = 1, // PCM
            dataBytes = dataBytes
        )

        val result = WavDecoder.decode(ByteArrayInputStream(wavBytes))

        assertNotNull(result)
        assertEquals(44100, result!!.sampleRate)
        assertEquals(1, result.numChannels)
        assertEquals(samples24.size, result.samples.size)

        assertEquals(0f, result.samples[0], 0.001f)
        assertEquals(0.5f, result.samples[1], 0.001f)
        assertEquals(-0.5f, result.samples[2], 0.001f)
    }

    @Test
    fun `decode reports correct sample rate`() {
        val dataBytes = shortSamplesToBytes(shortArrayOf(100))

        for (sr in listOf(44100, 48000, 96000)) {
            val wavBytes = buildWavBytes(
                sampleRate = sr,
                channels = 1,
                bitsPerSample = 16,
                audioFormat = 1,
                dataBytes = dataBytes
            )
            val result = WavDecoder.decode(ByteArrayInputStream(wavBytes))
            assertNotNull("Should decode at sample rate $sr", result)
            assertEquals("Sample rate should be $sr", sr, result!!.sampleRate)
        }
    }

    // =========================================================================
    // Invalid / malformed WAV inputs
    // =========================================================================

    @Test
    fun `decode returns null for empty stream`() {
        val result = WavDecoder.decode(ByteArrayInputStream(ByteArray(0)))
        assertNull(result)
    }

    @Test
    fun `decode returns null for truncated header`() {
        // Only 20 bytes -- not enough for a complete WAV header
        val truncated = ByteArray(20)
        // Write valid RIFF magic so it gets past the first check
        System.arraycopy("RIFF".toByteArray(Charsets.US_ASCII), 0, truncated, 0, 4)
        val result = WavDecoder.decode(ByteArrayInputStream(truncated))
        assertNull(result)
    }

    @Test
    fun `decode returns null for invalid RIFF signature`() {
        val wavBytes = buildWavWithBadSignature(offset = 0)
        val result = WavDecoder.decode(ByteArrayInputStream(wavBytes))
        assertNull(result)
    }

    @Test
    fun `decode returns null for invalid WAVE signature`() {
        val wavBytes = buildWavWithBadSignature(offset = 8)
        val result = WavDecoder.decode(ByteArrayInputStream(wavBytes))
        assertNull(result)
    }

    @Test
    fun `decode handles zero-length data chunk`() {
        // Valid header structure but 0 bytes of audio data.
        val wavBytes = buildWavBytes(
            sampleRate = 44100,
            channels = 1,
            bitsPerSample = 16,
            audioFormat = 1,
            dataBytes = ByteArray(0) // No audio data
        )

        val result = WavDecoder.decode(ByteArrayInputStream(wavBytes))

        // The decoder returns null for zero frames because framesToDecode <= 0.
        assertNull(result)
    }

    // =========================================================================
    // Additional edge cases (Sprint 18)
    // =========================================================================

    @Test
    fun `decode single sample mono WAV`() {
        // Smallest valid WAV: 1 sample
        val dataBytes = shortSamplesToBytes(shortArrayOf(16384))
        val wavBytes = buildWavBytes(
            sampleRate = 44100,
            channels = 1,
            bitsPerSample = 16,
            audioFormat = 1,
            dataBytes = dataBytes
        )

        val result = WavDecoder.decode(ByteArrayInputStream(wavBytes))

        assertNotNull(result)
        assertEquals(1, result!!.samples.size)
        assertEquals(16384f / 32768f, result.samples[0], 0.001f)
    }

    @Test
    fun `decode WAV with data chunk smaller than header claims`() {
        // Build a valid WAV header claiming 100 bytes of data,
        // but only provide 4 bytes (2 samples of 16-bit mono).
        val smallData = shortSamplesToBytes(shortArrayOf(1000, -1000))
        val wavBytes = buildWavBytes(
            sampleRate = 44100,
            channels = 1,
            bitsPerSample = 16,
            audioFormat = 1,
            dataBytes = smallData
        )
        // Overwrite the data chunk size to claim more data than exists.
        // Data size field is at bytes 40-43 in a standard 44-byte header.
        val buffer = ByteBuffer.wrap(wavBytes).order(ByteOrder.LITTLE_ENDIAN)
        buffer.putInt(40, 100) // Claim 100 bytes but only 4 are present

        val result = WavDecoder.decode(ByteArrayInputStream(wavBytes))

        // Decoder should either return the partial data it could read
        // or return null. Either way, it should not crash.
        if (result != null) {
            assertTrue("Samples should not exceed what was actually provided",
                result.samples.size <= 2)
        }
    }

    @Test
    fun `decode stereo WAV extracts mono correctly`() {
        // 4 stereo frames: L=1000,R=2000 | L=3000,R=4000 | L=-5000,R=-6000 | L=0,R=0
        val stereoSamples = shortArrayOf(
            1000, 2000, 3000, 4000, -5000, -6000, 0, 0
        )
        val dataBytes = shortSamplesToBytes(stereoSamples)
        val wavBytes = buildWavBytes(
            sampleRate = 48000,
            channels = 2,
            bitsPerSample = 16,
            audioFormat = 1,
            dataBytes = dataBytes
        )

        val result = WavDecoder.decode(ByteArrayInputStream(wavBytes))

        assertNotNull(result)
        assertEquals(4, result!!.samples.size)
        // Verify left channel extraction
        assertEquals(1000f / 32768f, result.samples[0], 0.001f)
        assertEquals(3000f / 32768f, result.samples[1], 0.001f)
        assertEquals(-5000f / 32768f, result.samples[2], 0.001f)
        assertEquals(0f, result.samples[3], 0.001f)
    }

    @Test
    fun `decode 32-bit float stereo WAV`() {
        // Stereo float: L=0.5, R=-0.5, L=1.0, R=-1.0
        val stereoFloats = floatArrayOf(0.5f, -0.5f, 1.0f, -1.0f)
        val dataBytes = floatSamplesToBytes(stereoFloats)
        val wavBytes = buildWavBytes(
            sampleRate = 96000,
            channels = 2,
            bitsPerSample = 32,
            audioFormat = 3,
            dataBytes = dataBytes
        )

        val result = WavDecoder.decode(ByteArrayInputStream(wavBytes))

        assertNotNull(result)
        assertEquals(2, result!!.samples.size) // 2 frames from 4 interleaved samples
        assertEquals(0.5f, result.samples[0], 0.001f) // Left channel
        assertEquals(1.0f, result.samples[1], 0.001f) // Left channel
    }

    @Test
    fun `decode returns null for non-WAV random bytes`() {
        val garbage = ByteArray(256) { (it * 7 % 256).toByte() }
        val result = WavDecoder.decode(ByteArrayInputStream(garbage))
        assertNull(result)
    }
}
