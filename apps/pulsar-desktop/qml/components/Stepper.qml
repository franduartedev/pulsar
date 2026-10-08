import QtQuick
import "../theme/Theme.js" as Theme

// "−  value  +" control for numeric settings with a fixed step.
Row {
    id: stepper
    property string valueText: ""
    signal decrement()
    signal increment()
    spacing: 2
    PButton { width: 26; height: Theme.controlHeightSmall; text: "−"; onClicked: stepper.decrement() }
    Rectangle {
        width: 58; height: Theme.controlHeightSmall; radius: Theme.radiusM; color: Theme.bgInput
        border.width: 1; border.color: Theme.borderStrong
        Text { anchors.centerIn: parent; text: stepper.valueText; color: Theme.textPrimary; font.family: Theme.mono; font.pixelSize: Theme.fontSizeS }
    }
    PButton { width: 26; height: Theme.controlHeightSmall; text: "+"; onClicked: stepper.increment() }
}
