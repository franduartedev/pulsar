import QtQuick
import QtQuick.Window
import QtQuick.Dialogs
import "theme/Theme.js" as Theme
import "components"

// High-level composition: workspace chrome, quiet support panels and one
// central monitor module (viewer + attached transport). All behavior lives
// in the existing controller; this file only composes areas.
Window {
    id: root
    width: 1280
    height: 760
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: controller.projectDirty ? "PULSAR *" : "PULSAR"
    color: Theme.workspace

    // Viewer stays dominant: side panels take bounded relative slices.
    property real mediaWidth: Math.max(238, Math.min(310, width * 0.205))
    property real inspectorWidth: Math.max(270, Math.min(330, width * 0.22))
    property bool compactWorkspace: height <= 650

    Shortcut { sequence: "Space"; onActivated: controller.togglePlayback() }
    Shortcut { sequence: "Ctrl+I"; onActivated: fileDialog.open() }
    Shortcut { sequence: "Ctrl+O"; onActivated: openProjectDialog.open() }
    Shortcut { sequence: "Ctrl+S"; onActivated: controller.activeProjectPath.length > 0 ? controller.saveProject(controller.activeProjectPath) : saveProjectDialog.open() }
    Shortcut { sequence: "Ctrl+Shift+S"; onActivated: saveProjectDialog.open() }
    Shortcut { sequence: "Home"; onActivated: controller.seekToStart() }
    Shortcut { sequence: "End"; onActivated: controller.seekToEnd() }
    Shortcut { sequence: "S"; onActivated: controller.splitSelectedClip() }
    Shortcut { sequence: "Delete"; onActivated: controller.deleteSelectedClip() }
    Shortcut { sequence: "Ctrl+Z"; onActivated: controller.undoEdit() }
    Shortcut { sequence: "Ctrl+Shift+Z"; onActivated: controller.redoEdit() }

    FileDialog {
        id: fileDialog
        title: "Importar medio"
        fileMode: FileDialog.OpenFile
        nameFilters: [
            "Medios (*.mp4 *.mov *.mkv *.avi *.webm *.wav *.mp3 *.m4a *.aac *.flac)",
            "Todos los archivos (*)"
        ]
        onAccepted: controller.openUrl(selectedFile)
    }

    FileDialog {
        id: openProjectDialog
        title: "Abrir proyecto"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Proyectos PULSAR (*.pulsar)", "Todos los archivos (*)"]
        onAccepted: controller.openProject(decodeURIComponent(selectedFile.toString().replace(/^file:\/\//, "")))
    }

    FileDialog {
        id: saveProjectDialog
        title: "Guardar proyecto"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Proyectos PULSAR (*.pulsar)", "Todos los archivos (*)"]
        defaultSuffix: "pulsar"
        onAccepted: controller.saveProject(decodeURIComponent(selectedFile.toString().replace(/^file:\/\//, "")))
    }

    DropArea {
        anchors.fill: parent
        onDropped: function(drop) {
            if (drop.hasUrls && drop.urls.length > 0) {
                controller.openUrl(drop.urls[0])
                drop.acceptProposedAction()
            }
        }
    }

    HeaderBar {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 58
        onOpenProjectRequested: openProjectDialog.open()
        onSaveProjectRequested: controller.activeProjectPath.length > 0 ? controller.saveProject(controller.activeProjectPath) : saveProjectDialog.open()
        onSaveProjectAsRequested: saveProjectDialog.open()
        onImportMediaRequested: fileDialog.open()
    }

    MediaPanel {
        id: mediaPanel
        anchors.left: parent.left
        anchors.top: header.bottom
        anchors.topMargin: compactWorkspace ? 8 : 12
        anchors.bottom: scrubber.top
        anchors.bottomMargin: compactWorkspace ? 6 : 8
        width: root.mediaWidth
        onImportRequested: fileDialog.open()
    }

    InspectorPanel {
        id: inspector
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.topMargin: compactWorkspace ? 8 : 12
        anchors.bottom: scrubber.top
        anchors.bottomMargin: compactWorkspace ? 6 : 8
        width: root.inspectorWidth
    }

    Rectangle {
        id: monitor
        anchors.left: mediaPanel.right
        anchors.right: inspector.left
        anchors.top: header.bottom
        anchors.leftMargin: compactWorkspace ? 8 : 10
        anchors.rightMargin: compactWorkspace ? 8 : 10
        anchors.topMargin: compactWorkspace ? 8 : 12
        anchors.bottom: scrubber.top
        anchors.bottomMargin: compactWorkspace ? 6 : 8
        color: Theme.monitor
        border.color: Theme.border
        border.width: 1
        radius: Theme.radiusM
        Column {
            anchors.fill: parent
            anchors.margins: 1
            spacing: 0
            ViewerPane {
                width: parent.width
                height: Math.max(150, parent.height - transportBar.height - 1)
                onImportRequested: fileDialog.open()
            }
            Rectangle { width: parent.width; height: 1; color: Theme.edge; opacity: 0.55 }
            TransportBar { id: transportBar; width: parent.width; height: compactWorkspace ? 58 : 64 }
        }
    }

    SourceScrubber {
        id: scrubber
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: timelinePane.top
        anchors.leftMargin: compactWorkspace ? 8 : 12
        anchors.rightMargin: compactWorkspace ? 8 : 12
        height: compactWorkspace ? 38 : 42
    }

    TimelinePane {
        id: timelinePane
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: statusBar.top
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.bottomMargin: 12
        height: compactWorkspace ? 188 : Math.max(210, Math.min(276, root.height * 0.32))
    }

    StatusBar {
        id: statusBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.bottomMargin: 12
    }
}
