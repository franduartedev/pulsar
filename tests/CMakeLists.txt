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

if(NOT PULSAR_BUILD_FFMPEG)
  return()
endif()

find_program(PULSAR_TEST_FFMPEG_CLI ffmpeg REQUIRED)

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
