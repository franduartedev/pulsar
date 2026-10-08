import QtQuick
import "../theme/Theme.js" as Theme

// Timeline: toolbar, ruler (click/drag to seek), track lanes with clips, one
// playhead overlay and a snap guide shown only while a drag is snapping.
Panel {
    id: pane
    property real gutterWidth: Math.max(88, Math.min(120, width * 0.095))
    readonly property real timelineWidth: Math.max(1, width - gutterWidth)
    // Lanes use the room they are given (more waveform to read) within sane bounds.
    // The title track sits above VIDEO 1 and only appears once there is something to caption.
    readonly property real titleLaneH: controller.hasMedia ? 34 : 0
    property real trackLaneHeight: Math.max(52, Math.min(104, Math.floor((trackArea.height - titleLaneH) / Math.max(2, controller.timelineTracks.length))))
    // x (timeline pixels) of the magnetic edge being snapped to, or -1.
    property real snapGuideX: -1
    // ---- Drag & drop from the library ----------------------------------------------------
    // Ghost = where the item would land (green) or why it cannot (red); computed by the same
    // controller resolver that performs the drop.
    property bool dropVisible: false
    property bool dropValid: false
    property int dropLane: -1
    property real dropStartPx: 0
    property real dropWidthPx: 0
    property string dropReason: ""
    function laneAtScene(sx, sy) {
        var p = trackScroll.mapFromItem(null, sx, sy)
        if (p.x < pane.gutterWidth || p.x > trackScroll.width || p.y < 0 || p.y > trackScroll.height) return null
        var lane = Math.floor((p.y + trackScroll.contentY - pane.titleLaneH) / pane.trackLaneHeight)
        if (lane < 0 || lane >= controller.timelineTracks.length) return null
        return { lane: lane, pixel: p.x - pane.gutterWidth }
    }
    function updateDrop(mediaId, sx, sy) {
        var hit = laneAtScene(sx, sy)
        if (hit === null) { dropVisible = false; return }
        var r = controller.previewAssetDrop(mediaId, hit.lane, hit.pixel, pane.timelineWidth)
        dropLane = hit.lane; dropValid = r.valid; dropReason = r.reason
        dropStartPx = r.valid ? r.startPx : Math.max(0, hit.pixel - 24)
        dropWidthPx = r.valid ? Math.max(6, r.widthPx) : 48
        dropVisible = true
    }
    function commitDrop(mediaId, sx, sy) {
        var hit = laneAtScene(sx, sy)
        dropVisible = false
        if (hit !== null) controller.dropAsset(mediaId, hit.lane, hit.pixel, pane.timelineWidth)
    }
    function clearDrop() { dropVisible = false }

    // Right click on a clip: window coordinates for the context menu (hosted by Main).
    signal clipContextRequested(real sx, real sy)

    // Ctrl + wheel zooms around the pointer; Shift + wheel pans while zoomed in.
    function wheelZoom(x, delta) {
        controller.zoomTimelineAt(x - gutterWidth, timelineWidth, Math.pow(1.2, delta / 120))
    }
    function wheelPan(delta) {
        if (controller.timelineZoom > 1.0) controller.setTimelineScroll(controller.timelineScroll - delta / 120 * 0.05 / Math.max(1, Math.sqrt(controller.timelineZoom)))
    }

    function trackTitle(label) {
        // "VIDEO 1" -> "Video 1"
        return label.length > 0 ? label.charAt(0) + label.slice(1).toLowerCase() : label
    }

    // ---- Toolbar: title, edit actions, view controls ------------------
    Item {
        id: toolbar
        z: 30
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 40
        Row {
            anchors.left: parent.left
            anchors.leftMargin: Theme.spaceL
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            Text { anchors.verticalCenter: parent.verticalCenter; text: "Timeline"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeL; font.bold: true; rightPadding: Theme.spaceM }
            ToolButton { glyph: "split"; text: "Cortar"; tip: "Cortar en el cabezal (S)"; enabled: controller.canSplit; onClicked: controller.splitSelectedClip() }
            ToolButton { glyph: "audio"; text: "Separar audio"; tip: "Separar el audio del clip de video"; enabled: controller.canSeparateAudio; onClicked: controller.separateSelectedAudio() }
            ToolButton { glyph: "trash"; text: "Borrar"; tip: "Borrar (Supr)"; enabled: controller.selectedClipKey.length > 0 || controller.selectedTitleKey.length > 0; onClicked: controller.deleteSelectedClip() }
            ToolButton { glyph: "title"; text: "Título"; tip: "Agregar un título en el cabezal (T)"; enabled: controller.hasMedia; onClicked: controller.addTitle() }
            Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 1; height: 16; color: Theme.borderStrong }
            ToolButton { glyph: "magnet"; text: "Snap"; toggle: true; tip: controller.snapEnabled ? "Imán activado: se ajusta a bordes y cabezal (N)" : "Imán desactivado (N)"; checked: controller.snapEnabled; onClicked: controller.snapEnabled = !controller.snapEnabled }
            Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 1; height: 16; color: Theme.borderStrong }
            ToolButton { glyph: "undo"; text: "Deshacer"; tip: "Deshacer (Ctrl+Z)"; enabled: controller.canUndo; onClicked: controller.undoEdit() }
            ToolButton { glyph: "redo"; text: "Rehacer"; tip: "Rehacer (Ctrl+Shift+Z)"; enabled: controller.canRedo; onClicked: controller.redoEdit() }
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: Theme.spaceL
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            ToolButton { glyph: "fit"; text: "Ajustar"; tip: "Ver toda la timeline (Shift+Z)"; enabled: controller.hasMedia && controller.timelineZoom > 1.0; onClicked: controller.fitTimeline() }
            Item { width: Theme.spaceS; height: 1 }
            Text { anchors.verticalCenter: parent.verticalCenter; text: "Zoom"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeXS }
            PSlider {
                anchors.verticalCenter: parent.verticalCenter; width: 96
                value: Math.log(controller.timelineZoom) / Math.LN10 / 7
                onMoved: function(v) { controller.setTimelineZoom(Math.pow(10, 7 * v)) }
            }
            Item { visible: pane.width > 1500; width: Theme.spaceS; height: 1 }
            Text { visible: pane.width > 1500; anchors.verticalCenter: parent.verticalCenter; text: "Pan"; color: controller.timelineZoom > 1.0 ? Theme.textSecondary : Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
            PSlider {
                visible: pane.width > 1500
                anchors.verticalCenter: parent.verticalCenter; width: 72
                enabled: controller.timelineZoom > 1.0
                fill: Theme.textSecondary
                value: controller.timelineScroll
                onMoved: function(v) { controller.setTimelineScroll(v) }
            }
            Item { visible: pane.width > 1500; width: Theme.spaceS; height: 1 }
            Text { visible: pane.width > 1500; anchors.verticalCenter: parent.verticalCenter; text: "Onda"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeXS }
            PSlider {
                visible: pane.width > 1500
                anchors.verticalCenter: parent.verticalCenter; width: 56
                fill: Theme.waveform
                value: (controller.waveformDensity - 0.5) / 1.5
                onMoved: function(v) { controller.setWaveformDensity(0.5 + 1.5 * v) }
            }
        }
    }

    // ---- Ruler: major/minor ticks; press or drag anywhere to seek --------
    Item {
        id: ruler
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: toolbar.bottom
        height: 28
        clip: true
        Rectangle { anchors.fill: parent; color: Theme.ruler }
        Rectangle { width: pane.gutterWidth; anchors.top: parent.top; anchors.bottom: parent.bottom; color: Theme.bgElevated }
        Repeater {
            model: controller.hasMedia ? controller.timelineRulerTicks : []
            delegate: Item {
                x: pane.gutterWidth + modelData.progress * pane.timelineWidth
                width: 1
                height: ruler.height
                Rectangle { anchors.bottom: parent.bottom; width: 1; height: modelData.major ? 10 : 5; color: Theme.textMuted; opacity: modelData.major ? 0.9 : 0.5 }
                Text { visible: modelData.major; anchors.left: parent.left; anchors.leftMargin: 8; anchors.top: parent.top; anchors.topMargin: 5; text: modelData.label; color: Theme.textSecondary; font.family: Theme.mono; font.pixelSize: Theme.fontSizeXS }
            }
        }
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.borderSubtle }
        WheelHandler {
            acceptedModifiers: Qt.ControlModifier
            onWheel: function(event) { pane.wheelZoom(point.position.x, event.angleDelta.y) }
        }
        WheelHandler {
            acceptedModifiers: Qt.ShiftModifier
            onWheel: function(event) { pane.wheelPan(event.angleDelta.y !== 0 ? event.angleDelta.y : event.angleDelta.x) }
        }
        MouseArea {
            anchors.fill: parent
            enabled: controller.hasMedia
            cursorShape: Qt.PointingHandCursor
            onPressed: function(mouse) { controller.seekTimelineAt(mouse.x - pane.gutterWidth, pane.timelineWidth) }
            onPositionChanged: function(mouse) { if (pressed) controller.seekTimelineAt(mouse.x - pane.gutterWidth, pane.timelineWidth) }
        }
    }

    // ---- Tracks -----------------------------------------------------------
    Item {
        id: trackArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: ruler.bottom
        anchors.bottom: parent.bottom
        clip: true

        Flickable {
            id: trackScroll
            anchors.fill: parent
            contentWidth: width
            contentHeight: Math.max(height, pane.titleLaneH + controller.timelineTracks.length * pane.trackLaneHeight)
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Item {
                id: contentArea
                width: trackScroll.width
                height: trackScroll.contentHeight
                Rectangle { anchors.fill: parent; color: Theme.laneAudio }
                WheelHandler {
                    acceptedModifiers: Qt.ControlModifier
                    onWheel: function(event) { pane.wheelZoom(point.position.x, event.angleDelta.y) }
                }
                WheelHandler {
                    acceptedModifiers: Qt.ShiftModifier
                    onWheel: function(event) { pane.wheelPan(event.angleDelta.y !== 0 ? event.angleDelta.y : event.angleDelta.x) }
                }
                Repeater {
                    model: controller.timelineTracks
                    delegate: Item {
                        y: pane.titleLaneH + modelData.index * pane.trackLaneHeight
                        width: contentArea.width
                        height: pane.trackLaneHeight
                        // Lane (empty space is real background) + header gutter.
                        Rectangle { anchors.fill: parent; color: modelData.kind === "audio" ? Theme.laneAudio : Theme.laneVideo }
                        Rectangle { width: pane.gutterWidth; anchors.top: parent.top; anchors.bottom: parent.bottom; color: Theme.bgElevated }
                        Rectangle { width: 3; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; color: modelData.kind === "audio" ? Theme.waveformAudio : Theme.waveform; opacity: 0.85 }
                        Text { x: 14; anchors.verticalCenter: parent.verticalCenter; text: pane.trackTitle(modelData.label); color: Theme.textPrimary; font.pixelSize: Theme.fontSizeS; elide: Text.ElideRight; width: pane.gutterWidth - 22 }
                        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.bgRoot }
                    }
                }
                // Clips live in their own layer, cut off at the track-header edge so a clip that
                // starts before the zoomed view never paints over the headers.
                Item {
                    id: clipLayer
                    z: 2
                    x: pane.gutterWidth
                    width: pane.timelineWidth
                    height: contentArea.height
                    clip: true
                    Repeater {
                        model: controller.timelineClips
                        delegate: ClipBlock {
                            z: model.selected ? 3 : 2
                            visible: model.inViewport
                            y: pane.titleLaneH + model.trackIndex * pane.trackLaneHeight + 6
                            canonicalStartX: model.startProgress * pane.timelineWidth
                            canonicalWidth: Math.max(3, (model.endProgress - model.startProgress) * pane.timelineWidth)
                            height: pane.trackLaneHeight - 12
                            timelineWidth: pane.timelineWidth
                            timelineOffset: 0
                            clipName: model.clipName
                            startText: model.startText
                            endText: model.endText
                            missing: model.missing
                            selected: model.selected
                            muted: model.muted
                            gainDb: model.gainDb
                            audioClip: model.audioTrack
                            hasAudio: model.hasAudio
                            thumbSource: model.thumbSource
                            clipKey: model.clipKey
                            onActivated: controller.selectTimelineClip(clipKey)
                            onMuteToggled: { controller.selectTimelineClip(clipKey); controller.toggleSelectedClipMute() }
                            onContextRequested: function(sx, sy) { controller.selectTimelineClip(clipKey); pane.clipContextRequested(sx, sy) }
                            onSnapGuide: function(x) { pane.snapGuideX = x }
                        }
                    }
                }
                // Drop ghost.
                Rectangle {
                    visible: pane.dropVisible
                    z: 12
                    x: pane.gutterWidth + pane.dropStartPx
                    y: pane.titleLaneH + pane.dropLane * pane.trackLaneHeight + 6
                    width: pane.dropWidthPx; height: pane.trackLaneHeight - 12
                    radius: Theme.radiusS
                    color: pane.dropValid ? "#4433cc88" : "#44f26d6d"
                    border.width: 2; border.color: pane.dropValid ? Theme.success : Theme.error
                    Text { visible: !pane.dropValid && pane.dropReason.length > 0; anchors.left: parent.left; anchors.leftMargin: 6; anchors.verticalCenter: parent.verticalCenter
                           width: Math.max(parent.width, 220); text: pane.dropReason; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeXS; font.bold: true }
                }
                // ---- Title track -----------------------------------------------------
                Item {
                    visible: pane.titleLaneH > 0
                    width: contentArea.width; height: pane.titleLaneH
                    Rectangle { anchors.fill: parent; color: Theme.laneTitle }
                    Rectangle { width: pane.gutterWidth; anchors.top: parent.top; anchors.bottom: parent.bottom; color: Theme.bgElevated }
                    Rectangle { width: 3; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; color: Theme.titleSelected; opacity: 0.9 }
                    Text { x: 14; anchors.verticalCenter: parent.verticalCenter; text: "Títulos"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeS; elide: Text.ElideRight; width: pane.gutterWidth - 22 }
                    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.bgRoot }
                    Text { visible: controller.timelineTitles.rowCount() === 0 && pane.width > 700; x: pane.gutterWidth + 10; anchors.verticalCenter: parent.verticalCenter
                           text: "Sin títulos · botón Título o tecla T"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
                }
                Item {
                    id: titleLayer
                    visible: pane.titleLaneH > 0
                    x: pane.gutterWidth; z: 2
                    width: pane.timelineWidth; height: pane.titleLaneH
                    clip: true
                    Repeater {
                        model: controller.timelineTitles
                        delegate: TitleBlock {
                            visible: model.inViewport
                            y: 4; height: pane.titleLaneH - 8
                            canonicalStartX: model.startProgress * pane.timelineWidth
                            canonicalWidth: Math.max(6, (model.endProgress - model.startProgress) * pane.timelineWidth)
                            timelineWidth: pane.timelineWidth
                            timelineOffset: 0
                            titleKey: model.titleKey
                            titleText: model.titleText
                            selected: model.selected
                            onActivated: controller.selectTitle(titleKey)
                            onSnapGuide: function(x) { pane.snapGuideX = x }
                        }
                    }
                }
                // Empty lane: click selects nothing and seeks; drag scrubs.
                MouseArea {
                    anchors.fill: parent
                    z: 1
                    acceptedButtons: Qt.LeftButton
                    onPressed: function(mouse) {
                        controller.selectTimelineClip("")
                        controller.seekTimelineAt(mouse.x - pane.gutterWidth, pane.timelineWidth)
                    }
                    onPositionChanged: function(mouse) { if (pressed) controller.seekTimelineAt(mouse.x - pane.gutterWidth, pane.timelineWidth) }
                }
            }
        }

        // Thin position indicator when the tracks do not all fit (wheel scrolls them).
        Rectangle {
            visible: trackScroll.contentHeight > trackScroll.height + 1
            z: 25
            enabled: false
            anchors.right: parent.right; anchors.rightMargin: 2
            width: 4; radius: 2
            color: Theme.textSecondary
            opacity: 0.55
            height: Math.max(20, trackArea.height * trackScroll.height / trackScroll.contentHeight)
            y: trackScroll.contentHeight > trackScroll.height ? (trackArea.height - height) * trackScroll.contentY / (trackScroll.contentHeight - trackScroll.height) : 0
        }

        // One short sentence and nothing else when there is nothing to edit.
        Text {
            visible: !controller.hasMedia
            anchors.left: parent.left; anchors.leftMargin: pane.gutterWidth
            anchors.right: parent.right; anchors.top: parent.top; anchors.topMargin: 6
            height: pane.trackLaneHeight - 12
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: "Importá un medio y usá Insertar para empezar a editar"
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeS
        }
    }

    // ---- Overlays (never intercept the mouse) ------------------------------
    Rectangle {
        // Magnetic snap guide, only while a drag is snapping.
        visible: pane.snapGuideX >= 0
        x: pane.gutterWidth + pane.snapGuideX
        y: ruler.y
        width: 1
        height: ruler.height + trackArea.height
        color: Theme.warning
        opacity: 0.9
        z: 15
        enabled: false
    }
    Playhead {
        z: 20
        y: ruler.y + 2
        height: ruler.height + trackArea.height - 2
        progress: controller.hasMedia ? controller.timelinePlayheadProgress : -1
        timelineOffset: pane.gutterWidth
        timelineWidth: pane.timelineWidth
    }
}
