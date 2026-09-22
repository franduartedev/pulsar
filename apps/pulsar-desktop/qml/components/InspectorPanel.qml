import QtQuick
import "../theme/Theme.js" as Theme

// Real inspector: typographic sections with hairline rhythm, subdued keys,
// clear values, intelligent wrapping. No cards-in-cards.
Panel {
    border.width: 0
    radius: 0
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
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 10
        spacing: 8
        SectionLabel { anchors.verticalCenter: parent.verticalCenter; text: "INSPECTOR" }
        StatusDot { anchors.verticalCenter: parent.verticalCenter; tone: controller.errorMessage.length > 0 ? "error" : (controller.hasMedia ? "live" : "idle") }
    }
    Flickable {
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: titleRow.bottom; anchors.bottom: parent.bottom
        anchors.leftMargin: 12; anchors.rightMargin: 12; anchors.topMargin: 4; anchors.bottomMargin: 10
        contentWidth: width
        contentHeight: body.height
        clip: true
        Column {
            id: body
            width: parent.width
            spacing: 12
            Text { visible: metaGroups.length === 0; width: parent.width; text: "Abrí un medio para ver metadata real."; color: Theme.muted; font.pixelSize: Theme.fontSizeM; wrapMode: Text.WordWrap }
            Column {
                visible: controller.selectedClipKey.length > 0
                width: parent.width
                spacing: 6
                SectionLabel { width: parent.width; text: "AUDIO DEL CLIP" }
                Rectangle { width: parent.width; height: 1; color: Theme.borderSoft }
                Row {
                    width: parent.width
                    spacing: 6
                    Text { text: "Ganancia"; color: Theme.faint; font.pixelSize: Theme.fontSizeS; anchors.verticalCenter: parent.verticalCenter }
                    Rectangle {
                        width: 72
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
                        width: 92; height: Theme.controlHeightSmall
                        text: controller.selectedClipMuted ? "Activar" : "Silenciar"
                        onClicked: controller.toggleSelectedClipMute()
                    }
                }
                Text { width: parent.width; text: "Rango: -60 a +12 dB"; color: Theme.faint; font.pixelSize: Theme.fontSizeXS; wrapMode: Text.WordWrap }
            }
            Repeater {
                model: metaGroups
                delegate: Column {
                    width: body.width
                    spacing: 0
                    SectionLabel { width: parent.width; text: modelData.header }
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
                    height: diagnosticLabel.height + diagnosticText.height + 16
                    color: Theme.panelAlt
                    border.width: 1
                    border.color: Theme.borderSoft
                    radius: Theme.radiusS
                    opacity: 0.82
                    SectionLabel { id: diagnosticLabel; x: 8; y: 6; text: "DIAGNÓSTICO"; opacity: 0.65 }
                    Text { id: diagnosticText; x: 8; y: diagnosticLabel.bottom + 4; width: parent.width - 16; text: controller.debugMetrics; color: Theme.faint; font.pixelSize: Theme.fontSizeXS; wrapMode: Text.WordWrap }
                }
            }
        }
    }
}
