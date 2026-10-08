import QtQuick
import "../theme/Theme.js" as Theme

// Offered at start-up when a snapshot of unsaved work from an earlier session exists.
Item {
    id: dialog
    signal recoverRequested()
    signal discardRequested()

    visible: false
    anchors.fill: parent
    z: 1100

    function open() { visible = true; recoverButton.forceActiveFocus() }
    function close() { visible = false }

    Rectangle { anchors.fill: parent; color: "#99000000" }
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; hoverEnabled: true }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(460, parent.width - 48)
        height: column.implicitHeight + 2 * Theme.spaceXL
        radius: Theme.radiusL
        color: Theme.bgElevated
        border.color: Theme.borderStrong

        Column {
            id: column
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            anchors.margins: Theme.spaceXL
            spacing: Theme.spaceM
            Text { width: parent.width; text: "PULSAR encontró una sesión sin guardar."; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeXL; font.bold: true; wrapMode: Text.WordWrap }
            Text { width: parent.width; wrapMode: Text.WordWrap; text: controller.recoveryDescription; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeM }
            Text { width: parent.width; wrapMode: Text.WordWrap; text: "Recuperar la abre tal como estaba; seguirá marcada como sin guardar. Descartar la elimina."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeS }
            Item { width: 1; height: Theme.spaceS }
            Row {
                spacing: Theme.spaceS
                layoutDirection: Qt.RightToLeft
                width: parent.width
                PButton { id: recoverButton; primary: true; height: 34; text: "Recuperar"; onClicked: dialog.recoverRequested() }
                PButton { height: 34; text: "Descartar"; onClicked: dialog.discardRequested() }
            }
        }
    }
}
