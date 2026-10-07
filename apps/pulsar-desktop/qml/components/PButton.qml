import QtQuick
import "../theme/Theme.js" as Theme

// Text button. Three weights:
//   primary: accent fill (one per surface, e.g. Exportar)
//   default: elevated fill (secondary actions)
//   flat:    no fill until hover (header / toolbar actions)
Rectangle {
    id: button
    property alias text: label.text
    property bool primary: false
    property bool flat: false
    signal clicked()
    activeFocusOnTab: true
    Keys.onReturnPressed: button.clicked()
    Keys.onEnterPressed: button.clicked()

    implicitHeight: Theme.controlHeight
    implicitWidth: label.implicitWidth + 2 * Theme.spaceL
    radius: Theme.radiusM
    opacity: enabled ? 1.0 : 0.42
    color: {
        if (primary) {
            return mouse.pressed ? Theme.accentPressed : (mouse.containsMouse ? Theme.accentHover : Theme.accent)
        }
        if (mouse.pressed) {
            return Theme.bgPressed
        }
        if (mouse.containsMouse) {
            return Theme.bgHover
        }
        return flat ? "transparent" : Theme.bgElevated
    }
    border.width: button.activeFocus ? 1 : 0
    border.color: Theme.focus

    Text {
        id: label
        anchors.centerIn: parent
        color: button.primary ? Theme.textOnAccent : Theme.textPrimary
        font.pixelSize: Theme.fontSizeM
        font.bold: button.primary
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: { button.forceActiveFocus(); button.clicked() }
    }
}
