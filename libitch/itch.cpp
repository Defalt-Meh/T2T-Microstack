#include "itch.h"
#include <fstream>
#include <charconv>
#include <string>

namespace t2t::itch {

static inline bool parse_bool01bs(const char* p, std::size_t len, bool& out) {
  if (len==1 && (p[0]=='0' || p[0]=='1')) { out = (p[0]=='1'); return true; }
  if (len==1 && (p[0]=='B' || p[0]=='b')) { out = true;  return true; }
  if (len==1 && (p[0]=='S' || p[0]=='s')) { out = false; return true; }
  return false;
}

static inline bool parse_u64(const char* p, std::size_t len, uint64_t& out) {
  auto res = std::from_chars(p, p+len, out);
  return res.ec == std::errc();
}
static inline bool parse_u32(const char* p, std::size_t len, uint32_t& out) {
  auto res = std::from_chars(p, p+len, out);
  return res.ec == std::errc();
}
static inline bool parse_i32(const char* p, std::size_t len, int32_t& out) {
  auto res = std::from_chars(p, p+len, out);
  return res.ec == std::errc();
}

bool Replay::load_csv(const std::string& path, std::size_t max_msgs, std::string* err) {
  std::ifstream ifs(path);
  if (!ifs) { if (err) *err = "cannot open: " + path; return false; }

  events.clear();
  events.reserve(max_msgs ? max_msgs : 1'000'000);

  std::string line;
  // Peek first line; if it doesn't start with header, rewind.
  if (std::getline(ifs, line)) {
    if (line.rfind("ts_ns,", 0) != 0) {
      ifs.clear();
      ifs.seekg(0);
    }
  } else {
    return true;
  }

  while (std::getline(ifs, line)) {
    if (max_msgs && events.size() >= max_msgs) break;

    const char* s = line.c_str();
    const char* e = s + line.size();
    const char* p = s;
    int col = 0;
    Event ev{};

    while (p <= e) {
      const char* q = p;
      while (q < e && *q != ',') ++q;
      std::size_t len = static_cast<std::size_t>(q - p);

      switch (col) {
        case 0: if (!parse_u64(p, len, ev.ts_ns))                goto bad; break;
        case 1: ev.type = static_cast<EvType>(len ? p[0] : 'A');        break;
        case 2: if (!parse_u32(p, len, ev.order_id))             goto bad; break;
        case 3: if (!parse_bool01bs(p, len, ev.side))            goto bad; break;
        case 4: if (!parse_i32(p, len, ev.px))                   goto bad; break;
        case 5: if (!parse_i32(p, len, ev.qty))                  goto bad; break;
        default: break;
      }
      ++col;
      p = (q < e) ? (q + 1) : (e + 1);
    }

    if (col < 6) goto bad;
    events.push_back(ev);
    continue;

  bad:
    if (err) *err = "parse error: " + line;
    return false;
  }
  return true;
}

bool write_output_csv(const std::string& path, const std::vector<char>& buf) {
  std::ofstream ofs(path, std::ios::out | std::ios::trunc | std::ios::binary);
  if (!ofs) return false;
  ofs.write(buf.data(), static_cast<std::streamsize>(buf.size()));
  return true;
}

} // namespace t2t::itch
