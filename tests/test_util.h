#pragma once
#include <cstdio>

extern int g_failures;

// Print cleanly to stderr without crashing the process
#define T2T_CHECK(cond) do {                                      \
  if (!(cond)) {                                                  \
    std::fprintf(::stderr, "CHECK FAILED: %s @ %s:%d\n",          \
                 #cond, __FILE__, __LINE__);                      \
    ++g_failures;                                                 \
  }                                                               \
} while(0)
