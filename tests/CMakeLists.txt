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
