import QtQuick
import "../theme/Theme.js" as Theme

// The PULSAR "Pulse Arc" mark, the official identity artwork (qml/assets/pulse_arc_mark.png,
// transparent background). The ONLY place the product symbol is drawn: header, About box and any
// future splash follow. `size` is the height of the symbol in pixels.
Item {
    id: mark
    property real size: 18
    width: size * 289 / 197; height: size
    Image {
        anchors.fill: parent
        source: "qrc:/qml/assets/pulse_arc_mark.png"
        fillMode: Image.PreserveAspectFit
        smooth: true; mipmap: true
        sourceSize.height: Math.ceil(mark.size * 2)
    }
}
