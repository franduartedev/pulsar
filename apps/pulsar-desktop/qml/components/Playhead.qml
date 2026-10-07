import QtQuick
import "../theme/Theme.js" as Theme

// Single accent line with a small head. Dragging seeks (behaviour unchanged).
Item {
    id: head
    property real progress: 0
    property real timelineOffset: 0
    property bool dragging: false
    visible: progress >= 0
    width: 12
    x: Math.max(0, Math.min(parent.width - width, progress * parent.width - width / 2))
    Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 2; height: parent.height; color: Theme.accent }
    // Half-clipped diamond reads as a downward pointer at the top edge.
    Rectangle { anchors.top: parent.top; anchors.topMargin: -5; anchors.horizontalCenter: parent.horizontalCenter; width: 10; height: 10; color: Theme.accent; rotation: 45 }
    MouseArea {
        id: hit
        anchors.fill: parent
        drag.target: null
        onPressed: function(mouse) { head.dragging = true; seekAt(mouse) }
        onReleased: head.dragging = false
        onPositionChanged: function(mouse) { if (pressed) seekAt(mouse) }
        function seekAt(point) {
            var p = hit.mapToItem(head.parent, point.x, point.y)
            controller.seekTimelineAt(p.x - head.timelineOffset, head.parent.width - head.timelineOffset)
        }
    }
}
