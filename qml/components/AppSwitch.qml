import QtQuick
import "../"

// AppSwitch — small on/off toggle (SECRET column of the Environments dialog
// and anywhere a row-level on/off is needed). Bespoke Item + Rectangles, not
// QtQuick.Controls Switch: the kit owns sizes, insets and the indicator.
// Geometry: 30x17 track fully rounded, 13x13 knob, 2px insets.
Item {
    id: root

    property bool checked: false
    // Interaction flag (named to avoid shadowing Item.enabled, which still
    // applies on top): false dims the switch and kills interaction.
    property bool interactive: true
    signal toggled(bool checked)

    implicitWidth: 30
    implicitHeight: 17
    opacity: root.interactive && root.enabled ? 1 : 0.4
    activeFocusOnTab: root.interactive && root.enabled

    function toggle() {
        if (!root.interactive || !root.enabled)
            return
        root.checked = !root.checked
        root.toggled(root.checked)
    }

    Keys.onSpacePressed: root.toggle()
    Keys.onReturnPressed: root.toggle()

    Rectangle {
        id: focusRing
        anchors.fill: track
        anchors.margins: -4
        radius: height / 2
        color: "transparent"
        border.width: 2
        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g,
                              Theme.accent.b, 0.5)
        visible: root.activeFocus
    }

    Rectangle {
        id: track
        anchors.fill: parent
        radius: height / 2
        color: root.checked ? Theme.accent
            : (mouse.containsMouse ? Theme.mutedSurfaceHover : Theme.mutedSurface)
        Behavior on color { ColorAnimation { duration: 120 } }

        Rectangle {
            id: knob
            width: 13
            height: 13
            radius: 6.5
            y: 2
            x: root.checked ? track.width - width - 2 : 2
            color: root.checked ? Theme.accentInk : Theme.textMuted
            Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
            Behavior on color { ColorAnimation { duration: 120 } }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        anchors.margins: -6
        hoverEnabled: true
        enabled: root.interactive && root.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.forceActiveFocus()
            root.toggle()
        }
    }

    Accessible.role: Accessible.CheckBox
    Accessible.checkable: true
    Accessible.checked: root.checked
    Accessible.onToggleAction: root.toggle()
}
