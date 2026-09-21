import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

pragma ComponentBehavior: Bound

Control {
    id: root

    property string label: ""
    property string color: "primary"
    property string variant: "solid"
    property string size: "md"
    property bool square: false
    property url leadingIconSource
    property url trailingIconSource
    property bool leading: false
    property bool trailing: false
    property url iconSource
    property url avatarSource
    property string avatarFallback: ""
    property alias leadingContent: leadingSlot.sourceComponent
    property alias trailingContent: trailingSlot.sourceComponent

    // NOTE: url must be compared via toString() — strict !== against ""
    // does not coerce and is always true, even for an empty url.
    readonly property bool hasLabel: label !== ""
    readonly property url leadingSrc: leadingIconSource.toString() !== "" ? leadingIconSource
                                                              : ((leading || !trailing) && iconSource.toString() !== "" ? iconSource : "")
    readonly property url trailingSrc: trailingIconSource.toString() !== "" ? trailingIconSource
                                                                : (trailing && iconSource.toString() !== "" ? iconSource : "")
    readonly property bool showLeading: leadingSrc.toString() !== "" || avatarSource.toString() !== ""
    readonly property bool showTrailing: trailingSrc.toString() !== ""
    property bool labelTruncated: false
    readonly property bool showLabel: hasLabel || (!showLeading && !showTrailing)

    function colorFor(name) {
        switch (name) {
            case "secondary": return Theme.secondary
            case "success": return Theme.success
            case "info": return Theme.info
            case "warning": return Theme.warning
            case "error": return Theme.error
            case "neutral": return Theme.backgroundInverted
            default: return Theme.accent
        }
    }

    readonly property color tint: colorFor(root.color)
    readonly property color foreground: {
        if (variant === "solid")
            return color === "neutral" ? Theme.textInverted : Theme.primaryForeground
        return color === "neutral" ? Theme.textPrimary : tint
    }
    readonly property color fill: {
        if (variant === "solid")
            return color === "neutral" ? Theme.backgroundInverted : Theme.accent
        if (variant === "subtle" || variant === "soft")
            return color === "neutral" ? Theme.backgroundElevated : Qt.rgba(tint.r, tint.g, tint.b, 0.1)
        return color === "neutral" ? Theme.backgroundDefault : "transparent"
    }
    readonly property color stroke: {
        if (variant === "outline")
            return color === "neutral" ? Theme.borderAccented : Qt.rgba(tint.r, tint.g, tint.b, 0.5)
        if (variant === "subtle")
            return color === "neutral" ? Theme.borderAccented : Qt.rgba(tint.r, tint.g, tint.b, 0.25)
        return "transparent"
    }

    readonly property int fontPx: {switch (size) {
        case "xs": return 8
        case "sm": return 10
        case "lg": return 14
        case "xl": return 16
        default: return 12
    }}
    readonly property int lineHeightPx: {switch (size) {
        case "xs": return 12
        case "sm": return 12
        case "lg": return 20
        case "xl": return 24
        default: return 16
    }}
    readonly property int padX: {switch (size) {
        case "xs": return 4
        case "sm": return 6
        case "lg": return 8
        case "xl": return 10
        default: return 8
    }}
    readonly property int padY: {switch (size) {
        case "xs": return 2
        default: return 4
    }}
    readonly property int gapPx: {switch (size) {
        case "lg": case "xl": return 6
        default: return 4
    }}
    readonly property int radiusPx: {switch (size) {
        case "xs": return 2
        case "sm": return Theme.radiusSm
        default: return 6
    }}
    readonly property int iconPx: {switch (size) {
        case "lg": return 20
        case "xl": return 24
        default: return size === "md" ? 16 : 12
    }}
    readonly property int avatarPx: size === "lg" || size === "xl" ? 16 : 12

    leftPadding: root.padX
    rightPadding: root.padX
    topPadding: root.padY
    bottomPadding: root.padY
    implicitWidth: Math.max(implicitBackgroundWidth + leftPadding + rightPadding,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topPadding + bottomPadding,
                             implicitContentHeight + topPadding + bottomPadding)

    background: Rectangle {
        implicitWidth: root.square ? root.iconPx : 0
        implicitHeight: root.iconPx
        radius: root.square ? 0 : root.radiusPx
        color: root.fill
        border.color: root.stroke
        border.width: root.stroke !== "transparent" ? 1 : 0
    }

    contentItem: RowLayout {
        spacing: root.gapPx

        Loader {
            Layout.preferredWidth: active ? root.avatarPx : 0
            Layout.preferredHeight: active ? root.avatarPx : 0
            visible: active
            active: root.avatarSource.toString() !== ""
            sourceComponent: avatarComponent
        }

        Loader {
            id: leadingSlot
            Layout.preferredWidth: active ? root.iconPx : 0
            Layout.preferredHeight: active ? root.iconPx : 0
            Layout.alignment: Qt.AlignVCenter
            visible: active
            active: root.showLeading && root.avatarSource.toString() === ""
            sourceComponent: root.leadingSrc !== "" ? leadingComponent : undefined
        }

        Label {
            id: labelItem
            Layout.fillWidth: !root.square
            visible: root.showLabel
            text: root.label
            color: root.foreground
            font.pixelSize: root.fontPx
            font.weight: Font.Medium
            lineHeight: root.lineHeightPx
            lineHeightMode: Text.FixedHeight
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            onTruncatedChanged: root.labelTruncated = truncated
        }

        Loader {
            id: trailingSlot
            Layout.preferredWidth: active ? root.iconPx : 0
            Layout.preferredHeight: active ? root.iconPx : 0
            Layout.alignment: Qt.AlignVCenter
            visible: active
            active: root.showTrailing
            sourceComponent: root.trailingSrc !== "" ? trailingComponent : undefined
        }
    }

    Component {
        id: avatarComponent

        Item {
            clip: true
            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: root.color === "neutral"
                    ? Qt.rgba(Theme.textMuted.r, Theme.textMuted.g, Theme.textMuted.b, 0.25)
                    : Qt.rgba(root.tint.r, root.tint.g, root.tint.b, 0.2)
            }
            Text {
                anchors.centerIn: parent
                text: root.avatarFallback !== "" ? root.avatarFallback.charAt(0) : ""
                color: root.foreground
                font.pixelSize: 8
                font.bold: true
            }
        }
    }

    Component {
        id: leadingComponent

        UiIcon {
            source: root.leadingSrc
            color: root.foreground
            size: root.iconPx
        }
    }

    Component {
        id: trailingComponent

        UiIcon {
            source: root.trailingSrc
            color: root.foreground
            size: root.iconPx
        }
    }

    HoverHandler {
        id: hover
        enabled: root.enabled && root.hasLabel
    }

    UiTooltip {
        text: root.label
        visible: hover.hovered && root.labelTruncated && root.enabled && root.visible
    }
}
