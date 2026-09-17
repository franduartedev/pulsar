import QtQuick
import "../theme/Theme.js" as Theme

Item {
    id: slider
    property real value: 0
    property bool enabled: true
    signal moved(real value)

    height: 24

    // Track background
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: 4
        radius: 2
        color: Theme.track
    }

    // Filled portion
    Rectangle {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: Math.max(0, Math.min(parent.width, parent.width * slider.value))
        height: 4
        radius: 2
        color: slider.enabled ? Theme.accent : Theme.muted
    }

    // Thumb
    Rectangle {
        x: Math.max(0, Math.min(parent.width - width, parent.width * slider.value - width / 2))
        anchors.verticalCenter: parent.verticalCenter
        width: 12
        height: 12
        radius: 6
        color: slider.enabled ? Theme.text : Theme.muted
        border.color: Theme.border
    }

    MouseArea {
        anchors.fill: parent
        enabled: slider.enabled
        onPressed: update(mouse.x)
        onPositionChanged: if (pressed) update(mouse.x)
        function update(px) {
            var next = Math.max(0, Math.min(1, px / Math.max(1, slider.width)))
            slider.value = next
            slider.moved(next)
        }
    }
}
