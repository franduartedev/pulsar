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
    signal aboutRequested()
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
        BrandMark { size: 24; anchors.verticalCenter: parent.verticalCenter }
        Text { anchors.verticalCenter: parent.verticalCenter; text: "PULSAR"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeXL; font.bold: true; font.letterSpacing: 2.4
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: header.aboutRequested() } }
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
        PButton { visible: header.width >= 1120; flat: true; text: "Guardar como…"; onClicked: header.saveProjectAsRequested() }
        PButton { visible: header.width >= 1040; flat: true; text: "Importar"; onClicked: header.importMediaRequested() }
        Item { width: Theme.spaceM; height: 1 }
        PButton { text: "Cancelar"; visible: controller.exportRunning; onClicked: controller.cancelExport() }
        PButton {
            width: 136; height: 34
            primary: true
            text: controller.exportRunning ? "Exportando…" : "Exportar video"
            enabled: controller.hasVideoMedia && !controller.exportRunning
            onClicked: header.exportRequested()
        }
    }

    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.borderSubtle }
}
