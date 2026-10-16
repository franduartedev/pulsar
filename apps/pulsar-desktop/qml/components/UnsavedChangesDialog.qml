import QtQuick
import "../theme/Theme.js" as Theme

// Modal confirmation shown before anything replaces or closes a project that
// has unsaved changes. It decides nothing: it only reports the user's choice.
Item {
    id: dialog
    signal saveRequested()
    signal discardRequested()
    signal cancelRequested()

    visible: false
    anchors.fill: parent
    z: 1000

    function open() { visible = true; saveButton.forceActiveFocus() }
    function close() { visible = false }

    // Scrim: blocks the editor behind the dialog.
    Rectangle { anchors.fill: parent; color: Theme.overlayScrim }
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; hoverEnabled: true }
    Keys.onEscapePressed: dialog.cancelRequested()

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(440, parent.width - 48)
        height: column.implicitHeight + 2 * Theme.spaceXL
        radius: Theme.radiusL
        color: Theme.bgElevated
        border.color: Theme.borderStrong

        Column {
            id: column
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            anchors.margins: Theme.spaceXL
            spacing: Theme.spaceM
            Text { width: parent.width; text: "Hay cambios sin guardar."; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeXL; font.bold: true }
            Text { width: parent.width; wrapMode: Text.WordWrap; text: "Si continuás sin guardar, los cambios hechos en este proyecto se van a perder."; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeM }
            Item { width: 1; height: Theme.spaceS }
            Row {
                spacing: Theme.spaceS
                layoutDirection: Qt.RightToLeft
                width: parent.width
                PButton { id: saveButton; primary: true; height: 34; text: "Guardar"; onClicked: dialog.saveRequested() }
                PButton { height: 34; text: "Descartar"; onClicked: dialog.discardRequested() }
                PButton { height: 34; text: "Cancelar"; onClicked: dialog.cancelRequested() }
            }
        }
    }
}
