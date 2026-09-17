import QtQuick
import "../theme/Theme.js" as Theme

Rectangle {
    id: block
    property string clipKey: ""
    property string clipName: ""
    property string startText: ""
    property string endText: ""
    property bool missing: false
    property bool selected: false
    signal activated()
    color: missing ? "#3a2024" : (selected ? Theme.accentDim : Theme.buttonBase)
    border.color: missing ? Theme.danger : (selected ? Theme.accent : Theme.border)
    border.width: selected ? 2 : 1
    radius: 3
    Text { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 7; text: block.clipName; color: Theme.textBright; font.pixelSize: Theme.fontSizeS; elide: Text.ElideRight }
    Text { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 7; text: block.missing ? "OFFLINE" : block.startText; color: block.missing ? Theme.danger : Theme.muted; font.pixelSize: Theme.fontSizeXS; font.family: "monospace" }
    MouseArea { anchors.fill: parent; onClicked: block.activated() }
}
