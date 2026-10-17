import QtQuick
import "../theme/Theme.js" as Theme

// Joined option group (one choice at a time). `model` is a list of labels; `current` the active index
// (-1 = none). Emits activated(index) only on a user click.
Rectangle {
    id: seg
    property var model: []
    property var tips: []
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
                id: segment
                width: (row.width) / Math.max(1, seg.model.length); height: row.height
                radius: Theme.radiusM - 1
                color: seg.current === index ? Theme.accentDim : (cell.containsMouse ? Theme.bgHover : "transparent")
                readonly property string tipText: index < seg.tips.length ? seg.tips[index] : ""
                Accessible.name: tipText.length > 0 ? tipText : modelData
                Text { anchors.centerIn: parent; text: modelData; font.pixelSize: Theme.fontSizeS
                       color: seg.current === index ? Theme.focus : Theme.textSecondary; font.bold: seg.current === index }
                Timer { id: tipDelay; interval: 600; onTriggered: tip.visible = segment.tipText.length > 0 && cell.containsMouse }
                Rectangle {
                    id: tip
                    visible: false; z: 100; y: segment.height + 5
                    x: index === seg.model.length - 1 ? segment.width - width : 0
                    width: tipLabel.implicitWidth + 16; height: 24; radius: Theme.radiusS
                    color: Theme.bgElevated; border.color: Theme.borderStrong
                    Text { id: tipLabel; anchors.centerIn: parent; text: segment.tipText; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeXS }
                }
                MouseArea {
                    id: cell; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onContainsMouseChanged: { tip.visible = false; if (containsMouse) tipDelay.restart(); else tipDelay.stop() }
                    onClicked: { tip.visible = false; seg.activated(index) }
                }
            }
        }
    }
}
