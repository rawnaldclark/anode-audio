package com.guitaremulator.app.audio

import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Decoded WAV file data containing normalized audio samples.
 *
 * @property samples Audio samples normalized to the [-1.0, 1.0] range.
 *                   For stereo files, only the left channel is included.
 *                   Truncated to [WavDecoder.MAX_SAMPLES] if the source is longer.
 * @property sampleRate The sample rate in Hz (e.g., 44100, 48000).
 * @property numChannels The number of channels in the original file (1 or 2).
 */
data class WavData(
    val samples: FloatArray,
    val sampleRate: Int,
    val numChannels: Int
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is WavData) return false
        return samples.contentEquals(other.samples) &&
            sampleRate == other.sampleRate &&
            numChannels == other.numChannels
    }

    override fun hashCode(): Int {
        var result = samples.contentHashCode()
        result = 31 * result + sampleRate
        result = 31 * result + numChannels
        return result
    }
}

/**
 * Decoder for standard RIFF/WAV audio files.
 *
 * This utility parses WAV files commonly used for impulse response (IR) cabinet
 * simulation data in the guitar emulator's signal chain. WAV files are the
 * de facto standard format for distributing IR files.
 *
 * Supported formats:
 *   - 16-bit PCM integer (audioFormat = 1, bitsPerSample = 16)
 *   - 24-bit PCM integer (audioFormat = 1, bitsPerSample = 24)
 *   - 32-bit IEEE 754 float (audioFormat = 3, bitsPerSample = 32)
 *   - WAVE_FORMAT_EXTENSIBLE (audioFormat = 0xFFFE) wrapping any of the above
 *
 * Channel handling:
 *   - Mono files are decoded directly.
 *   - Stereo files have only the left channel extracted. This is standard
 *     practice for IR loading, as stereo IRs are typically dual-mono or have
 *     the primary response in the left channel.
 *
 * Design decisions:
 *   - The entire input stream is read into memory first. IR files are small
 *     (max ~32 KB for 8192 32-bit float mono samples), so this avoids the
 *     complexity of streaming parsers.
 *   - The input stream is NOT closed by this decoder. The caller owns the
 *     stream lifecycle (e.g., AssetManager or ContentResolver).
 *   - Any malformed or unsupported file returns null rather than throwing an
 *     exception, making it safe to use in UI code without try/catch blocks.
 *   - Samples are capped at [MAX_SAMPLES] (8192) to match the native engine's
 *     IR buffer size and prevent excessive memory allocation.
 *
 * WAV format reference:
 *   - RIFF header: "RIFF" + fileSize(4) + "WAVE"
 *   - fmt chunk: "fmt " + chunkSize(4) + audioFormat(2) + numChannels(2)
 *                + sampleRate(4) + byteRate(4) + blockAlign(2) + bitsPerSample(2)
 *   - data chunk: "data" + chunkSize(4) + raw sample bytes
 *   - All multi-byte values are little-endian per the RIFF specification.
 *   - Chunks with odd sizes have a single padding byte appended (RIFF rule).
 *
 * Usage:
 * ```kotlin
 * val wavData = assets.open("ir/cabinet.wav").use { stream ->
 *     WavDecoder.decode(stream)
 * }
 * if (wavData != null) {
 *     nativeEngine.loadImpulseResponse(wavData.samples, wavData.sampleRate)
 * }
 * ```
 */
class WavDecoder private constructor() {

    companion object {

        // =====================================================================
        // Constants
        // =====================================================================

        /** Maximum number of decoded samples. Files longer than this are truncated. */
        const val MAX_SAMPLES = 8192

        /** WAV audio format code for PCM integer data (16-bit, 24-bit). */
        private const val AUDIO_FORMAT_PCM = 1.toShort()

        /** WAV audio format code for IEEE 754 floating-point data (32-bit). */
        private const val AUDIO_FORMAT_IEEE_FLOAT = 3.toShort()

        /**
         * WAV audio format code for WAVE_FORMAT_EXTENSIBLE.
         *
         * WAVE_FORMAT_EXTENSIBLE (0xFFFE) is a container format that wraps
         * the actual audio format in an extended header. It is commonly used
         * by professional audio tools and many commercial IR files because it
         * supports:
         *   - Channel masks for surround sound (not relevant for IRs)
         *   - Sample formats beyond 16-bit in a standardized way
         *   - Sub-format GUIDs that identify the actual encoding
         *
         * When we encounter this format code, we read the cbSize field to
         * find the extended header, then extract the 2-byte sub-format code
         * from the first two bytes of the SubFormat GUID. For PCM and IEEE
         * float data, the sub-format code matches the standard format codes
         * (1 for PCM, 3 for float).
         *
         * Reference: Microsoft WAVEFORMATEXTENSIBLE documentation
         *   https://docs.microsoft.com/en-us/windows/win32/api/mmreg/ns-mmreg-waveformatextensible
         */
        private const val AUDIO_FORMAT_EXTENSIBLE = 0xFFFE.toShort()

        /** RIFF container magic bytes: "RIFF" in ASCII. */
        private const val RIFF_MAGIC = 0x46464952 // "RIFF" as little-endian int

        /** WAVE format identifier: "WAVE" in ASCII. */
        private const val WAVE_MAGIC = 0x45564157 // "WAVE" as little-endian int

        /** Format chunk identifier: "fmt " in ASCII. */
        private const val FMT_CHUNK_ID = 0x20746D66 // "fmt " as little-endian int

        /** Data chunk identifier: "data" in ASCII. */
        private const val DATA_CHUNK_ID = 0x61746164 // "data" as little-endian int

        /** Normalization divisor for 16-bit signed PCM: 2^15. */
        private const val NORMALIZE_16BIT = 32768f

        /** Normalization divisor for 24-bit signed PCM: 2^23. */
        private const val NORMALIZE_24BIT = 8388608f

        /** Minimum valid fmt chunk size in bytes (PCM format, no extensions). */
        private const val MIN_FMT_CHUNK_SIZE = 16

        // =====================================================================
        // Public API
        // =====================================================================

        /**
         * Decode a WAV file from the given input stream.
         *
         * Reads the entire stream into memory, parses the RIFF/WAV structure,
         * validates the format, and returns normalized audio samples.
         *
         * @param inputStream The input stream containing WAV file data.
         *                    The stream is read from its current position but
         *                    is NOT closed by this method.
         * @return A [WavData] instance with normalized samples, or null if the
         *         file is malformed, uses an unsupported format, or any I/O
         *         error occurs during reading.
         */
        /**
         * Maximum raw file size to read into memory (512 KB).
         *
         * The largest valid mono IR at MAX_SAMPLES (8192) in 32-bit float
         * is ~32 KB. 512 KB provides generous headroom for stereo files,
         * metadata chunks, and format overhead while preventing OOM from
         * accidentally selecting a multi-GB recording.
         */
        private const val MAX_FILE_SIZE = 512 * 1024

        fun decode(inputStream: InputStream): WavData? {
            return try {
                decodeInternal(inputStream)
            } catch (_: Throwable) {
                // Catch all throwables including OutOfMemoryError (which is
                // a Throwable, not an Exception). This prevents OOM from
                // crashing the app if a very large file bypasses the size check.
                null
            }
        }

        // =====================================================================
        // Internal implementation
        // =====================================================================

        /**
         * Internal decode implementation that may throw exceptions.
         * All exceptions are caught by the public [decode] method.
         */
        private fun decodeInternal(inputStream: InputStream): WavData? {
            // Read with size cap to prevent OOM from accidentally selecting
            // a multi-GB audio recording. IR files are tiny (<32KB typically).
            val rawBytes = readBytesLimited(inputStream, MAX_FILE_SIZE)
                ?: return null // File exceeds size limit
            if (rawBytes.size < 12) {
                // Too small to contain even the RIFF header
                return null
            }

            val buffer = ByteBuffer.wrap(rawBytes).order(ByteOrder.LITTLE_ENDIAN)

            // -----------------------------------------------------------------
            // 1. Parse RIFF header
            // -----------------------------------------------------------------
            val riffMagic = buffer.int
            if (riffMagic != RIFF_MAGIC) return null

            // File size field (total file size minus 8 bytes for RIFF header).
            // We read it for validation but do not enforce it strictly, since
            // some WAV writers produce incorrect values.
            @Suppress("UNUSED_VARIABLE")
            val fileSize = buffer.int

            val waveMagic = buffer.int
            if (waveMagic != WAVE_MAGIC) return null

            // -----------------------------------------------------------------
            // 2. Iterate chunks to find "fmt " and "data"
            // -----------------------------------------------------------------
            var audioFormat: Short = 0
            var numChannels: Short = 0
            var sampleRate = 0
            var bitsPerSample: Short = 0
            var fmtFound = false
            var dataChunkOffset = -1
            var dataChunkSize = 0

            while (buffer.remaining() >= 8) {
                val chunkId = buffer.int
                val chunkSize = buffer.int

                // Validate chunk size: must be non-negative and not exceed
                // remaining buffer (accounting for potential padding byte).
                if (chunkSize < 0 || chunkSize > buffer.remaining()) {
                    return null
                }

                val chunkStartPos = buffer.position()

                when (chunkId) {
                    FMT_CHUNK_ID -> {
                        if (chunkSize < MIN_FMT_CHUNK_SIZE) return null

                        audioFormat = buffer.short
                        numChannels = buffer.short
                        sampleRate = buffer.int

                        // byteRate and blockAlign are read but not used directly;
                        // we compute frame sizes from channels and bitsPerSample.
                        @Suppress("UNUSED_VARIABLE")
                        val byteRate = buffer.int
                        @Suppress("UNUSED_VARIABLE")
                        val blockAlign = buffer.short

                        bitsPerSample = buffer.short

                        // Handle WAVE_FORMAT_EXTENSIBLE: extract the actual
                        // sub-format code from the extended header.
                        if (audioFormat == AUDIO_FORMAT_EXTENSIBLE) {
                            // The extended header follows the standard 16-byte
                            // fmt fields. It contains:
                            //   cbSize (2 bytes): size of extension (at least 22)
                            //   wValidBitsPerSample (2 bytes): valid bits
                            //   dwChannelMask (4 bytes): speaker position mask
                            //   SubFormat (16 bytes): GUID identifying encoding
                            //
                            // The first 2 bytes of SubFormat are the actual
                            // format code (1=PCM, 3=float), followed by the
                            // standard KSDATAFORMAT_SUBTYPE GUID suffix.
                            val bytesRead = 16 // we already read 16 bytes
                            val remaining = chunkSize - bytesRead
                            if (remaining < 24) return null // need cbSize(2) + validBits(2) + channelMask(4) + SubFormat(16)

                            val cbSize = buffer.short.toInt() and 0xFFFF
                            if (cbSize < 22) return null // extension too small

                            @Suppress("UNUSED_VARIABLE")
                            val validBitsPerSample = buffer.short
                            @Suppress("UNUSED_VARIABLE")
                            val channelMask = buffer.int

                            // SubFormat GUID: first 2 bytes are the format code
                            audioFormat = buffer.short
                            // Skip remaining 14 bytes of the GUID
                            skipBytes(buffer, 14)
                        }

                        fmtFound = true
                    }
                    DATA_CHUNK_ID -> {
                        dataChunkOffset = chunkStartPos
                        dataChunkSize = chunkSize
                    }
                }

                // Advance past this chunk's data. The position may already
                // have been advanced by reading fields (e.g., in the fmt chunk),
                // so we set it absolutely rather than relatively.
                buffer.position(chunkStartPos + chunkSize)

                // RIFF chunks with an odd size have one padding byte appended
                // to maintain 16-bit alignment. Skip it if present.
                if (chunkSize % 2 != 0 && buffer.remaining() > 0) {
                    buffer.position(buffer.position() + 1)
                }

                // If we have found both required chunks, stop scanning.
                // This avoids unnecessary work on files with many metadata chunks.
                if (fmtFound && dataChunkOffset >= 0) break
            }

            // -----------------------------------------------------------------
            // 3. Validate that we found both required chunks
            // -----------------------------------------------------------------
            if (!fmtFound || dataChunkOffset < 0) return null

            // -----------------------------------------------------------------
            // 4. Validate format parameters
            // -----------------------------------------------------------------
            val channels = numChannels.toInt()
            if (channels != 1 && channels != 2) return null
            if (sampleRate <= 0) return null

            val bits = bitsPerSample.toInt()
            val isValidFormat = when (audioFormat) {
                AUDIO_FORMAT_PCM -> bits == 16 || bits == 24
                AUDIO_FORMAT_IEEE_FLOAT -> bits == 32
                else -> false
            }
            if (!isValidFormat) return null

            // -----------------------------------------------------------------
            // 5. Decode samples from the data chunk
            // -----------------------------------------------------------------
            buffer.position(dataChunkOffset)

            val bytesPerSample = bits / 8
            val bytesPerFrame = bytesPerSample * channels
            if (bytesPerFrame <= 0) return null

            val totalFrames = dataChunkSize / bytesPerFrame
            val framesToDecode = totalFrames.coerceAtMost(MAX_SAMPLES)

            if (framesToDecode <= 0) return null

            val samples = FloatArray(framesToDecode)

            for (i in 0 until framesToDecode) {
                // Decode the left channel sample (first sample in each frame)
                val sample = decodeSample(buffer, audioFormat, bits)
                samples[i] = sample

                // If stereo, skip the right channel sample(s)
                if (channels == 2) {
                    skipBytes(buffer, bytesPerSample)
                }
            }

            return WavData(
                samples = samples,
                sampleRate = sampleRate,
                numChannels = channels
            )
        }

        /**
         * Decode a single audio sample from the buffer at its current position.
         *
         * The sample is normalized to the [-1.0, 1.0] range based on the format:
         *   - 16-bit PCM: signed short divided by 32768
         *   - 24-bit PCM: signed 24-bit integer divided by 8388608
         *   - 32-bit float: read directly (already in normalized range by convention)
         *
         * @param buffer The byte buffer positioned at the start of the sample.
         * @param audioFormat WAV audio format code (1 = PCM, 3 = IEEE float).
         * @param bitsPerSample Number of bits per sample (16, 24, or 32).
         * @return The normalized sample value.
         */
        private fun decodeSample(
            buffer: ByteBuffer,
            audioFormat: Short,
            bitsPerSample: Int
        ): Float {
            return when {
                audioFormat == AUDIO_FORMAT_PCM && bitsPerSample == 16 -> {
                    // 16-bit signed PCM: range [-32768, 32767]
                    buffer.short.toFloat() / NORMALIZE_16BIT
                }
                audioFormat == AUDIO_FORMAT_PCM && bitsPerSample == 24 -> {
                    // 24-bit signed PCM: three bytes, little-endian.
                    // Bytes arrive as [low, mid, high]. We must reconstruct a
                    // signed 32-bit integer and then normalize.
                    //
                    // The AND with 0xFF converts the signed byte to an unsigned
                    // int (0-255). The high byte is sign-extended by using
                    // toInt() directly (which preserves the sign bit via
                    // Java/Kotlin's byte-to-int sign extension), then shifted
                    // left 16 bits to place it in the correct position.
                    val low = buffer.get().toInt() and 0xFF
                    val mid = buffer.get().toInt() and 0xFF
                    val high = buffer.get().toInt() // Sign-extended to 32 bits
                    val sample24 = low or (mid shl 8) or (high shl 16)
                    sample24.toFloat() / NORMALIZE_24BIT
                }
                audioFormat == AUDIO_FORMAT_IEEE_FLOAT && bitsPerSample == 32 -> {
                    // 32-bit IEEE 754 float: already in [-1.0, 1.0] by convention.
                    // ByteBuffer.getFloat() reads 4 bytes and interprets them as
                    // a float using the buffer's byte order (little-endian).
                    buffer.float
                }
                else -> {
                    // This branch should never be reached because format
                    // validation occurs before sample decoding. Return silence
                    // as a defensive measure.
                    0f
                }
            }
        }

        /**
         * Advance the buffer position by the specified number of bytes.
         *
         * Used to skip unwanted channels (e.g., the right channel in stereo files).
         *
         * @param buffer The byte buffer to advance.
         * @param count Number of bytes to skip.
         */
        private fun skipBytes(buffer: ByteBuffer, count: Int) {
            buffer.position(buffer.position() + count)
        }

        /**
         * Read up to [maxSize] bytes from the input stream.
         *
         * Returns null if the stream contains more than [maxSize] bytes,
         * signaling that the file is too large to be a valid IR.
         *
         * @param inputStream The stream to read from.
         * @param maxSize Maximum number of bytes to accept.
         * @return The file contents, or null if too large.
         */
        private fun readBytesLimited(inputStream: InputStream, maxSize: Int): ByteArray? {
            val buffer = ByteArray(8192)
            var totalRead = 0
            val output = java.io.ByteArrayOutputStream(minOf(maxSize, 32768))

            while (true) {
                val bytesRead = inputStream.read(buffer)
                if (bytesRead == -1) break

                totalRead += bytesRead
                if (totalRead > maxSize) {
                    return null // File too large
                }

                output.write(buffer, 0, bytesRead)
            }

            return output.toByteArray()
        }
    }
}
