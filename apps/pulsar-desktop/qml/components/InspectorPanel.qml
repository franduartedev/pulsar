import QtQuick
import "../theme/Theme.js" as Theme

// Real inspector: typographic sections with hairline rhythm, subdued keys,
// clear values, intelligent wrapping. No cards-in-cards.
Panel {
    id: inspector

    function metaSection(key) {
        if (key.indexOf("Audio") === 0) {
            return "AUDIO"
        }
        if (key === "Nombre" || key === "Contenedor") {
            return "ARCHIVO"
        }
        if (key === "Stream" || key === "Codec" || key === "Resolución"
                || key.indexOf("FPS") === 0 || key === "Time base") {
            return "VIDEO"
        }
        if (key === "Pixel format" || key === "Color range" || key === "Primaries"
                || key === "Transfer" || key === "Matrix") {
            return "COLOR"
        }
        if (key === "Preview") {
            return "SALIDA"
        }
        return "OTROS"
    }
    function parseMeta(text) {
        var out = []
        if (!text) {
            return out
        }
        var order = ["ARCHIVO", "VIDEO", "COLOR", "SALIDA", "OTROS"]
        var buckets = {}
        var lines = text.split("\n")
        for (var i = 0; i < lines.length; ++i) {
            var idx = lines[i].indexOf(":")
            var key = idx >= 0 ? lines[i].slice(0, idx).trim() : lines[i].trim()
            var val = idx >= 0 ? lines[i].slice(idx + 1).trim() : ""
            if (key.length === 0) {
                continue
            }
            var sec = metaSection(key)
            if (!buckets[sec]) {
                buckets[sec] = []
            }
            buckets[sec].push({key: key, value: val})
        }
        for (var s = 0; s < order.length; ++s) {
            if (buckets[order[s]] && buckets[order[s]].length > 0) {
                out.push({header: order[s], rows: buckets[order[s]]})
            }
        }
        return out
    }
    property var metaGroups: parseMeta(controller.metadata)
    property bool technicalExpanded: false

    Connections {
        target: controller
        function onTimelineChanged() {
            if (!gainInput.activeFocus) {
                gainInput.text = Number(controller.selectedClipGainDb).toFixed(1)
            }
        }
    }

    Row {
        id: titleRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 16
        anchors.rightMargin: 12
        anchors.topMargin: 14
        spacing: 8
        SectionLabel { anchors.verticalCenter: parent.verticalCenter; text: "INSPECTOR"; color: Theme.textBright }
        StatusDot { anchors.verticalCenter: parent.verticalCenter; tone: controller.errorMessage.length > 0 ? "error" : (controller.hasMedia ? "live" : "idle") }
    }
    Flickable {
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: titleRow.bottom; anchors.bottom: parent.bottom
        anchors.leftMargin: 16; anchors.rightMargin: 16; anchors.topMargin: 8; anchors.bottomMargin: 12
        contentWidth: width
        contentHeight: body.height
        clip: true
        Column {
            id: body
            width: parent.width
            spacing: 12
            Rectangle {
                visible: controller.selectedClipKey.length === 0 && metaGroups.length === 0
                width: parent.width; height: 112; color: Theme.panelAlt; border.color: Theme.borderSoft; radius: Theme.radiusS
                Column {
                    anchors.centerIn: parent; spacing: 7
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: "INSPECTOR VACÍO"; color: Theme.text; font.pixelSize: Theme.fontSizeS; font.bold: true; font.letterSpacing: 1.2 }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Seleccioná un clip o importá un medio"; color: Theme.muted; font.pixelSize: Theme.fontSizeS; horizontalAlignment: Text.AlignHCenter }
                    Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 34; height: 2; color: Theme.accent; opacity: 0.7 }
                }
            }
            Column {
                visible: controller.selectedClipKey.length > 0
                width: parent.width
                spacing: 6
                SectionLabel { width: parent.width; text: "AUDIO DEL CLIP"; color: Theme.textBright }
                Rectangle { width: parent.width; height: 1; color: Theme.borderSoft }
                Row {
                    width: parent.width
                    spacing: 6
                    Text { text: "GAIN"; color: Theme.faint; font.pixelSize: Theme.fontSizeXS; font.bold: true; font.letterSpacing: 1; anchors.verticalCenter: parent.verticalCenter }
                    PButton { width: 26; height: Theme.controlHeightSmall; text: "−"; onClicked: controller.setSelectedClipGainDb(controller.selectedClipGainDb - 1) }
                    Rectangle {
                        width: 66
                        height: Theme.controlHeightSmall
                        color: Theme.input
                        border.width: 1
                        border.color: gainInput.activeFocus ? Theme.focus : Theme.border
                        radius: Theme.radiusS
                        TextInput {
                            id: gainInput
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeM
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            text: Number(controller.selectedClipGainDb).toFixed(1)
                            selectByMouse: true
                            onEditingFinished: controller.setSelectedClipGainDb(Number(text))
                        }
                    }
                    Text { text: "dB"; color: Theme.faint; font.pixelSize: Theme.fontSizeS; anchors.verticalCenter: parent.verticalCenter }
                    PButton {
                        width: 86; height: Theme.controlHeightSmall
                        text: controller.selectedClipMuted ? "ACTIVAR" : "SILENCIAR"
                        onClicked: controller.toggleSelectedClipMute()
                    }
                    PButton { width: 26; height: Theme.controlHeightSmall; text: "+"; onClicked: controller.setSelectedClipGainDb(controller.selectedClipGainDb + 1) }
                }
                Text { width: parent.width; text: "−60 a +12 dB · cambios aplicados al clip seleccionado"; color: Theme.faint; font.pixelSize: Theme.fontSizeXS; wrapMode: Text.WordWrap }
            }
            Repeater {
                model: metaGroups
                delegate: Column {
                    width: body.width
                    spacing: 0
                    SectionLabel { width: parent.width; text: modelData.header; color: Theme.textBright }
                    Rectangle { width: parent.width; height: 1; color: Theme.borderSoft }
                    Repeater {
                        model: modelData.rows
                        delegate: Column {
                            width: body.width
                            spacing: 0
                            Row {
                                width: parent.width
                                spacing: 8
                                Text { width: 100; anchors.verticalCenter: parent.verticalCenter; text: modelData.key; color: Theme.faint; font.pixelSize: Theme.fontSizeS; elide: Text.ElideRight }
                                Text { width: parent.width - 108; anchors.verticalCenter: parent.verticalCenter; text: modelData.value; color: Theme.text; font.pixelSize: Theme.fontSizeM; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight }
                            }
                            Rectangle { width: parent.width; height: 5; color: "transparent" }
                        }
                    }
                }
            }
            Row {
                visible: controller.errorMessage.length > 0
                width: parent.width
                spacing: 8
                Rectangle { width: 2; height: errorText.height; color: Theme.danger }
                Text { id: errorText; width: parent.width - 10; text: controller.errorMessage; color: Theme.danger; font.pixelSize: Theme.fontSizeM; wrapMode: Text.WordWrap }
            }
            Column {
                width: parent.width
                spacing: 6
                Rectangle {
                    width: parent.width
                    height: technicalHeader.height + (inspector.technicalExpanded ? diagnosticText.height + 8 : 0)
                    color: Theme.panelAlt
                    border.width: 1
                    border.color: Theme.borderSoft
                    radius: Theme.radiusS
                    opacity: 0.82
                    Item {
                        id: technicalHeader
                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                        height: 28; anchors.leftMargin: 8; anchors.rightMargin: 8
                        activeFocusOnTab: true
                        Keys.onReturnPressed: inspector.technicalExpanded = !inspector.technicalExpanded
                        Keys.onEnterPressed: inspector.technicalExpanded = !inspector.technicalExpanded
                        Row {
                            anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; spacing: 8
                            Text { text: inspector.technicalExpanded ? "⌄" : ">"; color: Theme.muted; font.pixelSize: Theme.fontSizeM }
                            SectionLabel { text: "DETALLES TÉCNICOS"; opacity: 0.72 }
                        }
                        MouseArea { anchors.fill: parent; onClicked: inspector.technicalExpanded = !inspector.technicalExpanded }
                    }
                    Text { id: diagnosticText; visible: inspector.technicalExpanded; x: 8; y: technicalHeader.bottom + 2; width: parent.width - 16; text: controller.debugMetrics; color: Theme.faint; font.pixelSize: Theme.fontSizeXS; wrapMode: Text.WordWrap }
                }
            }
        }
    }
}
