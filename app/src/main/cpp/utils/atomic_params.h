#ifndef GUITAR_ENGINE_ATOMIC_PARAMS_H
#define GUITAR_ENGINE_ATOMIC_PARAMS_H

#include <atomic>
#include <type_traits>

/**
 * Thread-safe wrapper for a single parameter value shared between
 * the UI thread (writer) and the audio thread (reader).
 *
 * Uses acquire/release memory ordering which is the minimum needed to
 * guarantee the audio thread sees the latest value written by the UI thread.
 * This is cheaper than sequential consistency on ARM architectures.
 *
 * The type T must be trivially copyable and small enough for lock-free
 * atomic operations (typically <= 8 bytes on 64-bit platforms).
 *
 * Usage:
 *   AtomicParam<float> gain{1.0f};
 *
 *   // UI thread:
 *   gain.set(0.75f);
 *
 *   // Audio thread (real-time safe read):
 *   float currentGain = gain.get();
 */
template<typename T>
class AtomicParam {
    static_assert(std::is_trivially_copyable_v<T>,
                  "AtomicParam requires a trivially copyable type");

public:
    /** Construct with an initial value. */
    explicit AtomicParam(T initial = T{}) : value_(initial) {}

    /**
     * Set a new value from the UI thread.
     * Uses release ordering so the audio thread will see this write
     * when it does an acquire load.
     */
    void set(T newValue) {
        value_.store(newValue, std::memory_order_release);
    }

    /**
     * Read the current value from the audio thread.
     * Uses acquire ordering to see the latest value stored by the UI thread.
     * This is real-time safe: no allocation, no lock, no syscall.
     */
    T get() const {
        return value_.load(std::memory_order_acquire);
    }

    /** Implicit conversion for convenient use in expressions on the audio thread. */
    operator T() const { return get(); }

private:
    std::atomic<T> value_;
};

/**
 * Struct holding a set of commonly-used audio parameters that can be
 * updated atomically from the UI thread. Each field is independently
 * atomic, so updating one parameter does not require updating all.
 */
struct AudioParams {
    AtomicParam<float> inputGain{1.0f};
    AtomicParam<float> outputGain{1.0f};
    AtomicParam<bool> bypassed{false};
};

#endif // GUITAR_ENGINE_ATOMIC_PARAMS_H
