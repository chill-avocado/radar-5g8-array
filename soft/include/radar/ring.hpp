//============================================================================
// ring.hpp -- lock-free single-producer single-consumer ring buffer
//
// One writer thread, one reader thread, no mutex, no allocation once the ring
// exists.  This is the only structure the receive thread and the pipeline
// share, so it has to be correct rather than merely fast: the receive thread
// is running under a real-time policy and must never block on anything the
// scheduler could hold.
//
// HOW IT IS SAFE WITHOUT A LOCK
//   head_ is written only by the producer, tail_ only by the consumer.  Each
//   side publishes its index with a release store and reads the other side's
//   with an acquire load, which is exactly enough ordering for the data written
//   into a slot before the release to be visible to whoever sees the new index.
//   No compare-and-swap is needed because there is never more than one writer
//   of either index.
//
//   head_ and tail_ sit on separate 64-byte cache lines.  Sharing a line would
//   make every producer store invalidate the consumer's copy and vice versa --
//   false sharing, and on this Core i5 it costs roughly an order of magnitude
//   in throughput.
//
// CAPACITY
//   A power of two, enforced at compile time, so the wrap is a mask and not a
//   modulo.  One slot is left permanently empty: that is what distinguishes
//   "full" (head + 1 == tail) from "empty" (head == tail) without a separate
//   count that both sides would have to write.  A ring declared with capacity
//   N therefore holds N - 1 items.
//============================================================================
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace radar {

/// Cache line size assumed throughout.  std::hardware_destructive_interference_size
/// is C++17 but libc++ does not ship it, so it is spelled out.
constexpr std::size_t kRingCacheLine = 64;

template <typename T, std::size_t Capacity>
class SpscRing {
    static_assert(Capacity >= 2, "capacity must be at least 2");
    static_assert((Capacity & (Capacity - 1)) == 0, "capacity must be a power of two");

public:
    using value_type = T;

    SpscRing() = default;

    ~SpscRing() {
        // Destroy whatever is still queued.  Only safe once both threads have
        // stopped, which is the caller's contract for destruction.
        while (T* p = front()) {
            p->~T();
            tail_.store((tail_.load(std::memory_order_relaxed) + 1) & kMask,
                        std::memory_order_relaxed);
        }
    }

    SpscRing(const SpscRing&)            = delete;
    SpscRing& operator=(const SpscRing&) = delete;
    SpscRing(SpscRing&&)                 = delete;
    SpscRing& operator=(SpscRing&&)      = delete;

    //------------------------------------------------------------------
    // Producer side.  Only one thread may call these.
    //------------------------------------------------------------------

    /// Move an item in.  False when the ring is full; the item is untouched.
    bool try_push(T&& v) {
        const std::size_t h    = head_.load(std::memory_order_relaxed);
        const std::size_t next = (h + 1) & kMask;
        if (next == tail_.load(std::memory_order_acquire)) return false;
        new (slot(h)) T(std::move(v));
        head_.store(next, std::memory_order_release);
        return true;
    }

    /// Copy an item in.  False when the ring is full.
    bool try_push(const T& v) {
        const std::size_t h    = head_.load(std::memory_order_relaxed);
        const std::size_t next = (h + 1) & kMask;
        if (next == tail_.load(std::memory_order_acquire)) return false;
        new (slot(h)) T(v);
        head_.store(next, std::memory_order_release);
        return true;
    }

    /// Construct in place from the given arguments, with no temporary.
    template <typename... Args>
    bool try_emplace(Args&&... args) {
        const std::size_t h    = head_.load(std::memory_order_relaxed);
        const std::size_t next = (h + 1) & kMask;
        if (next == tail_.load(std::memory_order_acquire)) return false;
        new (slot(h)) T(std::forward<Args>(args)...);
        head_.store(next, std::memory_order_release);
        return true;
    }

    /// Count of items the producer had to throw away.  The producer increments
    /// this itself with drop(); the ring never discards anything on its own,
    /// because only the producer knows whether dropping or blocking is right.
    void drop(std::uint64_t n = 1) {
        dropped_.store(dropped_.load(std::memory_order_relaxed) + n,
                       std::memory_order_relaxed);
    }
    std::uint64_t dropped() const { return dropped_.load(std::memory_order_relaxed); }

    //------------------------------------------------------------------
    // Consumer side.  Only one thread may call these.
    //------------------------------------------------------------------

    /// Move the oldest item out.  False when the ring is empty.
    bool try_pop(T& out) {
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire)) return false;
        T* p = slot(t);
        out  = std::move(*p);
        p->~T();
        tail_.store((t + 1) & kMask, std::memory_order_release);
        return true;
    }

    /// Pointer to the oldest item without removing it, or null when empty.
    /// Valid until the next pop.
    T* front() {
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire)) return nullptr;
        return slot(t);
    }

    /// Discard the oldest item.  Undefined when empty; check front() first.
    void pop_front() {
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        slot(t)->~T();
        tail_.store((t + 1) & kMask, std::memory_order_release);
    }

    //------------------------------------------------------------------
    // Observers.  Safe from either thread, but the answer is a snapshot: by
    // the time it is read the other thread may have changed it.
    //------------------------------------------------------------------
    std::size_t size() const {
        const std::size_t h = head_.load(std::memory_order_acquire);
        const std::size_t t = tail_.load(std::memory_order_acquire);
        return (h - t) & kMask;
    }
    bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }
    bool full() const {
        const std::size_t h = head_.load(std::memory_order_acquire);
        return ((h + 1) & kMask) == tail_.load(std::memory_order_acquire);
    }
    /// Items the ring can hold, which is one less than the declared capacity.
    static constexpr std::size_t capacity() { return Capacity - 1; }

private:
    static constexpr std::size_t kMask = Capacity - 1;

    T* slot(std::size_t i) { return reinterpret_cast<T*>(storage_) + i; }

    alignas(kRingCacheLine) std::atomic<std::size_t> head_{0};
    alignas(kRingCacheLine) std::atomic<std::size_t> tail_{0};
    alignas(kRingCacheLine) std::atomic<std::uint64_t> dropped_{0};
    alignas(kRingCacheLine) alignas(T) unsigned char storage_[sizeof(T) * Capacity];
};

} // namespace radar
