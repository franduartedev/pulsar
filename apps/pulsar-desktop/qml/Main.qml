import QtQuick
import QtQuick.Window
import "theme/Theme.js" as Theme
import "components"

Window {
    id: root
    width: 1280
    height: 760
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: "PULSAR"
    color: Theme.base

    // Presentation-only grouping of the controller metadata blob.
    // Unknown keys fall back to OTROS so controller text changes degrade gracefully.
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

    Shortcut { sequence: "Space"; onActivated: controller.togglePlayback() }
    Shortcut { sequence: "Ctrl+I"; onActivated: controller.importRequested() }
    Shortcut { sequence: "Home"; onActivated: controller.seekToStart() }
    Shortcut { sequence: "End"; onActivated: controller.seekToEnd() }

    Connections {
        target: controller
        function onFocusImportPathRequested() {
            importPath.forceActiveFocus()
            importPath.selectAll()
        }
    }

    DropArea {
        anchors.fill: parent
        onDropped: function(drop) {
            if (drop.hasUrls && drop.urls.length > 0) {
                controller.openUrl(drop.urls[0])
                drop.acceptProposedAction()
            }
        }
    }

    Rectangle {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 44
        color: Theme.header
        border.color: Theme.borderSoft
        Row {
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10
            Text { anchors.verticalCenter: parent.verticalCenter; text: "PULSAR"; color: Theme.text; font.pixelSize: Theme.fontSizeXL; font.bold: true; font.letterSpacing: 2 }
            Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 1; height: 14; color: Theme.border }
            Text { anchors.verticalCenter: parent.verticalCenter; text: "MEDIA VIEWER"; color: Theme.muted; font.pixelSize: Theme.fontSizeS; font.letterSpacing: 1.5 }
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            StatusDot { anchors.verticalCenter: parent.verticalCenter; tone: controller.errorMessage.length > 0 ? "error" : (controller.busy || controller.playing ? "live" : "idle") }
            Text { anchors.verticalCenter: parent.verticalCenter; text: controller.status; color: controller.errorMessage.length > 0 ? Theme.danger : Theme.muted; font.pixelSize: Theme.fontSizeM }
        }
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.accent; opacity: 0.25 }
    }

    Panel {
        id: mediaPanel
        anchors.left: parent.left
        anchors.top: header.bottom
        anchors.bottom: scrubberPanel.top
        anchors.margins: 10
        width: 224
        SectionLabel { id: mediaTitle; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.leftMargin: 10; anchors.rightMargin: 10; anchors.topMargin: 10; text: "MEDIOS" }
        PButton { id: importButton; anchors.left: parent.left; anchors.right: parent.right; anchors.top: mediaTitle.bottom; anchors.leftMargin: 10; anchors.rightMargin: 10; anchors.topMargin: 8; text: "Importar"; primary: true; onClicked: controller.importRequested() }
        Rectangle {
            id: pathBox
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: importButton.bottom; anchors.leftMargin: 10; anchors.rightMargin: 10; anchors.topMargin: 8; height: 30
            color: Theme.input; border.color: importPath.activeFocus ? Theme.accent : Theme.border; radius: Theme.radiusS
            TextInput { id: importPath; anchors.fill: parent; anchors.margins: 7; color: Theme.text; selectedTextColor: Theme.textBright; selectionColor: Theme.accent; clip: true; font.pixelSize: Theme.fontSizeM; onAccepted: controller.openPath(text) }
            Text { anchors.left: parent.left; anchors.leftMargin: 7; anchors.verticalCenter: parent.verticalCenter; text: "/ruta/video.mp4"; color: Theme.faint; font.pixelSize: Theme.fontSizeM; visible: importPath.text.length === 0 && !importPath.activeFocus }
        }
        PButton { id: openButton; anchors.left: parent.left; anchors.right: parent.right; anchors.top: pathBox.bottom; anchors.leftMargin: 10; anchors.rightMargin: 10; anchors.topMargin: 8; text: "Abrir ruta"; enabled: importPath.text.length > 0; onClicked: controller.openPath(importPath.text) }
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: openButton.bottom; anchors.leftMargin: 10; anchors.rightMargin: 10; anchors.topMargin: 10; height: 1; color: Theme.borderSoft }
        SectionLabel { id: sessionLabel; anchors.left: parent.left; anchors.right: parent.right; anchors.top: openButton.bottom; anchors.leftMargin: 10; anchors.rightMargin: 10; anchors.topMargin: 16; text: "EN SESIÓN" }
        Rectangle {
            id: mediaCard
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: sessionLabel.bottom; anchors.leftMargin: 10; anchors.rightMargin: 10; anchors.topMargin: 8
            height: cardCol.height + 16
            color: Theme.input; border.color: Theme.borderSoft; radius: Theme.radiusS
            Column {
                id: cardCol
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.leftMargin: 8; anchors.rightMargin: 8; anchors.topMargin: 8
                spacing: 5
                Row {
                    width: parent.width
                    spacing: 7
                    StatusDot { anchors.verticalCenter: parent.verticalCenter; tone: controller.hasMedia ? "live" : "idle" }
                    PText { width: parent.width - 14; anchors.verticalCenter: parent.verticalCenter; text: controller.hasMedia ? controller.mediaName : "Sin medios abiertos"; color: controller.hasMedia ? Theme.text : Theme.muted }
                }
                PMuted { width: parent.width; text: "Arrastrá un video aquí o pasá la ruta por CLI."; wrapMode: Text.WordWrap }
            }
        }
    }

    Panel {
        id: inspector
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: scrubberPanel.top
        anchors.margins: 10
        width: 264
        SectionLabel { id: inspectorTitle; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.leftMargin: 10; anchors.rightMargin: 10; anchors.topMargin: 10; text: "INSPECTOR" }
        Flickable {
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: inspectorTitle.bottom; anchors.bottom: parent.bottom
            anchors.leftMargin: 10; anchors.rightMargin: 10; anchors.topMargin: 6; anchors.bottomMargin: 10
            contentWidth: width
            contentHeight: inspectorBody.height
            clip: true
            Column {
                id: inspectorBody
                width: parent.width
                spacing: 10
                Text { visible: metaGroups.length === 0; width: parent.width; text: "Abrí un medio para ver metadata real."; color: Theme.muted; font.pixelSize: Theme.fontSizeM; wrapMode: Text.WordWrap }
                Repeater {
                    model: metaGroups
                    delegate: Column {
                        width: inspectorBody.width
                        spacing: 5
                        SectionLabel { width: parent.width; text: modelData.header }
                        Repeater {
                            model: modelData.rows
                            delegate: Row {
                                width: inspectorBody.width
                                spacing: 8
                                Text { width: 96; text: modelData.key; color: Theme.muted; font.pixelSize: Theme.fontSizeS; elide: Text.ElideRight }
                                Text { width: parent.width - 104; text: modelData.value; color: Theme.text; font.pixelSize: Theme.fontSizeM; wrapMode: Text.WordWrap }
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

    Column {
        id: center
        anchors.left: mediaPanel.right
        anchors.right: inspector.left
        anchors.top: header.bottom
        anchors.bottom: scrubberPanel.top
        anchors.margins: 10
        spacing: 6
        Panel {
            id: viewer
            width: parent.width
            height: parent.height - transport.height - 6
            color: Theme.viewer
            border.color: Theme.border
            clip: true
            Image { anchors.fill: parent; anchors.margins: 1; source: controller.previewSource; cache: false; fillMode: Image.PreserveAspectFit; visible: controller.previewSource.length > 0 }
            Column {
                anchors.centerIn: parent
                spacing: 10
                visible: controller.previewSource.length === 0
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "PULSAR"; color: Theme.textBright; font.pixelSize: Theme.fontSizeXXL; font.bold: true; font.letterSpacing: 4 }
                Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 56; height: 2; color: Theme.accent; opacity: 0.55 }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: controller.busy ? "Preparando medio…" : "Importá material para empezar"; color: Theme.muted; font.pixelSize: Theme.fontSizeL }
                PButton { anchors.horizontalCenter: parent.horizontalCenter; width: 170; text: "Importar video"; primary: true; onClicked: controller.importRequested() }
                PMuted { anchors.horizontalCenter: parent.horizontalCenter; text: "o arrastrá un video a la ventana"; color: Theme.faint }
            }
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                width: badgeRow.width + 16
                height: 22
                radius: Theme.radiusS
                color: Theme.badge
                border.color: controller.previewLate ? Theme.danger : Theme.borderSoft
                visible: controller.hasMedia
                Row {
                    id: badgeRow
                    anchors.centerIn: parent
                    spacing: 6
                    StatusDot { anchors.verticalCenter: parent.verticalCenter; tone: controller.previewLate ? "error" : "idle" }
                    Text { anchors.verticalCenter: parent.verticalCenter; text: controller.previewLate ? "Preview atrasada" : "Preview CPU"; color: controller.previewLate ? Theme.danger : Theme.muted; font.pixelSize: Theme.fontSizeS }
                }
            }
        }
        Panel {
            id: transport
            width: parent.width
            height: 52
            color: Theme.panelAlt
            Row {
                id: transportButtons
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6
                PButton { id: homeButton; width: 34; text: "◀"; enabled: controller.hasMedia; onClicked: controller.seekToStart() }
                PButton { id: playButton; width: 46; text: controller.playing ? "❚❚" : "▶"; enabled: controller.hasMedia && !controller.busy; onClicked: controller.togglePlayback() }
                PButton { id: endButton; width: 34; text: "▶|"; enabled: controller.hasMedia; onClicked: controller.seekToEnd() }
            }
            Row {
                anchors.left: transportButtons.right
                anchors.leftMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8
                Text { anchors.verticalCenter: parent.verticalCenter; text: controller.positionText; color: Theme.textBright; font.family: "monospace"; font.pixelSize: Theme.fontSizeXL }
                Text { anchors.verticalCenter: parent.verticalCenter; text: "/"; color: Theme.faint; font.pixelSize: Theme.fontSizeM }
                Text { anchors.verticalCenter: parent.verticalCenter; text: controller.durationText; color: Theme.muted; font.family: "monospace"; font.pixelSize: Theme.fontSizeM }
            }
        }
    }

    Panel {
        id: scrubberPanel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: statusBar.top
        anchors.margins: 10
        height: 60
        color: Theme.panelAlt
        SectionLabel { anchors.left: parent.left; anchors.top: parent.top; anchors.leftMargin: 10; anchors.topMargin: 10; text: "SOURCE SCRUBBER" }
        Text { anchors.right: parent.right; anchors.top: parent.top; anchors.rightMargin: 10; anchors.topMargin: 10; text: controller.positionText; color: Theme.text; font.family: "monospace"; font.pixelSize: Theme.fontSizeM }
        PSlider { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.leftMargin: 10; anchors.rightMargin: 10; anchors.bottomMargin: 8; value: controller.progress; enabled: controller.hasMedia && !controller.busy; onMoved: function(v) { controller.seekNormalized(v) } }
    }

    Rectangle {
        id: statusBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.bottomMargin: 10
        height: 26
        color: Theme.header
        border.color: Theme.borderSoft
        radius: Theme.radiusS
        Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: "Espacio: Play/Pausa · Ctrl+I: Importar · Inicio/Fin: Buscar"; color: Theme.faint; font.pixelSize: Theme.fontSizeS }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            spacing: 7
            StatusDot { anchors.verticalCenter: parent.verticalCenter; tone: controller.errorMessage.length > 0 ? "error" : "idle" }
            Text { anchors.verticalCenter: parent.verticalCenter; text: controller.errorMessage.length > 0 ? controller.errorMessage : controller.technicalStatus; color: controller.errorMessage.length > 0 ? Theme.danger : Theme.muted; font.pixelSize: Theme.fontSizeS; elide: Text.ElideRight }
        }
    }
}
