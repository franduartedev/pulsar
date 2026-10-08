import QtQuick
import "../theme/Theme.js" as Theme

// Inspector row: a fixed label column and the control(s) to its right, vertically centred.
Item {
    id: row
    property alias label: caption.text
    default property alias content: slot.data
    width: parent ? parent.width : 0
    height: Theme.controlHeightSmall + 2
    Text { id: caption; width: 62; anchors.verticalCenter: parent.verticalCenter; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS; elide: Text.ElideRight }
    Row { id: slot; anchors.left: parent.left; anchors.leftMargin: 66; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; spacing: 4 }
}
