import QtQuick
import "../theme/Theme.js" as Theme

Panel {
    id: pane
    property real trackHeight: 54

    // Compact editorial command strip. The ruler and lane below it are one
    // continuous surface so gaps read as intentional empty space.
    Rectangle {
        id: toolbar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 34
        color: Theme.panel
        border.color: Theme.borderSoft
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.border }
        Row {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 7
            Text { text: "TIMELINE"; color: Theme.text; font.pixelSize: Theme.fontSizeS; font.bold: true; font.letterSpacing: 1.5 }
            Text { text: controller.timelineDurationText; color: Theme.faint; font.family: "monospace"; font.pixelSize: Theme.fontSizeS }
            Rectangle { width: 1; height: 16; color: Theme.border; opacity: 0.8 }
            PButton { text: "Split"; enabled: controller.canSplit; width: 56; height: 24; onClicked: controller.splitSelectedClip() }
            PButton { text: "Delete"; enabled: controller.selectedClipKey.length > 0; width: 60; height: 24; onClicked: controller.deleteSelectedClip() }
            PButton { text: "Undo"; enabled: controller.canUndo; width: 54; height: 24; onClicked: controller.undoEdit() }
            PButton { text: "Redo"; enabled: controller.canRedo; width: 54; height: 24; onClicked: controller.redoEdit() }
        }
    }

    Item {
        id: ruler
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: toolbar.bottom
        height: 25
        clip: true
        Rectangle { anchors.fill: parent; color: Theme.ruler }
        Repeater {
            model: 11
            delegate: Item {
                x: index * (ruler.width / 10)
                width: 1
                height: ruler.height
                Rectangle { anchors.bottom: parent.bottom; width: 1; height: index % 5 === 0 ? 10 : 6; color: Theme.edge; opacity: index % 5 === 0 ? 0.9 : 0.55 }
                Text {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 10
                    text: index === 10 ? controller.timelineDurationText : (index === 0 ? "00:00:00.000" : (index * 10) + "%")
                    color: index % 5 === 0 ? Theme.muted : Theme.faint
                    font.family: "monospace"
                    font.pixelSize: Theme.fontSizeXS
                    visible: index % 2 === 0
                }
            }
        }
    }

    Item {
        id: trackArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: ruler.bottom
        anchors.bottom: parent.bottom
        clip: true

        Rectangle { anchors.fill: parent; color: Theme.lane; border.color: Theme.borderSoft; border.width: 1 }
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; y: 1; height: 1; color: Theme.text; opacity: 0.035 }
        VideoTrack { anchors.fill: parent; z: 2 }

        MouseArea {
            anchors.fill: parent
            z: 1
            acceptedButtons: Qt.LeftButton
            onPressed: function(mouse) {
                controller.selectTimelineClip("")
                controller.seekTimelineAt(mouse.x, width)
            }
            onPositionChanged: if (pressed) controller.seekTimelineAt(mouse.x, width)
        }
        Playhead {
            z: 4
            progress: controller.timelinePlayheadProgress
            height: parent.height
        }
    }
}
