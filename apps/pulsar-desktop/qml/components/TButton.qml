import QtQuick
import "../theme/Theme.js" as Theme

// Designed transport button. Play uses dominant: true for a larger,
// accent-framed primary action. Glyphs come from TGlyph.
Rectangle {
    id: btn
    property string glyph: "play"
    property bool dominant: false
    signal clicked()

    width: dominant ? 58 : 44
    height: 38
    radius: Theme.radiusS
    color: {
        if (!enabled) {
            return Theme.buttonDisabled
        }
        if (mouse.pressed) {
            return dominant ? Theme.accent : Theme.buttonPressed
        }
        if (mouse.containsMouse) {
            return dominant ? Theme.accentHover : Theme.buttonHover
        }
        return dominant ? Theme.accentDim : Theme.buttonBase
    }
    border.color: {
        if (!enabled) {
            return Theme.borderSoft
        }
        return dominant ? Theme.accent : Theme.border
    }

    TGlyph {
        anchors.centerIn: parent
        kind: btn.glyph
        ink: {
            if (!btn.enabled) {
                return Theme.muted
            }
            return btn.dominant ? Theme.textBright : Theme.text
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: btn.clicked()
    }
}
