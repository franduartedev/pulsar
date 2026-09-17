import QtQuick
import "../theme/Theme.js" as Theme

Item {
    id: head
    property real progress: 0
    property bool dragging: false
    width: 12
    x: Math.max(0, Math.min(parent.width - width, progress * parent.width - width / 2))
    Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 1; height: parent.height; color: Theme.accentHover }
    Rectangle { anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; width: 9; height: 9; color: Theme.accentHover; rotation: 45 }
    MouseArea {
        anchors.fill: parent
        drag.target: null
        onPressed: { head.dragging = true; seekAt(mouse) }
        onReleased: head.dragging = false
        onPositionChanged: if (pressed) seekAt(mouse)
        function seekAt(point) {
            var p = hit.mapToItem(head.parent, point.x, point.y)
            controller.seekTimelineAt(p.x, head.parent.width)
        }
    }
}
