import QtQuick
import "../theme/Theme.js" as Theme

// Inspector: what you can edit (CLIP / AUDIO) leads; what is merely
// informative (MEDIA) is summarised in one line, with the full technical
// dump collapsed under "Detalles".
Panel {
    id: inspector

    property string selName: ""
    property string selRange: ""
    property bool detailsExpanded: false

    function metaValue(key) {
        var text = controller.metadata
        if (!text) {
            return ""
        }
        var lines = text.split("\n")
        for (var i = 0; i < lines.length; ++i) {
            var idx = lines[i].indexOf(":")
            if (idx >= 0 && lines[i].slice(0, idx).trim() === key) {
                return lines[i].slice(idx + 1).trim()
            }
        }
        return ""
    }
    function fpsText(raw) {
        var parts = raw.split("/")
        var n = Number(parts[0])
        var d = parts.length > 1 ? Number(parts[1]) : 1
        if (!(n > 0) || !(d > 0)) {
            return ""
        }
        var v = n / d
        return (Math.abs(v - Math.round(v)) < 0.005 ? Math.round(v).toString() : v.toFixed(2)) + " fps"
    }
    // "H264 · 1280×720 · 24 fps" or "AAC · 44.1 kHz · estéreo"
    function mediaSummary(text) {
        var res = metaValue("Resolución")
        if (res.length > 0) {
            var bits = [metaValue("Codec").toUpperCase(), res.replace(/\s*×\s*/, "×"), fpsText(metaValue("FPS informativo"))]
            return bits.filter(function(b) { return b.length > 0 }).join(" · ")
        }
        var codec = metaValue("Audio codec")
        if (codec.length === 0) {
            return ""
        }
        var rate = Number(metaValue("Audio sample rate").replace(/[^0-9.]/g, ""))
        var channels = Number(metaValue("Audio channels"))
        var parts = [codec.toUpperCase()]
        if (rate > 0) parts.push((rate / 1000).toFixed(rate % 1000 === 0 ? 0 : 1) + " kHz")
        if (channels > 0) parts.push(channels === 1 ? "mono" : (channels === 2 ? "estéreo" : channels + " canales"))
        return parts.join(" · ")
    }
    function detailRows(text) {
        var out = []
        if (!text) {
            return out
        }
        var lines = text.split("\n")
        for (var i = 0; i < lines.length; ++i) {
            var idx = lines[i].indexOf(":")
            if (idx > 0) {
                out.push({key: lines[i].slice(0, idx).trim(), value: lines[i].slice(idx + 1).trim()})
            }
        }
        return out
    }
    property string summary: mediaSummary(controller.metadata)
    property var rows: detailRows(controller.metadata)

    // Mirror the selected clip's name/range from the timeline model, and keep
    // it fresh while trims/moves change the range of the same selection.
    Repeater {
        model: controller.timelineClips
        delegate: Item {
            visible: false
            property bool sel: model.selected
            property string range: model.startText + "  →  " + model.endText
            function push() { if (sel) { inspector.selName = model.clipName; inspector.selRange = range } }
            onSelChanged: push()
            onRangeChanged: push()
            Component.onCompleted: push()
        }
    }

    Connections {
        target: controller
        function onTimelineChanged() {
            if (!gainInput.activeFocus) {
                gainInput.text = Number(controller.selectedClipGainDb).toFixed(1)
            }
        }
    }

    Text { id: titleText; anchors.left: parent.left; anchors.top: parent.top; anchors.leftMargin: Theme.spaceL; anchors.topMargin: Theme.spaceL; text: "Inspector"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeL; font.bold: true }

    Flickable {
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: titleText.bottom; anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.spaceL; anchors.rightMargin: Theme.spaceL; anchors.topMargin: Theme.spaceM; anchors.bottomMargin: Theme.spaceM
        contentWidth: width
        contentHeight: body.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Column {
            id: body
            width: parent.width
            spacing: Theme.spaceL

            Text {
                visible: controller.selectedClipKey.length === 0 && inspector.summary.length === 0
                width: parent.width
                wrapMode: Text.WordWrap
                text: "Seleccioná un clip en la timeline o importá un medio."
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeS
            }

            // ---- CLIP (editable context) -------------------------------
            Column {
                visible: controller.selectedClipKey.length > 0
                width: parent.width
                spacing: 6
                SectionLabel { text: "CLIP" }
                Text { width: parent.width; text: inspector.selName; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeM; elide: Text.ElideMiddle }
                Text { width: parent.width; text: inspector.selRange; color: Theme.textSecondary; font.family: Theme.mono; font.pixelSize: Theme.fontSizeXS; elide: Text.ElideRight }
            }

            // ---- AUDIO (editable) --------------------------------------
            Column {
                visible: controller.selectedClipKey.length > 0
                width: parent.width
                spacing: Theme.spaceM
                SectionLabel { text: "AUDIO" }
                Row {
                    visible: controller.selectedClipHasAudio
                    width: parent.width
                    spacing: 6
                    Text { width: 52; anchors.verticalCenter: parent.verticalCenter; text: "Ganancia"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS }
                    PButton { width: 28; height: Theme.controlHeightSmall; text: "−"; onClicked: controller.setSelectedClipGainDb(controller.selectedClipGainDb - 1) }
                    Rectangle {
                        width: 64; height: Theme.controlHeightSmall
                        color: Theme.bgInput; radius: Theme.radiusM
                        border.width: 1
                        border.color: gainInput.activeFocus ? Theme.focus : Theme.borderStrong
                        TextInput {
                            id: gainInput
                            anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 22
                            verticalAlignment: TextInput.AlignVCenter
                            horizontalAlignment: TextInput.AlignRight
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeM
                            font.family: Theme.mono
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: DoubleValidator { bottom: -60; top: 12; decimals: 1; notation: DoubleValidator.StandardNotation }
                            text: Number(controller.selectedClipGainDb).toFixed(1)
                            selectByMouse: true
                            onEditingFinished: controller.setSelectedClipGainDb(Number(text))
                        }
                        Text { anchors.right: parent.right; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter; text: "dB"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
                    }
                    PButton { width: 28; height: Theme.controlHeightSmall; text: "+"; onClicked: controller.setSelectedClipGainDb(controller.selectedClipGainDb + 1) }
                }
                // One unambiguous action; the state is the clip's single mute flag.
                Rectangle {
                    id: muteButton
                    visible: controller.selectedClipHasAudio
                    width: parent.width; height: 32
                    radius: Theme.radiusM
                    activeFocusOnTab: true
                    color: muteMouse.pressed ? Theme.bgPressed
                         : (controller.selectedClipMuted ? (muteMouse.containsMouse ? "#4a3a1c" : "#3a2f18")
                         : (muteMouse.containsMouse ? Theme.bgHover : Theme.bgElevated))
                    border.width: 1
                    border.color: activeFocus ? Theme.focus : (controller.selectedClipMuted ? Theme.warning : Theme.borderStrong)
                    Keys.onSpacePressed: controller.toggleSelectedClipMute()
                    Keys.onReturnPressed: controller.toggleSelectedClipMute()
                    Row {
                        anchors.centerIn: parent
                        spacing: 8
                        TGlyph { anchors.verticalCenter: parent.verticalCenter; width: 16; height: 16; kind: controller.selectedClipMuted ? "speaker-off" : "speaker"; ink: controller.selectedClipMuted ? Theme.warning : Theme.textPrimary }
                        Text { anchors.verticalCenter: parent.verticalCenter; text: controller.selectedClipMuted ? "Activar audio" : "Silenciar audio"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeM }
                        Text { anchors.verticalCenter: parent.verticalCenter; text: "M"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS; font.family: Theme.mono }
                    }
                    MouseArea { id: muteMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { muteButton.forceActiveFocus(); controller.toggleSelectedClipMute() } }
                }
                Text { visible: !controller.selectedClipHasAudio; width: parent.width; wrapMode: Text.WordWrap; text: "Este clip no tiene audio."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeS }
                Text { visible: controller.selectedClipHasAudio; width: parent.width; text: "−60 a +12 dB"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
            }

            // ---- MEDIA INFO (informative) ------------------------------
            Column {
                // The controller only describes the active source; hide it when
                // the selected clip belongs to a different media.
                visible: inspector.summary.length > 0
                width: parent.width
                spacing: 6
                SectionLabel { text: "MEDIO" }
                // The clip above already names its media; avoid saying it twice.
                Text { visible: controller.selectedClipKey.length === 0 || inspector.selName !== controller.mediaName; width: parent.width; text: controller.mediaName; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS; elide: Text.ElideMiddle }
                Text { width: parent.width; text: inspector.summary; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS; wrapMode: Text.WordWrap }
            }

            // ---- Technical details (collapsed) -------------------------
            Column {
                visible: inspector.rows.length > 0
                width: parent.width
                spacing: 8
                Item {
                    width: parent.width; height: 24
                    activeFocusOnTab: true
                    Keys.onReturnPressed: inspector.detailsExpanded = !inspector.detailsExpanded
                    Keys.onSpacePressed: inspector.detailsExpanded = !inspector.detailsExpanded
                    Row {
                        anchors.verticalCenter: parent.verticalCenter; spacing: 6
                        Text { text: inspector.detailsExpanded ? "▾" : "▸"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeS }
                        Text { text: "Detalles"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeS }
                    }
                    MouseArea { anchors.fill: parent; onClicked: inspector.detailsExpanded = !inspector.detailsExpanded }
                }
                Repeater {
                    model: inspector.detailsExpanded ? inspector.rows : []
                    delegate: Row {
                        width: body.width
                        spacing: 8
                        Text { width: 96; text: modelData.key; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS; elide: Text.ElideRight }
                        Text { width: parent.width - 104; text: modelData.value; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeXS; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight }
                    }
                }
                Text { visible: inspector.detailsExpanded; width: parent.width; text: controller.debugMetrics; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS; wrapMode: Text.WordWrap }
            }
        }
    }
}
