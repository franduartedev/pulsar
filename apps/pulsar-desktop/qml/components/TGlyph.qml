import QtQuick
import "../theme/Theme.js" as Theme

// Static vector-style transport glyph. Kinds: "home" | "play" | "pause" | "end".
// Canvas is part of QtQuick itself: no new modules, painted once, no animation.
Item {
    id: glyph
    property string kind: "play"
    property color ink: Theme.textBright

    implicitWidth: 18
    implicitHeight: 18

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var w = width
            var h = height
            ctx.fillStyle = glyph.ink
            if (glyph.kind === "play") {
                ctx.beginPath()
                ctx.moveTo(w * 0.30, h * 0.22)
                ctx.lineTo(w * 0.30, h * 0.78)
                ctx.lineTo(w * 0.78, h * 0.50)
                ctx.closePath()
                ctx.fill()
            } else if (glyph.kind === "pause") {
                ctx.fillRect(w * 0.28, h * 0.22, w * 0.16, h * 0.56)
                ctx.fillRect(w * 0.56, h * 0.22, w * 0.16, h * 0.56)
            } else if (glyph.kind === "home") {
                ctx.fillRect(w * 0.18, h * 0.22, w * 0.12, h * 0.56)
                ctx.beginPath()
                ctx.moveTo(w * 0.74, h * 0.22)
                ctx.lineTo(w * 0.74, h * 0.78)
                ctx.lineTo(w * 0.38, h * 0.50)
                ctx.closePath()
                ctx.fill()
            } else if (glyph.kind === "end") {
                ctx.beginPath()
                ctx.moveTo(w * 0.26, h * 0.22)
                ctx.lineTo(w * 0.26, h * 0.78)
                ctx.lineTo(w * 0.62, h * 0.50)
                ctx.closePath()
                ctx.fill()
                ctx.fillRect(w * 0.70, h * 0.22, w * 0.12, h * 0.56)
            }
        }
        Component.onCompleted: requestPaint()
    }

    onKindChanged: canvas.requestPaint()
}
