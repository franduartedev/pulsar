import QtQuick
import "../theme/Theme.js" as Theme

// One title on the title track: select, move (with snapping) and trim its edges.
// Previews come from the controller so the committed result is exactly what was shown.
Rectangle {
    id: block
    property string titleKey: ""
    property string titleText: ""
    property bool selected: false
    property real canonicalStartX: 0
    property real canonicalWidth: 3
    property real timelineWidth: 1
    property real timelineOffset: 0
    property real previewOffset: 0
    property real trimWidthDelta: 0
    property real pressTimelineX: 0
    property real lastInsideCurrentX: 0
    property bool moving: false
    property bool trimming: false
    property bool trimIsStart: false
    property var dragHost: null
    signal activated()
    signal snapGuide(real x)

    function timelineX(area, mouse) {
        return area.mapToItem(block.parent, mouse.x, mouse.y).x - block.timelineOffset
    }

    // Re-derives the live preview from the pointer's last in-region position;
    // the host calls it on every edge-autoscroll tick so the title keeps
    // following the pointer while the viewport moves.
    function refreshDragPreview() {
        var current = block.lastInsideCurrentX
        if (block.moving) {
            var moved = controller.previewTitleMove(block.titleKey, block.pressTimelineX, current, block.timelineWidth)
            block.previewOffset = moved.offset
            block.snapGuide(moved.guide)
        } else if (block.trimming) {
            var trimmed = controller.previewTitleTrim(block.titleKey, block.trimIsStart, current, block.timelineWidth)
            if (block.trimIsStart) {
                var delta = trimmed.px - block.canonicalStartX
                block.previewOffset = delta
                block.trimWidthDelta = -delta
            } else {
                block.trimWidthDelta = trimmed.px - (block.canonicalStartX + block.canonicalWidth)
            }
            block.snapGuide(trimmed.guide)
        }
    }

    x: canonicalStartX + previewOffset
    width: Math.max(6, canonicalWidth + trimWidthDelta)
    color: selected ? Theme.titleSelected : (body.containsMouse ? Theme.titleHover : Theme.title)
    border.color: Theme.accent
    border.width: selected ? 2 : 0
    radius: Theme.radiusS
    clip: true
    opacity: moving ? 0.92 : 1.0

    Text {
        anchors.left: parent.left; anchors.leftMargin: 10
        anchors.right: parent.right; anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: block.titleText.replace(/\n/g, " ")
        color: Theme.textPrimary
        font.pixelSize: Theme.fontSizeXS
        font.bold: true
        elide: Text.ElideRight
        visible: block.width > 28
    }

    // Edge grips (selected only), same behaviour as clips.
    Repeater {
        model: [true, false]
        delegate: Rectangle {
            readonly property bool isStart: modelData
            x: isStart ? 0 : block.width - width
            width: block.selected ? 8 : 4; height: block.height
            color: "transparent"
            Rectangle { visible: block.selected || body.containsMouse; anchors.fill: parent; color: block.selected ? Theme.accent : Theme.textSecondary; opacity: block.selected ? 1.0 : 0.45; radius: 1 }
            MouseArea {
                anchors.fill: parent
                enabled: block.selected
                preventStealing: true
                cursorShape: Qt.SizeHorCursor
                onPressed: function(mouse) {
                    block.pressTimelineX = block.timelineX(parent, mouse)
                    block.lastInsideCurrentX = block.pressTimelineX
                    block.trimming = false; block.trimIsStart = isStart; block.previewOffset = 0; block.trimWidthDelta = 0
                    if (block.dragHost) {
                        var scene = parent.mapToItem(null, mouse.x, mouse.y)
                        block.dragHost.beginTimelineDrag(block, scene.x, scene.y)
                    }
                }
                onPositionChanged: function(mouse) {
                    if (!pressed) return
                    if (block.dragHost) {
                        var scene = parent.mapToItem(null, mouse.x, mouse.y)
                        block.dragHost.updateTimelineDrag(scene.x, scene.y)
                        if (!block.dragHost.dragInside) return
                    }
                    var current = block.timelineX(parent, mouse)
                    if (Math.abs(current - block.pressTimelineX) >= 4) block.trimming = true
                    if (!block.trimming) return
                    var r = controller.previewTitleTrim(block.titleKey, isStart, current, block.timelineWidth)
                    if (isStart) {
                        var delta = r.px - block.canonicalStartX
                        block.previewOffset = delta
                        block.trimWidthDelta = -delta
                    } else {
                        block.trimWidthDelta = r.px - (block.canonicalStartX + block.canonicalWidth)
                    }
                    block.lastInsideCurrentX = current
                    block.snapGuide(r.guide)
                }
                onReleased: function(mouse) {
                    var inside = true
                    if (block.dragHost) {
                        var scene = parent.mapToItem(null, mouse.x, mouse.y)
                        block.dragHost.updateTimelineDrag(scene.x, scene.y)
                        inside = block.dragHost.dragInside
                    }
                    var current = inside ? block.timelineX(parent, mouse) : block.lastInsideCurrentX
                    if (block.trimming) controller.trimTitle(block.titleKey, isStart, current, block.timelineWidth)
                    block.previewOffset = 0; block.trimWidthDelta = 0; block.trimming = false
                    block.snapGuide(-1)
                    if (block.dragHost) block.dragHost.endTimelineDrag()
                }
                onCanceled: {
                    block.previewOffset = 0; block.trimWidthDelta = 0; block.trimming = false
                    block.snapGuide(-1)
                    if (block.dragHost) block.dragHost.endTimelineDrag()
                }
            }
        }
    }

    MouseArea {
        id: body
        objectName: "titleBody"
        anchors.fill: parent
        anchors.leftMargin: block.selected ? 8 : 4
        anchors.rightMargin: block.selected ? 8 : 4
        preventStealing: true
        hoverEnabled: true
        cursorShape: block.moving ? Qt.ClosedHandCursor : Qt.ArrowCursor
        onPressed: function(mouse) {
            block.pressTimelineX = block.timelineX(parent, mouse)
            block.lastInsideCurrentX = block.pressTimelineX
            block.moving = false; block.previewOffset = 0
            if (block.dragHost) {
                var scene = body.mapToItem(null, mouse.x, mouse.y)
                block.dragHost.beginTimelineDrag(block, scene.x, scene.y)
            }
        }
        onPositionChanged: function(mouse) {
            if (!pressed) return
            if (block.dragHost) {
                var scene = body.mapToItem(null, mouse.x, mouse.y)
                block.dragHost.updateTimelineDrag(scene.x, scene.y)
                if (!block.dragHost.dragInside) return
            }
            var current = block.timelineX(parent, mouse)
            if (Math.abs(current - block.pressTimelineX) >= 4) block.moving = true
            if (block.moving) {
                var r = controller.previewTitleMove(block.titleKey, block.pressTimelineX, current, block.timelineWidth)
                block.previewOffset = r.offset
                block.lastInsideCurrentX = current
                block.snapGuide(r.guide)
            }
        }
        onReleased: function(mouse) {
            var inside = true
            if (block.dragHost) {
                var scene = body.mapToItem(null, mouse.x, mouse.y)
                block.dragHost.updateTimelineDrag(scene.x, scene.y)
                inside = block.dragHost.dragInside
            }
            var current = inside ? block.timelineX(parent, mouse) : block.lastInsideCurrentX
            if (block.moving) controller.moveTitle(block.titleKey, block.pressTimelineX, current, block.timelineWidth)
            else block.activated()
            block.previewOffset = 0; block.moving = false
            block.snapGuide(-1)
            if (block.dragHost) block.dragHost.endTimelineDrag()
        }
        onCanceled: {
            block.previewOffset = 0; block.moving = false
            block.snapGuide(-1)
            if (block.dragHost) block.dragHost.endTimelineDrag()
        }
    }
}
