import QtQuick
import "../theme/Theme.js" as Theme

// Slim seek line across the workspace. Quiet until hovered.
Item {
    id: strip

    Item {
        id: track
        anchors.fill: parent
        anchors.leftMargin: Theme.spaceM
        anchors.rightMargin: Theme.spaceM

        property real pos: Math.max(0, Math.min(1, controller.progress))
        property bool interactive: controller.hasMedia && !controller.busy
        readonly property bool hot: interactive && (hit.containsMouse || hit.pressed)

        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
            height: track.hot ? 5 : 3; radius: 2.5
            color: Theme.track
            opacity: track.interactive ? 1.0 : 0.5
        }
        Rectangle {
            anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, Math.min(parent.width, parent.width * track.pos))
            height: track.hot ? 5 : 3; radius: 2.5
            color: track.interactive ? Theme.accent : Theme.textMuted
            opacity: track.interactive ? 1.0 : 0.4
        }
        Rectangle {
            visible: track.hot
            x: Math.max(0, Math.min(parent.width - width, parent.width * track.pos - width / 2))
            anchors.verticalCenter: parent.verticalCenter
            width: 11; height: 11; radius: 5.5
            color: Theme.textPrimary
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
