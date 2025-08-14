#include "nomalloc.h"
#include <cstdlib>
#include <new>   // for sized delete signatures

namespace t2t::nomalloc {

std::atomic<bool> g_guard_enabled{false};

void enable_guard()  { g_guard_enabled.store(true,  std::memory_order_seq_cst); }
void disable_guard() { g_guard_enabled.store(false, std::memory_order_seq_cst); }

// Internal helpers that the global operators will call
void* operator_new(std::size_t sz) {
  if (g_guard_enabled.load(std::memory_order_relaxed)) {
    std::fprintf(stderr, "[nomalloc] allocation of %zu bytes detected in hot path\n", sz);
    std::abort();
  }
  return std::malloc(sz);
}

void operator_delete(void* p) noexcept {
  std::free(p);
}

} // namespace t2t::nomalloc

// -----------------------
// Global new/delete overrides (must be at global scope)
// -----------------------
void* operator new(std::size_t sz)               { return t2t::nomalloc::operator_new(sz); }
void* operator new[](std::size_t sz)             { return t2t::nomalloc::operator_new(sz); }

void  operator delete(void* p) noexcept          { t2t::nomalloc::operator_delete(p); }
void  operator delete[](void* p) noexcept        { t2t::nomalloc::operator_delete(p); }

// Sized delete (some libstdc++/libc++ paths call these)
void  operator delete(void* p, std::size_t) noexcept   { t2t::nomalloc::operator_delete(p); }
void  operator delete[](void* p, std::size_t) noexcept { t2t::nomalloc::operator_delete(p); }
