# Test cases use the standard assert-based contract throughout the suite.
# Keep those checks active even when the product is built in Release mode.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -UNDEBUG")
elseif(MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /UNDEBUG")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME pulsar.assets.core_sounds_content
         COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/test_core_sounds.py)

# Tests embed this path as a C string literal. Normalize it so Windows
# backslashes are not interpreted as escape sequences by the compiler.
file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}" PULSAR_SOURCE_DIR)

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

add_executable(pulsar_wide_integer_tests
  domain/wide_integer_test.cpp
)

target_link_libraries(pulsar_wide_integer_tests
  PRIVATE
    pulsar::domain
)

pulsar_apply_project_options(pulsar_wide_integer_tests)
pulsar_apply_compiler_warnings(pulsar_wide_integer_tests)

add_test(NAME pulsar.domain.wide_integer COMMAND pulsar_wide_integer_tests)

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

add_executable(pulsar_visual_compositor_tests application/visual_compositor_test.cpp)
target_link_libraries(pulsar_visual_compositor_tests PRIVATE pulsar::application)
pulsar_apply_project_options(pulsar_visual_compositor_tests)
pulsar_apply_compiler_warnings(pulsar_visual_compositor_tests)
add_test(NAME pulsar.application.visual_compositor COMMAND pulsar_visual_compositor_tests)

add_executable(pulsar_visual_transform_tests application/visual_transform_test.cpp)
target_link_libraries(pulsar_visual_transform_tests PRIVATE pulsar::application pulsar::domain)
pulsar_apply_project_options(pulsar_visual_transform_tests)
pulsar_apply_compiler_warnings(pulsar_visual_transform_tests)
add_test(NAME pulsar.application.visual_transform COMMAND pulsar_visual_transform_tests)

add_executable(pulsar_transition_tests application/transition_test.cpp)
target_link_libraries(pulsar_transition_tests PRIVATE pulsar::application pulsar::domain)
pulsar_apply_project_options(pulsar_transition_tests)
pulsar_apply_compiler_warnings(pulsar_transition_tests)
add_test(NAME pulsar.application.transitions COMMAND pulsar_transition_tests)

add_executable(pulsar_frame_coverage_parity_tests application/frame_coverage_parity_test.cpp)
target_link_libraries(pulsar_frame_coverage_parity_tests PRIVATE pulsar::application)
pulsar_apply_project_options(pulsar_frame_coverage_parity_tests)
pulsar_apply_compiler_warnings(pulsar_frame_coverage_parity_tests)
add_test(NAME pulsar.application.frame_coverage_parity COMMAND pulsar_frame_coverage_parity_tests)

add_executable(pulsar_composition_preview_tests application/composition_preview_test.cpp)
target_link_libraries(pulsar_composition_preview_tests PRIVATE pulsar::application)
pulsar_apply_project_options(pulsar_composition_preview_tests)
pulsar_apply_compiler_warnings(pulsar_composition_preview_tests)
add_test(NAME pulsar.application.composition_preview COMMAND pulsar_composition_preview_tests)

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

add_executable(pulsar_export_job_tests
  application/export_job_test.cpp
)

target_link_libraries(pulsar_export_job_tests
  PRIVATE
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_export_job_tests)
pulsar_apply_compiler_warnings(pulsar_export_job_tests)

add_test(NAME pulsar.application.export_job COMMAND pulsar_export_job_tests)

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
target_compile_definitions(pulsar_project_persistence_tests PRIVATE PULSAR_SOURCE_DIR="${PULSAR_SOURCE_DIR}")

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
target_compile_definitions(pulsar_audio_model_tests PRIVATE PULSAR_SOURCE_DIR="${PULSAR_SOURCE_DIR}")

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

add_executable(pulsar_distant_music_processor_tests
  application/distant_music_processor_test.cpp
)
target_link_libraries(pulsar_distant_music_processor_tests PRIVATE pulsar::application pulsar::domain)
pulsar_apply_project_options(pulsar_distant_music_processor_tests)
pulsar_apply_compiler_warnings(pulsar_distant_music_processor_tests)
add_test(NAME pulsar.application.distant_music_processor COMMAND pulsar_distant_music_processor_tests)

add_executable(pulsar_pulse_map_tests
  application/pulse_map_test.cpp
)
target_link_libraries(pulsar_pulse_map_tests PRIVATE pulsar::application pulsar::domain)
pulsar_apply_project_options(pulsar_pulse_map_tests)
pulsar_apply_compiler_warnings(pulsar_pulse_map_tests)
add_test(NAME pulsar.application.pulse_map COMMAND pulsar_pulse_map_tests)

add_executable(pulsar_audio_recipe_tests
  application/audio_recipe_test.cpp
)
target_link_libraries(pulsar_audio_recipe_tests PRIVATE pulsar::application pulsar::domain)
pulsar_apply_project_options(pulsar_audio_recipe_tests)
pulsar_apply_compiler_warnings(pulsar_audio_recipe_tests)
add_test(NAME pulsar.application.audio_recipe COMMAND pulsar_audio_recipe_tests)

add_executable(pulsar_scene_cards_tests
  application/scene_cards_test.cpp
)
target_link_libraries(pulsar_scene_cards_tests PRIVATE pulsar::application pulsar::domain)
pulsar_apply_project_options(pulsar_scene_cards_tests)
pulsar_apply_compiler_warnings(pulsar_scene_cards_tests)
add_test(NAME pulsar.application.scene_cards COMMAND pulsar_scene_cards_tests)

add_executable(pulsar_srt_tests
  application/srt_test.cpp
)
target_link_libraries(pulsar_srt_tests PRIVATE pulsar::application pulsar::domain)
pulsar_apply_project_options(pulsar_srt_tests)
pulsar_apply_compiler_warnings(pulsar_srt_tests)
add_test(NAME pulsar.application.srt COMMAND pulsar_srt_tests)

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

# Integration tests need the ffmpeg command-line tool built with libx264.
# The vcpkg ffmpeg package ships libraries only (no CLI, no x264), so the CI
# installs a separate CLI and passes its path explicitly. Accept a cache
# override, then fall back to a PATH search with an actionable error.
set(PULSAR_TEST_FFMPEG_CLI "" CACHE FILEPATH
    "Path to an ffmpeg executable with libx264 support for media integration tests")

if(NOT PULSAR_TEST_FFMPEG_CLI)
  # find_program() does not search when the cache entry already exists, even
  # when its value is empty, so drop the placeholder first.
  unset(PULSAR_TEST_FFMPEG_CLI CACHE)
  find_program(PULSAR_TEST_FFMPEG_CLI NAMES ffmpeg ffmpeg.exe)
endif()

if(NOT PULSAR_TEST_FFMPEG_CLI)
  message(FATAL_ERROR
    "PULSAR_TEST_FFMPEG_CLI was not found. Media integration tests require the "
    "ffmpeg command-line tool with libx264 (the vcpkg port provides libraries, "
    "not the CLI). Install ffmpeg or configure with "
    "-DPULSAR_TEST_FFMPEG_CLI=/path/to/ffmpeg.")
endif()

if(NOT EXISTS "${PULSAR_TEST_FFMPEG_CLI}")
  message(FATAL_ERROR
    "PULSAR_TEST_FFMPEG_CLI='${PULSAR_TEST_FFMPEG_CLI}' does not exist.")
endif()

# The path is embedded in a C string literal, so backslashes must be avoided.
file(TO_CMAKE_PATH "${PULSAR_TEST_FFMPEG_CLI}" PULSAR_TEST_FFMPEG_CLI)

add_executable(pulsar_ffmpeg_offline_video_renderer_tests
  media/ffmpeg_offline_video_renderer_test.cpp
)

target_compile_definitions(pulsar_ffmpeg_offline_video_renderer_tests
  PRIVATE
    PULSAR_SOURCE_DIR="${PULSAR_SOURCE_DIR}"
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
    PULSAR_SOURCE_DIR="${PULSAR_SOURCE_DIR}"
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

add_executable(pulsar_ffmpeg_export_media_writer_tests
  media/ffmpeg_export_media_writer_test.cpp
)

target_link_libraries(pulsar_ffmpeg_export_media_writer_tests
  PRIVATE
    pulsar::media_ffmpeg
    pulsar::ffmpeg_dependencies
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_ffmpeg_export_media_writer_tests)
pulsar_apply_compiler_warnings(pulsar_ffmpeg_export_media_writer_tests)

add_test(NAME pulsar.media.ffmpeg_export_media_writer
         COMMAND pulsar_ffmpeg_export_media_writer_tests)

add_executable(pulsar_ffmpeg_export_job_smoke_tests
  media/ffmpeg_export_job_smoke_test.cpp
)

target_compile_definitions(pulsar_ffmpeg_export_job_smoke_tests
  PRIVATE
    PULSAR_SOURCE_DIR="${PULSAR_SOURCE_DIR}"
)

target_link_libraries(pulsar_ffmpeg_export_job_smoke_tests
  PRIVATE
    pulsar::media_ffmpeg
    pulsar::project_file
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_ffmpeg_export_job_smoke_tests)
pulsar_apply_compiler_warnings(pulsar_ffmpeg_export_job_smoke_tests)

add_test(NAME pulsar.media.ffmpeg_export_job_smoke
         COMMAND pulsar_ffmpeg_export_job_smoke_tests)

add_executable(pulsar_ffmpeg_export_e2e_tests
  media/ffmpeg_export_e2e_test.cpp
)

target_compile_definitions(pulsar_ffmpeg_export_e2e_tests
  PRIVATE
    PULSAR_SOURCE_DIR="${PULSAR_SOURCE_DIR}"
)

target_link_libraries(pulsar_ffmpeg_export_e2e_tests
  PRIVATE
    pulsar::media_ffmpeg
    pulsar::ffmpeg_dependencies
    pulsar::project_file
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_ffmpeg_export_e2e_tests)
pulsar_apply_compiler_warnings(pulsar_ffmpeg_export_e2e_tests)

add_test(NAME pulsar.media.ffmpeg_export_e2e
         COMMAND pulsar_ffmpeg_export_e2e_tests)

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
    PULSAR_SOURCE_DIR="${PULSAR_SOURCE_DIR}"
)

target_link_libraries(pulsar_ffmpeg_audio_decode_session_tests
  PRIVATE
    pulsar::media_ffmpeg
    pulsar::ffmpeg_dependencies
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
  PRIVATE PULSAR_SOURCE_DIR="${PULSAR_SOURCE_DIR}"
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
    pulsar::ffmpeg_dependencies
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

add_executable(pulsar_title_tests
  application/title_test.cpp
)
target_link_libraries(pulsar_title_tests PRIVATE pulsar::application pulsar::domain)
pulsar_apply_project_options(pulsar_title_tests)
pulsar_apply_compiler_warnings(pulsar_title_tests)
add_test(NAME pulsar.application.titles COMMAND pulsar_title_tests)

add_executable(pulsar_still_tests
  application/still_media_test.cpp
)
target_link_libraries(pulsar_still_tests PRIVATE pulsar::application pulsar::domain)
pulsar_apply_project_options(pulsar_still_tests)
pulsar_apply_compiler_warnings(pulsar_still_tests)
add_test(NAME pulsar.application.still_media COMMAND pulsar_still_tests)

add_executable(pulsar_ffmpeg_still_tests
  media/ffmpeg_still_test.cpp
)
target_compile_definitions(pulsar_ffmpeg_still_tests PRIVATE PULSAR_SOURCE_DIR="${PULSAR_SOURCE_DIR}")
target_link_libraries(pulsar_ffmpeg_still_tests PRIVATE pulsar::media_ffmpeg pulsar::application pulsar::domain)
pulsar_apply_project_options(pulsar_ffmpeg_still_tests)
pulsar_apply_compiler_warnings(pulsar_ffmpeg_still_tests)
add_test(NAME pulsar.media.ffmpeg_still COMMAND pulsar_ffmpeg_still_tests)
