#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <cstdio>

namespace t2t::itch {

enum class EvType : uint8_t { Add='A', Cancel='C', Exec='E' };

struct Event {
  uint64_t ts_ns;
  EvType   type;
  uint32_t order_id;
  bool     side;   // true=buy, false=sell
  int32_t  px;
  int32_t  qty;
};

struct Replay {
  // Memory-mapped or file-loaded CSV for deterministic parsing without allocation
  std::vector<Event> events; // pre-parsed
  bool load_csv(const std::string& path, size_t max_msgs, std::string* err);
};

bool write_output_csv(const std::string& path, const std::vector<char>& buf);

} // namespace t2t::itch
