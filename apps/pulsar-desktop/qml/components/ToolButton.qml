import QtQuick
import "../theme/Theme.js" as Theme

// Toolbar action: glyph + short label, flat until hover, delayed shortcut tooltip.
Rectangle {
    id: tool
    property string glyph: "split"
    property alias text: label.text
    property string tip: ""
    signal clicked()
    activeFocusOnTab: true
    Keys.onReturnPressed: tool.clicked()
    Keys.onEnterPressed: tool.clicked()

    implicitHeight: Theme.controlHeightSmall
    implicitWidth: row.implicitWidth + 2 * Theme.spaceM
    radius: Theme.radiusM
    opacity: enabled ? 1.0 : 0.38
    color: mouse.pressed ? Theme.bgPressed : (mouse.containsMouse ? Theme.bgHover : "transparent")
    border.width: tool.activeFocus ? 1 : 0
    border.color: Theme.focus

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6
        TGlyph { anchors.verticalCenter: parent.verticalCenter; width: 16; height: 16; kind: tool.glyph; ink: Theme.textPrimary }
        Text { id: label; anchors.verticalCenter: parent.verticalCenter; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeS }
    }

    Timer { id: delay; interval: 600; onTriggered: tip.visible = tool.tip.length > 0 && mouse.containsMouse }
    Rectangle {
        id: tip
        visible: false
        z: 100
        y: tool.height + 6
        x: 0
        width: tipText.implicitWidth + 16
        height: 24
        radius: Theme.radiusS
        color: Theme.bgElevated
        border.color: Theme.borderStrong
        Text { id: tipText; anchors.centerIn: parent; text: tool.tip; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeXS }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onContainsMouseChanged: { tip.visible = false; if (containsMouse) delay.restart(); else delay.stop() }
        onClicked: { tip.visible = false; tool.forceActiveFocus(); tool.clicked() }
    }
}
