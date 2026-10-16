find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)
if(PULSAR_BUILD_QT_AUDIO)
  find_package(Qt6 REQUIRED COMPONENTS Multimedia)
endif()

qt_add_executable(pulsar_desktop
  src/main.cpp
  src/desktop_controller.cpp
  src/desktop_controller.hpp
  src/builtin_sound_catalog.hpp
  src/sound_auditioner.cpp
  src/sound_auditioner.hpp
  src/audition_audio.hpp
  src/timeline_clip_list_model.cpp
  src/timeline_clip_list_model.hpp
  src/media_asset_list_model.cpp
  src/media_asset_list_model.hpp
  src/waveform_quick_item.cpp
  src/waveform_quick_item.hpp
  src/editor_session.cpp
  src/editor_session.hpp
  src/preview_image_provider.cpp
  src/preview_image_provider.hpp
  src/thumbnail_service.cpp
  src/thumbnail_service.hpp
  src/title_rasterizer.cpp
  src/title_rasterizer.hpp
  src/title_list_model.cpp
  src/title_list_model.hpp
  src/recovery_service.cpp
  src/recovery_service.hpp
)

qt_add_resources(pulsar_desktop pulsar_desktop_qml
  PREFIX "/"
  FILES
    qml/Main.qml
    qml/theme/Theme.js
    qml/assets/pulse_arc_mark.png
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
    qml/components/TitleBlock.qml
    qml/components/Playhead.qml
    qml/components/ToolButton.qml
    qml/components/UnsavedChangesDialog.qml
    qml/components/RecoveryDialog.qml
    qml/components/ClipContextMenu.qml
    qml/components/ColorCardMenu.qml
    qml/components/BrandMark.qml
    qml/components/AboutDialog.qml
    qml/components/PathPromptDialog.qml
    qml/components/ImportDialog.qml
    qml/components/PSlider.qml
    qml/components/FieldRow.qml
    qml/components/Segmented.qml
    qml/components/Stepper.qml
    qml/components/InspectorSection.qml
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

if(PULSAR_BUILD_QT_AUDIO)
  target_compile_definitions(pulsar_desktop PRIVATE PULSAR_HAS_QT_AUDIO=1)
  target_link_libraries(pulsar_desktop PRIVATE Qt6::Multimedia pulsar::media_qt_audio)
endif()

pulsar_apply_project_options(pulsar_desktop)
pulsar_apply_compiler_warnings(pulsar_desktop)

# Build tree mirrors the portable layout: assets/ next to the executable.
add_custom_command(TARGET pulsar_desktop POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:pulsar_desktop>/assets"
  COMMENT "Staging assets next to pulsar-desktop")

qt_import_qml_plugins(pulsar_desktop)

if(PULSAR_BUILD_TESTS)
find_package(Qt6 REQUIRED COMPONENTS Test)
add_test(NAME pulsar.desktop.smoke
         COMMAND pulsar_desktop --smoke-exit-ms 250)
set_tests_properties(pulsar.desktop.smoke
  PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PULSAR_RECOVERY_DIR=${CMAKE_CURRENT_BINARY_DIR}/recovery-test;LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/lsan.supp:print_suppressions=0"
)

add_executable(pulsar_desktop_export_tests
  tests/desktop_controller_export_test.cpp
  src/desktop_controller.cpp
  src/desktop_controller.hpp
  src/timeline_clip_list_model.cpp
  src/timeline_clip_list_model.hpp
  src/media_asset_list_model.cpp
  src/media_asset_list_model.hpp
  src/waveform_quick_item.cpp
  src/waveform_quick_item.hpp
  src/editor_session.cpp
  src/editor_session.hpp
  src/preview_image_provider.cpp
  src/preview_image_provider.hpp
  src/thumbnail_service.cpp
  src/thumbnail_service.hpp
  src/title_rasterizer.cpp
  src/title_rasterizer.hpp
  src/title_list_model.cpp
  src/title_list_model.hpp
  src/recovery_service.cpp
  src/recovery_service.hpp
  src/builtin_sound_catalog.hpp
  src/sound_auditioner.cpp
  src/sound_auditioner.hpp
  src/audition_audio.hpp
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
add_dependencies(pulsar_desktop_export_tests pulsar_desktop)  # stages assets/ next to the test
add_test(NAME pulsar.desktop.export_integration
         COMMAND pulsar_desktop_export_tests)
set_tests_properties(pulsar.desktop.export_integration
  PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PULSAR_RECOVERY_DIR=${CMAKE_CURRENT_BINARY_DIR}/recovery-export")

add_executable(pulsar_desktop_ingest_tests
  tests/desktop_controller_ingest_test.cpp
  src/desktop_controller.cpp
  src/desktop_controller.hpp
  src/timeline_clip_list_model.cpp
  src/timeline_clip_list_model.hpp
  src/media_asset_list_model.cpp
  src/media_asset_list_model.hpp
  src/waveform_quick_item.cpp
  src/waveform_quick_item.hpp
  src/editor_session.cpp
  src/editor_session.hpp
  src/preview_image_provider.cpp
  src/preview_image_provider.hpp
  src/thumbnail_service.cpp
  src/thumbnail_service.hpp
  src/title_rasterizer.cpp
  src/title_rasterizer.hpp
  src/title_list_model.cpp
  src/title_list_model.hpp
  src/recovery_service.cpp
  src/recovery_service.hpp
  src/builtin_sound_catalog.hpp
  src/sound_auditioner.cpp
  src/sound_auditioner.hpp
  src/audition_audio.hpp
)
set_target_properties(pulsar_desktop_ingest_tests PROPERTIES AUTOMOC ON)
target_include_directories(pulsar_desktop_ingest_tests PRIVATE src)
target_link_libraries(pulsar_desktop_ingest_tests
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
target_compile_definitions(pulsar_desktop_ingest_tests
  PRIVATE
    PULSAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    PULSAR_HAS_QT_AUDIO=1
)
pulsar_apply_project_options(pulsar_desktop_ingest_tests)
pulsar_apply_compiler_warnings(pulsar_desktop_ingest_tests)
add_dependencies(pulsar_desktop_ingest_tests pulsar_desktop)
add_test(NAME pulsar.desktop.ingest_integration
         COMMAND pulsar_desktop_ingest_tests)
set_tests_properties(pulsar.desktop.ingest_integration
  PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PULSAR_RECOVERY_DIR=${CMAKE_CURRENT_BINARY_DIR}/recovery-ingest")

# Release-safety desktop tests (same sources as the other controller tests).
foreach(pair "safety|pulsar.desktop.safety" "shutdown|pulsar.desktop.shutdown" "batch|pulsar.desktop.batch" "title_parity|pulsar.desktop.title_parity" "recovery|pulsar.desktop.recovery" "dnd|pulsar.desktop.dnd" "gate100|pulsar.desktop.gate100" "stills|pulsar.desktop.stills" "polish|pulsar.desktop.polish" "workspace|pulsar.desktop.workspace" "audition|pulsar.desktop.audition")
  string(REPLACE "|" ";" parts "${pair}")
  list(GET parts 0 short)
  list(GET parts 1 test_name)
  set(target pulsar_desktop_${short}_tests)
  add_executable(${target}
    tests/desktop_controller_${short}_test.cpp
    src/desktop_controller.cpp
    src/desktop_controller.hpp
    src/timeline_clip_list_model.cpp
    src/timeline_clip_list_model.hpp
    src/media_asset_list_model.cpp
    src/media_asset_list_model.hpp
    src/waveform_quick_item.cpp
    src/waveform_quick_item.hpp
    src/editor_session.cpp
    src/editor_session.hpp
    src/preview_image_provider.cpp
    src/preview_image_provider.hpp
    src/thumbnail_service.cpp
    src/thumbnail_service.hpp
    src/title_rasterizer.cpp
    src/title_rasterizer.hpp
    src/title_list_model.cpp
    src/title_list_model.hpp
    src/recovery_service.cpp
    src/recovery_service.hpp
    src/builtin_sound_catalog.hpp
    src/sound_auditioner.cpp
    src/sound_auditioner.hpp
    src/audition_audio.hpp
  )
  set_target_properties(${target} PROPERTIES AUTOMOC ON)
  target_include_directories(${target} PRIVATE src)
  target_link_libraries(${target} PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Multimedia
    pulsar::media_ffmpeg pulsar::media_qt_audio pulsar::application pulsar::domain pulsar::project_file)
  target_compile_definitions(${target} PRIVATE PULSAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}" PULSAR_HAS_QT_AUDIO=1)
  pulsar_apply_project_options(${target})
  pulsar_apply_compiler_warnings(${target})
  add_dependencies(${target} pulsar_desktop)
  add_test(NAME ${test_name} COMMAND ${target})
  set_tests_properties(${test_name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PULSAR_RECOVERY_DIR=${CMAKE_CURRENT_BINARY_DIR}/recovery-${short}" TIMEOUT 300)
endforeach()
target_link_libraries(pulsar_desktop_polish_tests PRIVATE Qt6::Test)
target_link_libraries(pulsar_desktop_workspace_tests PRIVATE Qt6::Test)
# The audition test needs the sound pack of the source tree (installed builds find it next to the binary).
set_property(TEST pulsar.desktop.audition APPEND PROPERTY ENVIRONMENT "PULSAR_ASSETS_DIR=${CMAKE_SOURCE_DIR}/assets")
endif()

# ---- Installation (portable bundle / AppImage / distro packaging) ----------
# Layout under the prefix:
#   bin/pulsar-desktop, share/pulsar/assets/sounds/*, share/applications/pulsar.desktop,
#   share/icons/hicolor/{scalable,256x256}/apps/pulsar.*
include(GNUInstallDirs)
install(TARGETS pulsar_desktop RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/" DESTINATION ${CMAKE_INSTALL_DATADIR}/pulsar/assets)
install(FILES "${CMAKE_SOURCE_DIR}/packaging/linux/pulsar.desktop" DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES "${CMAKE_SOURCE_DIR}/packaging/linux/pulsar.svg"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES "${CMAKE_SOURCE_DIR}/packaging/linux/pulsar-256.png"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME pulsar.png)
