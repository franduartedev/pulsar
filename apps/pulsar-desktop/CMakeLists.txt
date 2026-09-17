find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)

qt_add_executable(pulsar_desktop
  src/main.cpp
  src/desktop_controller.cpp
  src/desktop_controller.hpp
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
)

pulsar_apply_project_options(pulsar_desktop)
pulsar_apply_compiler_warnings(pulsar_desktop)

qt_import_qml_plugins(pulsar_desktop)

add_test(NAME pulsar.desktop.smoke
         COMMAND pulsar_desktop --smoke-exit-ms 250)
set_tests_properties(pulsar.desktop.smoke
  PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
