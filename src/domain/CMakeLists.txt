add_library(pulsar_domain
  src/build_info.cpp
  src/time/frame_number.cpp
  src/time/frame_rate.cpp
  src/time/media_time.cpp
  src/time/rational.cpp
  src/time/time_range.cpp
  src/timeline/clip.cpp
  src/timeline/title.cpp
  src/timeline/timeline.cpp
  src/timeline/track.cpp
)

add_library(pulsar::domain ALIAS pulsar_domain)

# The product version is injected into exactly one translation unit.
set_source_files_properties(src/build_info.cpp PROPERTIES
  COMPILE_DEFINITIONS "PULSAR_VERSION_STRING=\"${PULSAR_VERSION_STRING}\"")

target_include_directories(pulsar_domain
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

pulsar_apply_project_options(pulsar_domain)
pulsar_apply_compiler_warnings(pulsar_domain)
