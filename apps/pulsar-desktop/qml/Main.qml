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
        height: 40
        color: Theme.header
        border.color: Theme.border
        Text { anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: "PULSAR"; color: Theme.text; font.pixelSize: Theme.fontSizeXL; font.letterSpacing: 2 }
        Text { anchors.left: parent.left; anchors.leftMargin: 96; anchors.verticalCenter: parent.verticalCenter; text: "Media Viewer"; color: Theme.muted; font.pixelSize: Theme.fontSizeM }
        Text { anchors.right: parent.right; anchors.rightMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: controller.status; color: controller.errorMessage.length > 0 ? Theme.danger : Theme.muted; font.pixelSize: Theme.fontSizeM }
    }

    Panel {
        id: mediaPanel
        anchors.left: parent.left
        anchors.top: header.bottom
        anchors.bottom: scrubberPanel.top
        anchors.margins: 10
        width: 220
        Text { id: mediaTitle; anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 10; text: "MEDIOS"; color: Theme.muted; font.pixelSize: Theme.fontSizeS; font.letterSpacing: 1.5 }
        PButton { id: importButton; anchors.left: parent.left; anchors.right: parent.right; anchors.top: mediaTitle.bottom; anchors.margins: 10; text: "Importar"; onClicked: controller.importRequested() }
        Rectangle {
            id: pathBox
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: importButton.bottom; anchors.margins: 10; height: 30
            color: Theme.input; border.color: importPath.activeFocus ? Theme.accent : Theme.border
            TextInput { id: importPath; anchors.fill: parent; anchors.margins: 7; color: Theme.text; selectedTextColor: "white"; selectionColor: Theme.accent; clip: true; font.pixelSize: Theme.fontSizeM; onAccepted: controller.openPath(text) }
            Text { anchors.left: parent.left; anchors.leftMargin: 7; anchors.verticalCenter: parent.verticalCenter; text: "/ruta/video.mp4"; color: Theme.muted; font.pixelSize: Theme.fontSizeM; visible: importPath.text.length === 0 && !importPath.activeFocus }
        }
        PButton { id: openButton; anchors.left: parent.left; anchors.right: parent.right; anchors.top: pathBox.bottom; anchors.margins: 10; text: "Abrir ruta"; enabled: importPath.text.length > 0; onClicked: controller.openPath(importPath.text) }
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: openButton.bottom; anchors.margins: 10; height: 1; color: Theme.border }
        PText { id: mediaName; anchors.left: parent.left; anchors.right: parent.right; anchors.top: openButton.bottom; anchors.topMargin: 16; text: controller.hasMedia ? controller.mediaName : "Sin medios abiertos" }
        PMuted { anchors.left: parent.left; anchors.right: parent.right; anchors.top: mediaName.bottom; anchors.margins: 10; text: "Arrastrá un video aquí o pasá la ruta por CLI."; wrapMode: Text.WordWrap }
    }

    Panel {
        id: inspector
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: scrubberPanel.top
        anchors.margins: 10
        width: 260
        Text { id: inspectorTitle; anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 10; text: "INSPECTOR"; color: Theme.muted; font.pixelSize: Theme.fontSizeS; font.letterSpacing: 1.5 }
        Text { id: metadata; anchors.left: parent.left; anchors.right: parent.right; anchors.top: inspectorTitle.bottom; anchors.margins: 10; text: controller.metadata.length > 0 ? controller.metadata : "Abrí un medio para ver metadata real."; color: controller.metadata.length > 0 ? Theme.text : Theme.muted; font.pixelSize: Theme.fontSizeM; wrapMode: Text.WordWrap }
        Rectangle { id: inspectorSep; anchors.left: parent.left; anchors.right: parent.right; anchors.top: metadata.bottom; anchors.margins: 10; height: 1; color: Theme.border }
        Text { id: errorText; anchors.left: parent.left; anchors.right: parent.right; anchors.top: inspectorSep.bottom; anchors.margins: 10; text: controller.errorMessage; color: Theme.danger; font.pixelSize: Theme.fontSizeM; wrapMode: Text.WordWrap; visible: controller.errorMessage.length > 0 }
        Text { anchors.left: parent.left; anchors.right: parent.right; anchors.top: errorText.visible ? errorText.bottom : inspectorSep.bottom; anchors.margins: 10; text: controller.debugMetrics; color: Theme.muted; font.pixelSize: Theme.fontSizeXS; wrapMode: Text.WordWrap }
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
            clip: true
            Image { anchors.fill: parent; anchors.margins: 1; source: controller.previewSource; cache: false; fillMode: Image.PreserveAspectFit; visible: controller.previewSource.length > 0 }
            Column {
                anchors.centerIn: parent
                spacing: 12
                visible: controller.previewSource.length === 0
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "PULSAR"; color: Theme.text; font.pixelSize: Theme.fontSizeXXL; font.letterSpacing: 3 }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: controller.busy ? "Preparando medio…" : "Importá material para empezar"; color: Theme.muted; font.pixelSize: Theme.fontSizeL }
                PButton { anchors.horizontalCenter: parent.horizontalCenter; width: 150; text: "Importar video"; onClicked: controller.importRequested() }
            }
            Rectangle { anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 10; width: badgeText.width + 14; height: 22; radius: 3; color: Theme.badge; border.color: controller.previewLate ? Theme.danger : Theme.border; visible: controller.hasMedia; Text { id: badgeText; anchors.centerIn: parent; text: controller.previewLate ? "Preview atrasada" : "Preview CPU"; color: controller.previewLate ? Theme.danger : Theme.muted; font.pixelSize: Theme.fontSizeS } }
        }
        Panel {
            id: transport
            width: parent.width
            height: 48
            PButton { id: homeButton; anchors.left: parent.left; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter; width: 34; text: "◀"; enabled: controller.hasMedia; onClicked: controller.seekToStart() }
            PButton { id: playButton; anchors.left: homeButton.right; anchors.leftMargin: 6; anchors.verticalCenter: parent.verticalCenter; width: 42; text: controller.playing ? "❚❚" : "▶"; enabled: controller.hasMedia && !controller.busy; onClicked: controller.togglePlayback() }
            PButton { id: endButton; anchors.left: playButton.right; anchors.leftMargin: 6; anchors.verticalCenter: parent.verticalCenter; width: 34; text: "▶|"; enabled: controller.hasMedia; onClicked: controller.seekToEnd() }
            Text { id: tc; anchors.left: endButton.right; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: controller.positionText + " / " + controller.durationText; color: Theme.text; font.family: "monospace"; font.pixelSize: Theme.fontSizeL }
        }
    }

    Panel {
        id: scrubberPanel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: statusBar.top
        anchors.margins: 10
        height: 58
        Text { anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 10; text: "SOURCE SCRUBBER"; color: Theme.muted; font.pixelSize: Theme.fontSizeS; font.letterSpacing: 1.3 }
        Text { anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 10; text: controller.positionText; color: Theme.text; font.family: "monospace"; font.pixelSize: Theme.fontSizeM }
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
        height: 22
        color: Theme.header
        border.color: Theme.border
        Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: "Espacio: Play/Pausa · Ctrl+I: Importar · Inicio/Fin: Buscar"; color: Theme.muted; font.pixelSize: Theme.fontSizeS }
        Text { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: controller.errorMessage.length > 0 ? controller.errorMessage : controller.technicalStatus; color: controller.errorMessage.length > 0 ? Theme.danger : Theme.muted; font.pixelSize: Theme.fontSizeS }
    }
}