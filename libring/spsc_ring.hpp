#pragma once
#include <cstddef>
#include <cstdint>
#include <atomic>
#include <type_traits>
#include <cassert>
#include <new>

// Single-Producer/Single-Consumer ring (power-of-two capacity).
// Non-blocking try_push / try_pop. Cache-line padded head/tail to avoid false sharing.
namespace t2t::ring {

template <typename T>
class SpscRing {
  static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

 public:
  explicit SpscRing(size_t capacity_pow2)
  : cap_(capacity_pow2), mask_(capacity_pow2 - 1),
    buf_(static_cast<T*>(::operator new[](capacity_pow2 * sizeof(T)))) {
    assert((capacity_pow2 & (capacity_pow2 - 1)) == 0 && "capacity must be power of two");
  }
  ~SpscRing() { ::operator delete[](buf_); }

  SpscRing(const SpscRing&)            = delete;
  SpscRing& operator=(const SpscRing&) = delete;

  // Producer thread
  bool try_push(const T& x) noexcept {
    const size_t head = head_.load(std::memory_order_relaxed);
    const size_t next = (head + 1u) & mask_;
    if (next == tail_.load(std::memory_order_acquire)) return false; // full
    buf_[head] = x;
    head_.store(next, std::memory_order_release);
    return true;
  }

  // Consumer thread
  bool try_pop(T& out) noexcept {
    const size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) return false; // empty
    out = buf_[tail];
    tail_.store((tail + 1u) & mask_, std::memory_order_release);
    return true;
  }

  size_t capacity() const noexcept { return cap_; }

 private:
  alignas(64) std::atomic<size_t> head_{0};
  alignas(64) std::atomic<size_t> tail_{0};
  const size_t cap_;
  const size_t mask_;
  alignas(64) T* const buf_;
  char _pad_[64]; // guard against false sharing with following objects
};

} // namespace t2t::ring
