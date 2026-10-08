import QtQuick
import "../theme/Theme.js" as Theme

// One quiet line: shortcuts on the left, the single global status on the right.
Item {
    id: bar
    signal aboutRequested()
    readonly property bool hasError: controller.errorMessage.length > 0
    readonly property string statusText: {
        if (hasError) return controller.errorMessage
        if (controller.exportRunning) return "Exportando " + Math.round(controller.exportProgress * 100) + "%"
        if (controller.busy) return "Procesando…"
        if (controller.status.indexOf("Proyecto guardado") === 0) return controller.status
        if (controller.exportStatus.length > 0) {
            var path = controller.exportDestination
            var name = path.length > 0 ? path.substring(path.lastIndexOf("/") + 1) : ""
            return controller.exportStatus.indexOf("completada") >= 0 && name.length > 0 ? "Video exportado: " + name : controller.exportStatus
        }
        return controller.playing ? "Reproduciendo" : "Listo"
    }

    Text {
        anchors.left: parent.left; anchors.leftMargin: Theme.spaceM
        anchors.verticalCenter: parent.verticalCenter
        text: "Espacio  Reproducir   ·   S  Cortar   ·   T  Título   ·   Ctrl+D  Duplicar   ·   Supr  Borrar   ·   Ctrl+Z  Deshacer   ·   Ctrl+S  Guardar"
        color: Theme.textMuted
        font.pixelSize: Theme.fontSizeXS
        elide: Text.ElideRight
        width: Math.max(0, parent.width - statusRow.width - versionText.width - 4 * Theme.spaceL)
    }
    Text {
        id: versionText
        anchors.right: parent.right; anchors.rightMargin: Theme.spaceM
        anchors.verticalCenter: parent.verticalCenter
        text: controller.appVersion
        color: Theme.textMuted
        font.pixelSize: Theme.fontSizeXS
        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: bar.aboutRequested() }
    }
    Row {
        id: statusRow
        anchors.right: versionText.left; anchors.rightMargin: Theme.spaceL
        anchors.verticalCenter: parent.verticalCenter
        spacing: 7
        StatusDot { anchors.verticalCenter: parent.verticalCenter; tone: bar.hasError ? "error" : (controller.exportRunning ? "live" : "ok") }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: bar.statusText
            color: bar.hasError ? Theme.error : Theme.textSecondary
            font.pixelSize: Theme.fontSizeS
            elide: Text.ElideRight
            width: Math.min(implicitWidth, Math.max(120, bar.width * 0.45))
        }
    }
    Rectangle {
        visible: controller.exportRunning
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 2; color: Theme.track
        Rectangle { width: parent.width * controller.exportProgress; height: parent.height; color: Theme.accent }
    }
}
