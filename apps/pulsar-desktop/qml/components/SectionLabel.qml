import QtQuick
import "../theme/Theme.js" as Theme

// Small caps-style label for panel sections (INSPECTOR / AUDIO / ...).
// Use sparingly and with short words.
Text {
    color: Theme.textSecondary
    font.pixelSize: Theme.fontSizeXS
    font.bold: true
    font.letterSpacing: 1.2
    elide: Text.ElideRight
}
