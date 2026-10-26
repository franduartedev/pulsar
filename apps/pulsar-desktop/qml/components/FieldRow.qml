import QtQuick
import "../theme/Theme.js" as Theme

// Inspector row: a fixed label column and the control(s) to its right, vertically centred.
Item {
    id: row
    property alias label: caption.text
    // Give longer Spanish labels room on wide inspectors while retaining a
    // compact minimum for narrow Windows/Linux windows.
    readonly property real labelWidth: Math.min(96, Math.max(62, width * 0.28))
    default property alias content: slot.data
    width: parent ? parent.width : 0
    // Grows with wrapped content (e.g. a Flow of buttons) instead of overlapping the rows below.
    height: Math.max(Theme.controlHeightSmall + 2, slot.implicitHeight + 2)
    Text { id: caption; width: row.labelWidth; anchors.verticalCenter: parent.verticalCenter; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS; elide: Text.ElideRight }
    Row { id: slot; anchors.left: parent.left; anchors.leftMargin: row.labelWidth + 4; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; spacing: 4 }
}
