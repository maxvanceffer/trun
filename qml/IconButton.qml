import QtQuick
import QtQuick.Controls

// Ghost icon button (shadcn ghost style): transparent until hovered.
Button {
    id: iconButton

    property url iconSource
    property string tooltipText: ""
    // Toggle state (e.g. pinned): tinted like hover, off by default
    property bool active: false
    // Suppress the tooltip while e.g. a context menu is open
    property bool suppressTooltip: false

    implicitWidth: Theme.sidebarRowHeight
    implicitHeight: Theme.sidebarRowHeight
    padding: 6
    flat: true
    hoverEnabled: true
    opacity: enabled ? 1 : 0.4

    HoverHandler {
        id: hover
        enabled: iconButton.enabled
        cursorShape: Qt.PointingHandCursor
    }

    background: Rectangle {
        color: (hover.hovered || iconButton.hovered || iconButton.active)
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
            : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0)
        radius: Theme.radiusSm

        Behavior on color {
            ColorAnimation {
                duration: Theme.animHover
                easing.type: Theme.easingStandard
            }
        }
    }

    contentItem: Image {
        source: iconButton.iconSource
        sourceSize.width: 32
        sourceSize.height: 32
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    UiTooltip {
        text: iconButton.tooltipText
        visible: iconButton.hovered && iconButton.enabled && iconButton.visible
            && text !== "" && !iconButton.suppressTooltip
    }
}
