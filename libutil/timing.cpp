#include "timing.h"
#include <algorithm>
#include <fstream>

namespace t2t::timing {

ScopedTimer::~ScopedTimer() noexcept {
  auto t1 = clock::now();
  uint64_t ns = (uint64_t) std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
  buf.push(ns);
}

uint64_t now_ns() {
  return (uint64_t) std::chrono::duration_cast<std::chrono::nanoseconds>(
           clock::now().time_since_epoch()).count();
}

static void write_one(std::ofstream& ofs, const char* stage,
                      const std::vector<uint64_t>& ns, size_t warmup, size_t total) {
  size_t start = warmup < ns.size() ? warmup : ns.size();
  size_t end   = std::min(total, ns.size());
  for (size_t i = start; i < end; ++i) {
    ofs << stage << ',' << ns[i] << '\n';
  }
}

void write_csv_latency(const std::string& path,
                       const StageTimers& st,
                       size_t warmup,
                       size_t total) {
  std::ofstream ofs(path, std::ios::out | std::ios::trunc);
  ofs << "stage,ns\n";
  write_one(ofs, "parse", st.parse.ns, warmup, total);
  write_one(ofs, "lob",   st.lob.ns,   warmup, total);
  write_one(ofs, "sig",   st.sig.ns,   warmup, total);
  write_one(ofs, "risk",  st.risk.ns,  warmup, total);
  write_one(ofs, "e2e",   st.e2e.ns,   warmup, total);
}

static double quantile_us(std::vector<uint64_t> v, size_t warmup, size_t total, double q) {
  size_t start = std::min(warmup, v.size());
  size_t end   = std::min(total, v.size());
  if (end <= start + 1) return 0.0;
  auto first = v.begin() + (ptrdiff_t)start;
  auto last  = v.begin() + (ptrdiff_t)end;
  auto kth   = first + (ptrdiff_t)((end - start - 1) * q);
  std::nth_element(first, kth, last);
  uint64_t ns = *kth;
  return ns / 1000.0;
}

Summary summarize(const std::vector<uint64_t>& ns, size_t warmup, size_t total) {
  Summary s;
  s.p50_us  = quantile_us(ns, warmup, total, 0.50);
  s.p90_us  = quantile_us(ns, warmup, total, 0.90);
  s.p99_us  = quantile_us(ns, warmup, total, 0.99);
  s.p999_us = quantile_us(ns, warmup, total, 0.999);
  return s;
}

} // namespace t2t::timing
