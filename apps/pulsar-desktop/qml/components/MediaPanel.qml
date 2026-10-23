import QtQuick
import "../theme/Theme.js" as Theme

// Media bin: real tabs (Medios / Sonidos), a kind filter and a search box, one row per asset with a
// type icon, name and short secondary info. Technical detail lives in the Inspector.
Panel {
    signal importRequested()
    signal importFolderRequested()
    signal relinkRequested(string mediaId)
    // "Placa de color": the palette is hosted by Main (scene coordinates of the button).
    signal createColorRequested(real sx, real sy)
    // Drag from the library towards the timeline (scene coordinates; Main hosts the ghost).
    signal assetDragStarted(string mediaId, string name, bool isVideo)
    signal assetDragMoved(string mediaId, real sx, real sy)
    signal assetDragEnded(string mediaId, real sx, real sy)
    id: mediaPanel

    function focusPath() {
        pathInput.forceActiveFocus()
        pathInput.selectAll()
    }

    property bool soundsTab: false
    // Medios filter ("" = all, "video", "image", "audio") and the shared name search.
    property string kindFilter: ""
    property string searchText: ""
    onKindFilterChanged: controller.mediaAssets.setFilter(kindFilter, searchText)
    onSearchTextChanged: controller.mediaAssets.setFilter(kindFilter, searchText)
    // Sonidos category filter ("" = all). Categories come from the catalogue, in its order.
    property string soundCategory: ""
    readonly property var soundCategories: {
        var seen = {}, out = []
        var list = controller.soundLibrary
        for (var i = 0; i < list.length; ++i) if (!seen[list[i].category]) { seen[list[i].category] = true; out.push(list[i].category) }
        return out
    }
    readonly property var visibleSounds: {
        var list = controller.soundLibrary, out = [], needle = searchText.toLowerCase()
        for (var i = 0; i < list.length; ++i) {
            if (soundCategory !== "" && list[i].category !== soundCategory) continue
            if (needle.length > 0 && list[i].name.toLowerCase().indexOf(needle) < 0) continue
            out.push(list[i])
        }
        return out
    }
    readonly property int totalAssets: controller.mediaAssets.totalCount
    readonly property bool showSearch: height > 520 && (soundsTab ? controller.soundLibrary.length > 8 : totalAssets > 6)

    Text { id: title; anchors.left: parent.left; anchors.top: parent.top; anchors.leftMargin: Theme.spaceL; anchors.topMargin: Theme.spaceL; text: "Biblioteca"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeL; font.bold: true }
    PButton {
        id: importButton
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: title.bottom
        anchors.leftMargin: Theme.spaceM; anchors.rightMargin: Theme.spaceM; anchors.topMargin: Theme.spaceM
        primary: controller.importPending === 0
        text: controller.importPending > 1 ? "Importando " + (controller.importTotal - controller.importPending + 1) + " de " + controller.importTotal + "…  Cancelar" : "+  Importar medio"
        onClicked: controller.importPending > 1 ? controller.cancelImport() : mediaPanel.importRequested()
    }

    // Secondary ways to bring material in or make it.
    Row {
        id: extras
        visible: controller.importPending === 0
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: importButton.bottom
        anchors.leftMargin: Theme.spaceM; anchors.rightMargin: Theme.spaceM; anchors.topMargin: 4
        spacing: 4
        PButton {
            flat: true; height: 26
            width: (parent.width - 4) / 2
            text: "Carpeta…"
            onClicked: mediaPanel.importFolderRequested()
        }
        PButton {
            id: colorButton
            flat: true; height: 26
            width: (parent.width - 4) / 2
            text: "Placa de color"
            onClicked: { var p = colorButton.mapToItem(null, 0, colorButton.height + 4); mediaPanel.createColorRequested(p.x, p.y) }
        }
    }

    // Underline tabs.
    Item {
        id: tabs
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: extras.bottom
        anchors.leftMargin: Theme.spaceM; anchors.rightMargin: Theme.spaceM; anchors.topMargin: Theme.spaceS
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

    // Filter chips (+ search when the bin is long enough to need it).
    Column {
        id: filters
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: tabs.bottom
        anchors.leftMargin: Theme.spaceM; anchors.rightMargin: Theme.spaceM; anchors.topMargin: Theme.spaceS
        spacing: 6
        visible: mediaPanel.soundsTab ? controller.soundPackAvailable : mediaPanel.totalAssets > 0
        // One scrollable line: ten sound categories must not push the list off a 768 px screen.
        Flickable {
            id: chipStrip
            width: parent.width; height: 22
            contentWidth: chipRow.width; contentHeight: 22
            clip: true
            interactive: contentWidth > width
            boundsBehavior: Flickable.StopAtBounds
            WheelHandler { acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: function(event) { chipStrip.contentX = Math.max(0, Math.min(chipStrip.contentWidth - chipStrip.width, chipStrip.contentX - (event.angleDelta.y + event.angleDelta.x) / 2)) } }
        Row {
            id: chipRow
            spacing: 4
            Repeater {
                model: mediaPanel.soundsTab
                       ? [{label: "Todos", key: ""}].concat(mediaPanel.soundCategories.map(function(c) { return {label: c, key: c} }))
                       : [{label: "Todo", key: ""}, {label: "Video", key: "video"}, {label: "Imagen", key: "image"}, {label: "Audio", key: "audio"}]
                delegate: Rectangle {
                    readonly property bool current: (mediaPanel.soundsTab ? mediaPanel.soundCategory : mediaPanel.kindFilter) === modelData.key
                    height: 22
                    width: chipText.implicitWidth + 16
                    radius: 11
                    color: current ? Theme.accentDim : (chipMouse.containsMouse ? Theme.bgHover : Theme.bgElevated)
                    border.width: 1
                    border.color: current ? Theme.accent : "transparent"
                    Text { id: chipText; anchors.centerIn: parent; text: modelData.label; color: parent.current ? Theme.textPrimary : Theme.textSecondary; font.pixelSize: Theme.fontSizeXS }
                    MouseArea { id: chipMouse; anchors.fill: parent; hoverEnabled: true; onClicked: { if (mediaPanel.soundsTab) mediaPanel.soundCategory = modelData.key; else mediaPanel.kindFilter = modelData.key } }
                }
            }
        }
        }
        Rectangle {
            visible: mediaPanel.showSearch
            width: parent.width; height: 26
            color: Theme.bgInput; radius: Theme.radiusM
            border.width: 1; border.color: searchInput.activeFocus ? Theme.focus : Theme.borderSubtle
            TextInput {
                id: searchInput
                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8
                verticalAlignment: TextInput.AlignVCenter; clip: true
                color: Theme.textPrimary; selectedTextColor: Theme.textOnAccent; selectionColor: Theme.accent
                font.pixelSize: Theme.fontSizeS
                onTextChanged: mediaPanel.searchText = text
                Keys.onEscapePressed: { text = ""; focus = false }
                onAccepted: focus = false
            }
            Text { anchors.left: parent.left; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter; text: "Buscar por nombre"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeS; visible: searchInput.text.length === 0 && !searchInput.activeFocus }
        }
    }

    // ---- Medios -------------------------------------------------------
    ListView {
        id: mediaList
        visible: !mediaPanel.soundsTab
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: filters.visible ? filters.bottom : tabs.bottom
        anchors.bottom: pathRow.visible ? pathLabel.top : parent.bottom
        anchors.leftMargin: Theme.spaceM; anchors.rightMargin: Theme.spaceM
        anchors.topMargin: Theme.spaceS; anchors.bottomMargin: Theme.spaceS
        clip: true; spacing: 4
        boundsBehavior: Flickable.StopAtBounds
        model: controller.mediaAssets
        delegate: Rectangle {
            id: assetRow
            readonly property bool current: model.mediaId === controller.selectedAssetId
            // Anything that lives on VIDEO 1: footage, photos and colour cards.
            readonly property bool isVideo: model.kind === "video" || model.kind === "image" || model.kind === "color"
            readonly property bool isAudio: model.kind === "audio"
            // Actions only on the selected asset: one quiet row, no per-row buttons.
            readonly property bool showActions: current && (isVideo || isAudio)
            width: mediaList.width
            height: 52 + (showActions ? 38 : 0)
            radius: Theme.radiusM
            color: current ? Theme.bgSelected : (rowMouse.containsMouse ? Theme.bgHover : "transparent")
            opacity: model.available ? 1.0 : 0.6
            MouseArea {
                id: rowMouse
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 52
                hoverEnabled: true
                preventStealing: true
                property real pressX: 0
                property real pressY: 0
                property bool dragging: false
                property bool wasDragged: false
                onPressed: function(mouse) { pressX = mouse.x; pressY = mouse.y; dragging = false; wasDragged = false }
                onPositionChanged: function(mouse) {
                    if (!pressed) return
                    if (!dragging && model.available && Math.abs(mouse.x - pressX) + Math.abs(mouse.y - pressY) > 10) {
                        dragging = true
                        mediaPanel.assetDragStarted(model.mediaId, model.name, assetRow.isVideo)
                    }
                    if (dragging) { var p = rowMouse.mapToItem(null, mouse.x, mouse.y); mediaPanel.assetDragMoved(model.mediaId, p.x, p.y) }
                }
                onReleased: function(mouse) {
                    if (!dragging) return
                    var p = rowMouse.mapToItem(null, mouse.x, mouse.y)
                    dragging = false; wasDragged = true
                    mediaPanel.assetDragEnded(model.mediaId, p.x, p.y)
                }
                onClicked: if (!wasDragged) controller.selectAsset(model.mediaId)
                // Same action as the Insertar button.
                onDoubleClicked: { wasDragged = false; controller.selectAsset(model.mediaId); if ((assetRow.isVideo || assetRow.isAudio) && model.available) controller.insertAssetAtPlayhead(model.mediaId) }
            }
            Rectangle { visible: assetRow.current; anchors.left: parent.left; anchors.top: parent.top; anchors.topMargin: 10; width: 2; height: 32; radius: 1; color: Theme.accent }
            Rectangle {
                id: iconBox
                anchors.left: parent.left; anchors.leftMargin: 12; anchors.top: parent.top; anchors.topMargin: 10
                width: 32; height: 32; radius: Theme.radiusM; color: Theme.bgElevated
                // A colour card shows its colour; everything else a glyph for its kind.
                Rectangle {
                    visible: model.kind === "color"
                    anchors.fill: parent; anchors.margins: 5; radius: 3
                    color: model.kind === "color" ? ("#" + model.name.substring(model.name.length - 6)) : "transparent"
                    border.width: 1; border.color: Theme.lightBorder
                }
                TGlyph {
                    visible: model.kind !== "color"
                    anchors.centerIn: parent; width: 18; height: 18
                    kind: model.kind === "audio" ? "audio" : (model.kind === "image" ? "image" : "video")
                    ink: model.kind === "audio" ? Theme.waveformAudio : (model.kind === "image" ? Theme.imageInk : Theme.waveform)
                }
            }
            Column {
                anchors.left: iconBox.right; anchors.leftMargin: 10; anchors.right: parent.right; anchors.rightMargin: 10
                anchors.top: parent.top; anchors.topMargin: 10
                spacing: 2
                Text { width: parent.width; text: model.name; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeM; elide: Text.ElideMiddle }
                Text { width: parent.width; text: model.info; color: model.available ? Theme.textSecondary : Theme.error; font.pixelSize: Theme.fontSizeXS; elide: Text.ElideRight }
            }
            Row {
                visible: assetRow.showActions
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                anchors.leftMargin: 12; anchors.rightMargin: 12; anchors.bottomMargin: 8
                spacing: 6
                PButton { visible: model.available; width: assetRow.isAudio ? parent.width : (parent.width - 6) / 2; height: Theme.controlHeightSmall; primary: true; text: assetRow.isAudio ? "Insertar en el cabezal" : "Insertar"; onClicked: controller.insertAssetAtPlayhead(model.mediaId) }
                PButton { visible: model.available && !assetRow.isAudio; width: (parent.width - 6) / 2; height: Theme.controlHeightSmall; text: "Al final"; onClicked: controller.appendAssetToVideoTrack(model.mediaId) }
                PButton { visible: !model.available; width: parent.width; height: Theme.controlHeightSmall; primary: true; text: "Relinkear archivo…"; onClicked: mediaPanel.relinkRequested(model.mediaId) }
            }
        }
    }
    Column {
        visible: !mediaPanel.soundsTab && mediaList.count === 0
        anchors.horizontalCenter: parent.horizontalCenter; anchors.top: filters.visible ? filters.bottom : tabs.bottom; anchors.topMargin: 36
        width: parent.width - 2 * Theme.spaceXL
        spacing: 6
        Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: mediaPanel.totalAssets === 0 ? "Sin medios" : "Nada coincide"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeM }
        Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; text: mediaPanel.totalAssets === 0 ? "Importá videos, fotos o audio, o arrastralos a la ventana. También podés importar una carpeta entera." : "Probá otro filtro o borrá la búsqueda."; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
    }

    // ---- Sonidos ------------------------------------------------------
    ListView {
        id: soundsList
        visible: mediaPanel.soundsTab
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: filters.visible ? filters.bottom : tabs.bottom; anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.spaceM; anchors.rightMargin: Theme.spaceM
        anchors.topMargin: Theme.spaceS; anchors.bottomMargin: Theme.spaceM
        clip: true; spacing: 4; model: mediaPanel.visibleSounds
        boundsBehavior: Flickable.StopAtBounds
        delegate: Rectangle {
            width: soundsList.width; height: 48; radius: Theme.radiusM
            color: soundMouse.containsMouse ? Theme.bgHover : "transparent"
            // Listen first: the play button auditions the sound without touching the project.
            Rectangle {
                id: soundIcon
                anchors.left: parent.left; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter
                width: 32; height: 32; radius: 16
                readonly property bool playing: controller.auditioningSound === modelData.id
                color: playing || playMouse.containsMouse ? Theme.accentDim : Theme.bgElevated
                border.width: playing || playMouse.containsMouse ? 1 : 0; border.color: Theme.accent
                TGlyph { anchors.centerIn: parent; width: 16; height: 16; kind: soundIcon.playing ? "pause" : "play"; ink: soundIcon.playing || playMouse.containsMouse ? Theme.accentHover : Theme.waveformAudio }
                MouseArea { id: playMouse; anchors.fill: parent; hoverEnabled: true; onClicked: controller.auditionSound(modelData.id) }
            }
            Column {
                anchors.left: soundIcon.right; anchors.leftMargin: 10; anchors.right: insert.left; anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Text { width: parent.width; text: modelData.name; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeM; elide: Text.ElideRight }
                Text { width: parent.width; text: modelData.category + " · " + modelData.duration; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeXS; elide: Text.ElideRight }
            }
            MouseArea { id: soundMouse; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
            PButton { id: insert; anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; height: Theme.controlHeightSmall; text: "Insertar"; onClicked: controller.insertBuiltinSound(modelData.id) }
        }
    }

    Text {
        visible: mediaPanel.soundsTab && !controller.soundPackAvailable
        anchors.horizontalCenter: parent.horizontalCenter; anchors.top: tabs.bottom; anchors.topMargin: 40
        width: parent.width - 2 * Theme.spaceXL
        horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
        text: "No se encontró el paquete de sonidos de PULSAR. Reinstalá la aplicación."
        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS
    }

    // ---- Open by path (secondary) -------------------------------------
    Text { id: pathLabel; visible: pathRow.visible; anchors.left: parent.left; anchors.leftMargin: Theme.spaceM; anchors.bottom: pathRow.top; anchors.bottomMargin: 6; text: "Archivo o carpeta (ruta)"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
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
            TextInput { id: pathInput; anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; verticalAlignment: TextInput.AlignVCenter; color: Theme.textPrimary; selectedTextColor: Theme.textOnAccent; selectionColor: Theme.accent; clip: true; font.pixelSize: Theme.fontSizeS; onAccepted: { controller.openPath(text); focus = false } }
            Text { anchors.left: parent.left; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter; text: "/ruta/al/video.mp4"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeS; visible: pathInput.text.length === 0 && !pathInput.activeFocus }
        }
        PButton { id: openButton; width: 56; text: "Abrir"; enabled: pathInput.text.length > 0; onClicked: controller.openPath(pathInput.text) }
    }
}
