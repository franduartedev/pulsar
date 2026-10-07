import QtQuick
import "../theme/Theme.js" as Theme

// Program transport strip: timeline position with a precise, quiet seek control.
Panel {
    id: strip

    Row {
        id: head
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 16
        anchors.topMargin: 9
        spacing: 10
        Text { anchors.verticalCenter: parent.verticalCenter; text: "SEEK"; color: Theme.textBright; font.pixelSize: Theme.fontSizeXS; font.bold: true; font.letterSpacing: 1.3 }
    }

    Item {
        id: track
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: head.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 2
        anchors.bottomMargin: 6

        property real pos: Math.max(0, Math.min(1, controller.progress))
        property bool interactive: controller.hasMedia && !controller.busy

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: 4
            radius: 2
            color: Theme.track
            opacity: track.interactive ? 1.0 : 0.6
        }
        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, Math.min(parent.width, parent.width * track.pos))
            height: 4
            radius: 2
            color: track.interactive ? Theme.accent : Theme.muted
            opacity: track.interactive ? 0.85 : 0.5
        }
        Rectangle {
            x: Math.max(0, Math.min(parent.width - width, parent.width * track.pos - width / 2))
            anchors.verticalCenter: parent.verticalCenter
            width: track.interactive && (hit.containsMouse || hit.pressed) ? 10 : 2
            height: track.interactive && (hit.containsMouse || hit.pressed) ? 10 : 14
            radius: track.interactive && (hit.containsMouse || hit.pressed) ? 5 : 1
            color: track.interactive ? Theme.textBright : Theme.muted
            opacity: track.interactive ? 1.0 : 0.5
            border.color: track.interactive && (hit.containsMouse || hit.pressed) ? Theme.accent : "transparent"
            border.width: track.interactive && (hit.containsMouse || hit.pressed) ? 2 : 0
        }

        MouseArea {
            id: hit
            anchors.fill: parent
            hoverEnabled: true
            enabled: track.interactive
            onPressed: function(mouse) { seek(mouse.x) }
            onPositionChanged: function(mouse) { if (pressed) seek(mouse.x) }
            function seek(px) {
                controller.seekNormalized(Math.max(0, Math.min(1, px / Math.max(1, track.width))))
            }
        }
    }
}
