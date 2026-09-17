import QtQuick
import "../theme/Theme.js" as Theme

// Subtle workstation footer: hints left, technical state right.
Rectangle {
    color: Theme.header
    border.color: Theme.borderSoft
    radius: Theme.radiusS
    height: 24

    Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: "Espacio · Play/Pausa      Ctrl+I · Importar      Inicio/Fin · Buscar"; color: Theme.faint; font.pixelSize: Theme.fontSizeS }
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        spacing: 7
        StatusDot { anchors.verticalCenter: parent.verticalCenter; tone: controller.errorMessage.length > 0 ? "error" : "idle" }
        Text { anchors.verticalCenter: parent.verticalCenter; text: controller.errorMessage.length > 0 ? controller.errorMessage : controller.technicalStatus; color: controller.errorMessage.length > 0 ? Theme.danger : Theme.muted; font.pixelSize: Theme.fontSizeS; elide: Text.ElideRight }
    }
}
