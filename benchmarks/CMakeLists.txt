add_executable(pulsar_benchmarks
  main.cpp
)

set_target_properties(pulsar_benchmarks
  PROPERTIES
    OUTPUT_NAME pulsar-benchmarks
)

target_link_libraries(pulsar_benchmarks
  PRIVATE
    pulsar::application
    pulsar::domain
)

pulsar_apply_project_options(pulsar_benchmarks)
pulsar_apply_compiler_warnings(pulsar_benchmarks)
