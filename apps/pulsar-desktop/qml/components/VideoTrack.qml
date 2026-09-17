import QtQuick
import "../theme/Theme.js" as Theme

Item {
    id: track
    Rectangle { anchors.left: parent.left; anchors.right: parent.right; y: 26; height: 1; color: Theme.edge; opacity: 0.65 }
    Text { x: 10; y: 5; text: "V1"; color: Theme.faint; font.pixelSize: Theme.fontSizeXS; font.bold: true }
    Repeater {
        model: controller.timelineClips
        delegate: ClipBlock {
            y: 9
            canonicalStartX: modelData.startProgress * track.width
            canonicalWidth: Math.max(3, (modelData.endProgress - modelData.startProgress) * track.width)
            height: track.height - 14
            timelineWidth: track.width
            clipName: modelData.name
            startText: modelData.start
            endText: modelData.end
            missing: modelData.missing
            selected: modelData.selected
            clipKey: modelData.clipKey
            onActivated: controller.selectTimelineClip(clipKey)
        }
    }
}
