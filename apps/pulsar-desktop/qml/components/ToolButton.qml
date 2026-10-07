import QtQuick
import "../theme/Theme.js" as Theme

// Toolbar action: glyph + short label, flat until hover, delayed shortcut tooltip.
Rectangle {
    id: tool
    property string glyph: "split"
    property alias text: label.text
    property string tip: ""
    property bool toggle: false     // behaves as a switch: dimmed text when off
    property bool checked: false   // latched tool (e.g. Snap): accent while on
    signal clicked()
    activeFocusOnTab: true
    Keys.onReturnPressed: tool.clicked()
    Keys.onEnterPressed: tool.clicked()

    implicitHeight: Theme.controlHeightSmall
    implicitWidth: row.implicitWidth + 2 * Theme.spaceM
    radius: Theme.radiusM
    opacity: enabled ? 1.0 : 0.38
    color: mouse.pressed ? Theme.bgPressed : (mouse.containsMouse ? Theme.bgHover : (checked ? Theme.accentDim : "transparent"))
    border.width: (tool.activeFocus || checked) ? 1 : 0
    border.color: tool.activeFocus ? Theme.focus : Theme.accent

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6
        TGlyph { anchors.verticalCenter: parent.verticalCenter; width: 16; height: 16; kind: tool.glyph; ink: tool.checked ? Theme.accentHover : (tool.toggle ? Theme.textSecondary : Theme.textPrimary) }
        Text { id: label; anchors.verticalCenter: parent.verticalCenter; color: (tool.toggle && !tool.checked) ? Theme.textSecondary : Theme.textPrimary; font.pixelSize: Theme.fontSizeS }
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
        onClicked: { tip.visible = false; if (!tool.toggle) tool.forceActiveFocus(); tool.clicked() }
    }
}
