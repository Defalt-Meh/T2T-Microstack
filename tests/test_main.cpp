#include <cstdio>
#include "tests/test_util.h"

int g_failures = 0;

extern void run_ring_tests();
extern void run_itch_tests();
extern void run_lob_tests();
extern void run_sig_risk_tests();
extern void run_stoch_tests();
extern void run_determinism_tests();

int main() {
  run_ring_tests();
  run_itch_tests();
  run_lob_tests();
  run_sig_risk_tests();
  run_stoch_tests();
  run_determinism_tests(); 
  
  if (g_failures) {
    std::fprintf(::stderr, "unit tests: %d failure(s)\n", g_failures);
    return 1;
  }
  std::puts("unit tests: OK");
  return 0;
}
