import QtQuick
import "../theme/Theme.js" as Theme

// Compact horizontal slider for timeline view controls. value is 0..1;
// the owner maps it to its own units and reacts to moved().
Item {
    id: slider
    property real value: 0
    property color fill: Theme.accent
    signal moved(real value)

    implicitWidth: 88
    implicitHeight: 20
    opacity: enabled ? 1.0 : 0.38

    Rectangle { id: rail; anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; height: 3; radius: 1.5; color: Theme.track }
    Rectangle { anchors.left: rail.left; anchors.verticalCenter: rail.verticalCenter; height: 3; radius: 1.5; color: slider.fill; width: rail.width * Math.max(0, Math.min(1, slider.value)) }
    Rectangle {
        x: Math.max(0, Math.min(1, slider.value)) * (slider.width - width)
        anchors.verticalCenter: parent.verticalCenter
        width: 10; height: 10; radius: 5
        color: Theme.textPrimary
        border.width: hit.containsMouse || hit.pressed ? 2 : 0
        border.color: slider.fill
    }
    MouseArea {
        id: hit
        anchors.fill: parent
        hoverEnabled: true
        onPressed: function(mouse) { slider.moved(Math.max(0, Math.min(1, mouse.x / width))) }
        onPositionChanged: function(mouse) { if (pressed) slider.moved(Math.max(0, Math.min(1, mouse.x / width))) }
    }
}
