add_library(pulsar_domain
  src/build_info.cpp
)

add_library(pulsar::domain ALIAS pulsar_domain)

target_include_directories(pulsar_domain
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

pulsar_apply_project_options(pulsar_domain)
pulsar_apply_compiler_warnings(pulsar_domain)
