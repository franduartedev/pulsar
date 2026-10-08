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

        Image { id: frame; anchors.fill: parent; source: controller.previewSource; cache: false; fillMode: Image.PreserveAspectFit; visible: controller.previewSource.length > 0 }

        // Title placement: the selected title's block, drawn where the compositor puts it. Dragging
        // moves a ghost outline (no per-move re-render); releasing commits ONE position edit, which
        // preview and export both honour. Centre guides snap at the middle of the frame.
        Item {
            id: placer
            readonly property var info: controller.selectedTitle
            readonly property bool ready: controller.previewSource.length > 0 && controller.selectedTitleOnScreen
                                          && info !== undefined && info.blockW !== undefined && frame.paintedWidth > 0
            readonly property real picX: frame.x + (frame.width - frame.paintedWidth) / 2
            readonly property real picY: frame.y + (frame.height - frame.paintedHeight) / 2
            visible: ready
            anchors.fill: parent
            property real dx: 0
            property real dy: 0
            property bool snapX: false
            property bool snapY: false
            Rectangle {   // vertical / horizontal centre guides, only while dragging
                visible: grab.drag.active && placer.snapX
                x: placer.picX + frame.paintedWidth / 2; y: placer.picY; width: 1; height: frame.paintedHeight; color: Theme.accent; opacity: 0.8
            }
            Rectangle {
                visible: grab.drag.active && placer.snapY
                x: placer.picX; y: placer.picY + frame.paintedHeight / 2; width: frame.paintedWidth; height: 1; color: Theme.accent; opacity: 0.8
            }
            Rectangle {
                id: outline
                readonly property real bx: placer.ready ? placer.picX + placer.info.blockX / 1000 * frame.paintedWidth : 0
                readonly property real by: placer.ready ? placer.picY + placer.info.blockY / 1000 * frame.paintedHeight : 0
                x: bx + placer.dx; y: by + placer.dy
                width: placer.ready ? Math.max(12, placer.info.blockW / 1000 * frame.paintedWidth) : 0
                height: placer.ready ? Math.max(12, placer.info.blockH / 1000 * frame.paintedHeight) : 0
                color: "transparent"
                border.width: grab.drag.active || grab.containsMouse ? 2 : 1
                border.color: Theme.focus
                opacity: grab.drag.active || grab.containsMouse ? 1.0 : 0.85
            }
            MouseArea {   // fixed to the block's resting rectangle so the drag delta is not skewed by the moving outline
                    id: grab
                    x: outline.bx - 4; y: outline.by - 4
                    width: outline.width + 8; height: outline.height + 8
                    hoverEnabled: true
                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    drag.target: ghost
                    drag.axis: Drag.XAndYAxis
                    onPressed: { ghost.x = 0; ghost.y = 0 }
                    Item { id: ghost }   // tracks the raw drag delta; the outline applies clamping and snapping
                    onPositionChanged: {
                        if (!drag.active) return
                        var minDx = placer.picX - outline.bx, maxDx = placer.picX + frame.paintedWidth - outline.width - outline.bx
                        var minDy = placer.picY - outline.by, maxDy = placer.picY + frame.paintedHeight - outline.height - outline.by
                        var nx = Math.max(minDx, Math.min(maxDx, ghost.x))
                        var ny = Math.max(minDy, Math.min(maxDy, ghost.y))
                        var cx = outline.bx + nx + outline.width / 2 - (placer.picX + frame.paintedWidth / 2)
                        var cy = outline.by + ny + outline.height / 2 - (placer.picY + frame.paintedHeight / 2)
                        placer.snapX = Math.abs(cx) < 7
                        placer.snapY = Math.abs(cy) < 7
                        if (placer.snapX) nx -= cx
                        if (placer.snapY) ny -= cy
                        placer.dx = nx; placer.dy = ny
                    }
                    onReleased: {
                        if (placer.dx === 0 && placer.dy === 0) { placer.snapX = false; placer.snapY = false; return }
                        var cx = (outline.bx + placer.dx + outline.width / 2 - placer.picX) / frame.paintedWidth * 1000
                        var cy = (outline.by + placer.dy + outline.height / 2 - placer.picY) / frame.paintedHeight * 1000
                        if (placer.snapX) cx = 500
                        if (placer.snapY) cy = 500
                        placer.dx = 0; placer.dy = 0; placer.snapX = false; placer.snapY = false
                        controller.setSelectedTitlePosition(Math.round(cx), Math.round(cy))
                    }
            }
        }

        Column {
            anchors.centerIn: parent
            spacing: mat.height < 180 ? 6 : 12
            visible: controller.previewSource.length === 0 && controller.viewerState === "empty"
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "PULSAR"; color: Theme.textMuted; font.pixelSize: mat.height < 180 ? 18 : Theme.fontSizeXXL; font.bold: true; font.letterSpacing: 6 }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: controller.busy ? "Preparando medio…" : "Importá un medio para comenzar"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeM }
            PButton { anchors.horizontalCenter: parent.horizontalCenter; visible: !controller.busy; primary: true; text: "Importar medio"; onClicked: pane.importRequested() }
            Text { anchors.horizontalCenter: parent.horizontalCenter; visible: !controller.busy && mat.height >= 220; text: "También podés arrastrar videos, fotos, música o una carpeta a esta ventana."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
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
