import QtQuick
import "../theme/Theme.js" as Theme

Panel {
    id: pane
    property real gutterWidth: Math.max(88, Math.min(120, width * 0.095))
    property real trackLaneHeight: 72

    function trackTitle(label) {
        // "VIDEO 1" -> "Video 1"
        return label.length > 0 ? label.charAt(0) + label.slice(1).toLowerCase() : label
    }

    // ---- Toolbar: title, edit actions, view controls ------------------
    Item {
        id: toolbar
        z: 10
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 40
        Row {
            anchors.left: parent.left
            anchors.leftMargin: Theme.spaceL
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            Text { anchors.verticalCenter: parent.verticalCenter; text: "Timeline"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeL; font.bold: true; rightPadding: Theme.spaceM }
            ToolButton { glyph: "split"; text: "Cortar"; tip: "Cortar en el cabezal (S)"; enabled: controller.canSplit; onClicked: controller.splitSelectedClip() }
            ToolButton { glyph: "trash"; text: "Borrar"; tip: "Borrar clip (Supr)"; enabled: controller.selectedClipKey.length > 0; onClicked: controller.deleteSelectedClip() }
            Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 1; height: 16; color: Theme.borderStrong }
            ToolButton { glyph: "undo"; text: "Deshacer"; tip: "Deshacer (Ctrl+Z)"; enabled: controller.canUndo; onClicked: controller.undoEdit() }
            ToolButton { glyph: "redo"; text: "Rehacer"; tip: "Rehacer (Ctrl+Shift+Z)"; enabled: controller.canRedo; onClicked: controller.redoEdit() }
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: Theme.spaceL
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            Text { anchors.verticalCenter: parent.verticalCenter; text: "Zoom"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeXS }
            PSlider {
                anchors.verticalCenter: parent.verticalCenter; width: 96
                value: Math.log(controller.timelineZoom) / Math.LN10 / 7
                onMoved: function(v) { controller.setTimelineZoom(Math.pow(10, 7 * v)) }
            }
            Item { width: Theme.spaceS; height: 1 }
            Text { anchors.verticalCenter: parent.verticalCenter; text: "Pan"; color: controller.timelineZoom > 1.0 ? Theme.textSecondary : Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
            PSlider {
                anchors.verticalCenter: parent.verticalCenter; width: 72
                enabled: controller.timelineZoom > 1.0
                fill: Theme.textSecondary
                value: controller.timelineScroll
                onMoved: function(v) { controller.setTimelineScroll(v) }
            }
            Item { width: Theme.spaceS; height: 1 }
            Text { anchors.verticalCenter: parent.verticalCenter; text: "Onda"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeXS }
            PSlider {
                anchors.verticalCenter: parent.verticalCenter; width: 64
                fill: Theme.waveform
                value: (controller.waveformDensity - 0.5) / 1.5
                onMoved: function(v) { controller.setWaveformDensity(0.5 + 1.5 * v) }
            }
        }
    }

    // ---- Ruler ---------------------------------------------------------
    Item {
        id: ruler
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: toolbar.bottom
        height: 26
        clip: true
        Rectangle { anchors.fill: parent; color: Theme.ruler }
        Repeater {
            model: controller.hasMedia ? controller.timelineRulerTicks : []
            delegate: Item {
                x: pane.gutterWidth + modelData.progress * (ruler.width - pane.gutterWidth)
                width: 1
                height: ruler.height
                Rectangle { anchors.bottom: parent.bottom; width: 1; height: 7; color: Theme.textMuted; opacity: 0.7 }
                Text { anchors.left: parent.left; anchors.leftMargin: 4; anchors.verticalCenter: parent.verticalCenter; anchors.verticalCenterOffset: -2; text: modelData.label; color: Theme.textSecondary; font.family: Theme.mono; font.pixelSize: Theme.fontSizeXS; visible: index % 2 === 0 }
            }
        }
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.borderSubtle }
    }

    Item {
        id: trackArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: ruler.bottom
        anchors.bottom: parent.bottom
        clip: true

        Flickable {
            id: trackScroll
            anchors.fill: parent
            contentWidth: width
            contentHeight: Math.max(height, controller.timelineTracks.length * pane.trackLaneHeight)
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Item {
                id: contentArea
                width: trackScroll.width
                height: trackScroll.contentHeight
                Rectangle { anchors.fill: parent; color: Theme.laneAudio }
                Repeater {
                    model: controller.timelineTracks
                    delegate: Item {
                        y: modelData.index * pane.trackLaneHeight
                        width: contentArea.width
                        height: pane.trackLaneHeight
                        // Lane (empty space is real background) + header gutter.
                        Rectangle { anchors.fill: parent; color: modelData.kind === "audio" ? Theme.laneAudio : Theme.laneVideo }
                        Rectangle { width: pane.gutterWidth; anchors.top: parent.top; anchors.bottom: parent.bottom; color: Theme.bgElevated }
                        Rectangle { width: 3; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; color: modelData.kind === "audio" ? Theme.waveformAudio : Theme.waveform; opacity: 0.85 }
                        Text { x: 14; anchors.verticalCenter: parent.verticalCenter; text: pane.trackTitle(modelData.label); color: Theme.textPrimary; font.pixelSize: Theme.fontSizeS; elide: Text.ElideRight; width: pane.gutterWidth - 22 }
                        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.bgRoot }
                    }
                }
                Repeater {
                    model: controller.timelineClips
                    delegate: ClipBlock {
                        z: 2
                        visible: model.inViewport
                        y: model.trackIndex * pane.trackLaneHeight + 6
                        canonicalStartX: pane.gutterWidth + model.startProgress * (contentArea.width - pane.gutterWidth)
                        canonicalWidth: Math.max(3, (model.endProgress - model.startProgress) * (contentArea.width - pane.gutterWidth))
                        height: pane.trackLaneHeight - 12
                        audioClip: model.audioTrack
                        timelineWidth: contentArea.width - pane.gutterWidth
                        clipName: model.clipName
                        startText: model.startText
                        endText: model.endText
                        missing: model.missing
                        selected: model.selected
                        muted: model.muted
                        gainDb: model.gainDb
                        clipKey: model.clipKey
                        onActivated: controller.selectTimelineClip(clipKey)
                    }
                }
            MouseArea {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                z: 1
                acceptedButtons: Qt.LeftButton
                onPressed: function(mouse) {
                    controller.selectTimelineClip("")
                    controller.seekTimelineAt(mouse.x - pane.gutterWidth, width - pane.gutterWidth)
                }
                onPositionChanged: function(mouse) { if (pressed) controller.seekTimelineAt(mouse.x - pane.gutterWidth, width - pane.gutterWidth) }
            }
            Playhead {
                z: 4
                progress: controller.timelinePlayheadProgress
                timelineOffset: pane.gutterWidth
                x: pane.gutterWidth + progress * (contentArea.width - pane.gutterWidth)
                height: contentArea.height
            }
            }
        }
    }
}
