import QtQuick
import "../theme/Theme.js" as Theme

// A titled group in the Inspector: small-caps heading, hairline above, content below.
Column {
    id: section
    property string heading: ""
    property bool first: false
    default property alias content: inner.data
    width: parent ? parent.width : 0
    spacing: Theme.spaceS
    Rectangle { visible: !section.first; width: parent.width; height: 1; color: Theme.borderSubtle }
    Item { visible: !section.first; width: 1; height: 2 }
    SectionLabel { visible: section.heading.length > 0; text: section.heading }
    Column { id: inner; width: parent.width; spacing: 6 }
}
