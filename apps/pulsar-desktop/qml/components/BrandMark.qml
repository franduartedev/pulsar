import QtQuick
import "../theme/Theme.js" as Theme

// The PULSAR mark. The ONLY place the product symbol is drawn: replace the three bars below (or
// swap in an Image) when the final logo arrives and the header, About box and any future splash
// follow. `size` is the height of the symbol in pixels.
Item {
    id: mark
    property real size: 18
    readonly property real unit: size / 18
    width: 16 * unit; height: size
    Rectangle { x: 0; y: 6 * mark.unit; width: 3 * mark.unit; height: 8 * mark.unit; radius: 1.5 * mark.unit; color: Theme.textMuted }
    Rectangle { x: 6.5 * mark.unit; y: 0; width: 3 * mark.unit; height: 18 * mark.unit; radius: 1.5 * mark.unit; color: Theme.accent }
    Rectangle { x: 13 * mark.unit; y: 4 * mark.unit; width: 3 * mark.unit; height: 11 * mark.unit; radius: 1.5 * mark.unit; color: Theme.textMuted }
}
