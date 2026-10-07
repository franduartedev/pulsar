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

    // ---- Unsaved changes guard -------------------------------------------
    // Anything that would replace or close the project goes through guardedRun.
    //   Guardar  -> save (Save dialog if the project has no file yet); continue ONLY if it succeeded
    //   Descartar -> continue explicitly
    //   Cancelar  -> change nothing
    property string pendingAction: ""
    property bool savingForAction: false
    property bool allowClose: false

    function perform(action) {
        if (action === "close") { allowClose = true; root.close() }
        else if (action === "open") openProjectDialog.open()
    }
    function guardedRun(action) {
        if (!controller.projectDirty) { perform(action); return }
        pendingAction = action
        unsavedDialog.open()
    }
    function finishPending() {
        var action = pendingAction
        pendingAction = ""
        unsavedDialog.close()
        if (action.length > 0) perform(action)
    }
    function saveThenContinue() {
        if (controller.activeProjectPath.length > 0) {
            // Failure keeps the project open (the error is shown in the status bar).
            if (controller.saveProject("")) finishPending()
            else unsavedDialog.close()
        } else {
            unsavedDialog.close()
            savingForAction = true
            root.askSavePath("project")
        }
    }
    onClosing: function(close) {
        if (controller.projectDirty && !allowClose) {
            close.accepted = false
            guardedRun("close")
        }
    }

    // Guardar / Guardar como / Exportar ask for a destination path (see PathPromptDialog).
    property string pathPurpose: ""
    function askSavePath(purpose) {
        pathPurpose = purpose
        if (purpose === "export") {
            pathPrompt.title = "Exportar vídeo"; pathPrompt.confirmText = "Exportar"; pathPrompt.extension = ".mp4"
        } else {
            pathPrompt.title = "Guardar proyecto"; pathPrompt.confirmText = "Guardar"; pathPrompt.extension = ".pulsar"
        }
        pathPrompt.openWith(controller.suggestedPath(purpose))
    }

    Shortcut { sequence: "Space"; onActivated: controller.togglePlayback() }
    Shortcut { sequence: "Ctrl+I"; onActivated: fileDialog.open() }
    Shortcut { sequence: "Ctrl+O"; onActivated: root.guardedRun("open") }
    Shortcut { sequence: "Ctrl+S"; onActivated: controller.activeProjectPath.length > 0 ? controller.saveProject(controller.activeProjectPath) : root.askSavePath("project") }
    Shortcut { sequence: "Ctrl+Shift+S"; onActivated: root.askSavePath("project") }
    Shortcut { sequence: "Home"; onActivated: controller.seekToStart() }
    Shortcut { sequence: "End"; onActivated: controller.seekToEnd() }
    Shortcut { sequence: "S"; onActivated: controller.splitSelectedClip() }
    Shortcut { sequence: "Delete"; onActivated: controller.deleteSelectedClip() }
    Shortcut { sequence: "Ctrl+Z"; onActivated: controller.undoEdit() }
    Shortcut { sequence: "Ctrl+Shift+Z"; onActivated: controller.redoEdit() }
    Shortcut { sequence: "Shift+Z"; onActivated: controller.fitTimeline() }

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
        height: 48
        onOpenProjectRequested: root.guardedRun("open")
        onSaveProjectRequested: controller.activeProjectPath.length > 0 ? controller.saveProject(controller.activeProjectPath) : root.askSavePath("project")
        onSaveProjectAsRequested: root.askSavePath("project")
        onImportMediaRequested: fileDialog.open()
        onExportRequested: root.askSavePath("export")
    }

    MediaPanel {
        id: mediaPanel
        anchors.left: parent.left
        anchors.top: header.bottom
        anchors.bottom: timelinePane.top
        anchors.leftMargin: root.gap
        anchors.topMargin: root.gap
        anchors.bottomMargin: root.gap
        width: root.mediaWidth
        onImportRequested: fileDialog.open()
    }

    InspectorPanel {
        id: inspector
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: timelinePane.top
        anchors.rightMargin: root.gap
        anchors.topMargin: root.gap
        anchors.bottomMargin: root.gap
        width: root.inspectorWidth
    }

    // Monitor module: viewer + attached transport on one surface.
    Rectangle {
        id: monitor
        anchors.left: mediaPanel.right
        anchors.right: inspector.left
        anchors.top: header.bottom
        anchors.bottom: timelinePane.top
        anchors.leftMargin: root.gap
        anchors.rightMargin: root.gap
        anchors.topMargin: root.gap
        anchors.bottomMargin: root.gap
        color: Theme.bgPanel
        radius: Theme.radiusL
        Column {
            anchors.fill: parent
            spacing: 0
            ViewerPane {
                width: parent.width
                height: Math.max(150, parent.height - scrubber.height - transportBar.height)
                onImportRequested: fileDialog.open()
            }
            // Program seek line, attached to the monitor it controls.
            SourceScrubber { id: scrubber; width: parent.width; height: 20 }
            TransportBar { id: transportBar; width: parent.width; height: compactWorkspace ? 44 : 52 }
        }
    }

    TimelinePane {
        id: timelinePane
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: statusBar.top
        anchors.leftMargin: root.gap
        anchors.rightMargin: root.gap
        anchors.bottomMargin: 2
        // The timeline is where editing happens: it gets a third of the window.
        height: compactWorkspace ? 200 : Math.max(236, Math.min(360, Math.round(root.height * 0.36)))
    }

    StatusBar {
        id: statusBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 26
    }

    UnsavedChangesDialog {
        id: unsavedDialog
        onSaveRequested: root.saveThenContinue()
        onDiscardRequested: root.finishPending()
        onCancelRequested: { root.pendingAction = ""; unsavedDialog.close() }
    }

    PathPromptDialog {
        id: pathPrompt
        onAccepted: function(path) {
            if (root.pathPurpose === "export") {
                controller.exportProject(path)
            } else {
                var ok = controller.saveProject(path)
                var continuing = root.savingForAction
                root.savingForAction = false
                if (continuing && ok) root.finishPending()
                else if (continuing) root.pendingAction = ""   // save failed: stay in the project
            }
        }
        onRejected: { root.savingForAction = false; root.pendingAction = "" }
    }
}
