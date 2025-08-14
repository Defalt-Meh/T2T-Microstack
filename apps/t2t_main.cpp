#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>

#include "libutil/affinity.h"
#include "libutil/timing.h"
#include "libutil/histo.h"
#include "libutil/nomalloc.h"
#include "libitch/itch.h"
#include "liblob/lob.h"
#include "libsig/mm.h"
#include "librisk/risk.h"
#include "libstoch/ou.h"
#include "libstoch/avs.h"

using namespace t2t;

struct Args {
  std::string replay, results="out.csv", latency="latency.csv", histo="latency_hist.csv";
  int core=-1, warmup=200, max_msgs=1'000'000;
  int inv_cap=100, throttle=200;
  double notional_cap=1e12;
  std::string mode="heuristic";
  double avs_gamma=1e-6, avs_k=0.1, avs_horizon=10.0;
};

static void usage() {
  std::fprintf(stderr,
    "t2t_main --replay path.csv [--results out.csv] [--latency lat.csv] [--histo hist.csv]\n"
    "         [--pinner core_id] [--warmup N] [--max-msgs N]\n"
    "         [--inv-cap N] [--throttle N_per_ms]\n"
    "         [--mode heuristic|avs] [--avs-gamma G] [--avs-k K] [--avs-horizon S]\n");
}

static bool parse_args(int argc, char** argv, Args& a) {
  for (int i=1;i<argc;i++) {
    auto eq   = [&](const char* k){ return std::strcmp(argv[i], k)==0; };
    auto next = [&]{ return (i+1<argc) ? argv[++i] : (char*)nullptr; };
    if (eq("--replay")) a.replay = next();
    else if (eq("--results")) a.results = next();
    else if (eq("--latency")) a.latency = next();
    else if (eq("--histo")) a.histo = next();
    else if (eq("--pinner")) a.core = std::atoi(next());
    else if (eq("--warmup")) a.warmup = std::atoi(next());
    else if (eq("--max-msgs")) a.max_msgs = std::atoi(next());
    else if (eq("--inv-cap")) a.inv_cap = std::atoi(next());
    else if (eq("--throttle")) a.throttle = std::atoi(next());
    else if (eq("--mode")) a.mode = next();
    else if (eq("--avs-gamma")) a.avs_gamma = std::atof(next());
    else if (eq("--avs-k")) a.avs_k = std::atof(next());
    else if (eq("--avs-horizon")) a.avs_horizon = std::atof(next());
    else { std::fprintf(stderr, "Unknown arg: %s\n", argv[i]); return false; }
  }
  if (a.replay.empty()) { usage(); return false; }
  return true;
}

struct PnL {
  int inv{0};
  double pnl{0.0};
  void on_exec(int32_t px, int32_t qty, bool is_buy) {
    inv += is_buy ? qty : -qty;
    pnl += (is_buy ? -1.0 : 1.0) * static_cast<double>(px) * static_cast<double>(qty);
  }
};

// fast append of one CSV line for outputs
static inline int write_line(FILE* f, uint64_t ts_ns, char ev, uint32_t oid, bool side,
                             int32_t px, int32_t qty, int inv_after, double notional_after) {
  return std::fprintf(f, "%llu,%c,%u,%d,%d,%d,%d,%.6f\n",
                      (unsigned long long)ts_ns, ev, oid, side?1:0,
                      px, qty, inv_after, notional_after);
}

int main(int argc, char** argv) {
  Args args;
  if (!parse_args(argc, argv, args)) return 2;

  if (args.core >= 0) {
    std::string info;
    affinity::pin_to_core(args.core, &info);
    std::fprintf(stderr, "[pin] %s\n", info.c_str());
  }

  itch::Replay rep;
  std::string err;
  if (!rep.load_csv(args.replay, static_cast<size_t>(args.max_msgs), &err)) {
    std::fprintf(stderr, "replay load error: %s\n", err.c_str());
    return 3;
  }

  const size_t N = rep.events.size();
  lob::Lob book;
  sig::MM mm;
  risk::Risk rg; rg.configure(args.inv_cap, args.notional_cap, args.throttle);
  PnL pnl;

  std::vector<double>   mids; mids.reserve(N);
  std::vector<uint64_t> ts;   ts.reserve(N);

  timing::StageTimers st(N + 16);

  // Buffered CSV output via stdio with user buffer (created before guard)
  FILE* fout = std::fopen(args.results.c_str(), "wb");
  if (!fout) { std::perror("fopen(results)"); return 4; }
  const size_t BUF_SZ = 8ull * 1024ull * 1024ull; // 8MB
  char* outbuf = new char[BUF_SZ];
  std::setvbuf(fout, outbuf, _IOFBF, BUF_SZ);
  std::fputs("ts_ns,event,order_id,side,px,qty,inv_after,notional_after\n", fout);

  // histogram edges in microseconds
  std::vector<uint32_t> edges = {1,2,5,10,20,50,80,100,200,500,1000};
  histo::AllStageHistos H(edges);

  size_t processed = 0;
  bool guard_enabled = false;

  for (size_t i=0; i<N; ++i) {
    const auto& ev = rep.events[i];

    if (!guard_enabled && processed >= static_cast<size_t>(args.warmup)) {
      nomalloc::enable_guard();
      guard_enabled = true;
    }

    { timing::ScopedTimer T(st.parse); /* already parsed */ }

    { timing::ScopedTimer T(st.lob);
      if (ev.type == itch::EvType('A')) {
        book.add({ev.ts_ns, ev.order_id, ev.px, ev.qty, ev.side});
      } else if (ev.type == itch::EvType('C')) {
        book.cancel(ev.order_id); mm.on_cancel();
      } else { // Exec
        mm.on_exec();
        pnl.on_exec(ev.px, ev.qty, !ev.side);
        book.cancel(ev.order_id);
      }
    }

    const int bb = book.best_bid();
    const int aa = book.best_ask();
    if (bb != INT32_MIN && aa != INT32_MAX) {
      const int32_t mid = static_cast<int32_t>((bb + aa) / 2);
      mids.push_back(static_cast<double>(mid));
      ts.push_back(ev.ts_ns);
    }

    sig::Quote q{};
    { timing::ScopedTimer T(st.sig);
      if (args.mode == "avs" && mids.size() >= 64u) {
        const size_t M = mids.size();
        double dt_s = 1e-3;
        if (M > 1u) {
          dt_s = (static_cast<double>(ts[M-1] - ts[0]) / 1e9) / static_cast<double>(M-1);
          if (dt_s <= 0.0) dt_s = 1e-3;
        }
        const stoch::OuParams ou = stoch::fit_ou(mids, dt_s);
        const stoch::AvsParams avp{args.avs_gamma, args.avs_k, args.avs_horizon};
        const auto pxs = stoch::avellaneda_stoikov(mids.back(), pnl.inv, ou, avp);
        q = sig::Quote{pxs.bid_px, pxs.ask_px, 1, 1};
      } else {
        q = mm.quote(book, /*q_alpha=*/0.01, /*skew=*/2.0, pnl.inv, args.inv_cap);
      }
    }

    bool allowed = false;
    { timing::ScopedTimer T(st.risk);
      allowed = rg.allow(q, pnl.inv, args.inv_cap, args.notional_cap, ev.ts_ns);
    }

    { timing::ScopedTimer T(st.e2e);
      if (allowed) {
        write_line(fout, ev.ts_ns, static_cast<char>(ev.type), ev.order_id, ev.side,
                   q.bid_px, q.bid_qty, pnl.inv, pnl.pnl);
      }
    }

    ++processed;
  }

  if (guard_enabled) nomalloc::disable_guard();

  std::fflush(fout);
  std::fclose(fout);
  delete[] outbuf;

  timing::write_csv_latency(args.latency, st,
                            static_cast<size_t>(args.warmup),
                            processed);

  // Build histograms from samples post-warmup
  const size_t start = std::min(static_cast<size_t>(args.warmup), st.e2e.ns.size());
  const size_t end   = std::min(processed, st.e2e.ns.size());
  for (size_t i = start; i < end; ++i) {
    H.parse.add_ns(st.parse.ns[i]);
    H.lob.add_ns(st.lob.ns[i]);
    H.sig.add_ns(st.sig.ns[i]);
    H.risk.add_ns(st.risk.ns[i]);
    H.e2e.add_ns(st.e2e.ns[i]);
  }
  histo::write_csv(args.histo, H);

  const auto sum_e2e = timing::summarize(st.e2e.ns,
                                         static_cast<size_t>(args.warmup),
                                         processed);
  std::printf("End-to-end latency (post-warmup): p50=%.2f us p99=%.2f us\n",
              sum_e2e.p50_us, sum_e2e.p99_us);
  return 0;
}
