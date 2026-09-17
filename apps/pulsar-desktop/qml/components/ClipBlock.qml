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
    property real pressPixel: 0
    property real pressMouseX: 0
    property bool trimDragging: false
    property bool moveDragging: false
    signal activated()

    x: canonicalStartX + previewOffset
    width: Math.max(3, canonicalWidth + trimWidthDelta)
    color: missing ? "#3a2024" : (selected ? Theme.accentDim : Theme.buttonBase)
    border.color: missing ? Theme.danger : (selected ? Theme.accent : Theme.border)
    border.width: selected ? 2 : 1
    radius: 3

    Text { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 7; text: block.clipName; color: Theme.textBright; font.pixelSize: Theme.fontSizeS; elide: Text.ElideRight }
    Text { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 7; text: block.missing ? "OFFLINE" : block.startText; color: block.missing ? Theme.danger : Theme.muted; font.pixelSize: Theme.fontSizeXS; font.family: "monospace" }

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
                block.pressPixel = block.x + mouse.x
                block.pressMouseX = mouse.x
                block.trimDragging = false
                block.previewOffset = 0
                block.trimWidthDelta = 0
            }
            onPositionChanged: {
                var delta = mouse.x - block.pressMouseX
                if (Math.abs(delta) >= 4) block.trimDragging = true
                if (block.trimDragging) {
                    block.previewOffset = delta
                    block.trimWidthDelta = -delta
                }
            }
            onReleased: {
                if (block.trimDragging)
                    controller.trimTimelineClipStart(block.clipKey,
                                                     block.pressPixel + (mouse.x - block.pressMouseX),
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
                block.pressPixel = block.x + rightHandle.x + mouse.x
                block.pressMouseX = mouse.x
                block.trimDragging = false
                block.trimWidthDelta = 0
            }
            onPositionChanged: {
                var delta = mouse.x - block.pressMouseX
                if (Math.abs(delta) >= 4) block.trimDragging = true
                if (block.trimDragging) block.trimWidthDelta = delta
            }
            onReleased: {
                if (block.trimDragging)
                    controller.trimTimelineClipEnd(block.clipKey,
                                                   block.pressPixel + (mouse.x - block.pressMouseX),
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
        onPressed: {
            block.pressPixel = mouse.x + body.x + block.x
            block.pressMouseX = mouse.x
            block.moveDragging = false
            block.previewOffset = 0
        }
        onPositionChanged: {
            var delta = mouse.x - block.pressMouseX
            if (Math.abs(delta) >= 4) block.moveDragging = true
            if (block.moveDragging) block.previewOffset = delta
        }
        onReleased: {
            if (block.moveDragging) {
                controller.moveTimelineClip(block.clipKey, block.pressPixel,
                                            block.pressPixel + (mouse.x - block.pressMouseX),
                                            block.timelineWidth)
            } else {
                block.activated()
            }
            block.previewOffset = 0
            block.moveDragging = false
        }
    }
}
