add_library(pulsar_application
  src/bootstrap.cpp
  src/commands/command_processor.cpp
  src/media/media_probe_error.cpp
  src/project/media_asset.cpp
  src/project/project_document.cpp
  src/playback/monotonic_clock.cpp
  src/playback/playback_time.cpp
  src/playback/playback_transport.cpp
  src/playback/playback_session.cpp
  src/playback/presentation_scheduler.cpp
  src/playback/video_source.cpp
)

add_library(pulsar::application ALIAS pulsar_application)

target_include_directories(pulsar_application
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(pulsar_application
  PUBLIC
    pulsar::domain
)

pulsar_apply_project_options(pulsar_application)
pulsar_apply_compiler_warnings(pulsar_application)
