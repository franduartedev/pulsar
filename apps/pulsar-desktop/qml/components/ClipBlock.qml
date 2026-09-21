import QtQuick
import Pulsar.Desktop 1.0
import "../theme/Theme.js" as Theme

Rectangle {
    id: block
    property string clipKey: ""
    property string clipName: ""
    property string startText: ""
    property string endText: ""
    property bool missing: false
    property bool selected: false
    property bool muted: false
    property real gainDb: 0
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
    clip: true

    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 2; color: missing ? Theme.danger : (selected ? Theme.accent : Theme.edge); opacity: selected || missing ? 0.9 : 0.5 }
    WaveformItem {
        id: waveform
        x: 4
        y: 25
        width: Math.max(0, parent.width - 8)
        height: Math.max(0, parent.height - 48)
        clipKey: block.clipKey
        selected: block.selected
        muted: block.muted
        density: controller.waveformDensity
        normalColor: Theme.waveform
        selectedColor: Theme.waveformSelected
        pendingColor: Theme.waveformPending
        unavailableColor: Theme.waveformUnavailable
        baselineColor: Theme.waveformBaseline
        visible: width >= 10 && height >= 8
        Component.onCompleted: controller.attachWaveformItem(block.clipKey, waveform)
        Component.onDestruction: controller.detachWaveformItem(waveform)
    }
    Text { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 7; text: block.clipName; color: Theme.textBright; font.pixelSize: Theme.fontSizeS; font.bold: block.selected; elide: Text.ElideRight }
    Text { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.leftMargin: 7; anchors.bottomMargin: 6; text: block.missing ? "OFFLINE" : (block.muted ? "MUTED" : (Math.abs(block.gainDb) >= 0.05 ? block.gainDb.toFixed(0) + " dB" : "")); color: block.missing ? Theme.danger : (block.muted ? Theme.waveformPending : Theme.muted); font.pixelSize: Theme.fontSizeXS; font.bold: block.muted; font.family: "monospace"; visible: width > 24 }
    Text { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.rightMargin: 7; anchors.bottomMargin: 6; text: block.startText + "  →  " + block.endText; color: block.missing ? Theme.danger : Theme.muted; font.pixelSize: Theme.fontSizeXS; font.family: "monospace"; elide: Text.ElideRight; width: Math.max(0, parent.width - 18); horizontalAlignment: Text.AlignRight }
    Text {
        anchors.centerIn: waveform
        text: waveform.waveformState === WaveformItem.Failed ? "WAVEFORM UNAVAILABLE" :
              (waveform.waveformState === WaveformItem.Missing || waveform.waveformState === WaveformItem.Generating ? "WAVEFORM PENDING" :
              (waveform.hasUnavailableRange ? "NO SOURCE AUDIO" : ""))
        color: Theme.waveformPending
        font.pixelSize: Theme.fontSizeXS
        font.family: "monospace"
        visible: waveform.visible && text.length > 0
    }

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
            onPressed: function(mouse) {
                block.pressTimelineX = block.timelineX(parent, mouse)
                block.trimDragging = false
                block.previewOffset = 0
                block.trimWidthDelta = 0
            }
            onPositionChanged: function(mouse) {
                if (!pressed)
                    return
                var currentTimelineX = block.timelineX(parent, mouse)
            var delta = currentTimelineX - block.pressTimelineX
                if (Math.abs(delta) >= 4) block.trimDragging = true
                if (block.trimDragging) {
                    block.previewOffset = delta
                    block.trimWidthDelta = -delta
                }
            }
            onReleased: function(mouse) {
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
            onPressed: function(mouse) {
                block.pressTimelineX = block.timelineX(parent, mouse)
                block.trimDragging = false
                block.trimWidthDelta = 0
            }
            onPositionChanged: function(mouse) {
                if (!pressed)
                    return
                var currentTimelineX = block.timelineX(parent, mouse)
            var delta = currentTimelineX - block.pressTimelineX
                if (Math.abs(delta) >= 4) block.trimDragging = true
                if (block.trimDragging) block.trimWidthDelta = delta
            }
            onReleased: function(mouse) {
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
        onPressed: function(mouse) {
            block.pressTimelineX = block.timelineX(parent, mouse)
            block.moveDragging = false
            block.previewOffset = 0
        }
        onPositionChanged: function(mouse) {
            if (!pressed)
                return
            var currentTimelineX = block.timelineX(parent, mouse)
            var delta = currentTimelineX - block.pressTimelineX
            if (Math.abs(delta) >= 4) block.moveDragging = true
            if (block.moveDragging) block.previewOffset = delta
        }
        onReleased: function(mouse) {
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
