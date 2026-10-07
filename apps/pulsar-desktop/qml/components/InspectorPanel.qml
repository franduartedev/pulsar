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

    // Mirror the selected clip's name/range from the timeline model.
    Repeater {
        model: controller.timelineClips
        delegate: Item {
            visible: false
            property bool sel: model.selected
            function push() { if (sel) { inspector.selName = model.clipName; inspector.selRange = model.startText + "  →  " + model.endText } }
            onSelChanged: push()
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
                            text: Number(controller.selectedClipGainDb).toFixed(1)
                            selectByMouse: true
                            onEditingFinished: controller.setSelectedClipGainDb(Number(text))
                        }
                        Text { anchors.right: parent.right; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter; text: "dB"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
                    }
                    PButton { width: 28; height: Theme.controlHeightSmall; text: "+"; onClicked: controller.setSelectedClipGainDb(controller.selectedClipGainDb + 1) }
                }
                Row {
                    width: parent.width
                    spacing: 6
                    Text { width: 52; anchors.verticalCenter: parent.verticalCenter; text: "Silenciar"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS }
                    // Toggle: accent only while active.
                    Rectangle {
                        id: muteToggle
                        width: 38; height: 20; radius: 10
                        anchors.verticalCenter: parent.verticalCenter
                        color: controller.selectedClipMuted ? Theme.accent : Theme.track
                        border.width: activeFocus ? 1 : 0
                        border.color: Theme.focus
                        activeFocusOnTab: true
                        Keys.onSpacePressed: controller.toggleSelectedClipMute()
                        Keys.onReturnPressed: controller.toggleSelectedClipMute()
                        Rectangle { y: 2; x: controller.selectedClipMuted ? parent.width - width - 2 : 2; width: 16; height: 16; radius: 8; color: Theme.textPrimary }
                        MouseArea { anchors.fill: parent; onClicked: { muteToggle.forceActiveFocus(); controller.toggleSelectedClipMute() } }
                    }
                }
                Text { width: parent.width; text: "−60 a +12 dB"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
            }

            // ---- MEDIA INFO (informative) ------------------------------
            Column {
                // The controller only describes the active source; hide it when
                // the selected clip belongs to a different media.
                visible: inspector.summary.length > 0
                         && (controller.selectedClipKey.length === 0 || inspector.selName === controller.mediaName)
                width: parent.width
                spacing: 6
                SectionLabel { text: "MEDIO" }
                Text { width: parent.width; text: controller.mediaName; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS; elide: Text.ElideMiddle }
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
