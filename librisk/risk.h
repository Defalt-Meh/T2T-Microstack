#pragma once
#include <cstdint>
#include "libsig/mm.h"

namespace t2t::risk {

// Simple inventory/notional/throttle gate.
// For now notional is unused (we'll wire PnL later).
class Risk {
  int     inv_cap_{100};
  double  notional_cap_{1e12};
  int     throttle_per_ms_{100};
  uint64_t cur_ms_{0};
  int     sent_in_ms_{0};
  bool    killed_{false};
public:
  void configure(int inv_cap, double notional_cap, int throttle) {
    inv_cap_ = inv_cap; notional_cap_ = notional_cap; throttle_per_ms_ = throttle;
  }
  void kill() { killed_ = true; }

  bool allow(const sig::Quote& q, int inv, int /*inv_cap*/, double /*notional_cap*/, uint64_t ts_ns) {
    if (killed_) return false;

    // Inventory soft cap: don’t allow quoting on the side that increases imbalance
    if (inv > inv_cap_) {
      if (q.bid_qty > 0) return false; // would buy more -> worse
    } else if (-inv > inv_cap_) {
      if (q.ask_qty > 0) return false; // would sell more -> worse
    }

    // Throttle by ms
    const uint64_t ms = ts_ns / 1'000'000ull;
    if (ms != cur_ms_) { cur_ms_ = ms; sent_in_ms_ = 0; }
    if (sent_in_ms_ >= throttle_per_ms_) return false;
    ++sent_in_ms_;

    return true;
  }
};

} // namespace t2t::risk
