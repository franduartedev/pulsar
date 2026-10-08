import QtQuick
import "../theme/Theme.js" as Theme

// Transport attached to the viewer: timecode left, controls centered.
Item {
    id: bar

    Row {
        id: timecode
        anchors.left: parent.left; anchors.leftMargin: Theme.spaceL
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
        Text { anchors.verticalCenter: parent.verticalCenter; text: controller.positionText; color: controller.hasMedia ? Theme.textPrimary : Theme.textMuted; font.family: Theme.mono; font.pixelSize: Theme.fontSizeTC }
        Text { visible: bar.width > 560; anchors.verticalCenter: parent.verticalCenter; text: "/ " + controller.durationText; color: Theme.textMuted; font.family: Theme.mono; font.pixelSize: Theme.fontSizeM }
    }
    Row {
        id: controls
        x: Math.max(timecode.x + timecode.width + Theme.spaceL, (bar.width - width) / 2)
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6
        TButton { glyph: "home"; enabled: controller.hasMedia; onClicked: controller.seekToStart() }
        TButton { glyph: "step-back"; enabled: controller.hasMedia && !controller.playing; onClicked: controller.stepFrame(-1) }
        TButton { glyph: controller.playing ? "pause" : "play"; dominant: true; active: controller.playing; enabled: controller.hasMedia && !controller.busy; onClicked: controller.togglePlayback() }
        TButton { glyph: "step-forward"; enabled: controller.hasMedia && !controller.playing; onClicked: controller.stepFrame(1) }
        TButton { glyph: "end"; enabled: controller.hasMedia; onClicked: controller.seekToEnd() }
    }
}
