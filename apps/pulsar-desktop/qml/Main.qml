import QtQuick
import QtQuick.Window
import QtQuick.Dialogs
import "theme/Theme.js" as Theme
import "components"

// High-level composition: project bar, quiet support panels, one central
// monitor module (viewer + attached transport), timeline and status line.
// All behavior lives in the existing controller; this file only composes areas.
Window {
    id: root
    width: 1280
    height: 760
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: controller.projectDirty ? "PULSAR *" : "PULSAR"
    color: Theme.bgRoot

    // Viewer stays dominant: side panels take bounded relative slices.
    readonly property real gap: compactWorkspace ? Theme.spaceS : Theme.spaceM
    property real mediaWidth: Math.max(224, Math.min(290, width * 0.19))
    property real inspectorWidth: Math.max(250, Math.min(310, width * 0.21))
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

    FileDialog {
        id: exportDialog
        title: "Exportar vídeo"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Vídeo MP4 (*.mp4)"]
        defaultSuffix: "mp4"
        onAccepted: controller.exportProject(decodeURIComponent(selectedFile.toString().replace(/^file:\/\//, "")))
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
        height: 52
        onOpenProjectRequested: openProjectDialog.open()
        onSaveProjectRequested: controller.activeProjectPath.length > 0 ? controller.saveProject(controller.activeProjectPath) : saveProjectDialog.open()
        onSaveProjectAsRequested: saveProjectDialog.open()
        onImportMediaRequested: fileDialog.open()
        onExportRequested: exportDialog.open()
    }

    MediaPanel {
        id: mediaPanel
        anchors.left: parent.left
        anchors.top: header.bottom
        anchors.bottom: scrubber.top
        anchors.leftMargin: root.gap
        anchors.topMargin: root.gap
        anchors.bottomMargin: 2
        width: root.mediaWidth
        onImportRequested: fileDialog.open()
    }

    InspectorPanel {
        id: inspector
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: scrubber.top
        anchors.rightMargin: root.gap
        anchors.topMargin: root.gap
        anchors.bottomMargin: 2
        width: root.inspectorWidth
    }

    // Monitor module: viewer + attached transport on one surface.
    Rectangle {
        id: monitor
        anchors.left: mediaPanel.right
        anchors.right: inspector.left
        anchors.top: header.bottom
        anchors.bottom: scrubber.top
        anchors.leftMargin: root.gap
        anchors.rightMargin: root.gap
        anchors.topMargin: root.gap
        anchors.bottomMargin: 2
        color: Theme.bgPanel
        radius: Theme.radiusL
        Column {
            anchors.fill: parent
            spacing: 0
            ViewerPane {
                width: parent.width
                height: Math.max(150, parent.height - transportBar.height)
                onImportRequested: fileDialog.open()
            }
            TransportBar { id: transportBar; width: parent.width; height: compactWorkspace ? 48 : 56 }
        }
    }

    SourceScrubber {
        id: scrubber
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: timelinePane.top
        anchors.leftMargin: root.gap
        anchors.rightMargin: root.gap
        height: 20
    }

    TimelinePane {
        id: timelinePane
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: statusBar.top
        anchors.leftMargin: root.gap
        anchors.rightMargin: root.gap
        anchors.bottomMargin: 2
        height: compactWorkspace ? 188 : Math.max(214, Math.min(290, root.height * 0.34))
    }

    StatusBar {
        id: statusBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 26
    }
}
