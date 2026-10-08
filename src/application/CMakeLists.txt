add_library(pulsar_application
  src/bootstrap.cpp
  src/commands/command_processor.cpp
  src/audio/audio_converter.cpp
  src/audio/audio_contributor_resolver.cpp
  src/audio/audio_mix_session.cpp
  src/audio/clip_gain.cpp
  src/audio/audio_output_engine.cpp
  src/media/audio_block.cpp
  src/waveform/waveform_cache.cpp
  src/waveform/waveform_cache_manager.cpp
  src/waveform/waveform_renderer.cpp
  src/media/media_probe_error.cpp
  src/project/media_asset.cpp
  src/project/project_document.cpp
  src/project/project_codec.cpp
  src/timeline/timeline_clip_resolver.cpp
  src/composition/title_compositor.cpp
  src/export/export_plan.cpp
  src/export/export_job.cpp
  src/export/offline_video_renderer.cpp
  src/export/offline_audio_renderer.cpp
  src/playback/monotonic_clock.cpp
  src/playback/playback_time.cpp
  src/playback/playback_transport.cpp
  src/playback/playback_session.cpp
  src/playback/audio_playback_session.cpp
  src/playback/audio_mix_playback_session.cpp
  src/playback/timeline_playback_coordinator.cpp
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
