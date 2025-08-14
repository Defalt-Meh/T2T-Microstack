#include "tests/test_util.h"
#include "libitch/itch.h"
#include "liblob/lob.h"
#include "libsig/mm.h"
#include "librisk/risk.h"
#include <string>
#include <vector>
#include <fstream>

using namespace t2t;

static std::string run_once(const std::string& path_csv) {
  itch::Replay rep; std::string err;
  bool ok = rep.load_csv(path_csv, /*max_msgs*/0, &err);
  T2T_CHECK(ok);

  lob::Lob book;
  sig::MM mm;
  risk::Risk rg; rg.configure(/*inv_cap*/100, /*notional*/1e12, /*throttle*/1000);

  struct PnL { int inv{0}; double pnl{0.0};
    void on_exec(int32_t px, int32_t qty, bool is_buy) {
      inv += is_buy ? qty : -qty;
      pnl += (is_buy ? -1.0 : 1.0) * static_cast<double>(px) * static_cast<double>(qty);
    }
  } pnl;

  std::string out; out.reserve(4096);
  out.append("ts_ns,event,order_id,side,px,qty,inv_after,notional_after\n");

  auto write_line = [&](uint64_t ts_ns, char ev, uint32_t oid, bool side,
                        int32_t px, int32_t qty, int inv_after, double notional_after) {
    char buf[128];
    int n = std::snprintf(buf, sizeof(buf), "%llu,%c,%u,%d,%d,%d,%d,%.6f\n",
                          (unsigned long long)ts_ns, ev, oid, side?1:0,
                          px, qty, inv_after, notional_after);
    if (n>0) out.append(buf, static_cast<size_t>(n));
  };

  for (const auto& ev : rep.events) {
    // LOB update (no timing; we only care about determinism here)
    if (ev.type == itch::EvType('A')) {
      book.add({ev.ts_ns, ev.order_id, ev.px, ev.qty, ev.side});
    } else if (ev.type == itch::EvType('C')) {
      book.cancel(ev.order_id); mm.on_cancel();
    } else {
      mm.on_exec();
      pnl.on_exec(ev.px, ev.qty, !ev.side);
      book.cancel(ev.order_id);
    }

    // Heuristic quote only (determinism independent of stochastic layer)
    auto q = mm.quote(book, /*q_alpha=*/0.01, /*skew=*/2.0, pnl.inv, /*inv_cap*/100);
    bool allowed = rg.allow(q, pnl.inv, /*inv_cap*/100, /*notional*/1e12, ev.ts_ns);
    if (allowed) {
      write_line(ev.ts_ns, static_cast<char>(ev.type), ev.order_id, ev.side,
                 q.bid_px, q.bid_qty, pnl.inv, pnl.pnl);
    }
  }
  return out;
}

void run_determinism_tests() {
  // Build a tiny deterministic feed
  const char* path = "/tmp/t2t_determinism.csv";
  {
    std::ofstream ofs(path);
    ofs << "ts_ns,type,order_id,side,px,qty\n";
    ofs << "1,A,1,1,100,2\n";
    ofs << "2,A,2,0,101,3\n";
    ofs << "3,E,1,1,100,1\n";
    ofs << "4,C,2,0,0,0\n";
    ofs << "5,A,3,1,101,1\n";
  }

  std::string o1 = run_once(path);
  std::string o2 = run_once(path);
  std::string o3 = run_once(path);

  T2T_CHECK(o1.size() == o2.size() && o2.size() == o3.size());
  T2T_CHECK(o1 == o2 && o2 == o3);
}
