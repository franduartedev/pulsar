import QtQuick
import "../theme/Theme.js" as Theme

// Transport strip attached to the monitor module: centered control cluster
// with a dominant play action, strong current-position timecode at right.
// Lives inside the monitor module frame composed by Main.qml.
Item {
    id: bar

    Text { anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: "SOURCE"; color: Theme.faint; font.pixelSize: Theme.fontSizeS; font.letterSpacing: 1.5 }
    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
        TButton { glyph: "home"; enabled: controller.hasMedia; onClicked: controller.seekToStart() }
        TButton { glyph: controller.playing ? "pause" : "play"; dominant: true; enabled: controller.hasMedia && !controller.busy; onClicked: controller.togglePlayback() }
        TButton { glyph: "end"; enabled: controller.hasMedia; onClicked: controller.seekToEnd() }
    }
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
        Text { anchors.verticalCenter: parent.verticalCenter; text: controller.positionText; color: controller.hasMedia ? Theme.textBright : Theme.muted; font.family: "monospace"; font.pixelSize: Theme.fontSizeTC }
        Text { anchors.verticalCenter: parent.verticalCenter; text: "/ " + controller.durationText; color: Theme.faint; font.family: "monospace"; font.pixelSize: Theme.fontSizeM }
    }
}
