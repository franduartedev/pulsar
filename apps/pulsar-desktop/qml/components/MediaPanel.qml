import QtQuick
import "../theme/Theme.js" as Theme

// Media library: real tabs (Medios / Sonidos), one row per asset with type
// icon, name and short secondary info. Technical detail lives in the Inspector.
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

    // "Video · 1280 × 720" / "Audio · 44100 Hz": short, no codec jargon.
    property string assetInfo: {
        var res = metaValue("Resolución")
        if (res.length > 0) {
            return "Video · " + res
        }
        var rate = metaValue("Audio sample rate")
        return rate.length > 0 ? "Audio · " + rate : "Medio"
    }
    readonly property bool assetIsVideo: assetInfo.indexOf("Video") === 0
    property bool soundsTab: false

    Text { id: title; anchors.left: parent.left; anchors.top: parent.top; anchors.leftMargin: Theme.spaceL; anchors.topMargin: Theme.spaceL; text: "Biblioteca"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeL; font.bold: true }
    PButton {
        id: importButton
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: title.bottom
        anchors.leftMargin: Theme.spaceM; anchors.rightMargin: Theme.spaceM; anchors.topMargin: Theme.spaceM
        primary: true; text: "+  Importar medio"
        onClicked: mediaPanel.importRequested()
    }

    // Underline tabs.
    Item {
        id: tabs
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: importButton.bottom
        anchors.leftMargin: Theme.spaceM; anchors.rightMargin: Theme.spaceM; anchors.topMargin: Theme.spaceM
        height: 32
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.borderSubtle }
        Repeater {
            model: [{label: "Medios", sounds: false}, {label: "Sonidos", sounds: true}]
            delegate: Item {
                width: tabs.width / 2; height: tabs.height
                x: index * width
                readonly property bool current: mediaPanel.soundsTab === modelData.sounds
                activeFocusOnTab: true
                Keys.onReturnPressed: mediaPanel.soundsTab = modelData.sounds
                Keys.onSpacePressed: mediaPanel.soundsTab = modelData.sounds
                Text { anchors.centerIn: parent; text: modelData.label; color: parent.current ? Theme.textPrimary : (tabMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary); font.pixelSize: Theme.fontSizeM; font.bold: parent.current }
                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 2; color: parent.current ? Theme.accent : (parent.activeFocus ? Theme.focus : "transparent") }
                MouseArea { id: tabMouse; anchors.fill: parent; hoverEnabled: true; onClicked: mediaPanel.soundsTab = modelData.sounds }
            }
        }
    }

    // ---- Medios -------------------------------------------------------
    ListView {
        id: mediaList
        visible: !mediaPanel.soundsTab
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: tabs.bottom
        anchors.bottom: pathRow.visible ? pathLabel.top : parent.bottom
        anchors.leftMargin: Theme.spaceM; anchors.rightMargin: Theme.spaceM
        anchors.topMargin: Theme.spaceS; anchors.bottomMargin: Theme.spaceS
        clip: true; spacing: 4
        // One entry today (the active source). The row is the unit the media
        // bin will list per asset once the controller exposes more than one.
        model: controller.hasMedia ? 1 : 0
        delegate: Rectangle {
            width: mediaList.width; height: 52; radius: Theme.radiusM
            color: Theme.bgSelected
            Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.topMargin: 10; anchors.bottomMargin: 10; width: 2; radius: 1; color: Theme.accent }
            Rectangle {
                id: iconBox
                anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter
                width: 32; height: 32; radius: Theme.radiusM; color: Theme.bgElevated
                TGlyph { anchors.centerIn: parent; width: 18; height: 18; kind: mediaPanel.assetIsVideo ? "video" : "audio"; ink: mediaPanel.assetIsVideo ? Theme.waveform : Theme.waveformAudio }
            }
            Column {
                anchors.left: iconBox.right; anchors.leftMargin: 10; anchors.right: parent.right; anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Text { width: parent.width; text: controller.mediaName; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeM; elide: Text.ElideMiddle }
                Text { width: parent.width; text: mediaPanel.assetInfo; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeXS; elide: Text.ElideRight }
            }
        }
    }
    Column {
        visible: !mediaPanel.soundsTab && !controller.hasMedia
        anchors.horizontalCenter: parent.horizontalCenter; anchors.top: tabs.bottom; anchors.topMargin: 40
        width: parent.width - 2 * Theme.spaceXL
        spacing: 6
        Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: "Sin medios"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeM }
        Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; text: "Importá o arrastrá un archivo a la ventana"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
    }

    // ---- Sonidos ------------------------------------------------------
    ListView {
        id: soundsList
        visible: mediaPanel.soundsTab
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: tabs.bottom; anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.spaceM; anchors.rightMargin: Theme.spaceM
        anchors.topMargin: Theme.spaceS; anchors.bottomMargin: Theme.spaceM
        clip: true; spacing: 4; model: controller.soundLibrary
        delegate: Rectangle {
            width: soundsList.width; height: 48; radius: Theme.radiusM
            color: rowMouse.containsMouse ? Theme.bgHover : "transparent"
            Rectangle {
                id: soundIcon
                anchors.left: parent.left; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter
                width: 32; height: 32; radius: Theme.radiusM; color: Theme.bgElevated
                TGlyph { anchors.centerIn: parent; width: 18; height: 18; kind: "audio"; ink: Theme.waveformAudio }
            }
            Column {
                anchors.left: soundIcon.right; anchors.leftMargin: 10; anchors.right: insert.left; anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Text { width: parent.width; text: modelData.name; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeM; elide: Text.ElideRight }
                Text { width: parent.width; text: modelData.category; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeXS; elide: Text.ElideRight }
            }
            MouseArea { id: rowMouse; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
            PButton { id: insert; anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; height: Theme.controlHeightSmall; text: "Insertar"; onClicked: controller.insertBuiltinSound(modelData.id) }
        }
    }

    // ---- Open by path (secondary) -------------------------------------
    Text { id: pathLabel; visible: pathRow.visible; anchors.left: parent.left; anchors.leftMargin: Theme.spaceM; anchors.bottom: pathRow.top; anchors.bottomMargin: 6; text: "Abrir por ruta"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
    Row {
        id: pathRow
        visible: !mediaPanel.soundsTab && parent.height > 400
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.spaceM; anchors.rightMargin: Theme.spaceM; anchors.bottomMargin: Theme.spaceM
        spacing: 6
        Rectangle {
            width: parent.width - openButton.width - 6
            height: Theme.controlHeight
            color: Theme.bgInput; radius: Theme.radiusM
            border.width: 1; border.color: pathInput.activeFocus ? Theme.focus : Theme.borderSubtle
            TextInput { id: pathInput; anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; verticalAlignment: TextInput.AlignVCenter; color: Theme.textPrimary; selectedTextColor: Theme.textOnAccent; selectionColor: Theme.accent; clip: true; font.pixelSize: Theme.fontSizeS; onAccepted: controller.openPath(text) }
            Text { anchors.left: parent.left; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter; text: "/ruta/al/video.mp4"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeS; visible: pathInput.text.length === 0 && !pathInput.activeFocus }
        }
        PButton { id: openButton; width: 56; text: "Abrir"; enabled: pathInput.text.length > 0; onClicked: controller.openPath(pathInput.text) }
    }
}
