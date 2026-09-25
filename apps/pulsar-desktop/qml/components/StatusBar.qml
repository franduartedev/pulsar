import QtQuick
import "../theme/Theme.js" as Theme

// Subtle workstation footer: hints left, technical state right.
Rectangle {
    color: Theme.header
    border.color: Theme.borderSoft
    radius: Theme.radiusS
    height: 28

    Text { anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter; text: controller.exportRunning ? ("EXPORTANDO " + Math.round(controller.exportProgress * 100) + "%") : "ESPACIO reproducir/pausar   ·   CTRL+I importar   ·   CTRL+S guardar   ·   INICIO/FIN navegar"; color: controller.exportRunning ? Theme.accent : Theme.faint; font.pixelSize: Theme.fontSizeXS }
    Rectangle {
        visible: controller.exportRunning
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 2; color: Theme.border
        Rectangle { width: parent.width * controller.exportProgress; height: parent.height; color: Theme.accent }
    }
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 7
        StatusDot { anchors.verticalCenter: parent.verticalCenter; tone: controller.errorMessage.length > 0 ? "error" : "idle" }
        Text { anchors.verticalCenter: parent.verticalCenter; text: controller.errorMessage.length > 0 ? controller.errorMessage : (controller.exportStatus.length > 0 ? controller.exportStatus : controller.technicalStatus); color: controller.errorMessage.length > 0 ? Theme.danger : Theme.muted; font.pixelSize: Theme.fontSizeS; elide: Text.ElideRight }
    }
}
