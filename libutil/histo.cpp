#include "histo.h"
#include <fstream>

namespace t2t::histo {

static void write_one(std::ofstream& ofs, const char* name, const Histo& h) {
  for (size_t i = 0; i < h.edges_us.size(); ++i) {
    ofs << name << ',' << h.edges_us[i] << ',' << h.counts[i] << '\n';
  }
}

void write_csv(const std::string& path, const AllStageHistos& h) {
  std::ofstream ofs(path, std::ios::out | std::ios::trunc);
  ofs << "stage,bucket_us,count\n";
  write_one(ofs, "parse", h.parse);
  write_one(ofs, "lob",   h.lob);
  write_one(ofs, "sig",   h.sig);
  write_one(ofs, "risk",  h.risk);
  write_one(ofs, "e2e",   h.e2e);
}

} // namespace t2t::histo
