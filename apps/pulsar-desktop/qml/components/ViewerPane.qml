import QtQuick
import "../theme/Theme.js" as Theme

// Monitor surface: viewer chrome (PROGRAMA context + preview state) on top,
// clean black mat with the image as hero, no overlays over the picture.
// Lives inside the monitor module frame composed by Main.qml.
Item {
    id: pane

    Text { id: originLabel; anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: chromeCenter; text: "PROGRAMA"; color: Theme.faint; font.pixelSize: Theme.fontSizeS; font.letterSpacing: 1.5 }
    Text {
        anchors.left: originLabel.right
        anchors.right: badgeRow.left
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.verticalCenter: chromeCenter
        text: controller.hasMedia ? controller.mediaName : "SIN SEÑAL"
        color: Theme.muted
        font.pixelSize: Theme.fontSizeS
        elide: Text.ElideMiddle
    }
    Row {
        id: badgeRow
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: chromeCenter
        spacing: 6
        StatusDot { anchors.verticalCenter: parent.verticalCenter; tone: controller.previewLate ? "error" : (controller.hasMedia ? "live" : "idle") }
        Text { anchors.verticalCenter: parent.verticalCenter; text: controller.previewLate ? "PREVIEW LATE" : "CPU"; color: controller.previewLate ? Theme.danger : Theme.muted; font.pixelSize: Theme.fontSizeS; font.letterSpacing: 1 }
    }
    Item { id: chromeCenter; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 30 }

    Rectangle {
        id: mat
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: chromeCenter.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.bottomMargin: 10
        color: Theme.viewer
        border.color: "#000000"
        border.width: 1
        clip: true

        Image { anchors.fill: parent; anchors.margins: 1; source: controller.previewSource; cache: false; fillMode: Image.PreserveAspectFit; visible: controller.previewSource.length > 0 }
        Column {
            anchors.centerIn: parent
            spacing: 10
            visible: controller.previewSource.length === 0 && controller.viewerState === "empty"
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "PULSAR"; color: Theme.textBright; font.pixelSize: 34; font.bold: true; font.letterSpacing: 5 }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: controller.viewerState === "gap" ? "GAP" : (controller.viewerState === "missing" ? "MEDIO NO DISPONIBLE" : (controller.viewerState === "ended" ? "FIN DEL PROGRAMA" : "PROGRAM VIEWER READY")); color: Theme.muted; font.pixelSize: Theme.fontSizeS; font.letterSpacing: 2.5 }
            Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 56; height: 2; color: Theme.accent; opacity: 0.55 }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: controller.viewerState === "loading" ? "Preparando presentación…" : (controller.viewerState === "error" ? "No se pudo presentar este segmento" : (controller.busy ? "Preparando medio…" : "Importá o arrastrá un video")); color: Theme.muted; font.pixelSize: Theme.fontSizeL }
            PButton { anchors.horizontalCenter: parent.horizontalCenter; width: 170; text: "IMPORTAR"; primary: true; onClicked: controller.importRequested() }
            PMuted { anchors.horizontalCenter: parent.horizontalCenter; text: "Space · reproducir / pausar"; color: Theme.faint }
        }
        Text {
            anchors.centerIn: parent
            visible: controller.previewSource.length === 0 && controller.viewerState !== "empty"
            text: controller.viewerState === "loading" ? "PREPARANDO PRESENTACIÓN…" : (controller.viewerState === "gap" ? "GAP" : (controller.viewerState === "missing" ? "MEDIO NO DISPONIBLE" : (controller.viewerState === "error" ? "NO SE PUDO PRESENTAR ESTE SEGMENTO" : "FIN DEL PROGRAMA")))
            color: Theme.muted
            font.pixelSize: Theme.fontSizeS
            font.letterSpacing: 1.5
        }
    }
}
