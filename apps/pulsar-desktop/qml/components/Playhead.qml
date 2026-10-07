import QtQuick
import "../theme/Theme.js" as Theme

// Visual only: one accent line spanning ruler and tracks plus a small head.
// It never intercepts the mouse (a clip edge under it stays trimmable);
// seeking is done by pressing/dragging the ruler or the empty lane.
Item {
    id: head
    property real progress: 0   // 0..1 across the timeline area, <0 hides
    property real timelineOffset: 0
    property real timelineWidth: 1
    visible: progress >= 0
    width: 14
    x: timelineOffset + progress * timelineWidth - width / 2
    enabled: false
    Rectangle { anchors.horizontalCenter: parent.horizontalCenter; y: 10; width: 2; height: parent.height - 10; color: Theme.accent }
    // Head: rounded flag, wider than the line so it reads as a grab handle.
    Rectangle { anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; width: 14; height: 12; radius: 3; color: Theme.accent }
    Rectangle { anchors.top: parent.top; anchors.topMargin: 8; anchors.horizontalCenter: parent.horizontalCenter; width: 8; height: 8; rotation: 45; color: Theme.accent }
}
