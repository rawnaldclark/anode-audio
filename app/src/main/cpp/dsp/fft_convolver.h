#ifndef GUITAR_ENGINE_FFT_CONVOLVER_H
#define GUITAR_ENGINE_FFT_CONVOLVER_H

#include <cstdint>

/*
 * Forward-declare the PFFFT setup struct so we don't need to include pffft.h
 * in the header. Consumers of this class only need to know it exists;
 * the actual PFFFT calls live entirely in the .cpp file.
 */
struct PFFFT_Setup;

/**
 * Real-time FFT convolver using uniformly partitioned overlap-add.
 *
 * This class convolves an audio stream with an arbitrary-length impulse
 * response (IR) using the frequency-domain overlap-add method. It is
 * designed for use in the Oboe audio callback, where:
 *   - Frame counts vary between callbacks (typically 64-1024)
 *   - Zero heap allocations are permitted during processing
 *   - Latency must be bounded and predictable
 *
 * Algorithm overview (uniformly partitioned overlap-add):
 *   1. The IR is split into P partitions of B samples each, zero-padded
 *      to N=2B, and pre-transformed to the frequency domain during init().
 *   2. A circular buffer of P input-block spectra is maintained.
 *   3. For each B-sample input block:
 *      a. Zero-pad to N samples and forward-FFT
 *      b. Multiply-accumulate: for each partition p, multiply the input
 *         spectrum from p blocks ago with IR partition p's spectrum
 *      c. Inverse FFT the accumulated spectrum
 *      d. Add the first B samples of the IFFT result to the overlap tail
 *         from the previous block to produce B output samples
 *      e. Save the last B samples as the new overlap tail
 *   4. Input/output ring buffers decouple the variable Oboe callback
 *      size from the fixed internal block size B.
 *
 * Latency: exactly B samples (one internal block). The first B output
 * samples are silence while the first input block accumulates.
 *
 * PFFFT is used for all FFT operations. It provides NEON-optimized
 * transforms on ARM and SSE on x86 from a single C codebase.
 *
 * Memory: All FFT buffers are allocated with pffft_aligned_malloc()
 * (16-byte alignment required by PFFFT SIMD) during init(). The
 * process() method performs zero allocations.
 *
 * Thread safety: init() and cleanup() must not be called concurrently
 * with process(). process() and reset() are audio-thread safe (no
 * allocations, no locks, no syscalls).
 */
class FFTConvolver {
public:
    FFTConvolver();
    ~FFTConvolver();

    /* Non-copyable, non-movable: owns raw PFFFT-allocated resources that
     * cannot be trivially transferred. Preventing moves avoids subtle bugs
     * where a moved-from object still holds dangling pointers. */
    FFTConvolver(const FFTConvolver&) = delete;
    FFTConvolver& operator=(const FFTConvolver&) = delete;
    FFTConvolver(FFTConvolver&&) = delete;
    FFTConvolver& operator=(FFTConvolver&&) = delete;

    /**
     * Initialize the convolver with an impulse response.
     *
     * Partitions the IR, pre-computes frequency-domain representations,
     * and allocates all processing buffers. May be called multiple times
     * (re-entrant): a previous initialization is cleaned up first.
     *
     * NOT real-time safe (allocates memory).
     *
     * @param ir         Pointer to the impulse response samples.
     * @param irLength   Number of samples in the IR (must be >= 1).
     * @param blockSize  Internal processing block size in samples.
     *                   Must be a power of 2 and >= 64. Default: 256.
     * @return true on success, false if parameters are invalid or
     *         PFFFT setup creation fails.
     */
    bool init(const float* ir, int irLength, int blockSize = 256);

    /**
     * Process audio through the convolver. Real-time safe.
     *
     * Accepts any number of frames (need not match blockSize). Uses
     * internal ring buffers to accumulate input into fixed-size blocks,
     * runs the FFT convolution, and drains output back to the caller.
     *
     * Before the first full block is processed, outputs silence (zero).
     *
     * @param buffer     Audio samples to convolve in-place (mono float).
     * @param numFrames  Number of samples in the buffer.
     */
    void process(float* buffer, int numFrames);

    /**
     * Reset all processing state to silence without deallocating.
     *
     * Clears the input/output rings, input spectrum history, overlap
     * tail, and all FFT scratch buffers. Call this when the audio
     * stream restarts to prevent stale data from leaking through.
     *
     * Real-time safe (memset only, no allocations).
     */
    void reset();

    /**
     * Get the processing latency introduced by the convolver.
     *
     * The latency is exactly one internal block size (B samples),
     * because the first output block cannot be produced until a full
     * input block has been accumulated.
     *
     * @return Latency in samples, or 0 if not initialized.
     */
    int getLatencySamples() const;

    /**
     * Check whether the convolver has been successfully initialized.
     * @return true if init() completed successfully.
     */
    bool isInitialized() const;

private:
    /**
     * Process exactly one B-sample block through the convolution engine.
     *
     * This is the core of the uniformly partitioned overlap-add algorithm:
     *   1. Copy input into the first B slots of fftInputBuf_, zero-pad rest
     *   2. Forward FFT -> store in circular input spectrum buffer
     *   3. Multiply-accumulate across all IR partitions
     *   4. Inverse FFT the accumulated spectrum
     *   5. Add overlap tail from previous block to first B samples
     *   6. Save last B samples as new overlap tail
     *
     * Real-time safe (no allocations).
     *
     * @param input   Pointer to B input samples.
     * @param output  Pointer to B output samples (written, not accumulated).
     */
    void processBlock(const float* input, float* output);

    /**
     * Free all allocated resources and reset state to uninitialized.
     *
     * Frees PFFFT setup and all pffft_aligned_malloc'd buffers.
     * Safe to call multiple times (idempotent via null checks).
     *
     * NOT real-time safe (frees memory).
     */
    void cleanup();

    // =========================================================================
    // PFFFT plan
    // =========================================================================

    PFFFT_Setup* fftSetup_ = nullptr;

    // =========================================================================
    // Configuration (set once in init(), read-only during process())
    // =========================================================================

    /** Internal block size B. Input is processed in chunks of this size. */
    int blockSize_ = 0;

    /** FFT size N = 2 * B. The zero-padded transform length. */
    int fftSize_ = 0;

    /** Number of IR partitions P = ceil(irLength / B). */
    int numPartitions_ = 0;

    /**
     * Precomputed 1/N for IFFT normalization.
     * Passed as the 'scaling' parameter to pffft_zconvolve_* functions,
     * which fold the multiplication into the spectral convolution for free.
     * This avoids a separate O(N) normalization loop after the IFFT.
     */
    float invFftSize_ = 0.0f;

    // =========================================================================
    // Pre-transformed IR partitions (allocated in init(), read in process())
    // =========================================================================

    /**
     * Frequency-domain IR partitions stored contiguously.
     * Layout: P blocks of N floats each, in PFFFT's native z-domain order.
     * Partition p starts at irSpectra_[p * fftSize_].
     * Total size: P * N floats.
     */
    float* irSpectra_ = nullptr;

    // =========================================================================
    // Circular input spectrum buffer
    // =========================================================================

    /**
     * Circular buffer of P input-block spectra.
     * Each entry is N floats in PFFFT z-domain order.
     * The most recent input spectrum is at index currentPartition_.
     * The spectrum from p blocks ago is at
     *   (currentPartition_ - p + numPartitions_) % numPartitions_.
     * Total size: P * N floats.
     */
    float* inputSpectra_ = nullptr;

    /** Index of the most recently written slot in the input spectrum ring. */
    int currentPartition_ = 0;

    // =========================================================================
    // FFT scratch buffers (allocated in init(), used in processBlock())
    // =========================================================================

    /** Forward FFT input buffer. Size: N floats. First B filled with input,
     *  remaining B zeroed (overlap-add zero-padding). */
    float* fftInputBuf_ = nullptr;

    /** Inverse FFT output buffer. Size: N floats. Holds time-domain result
     *  after IFFT: first B samples are output, last B are overlap tail. */
    float* fftOutputBuf_ = nullptr;

    /** Spectral accumulator for multiply-accumulate across partitions.
     *  Size: N floats. Written by zconvolve_no_accu (first partition),
     *  accumulated by zconvolve_accumulate (remaining partitions). */
    float* spectralAccum_ = nullptr;

    /** PFFFT work buffer for transform calls. Size: N floats.
     *  Required by pffft_transform when N >= 16384 to avoid stack overflow
     *  on threads with small stacks. We always provide it for safety. */
    float* workBuf_ = nullptr;

    /** Overlap tail from the previous block's IFFT. Size: B floats.
     *  Added to the first B samples of the next block's IFFT output. */
    float* overlapTail_ = nullptr;

    // =========================================================================
    // Input/output ring buffers (decouple callback size from block size)
    // =========================================================================

    /** Input accumulation ring. Samples from process() are written here
     *  until B samples are available, then processBlock() is called.
     *  Size: B floats. */
    float* inputRing_ = nullptr;

    /** Output drain ring. processBlock() writes B output samples here,
     *  which are then drained back to the caller's buffer in process().
     *  Size: B floats. */
    float* outputRing_ = nullptr;

    /** Write position in inputRing_ (0 to B-1). */
    int inputRingPos_ = 0;

    /** Read position in outputRing_ (0 to B-1). */
    int outputRingPos_ = 0;

    /** Number of output samples available to drain from outputRing_.
     *  Starts at 0 (no output available until first block is processed).
     *  After processBlock(), set to B. */
    int outputRingAvail_ = 0;

    /** True once init() has completed successfully. */
    bool initialized_ = false;
};

#endif // GUITAR_ENGINE_FFT_CONVOLVER_H
