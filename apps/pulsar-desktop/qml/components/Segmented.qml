import QtQuick
import "../theme/Theme.js" as Theme

// Joined option group (one choice at a time). `model` is a list of labels; `current` the active index
// (-1 = none). Emits activated(index) only on a user click.
Rectangle {
    id: seg
    property var model: []
    property int current: -1
    signal activated(int index)
    height: Theme.controlHeightSmall
    implicitWidth: row.implicitWidth
    radius: Theme.radiusM
    color: Theme.bgInput
    border.width: 1; border.color: Theme.borderStrong
    Row {
        id: row
        anchors.fill: parent; anchors.margins: 1
        Repeater {
            model: seg.model
            delegate: Rectangle {
                width: (row.width) / Math.max(1, seg.model.length); height: row.height
                radius: Theme.radiusM - 1
                color: seg.current === index ? Theme.accentDim : (cell.containsMouse ? Theme.bgHover : "transparent")
                Text { anchors.centerIn: parent; text: modelData; font.pixelSize: Theme.fontSizeS
                       color: seg.current === index ? Theme.focus : Theme.textSecondary; font.bold: seg.current === index }
                MouseArea { id: cell; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: seg.activated(index) }
            }
        }
    }
}
