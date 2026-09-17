import QtQuick
import "../theme/Theme.js" as Theme

Rectangle {
    radius: Theme.radiusS
    color: Theme.panel
    border.color: Theme.borderSoft
    border.width: 1

    // Faint top sheen so stacked panels stop looking like flat boxes.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        height: 1
        color: Theme.text
        opacity: 0.025
    }
}
