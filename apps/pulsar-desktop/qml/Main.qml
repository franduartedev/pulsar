import QtQuick
import QtQuick.Window
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
    title: "PULSAR"
    color: Theme.workspace

    // Viewer stays dominant: side panels take bounded relative slices.
    property real mediaWidth: Math.max(210, Math.min(250, width * 0.17))
    property real inspectorWidth: Math.max(250, Math.min(310, width * 0.21))

    Shortcut { sequence: "Space"; onActivated: controller.togglePlayback() }
    Shortcut { sequence: "Ctrl+I"; onActivated: controller.importRequested() }
    Shortcut { sequence: "Home"; onActivated: controller.seekToStart() }
    Shortcut { sequence: "End"; onActivated: controller.seekToEnd() }
    Shortcut { sequence: "S"; onActivated: controller.splitSelectedClip() }
    Shortcut { sequence: "Delete"; onActivated: controller.deleteSelectedClip() }
    Shortcut { sequence: "Ctrl+Z"; onActivated: controller.undoEdit() }
    Shortcut { sequence: "Ctrl+Shift+Z"; onActivated: controller.redoEdit() }

    Connections {
        target: controller
        function onFocusImportPathRequested() {
            mediaPanel.focusPath()
        }
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
    }

    MediaPanel {
        id: mediaPanel
        anchors.left: parent.left
        anchors.top: header.bottom
        anchors.bottom: scrubber.top
        anchors.margins: 10
        width: root.mediaWidth
    }

    InspectorPanel {
        id: inspector
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: scrubber.top
        anchors.margins: 10
        width: root.inspectorWidth
    }

    Rectangle {
        id: monitor
        anchors.left: mediaPanel.right
        anchors.right: inspector.left
        anchors.top: header.bottom
        anchors.bottom: scrubber.top
        anchors.margins: 10
        color: Theme.monitor
        border.color: Theme.edge
        radius: Theme.radiusM
        Column {
            anchors.fill: parent
            anchors.margins: 1
            spacing: 0
            ViewerPane { width: parent.width; height: parent.height - transportBar.height - 1 }
            Rectangle { width: parent.width; height: 1; color: Theme.edge; opacity: 0.55 }
            TransportBar { id: transportBar; width: parent.width; height: 64 }
        }
    }

    SourceScrubber {
        id: scrubber
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: timelinePane.top
        anchors.margins: 10
        height: 68
    }

    TimelinePane {
        id: timelinePane
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: statusBar.top
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.bottomMargin: 10
        height: 172
    }

    StatusBar {
        id: statusBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.bottomMargin: 10
    }
}
