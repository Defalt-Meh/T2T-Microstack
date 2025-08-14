#include "avs.h"
#include <cmath>

namespace t2t::stoch {

QuotePx avellaneda_stoikov(double s, int q,
                           const OuParams& ou,
                           const AvsParams& avs) {
  const double sig2H = ou.sigma * ou.sigma * avs.horizon_s;
  const double rp = s - static_cast<double>(q) * avs.gamma * sig2H;
  const double half = (1.0/avs.k) * std::log(1.0 + avs.gamma/avs.k) + 0.5 * avs.gamma * sig2H;
  const int32_t bid = static_cast<int32_t>(rp - half);
  const int32_t ask = static_cast<int32_t>(rp + half);
  return { bid, ask };
}

} // namespace t2t::stoch
