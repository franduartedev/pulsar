# Test cases use the standard assert-based contract throughout the suite.
# Keep those checks active even when the product is built in Release mode.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -UNDEBUG")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME pulsar.assets.core_sounds_content
         COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/test_core_sounds.py)

add_executable(pulsar_smoke_tests
  smoke/bootstrap_test.cpp
)

target_link_libraries(pulsar_smoke_tests
  PRIVATE
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_smoke_tests)
pulsar_apply_compiler_warnings(pulsar_smoke_tests)

add_test(NAME pulsar.bootstrap COMMAND pulsar_smoke_tests)

add_executable(pulsar_time_tests
  domain/time_model_test.cpp
)

target_link_libraries(pulsar_time_tests
  PRIVATE
    pulsar::domain
)

pulsar_apply_project_options(pulsar_time_tests)
pulsar_apply_compiler_warnings(pulsar_time_tests)

add_test(NAME pulsar.domain.time_model COMMAND pulsar_time_tests)

add_executable(pulsar_timeline_tests
  domain/timeline_model_test.cpp
)

target_link_libraries(pulsar_timeline_tests
  PRIVATE
    pulsar::domain
)

pulsar_apply_project_options(pulsar_timeline_tests)
pulsar_apply_compiler_warnings(pulsar_timeline_tests)

add_test(NAME pulsar.domain.timeline_model COMMAND pulsar_timeline_tests)

add_executable(pulsar_command_history_tests
  application/command_history_test.cpp
)

target_link_libraries(pulsar_command_history_tests
  PRIVATE
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_command_history_tests)
pulsar_apply_compiler_warnings(pulsar_command_history_tests)

add_test(NAME pulsar.application.command_history COMMAND pulsar_command_history_tests)

add_executable(pulsar_project_document_tests
  application/project_document_test.cpp
)

target_link_libraries(pulsar_project_document_tests
  PRIVATE
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_project_document_tests)
pulsar_apply_compiler_warnings(pulsar_project_document_tests)

add_test(NAME pulsar.application.project_document COMMAND pulsar_project_document_tests)

add_executable(pulsar_timeline_clip_resolver_tests
  application/timeline_clip_resolver_test.cpp
)

target_link_libraries(pulsar_timeline_clip_resolver_tests
  PRIVATE
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_timeline_clip_resolver_tests)
pulsar_apply_compiler_warnings(pulsar_timeline_clip_resolver_tests)

add_test(NAME pulsar.application.timeline_clip_resolver COMMAND pulsar_timeline_clip_resolver_tests)

add_executable(pulsar_export_plan_tests
  application/export_plan_test.cpp
)

target_link_libraries(pulsar_export_plan_tests
  PRIVATE
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_export_plan_tests)
pulsar_apply_compiler_warnings(pulsar_export_plan_tests)

add_test(NAME pulsar.application.export_plan COMMAND pulsar_export_plan_tests)

add_executable(pulsar_offline_video_renderer_tests
  application/offline_video_renderer_test.cpp
)

target_link_libraries(pulsar_offline_video_renderer_tests
  PRIVATE
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_offline_video_renderer_tests)
pulsar_apply_compiler_warnings(pulsar_offline_video_renderer_tests)

add_test(NAME pulsar.application.offline_video_renderer COMMAND pulsar_offline_video_renderer_tests)

add_executable(pulsar_offline_audio_renderer_tests
  application/offline_audio_renderer_test.cpp
)

target_link_libraries(pulsar_offline_audio_renderer_tests
  PRIVATE
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_offline_audio_renderer_tests)
pulsar_apply_compiler_warnings(pulsar_offline_audio_renderer_tests)

add_test(NAME pulsar.application.offline_audio_renderer COMMAND pulsar_offline_audio_renderer_tests)

add_executable(pulsar_timeline_playback_coordinator_tests
  application/timeline_playback_coordinator_test.cpp
)

target_link_libraries(pulsar_timeline_playback_coordinator_tests
  PRIVATE
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_timeline_playback_coordinator_tests)
pulsar_apply_compiler_warnings(pulsar_timeline_playback_coordinator_tests)

add_test(NAME pulsar.application.timeline_playback_coordinator
         COMMAND pulsar_timeline_playback_coordinator_tests)


add_executable(pulsar_playback_transport_tests
  application/playback_transport_test.cpp
)

target_link_libraries(pulsar_playback_transport_tests
  PRIVATE
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_playback_transport_tests)
pulsar_apply_compiler_warnings(pulsar_playback_transport_tests)

add_test(NAME pulsar.application.playback_transport COMMAND pulsar_playback_transport_tests)

add_executable(pulsar_presentation_scheduler_tests
  application/presentation_scheduler_test.cpp
)

target_link_libraries(pulsar_presentation_scheduler_tests
  PRIVATE
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_presentation_scheduler_tests)
pulsar_apply_compiler_warnings(pulsar_presentation_scheduler_tests)

add_test(NAME pulsar.application.presentation_scheduler COMMAND pulsar_presentation_scheduler_tests)

add_executable(pulsar_playback_session_tests
  application/playback_session_test.cpp
)

target_link_libraries(pulsar_playback_session_tests
  PRIVATE
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_playback_session_tests)
pulsar_apply_compiler_warnings(pulsar_playback_session_tests)

add_test(NAME pulsar.application.playback_session COMMAND pulsar_playback_session_tests)

add_executable(pulsar_project_persistence_tests
  application/project_persistence_test.cpp
)

target_link_libraries(pulsar_project_persistence_tests
  PRIVATE
    pulsar::project_file
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_project_persistence_tests)
pulsar_apply_compiler_warnings(pulsar_project_persistence_tests)
target_compile_definitions(pulsar_project_persistence_tests PRIVATE PULSAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

add_test(NAME pulsar.application.project_persistence COMMAND pulsar_project_persistence_tests)

add_executable(pulsar_audio_model_tests
  application/audio_model_test.cpp
)

target_link_libraries(pulsar_audio_model_tests
  PRIVATE
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_audio_model_tests)
pulsar_apply_compiler_warnings(pulsar_audio_model_tests)
target_compile_definitions(pulsar_audio_model_tests PRIVATE PULSAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

add_test(NAME pulsar.application.audio_model COMMAND pulsar_audio_model_tests)

add_executable(pulsar_waveform_cache_tests
  application/waveform_cache_test.cpp
)

target_link_libraries(pulsar_waveform_cache_tests
  PRIVATE
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_waveform_cache_tests)
pulsar_apply_compiler_warnings(pulsar_waveform_cache_tests)

add_test(NAME pulsar.application.waveform_cache COMMAND pulsar_waveform_cache_tests)

add_executable(pulsar_waveform_renderer_tests
  application/waveform_renderer_test.cpp
)
target_link_libraries(pulsar_waveform_renderer_tests PRIVATE pulsar::application pulsar::domain)
pulsar_apply_project_options(pulsar_waveform_renderer_tests)
pulsar_apply_compiler_warnings(pulsar_waveform_renderer_tests)
add_test(NAME pulsar.application.waveform_renderer COMMAND pulsar_waveform_renderer_tests)

add_executable(pulsar_audio_output_engine_tests
  application/audio_output_engine_test.cpp
)

target_link_libraries(pulsar_audio_output_engine_tests
  PRIVATE
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_audio_output_engine_tests)
pulsar_apply_compiler_warnings(pulsar_audio_output_engine_tests)

add_test(NAME pulsar.application.audio_output_engine COMMAND pulsar_audio_output_engine_tests)

add_executable(pulsar_audio_playback_session_tests
  application/audio_playback_session_test.cpp
)

target_link_libraries(pulsar_audio_playback_session_tests
  PRIVATE
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_audio_playback_session_tests)
pulsar_apply_compiler_warnings(pulsar_audio_playback_session_tests)

add_test(NAME pulsar.application.audio_playback_session
         COMMAND pulsar_audio_playback_session_tests)

add_executable(pulsar_audio_mix_session_tests
  application/audio_mix_session_test.cpp
)

target_link_libraries(pulsar_audio_mix_session_tests
  PRIVATE
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_audio_mix_session_tests)
pulsar_apply_compiler_warnings(pulsar_audio_mix_session_tests)

add_test(NAME pulsar.application.audio_mix_session
         COMMAND pulsar_audio_mix_session_tests)

add_executable(pulsar_editor_session_tests
  application/editor_session_test.cpp
  ${CMAKE_SOURCE_DIR}/apps/pulsar-desktop/src/editor_session.cpp
)
target_include_directories(pulsar_editor_session_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/apps/pulsar-desktop/src
)
target_link_libraries(pulsar_editor_session_tests PRIVATE pulsar::application pulsar::domain)
pulsar_apply_project_options(pulsar_editor_session_tests)
pulsar_apply_compiler_warnings(pulsar_editor_session_tests)
add_test(NAME pulsar.desktop.editor_session COMMAND pulsar_editor_session_tests)

if(NOT PULSAR_BUILD_FFMPEG)
  return()
endif()

find_program(PULSAR_TEST_FFMPEG_CLI ffmpeg REQUIRED)

add_executable(pulsar_ffmpeg_offline_video_renderer_tests
  media/ffmpeg_offline_video_renderer_test.cpp
)

target_compile_definitions(pulsar_ffmpeg_offline_video_renderer_tests
  PRIVATE
    PULSAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    PULSAR_TEST_FFMPEG_CLI="${PULSAR_TEST_FFMPEG_CLI}"
)

target_link_libraries(pulsar_ffmpeg_offline_video_renderer_tests
  PRIVATE
    pulsar::media_ffmpeg
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_ffmpeg_offline_video_renderer_tests)
pulsar_apply_compiler_warnings(pulsar_ffmpeg_offline_video_renderer_tests)

add_test(NAME pulsar.media.ffmpeg_offline_video_renderer
         COMMAND pulsar_ffmpeg_offline_video_renderer_tests)

add_executable(pulsar_ffmpeg_offline_audio_renderer_tests
  media/ffmpeg_offline_audio_renderer_test.cpp
)

target_compile_definitions(pulsar_ffmpeg_offline_audio_renderer_tests
  PRIVATE
    PULSAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(pulsar_ffmpeg_offline_audio_renderer_tests
  PRIVATE
    pulsar::media_ffmpeg
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_ffmpeg_offline_audio_renderer_tests)
pulsar_apply_compiler_warnings(pulsar_ffmpeg_offline_audio_renderer_tests)

add_test(NAME pulsar.media.ffmpeg_offline_audio_renderer
         COMMAND pulsar_ffmpeg_offline_audio_renderer_tests)

add_executable(pulsar_ffmpeg_probe_tests
  media/ffmpeg_probe_test.cpp
)

target_link_libraries(pulsar_ffmpeg_probe_tests
  PRIVATE
    pulsar::media_ffmpeg
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_ffmpeg_probe_tests)
pulsar_apply_compiler_warnings(pulsar_ffmpeg_probe_tests)

add_test(NAME pulsar.media.ffmpeg_probe COMMAND pulsar_ffmpeg_probe_tests)

if(PULSAR_BUILD_QT_AUDIO)
  find_package(Qt6 REQUIRED COMPONENTS Core Multimedia)

  add_executable(pulsar_qt_audio_output_tests
    media/qt_audio_output_test.cpp
  )

  set_target_properties(pulsar_qt_audio_output_tests PROPERTIES
    AUTOMOC ON
  )

  target_link_libraries(pulsar_qt_audio_output_tests
    PRIVATE
      pulsar::media_qt_audio
      pulsar::application
      pulsar::domain
      Qt6::Core
      Qt6::Multimedia
  )

  pulsar_apply_project_options(pulsar_qt_audio_output_tests)
  pulsar_apply_compiler_warnings(pulsar_qt_audio_output_tests)

  add_test(NAME pulsar.media.qt_audio_output COMMAND pulsar_qt_audio_output_tests)
  # Threading regression insurance: a lifecycle deadlock must fail via
  # timeout, never hang CI.
  set_tests_properties(pulsar.media.qt_audio_output PROPERTIES TIMEOUT 300)
endif()

add_executable(pulsar_ffmpeg_audio_decode_session_tests
  media/ffmpeg_audio_decode_session_test.cpp
)

target_compile_definitions(pulsar_ffmpeg_audio_decode_session_tests
  PRIVATE
    PULSAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(pulsar_ffmpeg_audio_decode_session_tests
  PRIVATE
    pulsar::media_ffmpeg
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_ffmpeg_audio_decode_session_tests)
pulsar_apply_compiler_warnings(pulsar_ffmpeg_audio_decode_session_tests)

add_test(NAME pulsar.media.ffmpeg_audio_decode_session
         COMMAND pulsar_ffmpeg_audio_decode_session_tests)

add_executable(pulsar_ffmpeg_waveform_source_tests
  media/ffmpeg_waveform_source_test.cpp
)
target_compile_definitions(pulsar_ffmpeg_waveform_source_tests
  PRIVATE PULSAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
target_link_libraries(pulsar_ffmpeg_waveform_source_tests
  PRIVATE pulsar::media_ffmpeg pulsar::application pulsar::domain
)
pulsar_apply_project_options(pulsar_ffmpeg_waveform_source_tests)
pulsar_apply_compiler_warnings(pulsar_ffmpeg_waveform_source_tests)
add_test(NAME pulsar.media.ffmpeg_waveform_source COMMAND pulsar_ffmpeg_waveform_source_tests)

add_executable(pulsar_ffmpeg_video_decode_session_tests
  media/ffmpeg_video_decode_session_test.cpp
)

target_compile_definitions(pulsar_ffmpeg_video_decode_session_tests
  PRIVATE
    PULSAR_TEST_FFMPEG_CLI="${PULSAR_TEST_FFMPEG_CLI}"
)

target_link_libraries(pulsar_ffmpeg_video_decode_session_tests
  PRIVATE
    pulsar::media_ffmpeg
    pulsar::domain
)

pulsar_apply_project_options(pulsar_ffmpeg_video_decode_session_tests)
pulsar_apply_compiler_warnings(pulsar_ffmpeg_video_decode_session_tests)

add_test(NAME pulsar.media.ffmpeg_video_decode_session
         COMMAND pulsar_ffmpeg_video_decode_session_tests)

add_executable(pulsar_playback_diagnostics_tests
  media/playback_diagnostics_test.cpp
)

target_compile_definitions(pulsar_playback_diagnostics_tests
  PRIVATE
    PULSAR_TEST_FFMPEG_CLI="${PULSAR_TEST_FFMPEG_CLI}"
)

target_link_libraries(pulsar_playback_diagnostics_tests
  PRIVATE
    pulsar::media_ffmpeg
    pulsar::domain
)

pulsar_apply_project_options(pulsar_playback_diagnostics_tests)
pulsar_apply_compiler_warnings(pulsar_playback_diagnostics_tests)

add_test(NAME pulsar.media.playback_diagnostics
         COMMAND pulsar_playback_diagnostics_tests)
