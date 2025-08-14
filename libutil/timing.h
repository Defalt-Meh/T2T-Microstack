#pragma once
#include <cstdint>
#include <chrono>
#include <string>
#include <vector>
#include <atomic>

namespace t2t::timing {

using clock = std::chrono::steady_clock;

struct SampleBuffer {
  // Preallocated buffer for per-message ns samples.
  std::vector<uint64_t> ns;
  std::atomic<size_t>   idx{0};
  explicit SampleBuffer(size_t cap) : ns(cap, 0) {}
  inline void push(uint64_t v) noexcept {
    size_t i = idx.fetch_add(1, std::memory_order_relaxed);
    if (i < ns.size()) ns[i] = v;
  }
};

struct StageTimers {
  SampleBuffer parse, lob, sig, risk, e2e;
  explicit StageTimers(size_t cap)
  : parse(cap), lob(cap), sig(cap), risk(cap), e2e(cap) {}
};

struct ScopedTimer {
  SampleBuffer& buf;
  const clock::time_point t0;
  explicit ScopedTimer(SampleBuffer& b) noexcept : buf(b), t0(clock::now()) {}
  ~ScopedTimer() noexcept;
};

uint64_t now_ns();

void write_csv_latency(const std::string& path,
                       const StageTimers& st,
                       size_t warmup,
                       size_t total);

struct Summary {
  double p50_us{}, p90_us{}, p99_us{}, p999_us{};
};

Summary summarize(const std::vector<uint64_t>& ns, size_t warmup, size_t total);

} // namespace t2t::timing
