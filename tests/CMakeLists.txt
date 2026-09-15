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
