import QtQuick
import Pulsar.Desktop 1.0
import "../theme/Theme.js" as Theme

// One clip on the timeline. Composition (top to bottom):
//   name strip -> waveform (audio-only: full height; video: lower zone)
// Gestures: body = select / move, edge grips (selected only) = trim.
// Move and trim previews come from the controller so snapping is shown live
// and the committed result is exactly what was previewed.
Rectangle {
    id: block
    property string clipKey: ""
    property string clipName: ""
    property string startText: ""
    property string endText: ""
    property real durationSeconds: 0
    property var gainKeyframes: []
    property bool missing: false
    property bool selected: false
    property bool muted: false
    property real gainDb: 0
    property bool audioClip: false
    property bool hasAudio: false
    property string thumbSource: ""
    // "video" | "image" | "color" | "audio"; fades are fractions of the clip width.
    property string kind: "video"
    property real fadeIn: 0
    property real fadeOut: 0
    readonly property bool isCard: kind === "color"
    readonly property bool isStill: kind === "image" || kind === "color"
    readonly property string cardColor: isCard && clipName.length >= 6 ? "#" + clipName.substring(clipName.length - 6) : ""
    // The label without its file extension, elided in the middle so "clip_01" and "clip_14" stay apart.
    readonly property string shownName: isCard ? "Color" : clipName.replace(/\.[A-Za-z0-9]{2,5}$/, "")
    // One picture per video clip, at the left, only when the clip is wide/tall enough to
    // keep the name and waveform readable. Decoration: absent thumbnail = plain clip.
    readonly property real thumbH: Math.max(0, height - 6)
    readonly property real thumbW: Math.round(thumbH * 16 / 9)
    // Wide clips: a picture tile at the left. Narrow ones: the same picture as a dimmed backdrop, so
    // a long timeline still shows what each clip is. Colour cards are just their colour.
    readonly property bool showThumb: !audioClip && !missing && !isCard && thumbSource.length > 0 && thumbH >= 24 && width > thumbW + 110
    readonly property bool showBackdrop: !audioClip && !missing && !isCard && !showThumb && thumbSource.length > 0 && thumbH >= 24 && width >= 28
    readonly property real contentX: showThumb ? thumbW + 6 : 0
    property real canonicalStartX: 0
    property real canonicalWidth: 3
    property real timelineWidth: 1
    // Pixels between the lane's left edge (incl. track header) and timeline time 0.
    property real timelineOffset: 0
    property real previewOffset: 0
    property real trimWidthDelta: 0
    property real pressTimelineX: 0
    property real lastInsideCurrentX: 0
    property bool trimDragging: false
    property bool trimIsStart: false
    property bool moveDragging: false
    // TimelinePane that owns the pointer-drag lifecycle. Null when the block is
    // embedded outside the Timeline (legacy VideoTrack) and unconstrained.
    property var dragHost: null
    readonly property real nameStripHeight: 18
    signal activated()
    signal muteToggled()
    // Right click: window coordinates, so the menu can live above the timeline.
    signal contextRequested(real sx, real sy)
    // Snap guide x in timeline pixels (excluding the header), or -1.
    signal snapGuide(real x)

    // Mouse position in timeline pixels, as the controller expects (0..timelineWidth).
    function timelineX(area, mouse) {
        return area.mapToItem(block.parent, mouse.x, mouse.y).x - block.timelineOffset
    }

    // Re-derives the live preview from the pointer's last in-region position.
    // Used by the host's edge-autoscroll tick so the block stays under the
    // pointer as the viewport moves beneath it.
    function refreshDragPreview() {
        var current = block.lastInsideCurrentX
        if (block.moveDragging) {
            var moved = controller.previewTimelineMove(block.clipKey, block.pressTimelineX, current, block.timelineWidth)
            block.previewOffset = moved.offset
            block.snapGuide(moved.guide)
        } else if (block.trimDragging) {
            var trimmed = controller.previewTimelineTrim(block.clipKey, block.trimIsStart, current, block.timelineWidth)
            if (block.trimIsStart) {
                var delta = trimmed.px - (block.canonicalStartX - block.timelineOffset)
                block.previewOffset = delta
                block.trimWidthDelta = -delta
            } else {
                block.trimWidthDelta = trimmed.px - (block.canonicalStartX - block.timelineOffset + block.canonicalWidth)
            }
            block.snapGuide(trimmed.guide)
        }
    }

    x: canonicalStartX + previewOffset
    width: Math.max(3, canonicalWidth + trimWidthDelta)
    color: missing ? Theme.clipMissing
         : isCard ? cardColor
         : (selected ? (audioClip ? Theme.clipAudioSelected : (isStill ? Theme.clipImageSelected : Theme.clipVideoSelected))
         : (body.containsMouse ? (audioClip ? Theme.clipAudioHover : (isStill ? Theme.clipImageHover : Theme.clipVideoHover))
         : (audioClip ? Theme.clipAudio : (isStill ? Theme.clipImage : Theme.clipVideo))))
    // Selection is the only outline; rest and hover stay borderless.
    border.color: missing ? Theme.error : Theme.accent
    border.width: selected ? 2 : (missing ? 1 : 0)
    radius: Theme.radiusS
    clip: true
    opacity: muted ? 0.62 : (moveDragging ? 0.92 : 1.0)

    // Name strip: a quiet darker band that doubles as the label area.
    Rectangle {
        visible: block.width > 40
        anchors.left: parent.left; anchors.leftMargin: block.contentX
        anchors.right: parent.right; anchors.top: parent.top
        height: block.nameStripHeight
        color: Theme.overlaySoft
    }
    Image {
        visible: block.showBackdrop
        anchors.fill: parent
        source: block.showBackdrop ? "image://pulsar-thumb/" + block.thumbSource : ""
        asynchronous: true
        cache: true
        fillMode: Image.PreserveAspectCrop
        sourceSize.width: 160; sourceSize.height: 90
        opacity: 0.55
    }
    Image {
        visible: block.showThumb
        x: 3; y: 3
        width: block.thumbW; height: block.thumbH
        source: block.showThumb ? "image://pulsar-thumb/" + block.thumbSource : ""
        asynchronous: true
        cache: true
        fillMode: Image.PreserveAspectCrop
        sourceSize.width: 160; sourceSize.height: 90
    }
    Text {
        visible: block.width > 40
        anchors.left: parent.left; anchors.leftMargin: 8 + block.contentX
        anchors.right: chips.visible ? chips.left : (speaker.visible ? speaker.left : parent.right); anchors.rightMargin: 6
        anchors.top: parent.top
        height: block.nameStripHeight
        verticalAlignment: Text.AlignVCenter
        text: block.shownName
        // A colour card can be any colour: pick the label ink that reads on it.
        color: block.isCard ? (Qt.color(block.cardColor).hslLightness > 0.6 ? "#101419" : "#ffffff") : Theme.textPrimary
        font.pixelSize: Theme.fontSizeXS
        font.bold: true
        // Wide clips keep the start of the name; narrow ones keep its END, where file names differ (clip_07).
        elide: block.width < 130 ? Text.ElideLeft : Text.ElideRight
    }

    // Waveform lane. Audio-only clips use the whole body; video clips keep the
    // upper part free (future thumbnails) and place the audio in the lower zone.
    Rectangle {
        // Dark inset: the waveform reads against a constant backdrop whatever the clip colour.
        x: waveform.x; y: waveform.y; width: waveform.width; height: waveform.height
        visible: waveform.visible; radius: 3; color: Theme.waveformWell
    }
    WaveformItem {
        id: waveform
        readonly property real zoneTop: block.nameStripHeight + 3
        readonly property real zoneHeight: Math.max(0, block.height - zoneTop - 4)
        x: 4 + block.contentX
        y: zoneTop
        width: Math.max(0, block.width - 8 - block.contentX)
        height: zoneHeight
        clipKey: block.clipKey
        selected: block.selected
        muted: block.muted
        density: controller.waveformDensity
        normalColor: block.audioClip ? Theme.waveformAudio : Theme.waveform
        selectedColor: block.audioClip ? Theme.waveformAudioSelected : Theme.waveformSelected
        pendingColor: Theme.waveformPending
        unavailableColor: Theme.waveformUnavailable
        baselineColor: Theme.waveformBaseline
        visible: width >= 10 && height >= 8
        Component.onCompleted: controller.attachWaveformItem(block.clipKey, waveform)
        Component.onDestruction: controller.detachWaveformItem(waveform)
    }

    // Distant Music automation is visible directly on the clip, so the
    // editor can read the intended entrance without opening the Inspector.
    Canvas {
        id: gainEnvelope
        visible: block.gainKeyframes.length > 0 && waveform.visible
        x: waveform.x; y: waveform.y; width: waveform.width; height: waveform.height
        z: 3
        onVisibleChanged: requestPaint()
        onWidthChanged: requestPaint(); onHeightChanged: requestPaint()
        Connections {
            target: block
            function onGainKeyframesChanged() { gainEnvelope.requestPaint() }
            function onDurationSecondsChanged() { gainEnvelope.requestPaint() }
        }
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var keys = block.gainKeyframes
            if (keys.length === 0 || block.durationSeconds <= 0) return
            ctx.strokeStyle = Theme.focus
            ctx.lineWidth = 1.5
            ctx.beginPath()
            for (var i = 0; i < keys.length; ++i) {
                var key = keys[i]
                var x = Math.max(0, Math.min(width, Number(key.time) / block.durationSeconds * width))
                var level = Math.max(0, Math.min(1, (Number(key.value) + 60) / 60))
                var y = height - 4 - level * Math.max(4, height - 8)
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
            }
            ctx.stroke()
            ctx.fillStyle = Theme.focus
            for (var j = 0; j < keys.length; ++j) {
                var point = keys[j]
                var px = Math.max(0, Math.min(width, Number(point.time) / block.durationSeconds * width))
                var py = height - 4 - Math.max(0, Math.min(1, (Number(point.value) + 60) / 60)) * Math.max(4, height - 8)
                ctx.beginPath(); ctx.arc(px, py, 2.5, 0, 2 * Math.PI); ctx.fill()
            }
        }
    }

    // Fades: translucent ramps over the clip's sound, drawn only when a fade exists.
    Canvas {
        id: fadeCanvas
        visible: (block.fadeIn > 0 || block.fadeOut > 0) && block.width > 30
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.top: parent.top; anchors.topMargin: block.nameStripHeight
        onVisibleChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections { target: block; function onFadeInChanged() { fadeCanvas.requestPaint() } function onFadeOutChanged() { fadeCanvas.requestPaint() } }
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = Theme.overlayStrong
            var w = width, h = height
            if (block.fadeIn > 0) {
                var fw = Math.min(w, block.fadeIn * block.width)
                ctx.beginPath(); ctx.moveTo(0, 0); ctx.lineTo(fw, 0); ctx.lineTo(0, h); ctx.closePath(); ctx.fill()
                ctx.strokeStyle = Theme.strongLightLine; ctx.lineWidth = 1
                ctx.beginPath(); ctx.moveTo(0, h); ctx.lineTo(fw, 0); ctx.stroke()
            }
            if (block.fadeOut > 0) {
                var ow = Math.min(w, block.fadeOut * block.width)
                ctx.beginPath(); ctx.moveTo(w, 0); ctx.lineTo(w - ow, 0); ctx.lineTo(w, h); ctx.closePath(); ctx.fill()
                ctx.strokeStyle = Theme.strongLightLine; ctx.lineWidth = 1
                ctx.beginPath(); ctx.moveTo(w - ow, 0); ctx.lineTo(w, h); ctx.stroke()
            }
        }
    }

    // Only states that are not the default: mute, offline, or automated audio.
    Row {
        id: chips
        visible: block.width > 90 && (block.muted || block.missing || block.gainKeyframes.length > 0 || speakerMouse.containsMouse)
        anchors.right: speaker.visible ? speaker.left : parent.right; anchors.rightMargin: speaker.visible ? 2 : 6
        anchors.top: parent.top; anchors.topMargin: 2
        spacing: 4
        Rectangle { visible: block.missing; height: 14; width: offlineText.implicitWidth + 10; radius: 3; color: Theme.error
            Text { id: offlineText; anchors.centerIn: parent; text: "Sin conexión"; color: Theme.textOnAccent; font.pixelSize: 10; font.bold: true } }
        Rectangle { visible: block.muted && !speakerMouse.containsMouse; height: 14; width: muteText.implicitWidth + 10; radius: 3; color: Theme.warning
            Text { id: muteText; anchors.centerIn: parent; text: "Silenciado"; color: Theme.textOnWarning; font.pixelSize: 10; font.bold: true } }
        Rectangle { visible: block.gainKeyframes.length > 0 && !speakerMouse.containsMouse; height: 14; width: automationText.implicitWidth + 10; radius: 3; color: Theme.focus
            Text { id: automationText; anchors.centerIn: parent; text: "Autom."; color: Theme.textOnAccent; font.pixelSize: 10; font.bold: true } }
        // Hover hint for the speaker (inside the clip: a floating tooltip would be clipped).
        Rectangle { visible: speakerMouse.containsMouse; height: 14; width: hintText.implicitWidth + 10; radius: 3; color: Theme.chipOverlay
            Text { id: hintText; anchors.centerIn: parent; text: block.muted ? "Activar audio (M)" : "Silenciar audio (M)"; color: Theme.textPrimary; font.pixelSize: 10; font.bold: true } }
    }

    Text {
        readonly property string note: waveform.waveformState === WaveformItem.Failed ? "Onda no disponible"
            : (waveform.waveformState === WaveformItem.Missing || waveform.waveformState === WaveformItem.Generating ? "Generando onda…" : "")
        anchors.left: parent.left; anchors.leftMargin: 8
        anchors.bottom: parent.bottom; anchors.bottomMargin: 4
        text: note
        color: Theme.textSecondary
        opacity: 0.7
        font.pixelSize: 10
        visible: waveform.visible && note.length > 0 && parent.width > 90
    }

    // ---- Trim grips -----------------------------------------------------
    // Hit width is constant; the drawn grip is visible on hover (hint) and
    // selection (accent). Only a selected clip can be trimmed.
    Rectangle {
        id: leftHandle
        x: 0; y: 0; width: selected ? 8 : 4; height: parent.height
        color: "transparent"
        Rectangle { visible: block.selected || body.containsMouse; anchors.fill: parent; color: block.selected ? Theme.accent : Theme.textSecondary; opacity: block.selected ? 1.0 : 0.45; radius: 1 }
        Rectangle { visible: block.selected && parent.width >= 8; anchors.centerIn: parent; width: 2; height: Math.min(14, parent.height - 8); radius: 1; color: Theme.textOnAccent; opacity: 0.9 }
        MouseArea {
            anchors.fill: parent
            enabled: block.selected
            preventStealing: true
            cursorShape: Qt.SizeHorCursor
            onPressed: function(mouse) {
                block.pressTimelineX = block.timelineX(parent, mouse)
                block.lastInsideCurrentX = block.pressTimelineX
                block.trimDragging = false
                block.trimIsStart = true
                block.previewOffset = 0
                block.trimWidthDelta = 0
                if (block.dragHost) {
                    var scene = leftHandle.mapToItem(null, mouse.x, mouse.y)
                    block.dragHost.beginTimelineDrag(block, scene.x, scene.y)
                }
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                if (block.dragHost) {
                    var scene = leftHandle.mapToItem(null, mouse.x, mouse.y)
                    block.dragHost.updateTimelineDrag(scene.x, scene.y)
                    if (!block.dragHost.dragInside) return
                }
                var current = block.timelineX(parent, mouse)
                if (Math.abs(current - block.pressTimelineX) >= 4) block.trimDragging = true
                if (block.trimDragging) {
                    var r = controller.previewTimelineTrim(block.clipKey, true, current, block.timelineWidth)
                    var delta = r.px - (block.canonicalStartX - block.timelineOffset)
                    block.previewOffset = delta
                    block.trimWidthDelta = -delta
                    block.lastInsideCurrentX = current
                    block.snapGuide(r.guide)
                }
            }
            onReleased: function(mouse) {
                var inside = true
                if (block.dragHost) {
                    var scene = leftHandle.mapToItem(null, mouse.x, mouse.y)
                    block.dragHost.updateTimelineDrag(scene.x, scene.y)
                    inside = block.dragHost.dragInside
                }
                var current = inside ? block.timelineX(parent, mouse) : block.lastInsideCurrentX
                if (block.trimDragging)
                    controller.trimTimelineClipStart(block.clipKey, current, block.timelineWidth)
                block.previewOffset = 0
                block.trimWidthDelta = 0
                block.trimDragging = false
                block.snapGuide(-1)
                if (block.dragHost) block.dragHost.endTimelineDrag()
            }
            onCanceled: {
                block.previewOffset = 0
                block.trimWidthDelta = 0
                block.trimDragging = false
                block.snapGuide(-1)
                if (block.dragHost) block.dragHost.endTimelineDrag()
            }
        }
    }
    Rectangle {
        id: rightHandle
        anchors.right: parent.right; y: 0; width: selected ? 8 : 4; height: parent.height
        color: "transparent"
        Rectangle { visible: block.selected || body.containsMouse; anchors.fill: parent; color: block.selected ? Theme.accent : Theme.textSecondary; opacity: block.selected ? 1.0 : 0.45; radius: 1 }
        Rectangle { visible: block.selected && parent.width >= 8; anchors.centerIn: parent; width: 2; height: Math.min(14, parent.height - 8); radius: 1; color: Theme.textOnAccent; opacity: 0.9 }
        MouseArea {
            anchors.fill: parent
            enabled: block.selected
            preventStealing: true
            cursorShape: Qt.SizeHorCursor
            onPressed: function(mouse) {
                block.pressTimelineX = block.timelineX(parent, mouse)
                block.lastInsideCurrentX = block.pressTimelineX
                block.trimDragging = false
                block.trimIsStart = false
                block.trimWidthDelta = 0
                if (block.dragHost) {
                    var scene = rightHandle.mapToItem(null, mouse.x, mouse.y)
                    block.dragHost.beginTimelineDrag(block, scene.x, scene.y)
                }
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                if (block.dragHost) {
                    var scene = rightHandle.mapToItem(null, mouse.x, mouse.y)
                    block.dragHost.updateTimelineDrag(scene.x, scene.y)
                    if (!block.dragHost.dragInside) return
                }
                var current = block.timelineX(parent, mouse)
                if (Math.abs(current - block.pressTimelineX) >= 4) block.trimDragging = true
                if (block.trimDragging) {
                    var r = controller.previewTimelineTrim(block.clipKey, false, current, block.timelineWidth)
                    block.trimWidthDelta = r.px - (block.canonicalStartX - block.timelineOffset + block.canonicalWidth)
                    block.lastInsideCurrentX = current
                    block.snapGuide(r.guide)
                }
            }
            onReleased: function(mouse) {
                var inside = true
                if (block.dragHost) {
                    var scene = rightHandle.mapToItem(null, mouse.x, mouse.y)
                    block.dragHost.updateTimelineDrag(scene.x, scene.y)
                    inside = block.dragHost.dragInside
                }
                var current = inside ? block.timelineX(parent, mouse) : block.lastInsideCurrentX
                if (block.trimDragging)
                    controller.trimTimelineClipEnd(block.clipKey, current, block.timelineWidth)
                block.trimWidthDelta = 0
                block.trimDragging = false
                block.snapGuide(-1)
                if (block.dragHost) block.dragHost.endTimelineDrag()
            }
            onCanceled: {
                block.trimWidthDelta = 0
                block.trimDragging = false
                block.snapGuide(-1)
                if (block.dragHost) block.dragHost.endTimelineDrag()
            }
        }
    }

    // ---- Body: select, move ------------------------------------------------
    MouseArea {
        id: body
        objectName: "clipBody"
        anchors.fill: parent
        anchors.leftMargin: leftHandle.width
        anchors.rightMargin: rightHandle.width
        preventStealing: true
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: block.moveDragging ? Qt.ClosedHandCursor : Qt.ArrowCursor
        onPressed: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                var p = body.mapToItem(null, mouse.x, mouse.y)
                block.contextRequested(p.x, p.y)
                return
            }
            block.pressTimelineX = block.timelineX(parent, mouse)
            block.lastInsideCurrentX = block.pressTimelineX
            block.moveDragging = false
            block.previewOffset = 0
            if (block.dragHost) {
                var scene = body.mapToItem(null, mouse.x, mouse.y)
                block.dragHost.beginTimelineDrag(block, scene.x, scene.y)
            }
        }
        onPositionChanged: function(mouse) {
            if (!pressed || pressedButtons !== Qt.LeftButton) return
            if (block.dragHost) {
                var scene = body.mapToItem(null, mouse.x, mouse.y)
                block.dragHost.updateTimelineDrag(scene.x, scene.y)
                if (!block.dragHost.dragInside) return
            }
            var current = block.timelineX(parent, mouse)
            if (Math.abs(current - block.pressTimelineX) >= 4) block.moveDragging = true
            if (block.moveDragging) {
                var r = controller.previewTimelineMove(block.clipKey, block.pressTimelineX, current, block.timelineWidth)
                block.previewOffset = r.offset
                block.lastInsideCurrentX = current
                block.snapGuide(r.guide)
            }
        }
        onReleased: function(mouse) {
            if (mouse.button === Qt.RightButton) return
            var inside = true
            if (block.dragHost) {
                var scene = body.mapToItem(null, mouse.x, mouse.y)
                block.dragHost.updateTimelineDrag(scene.x, scene.y)
                inside = block.dragHost.dragInside
            }
            var current = inside ? block.timelineX(parent, mouse) : block.lastInsideCurrentX
            if (block.moveDragging) {
                controller.moveTimelineClip(block.clipKey, block.pressTimelineX, current, block.timelineWidth)
            } else {
                block.activated()
            }
            block.previewOffset = 0
            block.moveDragging = false
            block.snapGuide(-1)
            if (block.dragHost) block.dragHost.endTimelineDrag()
        }
        onCanceled: {
            block.previewOffset = 0
            block.moveDragging = false
            block.snapGuide(-1)
            if (block.dragHost) block.dragHost.endTimelineDrag()
        }
    }

    // Audio state: speaker = audible, crossed speaker = silenced. Click toggles the clip's
    // one mute state (same operation as the Inspector button, M and the context menu).
    Item {
        id: speaker
        visible: block.hasAudio && !block.missing && block.width > 56
        anchors.right: parent.right; anchors.rightMargin: 3
        anchors.top: parent.top
        width: 24; height: block.nameStripHeight
        TGlyph {
            anchors.centerIn: parent; width: 15; height: 15
            kind: block.muted ? "speaker-off" : "speaker"
            ink: block.muted ? Theme.warning : Theme.textPrimary
            opacity: block.muted || block.selected || body.containsMouse || speakerMouse.containsMouse ? 1.0 : 0.55
        }
        MouseArea {
            id: speakerMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: block.muteToggled()
        }
    }
}
