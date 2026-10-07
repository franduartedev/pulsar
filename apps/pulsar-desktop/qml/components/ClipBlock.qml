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
    property bool audioClip: false
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
    color: missing ? Theme.clipMissing
         : (selected ? (audioClip ? Theme.clipAudioSelected : Theme.clipVideoSelected)
         : (body.containsMouse ? (audioClip ? Theme.clipAudioHover : Theme.clipVideoHover)
         : (audioClip ? Theme.clipAudio : Theme.clipVideo)))
    // Selection is the only outline; hover and rest stay borderless.
    border.color: missing ? Theme.error : Theme.accent
    border.width: selected ? 2 : (missing ? 1 : 0)
    radius: Theme.radiusM
    clip: true
    opacity: muted ? 0.62 : 1.0

    WaveformItem {
        id: waveform
        x: 4
        y: 22
        width: Math.max(0, parent.width - 8)
        height: Math.max(0, parent.height - 26)
        clipKey: block.clipKey
        selected: block.selected
        muted: block.muted
        density: controller.waveformDensity
        normalColor: block.audioClip ? Theme.waveformAudio : Theme.waveform
        selectedColor: block.audioClip ? Theme.waveformAudioSelected : Theme.waveformSelected
        pendingColor: Theme.waveformPending
        unavailableColor: Theme.waveformUnavailable
        baselineColor: Theme.waveformBaseline
        visible: width >= 10 && height >= 8
        Component.onCompleted: controller.attachWaveformItem(block.clipKey, waveform)
        Component.onDestruction: controller.detachWaveformItem(waveform)
    }
    // One name per clip; hidden when the clip is too narrow to read it.
    Text {
        visible: parent.width > 56
        anchors.left: parent.left; anchors.leftMargin: 8
        anchors.right: chips.visible ? chips.left : parent.right; anchors.rightMargin: 6
        anchors.top: parent.top; anchors.topMargin: 5
        text: block.clipName
        color: Theme.textPrimary
        font.pixelSize: Theme.fontSizeXS
        font.bold: true
        elide: Text.ElideRight
    }
    // State chips: only what is not the default (mute, gain, offline).
    Row {
        id: chips
        visible: parent.width > 90 && (block.muted || block.missing || Math.abs(block.gainDb) >= 0.05)
        anchors.right: parent.right; anchors.rightMargin: 6
        anchors.top: parent.top; anchors.topMargin: 4
        spacing: 4
        Rectangle { visible: block.missing; height: 15; width: offlineText.implicitWidth + 10; radius: 3; color: Theme.error
            Text { id: offlineText; anchors.centerIn: parent; text: "Offline"; color: Theme.textOnAccent; font.pixelSize: 10; font.bold: true } }
        Rectangle { visible: block.muted; height: 15; width: muteText.implicitWidth + 10; radius: 3; color: Theme.warning
            Text { id: muteText; anchors.centerIn: parent; text: "Mute"; color: Theme.textOnWarning; font.pixelSize: 10; font.bold: true } }
        Rectangle { visible: !block.muted && Math.abs(block.gainDb) >= 0.05; height: 15; width: gainText.implicitWidth + 10; radius: 3; color: Theme.chipOverlay
            Text { id: gainText; anchors.centerIn: parent; text: (block.gainDb > 0 ? "+" : "") + block.gainDb.toFixed(0) + " dB"; color: Theme.textPrimary; font.pixelSize: 10; font.family: Theme.mono } }
    }
    // Waveform status stays a whisper, never a headline. Ranges without source
    // audio are drawn by the waveform itself (flat unavailable baseline).
    Text {
        readonly property string note: waveform.waveformState === WaveformItem.Failed ? "Onda no disponible"
            : (waveform.waveformState === WaveformItem.Missing || waveform.waveformState === WaveformItem.Generating ? "Generando onda…" : "")
        anchors.left: parent.left; anchors.leftMargin: 8
        anchors.bottom: parent.bottom; anchors.bottomMargin: 5
        text: note
        color: Theme.textSecondary
        opacity: 0.7
        font.pixelSize: 10
        visible: waveform.visible && note.length > 0 && parent.width > 90
    }

    Rectangle {
        id: leftHandle
        x: 0; y: 0; width: selected ? 8 : 4; height: parent.height
        color: "transparent"
        // Visible only on hover (hint) or selection (grip); never idle.
        Rectangle { visible: block.selected || body.containsMouse; anchors.fill: parent; color: block.selected ? Theme.accent : Theme.textSecondary; opacity: block.selected ? 1.0 : 0.45; radius: 1 }
        Rectangle { visible: block.selected && parent.width >= 8; anchors.centerIn: parent; width: 2; height: Math.min(14, parent.height - 8); radius: 1; color: Theme.textOnAccent; opacity: 0.85 }
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
        color: "transparent"
        Rectangle { visible: block.selected || body.containsMouse; anchors.fill: parent; color: block.selected ? Theme.accent : Theme.textSecondary; opacity: block.selected ? 1.0 : 0.45; radius: 1 }
        Rectangle { visible: block.selected && parent.width >= 8; anchors.centerIn: parent; width: 2; height: Math.min(14, parent.height - 8); radius: 1; color: Theme.textOnAccent; opacity: 0.85 }
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
