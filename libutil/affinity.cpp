#include "affinity.h"
#include <string>
#include <thread>

#if defined(__linux__)
  #include <sched.h>
  #include <unistd.h>
#elif defined(__APPLE__)
  #include <pthread.h>
  #include <sys/sysctl.h>
#endif

namespace t2t::affinity {

bool pin_to_core(int core_id, std::string* info_out) {
#if defined(__linux__)
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  int rc = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
  if (info_out) *info_out = "linux core " + std::to_string(core_id) + (rc==0? " pinned" : " pin-failed");
  return rc == 0;
#elif defined(__APPLE__)
  // macOS doesn't expose strict pinning; we return success with an info note.
  if (info_out) *info_out = "macOS: strict core pinning unsupported (best-effort)";
  (void)core_id;
  return true;
#else
  if (info_out) *info_out = "pin not supported on this OS";
  (void)core_id;
  return false;
#endif
}

int current_cpu() {
#if defined(__linux__)
  return sched_getcpu();
#else
  return -1;
#endif
}

} // namespace t2t::affinity
