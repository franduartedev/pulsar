find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)
if(PULSAR_BUILD_QT_AUDIO)
  find_package(Qt6 REQUIRED COMPONENTS Multimedia)
endif()

qt_add_executable(pulsar_desktop
  src/main.cpp
  src/desktop_controller.cpp
  src/desktop_controller.hpp
  src/builtin_sound_catalog.hpp
  src/timeline_clip_list_model.cpp
  src/timeline_clip_list_model.hpp
  src/waveform_quick_item.cpp
  src/waveform_quick_item.hpp
  src/editor_session.cpp
  src/editor_session.hpp
  src/preview_image_provider.cpp
  src/preview_image_provider.hpp
)

qt_add_resources(pulsar_desktop pulsar_desktop_qml
  PREFIX "/"
  FILES
    qml/Main.qml
    qml/theme/Theme.js
    qml/components/Panel.qml
    qml/components/PButton.qml
    qml/components/PText.qml
    qml/components/PMuted.qml
    qml/components/SectionLabel.qml
    qml/components/StatusDot.qml
    qml/components/TGlyph.qml
    qml/components/TButton.qml
    qml/components/HeaderBar.qml
    qml/components/MediaPanel.qml
    qml/components/ViewerPane.qml
    qml/components/TransportBar.qml
    qml/components/InspectorPanel.qml
    qml/components/SourceScrubber.qml
    qml/components/StatusBar.qml
    qml/components/TimelinePane.qml
    qml/components/VideoTrack.qml
    qml/components/ClipBlock.qml
    qml/components/Playhead.qml
    qml/components/ToolButton.qml
    qml/components/PSlider.qml
)

set_source_files_properties(imports/QtQml/WorkerScript/qmldir
  PROPERTIES QT_RESOURCE_ALIAS qmldir
)
set_source_files_properties(imports/QtQml/WorkerScript/WorkerScript.qml
  PROPERTIES QT_RESOURCE_ALIAS WorkerScript.qml
)
qt_add_resources(pulsar_desktop pulsar_desktop_qml_import_fallbacks
  PREFIX "/pulsar/imports/QtQml/WorkerScript"
  FILES
    imports/QtQml/WorkerScript/qmldir
    imports/QtQml/WorkerScript/WorkerScript.qml
)

set_target_properties(pulsar_desktop
  PROPERTIES
    OUTPUT_NAME pulsar-desktop
    AUTOMOC ON
)

target_link_libraries(pulsar_desktop
  PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    pulsar::media_ffmpeg
    pulsar::application
    pulsar::domain
    pulsar::project_file
)
target_compile_definitions(pulsar_desktop PRIVATE PULSAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

if(PULSAR_BUILD_QT_AUDIO)
  target_compile_definitions(pulsar_desktop PRIVATE PULSAR_HAS_QT_AUDIO=1)
  target_link_libraries(pulsar_desktop PRIVATE Qt6::Multimedia pulsar::media_qt_audio)
endif()

pulsar_apply_project_options(pulsar_desktop)
pulsar_apply_compiler_warnings(pulsar_desktop)

qt_import_qml_plugins(pulsar_desktop)

add_test(NAME pulsar.desktop.smoke
         COMMAND pulsar_desktop --smoke-exit-ms 250)
set_tests_properties(pulsar.desktop.smoke
  PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(pulsar_desktop_export_tests
  tests/desktop_controller_export_test.cpp
  src/desktop_controller.cpp
  src/desktop_controller.hpp
  src/timeline_clip_list_model.cpp
  src/timeline_clip_list_model.hpp
  src/waveform_quick_item.cpp
  src/waveform_quick_item.hpp
  src/editor_session.cpp
  src/editor_session.hpp
  src/preview_image_provider.cpp
  src/preview_image_provider.hpp
  src/builtin_sound_catalog.hpp
)
set_target_properties(pulsar_desktop_export_tests PROPERTIES AUTOMOC ON)
target_include_directories(pulsar_desktop_export_tests PRIVATE src)
target_link_libraries(pulsar_desktop_export_tests
  PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Multimedia
    pulsar::media_ffmpeg
    pulsar::media_qt_audio
    pulsar::application
    pulsar::domain
    pulsar::project_file
)
target_compile_definitions(pulsar_desktop_export_tests
  PRIVATE
    PULSAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    PULSAR_HAS_QT_AUDIO=1
)
pulsar_apply_project_options(pulsar_desktop_export_tests)
pulsar_apply_compiler_warnings(pulsar_desktop_export_tests)
add_test(NAME pulsar.desktop.export_integration
         COMMAND pulsar_desktop_export_tests)
set_tests_properties(pulsar.desktop.export_integration
  PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
