import QtQuick
import "../theme/Theme.js" as Theme

// "Placa de color": pick the colour of a new 5 s card. Plain QML overlay (no Controls module), like
// the clip menu. The card goes in at the playhead (or at the end of VIDEO 1 when that is taken).
Item {
    id: menu
    anchors.fill: parent
    visible: false
    z: 900

    readonly property var palette: [
        {name: "Negro", color: "#000000"}, {name: "Blanco", color: "#ffffff"},
        {name: "Grafito", color: "#1a1f29"}, {name: "Gris", color: "#7b8493"},
        {name: "Azul", color: "#4d8dff"}, {name: "Verde azulado", color: "#2ec4b6"},
        {name: "Rojo", color: "#f26d6d"}, {name: "Amarillo", color: "#f2c14e"},
        {name: "Violeta", color: "#7a5cff"}, {name: "Rosa", color: "#ff7ab6"},
        {name: "Verde", color: "#3fcf8e"}, {name: "Naranja", color: "#ff8a3d"}
    ]
    property string hovered: ""

    function openAt(sx, sy) {
        card.x = Math.max(4, Math.min(sx, menu.width - card.width - 4))
        card.y = Math.max(4, Math.min(sy, menu.height - card.height - 4))
        hovered = ""
        visible = true
    }
    function close() { visible = false }

    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onPressed: menu.close() }
    focus: visible
    Keys.onEscapePressed: menu.close()

    Rectangle {
        id: card
        width: 232
        height: content.implicitHeight + 2 * Theme.spaceM
        radius: Theme.radiusM
        color: Theme.bgElevated
        border.color: Theme.borderStrong
        MouseArea { anchors.fill: parent }   // clicks on the card do not dismiss it
        Column {
            id: content
            x: Theme.spaceM; y: Theme.spaceM
            width: parent.width - 2 * Theme.spaceM
            spacing: Theme.spaceS
            Text { text: "Placa de color · 5 s"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeM; font.bold: true }
            Text { width: parent.width; wrapMode: Text.WordWrap; text: "Sirve para intros, cierres y fondos de títulos. Se agrega en el cabezal."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
            Grid {
                columns: 4
                spacing: 6
                Repeater {
                    model: menu.palette
                    delegate: Rectangle {
                        width: 46; height: 34; radius: Theme.radiusS
                        color: modelData.color
                        border.width: swatchMouse.containsMouse ? 2 : 1
                        border.color: swatchMouse.containsMouse ? Theme.accent : "#44ffffff"
                        MouseArea {
                            id: swatchMouse
                            anchors.fill: parent; hoverEnabled: true
                            onContainsMouseChanged: if (containsMouse) menu.hovered = modelData.name
                            onClicked: { menu.close(); controller.createColorCard(modelData.color) }
                        }
                    }
                }
            }
            Text { text: menu.hovered.length > 0 ? menu.hovered : " "; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeXS }
        }
    }
}
