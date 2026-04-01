#include "fft_convolver.h"
#include <pffft/pffft.h>
#include <cstring>
#include <cmath>
#include <android/log.h>

/*
 * Verify PFFFT SIMD is active at library load time.
 * This runs once when the shared library is loaded and logs whether
 * NEON (ARM) or SSE (x86) is being used. A simd_size of 4 means
 * SIMD is active; 1 means scalar fallback.
 */
namespace {
    struct PffftSIMDCheck {
        PffftSIMDCheck() {
            int simdSize = pffft_simd_size();
            __android_log_print(ANDROID_LOG_INFO, "GuitarEngine",
                "PFFFT SIMD size: %d (4 = SIMD active, 1 = scalar fallback)",
                simdSize);
        }
    };
    static PffftSIMDCheck sPffftCheck;
}

// =============================================================================
// Helper: PFFFT-aligned allocation with null check
// =============================================================================

/**
 * Allocate a float buffer with 16-byte alignment required by PFFFT.
 *
 * All buffers passed to pffft_transform() and pffft_zconvolve_*() must be
 * 16-byte aligned for SIMD (NEON/SSE) correctness. Using pffft_aligned_malloc
 * guarantees this regardless of platform.
 *
 * @param count  Number of floats to allocate.
 * @return Pointer to the aligned buffer, or nullptr on failure.
 */
static float* allocAligned(int count) {
    return static_cast<float*>(
        pffft_aligned_malloc(static_cast<size_t>(count) * sizeof(float)));
}

/**
 * Free a PFFFT-aligned buffer and set the pointer to nullptr.
 * Safe to call with a null pointer (pffft_aligned_free handles null).
 *
 * @param ptr  Reference to the pointer to free.
 */
static void freeAligned(float*& ptr) {
    if (ptr) {
        pffft_aligned_free(ptr);
        ptr = nullptr;
    }
}

// =============================================================================
// Construction / Destruction
// =============================================================================

FFTConvolver::FFTConvolver() = default;

FFTConvolver::~FFTConvolver() {
    cleanup();
}

// =============================================================================
// init()
// =============================================================================

bool FFTConvolver::init(const float* ir, int irLength, int blockSize) {
    // ------------------------------------------------------------------
    // Validate parameters
    // ------------------------------------------------------------------

    if (ir == nullptr || irLength < 1) {
        __android_log_print(ANDROID_LOG_ERROR, "GuitarEngine",
            "FFTConvolver::init: invalid IR (null=%d, length=%d)",
            ir == nullptr, irLength);
        return false;
    }

    if (blockSize < 64) {
        __android_log_print(ANDROID_LOG_ERROR, "GuitarEngine",
            "FFTConvolver::init: blockSize %d < minimum 64", blockSize);
        return false;
    }

    /* Verify blockSize is a power of two. The bitwise trick (n & (n-1)) == 0
     * works because a power of two has exactly one bit set, and subtracting
     * one flips that bit and sets all lower bits. ANDing gives zero only if
     * exactly one bit was set. */
    if ((blockSize & (blockSize - 1)) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, "GuitarEngine",
            "FFTConvolver::init: blockSize %d is not a power of 2", blockSize);
        return false;
    }

    // ------------------------------------------------------------------
    // Re-entrant: free previous resources if init() was called before
    // ------------------------------------------------------------------

    if (initialized_) {
        cleanup();
    }

    // ------------------------------------------------------------------
    // Compute dimensions
    // ------------------------------------------------------------------

    blockSize_ = blockSize;
    fftSize_ = 2 * blockSize;
    invFftSize_ = 1.0f / static_cast<float>(fftSize_);

    /* Number of partitions: ceil(irLength / blockSize).
     * Integer ceiling via (a + b - 1) / b avoids floating-point rounding. */
    numPartitions_ = (irLength + blockSize - 1) / blockSize;

    // ------------------------------------------------------------------
    // Create PFFFT plan
    // ------------------------------------------------------------------

    /* PFFFT_REAL: we are transforming real-valued audio signals.
     * The setup struct is read-only and could be shared across threads,
     * but we keep one per convolver instance for simplicity. */
    fftSetup_ = pffft_new_setup(fftSize_, PFFFT_REAL);
    if (fftSetup_ == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "GuitarEngine",
            "FFTConvolver::init: pffft_new_setup failed for N=%d", fftSize_);
        cleanup();
        return false;
    }

    // ------------------------------------------------------------------
    // Allocate all buffers with PFFFT alignment
    // ------------------------------------------------------------------

    irSpectra_     = allocAligned(numPartitions_ * fftSize_);
    inputSpectra_  = allocAligned(numPartitions_ * fftSize_);
    fftInputBuf_   = allocAligned(fftSize_);
    fftOutputBuf_  = allocAligned(fftSize_);
    spectralAccum_ = allocAligned(fftSize_);
    workBuf_       = allocAligned(fftSize_);
    overlapTail_   = allocAligned(blockSize_);
    inputRing_     = allocAligned(blockSize_);
    outputRing_    = allocAligned(blockSize_);

    /* Verify all allocations succeeded. On Android, malloc failure is rare
     * (the OOM killer fires first), but we check anyway for robustness. */
    if (!irSpectra_ || !inputSpectra_ || !fftInputBuf_ || !fftOutputBuf_ ||
        !spectralAccum_ || !workBuf_ || !overlapTail_ ||
        !inputRing_ || !outputRing_) {
        __android_log_print(ANDROID_LOG_ERROR, "GuitarEngine",
            "FFTConvolver::init: aligned allocation failed (P=%d, N=%d)",
            numPartitions_, fftSize_);
        cleanup();
        return false;
    }

    // ------------------------------------------------------------------
    // Zero all processing state buffers
    // ------------------------------------------------------------------

    std::memset(inputSpectra_, 0,
        static_cast<size_t>(numPartitions_) * fftSize_ * sizeof(float));
    std::memset(fftInputBuf_, 0,
        static_cast<size_t>(fftSize_) * sizeof(float));
    std::memset(fftOutputBuf_, 0,
        static_cast<size_t>(fftSize_) * sizeof(float));
    std::memset(spectralAccum_, 0,
        static_cast<size_t>(fftSize_) * sizeof(float));
    std::memset(workBuf_, 0,
        static_cast<size_t>(fftSize_) * sizeof(float));
    std::memset(overlapTail_, 0,
        static_cast<size_t>(blockSize_) * sizeof(float));
    std::memset(inputRing_, 0,
        static_cast<size_t>(blockSize_) * sizeof(float));
    std::memset(outputRing_, 0,
        static_cast<size_t>(blockSize_) * sizeof(float));

    // ------------------------------------------------------------------
    // Pre-transform IR partitions into frequency domain
    // ------------------------------------------------------------------

    /* Each partition is B samples of the IR, zero-padded to N=2B samples,
     * then forward-FFT'd. The frequency-domain data is stored in PFFFT's
     * native "z-domain" order (not reordered), which is the format required
     * by pffft_zconvolve_*. Re-ordering is unnecessary and would actually
     * break the convolve functions. */
    for (int p = 0; p < numPartitions_; ++p) {
        /* Determine how many IR samples belong to this partition.
         * The last partition may be shorter than B if irLength is not
         * a multiple of blockSize. */
        int srcOffset = p * blockSize_;
        int srcCount = irLength - srcOffset;
        if (srcCount > blockSize_) {
            srcCount = blockSize_;
        }

        /* Zero the entire N-sample input buffer first. This handles both:
         *   - The zero-padding in the second half (N/2..N-1) required by
         *     overlap-add to produce linear (not circular) convolution
         *   - Partial last partitions where srcCount < B */
        std::memset(fftInputBuf_, 0,
            static_cast<size_t>(fftSize_) * sizeof(float));

        /* Copy this partition's IR samples into the first B slots. */
        std::memcpy(fftInputBuf_, ir + srcOffset,
            static_cast<size_t>(srcCount) * sizeof(float));

        /* Forward FFT, storing the result directly into the irSpectra_ array
         * at this partition's offset. The work buffer is provided to avoid
         * stack allocation inside PFFFT (important for threads with small
         * stacks, which is common on Android). */
        pffft_transform(fftSetup_, fftInputBuf_,
            irSpectra_ + p * fftSize_, workBuf_, PFFFT_FORWARD);
    }

    // ------------------------------------------------------------------
    // Initialize ring buffer state
    // ------------------------------------------------------------------

    currentPartition_ = 0;
    inputRingPos_ = 0;
    outputRingPos_ = 0;

    /* Start with zero available output. The first B samples output by
     * process() will be silence (0.0f) while the first input block
     * accumulates. This models the one-block latency inherent to the
     * partitioned convolution. */
    outputRingAvail_ = 0;

    initialized_ = true;

    __android_log_print(ANDROID_LOG_INFO, "GuitarEngine",
        "FFTConvolver::init: OK (blockSize=%d, fftSize=%d, partitions=%d, "
        "irLength=%d, latency=%d samples)",
        blockSize_, fftSize_, numPartitions_, irLength, blockSize_);

    return true;
}

// =============================================================================
// process() -- real-time safe, zero allocations
// =============================================================================

void FFTConvolver::process(float* buffer, int numFrames) {
    /* Guard against zero or negative frame counts. A negative numFrames would
     * cause memset with a huge size_t (unsigned wrap), so we check first. */
    if (numFrames <= 0) return;

    /* Guard: if not initialized, zero the buffer to avoid passing garbage
     * downstream. This is a normal condition (e.g., convolver not yet set up). */
    if (!initialized_) {
        std::memset(buffer, 0, static_cast<size_t>(numFrames) * sizeof(float));
        return;
    }

    /* Process sample-by-sample, using the ring buffers to bridge between
     * the variable external frame count and the fixed internal block size.
     *
     * IMPORTANT: We must save the input sample BEFORE overwriting buffer[i]
     * with the output, because process() operates in-place. The order is:
     *   1. Save the input sample from buffer[i]
     *   2. Write the output sample to buffer[i]
     *   3. Store the saved input into the input ring
     *   4. If the input ring is full, run processBlock() */
    for (int i = 0; i < numFrames; ++i) {
        /* Save the input sample before we overwrite the buffer with output. */
        const float inputSample = buffer[i];

        /* --- Drain one output sample --- */
        if (outputRingAvail_ > 0) {
            buffer[i] = outputRing_[outputRingPos_];
            outputRingPos_++;
            outputRingAvail_--;
        } else {
            /* No output available yet (startup latency period). Output
             * silence. The convolution introduces exactly B samples of
             * latency because we need a full block of input before we
             * can produce the first block of output. */
            buffer[i] = 0.0f;
        }

        /* --- Accumulate the original input sample into the input ring --- */
        inputRing_[inputRingPos_] = inputSample;
        inputRingPos_++;

        /* --- When a full block is accumulated, process it --- */
        if (inputRingPos_ >= blockSize_) {
            processBlock(inputRing_, outputRing_);

            /* Reset ring positions for the next cycle. */
            inputRingPos_ = 0;
            outputRingPos_ = 0;
            outputRingAvail_ = blockSize_;
        }
    }
}

// =============================================================================
// processBlock() -- core partitioned overlap-add convolution
// =============================================================================

void FFTConvolver::processBlock(const float* input, float* output) {
    // --- Step 1: Prepare the FFT input buffer ---
    // Copy B input samples into the first half, zero the second half.
    // The zero-padding is essential for overlap-add: it ensures the
    // N-point circular convolution yields the same result as a 2B-point
    // linear convolution, preventing time-domain aliasing.
    std::memcpy(fftInputBuf_, input,
        static_cast<size_t>(blockSize_) * sizeof(float));
    std::memset(fftInputBuf_ + blockSize_, 0,
        static_cast<size_t>(blockSize_) * sizeof(float));

    // --- Step 2: Advance the circular partition index ---
    // We advance BEFORE writing so that currentPartition_ points to
    // the slot where the new input spectrum will be stored. During the
    // accumulation loop, the mapping is:
    //   p=0: currentPartition_     -> most recent input (pairs with IR[0])
    //   p=1: currentPartition_-1   -> previous input    (pairs with IR[1])
    //   p=k: currentPartition_-k   -> k blocks ago      (pairs with IR[k])
    currentPartition_ = (currentPartition_ + 1) % numPartitions_;

    // --- Step 3: Forward FFT the input block ---
    // Store the result in the circular input spectrum buffer at the
    // current partition slot. The output is in PFFFT's native z-domain
    // order, which is what pffft_zconvolve_* expects.
    float* currentInputSpectrum = inputSpectra_ + currentPartition_ * fftSize_;
    pffft_transform(fftSetup_, fftInputBuf_, currentInputSpectrum,
        workBuf_, PFFFT_FORWARD);

    // --- Step 4: Spectral multiply-accumulate across all partitions ---
    //
    // Convolution in the frequency domain:
    //   Y(k) = sum_{p=0}^{P-1} X_p(k) * H_p(k)
    //
    // where X_p is the input spectrum from p blocks ago and H_p is the
    // spectrum of IR partition p. The 1/N IFFT normalization is folded
    // into the scaling parameter of pffft_zconvolve_*. Since:
    //   accum = scaling * X_0*H_0 + scaling * X_1*H_1 + ...
    //         = scaling * (X_0*H_0 + X_1*H_1 + ...)
    //         = (1/N) * sum(X_p * H_p)
    // the normalization distributes correctly across the accumulation.

    // Partition 0: most recent input * earliest IR segment.
    // Uses _no_accu to OVERWRITE the accumulator (not add to stale data).
    {
        int inputIdx = currentPartition_;
        const float* inSpec = inputSpectra_ + inputIdx * fftSize_;
        const float* irSpec = irSpectra_; /* partition 0 starts at offset 0 */
        pffft_zconvolve_no_accu(fftSetup_, inSpec, irSpec,
            spectralAccum_, invFftSize_);
    }

    // Partitions 1..P-1: progressively older inputs paired with later
    // IR segments. Uses _accumulate to ADD to the running sum.
    for (int p = 1; p < numPartitions_; ++p) {
        /* Circular index: the input from p blocks ago. Adding numPartitions_
         * before the modulo ensures the result is non-negative even when
         * currentPartition_ < p. */
        int inputIdx =
            (currentPartition_ - p + numPartitions_) % numPartitions_;
        const float* inSpec = inputSpectra_ + inputIdx * fftSize_;
        const float* irSpec = irSpectra_ + p * fftSize_;
        pffft_zconvolve_accumulate(fftSetup_, inSpec, irSpec,
            spectralAccum_, invFftSize_);
    }

    // --- Step 5: Inverse FFT the accumulated spectrum ---
    // The result is already correctly scaled by 1/N because the scaling
    // was folded into the spectral multiply step above.
    // The N-sample time-domain output contains:
    //   [0..B-1]   : this block's contribution (add overlap tail -> output)
    //   [B..N-1]   : tail that overlaps into the next block
    pffft_transform(fftSetup_, spectralAccum_, fftOutputBuf_,
        workBuf_, PFFFT_BACKWARD);

    // --- Step 6: Overlap-add ---
    // Add the saved tail from the previous block to the first B samples
    // of this block's IFFT output. This completes the linear convolution
    // result for these B output samples.
    for (int i = 0; i < blockSize_; ++i) {
        output[i] = fftOutputBuf_[i] + overlapTail_[i];
    }

    // Save the second half (samples B..N-1) as the new overlap tail.
    // These will be added to the next block's first B output samples.
    std::memcpy(overlapTail_, fftOutputBuf_ + blockSize_,
        static_cast<size_t>(blockSize_) * sizeof(float));
}

// =============================================================================
// reset() -- real-time safe (memset only, no allocations)
// =============================================================================

void FFTConvolver::reset() {
    if (!initialized_) return;

    /* The IR spectra (irSpectra_) are NOT cleared because they are
     * read-only configuration data computed during init(). Only
     * processing state is reset. */

    /* Clear the circular input spectrum history. This prevents stale
     * spectral data from producing output when processing resumes
     * after a pause or stream restart. */
    std::memset(inputSpectra_, 0,
        static_cast<size_t>(numPartitions_) * fftSize_ * sizeof(float));

    /* Clear FFT scratch buffers. Not strictly necessary since they are
     * overwritten each block, but avoids any edge cases with partial
     * blocks or stale data. */
    std::memset(fftInputBuf_, 0,
        static_cast<size_t>(fftSize_) * sizeof(float));
    std::memset(fftOutputBuf_, 0,
        static_cast<size_t>(fftSize_) * sizeof(float));
    std::memset(spectralAccum_, 0,
        static_cast<size_t>(fftSize_) * sizeof(float));

    /* Clear the overlap tail so previous convolution results don't bleed
     * into new output after the reset. */
    std::memset(overlapTail_, 0,
        static_cast<size_t>(blockSize_) * sizeof(float));

    /* Clear and reset the input/output ring buffers. */
    std::memset(inputRing_, 0,
        static_cast<size_t>(blockSize_) * sizeof(float));
    std::memset(outputRing_, 0,
        static_cast<size_t>(blockSize_) * sizeof(float));

    /* Reset all position/state counters to initial values. The convolver
     * will output B samples of silence again (startup latency) after
     * reset, which is the correct behavior -- it prevents clicks from
     * partial convolution state. */
    currentPartition_ = 0;
    inputRingPos_ = 0;
    outputRingPos_ = 0;
    outputRingAvail_ = 0;
}

// =============================================================================
// getLatencySamples()
// =============================================================================

int FFTConvolver::getLatencySamples() const {
    /* Latency is exactly one block size B: we must accumulate B input
     * samples before we can produce the first B output samples. This
     * is inherent to the block-based FFT convolution approach. */
    return initialized_ ? blockSize_ : 0;
}

// =============================================================================
// isInitialized()
// =============================================================================

bool FFTConvolver::isInitialized() const {
    return initialized_;
}

// =============================================================================
// cleanup()
// =============================================================================

void FFTConvolver::cleanup() {
    /* Free the PFFFT plan first, since transforms must not be called
     * after the setup is destroyed. */
    if (fftSetup_) {
        pffft_destroy_setup(fftSetup_);
        fftSetup_ = nullptr;
    }

    /* Free all PFFFT-aligned buffers. The freeAligned helper sets each
     * pointer to nullptr after freeing, making cleanup() idempotent
     * (safe to call multiple times). */
    freeAligned(irSpectra_);
    freeAligned(inputSpectra_);
    freeAligned(fftInputBuf_);
    freeAligned(fftOutputBuf_);
    freeAligned(spectralAccum_);
    freeAligned(workBuf_);
    freeAligned(overlapTail_);
    freeAligned(inputRing_);
    freeAligned(outputRing_);

    /* Reset all scalar state to default values so the object is in
     * a clean "uninitialized" state, consistent with a freshly
     * constructed instance. */
    blockSize_ = 0;
    fftSize_ = 0;
    numPartitions_ = 0;
    invFftSize_ = 0.0f;
    currentPartition_ = 0;
    inputRingPos_ = 0;
    outputRingPos_ = 0;
    outputRingAvail_ = 0;
    initialized_ = false;
}
