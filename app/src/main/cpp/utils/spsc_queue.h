#ifndef GUITAR_ENGINE_SPSC_QUEUE_H
#define GUITAR_ENGINE_SPSC_QUEUE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

/**
 * Lock-free single-producer single-consumer (SPSC) ring buffer.
 *
 * Designed for real-time audio use: the audio callback thread (consumer)
 * never blocks, and the UI/control thread (producer) can push parameter
 * updates without locks.
 *
 * Template parameters:
 *   T        - Element type (must be trivially copyable for real-time safety)
 *   Capacity - Buffer size, must be a power of 2 for efficient modulo via bitmask
 *
 * Memory ordering:
 *   - write index uses release semantics (producer publishes data)
 *   - read index uses acquire semantics (consumer sees published data)
 *   - This is the minimum ordering needed for correctness on ARM/x86.
 */
template<typename T, size_t Capacity>
class SpscQueue {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "SpscQueue capacity must be a power of 2");
    static_assert(std::is_trivially_copyable_v<T>,
                  "SpscQueue element type must be trivially copyable for real-time safety");

public:
    SpscQueue() : readIndex_(0), writeIndex_(0) {}

    /**
     * Push an element into the queue. Called by the producer thread only.
     *
     * @param item The element to enqueue.
     * @return true if the element was enqueued, false if the queue is full.
     */
    bool push(const T& item) {
        const size_t currentWrite = writeIndex_.load(std::memory_order_relaxed);
        const size_t nextWrite = (currentWrite + 1) & kMask;

        // Check if queue is full by comparing next write position to read position
        if (nextWrite == readIndex_.load(std::memory_order_acquire)) {
            return false; // Queue is full
        }

        buffer_[currentWrite] = item;

        // Release ensures the item write is visible before the index update
        writeIndex_.store(nextWrite, std::memory_order_release);
        return true;
    }

    /**
     * Pop an element from the queue. Called by the consumer thread only.
     *
     * @param item Output parameter receiving the dequeued element.
     * @return true if an element was dequeued, false if the queue is empty.
     */
    bool pop(T& item) {
        const size_t currentRead = readIndex_.load(std::memory_order_relaxed);

        // Check if queue is empty
        if (currentRead == writeIndex_.load(std::memory_order_acquire)) {
            return false; // Queue is empty
        }

        item = buffer_[currentRead];

        const size_t nextRead = (currentRead + 1) & kMask;
        // Release ensures the item read completes before we advance the index
        readIndex_.store(nextRead, std::memory_order_release);
        return true;
    }

    /**
     * Check if the queue is empty. Can be called from either thread,
     * but the result is only a snapshot and may be stale by the time
     * the caller acts on it.
     */
    bool isEmpty() const {
        return readIndex_.load(std::memory_order_acquire) ==
               writeIndex_.load(std::memory_order_acquire);
    }

    /**
     * Approximate number of elements available. Not exact due to
     * concurrent access, but useful for monitoring/debugging.
     */
    size_t sizeApprox() const {
        const size_t write = writeIndex_.load(std::memory_order_acquire);
        const size_t read = readIndex_.load(std::memory_order_acquire);
        return (write - read) & kMask;
    }

private:
    static constexpr size_t kMask = Capacity - 1;

    // Separate cache lines to avoid false sharing between producer and consumer.
    // The producer writes to writeIndex_ and the consumer writes to readIndex_.
    alignas(64) std::atomic<size_t> readIndex_;
    alignas(64) std::atomic<size_t> writeIndex_;

    T buffer_[Capacity];
};

#endif // GUITAR_ENGINE_SPSC_QUEUE_H
