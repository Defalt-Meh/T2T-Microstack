# T2T-Microstack — Tick-to-Trade in Microseconds

A production-style miniature HFT stack built for low latency, determinism, and correctness. In replay mode it pushes an ITCH-like event stream through:

```
parse → price-time LOB → (heuristic or Avellaneda–Stoikov) signal → risk gates → CSV encode
```

You get per-stage timing histograms, a no-malloc guard on the hot path, and byte-wise determinism checks. The code aims to be small enough to read in an afternoon, but disciplined the way real HFT code is.

## Table of Contents

- [Non-functional SLOs](#non-functional-slos)
- [Repository Layout](#repository-layout)
- [Build & Dependencies](#build--dependencies)
- [Quickstart](#quickstart)
- [Data Model & Formats](#data-model--formats)
- [System Architecture](#system-architecture)
- [Performance & Observability](#performance--observability)
- [Measured Results](#measured-results-this-run)
- [Determinism](#determinism)
- [Risk Gates](#risk-gates)
- [Stochastic Layer: OU + Avellaneda–Stoikov](#stochastic-layer-ou--avellaneda–stoikov)
- [Design Notes: LOB, Ring, Memory Discipline](#design-notes-lob-ring-memory-discipline)
- [Reproducibility Notes](#reproducibility-notes)
- [Acceptance Checklist](#acceptance-checklist)
- [Troubleshooting](#troubleshooting)
- [CI](#ci)
- [License](#license)
- [Appendix: Mathematical Details](#appendix-mathematical-details)

## Non-functional SLOs

Replay mode; single core, pinned; warm-ups discarded.

- **Latency** (end-to-end parse→LOB→signal→risk→encode): p50 ≤ 20 µs, p99 ≤ 80 µs
- **Throughput**: ≥ 1 M msgs/s sustained on synthetic ITCH stream
- **Hot path**: zero dynamic allocations after warm-up (guard trips on any new/malloc)
- **Determinism**: three identical replays produce byte-identical outputs
- **Observability**: per-stage histograms (p50/p90/p99/p99.9) + counters to CSV; PNG plots
- **Reproducibility**: fixed CPU affinity; warm-ups discarded; hardware/OS recorded

## Repository Layout

```
apps/      t2t_main.cpp                # ties modules, CLI, timers, CSV logging
libring/   spsc_ring.hpp               # lock-free SPSC ring (header-only)
libitch/   itch.hpp, itch.cpp          # ITCH-like CSV replay loader
liblob/    lob.hpp, lob.cpp            # price-time LOB (SoA, fixed pools)
libsig/    mm.hpp                      # queue-reactive MM signal
librisk/   risk.hpp                    # inventory, throttle, notional, kill-switch
libstoch/  ou.{hpp,cpp}, avs.{hpp,cpp} # OU fit + Avellaneda–Stoikov quoting
libutil/   affinity.hpp, timing.*, histo.*, nomalloc.*
tests/     unit tests incl. determinism & stochastic behavior
tools/     gen_synth_feed.py, plot_latency.py, diff_runs.py
ci/        workflow.yaml
```

## Build & Dependencies

**Requirements:**
- C++20, CMake ≥ 3.20
- Compilation flags: `-O3 -march=native -fno-exceptions -fno-rtti -Werror`
- No external C++ libs (std-only)
- Python 3.10+ for tools (recommended: matplotlib + pandas)

**Setup:**

Create a venv and install plotting deps:
```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -U pip pandas matplotlib
```

## Quickstart

```bash
# build + tests
cmake -S . -B build
cmake --build build -j
(cd build && ctest --output-on-failure)

# generate synthetic feed (e.g., 200k)
python tools/gen_synth_feed.py --out feed.csv --n 200000 --seed 7

# run (heuristic mode)
./build/t2t_main \
  --replay feed.csv \
  --results out.csv \
  --latency latency.csv \
  --histo latency_hist.csv \
  --warmup 200 \
  --max-msgs 200000 \
  --mode heuristic

# plots
source .venv/bin/activate
python tools/plot_latency.py --lat latency.csv --out-prefix latency
deactivate
```

## Data Model & Formats

Prices are integer ticks (`int32_t px`). Sides are boolean (`is_buy`: buy=1/sell=0).

**Input (ITCH-like CSV):**
```csv
ts_ns,type,order_id,side,px,qty
1,A,1,1,100,5     # add
2,C,1,1,0,0       # cancel (idempotent)
3,E,2,0,102,3     # execute (references existing order_id)
```

**Output (normalized executions & quotes):**
```csv
ts_ns,event,order_id,side,px,qty,inv_after,notional_after
```

## System Architecture

```
CSV replay → parse (already structured)
           → price-time LOB update
           → quote (heuristic or AvS)
           → risk.allow?
           → encode CSV (buffered)
```

- **LOB**: structure-of-arrays with fixed pools per side; FIFO per price level; idempotent cancels
- **Signal**:
  - `heuristic`: queue-reactive spread based on cancels/execs + inventory skew
  - `avs`: (Avellaneda–Stoikov) closed-form quoting fed by OU-estimated volatility
- **Risk**: inventory cap, per-ms throttle, notional cap scaffold, kill-switch
- **Observability**: per-stage timers + histograms; no-malloc guard enabled after warm-up

## Performance & Observability

We record per-stage nanosecond samples to `latency.csv` (parse, lob, sig, risk, e2e) and bucketized histograms to `latency_hist.csv`. The plotting tool emits one PNG per stage.

## Measured Results (this run)

To print exact quantiles (µs) from `latency.csv`:

```bash
source .venv/bin/activate
python - << 'PY'
import pandas as pd
df = pd.read_csv('latency.csv')
for s in ['parse','lob','sig','risk','e2e']:
    d = (df[df['stage']==s]['ns']/1000.0)
    if d.empty: continue
    print(f"{s:>4}: p50={d.quantile(0.50):.3f}  p90={d.quantile(0.90):.3f}  p99={d.quantile(0.99):.3f}  p99.9={d.quantile(0.999):.3f}")
PY
deactivate
```

| Stage | Target (µs) | Measured p50 / p90 / p99 / p99.9 (µs) |
|-------|-------------|---------------------------------------|
| Parse (ITCH) | ≤ 3 |  |
| LOB update | ≤ 5 |  |
| Signal (MM) | ≤ 4 |  |
| Risk gates | ≤ 3 |  |
| End-to-end | ≤ 20 |  |

## Determinism

Three identical replays must yield byte-identical outputs:

```bash
./build/t2t_main --replay feed.csv --results out1.csv --latency lat1.csv --histo hist1.csv --warmup 5000 --max-msgs 1000000 --mode heuristic
./build/t2t_main --replay feed.csv --results out2.csv --latency lat2.csv --histo hist2.csv --warmup 5000 --max-msgs 1000000 --mode heuristic
python tools/diff_runs.py out1.csv out2.csv   # → "IDENTICAL"
```

A unit test (`tests/determinism_test.cpp`) asserts this on a micro-feed (three runs, byte-wise equality).

**Why this matters**: Determinism makes regression analysis surgical: byte-diff across commits exposes exact behavioral changes without the usual replay noise.

## Risk Gates

- **Inventory cap**: blocks the side that would worsen over-inventory
- **Per-ms throttle**: hard limit on orders/ms (simple, effective)
- **Notional cap scaffold**: wire up to your MTM/P&L as needed
- **Kill-switch**: instant cutoff

All checks are synchronous and branch-light.

## Stochastic Layer: OU + Avellaneda–Stoikov

### OU calibration

We model mid (or log-mid) $X_t$ as Ornstein–Uhlenbeck:

$$dX_t = \kappa(\theta - X_t) dt + \sigma dW_t$$

Discretized at interval $\Delta$:

$$X_{t+\Delta} = aX_t + b + \varepsilon_t, \quad a = e^{-\kappa\Delta}, \quad b = \theta(1-a), \quad \varepsilon_t \sim \mathcal{N}(0, \sigma_\Delta^2)$$

$$\sigma_\Delta^2 = \sigma^2 \frac{1 - e^{-2\kappa\Delta}}{2\kappa}$$

We OLS-fit $a, b$ in $Y = aX + b + \varepsilon$ and invert:

$$\hat{\kappa} = -\frac{\ln \hat{a}}{\Delta}, \quad \hat{\theta} = \frac{\hat{b}}{1 - \hat{a}}, \quad \hat{\sigma} = \hat{\sigma}_\Delta^2 \cdot \sqrt{\frac{2\hat{\kappa}}{1 - e^{-2\hat{\kappa}\Delta}}}$$

**Implementation**: `libstoch/ou.cpp`. (Standard errors & QQ-plot CSV are straightforward extensions.)

### Avellaneda–Stoikov quoting

With exponential utility and Poisson fills $\lambda_\pm(\delta) = Ae^{-k\delta}$, the HJB yields a closed-form reservation price and half-spread:

$$r_t = s_t - q_t \gamma \sigma^2 (T-t), \quad \delta_t^* = \frac{1}{k}\ln\left(1 + \frac{\gamma}{k}\right) + \frac{1}{2}\gamma\sigma^2(T-t)$$

Quotes are $r_t \pm \delta_t^*$. Higher inventory shifts/skews quotes; higher volatility or risk aversion widens spreads; shorter horizons tighten.

**Implementation**: `libstoch/avs.cpp`. In `--mode avs`, we fit OU on a rolling mid series and quote using $(\gamma, k, \text{horizon})$.

**Ticking note**: Prices are integer ticks. If $\delta^*$ is sub-tick, integer rounding can collapse spreads; choose $\gamma, k, \text{horizon}$ to be tick-meaningful for the symbol.

## Design Notes: LOB, Ring, Memory Discipline

- **LOB (SoA)**: fixed pools; FIFO per price level; idempotent cancels; invariants (non-negative sizes, monotone timestamps); no heap once warmed
- **SPSC Ring**: `libring/spsc_ring.hpp`, cache-line padded; ready to decouple feed/strategy in live mode
- **No-malloc guard**: enables after warm-up; any hot-path allocation aborts with a clear message. We pre-allocate the CSV buffer before enabling the guard; we free it after disabling the guard.

## Reproducibility Notes

Fill these after your 1M run:

- **CPU**: (e.g., Apple M-series / Intel 13th-gen / AMD Ryzen 7000)
- **OS**: (e.g., macOS 14.x / Ubuntu 24.04)
- **Governor**: performance on Linux
- **Affinity**: `--pinner N` (pin main thread)
- **Warm-up**: first N msgs discarded; guard enables afterward

**Recipe (1M msgs):**
```bash
python tools/gen_synth_feed.py --out feed1m.csv --n 1000000 --seed 42
./build/t2t_main --replay feed1m.csv --results out.csv --latency latency.csv --histo hist.csv --warmup 5000 --max-msgs 1000000 --mode heuristic
python tools/plot_latency.py --lat latency.csv --out-prefix latency
```

## Acceptance Checklist

- [ ] Latency SLOs met (e2e p50 ≤ 20 µs, p99 ≤ 80 µs); results recorded
- [ ] Throughput ≥ 1 M msgs/s on synthetic
- [ ] No allocations after warm-up (guard proves it)
- [ ] Determinism: three runs → byte-identical logs
- [ ] OU fit returns sensible $\hat{\kappa}, \hat{\theta}, \hat{\sigma}$; residuals behave
- [ ] AvS widens with higher $\sigma$ and skews with inventory
- [ ] README updated with hardware, results table, and plots
- [ ] CI green on Linux & macOS

## Troubleshooting

- **Guard trips (`[nomalloc] allocation detected...`)** → A hot-loop allocation snuck in; pre-reserve vectors, use pre-allocated buffers, avoid dynamic `std::string` churn in the loop
- **Clang `-Wsign-conversion`** → Use `size_t` for indexing; cast carefully when assigning to signed fields
- **Determinism diff** → Remove non-deterministic containers, timestamps, or RNG from the hot path

## CI

`ci/workflow.yaml` builds on macOS and Linux, runs unit tests, does a perf smoke (100k msgs), asserts a latency gate (e2e p50 ≤ 24 µs, p99 ≤ 96 µs for 20% slack), verifies determinism (diff of two runs), and uploads artifacts (`out*.csv`, `latency*.csv`, `hist*.csv`). If you prefer GitHub's conventional path, move it to `.github/workflows/ci.yaml`.

## License

MIT — attribution appreciated.

## Appendix: Mathematical Details

### OU → discrete regression

Given $X_{t+\Delta} = aX_t + b + \varepsilon_t$:

$$\hat{a} = \frac{n\sum x_t y_t - (\sum x_t)(\sum y_t)}{n\sum x_t^2 - (\sum x_t)^2}, \quad \hat{b} = \frac{\sum y_t - \hat{a}\sum x_t}{n}$$

Residual variance $\hat{\sigma}_\Delta^2 = SSE/(n-2)$.

Invert $(\hat{a}, \hat{b}, \hat{\sigma}_\Delta)$ to $(\hat{\kappa}, \hat{\theta}, \hat{\sigma})$ via:

$$\hat{\kappa} = -\frac{\ln \hat{a}}{\Delta}, \quad \hat{\theta} = \frac{\hat{b}}{1 - \hat{a}}, \quad \hat{\sigma} = \hat{\sigma}_\Delta^2 \cdot \sqrt{\frac{2\hat{\kappa}}{1 - e^{-2\hat{\kappa}\Delta}}}$$

### Avellaneda–Stoikov intuition

With exponential utility and $\lambda_\pm(\delta) = Ae^{-k\delta}$:

$$r_t = s_t - q_t \gamma \sigma^2 (T-t), \quad \delta_t^* = \frac{1}{k}\ln\left(1 + \frac{\gamma}{k}\right) + \frac{1}{2}\gamma\sigma^2(T-t)$$

The first term is a utility-curvature premium; the second is an inventory-risk buffer proportional to residual variance $\sigma^2(T-t)$.