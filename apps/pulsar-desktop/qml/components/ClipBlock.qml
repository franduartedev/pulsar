import QtQuick
import "../theme/Theme.js" as Theme

Rectangle {
    id: block
    property string clipKey: ""
    property string clipName: ""
    property string startText: ""
    property string endText: ""
    property bool missing: false
    property bool selected: false
    property real canonicalStartX: 0
    property real canonicalWidth: 3
    property real timelineWidth: 1
    property real previewOffset: 0
    property real trimWidthDelta: 0
    property real pressTimelineX: 0
    property bool trimDragging: false
    property bool moveDragging: false
    signal activated()

    function timelineX(area, mouse) {
        return area.mapToItem(block.parent, mouse.x, mouse.y).x
    }

    x: canonicalStartX + previewOffset
    width: Math.max(3, canonicalWidth + trimWidthDelta)
    color: missing ? "#302126" : (selected ? Theme.clipSelected : (body.containsMouse ? Theme.clipHover : Theme.clipBase))
    border.color: missing ? Theme.danger : (selected ? Theme.accent : (body.containsMouse ? Theme.edge : Theme.border))
    border.width: selected ? 2 : 1
    radius: Theme.radiusS

    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 2; color: missing ? Theme.danger : (selected ? Theme.accent : Theme.edge); opacity: selected || missing ? 0.9 : 0.5 }
    Text { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 7; text: block.clipName; color: Theme.textBright; font.pixelSize: Theme.fontSizeS; font.bold: block.selected; elide: Text.ElideRight }
    Text { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.leftMargin: 7; anchors.rightMargin: 7; anchors.bottomMargin: 6; text: block.missing ? "OFFLINE  ·  " + block.startText : block.startText + "  →  " + block.endText; color: block.missing ? Theme.danger : Theme.muted; font.pixelSize: Theme.fontSizeXS; font.family: "monospace"; elide: Text.ElideRight }

    Rectangle {
        id: leftHandle
        x: 0; y: 0; width: selected ? 8 : 4; height: parent.height
        color: selected ? Theme.accent : "transparent"
        opacity: selected ? 0.8 : 0.25
        MouseArea {
            anchors.fill: parent
            enabled: block.selected
            preventStealing: true
            cursorShape: Qt.SizeHorCursor
            onPressed: {
                block.pressTimelineX = block.timelineX(parent, mouse)
                block.trimDragging = false
                block.previewOffset = 0
                block.trimWidthDelta = 0
            }
            onPositionChanged: {
                var currentTimelineX = block.timelineX(parent, mouse)
            var delta = currentTimelineX - block.pressTimelineX
                if (Math.abs(delta) >= 4) block.trimDragging = true
                if (block.trimDragging) {
                    block.previewOffset = delta
                    block.trimWidthDelta = -delta
                }
            }
            onReleased: {
                var currentTimelineX = block.timelineX(parent, mouse)
                if (block.trimDragging)
                    controller.trimTimelineClipStart(block.clipKey,
                                                     currentTimelineX,
                                                     block.timelineWidth)
                block.previewOffset = 0
                block.trimWidthDelta = 0
                block.trimDragging = false
            }
        }
    }
    Rectangle {
        id: rightHandle
        anchors.right: parent.right; y: 0; width: selected ? 8 : 4; height: parent.height
        color: selected ? Theme.accent : "transparent"
        opacity: selected ? 0.8 : 0.25
        MouseArea {
            anchors.fill: parent
            enabled: block.selected
            preventStealing: true
            cursorShape: Qt.SizeHorCursor
            onPressed: {
                block.pressTimelineX = block.timelineX(parent, mouse)
                block.trimDragging = false
                block.trimWidthDelta = 0
            }
            onPositionChanged: {
                var currentTimelineX = block.timelineX(parent, mouse)
            var delta = currentTimelineX - block.pressTimelineX
                if (Math.abs(delta) >= 4) block.trimDragging = true
                if (block.trimDragging) block.trimWidthDelta = delta
            }
            onReleased: {
                var currentTimelineX = block.timelineX(parent, mouse)
                if (block.trimDragging)
                    controller.trimTimelineClipEnd(block.clipKey,
                                                   currentTimelineX,
                                                   block.timelineWidth)
                block.trimWidthDelta = 0
                block.trimDragging = false
            }
        }
    }
    MouseArea {
        id: body
        anchors.fill: parent
        anchors.leftMargin: leftHandle.width
        anchors.rightMargin: rightHandle.width
        preventStealing: true
        hoverEnabled: true
        onPressed: {
            block.pressTimelineX = block.timelineX(parent, mouse)
            block.moveDragging = false
            block.previewOffset = 0
        }
        onPositionChanged: {
            var currentTimelineX = block.timelineX(parent, mouse)
            var delta = currentTimelineX - block.pressTimelineX
            if (Math.abs(delta) >= 4) block.moveDragging = true
            if (block.moveDragging) block.previewOffset = delta
        }
        onReleased: {
            var currentTimelineX = block.timelineX(parent, mouse)
            if (block.moveDragging) {
                controller.moveTimelineClip(block.clipKey, block.pressTimelineX,
                                            currentTimelineX,
                                            block.timelineWidth)
            } else {
                block.activated()
            }
            block.previewOffset = 0
            block.moveDragging = false
        }
    }
}
