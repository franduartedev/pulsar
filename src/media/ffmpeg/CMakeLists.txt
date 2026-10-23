find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
  pkg_check_modules(
    FFMPEG
    QUIET
    IMPORTED_TARGET
      libavformat
      libavcodec
      libavutil
      libswscale
      libswresample
  )
endif()

if(FFMPEG_FOUND)
  set(PULSAR_FFMPEG_TARGET PkgConfig::FFMPEG)
else()
  # Windows/MSVC installations commonly expose FFmpeg through vcpkg or a
  # vendor SDK without a pkg-config file. Keep the Linux path above, but make
  # the dependency contract portable by resolving the same five libraries
  # from a conventional include/lib prefix.
  find_path(FFMPEG_INCLUDE_DIR libavformat/avformat.h REQUIRED)
  find_library(FFMPEG_AVFORMAT_LIBRARY NAMES avformat REQUIRED)
  find_library(FFMPEG_AVCODEC_LIBRARY NAMES avcodec REQUIRED)
  find_library(FFMPEG_AVUTIL_LIBRARY NAMES avutil REQUIRED)
  find_library(FFMPEG_SWSCALE_LIBRARY NAMES swscale REQUIRED)
  find_library(FFMPEG_SWRESAMPLE_LIBRARY NAMES swresample REQUIRED)

  add_library(pulsar_ffmpeg_dependencies INTERFACE)
  target_include_directories(pulsar_ffmpeg_dependencies INTERFACE "${FFMPEG_INCLUDE_DIR}")
  target_link_libraries(pulsar_ffmpeg_dependencies INTERFACE
    "${FFMPEG_AVFORMAT_LIBRARY}"
    "${FFMPEG_AVCODEC_LIBRARY}"
    "${FFMPEG_AVUTIL_LIBRARY}"
    "${FFMPEG_SWSCALE_LIBRARY}"
    "${FFMPEG_SWRESAMPLE_LIBRARY}"
  )
  set(PULSAR_FFMPEG_TARGET pulsar_ffmpeg_dependencies)
endif()

add_library(pulsar_media_ffmpeg
  src/ffmpeg_audio_decode_session.cpp
  src/ffmpeg_offline_audio_source.cpp
  src/ffmpeg_export_media_writer.cpp
  src/ffmpeg_audio_playback_source.cpp
  src/ffmpeg_waveform_source.cpp
  src/ffmpeg_video_decode_session.cpp
  src/ffmpeg_offline_video_source.cpp
  src/ffmpeg_offline_frame_scaler.cpp
  src/ffmpeg_media_probe.cpp
  src/ffmpeg_playback_worker.cpp
  src/ffmpeg_playback_visual_mailbox.cpp
  src/internal/ffmpeg_common.cpp
  src/media_decode_error.cpp
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
    ${PULSAR_FFMPEG_TARGET}
)

pulsar_apply_project_options(pulsar_media_ffmpeg)
pulsar_apply_compiler_warnings(pulsar_media_ffmpeg)
