#pragma once
#include <vector>

namespace t2t::stoch {

struct OuParams { double kappa, theta, sigma; };

// Fit OU parameters from a series x[0..n-1] sampled at fixed dt seconds.
// Discrete model: x_{t+Δ} = a x_t + b + ε, with
//   a = e^{-κΔ}, b = θ(1-a), Var(ε) = σ^2 (1-e^{-2κΔ})/(2κ)
OuParams fit_ou(const std::vector<double>& x, double dt);

} // namespace t2t::stoch
