add_executable(pulsar_engine_lab
  main.cpp
)

add_executable(pulsar::engine_lab ALIAS pulsar_engine_lab)

set_target_properties(pulsar_engine_lab
  PROPERTIES
    OUTPUT_NAME pulsar-engine-lab
)

target_link_libraries(pulsar_engine_lab
  PRIVATE
    pulsar::application
    pulsar::domain
    pulsar::media_ffmpeg
)

pulsar_apply_project_options(pulsar_engine_lab)
pulsar_apply_compiler_warnings(pulsar_engine_lab)
