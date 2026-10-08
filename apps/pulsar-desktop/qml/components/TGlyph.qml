import QtQuick
import "../theme/Theme.js" as Theme

// Static vector glyph painted once on a Canvas (no icon assets to package).
// Kinds: transport "home" | "play" | "pause" | "end";
//        edit "split" | "trash" | "undo" | "redo" | "fit";
//        media "video" | "audio";  audio state "speaker" | "speaker-off"; "magnet".
Item {
    id: glyph
    property string kind: "play"
    property color ink: Theme.textPrimary

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
            ctx.strokeStyle = glyph.ink
            ctx.lineWidth = 1.4
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            var flip = glyph.kind === "redo"
            function px(x) { return w * (flip ? 1 - x : x) }
            function tri(x1, y1, x2, y2, x3, y3) {
                ctx.beginPath()
                ctx.moveTo(w * x1, h * y1)
                ctx.lineTo(w * x2, h * y2)
                ctx.lineTo(w * x3, h * y3)
                ctx.closePath()
                ctx.fill()
            }
            if (glyph.kind === "play") {
                tri(0.30, 0.22, 0.30, 0.78, 0.78, 0.50)
            } else if (glyph.kind === "pause") {
                ctx.fillRect(w * 0.28, h * 0.22, w * 0.16, h * 0.56)
                ctx.fillRect(w * 0.56, h * 0.22, w * 0.16, h * 0.56)
            } else if (glyph.kind === "home") {
                ctx.fillRect(w * 0.18, h * 0.22, w * 0.12, h * 0.56)
                tri(0.74, 0.22, 0.74, 0.78, 0.38, 0.50)
            } else if (glyph.kind === "end") {
                tri(0.26, 0.22, 0.26, 0.78, 0.62, 0.50)
                ctx.fillRect(w * 0.70, h * 0.22, w * 0.12, h * 0.56)
            } else if (glyph.kind === "split") {
                ctx.beginPath()
                ctx.moveTo(w * 0.5, h * 0.12)
                ctx.lineTo(w * 0.5, h * 0.88)
                ctx.stroke()
                tri(0.40, 0.30, 0.40, 0.70, 0.14, 0.50)
                tri(0.60, 0.30, 0.60, 0.70, 0.86, 0.50)
            } else if (glyph.kind === "trash") {
                ctx.beginPath()
                ctx.moveTo(w * 0.18, h * 0.28); ctx.lineTo(w * 0.82, h * 0.28)
                ctx.moveTo(w * 0.40, h * 0.28); ctx.lineTo(w * 0.40, h * 0.18)
                ctx.lineTo(w * 0.60, h * 0.18); ctx.lineTo(w * 0.60, h * 0.28)
                ctx.moveTo(w * 0.26, h * 0.34); ctx.lineTo(w * 0.31, h * 0.84)
                ctx.lineTo(w * 0.69, h * 0.84); ctx.lineTo(w * 0.74, h * 0.34)
                ctx.stroke()
            } else if (glyph.kind === "undo" || glyph.kind === "redo") {
                ctx.beginPath()
                ctx.moveTo(px(0.34), h * 0.40)
                ctx.lineTo(px(0.64), h * 0.40)
                ctx.quadraticCurveTo(px(0.86), h * 0.40, px(0.86), h * 0.60)
                ctx.quadraticCurveTo(px(0.86), h * 0.78, px(0.64), h * 0.78)
                ctx.lineTo(px(0.36), h * 0.78)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(px(0.12), h * 0.40)
                ctx.lineTo(px(0.36), h * 0.24)
                ctx.lineTo(px(0.36), h * 0.56)
                ctx.closePath()
                ctx.fill()
            } else if (glyph.kind === "fit") {
                // Two bars with arrows pointing inward: fit everything in view.
                ctx.fillRect(w * 0.12, h * 0.22, w * 0.08, h * 0.56)
                ctx.fillRect(w * 0.80, h * 0.22, w * 0.08, h * 0.56)
                tri(0.26, 0.34, 0.26, 0.66, 0.46, 0.50)
                tri(0.74, 0.34, 0.74, 0.66, 0.54, 0.50)
            } else if (glyph.kind === "speaker" || glyph.kind === "speaker-off") {
                // Speaker cone; waves when active, a cross when silenced.
                ctx.beginPath()
                ctx.moveTo(w * 0.12, h * 0.40); ctx.lineTo(w * 0.30, h * 0.40)
                ctx.lineTo(w * 0.50, h * 0.22); ctx.lineTo(w * 0.50, h * 0.78)
                ctx.lineTo(w * 0.30, h * 0.60); ctx.lineTo(w * 0.12, h * 0.60)
                ctx.closePath()
                ctx.fill()
                if (glyph.kind === "speaker") {
                    ctx.beginPath(); ctx.arc(w * 0.50, h * 0.50, w * 0.20, -0.9, 0.9); ctx.stroke()
                    ctx.beginPath(); ctx.arc(w * 0.50, h * 0.50, w * 0.36, -0.9, 0.9); ctx.stroke()
                } else {
                    ctx.beginPath()
                    ctx.moveTo(w * 0.64, h * 0.38); ctx.lineTo(w * 0.90, h * 0.62)
                    ctx.moveTo(w * 0.90, h * 0.38); ctx.lineTo(w * 0.64, h * 0.62)
                    ctx.stroke()
                }
            } else if (glyph.kind === "title") {
                ctx.fillRect(w * 0.20, h * 0.20, w * 0.60, h * 0.14)
                ctx.fillRect(w * 0.43, h * 0.20, w * 0.14, h * 0.62)
            } else if (glyph.kind === "magnet") {
                // Horseshoe magnet: U shape with two pole tips.
                ctx.lineWidth = 2.6
                ctx.beginPath()
                ctx.moveTo(w * 0.24, h * 0.14); ctx.lineTo(w * 0.24, h * 0.52)
                ctx.arc(w * 0.50, h * 0.52, w * 0.26, Math.PI, 0, true)
                ctx.lineTo(w * 0.76, h * 0.14)
                ctx.stroke()
                ctx.lineWidth = 1.4
                ctx.fillRect(w * 0.17, h * 0.14, w * 0.14, h * 0.12)
                ctx.fillRect(w * 0.69, h * 0.14, w * 0.14, h * 0.12)
            } else if (glyph.kind === "video") {
                ctx.strokeRect(w * 0.12, h * 0.24, w * 0.76, h * 0.52)
                tri(0.43, 0.38, 0.43, 0.62, 0.63, 0.50)
            } else if (glyph.kind === "audio") {
                var heights = [0.30, 0.60, 0.84, 0.50, 0.24]
                for (var i = 0; i < heights.length; ++i) {
                    var bh = h * heights[i]
                    ctx.fillRect(w * (0.14 + i * 0.16), (h - bh) / 2, w * 0.09, bh)
                }
            }
        }
        Component.onCompleted: requestPaint()
    }

    onKindChanged: canvas.requestPaint()
    onInkChanged: canvas.requestPaint()
}
