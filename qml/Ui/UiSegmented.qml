import QtQuick
import "../"

pragma ComponentBehavior: Bound

// UiSegmented — segmented switch per the design spec: a frame with a
// hairline border (Theme.border), segments sized to text + padding, an
// animated thumb behind the selected segment. Colors come from Theme
// tokens (isDark honored automatically). Plain Items + MouseArea —
// deliberately no TabBar/ButtonGroup/RadioButton whose style paddings
// and backgrounds made earlier attempts drift.
//
// model: ["dev", "test", "prod"]; the current selection is
// currentIndex, switches emit activated(index, value).
FocusScope {
    id: root

    property var model: []                 // ["dev", "test", "prod"]
    property int currentIndex: 0
    readonly property string currentValue: model.length ? model[currentIndex] : ""
    // Optional monospace family for CLI tokens (e.g. "-v/-vv/-vvv");
    // empty falls back to the default application font family.
    property string fontFamily: ""
    // Stretch segments to share the full width equally (e.g. env switcher).
    // Requires an externally set width (Layout.fillWidth); standalone
    // implicit sizing keeps the text-sized segments.
    property bool stretch: false

    signal activated(int index, string value)

    readonly property int borderW: 1
    readonly property int pad: 2
    readonly property int segH: 20
    readonly property int segPadX: 9
    readonly property int gap: 2

    implicitWidth: row.implicitWidth + (borderW + pad) * 2
    implicitHeight: segH + (borderW + pad) * 2          // 26
    activeFocusOnTab: true

    function select(i) {
        if (i < 0 || i >= model.length || i === currentIndex) return
        currentIndex = i
        activated(i, model[i])
    }

    Keys.onLeftPressed: select(currentIndex - 1)
    Keys.onRightPressed: select(currentIndex + 1)

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: 6
        color: Theme.windowBackground
        border.width: root.borderW
        border.color: Theme.border
    }

    Rectangle {
        id: thumb
        property Item target: rep.count > root.currentIndex ? rep.itemAt(root.currentIndex) : null
        x: root.borderW + root.pad + (target ? target.x : 0)
        y: root.borderW + root.pad
        width: target ? target.width : 0
        height: root.segH
        radius: 4
        color: Theme.mutedSurface
        Behavior on x     { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
    }

    Row {
        id: row
        x: root.borderW + root.pad
        y: root.borderW + root.pad
        width: root.stretch ? root.width - (root.borderW + root.pad) * 2 : implicitWidth
        spacing: root.gap

        Repeater {
            id: rep
            model: root.model
            delegate: Item {
                id: segment
                required property int index
                required property string modelData
                width: root.stretch
                    ? Math.max(0, (row.width - root.gap * (rep.count - 1)) / Math.max(1, rep.count))
                    : Math.ceil(label.implicitWidth) + root.segPadX * 2
                height: root.segH

                Text {
                    id: label
                    anchors.centerIn: parent
                    text: segment.modelData
                    font.pixelSize: Theme.fontSizeSm        // 11
                    font.weight: Font.DemiBold
                    font.family: root.fontFamily
                    renderType: Text.NativeRendering
                    color: segment.index === root.currentIndex ? Theme.textPrimary
                         : mouse.containsMouse                 ? Theme.textMuted
                                                               : Theme.textSubtle
                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { root.forceActiveFocus(); root.select(segment.index) }
                }
            }
        }
    }

    Rectangle {                              // focus ring
        anchors.fill: parent
        anchors.margins: -3
        radius: 9
        color: "transparent"
        border.width: 2
        border.color: Theme.ring
        visible: root.activeFocus
    }
}
