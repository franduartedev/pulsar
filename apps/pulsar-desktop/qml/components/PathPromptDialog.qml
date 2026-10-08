import QtQuick
import "../theme/Theme.js" as Theme

// "Where to save?" prompt for Guardar / Guardar como / Exportar. A typed path with a
// sensible default works on every desktop; the toolkit's own file dialog cannot create a new
// file name when no native dialog is available (Qt 6.4), so it is not used for saving.
Item {
    id: dialog
    property string title: ""
    property string confirmText: "Guardar"
    property string extension: ""       // ".pulsar" / ".mp4": appended when the name has none
    property string problem: ""
    property string notice: ""
    property bool showPresets: false      // export only: size preset
    property string preset: "1080p"
    signal accepted(string path)
    signal rejected()

    visible: false
    anchors.fill: parent
    z: 1000

    function openWith(suggested) {
        field.text = suggested
        refresh()
        visible = true
        field.forceActiveFocus()
        field.selectAll()   // typing a bare name replaces the suggestion and lands in the same folder
    }
    function close() { visible = false }
    function currentPath() { return controller.normalizeSavePath(field.text, dialog.extension) }
    function refresh() {
        var path = currentPath()
        problem = controller.validateSavePath(path)
        notice = (problem.length === 0 && controller.pathExists(path)) ? "Ya existe un archivo con ese nombre: se va a reemplazar." : ""
    }
    function accept() {
        refresh()
        if (problem.length > 0) return
        var path = currentPath()
        close()
        accepted(path)
    }

    Rectangle { anchors.fill: parent; color: "#99000000" }
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; hoverEnabled: true }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(560, parent.width - 48)
        height: column.implicitHeight + 2 * Theme.spaceXL
        radius: Theme.radiusL
        color: Theme.bgElevated
        border.color: Theme.borderStrong

        Column {
            id: column
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            anchors.margins: Theme.spaceXL
            spacing: Theme.spaceM
            Text { width: parent.width; text: dialog.title; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeXL; font.bold: true }
            Text { width: parent.width; text: "Ruta del archivo"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS }
            Rectangle {
                width: parent.width; height: 34
                radius: Theme.radiusM
                color: Theme.bgInput
                border.width: 1
                border.color: dialog.problem.length > 0 ? Theme.error : (field.activeFocus ? Theme.focus : Theme.borderStrong)
                TextInput {
                    id: field
                    anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10
                    verticalAlignment: TextInput.AlignVCenter
                    color: Theme.textPrimary
                    selectedTextColor: Theme.textOnAccent
                    selectionColor: Theme.accent
                    font.pixelSize: Theme.fontSizeM
                    clip: true
                    selectByMouse: true
                    onTextChanged: dialog.refresh()
                    Keys.onReturnPressed: dialog.accept()
                    Keys.onEnterPressed: dialog.accept()
                    Keys.onEscapePressed: { dialog.close(); dialog.rejected() }
                }
            }
            Row {
                visible: dialog.showPresets
                spacing: Theme.spaceS
                Text { anchors.verticalCenter: parent.verticalCenter; text: "Tamaño"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS; rightPadding: 4 }
                Repeater {
                    model: [{id: "original", label: "Original"}, {id: "1080p", label: "1080p"}, {id: "720p", label: "720p"}]
                    delegate: PButton { height: 30; text: modelData.label; primary: dialog.preset === modelData.id; onClicked: dialog.preset = modelData.id }
                }
            }
            Text { visible: dialog.showPresets; width: parent.width; wrapMode: Text.WordWrap; text: dialog.showPresets ? controller.exportSizeNote(dialog.preset) : ""; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
            Text { visible: dialog.problem.length > 0; width: parent.width; wrapMode: Text.WordWrap; text: dialog.problem; color: Theme.error; font.pixelSize: Theme.fontSizeS }
            Text { visible: dialog.notice.length > 0; width: parent.width; wrapMode: Text.WordWrap; text: dialog.notice; color: Theme.warning; font.pixelSize: Theme.fontSizeS }
            Item { width: 1; height: Theme.spaceS }
            Row {
                spacing: Theme.spaceS
                layoutDirection: Qt.RightToLeft
                width: parent.width
                PButton { primary: true; height: 34; text: dialog.confirmText; enabled: dialog.problem.length === 0 && field.text.length > 0; onClicked: dialog.accept() }
                PButton { height: 34; text: "Cancelar"; onClicked: { dialog.close(); dialog.rejected() } }
            }
        }
    }
}
