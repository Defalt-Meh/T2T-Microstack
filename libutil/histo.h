#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace t2t::histo {

struct Histo {
  // Fixed microsecond bucket upper bounds, inclusive.
  std::vector<uint32_t> edges_us;
  std::vector<uint64_t> counts;
  explicit Histo(std::vector<uint32_t> edges) : edges_us(std::move(edges)), counts(edges_us.size(), 0) {}
  inline void add_ns(uint64_t ns) noexcept {
    uint32_t us = (uint32_t)(ns / 1000u);
    size_t i = 0;
    for (; i < edges_us.size(); ++i) { if (us <= edges_us[i]) { counts[i]++; return; } }
    if (!counts.empty()) counts.back()++;
  }
};

struct AllStageHistos {
  Histo parse, lob, sig, risk, e2e;
  explicit AllStageHistos(const std::vector<uint32_t>& edges)
  : parse(edges), lob(edges), sig(edges), risk(edges), e2e(edges) {}
};

void write_csv(const std::string& path, const AllStageHistos& h);

} // namespace t2t::histo
