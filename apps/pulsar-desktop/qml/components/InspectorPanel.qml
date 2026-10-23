import QtQuick
import "../theme/Theme.js" as Theme

// Contextual Inspector. VIDEO owns visual/title/transition controls, AUDIO
// owns audible clip controls, and EFFECTS exposes the currently supported
// non-destructive audio processors.
Panel {
    id: inspector

    property string selName: ""
    property string selRange: ""
    property bool detailsExpanded: false
    property int currentTab: 0 // 0 video, 1 audio, 2 effects

    function setFx(gain, duration, startCutoff, endCutoff, reverb, curve, enabled) {
        controller.setSelectedDistantMusic(gain, duration, startCutoff, endCutoff, reverb, curve, enabled)
    }
    function fxInput(value, fallback) {
        var n = Number(value)
        return isFinite(n) ? n : fallback
    }

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

            // ---- Pulse Map / Scene Cards ---------------------------------
            InspectorSection {
                id: pulseSection
                first: true
                heading: "PULSE MAP"
                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: controller.analysisRunning ? "Leyendo energía, transitorios y golpes…" :
                          (controller.sceneCards.length > 0 ? "Mapa editorial listo: " + controller.sceneCards.length + " escenas" :
                           "Detectá impactos y cambios de energía para encontrar cortes con intención.")
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS
                }
                Canvas {
                    id: pulseCanvas
                    width: parent.width; height: 58
                    visible: controller.pulseMap.length > 0
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.fillStyle = Theme.bgInput
                        ctx.fillRect(0, 0, width, height)
                        var points = controller.pulseMap
                        if (points.length < 1) return
                        ctx.beginPath()
                        for (var i = 0; i < points.length; ++i) {
                            var x = points[i].time / Math.max(0.001, points[points.length - 1].time) * width
                            var y = height - Math.min(1, Number(points[i].energy) * 2.0) * (height - 8) - 4
                            if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                        }
                        ctx.strokeStyle = Theme.accent; ctx.lineWidth = 1.5; ctx.stroke()
                        ctx.fillStyle = Theme.warning
                        for (var j = 0; j < points.length; ++j) {
                            if (!points[j].pulse) continue
                            var px = points[j].time / Math.max(0.001, points[points.length - 1].time) * width
                            ctx.fillRect(px - 1, 2, 2, height - 4)
                        }
                    }
                    Connections {
                        target: controller
                        function onAnalysisChanged() { pulseCanvas.requestPaint() }
                    }
                }
                Row {
                    width: parent.width; spacing: Theme.spaceS
                    PButton {
                        text: controller.analysisRunning ? "Cancelar análisis" : "Analizar clip"
                        enabled: controller.analysisRunning || controller.selectedClipHasAudio
                        onClicked: controller.analysisRunning ? controller.cancelAudioAnalysis() : controller.analyzeSelectedAudio()
                    }
                    Text { visible: controller.pulseMap.length > 0; anchors.verticalCenter: parent.verticalCenter; text: controller.pulseMap.length + " puntos"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
                }
                Flow {
                    width: parent.width; spacing: Theme.spaceS
                    Repeater {
                        model: controller.sceneCards
                        delegate: Rectangle {
                            required property var modelData
                            width: Math.max(76, Math.min(112, pulseSection.width / 3 - 8)); height: 34
                            radius: Theme.radiusS; color: Theme.bgInput; border.color: modelData.label === "Impact" ? Theme.warning : Theme.borderSubtle
                            Column {
                                anchors.centerIn: parent
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeXS; font.bold: true }
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.pulses + " pulsos"; color: Theme.textMuted; font.pixelSize: 10 }
                            }
                        }
                    }
                }
            }

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
                heading: "TRANSFORMACIÓN · PRECISIÓN"
                Rectangle {
                    width: parent.width; height: precisionHelp.implicitHeight + 2 * Theme.spaceS
                    radius: Theme.radiusM; color: Theme.accentDim
                    border.width: 1; border.color: Theme.accent
                    Text {
                        id: precisionHelp
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.leftMargin: Theme.spaceS; anchors.rightMargin: Theme.spaceS
                        anchors.verticalCenter: parent.verticalCenter
                        wrapMode: Text.WordWrap
                        text: controller.selectedClipOnScreen
                            ? "Arrastrá la imagen en el visor para moverla. Usá las esquinas para escalar."
                            : "Llevá el cabezal sobre este clip para editarlo directamente en el visor."
                        color: Theme.textPrimary; font.pixelSize: Theme.fontSizeS
                    }
                }
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
                Text { width: parent.width; wrapMode: Text.WordWrap; text: "Estos valores son controles de precisión. La posición usa el ancho y alto del lienzo; el giro está en grados."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
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
                Text { width: parent.width; text: "PULSAR Audio FX"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeM; font.bold: true }
                Text { width: parent.width; wrapMode: Text.WordWrap; text: "Distant Music: música que se acerca con volumen y agudos progresivos."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeS }
                PButton { width: parent.width; height: Theme.controlHeightSmall; primary: true; text: "Aplicar preset Distant Music"; enabled: inspector.clipSelected && controller.selectedClipHasAudio; onClicked: controller.applySelectedDistantMusicPreset() }
                FieldRow { visible: inspector.clipSelected && controller.selectedClipHasAudio; label: "Recetas"; Row { spacing: 4
                        Repeater { model: controller.cinematicRecipes
                            delegate: PButton { width: Math.max(58, implicitWidth + 14); height: Theme.controlHeightSmall; text: modelData.name; onClicked: controller.applySelectedCinematicRecipe(modelData.id) }
                        }
                    } }
                FieldRow { visible: inspector.clipSelected && controller.selectedClipHasAudio; label: "Estado"; PButton { width: 90; height: Theme.controlHeightSmall; text: controller.selectedDistantMusicEnabled ? "Activado" : "Bypass"; onClicked: inspector.setFx(controller.selectedDistantMusicGainDb, controller.selectedDistantMusicDuration, controller.selectedDistantMusicStartCutoff, controller.selectedDistantMusicEndCutoff, controller.selectedDistantMusicReverb, controller.selectedDistantMusicCurve, !controller.selectedDistantMusicEnabled) } }
                FieldRow { visible: inspector.clipSelected && controller.selectedClipHasAudio; label: "Distancia"; Rectangle { width: 72; height: Theme.controlHeightSmall; color: Theme.bgInput; radius: Theme.radiusM; border.width: 1; border.color: fxGain.activeFocus ? Theme.focus : Theme.borderStrong; TextInput { id: fxGain; anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 22; verticalAlignment: TextInput.AlignVCenter; horizontalAlignment: TextInput.AlignRight; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeS; font.family: Theme.mono; validator: DoubleValidator { bottom: -60; top: 0; decimals: 1 }; text: Number(controller.selectedDistantMusicGainDb).toFixed(1); selectByMouse: true; onEditingFinished: { inspector.setFx(fxInput(text, -30), controller.selectedDistantMusicDuration, controller.selectedDistantMusicStartCutoff, controller.selectedDistantMusicEndCutoff, controller.selectedDistantMusicReverb, controller.selectedDistantMusicCurve, true); focus = false } } Text { anchors.right: parent.right; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter; text: "dB"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS } } }
                FieldRow { visible: inspector.clipSelected && controller.selectedClipHasAudio; label: "Duración"; Rectangle { width: 72; height: Theme.controlHeightSmall; color: Theme.bgInput; radius: Theme.radiusM; border.width: 1; border.color: fxDuration.activeFocus ? Theme.focus : Theme.borderStrong; TextInput { id: fxDuration; anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 22; verticalAlignment: TextInput.AlignVCenter; horizontalAlignment: TextInput.AlignRight; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeS; font.family: Theme.mono; validator: DoubleValidator { bottom: 0.1; top: 60; decimals: 2 }; text: Number(controller.selectedDistantMusicDuration).toFixed(1); selectByMouse: true; onEditingFinished: { inspector.setFx(controller.selectedDistantMusicGainDb, fxInput(text, 4), controller.selectedDistantMusicStartCutoff, controller.selectedDistantMusicEndCutoff, controller.selectedDistantMusicReverb, controller.selectedDistantMusicCurve, true); focus = false } } Text { anchors.right: parent.right; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter; text: "s"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS } } }
                FieldRow { visible: inspector.clipSelected && controller.selectedClipHasAudio; label: "Filtro inicial"; PSlider { width: 130; value: Math.log(Math.max(20, controller.selectedDistantMusicStartCutoff) / 20) / Math.log(1000); onMoved: function(v) { inspector.setFx(controller.selectedDistantMusicGainDb, controller.selectedDistantMusicDuration, 20 * Math.pow(1000, v), controller.selectedDistantMusicEndCutoff, controller.selectedDistantMusicReverb, controller.selectedDistantMusicCurve, true) } } Text { text: Math.round(controller.selectedDistantMusicStartCutoff) + " Hz"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS } }
                FieldRow { visible: inspector.clipSelected && controller.selectedClipHasAudio; label: "Filtro final"; PSlider { width: 130; value: Math.log(Math.max(20, controller.selectedDistantMusicEndCutoff) / 20) / Math.log(1000); onMoved: function(v) { inspector.setFx(controller.selectedDistantMusicGainDb, controller.selectedDistantMusicDuration, controller.selectedDistantMusicStartCutoff, 20 * Math.pow(1000, v), controller.selectedDistantMusicReverb, controller.selectedDistantMusicCurve, true) } } Text { text: Math.round(controller.selectedDistantMusicEndCutoff) + " Hz"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS } }
                FieldRow { visible: inspector.clipSelected && controller.selectedClipHasAudio; label: "Reverb"; Segmented { width: 150; model: ["Sin", "Ligera", "Más"]; current: controller.selectedDistantMusicReverb < 0.05 ? 0 : controller.selectedDistantMusicReverb < 0.3 ? 1 : 2; onActivated: function(index) { controller.setSelectedDistantMusic(controller.selectedDistantMusicGainDb, controller.selectedDistantMusicDuration, controller.selectedDistantMusicStartCutoff, controller.selectedDistantMusicEndCutoff, [0, 0.18, 0.45][index], controller.selectedDistantMusicCurve, true) } } }
                FieldRow { visible: inspector.clipSelected && controller.selectedClipHasAudio; label: "Curva"; Segmented { width: 180; model: ["Suave", "Progresiva", "Explosiva"]; current: controller.selectedDistantMusicCurve; onActivated: function(index) { controller.setSelectedDistantMusic(controller.selectedDistantMusicGainDb, controller.selectedDistantMusicDuration, controller.selectedDistantMusicStartCutoff, controller.selectedDistantMusicEndCutoff, controller.selectedDistantMusicReverb, index, true) } } }
                FieldRow { visible: inspector.clipSelected && controller.selectedClipHasAudio; label: "Automatización"; Text { text: controller.selectedDistantMusicKeyframeCount + " keyframes"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS } }
                Row { visible: inspector.clipSelected && controller.selectedClipHasAudio; width: parent.width; spacing: Theme.spaceS
                    PButton { width: (parent.width - Theme.spaceS) / 2; height: Theme.controlHeightSmall; text: "Crear entrada/salida"; onClicked: controller.setSelectedDistantMusicGainKeyframes([{time: 0, value: controller.selectedDistantMusicGainDb, interpolation: 0}, {time: controller.selectedDistantMusicDuration, value: 0, interpolation: 2}]) }
                    PButton { width: (parent.width - Theme.spaceS) / 2; height: Theme.controlHeightSmall; text: "Limpiar"; onClicked: controller.setSelectedDistantMusicGainKeyframes([]) }
                }
                Column { visible: inspector.clipSelected && controller.selectedClipHasAudio && controller.selectedDistantMusicKeyframeCount > 0; width: parent.width; spacing: 4
                    Repeater { model: controller.selectedDistantMusicGainKeyframes
                        delegate: Row { width: parent.width; spacing: 4
                            Text { width: 16; text: (index + 1); color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS; anchors.verticalCenter: parent.verticalCenter }
                            Rectangle { width: 54; height: Theme.controlHeightSmall; color: Theme.bgInput; radius: Theme.radiusM; border.width: 1; border.color: keyTime.activeFocus ? Theme.focus : Theme.borderStrong
                                TextInput { id: keyTime; anchors.fill: parent; anchors.margins: 5; text: Number(modelData.time).toFixed(2); color: Theme.textPrimary; font.family: Theme.mono; font.pixelSize: Theme.fontSizeXS; validator: DoubleValidator { bottom: 0; top: 60; decimals: 2 }; onEditingFinished: controller.setSelectedDistantMusicGainKeyframe(index, Number(text), Number(modelData.value), Number(modelData.interpolation)) }
                            }
                            Rectangle { width: 54; height: Theme.controlHeightSmall; color: Theme.bgInput; radius: Theme.radiusM; border.width: 1; border.color: keyValue.activeFocus ? Theme.focus : Theme.borderStrong
                                TextInput { id: keyValue; anchors.fill: parent; anchors.margins: 5; text: Number(modelData.value).toFixed(1); color: Theme.textPrimary; font.family: Theme.mono; font.pixelSize: Theme.fontSizeXS; validator: DoubleValidator { bottom: -60; top: 0; decimals: 1 }; onEditingFinished: controller.setSelectedDistantMusicGainKeyframe(index, Number(modelData.time), Number(text), Number(modelData.interpolation)) }
                            }
                            Segmented { width: 92; model: ["H", "L", "S"]; current: Number(modelData.interpolation); onActivated: function(choice) { controller.setSelectedDistantMusicGainKeyframe(index, Number(modelData.time), Number(modelData.value), choice) } }
                            PButton { width: 24; height: Theme.controlHeightSmall; text: "×"; onClicked: controller.removeSelectedDistantMusicGainKeyframe(index) }
                        }
                    }
                }
                Canvas { visible: inspector.clipSelected && controller.selectedClipHasAudio; width: parent.width; height: 54; antialiasing: true; onPaint: { var p = getContext("2d"); p.clearRect(0, 0, width, height); p.strokeStyle = Theme.borderStrong; p.lineWidth = 1; p.beginPath(); p.moveTo(0, height - 1); p.lineTo(width, height - 1); p.stroke(); p.strokeStyle = Theme.accent; p.lineWidth = 2; p.beginPath(); for (var i = 0; i <= 40; ++i) { var x = width * i / 40; var q = i / 40; var shaped = controller.selectedDistantMusicCurve === 0 ? q*q*(3-2*q) : controller.selectedDistantMusicCurve === 2 ? Math.sqrt(q) : q*q; var y = height - 4 - shaped * (height - 10); if (i === 0) p.moveTo(x, y); else p.lineTo(x, y); } p.stroke(); var keys = controller.selectedDistantMusicGainKeyframes; for (var k = 0; k < keys.length; ++k) { var key = keys[k]; var kx = width * Math.max(0, Math.min(1, key.time / Math.max(0.1, controller.selectedDistantMusicDuration))); var ky = height - 4 - Math.max(0, Math.min(1, (key.value - controller.selectedDistantMusicGainDb) / Math.max(0.1, 0 - controller.selectedDistantMusicGainDb))) * (height - 10); p.fillStyle = Theme.focus; p.beginPath(); p.arc(kx, ky, 3, 0, 2 * Math.PI); p.fill(); } } Connections { target: controller; function onTimelineChanged() { parent.requestPaint() } } }
                InspectorSection {
                    visible: inspector.clipSelected && controller.selectedClipHasAudio
                    heading: "TELEPHONE"
                    Text { width: parent.width; wrapMode: Text.WordWrap; text: "Banda estrecha y saturación suave para radio, llamada o memoria sonora."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeS }
                    Row { width: parent.width; spacing: Theme.spaceS
                        PButton { width: (parent.width - Theme.spaceS) / 2; height: Theme.controlHeightSmall; primary: true; text: "Aplicar Telephone"; onClicked: controller.applySelectedTelephonePreset() }
                        PButton { width: (parent.width - Theme.spaceS) / 2; height: Theme.controlHeightSmall; text: controller.selectedTelephoneEnabled ? "Bypass" : "Activar"; onClicked: controller.setSelectedTelephone(controller.selectedTelephoneMix, controller.selectedTelephoneDrive, !controller.selectedTelephoneEnabled) }
                    }
                    FieldRow { label: "Mezcla"; PSlider { width: 150; value: controller.selectedTelephoneMix; onMoved: controller.setSelectedTelephone(value, controller.selectedTelephoneDrive, true) } Text { text: Math.round(controller.selectedTelephoneMix * 100) + "%"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS } }
                    FieldRow { label: "Drive"; PSlider { width: 150; value: controller.selectedTelephoneDrive; onMoved: controller.setSelectedTelephone(controller.selectedTelephoneMix, value, true) } Text { text: Math.round(controller.selectedTelephoneDrive * 100) + "%"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS } }
                }
                InspectorSection {
                    visible: inspector.clipSelected && controller.selectedClipHasAudio
                    heading: "UNDERWATER"
                    Text { width: parent.width; wrapMode: Text.WordWrap; text: "Música sumergida: graves suaves, agudos velados y movimiento espacial sutil."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeS }
                    Row { width: parent.width; spacing: Theme.spaceS
                        PButton { width: (parent.width - Theme.spaceS) / 2; height: Theme.controlHeightSmall; primary: true; text: "Aplicar Underwater"; onClicked: controller.applySelectedUnderwaterPreset() }
                        PButton { width: (parent.width - Theme.spaceS) / 2; height: Theme.controlHeightSmall; text: controller.selectedUnderwaterEnabled ? "Bypass" : "Activar"; onClicked: controller.setSelectedUnderwater(controller.selectedUnderwaterMix, controller.selectedUnderwaterDepth, !controller.selectedUnderwaterEnabled) }
                    }
                    FieldRow { label: "Mezcla"; PSlider { width: 150; value: controller.selectedUnderwaterMix; onMoved: controller.setSelectedUnderwater(value, controller.selectedUnderwaterDepth, true) } Text { text: Math.round(controller.selectedUnderwaterMix * 100) + "%"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS } }
                    FieldRow { label: "Profundidad"; PSlider { width: 150; value: controller.selectedUnderwaterDepth; onMoved: controller.setSelectedUnderwater(controller.selectedUnderwaterMix, value, true) } Text { text: Math.round(controller.selectedUnderwaterDepth * 100) + "%"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS } }
                }
                PButton { visible: inspector.clipSelected && controller.selectedClipHasAudio; width: parent.width; height: Theme.controlHeightSmall; text: "Restablecer efecto"; onClicked: controller.setSelectedDistantMusic(-30, 4, 900, 18000, 0.18, 1, false) }
                Text { visible: inspector.clipSelected && !controller.selectedClipHasAudio; width: parent.width; wrapMode: Text.WordWrap; text: "Seleccioná un clip con audio para aplicar Distant Music."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeS }
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
