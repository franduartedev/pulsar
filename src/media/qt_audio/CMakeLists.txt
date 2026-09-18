find_package(Qt6 REQUIRED COMPONENTS Core Multimedia)

add_library(pulsar_media_qt_audio
  src/qt_audio_output.cpp
)

add_library(pulsar::media_qt_audio ALIAS pulsar_media_qt_audio)

target_include_directories(pulsar_media_qt_audio
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(pulsar_media_qt_audio
  PUBLIC
    pulsar::application
  PRIVATE
    Qt6::Core
    Qt6::Multimedia
)

set_target_properties(pulsar_media_qt_audio PROPERTIES
  AUTOMOC ON
)

pulsar_apply_project_options(pulsar_media_qt_audio)
pulsar_apply_compiler_warnings(pulsar_media_qt_audio)
