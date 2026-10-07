import QtQuick
import "../theme/Theme.js" as Theme

// Small right-click menu for a timeline clip. Plain QML (no QtQuick.Controls Menu) so it adds
// no module to the package. Entries are decided when it opens, from the controller's own
// state, and each one calls the same controller operation as its toolbar/inspector twin.
Item {
    id: menu
    anchors.fill: parent
    visible: false
    z: 900
    property var entries: []

    function openAt(sx, sy) {
        var list = []
        if (controller.canSplit) list.push({ id: "split", label: "Cortar en cabezal", hint: "S" })
        if (controller.selectedClipHasAudio)
            list.push({ id: "mute", label: controller.selectedClipMuted ? "Activar audio" : "Silenciar audio", hint: "M" })
        if (controller.canSeparateAudio) list.push({ id: "separate", label: "Separar audio", hint: "" })
        if (controller.selectedClipKey.length > 0) list.push({ id: "delete", label: "Eliminar", hint: "Supr" })
        if (list.length === 0) return
        entries = list
        card.x = Math.max(4, Math.min(sx, menu.width - card.width - 4))
        card.y = Math.max(4, Math.min(sy, menu.height - card.height - 4))
        visible = true
    }
    function close() { visible = false }
    function run(id) {
        close()
        if (id === "split") controller.splitSelectedClip()
        else if (id === "mute") controller.toggleSelectedClipMute()
        else if (id === "separate") controller.separateSelectedAudio()
        else if (id === "delete") controller.deleteSelectedClip()
    }

    // Click anywhere else (or Esc) dismisses it.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onPressed: menu.close()
    }
    focus: visible
    Keys.onEscapePressed: menu.close()

    Rectangle {
        id: card
        width: 196
        height: list.height + 8
        radius: Theme.radiusM
        color: Theme.bgElevated
        border.color: Theme.borderStrong
        Column {
            id: list
            x: 4; y: 4
            width: parent.width - 8
            Repeater {
                model: menu.entries
                delegate: Rectangle {
                    width: list.width; height: 28
                    radius: Theme.radiusS
                    color: itemMouse.containsMouse ? Theme.bgHover : "transparent"
                    Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData.label; color: modelData.id === "delete" ? Theme.error : Theme.textPrimary; font.pixelSize: Theme.fontSizeS }
                    Text { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData.hint; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
                    MouseArea { id: itemMouse; anchors.fill: parent; hoverEnabled: true; onClicked: menu.run(modelData.id) }
                }
            }
        }
    }
}
