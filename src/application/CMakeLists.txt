add_library(pulsar_application
  src/bootstrap.cpp
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
