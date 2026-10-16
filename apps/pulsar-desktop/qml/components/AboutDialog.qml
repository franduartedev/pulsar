import QtQuick
import "../theme/Theme.js" as Theme

// "Acerca de PULSAR": product, version and credits. Opened from the wordmark or the version label.
Item {
    id: dialog
    visible: false
    anchors.fill: parent
    z: 1100

    function open() { visible = true; closeButton.forceActiveFocus() }
    function close() { visible = false }

    Rectangle { anchors.fill: parent; color: Theme.overlayScrim }
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onClicked: dialog.close() }
    focus: visible
    Keys.onEscapePressed: dialog.close()

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 48)
        height: column.implicitHeight + 2 * Theme.spaceXL
        radius: Theme.radiusL
        color: Theme.bgElevated
        border.color: Theme.borderStrong
        MouseArea { anchors.fill: parent }
        Column {
            id: column
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            anchors.margins: Theme.spaceXL
            spacing: Theme.spaceM
            Row {
                spacing: 12
                BrandMark { size: 30; anchors.verticalCenter: parent.verticalCenter }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    Text { text: "PULSAR"; color: Theme.textPrimary; font.pixelSize: 20; font.bold: true; font.letterSpacing: 3 }
                    Text { text: controller.appVersion.replace("PULSAR ", "Versión "); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeM }
                }
            }
            Text { width: parent.width; wrapMode: Text.WordWrap; text: "Editor de video para Linux: importá, cortá, agregá títulos y sonido, y exportá un MP4 listo para subir."; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeM }
            Rectangle { width: parent.width; height: 1; color: Theme.borderSubtle }
            Text { width: parent.width; wrapMode: Text.WordWrap; text: "Créditos: tipografía de los títulos Inter (SIL Open Font License 1.1). Video con FFmpeg, interfaz con Qt 6. Los sonidos incluidos son originales, generados para PULSAR."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeS }
            Text { width: parent.width; text: "© FD Labs"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeS }
            Row {
                layoutDirection: Qt.RightToLeft
                width: parent.width
                PButton { id: closeButton; primary: true; height: 34; text: "Cerrar"; onClicked: dialog.close() }
            }
        }
    }
}
