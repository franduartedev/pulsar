import QtQuick
import "../theme/Theme.js" as Theme

Rectangle {
    id: button
    property alias text: label.text
    property bool primary: false
    signal clicked()

    implicitHeight: 28
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
        return primary ? Theme.accent : Theme.border
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
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: button.clicked()
    }
}
