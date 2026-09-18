find_package(Qt6 REQUIRED COMPONENTS Core)

add_executable(pulsar-audio-smoke
  main.cpp
)

set_target_properties(pulsar-audio-smoke PROPERTIES
  AUTOMOC ON
)

target_link_libraries(pulsar-audio-smoke
  PRIVATE
    pulsar::media_qt_audio
    pulsar::media_ffmpeg
    pulsar::application
    pulsar::domain
    Qt6::Core
)

pulsar_apply_project_options(pulsar-audio-smoke)
pulsar_apply_compiler_warnings(pulsar-audio-smoke)
