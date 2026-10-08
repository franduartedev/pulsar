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
        if (metaValue("Codec").toLowerCase() === "pulsar-color") return "Placa de color · " + res.replace(/\s*×\s*/, "×")
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
                visible: controller.selectedClipKey.length === 0 && controller.selectedTitleKey.length === 0 && inspector.summary.length === 0
                width: parent.width
                wrapMode: Text.WordWrap
                text: "Seleccioná un clip en la timeline o importá un medio."
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeS
            }

            // ---- TÍTULO (static text over the video) -----------------------
            Column {
                id: titleSection
                visible: controller.selectedTitleKey.length > 0
                width: parent.width
                spacing: Theme.spaceM
                readonly property var info: controller.selectedTitle
                function style(size, color, h, v, shadow) { controller.setSelectedTitleStyle(size, color, h, v, shadow) }
                SectionLabel { text: "TÍTULO" }
                Rectangle {
                    width: parent.width; height: 74
                    radius: Theme.radiusM; color: Theme.bgInput
                    border.width: 1; border.color: titleEdit.activeFocus ? Theme.focus : Theme.borderStrong
                    TextEdit {
                        id: titleEdit
                        anchors.fill: parent; anchors.margins: 8
                        wrapMode: TextEdit.Wrap
                        color: Theme.textPrimary
                        selectionColor: Theme.accent
                        selectedTextColor: Theme.textOnAccent
                        font.pixelSize: Theme.fontSizeM
                        clip: true
                        text: titleSection.info && titleSection.info.text !== undefined ? titleSection.info.text : ""
                        // Applied when you leave the box or press Ctrl+Enter (one undo step).
                        onActiveFocusChanged: if (!activeFocus) controller.setSelectedTitleText(text)
                        Keys.onPressed: function(event) {
                            if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && (event.modifiers & Qt.ControlModifier)) {
                                controller.setSelectedTitleText(text); focus = false; event.accepted = true
                            } else if (event.key === Qt.Key_Escape) {
                                text = titleSection.info.text; focus = false; event.accepted = true
                            }
                        }
                    }
                }
                Text { width: parent.width; text: "Ctrl+Enter aplica el texto"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
                Row {
                    width: parent.width; spacing: 6
                    Text { width: 52; anchors.verticalCenter: parent.verticalCenter; text: "Tamaño"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS }
                    PButton { width: 28; height: Theme.controlHeightSmall; text: "−"; onClicked: titleSection.style(titleSection.info.size - 1, titleSection.info.color, titleSection.info.horizontal, titleSection.info.vertical, titleSection.info.shadow) }
                    Text { width: 40; horizontalAlignment: Text.AlignHCenter; anchors.verticalCenter: parent.verticalCenter; text: titleSection.info && titleSection.info.size !== undefined ? titleSection.info.size + " %" : ""; color: Theme.textPrimary; font.family: Theme.mono; font.pixelSize: Theme.fontSizeM }
                    PButton { width: 28; height: Theme.controlHeightSmall; text: "+"; onClicked: titleSection.style(titleSection.info.size + 1, titleSection.info.color, titleSection.info.horizontal, titleSection.info.vertical, titleSection.info.shadow) }
                }
                Row {
                    width: parent.width; spacing: 6
                    Text { width: 52; anchors.verticalCenter: parent.verticalCenter; text: "Color"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS }
                    Repeater {
                        model: ["#FFFFFF", "#FFD84D", "#FF5A5A", "#59D9FF", "#6BE38B", "#000000"]
                        delegate: Rectangle {
                            width: 22; height: 22; radius: 11
                            color: modelData
                            border.width: titleSection.info && titleSection.info.color === modelData ? 2 : 1
                            border.color: titleSection.info && titleSection.info.color === modelData ? Theme.accent : Theme.borderStrong
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: titleSection.style(titleSection.info.size, modelData, titleSection.info.horizontal, titleSection.info.vertical, titleSection.info.shadow) }
                        }
                    }
                }
                Row {
                    width: parent.width; spacing: 4
                    Text { width: 52; anchors.verticalCenter: parent.verticalCenter; text: "Horiz."; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS }
                    Repeater {
                        model: ["Izq.", "Centro", "Der."]
                        delegate: PButton {
                            width: 56; height: Theme.controlHeightSmall; text: modelData
                            primary: titleSection.info && titleSection.info.horizontal === index
                            onClicked: titleSection.style(titleSection.info.size, titleSection.info.color, index, titleSection.info.vertical, titleSection.info.shadow)
                        }
                    }
                }
                Row {
                    width: parent.width; spacing: 4
                    Text { width: 52; anchors.verticalCenter: parent.verticalCenter; text: "Vert."; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS }
                    Repeater {
                        model: ["Arriba", "Medio", "Abajo"]
                        delegate: PButton {
                            width: 56; height: Theme.controlHeightSmall; text: modelData
                            primary: titleSection.info && titleSection.info.vertical === index
                            onClicked: titleSection.style(titleSection.info.size, titleSection.info.color, titleSection.info.horizontal, index, titleSection.info.shadow)
                        }
                    }
                }
                Row {
                    width: parent.width; spacing: 6
                    Text { width: 52; anchors.verticalCenter: parent.verticalCenter; text: "Sombra"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS }
                    Rectangle {
                        width: 38; height: 20; radius: 10
                        color: titleSection.info && titleSection.info.shadow ? Theme.accent : Theme.track
                        Rectangle { y: 2; x: titleSection.info && titleSection.info.shadow ? parent.width - width - 2 : 2; width: 16; height: 16; radius: 8; color: Theme.textPrimary }
                        MouseArea { anchors.fill: parent; onClicked: titleSection.style(titleSection.info.size, titleSection.info.color, titleSection.info.horizontal, titleSection.info.vertical, !titleSection.info.shadow) }
                    }
                }
                Row {
                    width: parent.width; spacing: 4
                    Text { width: 52; anchors.verticalCenter: parent.verticalCenter; text: "Fondo"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS }
                    Repeater {
                        model: ["Sin", "Oscuro", "Claro"]
                        delegate: PButton {
                            width: 56; height: Theme.controlHeightSmall; text: modelData
                            primary: titleSection.info && titleSection.info.background === index
                            onClicked: controller.setSelectedTitleLook(index, titleSection.info.opacity)
                        }
                    }
                }
                Row {
                    width: parent.width; spacing: 6
                    Text { width: 52; anchors.verticalCenter: parent.verticalCenter; text: "Opacidad"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS }
                    PButton { width: 28; height: Theme.controlHeightSmall; text: "−"; onClicked: controller.setSelectedTitleLook(titleSection.info.background, titleSection.info.opacity - 10) }
                    Text { width: 46; horizontalAlignment: Text.AlignHCenter; anchors.verticalCenter: parent.verticalCenter; text: titleSection.info && titleSection.info.opacity !== undefined ? titleSection.info.opacity + " %" : ""; color: Theme.textPrimary; font.family: Theme.mono; font.pixelSize: Theme.fontSizeM }
                    PButton { width: 28; height: Theme.controlHeightSmall; text: "+"; onClicked: controller.setSelectedTitleLook(titleSection.info.background, titleSection.info.opacity + 10) }
                }
                Text { width: parent.width; wrapMode: Text.WordWrap; text: "Mové o recortá el título en su pista. Ctrl+D lo duplica."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
            }

            // ---- CLIP (editable context) -------------------------------
            Column {
                visible: controller.selectedClipKey.length > 0
                width: parent.width
                spacing: 6
                SectionLabel { text: "CLIP" }
                Text { width: parent.width; text: inspector.selName; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeM; elide: Text.ElideMiddle }
                Text { width: parent.width; text: inspector.selRange; color: Theme.textSecondary; font.family: Theme.mono; font.pixelSize: Theme.fontSizeXS; elide: Text.ElideRight }
                Text {
                    visible: controller.selectedClipKind === "image" || controller.selectedClipKind === "color"
                    width: parent.width; wrapMode: Text.WordWrap
                    text: controller.selectedClipKind === "color" ? "Placa de color. Arrastrá un borde para cambiar su duración." : "Imagen fija. Arrastrá un borde para cambiar su duración."
                    color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS
                }
                PButton {
                    width: parent.width; height: Theme.controlHeightSmall
                    text: "Duplicar   Ctrl+D"
                    enabled: controller.canDuplicate
                    onClicked: controller.duplicateSelected()
                }
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
                            onAccepted: focus = false      // Enter hands the keyboard back to the editor
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
                // Fades: linear ramps at the start / end of the clip's sound (music, intros, effects).
                Column {
                    visible: controller.selectedClipHasAudio
                    width: parent.width
                    spacing: 6
                    Repeater {
                        model: [{label: "Entrada", fadeIn: true}, {label: "Salida", fadeIn: false}]
                        delegate: Row {
                            id: fadeRow
                            readonly property bool isIn: modelData.fadeIn
                            readonly property real current: isIn ? controller.selectedClipFadeIn : controller.selectedClipFadeOut
                            width: parent.width
                            spacing: 4
                            Text { width: 52; anchors.verticalCenter: parent.verticalCenter; text: modelData.label; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS }
                            Repeater {
                                model: [{label: "Sin", seconds: 0}, {label: "¼ s", seconds: 0.25}, {label: "½ s", seconds: 0.5}, {label: "1 s", seconds: 1}, {label: "2 s", seconds: 2}]
                                delegate: PButton {
                                    width: 36; height: Theme.controlHeightSmall; text: modelData.label
                                    primary: Math.abs(fadeRow.current - modelData.seconds) < 0.0005
                                    onClicked: fadeRow.isIn ? controller.setSelectedClipFades(modelData.seconds, controller.selectedClipFadeOut)
                                                            : controller.setSelectedClipFades(controller.selectedClipFadeIn, modelData.seconds)
                                }
                            }
                        }
                    }
                    Text { width: parent.width; wrapMode: Text.WordWrap; text: "Fundido lineal al empezar o terminar el clip."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
                }
                Text { visible: !controller.selectedClipHasAudio; width: parent.width; wrapMode: Text.WordWrap; text: controller.selectedClipKind === "image" || controller.selectedClipKind === "color" ? "Las imágenes y placas no tienen audio." : "Este clip no tiene audio."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeS }
            }

            // ---- MEDIA INFO (informative) ------------------------------
            Column {
                // The controller only describes the active source; hide it when
                // the selected clip belongs to a different media.
                visible: inspector.summary.length > 0 && controller.selectedTitleKey.length === 0
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
