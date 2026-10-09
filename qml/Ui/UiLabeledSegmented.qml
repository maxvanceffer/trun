import QtQuick
import "../"

pragma ComponentBehavior: Bound

// UiLabeledSegmented — a segmented switch with a monospace caption on
// the left ("--env"). The caption is Menlo on purpose: in a
// proportional font the two hyphens collapse into an en-dash look.
Row {
    id: root

    property alias label: caption.text
    property alias model: seg.model
    property alias currentIndex: seg.currentIndex
    // Optional monospace family for segment labels (CLI tokens).
    property alias fontFamily: seg.fontFamily
    readonly property alias currentValue: seg.currentValue
    signal activated(int index, string value)

    spacing: 6

    Text {
        id: caption
        anchors.verticalCenter: parent.verticalCenter
        font.family: "Menlo"
        font.pixelSize: Theme.fontSizeSm          // 11
        color: Theme.textSubtle
    }

    UiSegmented {
        id: seg
        anchors.verticalCenter: parent.verticalCenter
        onActivated: (i, v) => root.activated(i, v)
    }
}
