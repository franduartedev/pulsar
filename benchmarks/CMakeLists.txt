add_executable(pulsar_benchmarks
  main.cpp
)

add_executable(pulsar_waveform_cache_benchmark
  waveform_cache_benchmark.cpp
)
add_executable(pulsar_visual_stack_benchmark visual_stack_benchmark.cpp)
target_link_libraries(pulsar_visual_stack_benchmark PRIVATE pulsar::application pulsar::domain pulsar::media_ffmpeg)
pulsar_apply_project_options(pulsar_visual_stack_benchmark)
pulsar_apply_compiler_warnings(pulsar_visual_stack_benchmark)
target_link_libraries(pulsar_waveform_cache_benchmark PRIVATE pulsar::application pulsar::domain)
pulsar_apply_project_options(pulsar_waveform_cache_benchmark)
pulsar_apply_compiler_warnings(pulsar_waveform_cache_benchmark)

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
