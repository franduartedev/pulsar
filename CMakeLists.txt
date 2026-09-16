cmake_minimum_required(VERSION 3.20)

project(
  pulsar
  VERSION 0.1.0
  DESCRIPTION "Linux-first video editor foundation"
  LANGUAGES CXX
)

option(PULSAR_BUILD_TESTS "Build PULSAR tests" ON)
option(PULSAR_BUILD_BENCHMARKS "Build PULSAR benchmark harness" OFF)
option(PULSAR_ENABLE_ASAN "Enable AddressSanitizer for supported compilers" OFF)
option(PULSAR_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for supported compilers" OFF)
option(PULSAR_ENABLE_CLANG_TIDY "Run clang-tidy during builds" OFF)
option(PULSAR_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(StaticAnalysis)
include(Sanitizers)
include(ProjectOptions)
include(CompilerWarnings)

add_subdirectory(src/domain)
add_subdirectory(src/application)
add_subdirectory(src/media/ffmpeg)
add_subdirectory(apps/engine-lab)

if(PULSAR_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(PULSAR_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()
