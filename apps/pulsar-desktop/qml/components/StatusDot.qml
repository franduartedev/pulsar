import QtQuick
import "../theme/Theme.js" as Theme

// Small workstation status dot. Tones: "idle" | "live" | "error".
Rectangle {
    property string tone: "idle"

    width: 7
    height: 7
    radius: 3.5
    color: tone === "error" ? Theme.danger : tone === "live" ? Theme.accent : Theme.muted
    opacity: tone === "idle" ? 0.55 : 1.0
}
