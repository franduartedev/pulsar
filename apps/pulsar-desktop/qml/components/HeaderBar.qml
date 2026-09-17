import QtQuick
import "../theme/Theme.js" as Theme

// Thin workspace chrome: wordmark left, small app state right.
Rectangle {
    color: Theme.header
    border.color: Theme.borderSoft
    height: 42

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10
        Text { anchors.verticalCenter: parent.verticalCenter; text: "PULSAR"; color: Theme.text; font.pixelSize: 16; font.bold: true; font.letterSpacing: 2 }
        Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 1; height: 13; color: Theme.border }
        Text { anchors.verticalCenter: parent.verticalCenter; text: "EDIT SUITE"; color: Theme.muted; font.pixelSize: Theme.fontSizeS; font.letterSpacing: 1.5 }
    }
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
        StatusDot { anchors.verticalCenter: parent.verticalCenter; tone: controller.errorMessage.length > 0 ? "error" : (controller.busy || controller.playing ? "live" : "idle") }
        Text { anchors.verticalCenter: parent.verticalCenter; text: controller.status; color: controller.errorMessage.length > 0 ? Theme.danger : Theme.muted; font.pixelSize: Theme.fontSizeM }
    }
    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.accent; opacity: 0.22 }
}
