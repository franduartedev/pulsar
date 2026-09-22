import QtQuick
import "../theme/Theme.js" as Theme

Item {
    id: head
    property real progress: 0
    property real timelineOffset: 0
    property bool dragging: false
    visible: progress >= 0
    width: 12
    x: Math.max(0, Math.min(parent.width - width, progress * parent.width - width / 2))
    Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 2; height: parent.height; color: Theme.accentHover; opacity: 0.95 }
    Rectangle { anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; width: 11; height: 11; color: Theme.accentHover; border.color: Theme.textBright; border.width: 1; rotation: 45 }
    MouseArea {
        id: hit
        anchors.fill: parent
        drag.target: null
        onPressed: { head.dragging = true; seekAt(mouse) }
        onReleased: head.dragging = false
        onPositionChanged: if (pressed) seekAt(mouse)
        function seekAt(point) {
            var p = hit.mapToItem(head.parent, point.x, point.y)
            controller.seekTimelineAt(p.x - head.timelineOffset, head.parent.width - head.timelineOffset)
        }
    }
}
