#ifndef GUITAR_ENGINE_SPSC_RING_BUFFER_H
#define GUITAR_ENGINE_SPSC_RING_BUFFER_H

#include <atomic>
#include <cstdint>
#include <cstring>
#include <vector>

/**
 * Lock-free Single-Producer Single-Consumer (SPSC) ring buffer for
 * streaming audio data from the real-time audio thread to a writer
 * thread without blocking.
 *
 * Design goals:
 *   - Zero allocations in write() and read() (buffer pre-allocated).
 *   - No locks, no system calls, no blocking in the producer path.
 *   - Power-of-2 capacity for branchless wrap via bitwise AND masking
 *     (eliminates expensive modulo operation in the hot path).
 *   - Single atomic store (release) on write, single atomic load (acquire)
 *     on read. No CAS loops, no contention.
 *
 * Memory ordering rationale:
 *   - The producer (audio thread) writes data into the buffer, then stores
 *     writePos_ with release semantics. This guarantees that the data
 *     writes are visible to the consumer before the updated writePos_.
 *   - The consumer (writer thread) loads writePos_ with acquire semantics,
 *     guaranteeing it sees all data written before that position.
 *   - readPos_ follows the same pattern in reverse: consumer stores with
 *     release, producer loads with acquire.
 *   - This is the standard Lamport single-producer single-consumer queue
 *     pattern, proven correct for x86 and ARM architectures.
 *
 * Capacity note:
 *   The usable capacity is (powerOf2Size - 1) elements, because one slot
 *   is reserved to distinguish full from empty. The constructor rounds up
 *   the requested capacity to the next power of 2 and that full power-of-2
 *   size is allocated, yielding at least the requested usable capacity.
 *
 * Usage example (looper linear recording):
 *   - Audio thread (producer): calls write() with each block of processed
 *     samples. If the ring is full, samples are silently dropped (acceptable
 *     because the writer thread is expected to drain faster than real-time).
 *   - Writer thread (consumer): calls read() in a loop, writing drained
 *     samples to disk via WavWriter. Sleeps briefly when the ring is empty.
 *
 * Thread safety:
 *   - Exactly ONE producer thread may call write().
 *   - Exactly ONE consumer thread may call read().
 *   - reset() is NOT thread-safe and must only be called when neither
 *     thread is accessing the buffer (e.g., during init/teardown).
 *
 * @tparam T Element type (typically float for audio samples).
 */
template<typename T>
class SPSCRingBuffer {
public:
    /**
     * Construct a ring buffer with at least the given usable capacity.
     *
     * The internal buffer size is rounded up to the next power of 2 to
     * enable branchless wrap-around via bitwise AND. The actual usable
     * capacity is (rounded size - 1) and is always >= the requested value.
     *
     * NOT real-time safe (allocates memory).
     *
     * @param requestedCapacity Minimum number of elements that must be
     *                          writable when the buffer is empty. Must be > 0.
     */
    explicit SPSCRingBuffer(int32_t requestedCapacity)
        : mask_(nextPowerOf2(requestedCapacity + 1) - 1)
        , buffer_(mask_ + 1)
        , writePos_(0)
        , readPos_(0)
    {
    }

    ~SPSCRingBuffer() = default;

    // Non-copyable, non-movable (contains atomics)
    SPSCRingBuffer(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer(SPSCRingBuffer&&) = delete;
    SPSCRingBuffer& operator=(SPSCRingBuffer&&) = delete;

    // =========================================================================
    // Producer interface (audio thread ONLY)
    // =========================================================================

    /**
     * Write elements into the ring buffer (producer / audio thread).
     *
     * REAL-TIME SAFE: No allocations, no locks, no system calls.
     *
     * If the ring buffer does not have enough space for the entire write,
     * NO partial write is performed and the call returns false. This is
     * intentional: partial writes would produce corrupted audio on the
     * consumer side. The caller (audio callback) should treat a false
     * return as "data dropped" and continue processing.
     *
     * Memory ordering: data is written first, then writePos_ is updated
     * with release semantics so the consumer sees consistent data.
     *
     * @param data  Pointer to elements to write. Must not be null.
     * @param count Number of elements to write. Must be > 0.
     * @return true if all elements were written, false if the ring was
     *         too full (data is silently dropped).
     */
    bool write(const T* data, int32_t count) {
        const int32_t w = writePos_.load(std::memory_order_relaxed);
        const int32_t r = readPos_.load(std::memory_order_acquire);
        const int32_t available = availableWriteInternal(w, r);

        if (count > available) {
            return false; // Not enough space; drop the entire block
        }

        // Write data in one or two memcpy calls (handles wrap-around)
        const int32_t writeIdx = w & mask_;
        const int32_t bufferSize = mask_ + 1;
        const int32_t firstChunk = std::min(count, bufferSize - writeIdx);

        std::memcpy(buffer_.data() + writeIdx, data,
                    firstChunk * sizeof(T));

        if (firstChunk < count) {
            // Wrap: write the remainder at the beginning of the buffer
            std::memcpy(buffer_.data(), data + firstChunk,
                        (count - firstChunk) * sizeof(T));
        }

        // Publish the new write position (release ensures data is visible)
        writePos_.store(w + count, std::memory_order_release);

        return true;
    }

    // =========================================================================
    // Consumer interface (writer thread ONLY)
    // =========================================================================

    /**
     * Read elements from the ring buffer (consumer / writer thread).
     *
     * Reads up to maxCount elements into the output buffer. Returns the
     * number of elements actually read (may be less than maxCount if the
     * ring does not contain that many elements, or 0 if the ring is empty).
     *
     * Memory ordering: writePos_ is loaded with acquire semantics so all
     * data written by the producer before that position is visible.
     * After reading, readPos_ is updated with release semantics.
     *
     * @param data     Output buffer to copy elements into. Must have room
     *                 for at least maxCount elements.
     * @param maxCount Maximum number of elements to read.
     * @return Number of elements actually read (0 if ring is empty).
     */
    int32_t read(T* data, int32_t maxCount) {
        const int32_t r = readPos_.load(std::memory_order_relaxed);
        const int32_t w = writePos_.load(std::memory_order_acquire);
        const int32_t available = availableReadInternal(w, r);

        if (available <= 0) {
            return 0;
        }

        const int32_t toRead = std::min(maxCount, available);
        const int32_t readIdx = r & mask_;
        const int32_t bufferSize = mask_ + 1;
        const int32_t firstChunk = std::min(toRead, bufferSize - readIdx);

        std::memcpy(data, buffer_.data() + readIdx,
                    firstChunk * sizeof(T));

        if (firstChunk < toRead) {
            // Wrap: read the remainder from the beginning of the buffer
            std::memcpy(data + firstChunk, buffer_.data(),
                        (toRead - firstChunk) * sizeof(T));
        }

        // Publish the new read position (release ensures consumer-side
        // reads are complete before the producer reclaims the slots)
        readPos_.store(r + toRead, std::memory_order_release);

        return toRead;
    }

    // =========================================================================
    // Query interface (safe from either thread)
    // =========================================================================

    /**
     * Number of elements available to read.
     *
     * The result is a snapshot and may be stale by the time the caller acts
     * on it, but it is always a conservative undercount (the actual number
     * can only increase from the consumer's perspective).
     *
     * @return Number of readable elements.
     */
    int32_t availableRead() const {
        const int32_t w = writePos_.load(std::memory_order_acquire);
        const int32_t r = readPos_.load(std::memory_order_relaxed);
        return availableReadInternal(w, r);
    }

    /**
     * Number of elements that can be written before the ring is full.
     *
     * The result is a snapshot and may be stale, but it is always a
     * conservative undercount (the actual space can only increase from
     * the producer's perspective).
     *
     * @return Number of writable element slots.
     */
    int32_t availableWrite() const {
        const int32_t w = writePos_.load(std::memory_order_relaxed);
        const int32_t r = readPos_.load(std::memory_order_acquire);
        return availableWriteInternal(w, r);
    }

    /**
     * Get the total usable capacity of the ring buffer.
     * This is (internal buffer size - 1) due to the full/empty disambiguation slot.
     *
     * @return Maximum number of elements that can be stored simultaneously.
     */
    int32_t capacity() const {
        return mask_; // (powerOf2Size - 1)
    }

    // =========================================================================
    // Reset (NOT thread-safe)
    // =========================================================================

    /**
     * Reset read and write positions to zero, effectively clearing the buffer.
     *
     * WARNING: NOT thread-safe. Must only be called when no producer or
     * consumer thread is accessing the buffer (e.g., during init/teardown).
     * Does NOT zero the underlying memory (unnecessary for correctness).
     */
    void reset() {
        writePos_.store(0, std::memory_order_relaxed);
        readPos_.store(0, std::memory_order_relaxed);
    }

private:
    /**
     * Round up to the next power of 2.
     *
     * Uses the standard bit-twiddling technique: decrement, smear the
     * highest set bit downward, then increment. This produces the smallest
     * power of 2 >= n.
     *
     * @param n Input value (must be > 0).
     * @return Smallest power of 2 >= n.
     */
    static int32_t nextPowerOf2(int32_t n) {
        if (n <= 1) return 1;
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n++;
        return n;
    }

    /**
     * Compute readable element count from write and read positions.
     * Works correctly even when positions wrap past INT32_MAX because
     * subtraction of unsigned-equivalent values yields the correct
     * difference modulo 2^32.
     */
    int32_t availableReadInternal(int32_t w, int32_t r) const {
        return w - r;
    }

    /**
     * Compute writable slot count from write and read positions.
     * Reserves one slot to distinguish full from empty.
     */
    int32_t availableWriteInternal(int32_t w, int32_t r) const {
        return mask_ - (w - r);
    }

    // ---- Members ----

    const int32_t mask_;                  ///< Bitmask for branchless wrap: (bufferSize - 1)
    std::vector<T> buffer_;              ///< Pre-allocated element storage

    /// Write position (monotonically increasing). Only the producer writes.
    /// Masked with mask_ to index into buffer_. Uses release on store.
    std::atomic<int32_t> writePos_;

    /// Read position (monotonically increasing). Only the consumer writes.
    /// Masked with mask_ to index into buffer_. Uses release on store.
    std::atomic<int32_t> readPos_;
};

#endif // GUITAR_ENGINE_SPSC_RING_BUFFER_H
