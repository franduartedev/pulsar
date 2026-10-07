import QtQuick
import "../theme/Theme.js" as Theme

// Monitor surface: the picture is the hero. A single quiet line above it
// carries the source name and preview health; nothing overlays the image.
Item {
    id: pane
    signal importRequested()

    readonly property string stateLabel: controller.viewerState === "loading" ? "Preparando…"
        : (controller.viewerState === "gap" ? "Espacio vacío"
        : (controller.viewerState === "missing" ? "Medio no disponible"
        : (controller.viewerState === "error" ? "No se pudo presentar este segmento"
        : "Fin del programa")))

    Item {
        id: chrome
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        height: 36
        Text {
            anchors.left: parent.left; anchors.leftMargin: Theme.spaceL
            anchors.right: lateRow.left; anchors.rightMargin: Theme.spaceM
            anchors.verticalCenter: parent.verticalCenter
            text: controller.hasMedia ? "Programa" : ""
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeS
            elide: Text.ElideMiddle
        }
        Row {
            id: lateRow
            visible: controller.previewLate
            anchors.right: parent.right; anchors.rightMargin: Theme.spaceL
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            StatusDot { anchors.verticalCenter: parent.verticalCenter; tone: "warn" }
            Text { anchors.verticalCenter: parent.verticalCenter; text: "Preview con retraso"; color: Theme.warning; font.pixelSize: Theme.fontSizeXS }
        }
    }

    Rectangle {
        id: mat
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: chrome.bottom; anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.spaceM; anchors.rightMargin: Theme.spaceM
        anchors.bottomMargin: Theme.spaceS
        color: Theme.bgViewer
        radius: Theme.radiusS
        clip: true

        Image { anchors.fill: parent; source: controller.previewSource; cache: false; fillMode: Image.PreserveAspectFit; visible: controller.previewSource.length > 0 }

        Column {
            anchors.centerIn: parent
            spacing: mat.height < 180 ? 6 : 12
            visible: controller.previewSource.length === 0 && controller.viewerState === "empty"
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "PULSAR"; color: Theme.textMuted; font.pixelSize: mat.height < 180 ? 18 : Theme.fontSizeXXL; font.bold: true; font.letterSpacing: 6 }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: controller.busy ? "Preparando medio…" : "Importá un medio para comenzar"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeM }
            PButton { anchors.horizontalCenter: parent.horizontalCenter; visible: !controller.busy; primary: true; text: "Importar medio"; onClicked: pane.importRequested() }
        }
        Text {
            anchors.centerIn: parent
            visible: controller.previewSource.length === 0 && controller.viewerState !== "empty"
            text: pane.stateLabel
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeM
        }
    }
}
