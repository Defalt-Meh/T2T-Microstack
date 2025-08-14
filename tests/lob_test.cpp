#include "tests/test_util.h"
#include "liblob/lob.h"
#include <climits>

using namespace t2t::lob;

extern void run_lob_tests() {
  Lob book;

  // Empty book
  T2T_CHECK(book.best_bid()==INT32_MIN);
  T2T_CHECK(book.best_ask()==INT32_MAX);

  // Add two bids, two asks
  book.add({10,  1, 100, 10, true});
  book.add({11,  2, 101,  5, true});
  book.add({12,  3, 103,  5, false});
  book.add({13,  4, 104, 10, false});
  T2T_CHECK(book.best_bid()==101);
  T2T_CHECK(book.best_ask()==103);

  // Cancel idempotent + remove existing
  book.cancel(999);
  book.cancel(3);
  T2T_CHECK(book.best_ask()==104);

  // Cross the book and match
  book.add({20,  5, 110, 5, true});  // strong bid
  Exec e{};
  bool x = book.match_top(e);
  T2T_CHECK(x);
  T2T_CHECK(e.qty>0);

  // Ensure cancelling again is safe
  book.cancel(3);
}
