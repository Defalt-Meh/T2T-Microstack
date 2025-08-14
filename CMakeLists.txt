cmake_minimum_required(VERSION 3.20)
project(t2t_microstack LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Strict, fast builds
if (MSVC)
  add_compile_options(/permissive- /W4 /WX)
else()
  add_compile_options(-O3 -DNDEBUG -march=native -Wall -Wextra -Wshadow -Wconversion -Wpedantic -Werror)
  add_compile_options(-fno-exceptions -fno-rtti)
endif()

# Header search paths (libs also expose their own includes below)
include_directories(
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/libutil
  ${CMAKE_SOURCE_DIR}/libring
  ${CMAKE_SOURCE_DIR}/libitch
  ${CMAKE_SOURCE_DIR}/liblob
  ${CMAKE_SOURCE_DIR}/libsig
  ${CMAKE_SOURCE_DIR}/librisk
  ${CMAKE_SOURCE_DIR}/libstoch
)

# ---------- Libraries ----------
# libutil
add_library(util STATIC
  libutil/timing.cpp
  libutil/histo.cpp
  libutil/affinity.cpp
  libutil/nomalloc.cpp
)
target_include_directories(util PUBLIC libutil)

# libitch
add_library(itch STATIC
  libitch/itch.cpp
)
target_include_directories(itch PUBLIC libitch)

# liblob
add_library(lob STATIC
  liblob/lob.cpp
)
target_include_directories(lob PUBLIC liblob)

# libstoch
add_library(stoch STATIC
  libstoch/ou.cpp
  libstoch/avs.cpp
)
target_include_directories(stoch PUBLIC libstoch)

# ---------- Apps ----------
add_executable(t2t_main
  apps/t2t_main.cpp
)
target_link_libraries(t2t_main PRIVATE util itch lob stoch)

# ---------- Tests ----------
add_executable(unit_tests
  tests/test_main.cpp
  tests/ring_test.cpp
  tests/itch_test.cpp
  tests/lob_test.cpp
  tests/sig_risk_test.cpp
  tests/stoch_test.cpp
  tests/determinism_test.cpp  
)
target_link_libraries(unit_tests PRIVATE util itch lob stoch)

enable_testing()
add_test(NAME unit_tests COMMAND unit_tests)
