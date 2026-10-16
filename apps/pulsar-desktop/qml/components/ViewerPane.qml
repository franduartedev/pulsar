import QtQuick
import "../theme/Theme.js" as Theme

// Monitor surface: the picture is the hero. A single quiet line above it
// carries the source name and preview health; nothing overlays the image.
Item {
    id: pane
    objectName: "viewerPane"
    signal importRequested()

    // Viewer display fit. "fit" shows the whole project canvas (default, no
    // crop, aspect preserved); "actual" shows one project pixel per screen
    // pixel. Display-only: never changes CanvasSpec, a transform or export.
    property string fitMode: "fit"

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
            anchors.right: fitControl.left; anchors.rightMargin: Theme.spaceM
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            StatusDot { anchors.verticalCenter: parent.verticalCenter; tone: "warn" }
            Text { anchors.verticalCenter: parent.verticalCenter; text: "Preview con retraso"; color: Theme.warning; font.pixelSize: Theme.fontSizeXS }
        }
        Segmented {
            id: fitControl
            objectName: "viewerFitControl"
            anchors.right: parent.right; anchors.rightMargin: Theme.spaceL
            anchors.verticalCenter: parent.verticalCenter
            width: 128
            model: ["Ajustar", "100%"]
            current: pane.fitMode === "actual" ? 1 : 0
            onActivated: function(index) { pane.fitMode = index === 1 ? "actual" : "fit" }
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
        border.width: 1; border.color: Theme.borderSubtle
        clip: true

        Image {
            id: frame
            objectName: "viewerFrame"
            x: interactionOverlay.canvasX
            y: interactionOverlay.canvasY
            width: interactionOverlay.canvasWidth
            height: interactionOverlay.canvasHeight
            source: controller.previewSource
            cache: false
            fillMode: Image.PreserveAspectFit
            visible: controller.previewSource.length > 0 && width > 0 && height > 0
        }

        // Stable interaction surface. It owns the pointer grab independently of
        // the asynchronously replaced frame/texture. All gesture geometry is
        // snapshotted in this item's coordinate system at press time.
        Item {
            id: interactionOverlay
            objectName: "viewerInteractionOverlay"
            anchors.fill: parent
            z: 20
            readonly property var geometry: controller.selectedClipVisualGeometry
            readonly property real canvasAspect: controller.viewerCanvasAspectRatio
            readonly property bool actualSize: pane.fitMode === "actual"
            readonly property real fitCanvasWidth: canvasAspect > 0
                ? Math.min(width, height * canvasAspect) : 0
            readonly property real fitCanvasHeight: canvasAspect > 0
                ? Math.min(height, width / canvasAspect) : 0
            readonly property real canvasWidth: canvasAspect > 0
                ? (actualSize && controller.viewerCanvasPixelWidth > 0
                   ? controller.viewerCanvasPixelWidth : fitCanvasWidth) : 0
            readonly property real canvasHeight: canvasAspect > 0
                ? (actualSize && controller.viewerCanvasPixelHeight > 0
                   ? controller.viewerCanvasPixelHeight : fitCanvasHeight) : 0
            readonly property real canvasX: (width - canvasWidth) / 2
            readonly property real canvasY: (height - canvasHeight) / 2
            readonly property bool available: controller.selectedClipOnScreen
                && geometry !== undefined && geometry.width !== undefined
                && canvasWidth > 0 && canvasHeight > 0
            visible: available || interactionArea.gesture.length > 0

            Item {
                objectName: "viewerCanvasRect"
                x: interactionOverlay.canvasX; y: interactionOverlay.canvasY
                width: interactionOverlay.canvasWidth; height: interactionOverlay.canvasHeight
            }

            Item {
                id: selectionBounds
                objectName: "viewerSelectionBounds"
                x: interactionOverlay.available
                    ? interactionOverlay.canvasX + (interactionOverlay.geometry.centerX - interactionOverlay.geometry.width / 2) * interactionOverlay.canvasWidth : 0
                y: interactionOverlay.available
                    ? interactionOverlay.canvasY + (interactionOverlay.geometry.centerY - interactionOverlay.geometry.height / 2) * interactionOverlay.canvasHeight : 0
                width: interactionOverlay.available
                    ? Math.max(8, interactionOverlay.geometry.width * interactionOverlay.canvasWidth) : 8
                height: interactionOverlay.available
                    ? Math.max(8, interactionOverlay.geometry.height * interactionOverlay.canvasHeight) : 8
                rotation: interactionOverlay.available && interactionOverlay.geometry.rotation !== undefined
                    ? interactionOverlay.geometry.rotation : 0
                transformOrigin: Item.Center

                Rectangle {
                    anchors.fill: parent
                    color: "transparent"
                    border.width: 2
                    border.color: interactionArea.gesture.length > 0 || interactionArea.insideSelection
                        ? Theme.textOnAccent : Theme.focus
                    Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
                }
                Repeater {
                    model: [{sx: -1, sy: -1}, {sx: 1, sy: -1}, {sx: -1, sy: 1}, {sx: 1, sy: 1}]
                    delegate: Rectangle {
                        id: handle
                        objectName: "viewerScaleHandle_" + sx + "_" + sy
                        required property var modelData
                        readonly property int sx: modelData.sx
                        readonly property int sy: modelData.sy
                        // Keep the complete handle inside the canvas for a
                        // full-frame VIDEO 1 clip instead of clipping half of it.
                        x: sx < 0 ? 0 : selectionBounds.width - width
                        y: sy < 0 ? 0 : selectionBounds.height - height
                        width: 12; height: 12; radius: 3
                        scale: interactionArea.hoveredSx === sx && interactionArea.hoveredSy === sy ? 1.18 : 1.0
                        color: interactionArea.hoveredSx === sx && interactionArea.hoveredSy === sy
                            ? Theme.textOnAccent : Theme.focus
                        border.width: 1; border.color: Theme.bgViewer
                        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                        Behavior on scale { NumberAnimation { duration: Theme.motionFast } }
                    }
                }
            }

            MouseArea {
                id: interactionArea
                objectName: "viewerInteractionArea"
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                hoverEnabled: true
                preventStealing: true
                enabled: interactionOverlay.available || gesture.length > 0

                property string gesture: ""
                property int cornerSx: 0
                property int cornerSy: 0
                property int hoveredSx: 0
                property int hoveredSy: 0
                property bool insideSelection: false
                property bool firstMoveLogged: false
                property real pressX: 0
                property real pressY: 0
                property real startCanvasX: 0
                property real startCanvasY: 0
                property real startCanvasWidth: 0
                property real startCanvasHeight: 0
                property real startBoundsX: 0
                property real startBoundsY: 0
                property real startBoundsWidth: 0
                property real startBoundsHeight: 0
                property real startCenterX: 0
                property real startCenterY: 0
                property real startPosX: 0
                property real startPosY: 0
                property real startScale: 1
                property real startRotation: 0
                property real startOpacity: 1
                property real scaleVectorX: 0
                property real scaleVectorY: 0

                function cornerPoint(sx, sy) {
                    var radians = selectionBounds.rotation * Math.PI / 180
                    var dx = sx * selectionBounds.width / 2
                    var dy = sy * selectionBounds.height / 2
                    return Qt.point(selectionBounds.x + selectionBounds.width / 2
                                    + Math.cos(radians) * dx - Math.sin(radians) * dy,
                                    selectionBounds.y + selectionBounds.height / 2
                                    + Math.sin(radians) * dx + Math.cos(radians) * dy)
                }
                function cornerAt(px, py) {
                    var corners = [[-1, -1], [1, -1], [-1, 1], [1, 1]]
                    for (var index = 0; index < corners.length; ++index) {
                        var point = cornerPoint(corners[index][0], corners[index][1])
                        var dx = px - point.x; var dy = py - point.y
                        if (dx * dx + dy * dy <= 18 * 18) return corners[index]
                    }
                    return [0, 0]
                }
                function containsVisual(px, py) {
                    var cx = selectionBounds.x + selectionBounds.width / 2
                    var cy = selectionBounds.y + selectionBounds.height / 2
                    var radians = selectionBounds.rotation * Math.PI / 180
                    var dx = px - cx; var dy = py - cy
                    var localX = Math.cos(radians) * dx + Math.sin(radians) * dy
                    var localY = -Math.sin(radians) * dx + Math.cos(radians) * dy
                    return Math.abs(localX) <= selectionBounds.width / 2
                        && Math.abs(localY) <= selectionBounds.height / 2
                }
                function updateHover(px, py) {
                    if (gesture.length > 0) return
                    var corner = cornerAt(px, py)
                    hoveredSx = corner[0]; hoveredSy = corner[1]
                    insideSelection = containsVisual(px, py)
                }
                function debugData(currentX, currentY, normalizedX, normalizedY, ratio, proposedScale) {
                    return {
                        gesture: gesture.toUpperCase(), cornerX: cornerSx, cornerY: cornerSy,
                        corner: cornerSx < 0 ? (cornerSy < 0 ? "TOP_LEFT" : "BOTTOM_LEFT")
                            : (cornerSx > 0 ? (cornerSy < 0 ? "TOP_RIGHT" : "BOTTOM_RIGHT") : "NONE"),
                        pressX: pressX, pressY: pressY, currentX: currentX, currentY: currentY,
                        canvasX: startCanvasX, canvasY: startCanvasY,
                        canvasWidth: startCanvasWidth, canvasHeight: startCanvasHeight,
                        boundsX: startBoundsX, boundsY: startBoundsY,
                        boundsWidth: startBoundsWidth, boundsHeight: startBoundsHeight,
                        initialX: startPosX, initialY: startPosY, initialScale: startScale,
                        initialRotation: startRotation, initialOpacity: startOpacity,
                        normalizedX: normalizedX, normalizedY: normalizedY,
                        scaleRatio: ratio,
                        proposedX: gesture === "move" ? startPosX + normalizedX : startPosX,
                        proposedY: gesture === "move" ? startPosY + normalizedY : startPosY,
                        proposedScale: proposedScale
                    }
                }
                function clearGesture() {
                    gesture = ""; cornerSx = 0; cornerSy = 0; firstMoveLogged = false
                }

                cursorShape: gesture === "move" ? Qt.ClosedHandCursor
                    : ((gesture === "scale" ? cornerSx : hoveredSx) !== 0
                       ? (((gesture === "scale" ? cornerSx : hoveredSx)
                           === (gesture === "scale" ? cornerSy : hoveredSy))
                          ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor)
                       : (insideSelection ? Qt.SizeAllCursor : Qt.ArrowCursor))

                onPressed: function(mouse) {
                    var corner = cornerAt(mouse.x, mouse.y)
                    if (corner[0] === 0 && !containsVisual(mouse.x, mouse.y)) {
                        mouse.accepted = false
                        return
                    }
                    gesture = corner[0] === 0 ? "move" : "scale"
                    cornerSx = corner[0]; cornerSy = corner[1]
                    pressX = mouse.x; pressY = mouse.y
                    startCanvasX = interactionOverlay.canvasX; startCanvasY = interactionOverlay.canvasY
                    startCanvasWidth = interactionOverlay.canvasWidth; startCanvasHeight = interactionOverlay.canvasHeight
                    startBoundsX = selectionBounds.x; startBoundsY = selectionBounds.y
                    startBoundsWidth = selectionBounds.width; startBoundsHeight = selectionBounds.height
                    startCenterX = selectionBounds.x + selectionBounds.width / 2
                    startCenterY = selectionBounds.y + selectionBounds.height / 2
                    startPosX = controller.selectedClipPosX; startPosY = controller.selectedClipPosY
                    startScale = controller.selectedClipScale
                    startRotation = controller.selectedClipRotation; startOpacity = controller.selectedClipOpacity
                    scaleVectorX = pressX - startCenterX; scaleVectorY = pressY - startCenterY
                    firstMoveLogged = false
                    controller.debugViewerGesture("BEGIN", debugData(mouse.x, mouse.y, 0, 0, 1, startScale))
                    controller.beginSelectedClipTransform()
                }
                onPositionChanged: function(mouse) {
                    updateHover(mouse.x, mouse.y)
                    if (gesture.length === 0) return
                    var normalizedX = (mouse.x - pressX) / startCanvasWidth
                    var normalizedY = (mouse.y - pressY) / startCanvasHeight
                    var ratio = 1
                    var proposedScale = startScale
                    if (gesture === "move") {
                        controller.updateSelectedClipTransform(startPosX + normalizedX, startPosY + normalizedY,
                            startScale, startRotation, startOpacity)
                    } else {
                        var denominator = scaleVectorX * scaleVectorX + scaleVectorY * scaleVectorY
                        var currentX = mouse.x - startCenterX; var currentY = mouse.y - startCenterY
                        ratio = denominator > 0.0001
                            ? (currentX * scaleVectorX + currentY * scaleVectorY) / denominator : 1
                        proposedScale = Math.max(0.01, Math.min(100, startScale * Math.max(0.01, ratio)))
                        controller.updateSelectedClipTransform(startPosX, startPosY, proposedScale,
                            startRotation, startOpacity)
                    }
                    if (!firstMoveLogged) {
                        firstMoveLogged = true
                        controller.debugViewerGesture("FIRST_MOVE",
                            debugData(mouse.x, mouse.y, normalizedX, normalizedY, ratio, proposedScale))
                    }
                }
                onReleased: function(mouse) {
                    if (gesture.length === 0) return
                    var committed = Math.abs(controller.selectedClipPosX - startPosX) > 0.0000001
                        || Math.abs(controller.selectedClipPosY - startPosY) > 0.0000001
                        || Math.abs(controller.selectedClipScale - startScale) > 0.0000001
                    var data = debugData(mouse.x, mouse.y, 0, 0,
                        startScale > 0 ? controller.selectedClipScale / startScale : 1,
                        controller.selectedClipScale)
                    data.committed = committed
                    data.finalX = controller.selectedClipPosX; data.finalY = controller.selectedClipPosY
                    data.finalScale = controller.selectedClipScale
                    controller.endSelectedClipTransform()
                    controller.debugViewerGesture("RELEASE", data)
                    clearGesture()
                    updateHover(mouse.x, mouse.y)
                }
                onCanceled: {
                    if (gesture.length === 0) return
                    var data = debugData(pressX, pressY, 0, 0, 1, startScale)
                    data.committed = false
                    controller.cancelSelectedClipTransform()
                    controller.debugViewerGesture("CANCEL", data)
                    clearGesture()
                }
            }
        }

        Rectangle {
            // Discoverability cue, shown only when direct manipulation is
            // actually available for the selected visual at the playhead.
            visible: interactionOverlay.available && interactionArea.gesture.length === 0
            anchors.left: parent.left; anchors.top: parent.top
            anchors.leftMargin: Theme.spaceM; anchors.topMargin: Theme.spaceM
            height: 26; width: directHint.implicitWidth + 2 * Theme.spaceM
            radius: Theme.radiusM
            color: Theme.chipOverlay
            border.width: 1; border.color: Theme.accent
            opacity: interactionArea.insideSelection ? 1.0 : 0.88
            Text {
                id: directHint; anchors.centerIn: parent
                text: "Arrastrá para mover  ·  Esquinas para escalar"
                color: Theme.textPrimary; font.pixelSize: Theme.fontSizeXS; font.bold: true
            }
        }

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
            Text { width: Math.max(120, mat.width - 48); horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; anchors.horizontalCenter: parent.horizontalCenter; visible: !controller.busy && mat.height >= 220; text: "También podés arrastrar videos, fotos, música o una carpeta a esta ventana."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
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
