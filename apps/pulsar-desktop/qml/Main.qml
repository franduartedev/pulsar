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

    // ---- Viewer / Timeline split -------------------------------------------
    // Session UI state only: the divider position never enters the project file,
    // a transform, or any export decision. The Viewer canvas re-fits to whatever
    // height it is given (see ViewerPane), so resizing cannot move a clip.
    property real timelineHeight: defaultSplit()
    readonly property real minTimelineHeight: 176
    readonly property real minViewerHeight: 200
    readonly property real splitterHeight: 10
    // True once the divider has been dragged: the user's split then survives
    // window resizes instead of snapping back to the proportional default.
    property bool splitAdjusted: false
    function defaultSplit() { return Math.max(176, Math.min(240, Math.round(root.height * 0.28))) }
    function clampTimelineHeight(value) {
        if (!header || !statusBar) return Math.max(root.minTimelineHeight, value)
        var maximum = root.height - header.height - statusBar.height - root.gap
                      - root.splitterHeight - 2 - root.minViewerHeight
        return Math.max(root.minTimelineHeight, Math.min(Math.max(root.minTimelineHeight, maximum), value))
    }
    function resetSplit() { splitAdjusted = false; timelineHeight = clampTimelineHeight(defaultSplit()) }
    onHeightChanged: timelineHeight = splitAdjusted
        ? clampTimelineHeight(timelineHeight)
        : clampTimelineHeight(defaultSplit())

    // ---- Unsaved changes guard -------------------------------------------
    // Anything that would replace or close the project goes through guardedRun.
    //   Guardar  -> save (Save dialog if the project has no file yet); continue ONLY if it succeeded
    //   Descartar -> continue explicitly
    //   Cancelar  -> change nothing
    property string pendingAction: ""
    property bool savingForAction: false
    property bool allowClose: false

    property string pendingProjectPath: ""
    property string relinkMediaId: ""
    function perform(action) {
        if (action === "close") { allowClose = true; root.close() }
        else if (action === "open") openProjectDialog.open()
        else if (action === "openfile") { var path = pendingProjectPath; pendingProjectPath = ""; controller.openProject(path) }
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
            pathPrompt.title = "Exportar video"; pathPrompt.confirmText = "Exportar"; pathPrompt.extension = ".mp4"; pathPrompt.showPresets = true
        } else {
            pathPrompt.title = "Guardar proyecto"; pathPrompt.confirmText = "Guardar"; pathPrompt.extension = ".pulsar"; pathPrompt.showPresets = false
        }
        pathPrompt.openWith(controller.suggestedPath(purpose))
    }

    // No editing shortcut fires behind a dialog (a file dialog swallows typing and the keys used
    // to reach the timeline), and single-key ones never fire while typing in a field.
    readonly property bool dialogOpen: pathPrompt.visible || unsavedDialog.visible || recoveryDialog.visible || aboutDialog.visible ||
                                       importDialog.visible || openProjectDialog.visible || colorMenu.visible
    readonly property bool typing: !!root.activeFocusItem && typeof root.activeFocusItem.cursorPosition !== "undefined"
    readonly property bool shortcutsBlocked: typing || dialogOpen
    Shortcut { sequence: "Space"; enabled: !root.shortcutsBlocked; onActivated: controller.togglePlayback() }
    Shortcut { sequence: "Ctrl+I"; enabled: !root.dialogOpen; onActivated: importDialog.openFiles() }
    Shortcut { sequence: "Ctrl+O"; enabled: !root.dialogOpen; onActivated: root.guardedRun("open") }
    Shortcut { sequence: "Ctrl+S"; enabled: !root.dialogOpen; onActivated: controller.activeProjectPath.length > 0 ? controller.saveProject(controller.activeProjectPath) : root.askSavePath("project") }
    Shortcut { sequence: "Ctrl+Shift+S"; enabled: !root.dialogOpen; onActivated: root.askSavePath("project") }
    Shortcut { sequence: "Home"; enabled: !root.shortcutsBlocked; onActivated: controller.seekToStart() }
    Shortcut { sequence: "End"; enabled: !root.shortcutsBlocked; onActivated: controller.seekToEnd() }
    Shortcut { sequence: "S"; enabled: !root.shortcutsBlocked; onActivated: controller.splitSelectedClip() }
    Shortcut { sequence: "Delete"; enabled: !root.shortcutsBlocked; onActivated: controller.deleteSelectedClip() }
    Shortcut { sequence: "Ctrl+Z"; enabled: !root.dialogOpen; onActivated: controller.undoEdit() }
    Shortcut { sequence: "Ctrl+Shift+Z"; enabled: !root.dialogOpen; onActivated: controller.redoEdit() }
    Shortcut { sequence: "Shift+Z"; enabled: !root.shortcutsBlocked; onActivated: controller.fitTimeline() }
    Shortcut { sequence: "Ctrl+D"; enabled: !root.dialogOpen; onActivated: controller.duplicateSelected() }
    Shortcut { sequence: "Left"; enabled: !root.shortcutsBlocked; onActivated: controller.stepFrame(-1) }
    Shortcut { sequence: "Right"; enabled: !root.shortcutsBlocked; onActivated: controller.stepFrame(1) }
    Shortcut { sequence: ","; enabled: !root.shortcutsBlocked; onActivated: controller.stepFrame(-1) }
    Shortcut { sequence: "."; enabled: !root.shortcutsBlocked; onActivated: controller.stepFrame(1) }
    Shortcut { sequence: "M"; enabled: !root.shortcutsBlocked; onActivated: controller.toggleSelectedClipMute() }
    Shortcut { sequence: "Shift+Delete"; enabled: !root.shortcutsBlocked; onActivated: controller.rippleDeleteSelectedClip() }
    Shortcut { sequence: "Q"; enabled: !root.shortcutsBlocked; onActivated: controller.trimSelectedStartToPlayhead() }
    Shortcut { sequence: "W"; enabled: !root.shortcutsBlocked; onActivated: controller.trimSelectedEndToPlayhead() }
    Shortcut { sequence: "Alt+Right"; enabled: !root.shortcutsBlocked; onActivated: controller.selectNextClip() }
    Shortcut { sequence: "Alt+Left"; enabled: !root.shortcutsBlocked; onActivated: controller.selectPreviousClip() }
    Shortcut { sequence: "T"; enabled: !root.shortcutsBlocked; onActivated: controller.addTitle() }
    Shortcut { sequence: "N"; enabled: !root.shortcutsBlocked; onActivated: controller.snapEnabled = !controller.snapEnabled }

    ImportDialog { id: importDialog }

    FileDialog {
        id: openProjectDialog
        title: "Abrir proyecto"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Proyectos PULSAR (*.pulsar)", "Todos los archivos (*)"]
        onAccepted: controller.openProject(decodeURIComponent(selectedFile.toString().replace(/^file:\/\//, "")))
    }
    FileDialog {
        id: relinkDialog
        title: "Relinkear medio"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Medios compatibles (*.mp4 *.mov *.mkv *.avi *.webm *.m4v *.wav *.mp3 *.m4a *.aac *.flac *.png *.jpg *.jpeg *.webp)", "Todos los archivos (*)"]
        onAccepted: {
            controller.relinkAsset(root.relinkMediaId, decodeURIComponent(selectedFile.toString().replace(/^file:\/\//, "")))
            root.relinkMediaId = ""
        }
    }

    DropArea {
        anchors.fill: parent
        onDropped: function(drop) {
            if (drop.hasUrls && drop.urls.length > 0) {
                var first = drop.urls[0].toString()
                if (drop.urls.length === 1 && first.toLowerCase().endsWith(".pulsar")) {
                    // A project dropped on the window opens as a project (after the unsaved-changes guard).
                    root.pendingProjectPath = decodeURIComponent(first.replace(/^file:\/\//, ""))
                    root.guardedRun("openfile")
                } else {
                    controller.openUrls(drop.urls)
                }
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
        onImportMediaRequested: importDialog.openFiles()
        onExportRequested: root.askSavePath("export")
        onAboutRequested: aboutDialog.open()
    }

    MediaPanel {
        id: mediaPanel
        objectName: "mediaPanel"
        anchors.left: parent.left
        anchors.top: header.bottom
        anchors.bottom: splitHandle.top
        anchors.leftMargin: root.gap
        anchors.topMargin: root.gap
        anchors.bottomMargin: root.gap
        width: root.mediaWidth
        onImportRequested: importDialog.openFiles()
        onImportFolderRequested: importDialog.openFolder()
        onRelinkRequested: function(mediaId) { root.relinkMediaId = mediaId; relinkDialog.open() }
        onCreateColorRequested: function(sx, sy) { var p = root.contentItem.mapFromItem(null, sx, sy); colorMenu.openAt(p.x, p.y) }
        onAssetDragStarted: function(mediaId, name, isVideo) { dragGhost.label = name; dragGhost.visible = true }
        onAssetDragMoved: function(mediaId, sx, sy) {
            var p = root.contentItem.mapFromItem(null, sx, sy)
            dragGhost.x = p.x + 12; dragGhost.y = p.y + 8
            timelinePane.updateDrop(mediaId, sx, sy)
        }
        onAssetDragEnded: function(mediaId, sx, sy) { dragGhost.visible = false; timelinePane.commitDrop(mediaId, sx, sy) }
    }

    // Floating label while an item is dragged from the library.
    Rectangle {
        id: dragGhost
        property string label: ""
        visible: false
        z: 2000
        width: Math.min(220, ghostText.implicitWidth + 20); height: 26
        radius: Theme.radiusM
        color: Theme.bgElevated; border.color: Theme.accent
        Text { id: ghostText; anchors.centerIn: parent; width: parent.width - 16; text: dragGhost.label; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeS; elide: Text.ElideMiddle }
    }

    InspectorPanel {
        id: inspector
        objectName: "inspectorPanel"
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: splitHandle.top
        anchors.rightMargin: root.gap
        anchors.topMargin: root.gap
        anchors.bottomMargin: root.gap
        width: root.inspectorWidth
    }

    // Monitor module: viewer + attached transport on one surface.
    Rectangle {
        id: monitor
        objectName: "monitorModule"
        anchors.left: mediaPanel.right
        anchors.right: inspector.left
        anchors.top: header.bottom
        anchors.bottom: splitHandle.top
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
                onImportRequested: importDialog.openFiles()
            }
            // Program seek line, attached to the monitor it controls.
            SourceScrubber { id: scrubber; width: parent.width; height: 20 }
            TransportBar { id: transportBar; width: parent.width; height: compactWorkspace ? 44 : 52 }
        }
    }

    // Divider between the Viewer workspace and the Timeline. Drag to trade
    // space; double-click restores the default split. Purely a workspace
    // affordance: it never touches project data.
    Rectangle {
        id: splitHandle
        objectName: "workspaceSplitter"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.gap
        anchors.rightMargin: root.gap
        anchors.bottom: timelinePane.top
        height: root.splitterHeight
        color: splitMouse.containsMouse || splitMouse.pressed ? Theme.bgHover : "transparent"
        Rectangle {
            anchors.centerIn: parent
            width: 44; height: 2; radius: 1
            color: splitMouse.containsMouse || splitMouse.pressed ? Theme.accent : Theme.borderStrong
        }
        MouseArea {
            id: splitMouse
            objectName: "workspaceSplitMouse"
            anchors.fill: parent
            anchors.topMargin: -3
            anchors.bottomMargin: -3
            hoverEnabled: true
            preventStealing: true
            cursorShape: Qt.SizeVerCursor
            property real pressSceneY: 0
            property real pressHeight: 0
            onPressed: function(mouse) {
                pressSceneY = splitMouse.mapToItem(null, mouse.x, mouse.y).y
                pressHeight = root.timelineHeight
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                var sceneY = splitMouse.mapToItem(null, mouse.x, mouse.y).y
                root.splitAdjusted = true
                root.timelineHeight = root.clampTimelineHeight(pressHeight - (sceneY - pressSceneY))
            }
            onDoubleClicked: root.resetSplit()
        }
    }

    TimelinePane {
        id: timelinePane
        objectName: "timelinePane"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: statusBar.top
        anchors.leftMargin: root.gap
        anchors.rightMargin: root.gap
        anchors.bottomMargin: 2
        // The splitter owns the divider; the Timeline takes the height it is
        // assigned so the Viewer can stay dominant by default.
        height: root.timelineHeight
        onClipContextRequested: function(sx, sy) { clipMenu.openAt(sx, sy) }
    }

    StatusBar {
        id: statusBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 26
        onAboutRequested: aboutDialog.open()
    }

    AboutDialog { id: aboutDialog }

    ClipContextMenu { id: clipMenu }
    ColorCardMenu { id: colorMenu }

    UnsavedChangesDialog {
        id: unsavedDialog
        onSaveRequested: root.saveThenContinue()
        onDiscardRequested: { controller.discardRecovery(); root.finishPending() }
        onCancelRequested: { root.pendingAction = ""; unsavedDialog.close() }
    }

    RecoveryDialog {
        id: recoveryDialog
        onRecoverRequested: { recoveryDialog.close(); controller.restoreRecovery() }
        onDiscardRequested: { recoveryDialog.close(); controller.discardRecovery() }
    }
    Component.onCompleted: { if (controller.recoveryAvailable) recoveryDialog.open(); root.resetSplit() }

    PathPromptDialog {
        id: pathPrompt
        onAccepted: function(path) {
            if (root.pathPurpose === "export") {
                controller.exportProjectWithPreset(path, pathPrompt.preset)
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
