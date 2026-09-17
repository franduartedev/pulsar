import QtQuick
import "../theme/Theme.js" as Theme

Panel {
    id: pane
    property real trackHeight: 48

    Row {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        height: 18
        spacing: 12
        Text { text: "TIMELINE"; color: Theme.muted; font.pixelSize: Theme.fontSizeS; font.letterSpacing: 1.4 }
        Text { text: controller.timelineDurationText; color: Theme.faint; font.family: "monospace"; font.pixelSize: Theme.fontSizeS }
    }

    Item {
        id: trackArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 34
        anchors.bottomMargin: 10
        clip: true

        Rectangle { anchors.fill: parent; color: Theme.panelAlt; opacity: 0.55 }
        VideoTrack { anchors.fill: parent }

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
