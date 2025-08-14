#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace t2t::itch {

// Event type: Add / Cancel / Exec (synthetic ITCH-like)
enum class EvType : uint8_t { Add = 'A', Cancel = 'C', Exec = 'E' };

// One normalized event row.
struct Event {
  uint64_t ts_ns;    // nanoseconds since start (from CSV)
  EvType   type;     // 'A'/'C'/'E'
  uint32_t order_id; // synthetic id
  bool     side;     // true=buy, false=sell
  int32_t  px;       // integer ticks
  int32_t  qty;      // quantity (positive)
};

// A deterministic, allocation-light replay loader (CSV → std::vector<Event>).
struct Replay {
  std::vector<Event> events; // pre-parsed rows

  // Parse a CSV with header: ts_ns,type,order_id,side,px,qty
  // - side accepts: 1/0, B/S, b/s
  // - type accepts: A/C/E
  // - max_msgs==0 means no hard cap
  // Returns true on success; false fills *err with message.
  bool load_csv(const std::string& path, std::size_t max_msgs, std::string* err);
};

// Utility: write a byte buffer to a file (used later for encoded outputs).
bool write_output_csv(const std::string& path, const std::vector<char>& buf);

} // namespace t2t::itch
