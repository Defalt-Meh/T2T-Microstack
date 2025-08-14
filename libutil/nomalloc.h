#pragma once
#include <atomic>
#include <new>
#include <cstdio>

namespace t2t::nomalloc {

extern std::atomic<bool> g_guard_enabled;

// Call before warm-up finishes.
void enable_guard();
// Call after hot-path finishes.
void disable_guard();

// Abort immediately on allocation if guard is enabled.
void* operator_new(std::size_t sz);
void  operator_delete(void* p) noexcept;

} // namespace t2t::nomalloc
