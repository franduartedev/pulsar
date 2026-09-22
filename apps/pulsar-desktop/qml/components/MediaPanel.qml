import QtQuick
import "../theme/Theme.js" as Theme

// Quiet support panel: the loaded session object leads, manual path entry
// stays secondary. Reads controller directly, like the rest of the UI.
Panel {
    signal importRequested()
    id: mediaPanel

    // Presentation-only lookup into the existing metadata blob.
    function metaValue(key) {
        var text = controller.metadata
        if (!text) {
            return ""
        }
        var lines = text.split("\n")
        for (var i = 0; i < lines.length; ++i) {
            var idx = lines[i].indexOf(":")
            if (idx < 0) {
                continue
            }
            if (lines[i].slice(0, idx).trim() === key) {
                return lines[i].slice(idx + 1).trim()
            }
        }
        return ""
    }
    function focusPath() {
        pathInput.forceActiveFocus()
        pathInput.selectAll()
    }

    property string codecInfo: metaValue("Codec") || metaValue("Audio codec")
    property string resInfo: metaValue("Resolución")
    property string subInfo: {
        if (!controller.hasMedia) {
            return "Importá o arrastrá un medio a la ventana."
        }
        if (codecInfo.length > 0 && resInfo.length > 0) {
            return codecInfo + " · " + resInfo
        }
        return codecInfo + resInfo
    }

    Rectangle { anchors.left: parent.left; anchors.top: parent.top; width: 4; height: 18; color: Theme.accent; opacity: 0.9 }
    SectionLabel { id: title; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.leftMargin: 14; anchors.rightMargin: 12; anchors.topMargin: 14; text: "BIBLIOTECA"; color: Theme.textBright }
    Text { anchors.left: title.left; anchors.right: title.right; anchors.top: title.bottom; anchors.topMargin: 3; text: "Medios, fuentes y sonidos"; color: Theme.faint; font.pixelSize: Theme.fontSizeXS }
    PButton { id: importButton; anchors.left: parent.left; anchors.right: parent.right; anchors.top: title.bottom; anchors.leftMargin: 12; anchors.rightMargin: 12; anchors.topMargin: 27; text: "+  IMPORTAR MEDIO"; primary: true; onClicked: mediaPanel.importRequested() }

    SectionLabel { id: sessionLabel; anchors.left: parent.left; anchors.right: parent.right; anchors.top: importButton.bottom; anchors.leftMargin: 12; anchors.rightMargin: 12; anchors.topMargin: 16; text: "FUENTE ACTIVA" }
    Rectangle {
        id: activeCard
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: sessionLabel.bottom; anchors.leftMargin: 12; anchors.rightMargin: 12; anchors.topMargin: 8
        height: cardCol.height + 18
        color: controller.hasMedia ? Theme.input : "transparent"
        border.color: controller.hasMedia ? Theme.border : Theme.borderSoft
        radius: Theme.radiusS
        Column {
            id: cardCol
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.leftMargin: 9; anchors.rightMargin: 9; anchors.topMargin: 9
            spacing: 4
            Row {
                width: parent.width
                spacing: 7
                StatusDot { anchors.verticalCenter: parent.verticalCenter; tone: controller.hasMedia ? "live" : "idle" }
                PText { width: parent.width - 14; anchors.verticalCenter: parent.verticalCenter; text: controller.hasMedia ? controller.mediaName : "Sin medios cargados"; color: controller.hasMedia ? Theme.textBright : Theme.muted }
            }
            PMuted { width: parent.width; text: mediaPanel.subInfo; wrapMode: Text.WordWrap }
        }
    }

    SectionLabel { id: soundsTitle; anchors.left: parent.left; anchors.right: parent.right; anchors.top: activeCard.bottom; anchors.leftMargin: 12; anchors.rightMargin: 12; anchors.topMargin: 16; text: "CORE SOUNDS"; color: Theme.textBright }
    ListView {
        id: soundsList
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: soundsTitle.bottom
        anchors.bottom: manualTitle.visible ? manualTitle.top : parent.bottom
        anchors.leftMargin: 12; anchors.rightMargin: 12; anchors.topMargin: 8; anchors.bottomMargin: manualTitle.visible ? 4 : 10
        clip: true; spacing: 6; model: controller.soundLibrary
        delegate: Rectangle {
            width: soundsList.width; height: 46; radius: Theme.radiusS; color: Theme.panelAlt; border.color: Theme.borderSoft
            Row {
                anchors.fill: parent; anchors.margins: 7; spacing: 8
                Rectangle { width: 3; height: parent.height; radius: 2; color: Theme.waveform; opacity: 0.7 }
            Column {
                width: parent.width - insert.width - 15; anchors.verticalCenter: parent.verticalCenter; spacing: 2
                Text { text: modelData.name; color: Theme.text; font.pixelSize: Theme.fontSizeM; font.bold: true; elide: Text.ElideRight; width: parent.width }
                Text { text: modelData.category; color: Theme.faint; font.pixelSize: Theme.fontSizeXS; font.letterSpacing: 0.8; elide: Text.ElideRight; width: parent.width }
            }
            PButton { id: insert; width: 62; height: 28; text: "INSERTAR"; anchors.verticalCenter: parent.verticalCenter; onClicked: controller.insertBuiltinSound(modelData.id) }
            }
        }
    }

    Rectangle { id: manualSep; visible: parent.height > 400; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: pathRow.top; anchors.leftMargin: 10; anchors.rightMargin: 10; anchors.bottomMargin: 8; height: 1; color: Theme.borderSoft }
    SectionLabel { id: manualTitle; visible: parent.height > 400; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: manualSep.top; anchors.leftMargin: 12; anchors.rightMargin: 12; anchors.bottomMargin: 8; text: "AVANZADO · RUTA DEL MEDIO"; opacity: 0.72 }
    Row {
        id: pathRow
        visible: parent.height > 400
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.leftMargin: 12; anchors.rightMargin: 12; anchors.bottomMargin: 12; opacity: 0.82
        spacing: 6
        Rectangle {
            id: pathBox
            width: parent.width - openButton.width - 6
            height: 28
            color: Theme.input; border.color: pathInput.activeFocus ? Theme.accent : Theme.border; radius: Theme.radiusS
            TextInput { id: pathInput; anchors.fill: parent; anchors.leftMargin: 7; anchors.rightMargin: 7; anchors.topMargin: 6; anchors.bottomMargin: 6; color: Theme.text; selectedTextColor: Theme.textBright; selectionColor: Theme.accent; clip: true; font.pixelSize: Theme.fontSizeM; onAccepted: controller.openPath(text) }
            Text { anchors.left: parent.left; anchors.leftMargin: 7; anchors.verticalCenter: parent.verticalCenter; text: "/ruta/video.mp4"; color: Theme.faint; font.pixelSize: Theme.fontSizeM; visible: pathInput.text.length === 0 && !pathInput.activeFocus }
        }
        PButton { id: openButton; width: 56; text: "Abrir"; enabled: pathInput.text.length > 0; onClicked: controller.openPath(pathInput.text) }
    }
}
