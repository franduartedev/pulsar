import QtQuick
import "../theme/Theme.js" as Theme

Item {
    id: track
    Rectangle { anchors.left: parent.left; anchors.right: parent.right; y: 26; height: 1; color: Theme.edge; opacity: 0.65 }
    Text { x: 10; y: 5; text: "V1"; color: Theme.faint; font.pixelSize: Theme.fontSizeXS; font.bold: true }
    Repeater {
        model: controller.timelineClips
        delegate: ClipBlock {
            visible: model.inViewport
            y: 9
            canonicalStartX: model.startProgress * track.width
            canonicalWidth: Math.max(3, (model.endProgress - model.startProgress) * track.width)
            height: track.height - 14
            timelineWidth: track.width
            clipName: model.clipName
            startText: model.startText
            endText: model.endText
            durationSeconds: model.durationSeconds
            gainKeyframes: model.gainKeyframes
            missing: model.missing
            selected: model.selected
            muted: model.muted
            gainDb: model.gainDb
            clipKey: model.clipKey
            onActivated: controller.selectTimelineClip(clipKey)
        }
    }
}
