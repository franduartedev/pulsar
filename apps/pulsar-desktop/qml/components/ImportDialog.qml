import QtQuick
import Qt.labs.folderlistmodel
import "../theme/Theme.js" as Theme

// "Importar medios": a small PULSAR-styled browser used for files AND folders (replaces the toolkit's
// light, foreign-looking file dialog, which also could not multi-select on Qt 6.4). It only picks
// paths; everything that follows (validation, folder expansion, duplicates, batch progress) is the
// controller's existing openPaths(). Esc / Cancelar changes nothing.
Item {
    id: dialog
    property bool folderMode: false
    property string currentPath: ""
    property var selection: []              // absolute paths of the ticked files
    property bool showAllFiles: false
    property string problem: ""
    readonly property var mediaFilters: ["*.mp4", "*.mov", "*.mkv", "*.avi", "*.webm", "*.m4v", "*.wav", "*.mp3", "*.m4a", "*.aac", "*.flac", "*.png", "*.jpg", "*.jpeg", "*.webp"]
    readonly property var places: controller.importPlaces()

    visible: false
    anchors.fill: parent
    z: 1000

    function openFiles() { folderMode = false; show() }
    function openFolder() { folderMode = true; show() }
    function show() {
        selection = []
        problem = ""
        if (currentPath.length === 0 || !controller.isDirectory(currentPath)) currentPath = controller.importStartFolder()
        pathField.text = currentPath
        visible = true
        list.forceActiveFocus()
    }
    function close() { visible = false }
    function cd(path) {
        if (!controller.isDirectory(path)) { problem = "Esa carpeta no existe o no se puede abrir."; return false }
        problem = ""
        currentPath = path
        pathField.text = path
        return true
    }
    function up() {
        if (currentPath === "/") return
        var parent = currentPath.replace(/\/+$/, "")
        parent = parent.substring(0, parent.lastIndexOf("/"))
        cd(parent.length === 0 ? "/" : parent)
    }
    function localPath(url) { return decodeURIComponent(url.toString().replace(/^file:\/\//, "")) }
    function isSelected(path) { return selection.indexOf(path) >= 0 }
    function toggle(path) {
        var next = selection.slice()
        var at = next.indexOf(path)
        if (at >= 0) next.splice(at, 1); else next.push(path)
        selection = next
    }
    function selectAll() {
        var all = []
        for (var i = 0; i < model.count; ++i) if (!model.get(i, "fileIsDir")) all.push(model.get(i, "filePath"))
        selection = (selection.length === all.length) ? [] : all
    }
    function fileCount() { var n = 0; for (var i = 0; i < model.count; ++i) if (!model.get(i, "fileIsDir")) ++n; return n }
    function accept() {
        if (folderMode || selection.length === 0) { importFolderHere(); return }
        var paths = selection.slice()
        close()
        controller.openPaths(paths)
    }
    function importFolderHere() {
        var path = currentPath
        close()
        controller.openPaths([path])
    }
    function sizeText(bytes) {
        if (bytes >= 1073741824) return (bytes / 1073741824).toFixed(1) + " GB"
        if (bytes >= 1048576) return (bytes / 1048576).toFixed(bytes >= 104857600 ? 0 : 1) + " MB"
        return Math.max(1, Math.round(bytes / 1024)) + " KB"
    }
    function kindOf(name) {
        var ext = name.toLowerCase().split(".").pop()
        if (["png", "jpg", "jpeg", "webp"].indexOf(ext) >= 0) return "image"
        if (["wav", "mp3", "m4a", "aac", "flac"].indexOf(ext) >= 0) return "audio"
        if (["mp4", "mov", "mkv", "avi", "webm", "m4v"].indexOf(ext) >= 0) return "video"
        return ""
    }

    FolderListModel {
        id: model
        folder: dialog.currentPath.length > 0 ? "file://" + encodeURI(dialog.currentPath).replace(/#/g, "%23").replace(/\?/g, "%3F") : ""
        showDirs: true
        showFiles: true
        showDotAndDotDot: false
        showHidden: false
        showDirsFirst: true
        caseSensitive: false
        sortField: FolderListModel.Name
        nameFilters: dialog.showAllFiles ? ["*"] : dialog.mediaFilters
    }

    Rectangle { anchors.fill: parent; color: Theme.overlayScrim }
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; hoverEnabled: true }

    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(820, parent.width - 48)
        height: Math.min(540, parent.height - 48)
        radius: Theme.radiusL
        color: Theme.bgElevated
        border.color: Theme.borderStrong
        focus: true
        Keys.onEscapePressed: dialog.close()

        Text { id: heading; x: Theme.spaceXL; y: Theme.spaceL; text: dialog.folderMode ? "Importar una carpeta" : "Importar medios"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeXL; font.bold: true }

        // ---- where am I: up button + editable path ----
        Row {
            id: pathRow
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: heading.bottom
            anchors.leftMargin: Theme.spaceXL; anchors.rightMargin: Theme.spaceXL; anchors.topMargin: Theme.spaceM
            spacing: Theme.spaceS
            PButton { width: 34; height: 32; text: "↑"; onClicked: dialog.up() }
            Rectangle {
                width: parent.width - 34 - Theme.spaceS; height: 32
                radius: Theme.radiusM; color: Theme.bgInput
                border.width: 1; border.color: dialog.problem.length > 0 ? Theme.error : (pathField.activeFocus ? Theme.focus : Theme.borderStrong)
                TextInput {
                    id: pathField
                    anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10
                    verticalAlignment: TextInput.AlignVCenter
                    color: Theme.textPrimary; selectedTextColor: Theme.textOnAccent; selectionColor: Theme.accent
                    font.pixelSize: Theme.fontSizeM; clip: true; selectByMouse: true
                    onAccepted: {
                        var typed = text.trim()
                        if (controller.isDirectory(typed)) { dialog.cd(typed); list.forceActiveFocus() }
                        else if (controller.pathExists(typed) && !dialog.folderMode) { dialog.close(); controller.openPaths([typed]) }
                        else dialog.problem = "No existe esa ruta."
                    }
                    Keys.onEscapePressed: { text = dialog.currentPath; list.forceActiveFocus() }
                }
            }
        }

        // ---- places ----
        Column {
            id: placesColumn
            anchors.left: parent.left; anchors.top: pathRow.bottom; anchors.bottom: footer.top
            anchors.leftMargin: Theme.spaceXL; anchors.topMargin: Theme.spaceM; anchors.bottomMargin: Theme.spaceM
            width: 138
            spacing: 2
            Text { text: "LUGARES"; color: Theme.textMuted; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1; bottomPadding: 4 }
            Repeater {
                model: dialog.places
                delegate: Rectangle {
                    width: placesColumn.width; height: 30; radius: Theme.radiusM
                    readonly property bool here: dialog.currentPath === modelData.path
                    color: here ? Theme.accentDim : (placeMouse.containsMouse ? Theme.bgHover : "transparent")
                    Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: modelData.name; elide: Text.ElideRight
                           color: parent.here ? Theme.focus : Theme.textSecondary; font.pixelSize: Theme.fontSizeM; font.bold: parent.here }
                    MouseArea { id: placeMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: dialog.cd(modelData.path) }
                }
            }
        }

        // ---- the folder listing ----
        Rectangle {
            id: listFrame
            anchors.left: placesColumn.right; anchors.right: parent.right; anchors.top: pathRow.bottom; anchors.bottom: footer.top
            anchors.leftMargin: Theme.spaceM; anchors.rightMargin: Theme.spaceXL; anchors.topMargin: Theme.spaceM; anchors.bottomMargin: Theme.spaceM
            radius: Theme.radiusM; color: Theme.bgPanel
            border.width: 1; border.color: Theme.borderSubtle
            clip: true
            ListView {
                id: list
                anchors.fill: parent; anchors.margins: 4
                model: model
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                activeFocusOnTab: true
                Keys.onReturnPressed: dialog.accept()
                Keys.onEnterPressed: dialog.accept()
                Keys.onEscapePressed: dialog.close()
                delegate: Rectangle {
                    id: row
                    width: list.width; height: 34; radius: Theme.radiusS
                    readonly property bool dir: model.fileIsDir
                    readonly property string kind: dialog.kindOf(model.fileName)
                    readonly property bool picked: !dir && dialog.isSelected(model.filePath)
                    readonly property bool selectable: !dir && !dialog.folderMode
                    color: picked ? Theme.accentDim : (rowMouse.containsMouse ? Theme.bgHover : "transparent")
                    opacity: dir || selectable ? 1.0 : 0.45
                    // tick box (files) / folder mark
                    Rectangle {
                        id: mark
                        anchors.left: parent.left; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter
                        width: 18; height: 18; radius: row.dir ? 3 : 4
                        color: row.dir ? Theme.accentDim : (row.picked ? Theme.accent : "transparent")
                        border.width: row.dir ? 0 : 1; border.color: row.picked ? Theme.accent : Theme.borderStrong
                        Text { visible: row.picked; anchors.centerIn: parent; text: "✓"; color: Theme.textOnAccent; font.pixelSize: 12; font.bold: true }
                        Rectangle { visible: row.dir; anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; anchors.margins: 3; height: 8; radius: 2; color: Theme.focus; opacity: 0.8 }
                    }
                    Text { anchors.left: mark.right; anchors.leftMargin: 10; anchors.right: info.left; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter
                           text: model.fileName; elide: Text.ElideMiddle; color: row.dir ? Theme.textPrimary : Theme.textPrimary; font.pixelSize: Theme.fontSizeM; font.bold: row.dir }
                    Text { id: info; anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; horizontalAlignment: Text.AlignRight
                           text: row.dir ? "carpeta" : ((row.kind === "video" ? "Video" : row.kind === "audio" ? "Audio" : row.kind === "image" ? "Imagen" : "Otro") + " · " + dialog.sizeText(model.fileSize))
                           color: Theme.textMuted; font.pixelSize: Theme.fontSizeXS }
                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: { list.currentIndex = index; if (row.dir) dialog.cd(model.filePath); else if (row.selectable) dialog.toggle(model.filePath) }
                        onDoubleClicked: { if (row.selectable) { dialog.close(); controller.openPaths([model.filePath]) } }
                    }
                }
                Text {
                    visible: list.count === 0
                    anchors.centerIn: parent; width: parent.width - 40; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                    text: dialog.showAllFiles ? "Esta carpeta está vacía." : "Esta carpeta no tiene videos, fotos ni audio que PULSAR pueda importar."
                    color: Theme.textMuted; font.pixelSize: Theme.fontSizeS
                }
            }
        }

        // ---- footer ----
        Item {
            id: footer
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.spaceXL; anchors.rightMargin: Theme.spaceXL; anchors.bottomMargin: Theme.spaceL
            height: 34
            Row {
                anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; spacing: Theme.spaceM
                PButton { visible: !dialog.folderMode && dialog.fileCount() > 0; height: 30; flat: true; text: dialog.selection.length === dialog.fileCount() && dialog.selection.length > 0 ? "Quitar selección" : "Seleccionar todo"; onClicked: dialog.selectAll() }
                Row {
                    spacing: 6; anchors.verticalCenter: parent.verticalCenter
                    Rectangle { width: 30; height: 16; radius: 8; anchors.verticalCenter: parent.verticalCenter; color: dialog.showAllFiles ? Theme.accent : Theme.track
                        Rectangle { y: 2; x: dialog.showAllFiles ? parent.width - width - 2 : 2; width: 12; height: 12; radius: 6; color: Theme.textPrimary }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: dialog.showAllFiles = !dialog.showAllFiles } }
                    Text { anchors.verticalCenter: parent.verticalCenter; text: "Mostrar todos los archivos"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeXS }
                }
            }
            Text { visible: dialog.problem.length > 0; anchors.centerIn: parent; text: dialog.problem; color: Theme.error; font.pixelSize: Theme.fontSizeS }
            Row {
                anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; spacing: Theme.spaceS
                PButton { height: 34; text: "Cancelar"; onClicked: dialog.close() }
                PButton { height: 34; visible: !dialog.folderMode; text: "Importar esta carpeta"; onClicked: dialog.importFolderHere() }
                PButton {
                    height: 34; primary: true
                    text: dialog.folderMode ? "Importar esta carpeta" : (dialog.selection.length === 0 ? "Importar" : "Importar " + dialog.selection.length + (dialog.selection.length === 1 ? " archivo" : " archivos"))
                    enabled: dialog.folderMode || dialog.selection.length > 0
                    onClicked: dialog.accept()
                }
            }
        }
    }
}
