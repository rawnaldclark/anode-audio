#ifndef GUITAR_ENGINE_WAV_WRITER_H
#define GUITAR_ENGINE_WAV_WRITER_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

/**
 * Simple WAV file writer for 32-bit float mono/stereo audio.
 *
 * Writes standard RIFF/WAVE files with IEEE float format (format tag 3),
 * suitable for lossless capture of the looper's audio output. The writer
 * is designed for streaming use: the header is written on open() with a
 * placeholder data size, and close() seeks back to patch the correct sizes.
 *
 * WAV header layout (44 bytes):
 *   Offset  Size  Field
 *   ------  ----  -----
 *   0       4     "RIFF"
 *   4       4     File size - 8 (patched on close)
 *   8       4     "WAVE"
 *   12      4     "fmt "
 *   16      4     Fmt chunk size (16 for PCM/float without extensible)
 *   20      2     Format tag (3 = IEEE float)
 *   22      2     Number of channels
 *   24      4     Sample rate
 *   28      4     Byte rate (sampleRate * numChannels * bytesPerSample)
 *   32      2     Block align (numChannels * bytesPerSample)
 *   34      2     Bits per sample (32)
 *   36      4     "data"
 *   40      4     Data chunk size in bytes (patched on close)
 *   44      ...   Audio sample data (32-bit float, interleaved if stereo)
 *
 * Crash resilience:
 *   If close() is never called (e.g., app crash or force-kill), the WAV
 *   file will have an incorrect size in the RIFF and data headers. Most
 *   WAV players and editors (Audacity, FFmpeg, SoX) will still read the
 *   file correctly by inferring the actual size from the file length.
 *   The caller can also repair the header on next launch by calling the
 *   static repairHeader() method with the file path.
 *
 * Thread safety:
 *   NOT thread-safe. The caller must ensure that only one thread accesses
 *   a WavWriter instance at a time. In the looper architecture, the writer
 *   thread owns the WavWriter exclusively while the audio thread feeds data
 *   through the SPSC ring buffer.
 *
 * Performance:
 *   writeSamples() calls fwrite() which is buffered by the C runtime
 *   (typically 8KB or 64KB internal buffer). This is sufficient for
 *   audio streaming at typical block sizes (128-1024 samples). For
 *   very high throughput, the caller should use larger write batches
 *   drained from the SPSC ring buffer.
 */
class WavWriter {
public:
    WavWriter() = default;

    /**
     * Destructor. Closes the file if still open, patching the header
     * with the correct data size. If the destructor is reached due to
     * an exception or cleanup path, the file will be as correct as
     * possible given the data written so far.
     */
    ~WavWriter() {
        if (file_) {
            close();
        }
    }

    // Non-copyable (owns a FILE*)
    WavWriter(const WavWriter&) = delete;
    WavWriter& operator=(const WavWriter&) = delete;

    // Movable
    WavWriter(WavWriter&& other) noexcept
        : file_(other.file_)
        , sampleRate_(other.sampleRate_)
        , numChannels_(other.numChannels_)
        , totalSamplesWritten_(other.totalSamplesWritten_)
    {
        other.file_ = nullptr;
        other.totalSamplesWritten_ = 0;
    }

    WavWriter& operator=(WavWriter&& other) noexcept {
        if (this != &other) {
            if (file_) {
                close();
            }
            file_ = other.file_;
            sampleRate_ = other.sampleRate_;
            numChannels_ = other.numChannels_;
            totalSamplesWritten_ = other.totalSamplesWritten_;

            other.file_ = nullptr;
            other.totalSamplesWritten_ = 0;
        }
        return *this;
    }

    // =========================================================================
    // File operations
    // =========================================================================

    /**
     * Open a WAV file for writing and emit the 44-byte header.
     *
     * The header is written with a placeholder data size (0xFFFFFFFF) so
     * that if the app crashes before close(), the file is still parseable
     * by most WAV readers (they will infer size from file length).
     *
     * NOT real-time safe (opens a file, writes to disk).
     *
     * @param path        Absolute file path for the output WAV file.
     * @param sampleRate  Sample rate in Hz (e.g., 48000).
     * @param numChannels Number of audio channels (1 = mono, 2 = stereo).
     *                    Defaults to 1 (mono).
     * @return true if the file was opened and the header was written
     *         successfully, false on I/O error.
     */
    bool open(const std::string& path, int32_t sampleRate,
              int32_t numChannels = 1) {
        if (file_) {
            close(); // Close any previously open file
        }

        file_ = std::fopen(path.c_str(), "wb");
        if (!file_) {
            return false;
        }

        sampleRate_ = sampleRate;
        numChannels_ = numChannels;
        totalSamplesWritten_ = 0;

        // Write the 44-byte WAV header with placeholder sizes
        return writeHeader();
    }

    /**
     * Append audio samples to the WAV file.
     *
     * Samples are written as 32-bit IEEE float values in the native byte
     * order (WAV specifies little-endian, and Android runs on ARM which
     * is little-endian, so no byte-swapping is needed).
     *
     * NOT real-time safe (writes to disk via buffered fwrite).
     *
     * @param data       Pointer to float samples. For stereo, samples must
     *                   be interleaved (L, R, L, R, ...).
     * @param numSamples Number of samples to write. For stereo, this is the
     *                   total number of float values (numFrames * numChannels).
     * @return true if all samples were written, false on I/O error.
     */
    bool writeSamples(const float* data, int32_t numSamples) {
        if (!file_ || !data || numSamples <= 0) {
            return false;
        }

        const size_t written = std::fwrite(data, sizeof(float),
                                           static_cast<size_t>(numSamples),
                                           file_);

        if (static_cast<int32_t>(written) != numSamples) {
            return false; // I/O error or disk full
        }

        totalSamplesWritten_ += numSamples;
        return true;
    }

    /**
     * Finalize the WAV file: seek back to the header and patch the RIFF
     * file size and data chunk size with the correct values, then close
     * the file handle.
     *
     * After close(), the WavWriter is in a "closed" state and can be
     * reused by calling open() again with a new path.
     *
     * NOT real-time safe (seeks and writes to disk, closes file).
     *
     * @return true if the header was patched and the file was closed
     *         successfully, false if an I/O error occurred during
     *         finalization. The file is always closed regardless.
     */
    bool close() {
        if (!file_) {
            return false;
        }

        bool success = true;

        // Compute actual sizes
        const uint32_t dataSize = static_cast<uint32_t>(
            totalSamplesWritten_ * sizeof(float));
        const uint32_t riffSize = dataSize + kHeaderSize - 8;

        // Patch RIFF chunk size at offset 4
        if (std::fseek(file_, 4, SEEK_SET) != 0) {
            success = false;
        } else if (std::fwrite(&riffSize, sizeof(uint32_t), 1, file_) != 1) {
            success = false;
        }

        // Patch data chunk size at offset 40
        if (std::fseek(file_, 40, SEEK_SET) != 0) {
            success = false;
        } else if (std::fwrite(&dataSize, sizeof(uint32_t), 1, file_) != 1) {
            success = false;
        }

        // Flush and close
        if (std::fflush(file_) != 0) {
            success = false;
        }
        std::fclose(file_);
        file_ = nullptr;

        return success;
    }

    // =========================================================================
    // Queries
    // =========================================================================

    /** Check if the writer currently has an open file. */
    bool isOpen() const {
        return file_ != nullptr;
    }

    /**
     * Get the total number of samples written since open().
     * For stereo files, this counts individual float values (not frames).
     *
     * @return Total samples written, or 0 if no file is open / nothing written.
     */
    int64_t totalSamplesWritten() const {
        return totalSamplesWritten_;
    }

    /**
     * Get the total number of frames written since open().
     * A frame contains one sample per channel.
     *
     * @return Total frames written.
     */
    int64_t totalFramesWritten() const {
        if (numChannels_ <= 0) return 0;
        return totalSamplesWritten_ / numChannels_;
    }

    /**
     * Get the duration of audio written so far, in seconds.
     *
     * @return Duration in seconds, or 0.0 if no file is open.
     */
    double durationSeconds() const {
        if (sampleRate_ <= 0 || numChannels_ <= 0) return 0.0;
        return static_cast<double>(totalSamplesWritten_)
               / (static_cast<double>(sampleRate_) * numChannels_);
    }

    // =========================================================================
    // Static utilities
    // =========================================================================

    /**
     * Repair the header of a WAV file that was not properly closed.
     *
     * Reads the actual file size, computes the correct RIFF and data
     * chunk sizes, and patches the header in place. The file must exist
     * and be at least 44 bytes (the minimum WAV header size).
     *
     * This is intended to be called on app startup to recover any WAV
     * files from a previous crash where close() was never called.
     *
     * @param path Absolute path to the WAV file to repair.
     * @return true if the header was successfully patched, false if the
     *         file could not be opened, is too small, or an I/O error
     *         occurred.
     */
    static bool repairHeader(const std::string& path) {
        FILE* f = std::fopen(path.c_str(), "r+b");
        if (!f) {
            return false;
        }

        // Get file size
        if (std::fseek(f, 0, SEEK_END) != 0) {
            std::fclose(f);
            return false;
        }
        const long fileSize = std::ftell(f);
        if (fileSize < static_cast<long>(kHeaderSize)) {
            std::fclose(f);
            return false; // File too small to contain a valid header
        }

        // Compute correct sizes
        const uint32_t dataSize = static_cast<uint32_t>(
            fileSize - static_cast<long>(kHeaderSize));
        const uint32_t riffSize = static_cast<uint32_t>(fileSize - 8);

        bool success = true;

        // Patch RIFF chunk size at offset 4
        if (std::fseek(f, 4, SEEK_SET) != 0) {
            success = false;
        } else if (std::fwrite(&riffSize, sizeof(uint32_t), 1, f) != 1) {
            success = false;
        }

        // Patch data chunk size at offset 40
        if (std::fseek(f, 40, SEEK_SET) != 0) {
            success = false;
        } else if (std::fwrite(&dataSize, sizeof(uint32_t), 1, f) != 1) {
            success = false;
        }

        std::fflush(f);
        std::fclose(f);

        return success;
    }

private:
    // =========================================================================
    // Constants
    // =========================================================================

    static constexpr uint32_t kHeaderSize      = 44;    ///< WAV header size in bytes
    static constexpr uint16_t kFormatFloat     = 3;     ///< IEEE float format tag
    static constexpr uint16_t kBitsPerSample   = 32;    ///< 32-bit float samples
    static constexpr uint32_t kFmtChunkSize    = 16;    ///< Standard fmt chunk (no extensible)

    // =========================================================================
    // Internal methods
    // =========================================================================

    /**
     * Write the 44-byte WAV header to the file.
     *
     * Uses placeholder data size (0xFFFFFFFF) for crash resilience. The
     * RIFF size is also set to a placeholder. Both are patched by close().
     *
     * The header is constructed as a byte array and written in a single
     * fwrite call to minimize I/O overhead and guarantee atomicity of
     * the header write on most filesystems.
     *
     * @return true if the header was written successfully.
     */
    bool writeHeader() {
        // Build the 44-byte header in a local buffer
        uint8_t header[kHeaderSize];
        std::memset(header, 0, kHeaderSize);

        const uint32_t byteRate = static_cast<uint32_t>(
            sampleRate_ * numChannels_ * (kBitsPerSample / 8));
        const uint16_t blockAlign = static_cast<uint16_t>(
            numChannels_ * (kBitsPerSample / 8));

        // Placeholder sizes (patched on close)
        const uint32_t placeholderRiffSize = 0xFFFFFFFFu - 8u;
        const uint32_t placeholderDataSize = 0xFFFFFFFFu;

        // RIFF header
        std::memcpy(header + 0, "RIFF", 4);
        std::memcpy(header + 4, &placeholderRiffSize, 4);
        std::memcpy(header + 8, "WAVE", 4);

        // fmt sub-chunk
        std::memcpy(header + 12, "fmt ", 4);
        std::memcpy(header + 16, &kFmtChunkSize, 4);

        const uint16_t formatTag = kFormatFloat;
        const uint16_t channels = static_cast<uint16_t>(numChannels_);
        const uint32_t sampleRateU32 = static_cast<uint32_t>(sampleRate_);
        const uint16_t bitsPerSample = kBitsPerSample;

        std::memcpy(header + 20, &formatTag, 2);
        std::memcpy(header + 22, &channels, 2);
        std::memcpy(header + 24, &sampleRateU32, 4);
        std::memcpy(header + 28, &byteRate, 4);
        std::memcpy(header + 32, &blockAlign, 2);
        std::memcpy(header + 34, &bitsPerSample, 2);

        // data sub-chunk
        std::memcpy(header + 36, "data", 4);
        std::memcpy(header + 40, &placeholderDataSize, 4);

        // Write entire header in one call
        const size_t written = std::fwrite(header, 1, kHeaderSize, file_);
        return written == kHeaderSize;
    }

    // =========================================================================
    // Member variables
    // =========================================================================

    FILE* file_              = nullptr;  ///< Output file handle (owned)
    int32_t sampleRate_      = 0;        ///< Sample rate in Hz
    int32_t numChannels_     = 0;        ///< Number of audio channels
    int64_t totalSamplesWritten_ = 0;    ///< Running count of float values written
};

#endif // GUITAR_ENGINE_WAV_WRITER_H
