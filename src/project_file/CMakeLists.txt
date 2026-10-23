add_library(pulsar_project_file
  src/file_project_repository.cpp
  src/filesystem_export_output_publisher.cpp
  src/runtime_resources.cpp
)

add_library(pulsar::project_file ALIAS pulsar_project_file)

target_include_directories(pulsar_project_file
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(pulsar_project_file
  PUBLIC
    pulsar::application
)

pulsar_apply_project_options(pulsar_project_file)
pulsar_apply_compiler_warnings(pulsar_project_file)
