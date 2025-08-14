#pragma once
#include <cstdint>
#include "ou.h"

namespace t2t::stoch {

struct AvsParams { double gamma, k, horizon_s; };
struct QuotePx { int32_t bid_px; int32_t ask_px; };

// Closed-form reservation price and half-spread
// rp = s - q * γ σ^2 (T-t)
// δ* = (1/k) ln(1 + γ/k) + (γ σ^2 (T-t))/2
QuotePx avellaneda_stoikov(double s, int q,
                           const OuParams& ou,
                           const AvsParams& avs);

} // namespace t2t::stoch
