#include "ou.h"
#include <cassert>
#include <cmath>

namespace t2t::stoch {

OuParams fit_ou(const std::vector<double>& x, double dt) {
  assert(x.size() >= 3);
  const size_t n = x.size() - 1;

  double sx=0, sy=0, sxx=0, sxy=0;
  for (size_t t=0; t<n; ++t) {
    const double xt = x[t];
    const double yt = x[t+1];
    sx  += xt; sy  += yt;
    sxx += xt*xt; sxy += xt*yt;
  }
  const double denom = n*sxx - sx*sx;
  const double a = (n*sxy - sx*sy) / denom;
  const double b = (sy - a*sx) / static_cast<double>(n);

  double sse = 0.0;
  for (size_t t=0; t<n; ++t) {
    const double r = x[t+1] - (a*x[t] + b);
    sse += r*r;
  }
  const double var_eps = sse / static_cast<double>(n - 2);

  const double kappa = -std::log(a) / dt;
  const double theta = b / (1.0 - a);
  const double sigma = std::sqrt(var_eps * (2.0*kappa) / (1.0 - std::exp(-2.0*kappa*dt)));
  return {kappa, theta, sigma};
}

} // namespace t2t::stoch
