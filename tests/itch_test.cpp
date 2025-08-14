#include "libitch/itch.h"
#include <fstream>
#include <string>

#ifndef T2T_CHECK
#define T2T_CHECK(x) do { if(!(x)) *(volatile int*)0=0; } while(0)
#endif

using namespace t2t::itch;

extern void run_itch_tests() {
  const char* path = "/tmp/t2t_itch_demo.csv";
  {
    std::ofstream ofs(path);
    ofs << "ts_ns,type,order_id,side,px,qty\n";
    ofs << "10,A,1,B,100,5\n";
    ofs << "20,C,1,1,0,0\n";
    ofs << "30,E,2,0,101,3\n";
  }
  Replay r;
  std::string err;
  bool ok = r.load_csv(path, /*max_msgs*/0, &err);
  T2T_CHECK(ok);
  T2T_CHECK(r.events.size() == 3);

  const auto& a = r.events[0];
  T2T_CHECK(a.ts_ns == 10);
  T2T_CHECK(a.type  == EvType('A'));
  T2T_CHECK(a.order_id == 1);
  T2T_CHECK(a.side == true);
  T2T_CHECK(a.px == 100);
  T2T_CHECK(a.qty == 5);

  const auto& c = r.events[1];
  T2T_CHECK(c.type == EvType('C'));
  T2T_CHECK(c.side == true);  // '1' -> buy

  const auto& e = r.events[2];
  T2T_CHECK(e.type == EvType('E'));
  T2T_CHECK(e.side == false); // '0'/'S' -> sell
}
