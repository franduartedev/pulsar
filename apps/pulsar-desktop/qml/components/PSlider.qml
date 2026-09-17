import QtQuick
import "../theme/Theme.js" as Theme

Item {
    id: slider
    property real value: 0
    signal moved(real value)

    height: 26

    // Track background
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: 3
        radius: 2
        color: Theme.track
    }

    // Elapsed fill
    Rectangle {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: Math.max(0, Math.min(parent.width, parent.width * slider.value))
        height: 3
        radius: 2
        color: slider.enabled ? Theme.accent : Theme.muted
        opacity: slider.enabled ? 1.0 : 0.5
    }

    // Ring handle
    Rectangle {
        x: Math.max(0, Math.min(parent.width - width, parent.width * slider.value - width / 2))
        anchors.verticalCenter: parent.verticalCenter
        width: 14
        height: 14
        radius: 7
        color: Theme.base
        border.width: 2
        border.color: {
            if (!slider.enabled) {
                return Theme.muted
            }
            if (mouse.pressed) {
                return Theme.textBright
            }
            return mouse.containsMouse ? Theme.accentHover : Theme.accent
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onPressed: update(mouse.x)
        onPositionChanged: if (pressed) update(mouse.x)
        function update(px) {
            var next = Math.max(0, Math.min(1, px / Math.max(1, slider.width)))
            slider.value = next
            slider.moved(next)
        }
    }
}
