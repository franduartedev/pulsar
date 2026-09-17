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
    property real startProgress: 0
    property real timelineWidth: 1
    property real previewOffset: 0
    property real pressPixel: 0
    property real pressMouseX: 0
    signal activated()

    x: startProgress * (parent ? parent.width : 0) + previewOffset
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
            onReleased: controller.trimTimelineClipStart(block.clipKey, block.x + leftHandle.width, block.timelineWidth)
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
            onReleased: controller.trimTimelineClipEnd(block.clipKey, block.x + block.width - rightHandle.width, block.timelineWidth)
        }
    }
    MouseArea {
        id: body
        anchors.fill: parent
        anchors.leftMargin: leftHandle.width
        anchors.rightMargin: rightHandle.width
        preventStealing: true
        drag.target: null
        onPressed: {
            block.activated()
            block.pressPixel = mouse.x + body.x + block.x
            block.pressMouseX = mouse.x
            block.previewOffset = 0
        }
        onPositionChanged: if (pressed) block.previewOffset = mouse.x - block.pressMouseX
        onReleased: {
            controller.moveTimelineClip(block.clipKey, block.pressPixel,
                                        block.pressPixel + (mouse.x - block.pressMouseX), block.timelineWidth)
            block.previewOffset = 0
        }
        onClicked: block.activated()
    }
}
