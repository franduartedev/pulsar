import QtQuick
import "../theme/Theme.js" as Theme

Rectangle {
    id: header
    signal openProjectRequested()
    signal saveProjectRequested()
    signal saveProjectAsRequested()
    signal importMediaRequested()
    color: Theme.header
    border.color: Theme.border
    border.width: 1

    function projectName() {
        var path = controller.activeProjectPath
        if (!path || path.length === 0) return "Proyecto sin guardar"
        var parts = path.split("/")
        return parts[parts.length - 1]
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12
        Rectangle { width: 5; height: 30; radius: 2; color: Theme.accent; anchors.verticalCenter: parent.verticalCenter }
        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Row {
                spacing: 8
                Text { text: "PULSAR"; color: Theme.textBright; font.pixelSize: Theme.fontSizeXL; font.bold: true; font.letterSpacing: 2.4 }
                Text { text: "EDITOR"; color: Theme.accent; font.pixelSize: Theme.fontSizeXS; font.bold: true; font.letterSpacing: 1.5; anchors.bottom: parent.bottom; anchors.bottomMargin: 2 }
            }
            Row {
                spacing: 6
                Text { text: projectName(); color: Theme.text; font.pixelSize: Theme.fontSizeS; elide: Text.ElideMiddle; width: 220 }
                Rectangle { visible: controller.projectDirty; width: 8; height: 8; radius: 4; color: Theme.waveformPending; anchors.verticalCenter: parent.verticalCenter }
                Text { visible: controller.projectDirty; text: "CAMBIOS SIN GUARDAR"; color: Theme.waveformPending; font.pixelSize: Theme.fontSizeXS; font.bold: true; font.letterSpacing: 0.7 }
                Text { visible: !controller.projectDirty && controller.status.indexOf("Proyecto guardado") === 0; text: "GUARDADO"; color: Theme.accent; font.pixelSize: Theme.fontSizeXS; font.bold: true; font.letterSpacing: 0.8 }
            }
        }
    }

    Row {
        id: projectActions
        anchors.right: statusArea.left
        anchors.rightMargin: 18
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6
        PButton { width: 76; text: "ABRIR"; onClicked: header.openProjectRequested() }
        PButton { width: 88; text: "GUARDAR"; primary: true; enabled: controller.projectDirty; onClicked: header.saveProjectRequested() }
        PButton { width: 118; text: "GUARDAR COMO"; onClicked: header.saveProjectAsRequested() }
        Rectangle { width: 1; height: 26; color: Theme.border; anchors.verticalCenter: parent.verticalCenter; opacity: 0.8 }
        PButton { width: 116; text: "IMPORTAR MEDIO"; onClicked: header.importMediaRequested() }
    }

    Row {
        id: statusArea
        anchors.right: parent.right
        anchors.rightMargin: 18
        anchors.verticalCenter: parent.verticalCenter
        spacing: 7
        StatusDot { anchors.verticalCenter: parent.verticalCenter; tone: controller.errorMessage.length > 0 ? "error" : (controller.playing ? "live" : "idle") }
        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1
            Text { text: controller.errorMessage.length > 0 ? "ATENCIÓN" : (controller.playing ? "REPRODUCIENDO" : (controller.busy ? "PROCESANDO" : "LISTO")); color: controller.errorMessage.length > 0 ? Theme.danger : Theme.muted; font.pixelSize: Theme.fontSizeXS; font.bold: true; font.letterSpacing: 0.8 }
            Text { text: controller.errorMessage.length > 0 ? controller.errorMessage : controller.status; color: controller.errorMessage.length > 0 ? Theme.danger : Theme.faint; font.pixelSize: Theme.fontSizeXS; elide: Text.ElideRight; width: 86 }
        }
    }
    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 2; color: Theme.accent; opacity: 0.28 }
}
