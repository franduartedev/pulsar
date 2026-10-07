cmake_minimum_required(VERSION 3.20)

project(
  pulsar
  VERSION 0.1.0
  DESCRIPTION "Lightweight video editor built for fast, predictable editing"
  LANGUAGES CXX
)

# Single source of truth for the product version shown to users:
#   "<major>.<minor>.<patch>[-<label>]", e.g. 0.1.0-beta. Change it here only.
set(PULSAR_VERSION_LABEL "beta" CACHE STRING "Pre-release label appended to the version (empty for a stable release)")
if(PULSAR_VERSION_LABEL)
  set(PULSAR_VERSION_STRING "${PROJECT_VERSION}-${PULSAR_VERSION_LABEL}")
else()
  set(PULSAR_VERSION_STRING "${PROJECT_VERSION}")
endif()

option(PULSAR_BUILD_TESTS "Build PULSAR tests" ON)
option(PULSAR_BUILD_BENCHMARKS "Build PULSAR benchmark harness" OFF)
option(PULSAR_BUILD_FFMPEG "Build FFmpeg adapters, media tests, and Engine Lab media tooling" ON)
option(PULSAR_BUILD_DESKTOP "Build the Qt Quick PULSAR desktop media viewer" OFF)
option(PULSAR_BUILD_QT_AUDIO "Build the Qt Multimedia audio output adapter and smoke tool" OFF)
option(PULSAR_ENABLE_ASAN "Enable AddressSanitizer for supported compilers" OFF)
option(PULSAR_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for supported compilers" OFF)
option(PULSAR_ENABLE_CLANG_TIDY "Run clang-tidy during builds" OFF)
option(PULSAR_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# A desktop media build must exercise the real playback path.  Keeping the
# audio adapter optional for non-UI consumers is useful, but a desktop build
# without it is not a valid M10 smoke target.
if(PULSAR_BUILD_DESKTOP)
  set(PULSAR_BUILD_QT_AUDIO ON CACHE BOOL "Build Qt Multimedia for desktop playback" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(StaticAnalysis)
include(Sanitizers)
include(ProjectOptions)
include(CompilerWarnings)

if(PULSAR_BUILD_TESTS)
  enable_testing()
endif()

add_subdirectory(src/domain)
add_subdirectory(src/application)
add_subdirectory(src/project_file)
if(PULSAR_BUILD_FFMPEG)
  add_subdirectory(src/media/ffmpeg)
  add_subdirectory(apps/engine-lab)
endif()

if(PULSAR_BUILD_QT_AUDIO)
  add_subdirectory(src/media/qt_audio)
  add_subdirectory(apps/audio-smoke)
endif()

if(PULSAR_BUILD_TESTS)
  add_subdirectory(tests)
endif()

if(PULSAR_BUILD_DESKTOP)
  if(NOT PULSAR_BUILD_FFMPEG)
    message(FATAL_ERROR "PULSAR_BUILD_DESKTOP requires PULSAR_BUILD_FFMPEG for the M8 media viewer")
  endif()
  add_subdirectory(apps/pulsar-desktop)
endif()

if(PULSAR_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()
