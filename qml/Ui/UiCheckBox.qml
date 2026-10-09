import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../"
pragma ComponentBehavior: Bound

// UiCheckBox — checkbox styled after Nuxt UI Checkbox: an 18px box with
// a radius, empty with a ring border; checked is filled with accent and
// a black checkmark (check.png reads on accent in both themes, hence no
// -dark pair). The state is owned by the consumer: `checked` is bound
// from above, the toggle goes out via the toggled signal (never assign
// to checked inside — that would break the binding).
RowLayout {
    id: root
    spacing: 8

    property bool checked: false
    property string label: ""

    signal toggled(bool on)

    // Box size/radius are specific to this component.
    readonly property int boxSize: 18
    readonly property int boxRadius: 4

    function flip() { root.toggled(!root.checked) }

    Rectangle {
        Layout.preferredWidth: root.boxSize
        Layout.preferredHeight: root.boxSize
        Layout.alignment: Qt.AlignVCenter
        radius: root.boxRadius
        color: root.checked ? Theme.accent : "transparent"
        border.color: root.checked ? Theme.accent : Theme.border
        border.width: 1

        Image {
            anchors.centerIn: parent
            width: 12
            height: 12
            visible: root.checked
            source: Theme.icon("check")
            sourceSize.width: 24
            sourceSize.height: 24
            fillMode: Image.PreserveAspectFit
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.flip()
        }
    }

    Label {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignVCenter
        text: root.label
        color: Theme.textPrimary
        font.pixelSize: Theme.fontSizeMd

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.flip()
        }
    }

    Accessible.role: Accessible.CheckBox
    Accessible.name: root.label
    activeFocusOnTab: true
    Keys.onSpacePressed: root.flip()
}
