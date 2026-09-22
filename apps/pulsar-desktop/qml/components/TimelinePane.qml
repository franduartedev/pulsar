import QtQuick
import "../theme/Theme.js" as Theme

Panel {
    border.width: 0
    radius: 0
    id: pane
    property real gutterWidth: Math.max(96, Math.min(138, width * 0.115))
    property real trackLaneHeight: 78

    Rectangle {
        id: toolbar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 46
        color: Theme.surfaceRaised
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.border; opacity: 0.8 }
            Row {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 7
            Text { text: "TIMELINE"; color: Theme.textBright; font.pixelSize: Theme.fontSizeS; font.bold: true; font.letterSpacing: 1.6 }
            Rectangle { width: 1; height: 16; color: Theme.border; opacity: 0.8 }
            PButton { text: "CORTAR"; enabled: controller.canSplit; width: 64; height: Theme.controlHeightSmall; onClicked: controller.splitSelectedClip() }
            PButton { text: "BORRAR"; enabled: controller.selectedClipKey.length > 0; width: 64; height: Theme.controlHeightSmall; onClicked: controller.deleteSelectedClip() }
            Rectangle { width: 1; height: 16; color: Theme.border; opacity: 0.8 }
            PButton { text: "DESHACER"; enabled: controller.canUndo; width: 76; height: Theme.controlHeightSmall; onClicked: controller.undoEdit() }
            PButton { text: "REHACER"; enabled: controller.canRedo; width: 72; height: Theme.controlHeightSmall; onClicked: controller.redoEdit() }
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            Text { text: "VISTA"; color: Theme.faint; font.pixelSize: Theme.fontSizeXS; font.bold: true; font.letterSpacing: 0.8; anchors.verticalCenter: parent.verticalCenter }
            Rectangle {
                width: 112; height: 20; radius: Theme.radiusS; color: Theme.track; anchors.verticalCenter: parent.verticalCenter
                Rectangle { x: (Math.log(controller.timelineZoom) / Math.LN10 / 7) * (parent.width - width); y: 5; width: 12; height: 10; radius: 5; color: Theme.accent }
                MouseArea {
                    anchors.fill: parent
                    onPressed: function(mouse) { controller.setTimelineZoom(Math.pow(10, 7 * Math.max(0, Math.min(1, mouse.x / width)))) }
                    onPositionChanged: function(mouse) { if (pressed) controller.setTimelineZoom(Math.pow(10, 7 * Math.max(0, Math.min(1, mouse.x / width)))) }
                }
            }
            Text { text: "PAN"; color: controller.timelineZoom > 1.0 ? Theme.muted : Theme.disabledText; font.pixelSize: Theme.fontSizeXS; anchors.verticalCenter: parent.verticalCenter }
            Rectangle {
                width: 90; height: 20; radius: Theme.radiusS; color: Theme.track; opacity: controller.timelineZoom > 1.0 ? 1.0 : 0.42; anchors.verticalCenter: parent.verticalCenter
                Rectangle { x: controller.timelineScroll * (parent.width - width); y: 5; width: 14; height: 10; radius: 4; color: controller.timelineZoom > 1.0 ? Theme.waveform : Theme.disabledText }
                MouseArea {
                    anchors.fill: parent
                    enabled: controller.timelineZoom > 1.0
                    onPressed: function(mouse) { controller.setTimelineScroll(Math.max(0, Math.min(1, mouse.x / width))) }
                    onPositionChanged: function(mouse) { if (pressed) controller.setTimelineScroll(Math.max(0, Math.min(1, mouse.x / width))) }
                }
            }
            Text { text: "FORMA %1x".arg(controller.waveformDensity.toFixed(1)); color: Theme.muted; font.pixelSize: Theme.fontSizeXS; anchors.verticalCenter: parent.verticalCenter }
            Rectangle {
                width: 72; height: 20; radius: Theme.radiusS; color: Theme.track; anchors.verticalCenter: parent.verticalCenter
                Rectangle { x: ((controller.waveformDensity - 0.5) / 1.5) * (parent.width - width); y: 5; width: 12; height: 10; radius: 4; color: Theme.waveform }
                MouseArea {
                    anchors.fill: parent
                    onPressed: function(mouse) { controller.setWaveformDensity(0.5 + 1.5 * Math.max(0, Math.min(1, mouse.x / width))) }
                    onPositionChanged: function(mouse) { if (pressed) controller.setWaveformDensity(0.5 + 1.5 * Math.max(0, Math.min(1, mouse.x / width))) }
                }
            }
        }
    }

    Item {
        id: ruler
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: toolbar.bottom
        height: 30
        clip: true
        Rectangle { anchors.fill: parent; color: Theme.ruler }
        Rectangle { width: pane.gutterWidth; anchors.top: parent.top; anchors.bottom: parent.bottom; color: Theme.surfaceRaised; opacity: 0.9 }
        Text { x: 16; anchors.bottom: parent.bottom; anchors.bottomMargin: 7; text: "PISTAS"; color: Theme.faint; font.pixelSize: Theme.fontSizeXS; font.bold: true; font.letterSpacing: 1.1 }
        Repeater {
            model: controller.timelineRulerTicks
            delegate: Item {
                x: pane.gutterWidth + modelData.progress * (ruler.width - pane.gutterWidth)
                width: 1
                height: ruler.height
                Rectangle { anchors.bottom: parent.bottom; width: 1; height: 12; color: Theme.accent; opacity: 0.6 }
                Text { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.bottomMargin: 12; text: modelData.label; color: Theme.muted; font.family: "monospace"; font.pixelSize: Theme.fontSizeXS; visible: index % 2 === 0 }
            }
        }
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.border }
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
                Rectangle { anchors.fill: parent; color: Theme.lane }
                Repeater {
                    model: controller.timelineTracks
                    delegate: Rectangle {
                        x: 0
                        y: modelData.index * pane.trackLaneHeight
                        width: contentArea.width
                        height: pane.trackLaneHeight
                        color: modelData.kind === "audio" ? Theme.panelAlt : Theme.lane
                        border.color: Theme.borderSoft
                        border.width: 1
                        Rectangle { width: pane.gutterWidth; anchors.top: parent.top; anchors.bottom: parent.bottom; color: Theme.surfaceRaised; opacity: 0.72 }
                        Rectangle { x: pane.gutterWidth; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 1; color: Theme.border; opacity: 0.9 }
                        Text { x: 16; y: 18; text: modelData.kind === "video" ? "VIDEO" : "AUDIO"; color: modelData.kind === "video" ? Theme.waveform : Theme.waveformPending; font.pixelSize: Theme.fontSizeXS; font.bold: true; font.letterSpacing: 1.2 }
                        Text { x: 16; y: 35; text: modelData.label; color: Theme.text; font.pixelSize: Theme.fontSizeS; font.bold: true; elide: Text.ElideRight; width: pane.gutterWidth - 24 }
                    }
                }
                Repeater {
                    model: controller.timelineClips
                    delegate: ClipBlock {
                        z: 2
                        visible: model.inViewport
                        y: model.trackIndex * pane.trackLaneHeight + 9
                        canonicalStartX: pane.gutterWidth + model.startProgress * (contentArea.width - pane.gutterWidth)
                        canonicalWidth: Math.max(3, (model.endProgress - model.startProgress) * (contentArea.width - pane.gutterWidth))
                        height: pane.trackLaneHeight - 14
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
