find_package(PkgConfig REQUIRED)

pkg_check_modules(
  FFMPEG
  REQUIRED
  IMPORTED_TARGET
    libavformat
    libavcodec
    libavutil
)

add_library(pulsar_media_ffmpeg
  src/ffmpeg_media_probe.cpp
)

add_library(pulsar::media_ffmpeg ALIAS pulsar_media_ffmpeg)

target_include_directories(pulsar_media_ffmpeg
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(pulsar_media_ffmpeg
  PUBLIC
    pulsar::application
  PRIVATE
    PkgConfig::FFMPEG
)

pulsar_apply_project_options(pulsar_media_ffmpeg)
pulsar_apply_compiler_warnings(pulsar_media_ffmpeg)
