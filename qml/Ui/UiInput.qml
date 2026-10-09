import QtQuick
import QtQuick.Controls
import "../"

// UiInput — text field styled after Nuxt UI Input (size md): bg-default,
// inset-ring, rounded-md, dimmed placeholder; the focus ring is painted in
// accent. Variants follow /tmp/nuxt-ui/src/theme/input.ts:
//   outline — default, the field look (bg-default + ring + hover nothing)
//   ghost   — bg-transparent, hover/focus bg-elevated + focus ring (an
//             editable-label affordance without a frame)
//   none    — fully bare, no bg, no ring (focus:outline-none)
// The px-2.5/py-1.5 paddings are specific to this component (there is no step
// for them in Theme).
TextField {
    id: root

    property string variant: "outline" // outline | ghost | none
    property string leadingIconSource: ""
    property string trailingIconSource: ""

    signal leadingClicked()
    signal trailingClicked()

    readonly property bool hasLeading: leadingIconSource !== ""
    readonly property bool hasTrailing: trailingIconSource !== ""

    implicitHeight: Theme.controlHeight
    font.pixelSize: Theme.fontSizeLg
    color: Theme.textPrimary
    placeholderTextColor: Theme.textMuted
    selectByMouse: true
    opacity: enabled ? 1 : 0.75

    leftPadding: root.hasLeading ? 28 : 10
    rightPadding: root.hasTrailing ? 28 : 10
    topPadding: 6
    bottomPadding: 6

    background: Item {
        // Focus ring: a barely visible outer ring (accent/25, matching
        // outline-{color}/25 in Nuxt), shown only while focused. The "none"
        // variant is fully bare (focus:outline-none in input.ts).
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: Theme.radiusSm + 3
            color: "transparent"
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g,
                                  Theme.accent.b, 0.25)
            border.width: 2
            visible: root.variant !== "none" && root.activeFocus
        }

        // Body: outline = the field look (bg-default + ring). ghost =
        // bg-transparent with hover/focus bg-elevated (Nuxt's
        // "bg-transparent hover:bg-elevated focus:bg-elevated").
        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusSm
            color: root.variant === "outline" ? Theme.windowBackground
                 : (root.variant === "ghost" && (root.hovered || root.activeFocus)
                     ? Theme.mutedSurface : "transparent")
            border.color: root.variant === "outline"
                ? (root.activeFocus ? Theme.accent : Theme.border)
                : "transparent"
            border.width: root.variant === "outline" ? 1 : 0
        }
    }

    Image {
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.iconSm
        height: Theme.iconSm
        visible: root.hasLeading
        source: root.leadingIconSource
        sourceSize.width: Theme.iconSm
        sourceSize.height: Theme.iconSm
        fillMode: Image.PreserveAspectFit

        MouseArea {
            anchors.fill: parent
            anchors.margins: -4
            cursorShape: Qt.PointingHandCursor
            onClicked: root.leadingClicked()
        }
    }

    Image {
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.iconSm
        height: Theme.iconSm
        visible: root.hasTrailing
        source: root.trailingIconSource
        sourceSize.width: Theme.iconSm
        sourceSize.height: Theme.iconSm
        fillMode: Image.PreserveAspectFit

        MouseArea {
            anchors.fill: parent
            anchors.margins: -4
            cursorShape: Qt.PointingHandCursor
            onClicked: root.trailingClicked()
        }
    }
}
