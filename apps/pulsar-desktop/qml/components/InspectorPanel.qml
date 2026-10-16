import QtQuick
import "../theme/Theme.js" as Theme

// Contextual Inspector. VIDEO owns visual/title/transition controls, AUDIO
// owns audible clip controls, and EFFECTS is an honest implemented-state
// placeholder rather than a catalogue of non-functional controls.
Panel {
    id: inspector

    property string selName: ""
    property string selRange: ""
    property bool detailsExpanded: false
    property int currentTab: 0 // 0 video, 1 audio, 2 effects

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
        var parts = [codec.toLowerCase() === "disponible" ? "Audio" : codec.toUpperCase()]
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

    // What the Inspector is describing right now (drives the context chip and which groups appear).
    readonly property bool titleSelected: controller.selectedTitleKey.length > 0
    readonly property bool clipSelected: controller.selectedClipKey.length > 0
    readonly property bool trackSelected: !titleSelected && !clipSelected && controller.selectedVisualTrackKey.length > 0
    readonly property var selectedTrack: controller.timelineTracks.find(function(t) { return String(t.id) === controller.selectedVisualTrackKey }) || null
    readonly property string contextLabel: titleSelected ? "TÍTULO"
        : (!clipSelected ? (trackSelected ? "PISTA" : "") : (controller.selectedClipKind === "image" ? "FOTO"
        : (controller.selectedClipKind === "color" ? "PLACA"
        : (controller.selectedClipKind === "audio" ? "AUDIO" : "VIDEO"))))
    onContextLabelChanged: {
        // Follow a newly selected audio-only clip; preserve an explicit Effects
        // choice until editing context genuinely changes.
        if (contextLabel === "AUDIO") currentTab = 1
        else if (contextLabel.length > 0 && currentTab !== 2) currentTab = 0
    }

    Rectangle {
        id: contextChip
        visible: inspector.contextLabel.length > 0
        anchors.verticalCenter: titleText.verticalCenter
        anchors.right: parent.right; anchors.rightMargin: Theme.spaceL
        height: 18; width: chipText.implicitWidth + 14; radius: 9
        color: Theme.accentDim
        Text { id: chipText; anchors.centerIn: parent; text: inspector.contextLabel; color: Theme.focus; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1 }
    }

    Item {
        id: inspectorTabs
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: titleText.bottom; anchors.topMargin: Theme.spaceM
        anchors.leftMargin: Theme.spaceM; anchors.rightMargin: Theme.spaceM
        height: 34
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.borderSubtle }
        Repeater {
            model: ["VIDEO", "AUDIO", "EFECTOS"]
            delegate: Item {
                id: tab
                required property string modelData
                required property int index
                readonly property bool current: inspector.currentTab === index
                x: index * width; width: inspectorTabs.width / 3; height: inspectorTabs.height
                activeFocusOnTab: true
                Accessible.name: modelData
                Accessible.role: Accessible.PageTab
                Keys.onReturnPressed: inspector.currentTab = index
                Keys.onSpacePressed: inspector.currentTab = index
                Text {
                    anchors.centerIn: parent; text: tab.modelData
                    color: tab.current ? Theme.textPrimary : (tabMouse.containsMouse ? Theme.textPrimary : Theme.textMuted)
                    font.pixelSize: Theme.fontSizeXS; font.bold: tab.current; font.letterSpacing: 0.8
                    Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                }
                Rectangle {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                    height: tab.current ? 2 : 1
                    color: tab.current ? Theme.accent : (tab.activeFocus ? Theme.focus : "transparent")
                    Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                }
                MouseArea { id: tabMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { inspector.currentTab = index; tab.forceActiveFocus() } }
            }
        }
    }

    Flickable {
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: inspectorTabs.bottom; anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.spaceL; anchors.rightMargin: Theme.spaceL; anchors.topMargin: Theme.spaceM; anchors.bottomMargin: Theme.spaceM
        contentWidth: width
        contentHeight: body.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Column {
            id: body
            width: parent.width
            spacing: Theme.spaceM

            // ---- Nothing selected ------------------------------------------
            Column {
                visible: inspector.currentTab === 0 && !inspector.titleSelected && !inspector.clipSelected && !inspector.trackSelected && inspector.summary.length === 0
                width: parent.width
                spacing: 6
                Text { width: parent.width; text: "Nada seleccionado"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeM }
                Text { width: parent.width; wrapMode: Text.WordWrap; text: "Elegí un clip o un título en la timeline para ver y ajustar sus propiedades."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeS }
            }

            InspectorSection {
                visible: inspector.currentTab === 0 && inspector.trackSelected && inspector.selectedTrack !== undefined
                first: true; heading: "PISTA DE VIDEO"
                Text { width: parent.width; text: inspector.selectedTrack ? inspector.selectedTrack.label.replace("VIDEO", "Video") + (inspector.selectedTrack.primary ? (inspector.selectedTrack.visualIndex === 0 ? " · Base" : " · Principal") : "") : "";
                       color: Theme.textPrimary; font.pixelSize: Theme.fontSizeM; font.bold: true }
                Text { width: parent.width; wrapMode: Text.WordWrap;
                       text: inspector.selectedTrack && inspector.selectedTrack.primary ? "Pista principal: el audio del video se escucha desde acá." : "Las pistas superiores cubren las inferiores donde tienen imagen.";
                       color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS }
                PButton { visible: !!inspector.selectedTrack && !inspector.selectedTrack.primary && inspector.selectedTrack.empty;
                          width: parent.width; height: Theme.controlHeightSmall; text: "Quitar pista vacía";
                          onClicked: controller.removeVisualTrack(controller.selectedVisualTrackKey) }
            }

            // ---- TÍTULO ------------------------------------------------------
            Column {
                id: titleSection
                visible: inspector.currentTab === 0 && inspector.titleSelected
                width: parent.width
                spacing: Theme.spaceM
                readonly property var info: controller.selectedTitle
                function style(size, color, h, v, shadow) { controller.setSelectedTitleStyle(size, color, h, v, shadow) }
                function cur(key, fallback) { return info && info[key] !== undefined ? info[key] : fallback }

                InspectorSection {
                    first: true; heading: "TEXTO"
                    Rectangle {
                        width: parent.width; height: 70
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
                            text: titleSection.cur("text", "")
                            // Applied when you leave the box or press Ctrl+Enter (one undo step).
                            onActiveFocusChanged: if (!activeFocus) controller.setSelectedTitleText(text)
                            Keys.onPressed: function(event) {
                                if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && (event.modifiers & Qt.ControlModifier)) {
                                    controller.setSelectedTitleText(text); focus = false; event.accepted = true
                                } else if (event.key === Qt.Key_Escape) {
                                    text = titleSection.cur("text", ""); focus = false; event.accepted = true
                                }
                            }
                        }
                    }
                    Text { width: parent.width; text: "Ctrl+Enter aplica el texto"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
                }

                InspectorSection {
                    heading: "POSICIÓN"
                    Row {
                        spacing: Theme.spaceM
                        // 3x3 presets: pick where the block sits; dragging in the viewer overrides them.
                        Grid {
                            columns: 3; spacing: 3
                            Repeater {
                                model: 9
                                delegate: Rectangle {
                                    readonly property int h: index % 3
                                    readonly property int v: Math.floor(index / 3)
                                    readonly property bool on: !titleSection.cur("hasPosition", false) && titleSection.cur("horizontal", -1) === h && titleSection.cur("vertical", -1) === v
                                    width: 30; height: 20; radius: 4
                                    color: on ? Theme.accentDim : (cell.containsMouse ? Theme.bgHover : Theme.bgInput)
                                    border.width: 1; border.color: on ? Theme.accent : Theme.borderStrong
                                    Rectangle { anchors.centerIn: parent; width: 8; height: 5; radius: 2; color: on ? Theme.focus : Theme.textMuted }
                                    MouseArea { id: cell; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                        onClicked: titleSection.style(titleSection.cur("size", 7), titleSection.cur("color", "#FFFFFF"), parent.h, parent.v, titleSection.cur("shadow", true)) }
                                }
                            }
                        }
                        Column {
                            width: parent.parent.width - 3 * 30 - 6 - Theme.spaceM
                            spacing: 6
                            Text { width: parent.width; wrapMode: Text.WordWrap; text: titleSection.cur("hasPosition", false) ? "Posición libre" : "Arrastrá el título en el visor para ubicarlo donde quieras."; color: titleSection.cur("hasPosition", false) ? Theme.textPrimary : Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
                            PButton { visible: titleSection.cur("hasPosition", false); width: parent.width; height: Theme.controlHeightSmall; text: "Restablecer"; onClicked: controller.resetSelectedTitlePosition() }
                        }
                    }
                }

                InspectorSection {
                    heading: "ESTILO"
                    FieldRow {
                        label: "Tamaño"
                        Stepper {
                            valueText: titleSection.cur("size", 0) + " %"
                            onDecrement: titleSection.style(titleSection.info.size - 1, titleSection.info.color, titleSection.info.horizontal, titleSection.info.vertical, titleSection.info.shadow)
                            onIncrement: titleSection.style(titleSection.info.size + 1, titleSection.info.color, titleSection.info.horizontal, titleSection.info.vertical, titleSection.info.shadow)
                        }
                    }
                    FieldRow {
                        label: "Color"
                        Repeater {
                            model: ["#FFFFFF", "#FFD84D", "#FF5A5A", "#59D9FF", "#6BE38B", "#000000"]
                            delegate: Rectangle {
                                readonly property bool on: titleSection.cur("color", "") === modelData
                                width: 22; height: 22; radius: 11
                                color: modelData
                                border.width: on ? 2 : 1
                                border.color: on ? Theme.accent : Theme.borderStrong
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: titleSection.style(titleSection.info.size, modelData, titleSection.info.horizontal, titleSection.info.vertical, titleSection.info.shadow) }
                            }
                        }
                    }
                    FieldRow {
                        label: "Sombra"
                        Rectangle {
                            width: 38; height: 20; radius: 10
                            color: titleSection.cur("shadow", false) ? Theme.accent : Theme.track
                            Rectangle { y: 2; x: titleSection.cur("shadow", false) ? parent.width - width - 2 : 2; width: 16; height: 16; radius: 8; color: Theme.textPrimary }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: titleSection.style(titleSection.info.size, titleSection.info.color, titleSection.info.horizontal, titleSection.info.vertical, !titleSection.info.shadow) }
                        }
                    }
                    FieldRow {
                        label: "Fondo"
                        Segmented {
                            width: 168
                            model: ["Sin", "Oscuro", "Claro"]
                            current: titleSection.cur("background", -1)
                            onActivated: function(index) { controller.setSelectedTitleLook(index, titleSection.info.opacity) }
                        }
                    }
                    FieldRow {
                        label: "Opacidad"
                        Stepper {
                            valueText: titleSection.cur("opacity", 0) + " %"
                            onDecrement: controller.setSelectedTitleLook(titleSection.info.background, titleSection.info.opacity - 10)
                            onIncrement: controller.setSelectedTitleLook(titleSection.info.background, titleSection.info.opacity + 10)
                        }
                    }
                }
                Text { width: parent.width; wrapMode: Text.WordWrap; text: "Mové o recortá el título en su pista. Ctrl+D lo duplica."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
            }

            // ---- CLIP ----------------------------------------------------------
            InspectorSection {
                visible: inspector.currentTab === 0 && inspector.clipSelected
                first: true; heading: "CLIP"
                Text { width: parent.width; text: inspector.selName; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeM; font.bold: true; elide: Text.ElideMiddle }
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
                Text {
                    visible: controller.selectedClipKind !== "audio" && controller.timelineTracks.filter(function(t) { return t.kind === "video" }).length > 1
                    text: "Mover a otra pista"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeXS
                }
                Repeater {
                    model: controller.selectedClipKind !== "audio" ? controller.timelineTracks : []
                    delegate: PButton {
                        visible: modelData.kind === "video" && controller.selectedVisualTrackKey !== String(modelData.id)
                        width: parent.width; height: Theme.controlHeightSmall
                        text: "Mover a " + modelData.label.replace("VIDEO", "Video")
                        onClicked: controller.moveSelectedClipToVisualTrack(String(modelData.id))
                    }
                }
            }

            // ---- TRANSFORMACIÓN (E2.1 mínimo) ----------------------------------
            InspectorSection {
                visible: inspector.currentTab === 0 && inspector.clipSelected && controller.selectedClipKind !== "audio"
                heading: "TRANSFORMACIÓN"
                FieldRow {
                    label: "Pos X"
                    Stepper {
                        valueText: Number(controller.selectedClipPosX).toFixed(2)
                        onDecrement: controller.setSelectedClipTransform(controller.selectedClipPosX - 0.05, controller.selectedClipPosY, controller.selectedClipScale, controller.selectedClipRotation, controller.selectedClipOpacity)
                        onIncrement: controller.setSelectedClipTransform(controller.selectedClipPosX + 0.05, controller.selectedClipPosY, controller.selectedClipScale, controller.selectedClipRotation, controller.selectedClipOpacity)
                    }
                }
                FieldRow {
                    label: "Pos Y"
                    Stepper {
                        valueText: Number(controller.selectedClipPosY).toFixed(2)
                        onDecrement: controller.setSelectedClipTransform(controller.selectedClipPosX, controller.selectedClipPosY - 0.05, controller.selectedClipScale, controller.selectedClipRotation, controller.selectedClipOpacity)
                        onIncrement: controller.setSelectedClipTransform(controller.selectedClipPosX, controller.selectedClipPosY + 0.05, controller.selectedClipScale, controller.selectedClipRotation, controller.selectedClipOpacity)
                    }
                }
                FieldRow {
                    label: "Escala"
                    Stepper {
                        valueText: Number(controller.selectedClipScale).toFixed(2)
                        onDecrement: controller.setSelectedClipTransform(controller.selectedClipPosX, controller.selectedClipPosY, Math.max(0.01, controller.selectedClipScale - 0.05), controller.selectedClipRotation, controller.selectedClipOpacity)
                        onIncrement: controller.setSelectedClipTransform(controller.selectedClipPosX, controller.selectedClipPosY, Math.min(100, controller.selectedClipScale + 0.05), controller.selectedClipRotation, controller.selectedClipOpacity)
                    }
                }
                FieldRow {
                    label: "Rotación"
                    Stepper {
                        valueText: Number(controller.selectedClipRotation).toFixed(0) + "°"
                        onDecrement: controller.setSelectedClipTransform(controller.selectedClipPosX, controller.selectedClipPosY, controller.selectedClipScale, controller.selectedClipRotation - 5, controller.selectedClipOpacity)
                        onIncrement: controller.setSelectedClipTransform(controller.selectedClipPosX, controller.selectedClipPosY, controller.selectedClipScale, controller.selectedClipRotation + 5, controller.selectedClipOpacity)
                    }
                }
                FieldRow {
                    label: "Opacidad"
                    Stepper {
                        valueText: Math.round(controller.selectedClipOpacity * 100) + " %"
                        onDecrement: controller.setSelectedClipTransform(controller.selectedClipPosX, controller.selectedClipPosY, controller.selectedClipScale, controller.selectedClipRotation, Math.max(0, controller.selectedClipOpacity - 0.05))
                        onIncrement: controller.setSelectedClipTransform(controller.selectedClipPosX, controller.selectedClipPosY, controller.selectedClipScale, controller.selectedClipRotation, Math.min(1, controller.selectedClipOpacity + 0.05))
                    }
                }
                PButton { width: parent.width; height: Theme.controlHeightSmall; text: "Restablecer"; onClicked: controller.resetSelectedClipTransform() }
                Text { width: parent.width; wrapMode: Text.WordWrap; text: "Posición en anchos/alto del lienzo. Escala uniforme y giro en grados."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
            }

            InspectorSection {
                visible: inspector.currentTab === 0 && inspector.clipSelected && controller.selectedClipKind !== "audio"
                heading: "TRANSICIÓN"
                PButton {
                    width: parent.width; height: Theme.controlHeightSmall
                    text: controller.selectedClipHasTransition ? "Quitar fundido cruzado" : "Agregar fundido cruzado"
                    onClicked: controller.selectedClipHasTransition ? controller.removeSelectedTransition() : controller.addCrossDissolve()
                }
                Text { width: parent.width; wrapMode: Text.WordWrap; text: "Entre dos clips contiguos. Usa hasta 1 s del margen del segundo clip."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
            }

            // ---- AUDIO ---------------------------------------------------------
            InspectorSection {
                visible: inspector.currentTab === 1 && inspector.clipSelected
                first: true; heading: "CLIP DE AUDIO"
                FieldRow {
                    visible: controller.selectedClipHasAudio
                    label: "Ganancia"
                    PButton { width: 26; height: Theme.controlHeightSmall; text: "−"; onClicked: controller.setSelectedClipGainDb(controller.selectedClipGainDb - 1) }
                    Rectangle {
                        width: 66; height: Theme.controlHeightSmall
                        color: Theme.bgInput; radius: Theme.radiusM
                        border.width: 1
                        border.color: gainInput.activeFocus ? Theme.focus : Theme.borderStrong
                        TextInput {
                            id: gainInput
                            anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 22
                            verticalAlignment: TextInput.AlignVCenter
                            horizontalAlignment: TextInput.AlignRight
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeS
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
                    PButton { width: 26; height: Theme.controlHeightSmall; text: "+"; onClicked: controller.setSelectedClipGainDb(controller.selectedClipGainDb + 1) }
                }
                // One unambiguous action; the state is the clip's single mute flag.
                Rectangle {
                    id: muteButton
                    visible: controller.selectedClipHasAudio
                    width: parent.width; height: 30
                    radius: Theme.radiusM
                    activeFocusOnTab: true
                     color: muteMouse.pressed ? Theme.bgPressed
                          : (controller.selectedClipMuted ? (muteMouse.containsMouse ? Theme.mutedActiveHover : Theme.mutedActive)
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
                Repeater {
                    model: controller.selectedClipHasAudio ? [{label: "Entrada", fadeIn: true}, {label: "Salida", fadeIn: false}] : []
                    delegate: FieldRow {
                        id: fadeRow
                        readonly property bool isIn: modelData.fadeIn
                        readonly property real current: isIn ? controller.selectedClipFadeIn : controller.selectedClipFadeOut
                        readonly property var steps: [{label: "Sin", seconds: 0}, {label: "¼ s", seconds: 0.25}, {label: "½ s", seconds: 0.5}, {label: "1 s", seconds: 1}, {label: "2 s", seconds: 2}]
                        label: modelData.label
                        Segmented {
                            width: 190
                            model: fadeRow.steps.map(function(step) { return step.label })
                            current: { for (var i = 0; i < fadeRow.steps.length; ++i) if (Math.abs(fadeRow.current - fadeRow.steps[i].seconds) < 0.0005) return i; return -1 }
                            onActivated: function(index) {
                                var seconds = fadeRow.steps[index].seconds
                                if (fadeRow.isIn) controller.setSelectedClipFades(seconds, controller.selectedClipFadeOut)
                                else controller.setSelectedClipFades(controller.selectedClipFadeIn, seconds)
                            }
                        }
                    }
                }
                Text { visible: controller.selectedClipHasAudio; width: parent.width; wrapMode: Text.WordWrap; text: "Los fundidos son lineales, al empezar o terminar el clip."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
                Text { visible: !controller.selectedClipHasAudio; width: parent.width; wrapMode: Text.WordWrap;
                       text: controller.selectedClipKind === "image" || controller.selectedClipKind === "color"
                           ? "Las fotos y placas no tienen audio."
                           : (inspector.selectedTrack && !inspector.selectedTrack.primary
                               ? "El audio de esta pista de video no se reproduce. Usá Video 1 para el sonido del clip."
                               : "Este clip no tiene audio.");
                       color: Theme.textMuted; font.pixelSize: Theme.fontSizeS }
            }

            Column {
                visible: inspector.currentTab === 1 && !inspector.clipSelected
                width: parent.width; spacing: Theme.spaceS
                Text { width: parent.width; text: "Audio"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeM }
                Text { width: parent.width; wrapMode: Text.WordWrap; text: "Seleccioná un clip con audio para ajustar su ganancia, silencio y fundidos. Los controles M y S de cada pista están en la timeline."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeS }
            }

            Column {
                visible: inspector.currentTab === 2
                width: parent.width; spacing: Theme.spaceS
                Text { width: parent.width; text: "Sin efectos aplicados"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeM }
                Text { width: parent.width; wrapMode: Text.WordWrap; text: "Los controles de efectos compatibles aparecerán acá."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeS }
            }

            // ---- MEDIO (informative) -----------------------------------------
            InspectorSection {
                // The controller only describes the active source; hide it when the selection is a title.
                 visible: inspector.currentTab === 0 && inspector.summary.length > 0 && !inspector.titleSelected && !inspector.trackSelected
                first: !inspector.clipSelected; heading: "MEDIO"
                // The clip above already names its media; avoid saying it twice.
                Text { visible: !inspector.clipSelected || inspector.selName !== controller.mediaName; width: parent.width; text: controller.mediaName; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS; elide: Text.ElideMiddle }
                Text { width: parent.width; text: inspector.summary; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS; wrapMode: Text.WordWrap }
            }

            // ---- Technical details (collapsed) ---------------------------------
            Column {
                  visible: inspector.currentTab === 0 && inspector.rows.length > 0 && !inspector.titleSelected && !inspector.trackSelected
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
                        Text { text: "Detalles técnicos"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeS }
                    }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: inspector.detailsExpanded = !inspector.detailsExpanded }
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
