import QtQuick
import "../theme/Theme.js" as Theme

// Transport icon button. Play uses dominant: true and takes the accent fill
// while playing (active state); otherwise it stays neutral.
Rectangle {
    id: btn
    property string glyph: "play"
    property bool dominant: false
    property bool active: false
    signal clicked()
    activeFocusOnTab: true
    Keys.onReturnPressed: btn.clicked()
    Keys.onEnterPressed: btn.clicked()

    width: dominant ? 48 : 36
    height: 32
    radius: Theme.radiusM
    opacity: enabled ? 1.0 : 0.42
    color: {
        if (active) {
            return mouse.pressed ? Theme.accentPressed : (mouse.containsMouse ? Theme.accentHover : Theme.accent)
        }
        if (mouse.pressed) {
            return Theme.bgPressed
        }
        if (mouse.containsMouse) {
            return Theme.bgHover
        }
        return dominant ? Theme.bgElevated : "transparent"
    }
    border.width: btn.activeFocus ? 1 : 0
    border.color: Theme.focus

    TGlyph {
        anchors.centerIn: parent
        kind: btn.glyph
        ink: btn.active ? Theme.textOnAccent : Theme.textPrimary
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: { btn.forceActiveFocus(); btn.clicked() }
    }
}
