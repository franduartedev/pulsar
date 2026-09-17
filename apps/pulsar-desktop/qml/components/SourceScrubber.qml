import QtQuick
import "../theme/Theme.js" as Theme

// Source navigation strip: elapsed vs remaining range with a distinct
// needle playhead. Tall hit area, thin visual track. No thumbnails.
Panel {
    id: strip

    Row {
        id: head
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 12
        anchors.topMargin: 9
        spacing: 10
        Text { anchors.verticalCenter: parent.verticalCenter; text: "SOURCE"; color: Theme.muted; font.pixelSize: Theme.fontSizeS; font.letterSpacing: 1.5 }
        Text { anchors.verticalCenter: parent.verticalCenter; text: controller.positionText; color: controller.hasMedia ? Theme.text : Theme.faint; font.family: "monospace"; font.pixelSize: Theme.fontSizeM }
    }
    Text { anchors.right: parent.right; anchors.top: parent.top; anchors.rightMargin: 12; anchors.topMargin: 9; text: controller.durationText; color: Theme.faint; font.family: "monospace"; font.pixelSize: Theme.fontSizeM }

    Item {
        id: track
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: head.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 2
        anchors.bottomMargin: 6

        property real pos: Math.max(0, Math.min(1, controller.progress))

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: 4
            radius: 2
            color: Theme.track
            opacity: enabled ? 1.0 : 0.6
        }
        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, Math.min(parent.width, parent.width * track.pos))
            height: 4
            radius: 2
            color: enabled ? Theme.accent : Theme.muted
            opacity: enabled ? 0.85 : 0.5
        }
        Rectangle {
            x: Math.max(0, Math.min(parent.width - width, parent.width * track.pos - width / 2))
            anchors.verticalCenter: parent.verticalCenter
            width: enabled && (hit.containsMouse || hit.pressed) ? 10 : 2
            height: enabled && (hit.containsMouse || hit.pressed) ? 10 : 14
            radius: enabled && (hit.containsMouse || hit.pressed) ? 5 : 1
            color: enabled ? Theme.textBright : Theme.muted
            opacity: enabled ? 1.0 : 0.5
            border.color: enabled && (hit.containsMouse || hit.pressed) ? Theme.accent : "transparent"
            border.width: enabled && (hit.containsMouse || hit.pressed) ? 2 : 0
        }

        MouseArea {
            id: hit
            anchors.fill: parent
            hoverEnabled: true
            enabled: controller.hasMedia && !controller.busy
            onPressed: seek(mouse.x)
            onPositionChanged: if (pressed) seek(mouse.x)
            function seek(px) {
                controller.seekNormalized(Math.max(0, Math.min(1, px / Math.max(1, track.width))))
            }
        }
    }
}
