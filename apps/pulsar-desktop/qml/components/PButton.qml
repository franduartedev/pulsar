import QtQuick
import "../theme/Theme.js" as Theme

Rectangle {
    id: button
    property alias text: label.text
    property bool primary: false
    signal clicked()
    activeFocusOnTab: true
    Keys.onReturnPressed: button.clicked()
    Keys.onEnterPressed: button.clicked()

    implicitHeight: Theme.controlHeight
    radius: Theme.radiusS
    color: {
        if (!enabled) {
            return Theme.buttonDisabled
        }
        if (primary) {
            if (mouse.pressed) {
                return Theme.accent
            }
            return mouse.containsMouse ? Theme.accentHover : Theme.accentDim
        }
        if (mouse.pressed) {
            return Theme.buttonPressed
        }
        return mouse.containsMouse ? Theme.buttonHover : Theme.buttonBase
    }
    border.color: {
        if (!enabled) {
            return Theme.borderSoft
        }
        return button.activeFocus ? Theme.focus : (primary ? Theme.accent : (mouse.containsMouse ? Theme.edge : Theme.border))
    }

    Text {
        id: label
        anchors.centerIn: parent
        color: {
            if (!button.enabled) {
                return Theme.muted
            }
            return button.primary ? Theme.textBright : Theme.text
        }
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
