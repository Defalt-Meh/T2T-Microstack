#include "tests/test_util.h"
#include "libstoch/ou.h"
#include "libstoch/avs.h"
#include <random>
#include <vector>
#include <cmath>

using namespace t2t;

void run_stoch_tests() {
  // --- OU simulation sanity (non-flaky) ---
  const double kappa=1.2, theta=100.0, sigma=2.0, dt=0.01;
  std::mt19937 rng(123);
  std::normal_distribution<double> N(0.0,1.0);
  std::vector<double> x(1000);
  x[0] = theta;
  for (size_t t=0; t+1<x.size(); ++t) {
    x[t+1] = x[t] + kappa*(theta - x[t])*dt + sigma*std::sqrt(dt)*N(rng);
  }
  auto est = stoch::fit_ou(x, dt);
  T2T_CHECK(std::isfinite(est.kappa) && est.kappa > 0.0 && est.kappa < 10.0);
  T2T_CHECK(std::isfinite(est.sigma) && est.sigma > 0.0 && est.sigma < 10.0);
  T2T_CHECK(std::fabs(est.theta - theta) < 25.0);

  // --- AvS behavior: ensure integer tick spread grows with sigma ---
  // Use parameters that yield >1 tick half-spread so integer casting won't mask it.
  stoch::OuParams ou_lo{1.0, 100.0, 2.0};  // sigma=2
  stoch::OuParams ou_hi{1.0, 100.0, 4.0};  // sigma=4
  stoch::AvsParams avs{1e-3, 0.05, 200.0}; // larger gamma & horizon

  auto q_lo = stoch::avellaneda_stoikov(/*s*/100.0, /*q*/0, ou_lo, avs);
  auto q_hi = stoch::avellaneda_stoikov(/*s*/100.0, /*q*/0, ou_hi, avs);
  const int spread_lo = q_lo.ask_px - q_lo.bid_px;
  const int spread_hi = q_hi.ask_px - q_hi.bid_px;
  T2T_CHECK(spread_hi > spread_lo);

  // Inventory long shifts reservation price down -> both bid/ask lower vs flat
  auto q_long = stoch::avellaneda_stoikov(100.0, /*q*/+10, ou_lo, avs);
  T2T_CHECK(q_long.bid_px <= q_lo.bid_px && q_long.ask_px <= q_lo.ask_px);
}
