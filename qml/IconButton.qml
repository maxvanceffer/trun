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

    onSuppressTooltipChanged: {
        if (suppressTooltip) {
            tooltip.visible = false
            tooltipDelay.stop()
        }
    }

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

    // Custom tooltip (native attached ToolTip cannot be themed):
    // themed card in the overlay layer, flips below when cramped above.
    property bool tooltipBelow: false

    Timer {
        id: tooltipDelay
        interval: 500
        onTriggered: {
            if (iconButton.suppressTooltip) return
            // Flip below when there is no room above (window coordinates)
            var p = iconButton.mapToItem(null, 0, 0)
            iconButton.tooltipBelow = p.y < 60
            tooltip.visible = true
        }
    }

    onHoveredChanged: {
        tooltip.visible = false
        if (hovered && iconButton.tooltipText !== "" && !suppressTooltip)
            tooltipDelay.restart()
        else
            tooltipDelay.stop()
    }

    ToolTip {
        id: tooltip
        text: iconButton.tooltipText
        visible: false
        y: iconButton.tooltipBelow ? iconButton.height + 6 : -height - 6
        x: (iconButton.width - width) / 2

        background: Rectangle {
            color: Theme.cardBackground
            border.color: Theme.border
            radius: Theme.radiusSm
        }

        contentItem: Label {
            text: tooltip.text
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeSm
            leftPadding: Theme.spacingSm
            rightPadding: Theme.spacingSm
            topPadding: Theme.spacingXs
            bottomPadding: Theme.spacingXs
        }
    }
}
