#pragma once
#include <cstdint>
#include <algorithm>
#include "liblob/lob.h"

namespace t2t::sig {

struct Quote {
  int32_t bid_px, ask_px;
  int32_t bid_qty, ask_qty;
};

class MM {
  // tiny queue-reactive heuristic with inventory skew
  int32_t last_mid_{0};
  int recent_execs_{0}, recent_cancels_{0};
  int window_{128};
public:
  // q_alpha: widen spread with |inv| ; skew: px skew magnitude with inv
  Quote quote(const lob::Lob& book, double q_alpha, double skew, int inv, int inv_cap) {
    const int bb = book.best_bid();
    const int aa = book.best_ask();
    const int32_t mid = (bb == INT32_MIN || aa == INT32_MAX)
                        ? (last_mid_ ? last_mid_ : 0)
                        : static_cast<int32_t>((bb + aa) / 2);
    last_mid_ = mid;

    int32_t base = 2 + (recent_execs_ > recent_cancels_ ? 2 : 0);
    base += static_cast<int32_t>(q_alpha * static_cast<double>(std::max(0, std::abs(inv))));

    const double skew_px = skew * static_cast<double>(inv) / static_cast<double>(std::max(1, inv_cap));
    const int32_t bid = mid - base - static_cast<int32_t>(skew_px);
    const int32_t ask = mid + base - static_cast<int32_t>(skew_px);
    return Quote{bid, ask, 1, 1};
  }

  // event hooks (optional in this step)
  inline void on_exec()   { if (++recent_execs_   > window_) recent_execs_   = window_; }
  inline void on_cancel() { if (++recent_cancels_ > window_) recent_cancels_ = window_; }
  inline void decay()     { if (recent_execs_) --recent_execs_; if (recent_cancels_) --recent_cancels_; }
};

} // namespace t2t::sig
