add_library(pulsar_project_file
  src/file_project_repository.cpp
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
target_compile_definitions(pulsar_project_file PRIVATE PULSAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

pulsar_apply_project_options(pulsar_project_file)
pulsar_apply_compiler_warnings(pulsar_project_file)
