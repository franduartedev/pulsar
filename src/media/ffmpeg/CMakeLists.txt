find_package(PkgConfig REQUIRED)

pkg_check_modules(
  FFMPEG
  REQUIRED
  IMPORTED_TARGET
    libavformat
    libavcodec
    libavutil
    libswscale
    libswresample
)

add_library(pulsar_media_ffmpeg
  src/ffmpeg_audio_decode_session.cpp
  src/ffmpeg_offline_audio_source.cpp
  src/ffmpeg_audio_playback_source.cpp
  src/ffmpeg_waveform_source.cpp
  src/ffmpeg_video_decode_session.cpp
  src/ffmpeg_offline_video_source.cpp
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
    PkgConfig::FFMPEG
)

pulsar_apply_project_options(pulsar_media_ffmpeg)
pulsar_apply_compiler_warnings(pulsar_media_ffmpeg)
