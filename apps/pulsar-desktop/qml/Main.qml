import QtQuick
import QtQuick.Window

Window {
    id: root
    width: 1280
    height: 760
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: "PULSAR"
    color: palette.base

    QtObject {
        id: palette
        property color base: "#0d1117"
        property color panel: "#111821"
        property color panel2: "#0f151d"
        property color border: "#263241"
        property color text: "#d7dee8"
        property color muted: "#7f8b99"
        property color accent: "#4c8dff"
        property color danger: "#ff6b6b"
        property color viewer: "#05070a"
    }

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

    component Panel: Rectangle {
        color: "#111821"
        border.color: "#263241"
        border.width: 1
    }

    component PButton: Rectangle {
        id: button
        property alias text: label.text
        property bool enabled: true
        signal clicked()
        implicitHeight: 30
        radius: 3
        color: enabled ? (mouse.pressed ? "#1d2a38" : mouse.containsMouse ? "#182232" : "#131c27") : "#10151c"
        border.color: enabled ? "#263241" : "#1b232c"
        Text { id: label; anchors.centerIn: parent; color: button.enabled ? "#d7dee8" : "#7f8b99"; font.pixelSize: 12 }
        MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; enabled: button.enabled; onClicked: button.clicked() }
    }

    component PText: Text {
        color: "#d7dee8"
        font.pixelSize: 12
        elide: Text.ElideRight
    }

    component PMuted: Text {
        color: "#7f8b99"
        font.pixelSize: 11
        elide: Text.ElideRight
    }

    component PSlider: Item {
        id: slider
        property real value: 0
        property bool enabled: true
        signal moved(real value)
        height: 24
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; height: 4; radius: 2; color: "#202b38" }
        Rectangle { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; width: Math.max(0, Math.min(parent.width, parent.width * slider.value)); height: 4; radius: 2; color: slider.enabled ? "#4c8dff" : "#7f8b99" }
        Rectangle { x: Math.max(0, Math.min(parent.width - width, parent.width * slider.value - width / 2)); anchors.verticalCenter: parent.verticalCenter; width: 12; height: 12; radius: 6; color: slider.enabled ? "#d7dee8" : "#7f8b99"; border.color: "#263241" }
        MouseArea {
            anchors.fill: parent
            enabled: slider.enabled
            onPressed: update(mouse.x)
            onPositionChanged: if (pressed) update(mouse.x)
            function update(px) {
                var next = Math.max(0, Math.min(1, px / Math.max(1, slider.width)))
                slider.value = next
                slider.moved(next)
            }
        }
    }

    Rectangle {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 42
        color: "#0b0f15"
        border.color: palette.border
        Text { anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: "PULSAR"; color: palette.text; font.pixelSize: 16; font.bold: true; font.letterSpacing: 2 }
        Text { anchors.left: parent.left; anchors.leftMargin: 105; anchors.verticalCenter: parent.verticalCenter; text: "Media Viewer"; color: palette.muted; font.pixelSize: 12 }
        Text { anchors.right: parent.right; anchors.rightMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: controller.status; color: controller.errorMessage.length > 0 ? palette.danger : palette.muted; font.pixelSize: 12 }
    }

    Panel {
        id: mediaPanel
        anchors.left: parent.left
        anchors.top: header.bottom
        anchors.bottom: scrubberPanel.top
        anchors.margins: 10
        width: 230
        Text { id: mediaTitle; anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 10; text: "MEDIOS"; color: palette.muted; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.5 }
        PButton { id: importButton; anchors.left: parent.left; anchors.right: parent.right; anchors.top: mediaTitle.bottom; anchors.margins: 10; text: "Importar"; onClicked: controller.importRequested() }
        Rectangle {
            id: pathBox
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: importButton.bottom; anchors.margins: 10; height: 32
            color: "#0c121a"; border.color: importPath.activeFocus ? palette.accent : palette.border
            TextInput { id: importPath; anchors.fill: parent; anchors.margins: 7; color: palette.text; selectedTextColor: "white"; selectionColor: palette.accent; clip: true; font.pixelSize: 12; onAccepted: controller.openPath(text) }
            Text { anchors.left: parent.left; anchors.leftMargin: 7; anchors.verticalCenter: parent.verticalCenter; text: "/ruta/video.mp4"; color: palette.muted; font.pixelSize: 12; visible: importPath.text.length === 0 && !importPath.activeFocus }
        }
        PButton { id: openButton; anchors.left: parent.left; anchors.right: parent.right; anchors.top: pathBox.bottom; anchors.margins: 10; text: "Abrir ruta"; enabled: importPath.text.length > 0; onClicked: controller.openPath(importPath.text) }
        Rectangle { id: mediaSep; anchors.left: parent.left; anchors.right: parent.right; anchors.top: openButton.bottom; anchors.margins: 10; height: 1; color: palette.border }
        PText { id: mediaName; anchors.left: parent.left; anchors.right: parent.right; anchors.top: mediaSep.bottom; anchors.margins: 10; text: controller.hasMedia ? controller.mediaName : "Sin medios abiertos" }
        PMuted { anchors.left: parent.left; anchors.right: parent.right; anchors.top: mediaName.bottom; anchors.margins: 10; text: "Arrastrá un video aquí o pasá la ruta por CLI."; wrapMode: Text.WordWrap }
    }

    Panel {
        id: inspector
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: scrubberPanel.top
        anchors.margins: 10
        width: 260
        Text { id: inspectorTitle; anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 10; text: "INSPECTOR"; color: palette.muted; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.5 }
        Text { id: metadata; anchors.left: parent.left; anchors.right: parent.right; anchors.top: inspectorTitle.bottom; anchors.margins: 10; text: controller.metadata.length > 0 ? controller.metadata : "Abrí un medio para ver metadata real."; color: controller.metadata.length > 0 ? palette.text : palette.muted; font.pixelSize: 12; wrapMode: Text.WordWrap }
        Rectangle { id: inspectorSep; anchors.left: parent.left; anchors.right: parent.right; anchors.top: metadata.bottom; anchors.margins: 10; height: 1; color: palette.border }
        Text { id: errorText; anchors.left: parent.left; anchors.right: parent.right; anchors.top: inspectorSep.bottom; anchors.margins: 10; text: controller.errorMessage; color: palette.danger; font.pixelSize: 12; wrapMode: Text.WordWrap; visible: controller.errorMessage.length > 0 }
        Text { anchors.left: parent.left; anchors.right: parent.right; anchors.top: errorText.visible ? errorText.bottom : inspectorSep.bottom; anchors.margins: 10; text: controller.debugMetrics; color: palette.muted; font.pixelSize: 10; wrapMode: Text.WordWrap }
    }

    Column {
        id: center
        anchors.left: mediaPanel.right
        anchors.right: inspector.left
        anchors.top: header.bottom
        anchors.bottom: scrubberPanel.top
        anchors.margins: 10
        spacing: 8
        Panel {
            id: viewer
            width: parent.width
            height: parent.height - transport.height - 8
            color: palette.viewer
            clip: true
            Image { anchors.fill: parent; anchors.margins: 1; source: controller.previewSource; cache: false; fillMode: Image.PreserveAspectFit; visible: controller.previewSource.length > 0 }
            Column {
                anchors.centerIn: parent
                spacing: 12
                visible: controller.previewSource.length === 0
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "PULSAR"; color: palette.text; font.pixelSize: 30; font.bold: true; font.letterSpacing: 3 }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: controller.busy ? "Preparando medio…" : "Importá material para empezar"; color: palette.muted; font.pixelSize: 14 }
                PButton { width: 150; text: "Importar video"; onClicked: controller.importRequested() }
            }
            Rectangle { anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 10; width: badgeText.width + 14; height: 24; radius: 3; color: "#aa101820"; border.color: controller.previewLate ? palette.danger : palette.border; visible: controller.hasMedia; Text { id: badgeText; anchors.centerIn: parent; text: controller.previewLate ? "Preview atrasada" : "Preview CPU"; color: controller.previewLate ? palette.danger : palette.muted; font.pixelSize: 11 } }
        }
        Panel {
            id: transport
            width: parent.width
            height: 58
            PButton { id: homeButton; anchors.left: parent.left; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter; width: 36; text: "◀"; enabled: controller.hasMedia; onClicked: controller.seekToStart() }
            PButton { id: playButton; anchors.left: homeButton.right; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter; width: 46; text: controller.playing ? "❚❚" : "▶"; enabled: controller.hasMedia && !controller.busy; onClicked: controller.togglePlayback() }
            Text { id: tc; anchors.left: playButton.right; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: controller.positionText + " / " + controller.durationText; color: palette.text; font.family: "monospace"; font.pixelSize: 13 }
            Text { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: controller.technicalStatus; color: palette.muted; font.pixelSize: 11 }
        }
    }

    Panel {
        id: scrubberPanel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: statusBar.top
        anchors.margins: 10
        height: 72
        Text { anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 10; text: "SOURCE SCRUBBER"; color: palette.muted; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.3 }
        Text { anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 10; text: controller.positionText; color: palette.text; font.family: "monospace"; font.pixelSize: 12 }
        PSlider { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 10; value: controller.progress; enabled: controller.hasMedia && !controller.busy; onMoved: function(v) { controller.seekNormalized(v) } }
    }

    Rectangle {
        id: statusBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.bottomMargin: 10
        height: 24
        color: "#0b0f15"
        border.color: palette.border
        Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: controller.status; color: palette.muted; font.pixelSize: 11 }
        Text { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: controller.errorMessage.length > 0 ? controller.errorMessage : controller.technicalStatus; color: controller.errorMessage.length > 0 ? palette.danger : palette.muted; font.pixelSize: 11 }
    }
}
