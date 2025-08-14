#include "tests/test_util.h"
#include "liblob/lob.h"
#include "libsig/mm.h"
#include "librisk/risk.h"

using namespace t2t;

extern void run_sig_risk_tests() {
  lob::Lob book;
  // seed a simple book
  book.add({10, 1, 100, 10, true});  // bid 100
  book.add({11, 2, 102, 10, false}); // ask 102

  sig::MM mm;
  auto q = mm.quote(book, 0.01, 2.0, /*inv=*/0, /*cap=*/100);
  T2T_CHECK(q.bid_px < q.ask_px);

  risk::Risk r; r.configure(/*inv_cap*/5, /*notional*/1e9, /*throttle*/3);
  // Allow at zero inventory
  bool ok1 = r.allow(q, /*inv=*/0, 5, 1e9, /*ts_ns=*/1'000'000);
  T2T_CHECK(ok1);

  // Exceed throttle in the same ms
  bool ok2 = r.allow(q, 0, 5, 1e9, 1'000'100);
  bool ok3 = r.allow(q, 0, 5, 1e9, 1'000'200);
  bool ok4 = r.allow(q, 0, 5, 1e9, 1'000'300);
  T2T_CHECK(ok2 && ok3 && !ok4); // 3rd accepted, 4th rejected (limit=3 per ms)

  // Inventory beyond cap blocks quoting on bid side
  bool deny_inv = r.allow(q, /*inv=*/6, 5, 1e9, 2'000'000);
  T2T_CHECK(!deny_inv);
}
