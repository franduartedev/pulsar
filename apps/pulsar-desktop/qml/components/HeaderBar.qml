import QtQuick
import "../theme/Theme.js" as Theme

// Project bar: identity + project on the left, secondary actions flat,
// Exportar as the single primary action. Global status lives in StatusBar.
Rectangle {
    id: header
    signal openProjectRequested()
    signal saveProjectRequested()
    signal saveProjectAsRequested()
    signal importMediaRequested()
    signal exportRequested()
    color: Theme.bgRoot

    function projectName() {
        var path = controller.activeProjectPath
        if (!path || path.length === 0) return "Proyecto sin título"
        var parts = path.split("/")
        return parts[parts.length - 1]
    }

    Row {
        id: identity
        anchors.left: parent.left
        anchors.leftMargin: Theme.spaceL
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10
        // Pulse mark: three bars, accent only on the tallest.
        Item {
            anchors.verticalCenter: parent.verticalCenter
            width: 16; height: 18
            Rectangle { x: 0; y: 6; width: 3; height: 8; radius: 1.5; color: Theme.textMuted }
            Rectangle { x: 6.5; y: 0; width: 3; height: 18; radius: 1.5; color: Theme.accent }
            Rectangle { x: 13; y: 4; width: 3; height: 11; radius: 1.5; color: Theme.textMuted }
        }
        Text { anchors.verticalCenter: parent.verticalCenter; text: "PULSAR"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeXL; font.bold: true; font.letterSpacing: 2.4 }
        Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 1; height: 16; color: Theme.borderStrong }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: projectName()
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeM
            elide: Text.ElideMiddle
            width: Math.min(implicitWidth, Math.max(90, header.width - 640))
        }
        Row {
            visible: controller.projectDirty
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 6; height: 6; radius: 3; color: Theme.warning }
            Text { anchors.verticalCenter: parent.verticalCenter; text: "Sin guardar"; color: Theme.warning; font.pixelSize: Theme.fontSizeXS }
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: Theme.spaceL
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4
        PButton { flat: true; text: "Abrir"; onClicked: header.openProjectRequested() }
        PButton { flat: true; text: "Guardar"; enabled: controller.projectDirty; onClicked: header.saveProjectRequested() }
        PButton { flat: true; text: "Guardar como…"; onClicked: header.saveProjectAsRequested() }
        PButton { flat: true; text: "Importar"; onClicked: header.importMediaRequested() }
        Item { width: Theme.spaceM; height: 1 }
        PButton { text: "Cancelar"; visible: controller.exportRunning; onClicked: controller.cancelExport() }
        PButton {
            width: 108; height: 34
            primary: true
            text: controller.exportRunning ? "Exportando…" : "Exportar"
            enabled: controller.hasVideoMedia && !controller.exportRunning
            onClicked: header.exportRequested()
        }
    }

    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.borderSubtle }
}
