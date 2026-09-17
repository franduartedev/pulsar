import QtQuick
import "../theme/Theme.js" as Theme

// Real inspector: typographic sections with hairline rhythm, subdued keys,
// clear values, intelligent wrapping. No cards-in-cards.
Panel {
    id: inspector

    function metaSection(key) {
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
                spacing: 4
                SectionLabel { width: parent.width; text: "DIAGNÓSTICO" }
                Text { width: parent.width; text: controller.debugMetrics; color: Theme.faint; font.pixelSize: Theme.fontSizeXS; wrapMode: Text.WordWrap }
            }
        }
    }
}
