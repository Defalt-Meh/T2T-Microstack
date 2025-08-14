#include "tests/test_util.h"
#include "libring/spsc_ring.hpp"
#include <cstdint>

using t2t::ring::SpscRing;

// Define with external linkage (no 'extern' keyword on the definition)
void run_ring_tests() {
  struct Item { uint32_t v; };
  SpscRing<Item> r(1024);

  // steady push/pop
  for (uint32_t i=0;i<10000;++i) {
    T2T_CHECK(r.try_push(Item{i}));
    Item out{};
    T2T_CHECK(r.try_pop(out));
    T2T_CHECK(out.v == i);
  }

  // wrap-around & drain
  for (uint32_t i=0;i<1000;++i) T2T_CHECK(r.try_push(Item{i}));
  Item tmp{};
  for (uint32_t i=0;i<1000;++i) T2T_CHECK(r.try_pop(tmp));

  // capacity
  T2T_CHECK(r.capacity() == 1024);
}
