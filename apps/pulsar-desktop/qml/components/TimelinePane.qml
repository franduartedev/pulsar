import QtQuick
import "../theme/Theme.js" as Theme

Panel {
    id: pane
    property real gutterWidth: 58

    Rectangle {
        id: toolbar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 36
        color: Theme.surfaceRaised
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.border; opacity: 0.8 }
        Row {
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            Text { text: "EDIT"; color: Theme.textBright; font.pixelSize: Theme.fontSizeS; font.bold: true; font.letterSpacing: 1.8 }
            Text { text: controller.timelineDurationText; color: Theme.faint; font.family: "monospace"; font.pixelSize: Theme.fontSizeS }
            Rectangle { width: 1; height: 16; color: Theme.border; opacity: 0.8 }
            PButton { text: "Split"; enabled: controller.canSplit; width: 58; height: 24; onClicked: controller.splitSelectedClip() }
            PButton { text: "Delete"; enabled: controller.selectedClipKey.length > 0; width: 62; height: 24; onClicked: controller.deleteSelectedClip() }
            PButton { text: "Undo"; enabled: controller.canUndo; width: 56; height: 24; onClicked: controller.undoEdit() }
            PButton { text: "Redo"; enabled: controller.canRedo; width: 56; height: 24; onClicked: controller.redoEdit() }
        }
        Text { anchors.right: parent.right; anchors.rightMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: "V1 · VIDEO"; color: Theme.faint; font.pixelSize: Theme.fontSizeXS; font.letterSpacing: 1.2 }
    }

    Item {
        id: ruler
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: toolbar.bottom
        height: 28
        clip: true
        Rectangle { anchors.fill: parent; color: Theme.ruler }
        Rectangle { width: pane.gutterWidth; anchors.top: parent.top; anchors.bottom: parent.bottom; color: Theme.surfaceRaised; opacity: 0.75 }
        Repeater {
            model: controller.timelineRulerTicks
            delegate: Item {
                x: pane.gutterWidth + modelData.progress * (ruler.width - pane.gutterWidth)
                width: 1
                height: ruler.height
                Rectangle { anchors.bottom: parent.bottom; width: 1; height: 12; color: Theme.accent; opacity: 0.6 }
                Text { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.bottomMargin: 12; text: modelData.label; color: Theme.muted; font.family: "monospace"; font.pixelSize: Theme.fontSizeXS; visible: index % 2 === 0 }
            }
        }
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.border }
    }

    Item {
        id: trackArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: ruler.bottom
        anchors.bottom: parent.bottom
        clip: true

        Rectangle { anchors.fill: parent; color: Theme.lane }
        Rectangle { width: pane.gutterWidth; anchors.top: parent.top; anchors.bottom: parent.bottom; color: Theme.surfaceRaised; opacity: 0.72 }
        Rectangle { x: pane.gutterWidth; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 1; color: Theme.border; opacity: 0.9 }
        Text { x: 15; y: 14; text: "V1"; color: Theme.text; font.pixelSize: Theme.fontSizeM; font.bold: true; font.letterSpacing: 1.2 }
        Text { x: 15; y: 33; text: "VIDEO"; color: Theme.faint; font.pixelSize: Theme.fontSizeXS; font.letterSpacing: 1.1 }

        Item {
            id: contentArea
            anchors.left: parent.left
            anchors.leftMargin: pane.gutterWidth
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            clip: true
            Rectangle { anchors.fill: parent; color: Theme.lane }
            VideoTrack { anchors.fill: parent; z: 2 }
            MouseArea {
                anchors.fill: parent
                z: 1
                acceptedButtons: Qt.LeftButton
                onPressed: function(mouse) {
                    controller.selectTimelineClip("")
                    controller.seekTimelineAt(mouse.x, width)
                }
                onPositionChanged: function(mouse) { if (pressed) controller.seekTimelineAt(mouse.x, width) }
            }
            Playhead {
                z: 4
                progress: controller.timelinePlayheadProgress
                height: parent.height
            }
        }
    }
}
