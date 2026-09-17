import QtQuick
import "../theme/Theme.js" as Theme

Rectangle {
    id: button
    property alias text: label.text
    property bool enabled: true
    signal clicked()

    implicitHeight: 28
    radius: 3
    color: enabled ? (mouse.pressed ? Theme.buttonPressed
                                    : mouse.containsMouse ? Theme.buttonHover
                                                          : Theme.buttonBase)
                   : Theme.buttonDisabled
    border.color: enabled ? Theme.border : Theme.borderSubtle

    Text {
        id: label
        anchors.centerIn: parent
        color: button.enabled ? Theme.text : Theme.muted
        font.pixelSize: Theme.fontSizeM
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: button.enabled
        onClicked: button.clicked()
    }
}
