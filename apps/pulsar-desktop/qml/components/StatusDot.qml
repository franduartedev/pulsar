import QtQuick
import "../theme/Theme.js" as Theme

// Status dot. Tones: "idle" | "live" | "ok" | "warn" | "error".
Rectangle {
    property string tone: "idle"

    width: 7
    height: 7
    radius: 3.5
    color: tone === "error" ? Theme.error
         : tone === "warn" ? Theme.warning
         : tone === "ok" ? Theme.success
         : tone === "live" ? Theme.accent : Theme.textMuted
    opacity: tone === "idle" ? 0.6 : 1.0
}
