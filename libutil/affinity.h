#pragma once
#include <string>

namespace t2t::affinity {
// Pin the current thread to a specific core (Linux: real pin, macOS: best-effort).
bool pin_to_core(int core_id, std::string* info_out);
// Logical CPU id, or -1 if unknown.
int  current_cpu();
}
