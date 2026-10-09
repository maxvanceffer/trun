import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../"

pragma ComponentBehavior: Bound

// UiButton — one-to-one port of Nuxt UI v4 Button under this config's Carbon theme.
//
// Spec: /tmp/nuxt-ui/src/theme/button.ts (slots, sizes, compoundVariants)
//       /tmp/nuxt-ui/src/runtime/components/Button.vue (props, icon slots)
// Geometry cross-checked against this config's compiled Tailwind CSS:
//   --spacing .25rem, --ui-radius .5rem -> .rounded-md = 12px
//   text-xs 12/16, text-sm 14/20, text-base 16/24, font-medium = 500
//   base     rounded-md font-medium inline-flex items-center transition-colors
//   disabled opacity .75 + cursor not-allowed        focus  outline 3px
//
// props:  color    primary | secondary | neutral | success | info | warning | error
//         variant  solid | outline | soft | subtle | ghost | link
//         size     xs | sm | md | lg | xl
//         block    full width, content centered  (w-full justify-center)
//         square   equal padding on all sides — implied when there is no label
//         loading  leading icon becomes a spinner and the button is disabled
Control {
    id: root

    property string label: ""
    property string color: "primary"
    property string variant: "solid"
    property string size: "md"
    property url leadingIconSource
    property url trailingIconSource
    property bool loading: false
    // Nuxt's `trailing`: the icon (and the loading spinner, see below) sits
    // on the right side. `loading` alone puts the spinner on the left.
    property bool trailing: false
    property bool block: false
    property bool square: false

    signal clicked()

    readonly property bool hasLabel: label !== ""
    readonly property bool hasLeading: leadingIconSource.toString() !== ""
    readonly property bool hasTrailing: trailingIconSource.toString() !== ""

    // Button.vue + useComponentIcons: `loading` replaces the leading icon
    // with a spinner (left); `trailing` moves it to the right side. A
    // loading button is disabled (button.ts: disabled: props.disabled ||
    // isLoading) — offering a cancel is the caller's job.
    readonly property bool loadingLeft: root.loading && !root.trailing
    readonly property bool loadingRight: root.loading && root.trailing

    // button.ts: square: props.square || (!slots.default && !props.label)
    readonly property bool isSquare: square || !hasLabel
    // Button.vue: disabled: props.disabled || isLoading
    readonly property bool isEnabled: enabled && !loading
    // every hover: and active: class in button.ts carries the same value
    readonly property bool isHot: (hoverHandler.hovered || pressHandler.pressed) && root.isEnabled

    // size -> [padX, padY, padSquare, fontPx, linePx, gapPx, iconPx]
    readonly property var metrics: ({
        xs: [8, 4, 4, 12, 16, 4, 16],
        sm: [10, 6, 6, 12, 16, 6, 16],
        md: [10, 6, 6, 14, 20, 6, 20],
        lg: [12, 8, 8, 14, 20, 8, 20],
        xl: [12, 8, 8, 16, 24, 8, 24]
    })[size] || [10, 6, 6, 14, 20, 6, 20] // unknown size falls back to md

    readonly property real padX: isSquare ? metrics[2] : metrics[0]
    readonly property real padY: isSquare ? metrics[2] : metrics[1]
    readonly property int fontPx: metrics[3]
    readonly property int linePx: metrics[4]
    readonly property real gapPx: metrics[5]
    readonly property int iconPx: metrics[6]

    implicitWidth: leftPadding + rightPadding + contentItem.implicitWidth
    // Tailwind line-height, not font metrics: text-xs 16 / text-sm 20 / text-base 24
    implicitHeight: topPadding + bottomPadding + linePx
    leftPadding: padX
    rightPadding: padX
    topPadding: padY
    bottomPadding: padY
    opacity: isEnabled ? 1 : 0.75 // disabled:opacity-75
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Layout.fillWidth: block // block: w-full

    // bg-<color> behind the solid variant, and the tinted hover fills.
    function fillFor(name) {
        switch (name) {
        case "secondary":
            return Theme.secondary
        case "neutral":
            return Theme.backgroundInverted // bg-inverted
        case "error":
            return Theme.error
        case "success":
            return Theme.success
        case "warning":
            return Theme.warning
        case "info":
            return Theme.info
        default:
            return Theme.primary
        }
    }

    // `text-inverted bg-<color>` — the label on a solid fill. Nuxt's
    // text-inverted is white in light mode (1.83–3.65:1, all fail AA), so
    // every non-neutral accent takes the shared carbon ink instead; only
    // neutral, whose fill is bg-inverted, keeps text-inverted.
    function solidForegroundFor(name) {
        if (name === "neutral")
            return Theme.textInverted
        return Theme.accentForeground
    }

    // text-<color> — the label of outline / soft / subtle / ghost / link.
    function accentFor(name) {
        switch (name) {
        case "secondary":
            // Nuxt resolves this to the yellow fill itself (1.85:1 on the light
            // page); the app's secondaryForeground token exists to keep this
            // label readable, and is used nowhere else.
            return Theme.secondaryForeground
        case "neutral":
            return Theme.textPrimary // text-default
        case "error":
            return Theme.error
        case "success":
            return Theme.success
        case "warning":
            return Theme.warning
        case "info":
            return Theme.info
        default:
            return Theme.primary
        }
    }

    // color-mix(in oklab, <color> N%, transparent) — Tailwind's /N alpha syntax.
    function withAlpha(c, a) {
        return Qt.rgba(c.r, c.g, c.b, a)
    }

    readonly property color tint: fillFor(color)
    readonly property color accent: accentFor(color)

    readonly property color fill: {
        const neutral = color === "neutral"
        switch (variant) {
        case "solid":
            // solid: bg-<color>  |  hover/active:bg-<color>/75  (neutral: /90)
            if (neutral)
                return isHot ? withAlpha(Theme.backgroundInverted, 0.9) : Theme.backgroundInverted
            return isHot ? withAlpha(tint, 0.75) : tint
        case "outline":
            if (neutral)
                return isHot ? Theme.backgroundElevated : Theme.backgroundDefault // hover:bg-elevated
            return isHot ? withAlpha(tint, 0.1) : "transparent" // hover:bg-<c>/10
        case "soft":
        case "subtle":
            if (neutral)
                return isHot ? withAlpha(Theme.mutedSurfaceHover, 0.75) : Theme.backgroundElevated
            return isHot ? withAlpha(tint, 0.15) : withAlpha(tint, 0.1)
        case "ghost":
            if (neutral)
                return isHot ? Theme.backgroundElevated : "transparent"
            return isHot ? withAlpha(tint, 0.1) : "transparent"
        default:
            return "transparent" // link
        }
    }

    // ring ring-inset ring-<c>/50 (outline) / ring-<c>/25 (subtle);
    // neutral uses ring-accented, promoted to ring-inverted while focused.
    readonly property color stroke: {
        if (variant !== "outline" && variant !== "subtle")
            return "transparent"
        if (color === "neutral")
            return visualFocus ? Theme.backgroundInverted : Theme.borderAccented
        const strength = variant === "outline" ? 0.5 : 0.25
        return visualFocus ? tint : withAlpha(tint, strength)
    }

    // focus-visible:outline-3, tinted outline-<color>/25 (outline-inverted/25 for neutral)
    readonly property color focusRing: {
        const base = color === "neutral" ? Theme.backgroundInverted : tint
        return withAlpha(base, 0.25)
    }

    readonly property color foreground: {
        if (variant === "solid")
            return solidForegroundFor(color)
        if (variant === "link") {
            if (color === "neutral") // text-muted, hover:text-default
                return isHot ? Theme.textPrimary : Theme.textMuted
            return isHot ? withAlpha(accent, 0.75) : accent
        }
        return accent
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: root.fill
        border.width: root.stroke !== "transparent" ? 1 : 0
        border.color: root.stroke
        Behavior on color {
            ColorAnimation {
                duration: Theme.animHover
                easing.type: Theme.easingStandard
            }
        }

        // CSS outlines sit outside the border box: grow by 3px on every side.
        Rectangle {
            visible: root.visualFocus
            anchors.fill: parent
            anchors.margins: -3
            radius: Theme.radiusMd + 3
            color: "transparent"
            border.width: 3
            border.color: root.focusRing
        }
    }

    contentItem: RowLayout {
        spacing: root.gapPx

        // block: justify-center (the trailing icon keeps an auto margin instead)
        Item {
            visible: root.block && !root.hasTrailing
            Layout.fillWidth: true
        }

        // Leading spinner slot (Nuxt `loading` default side: replaces the
        // leading icon).
        Loader {
            active: root.loadingLeft
            visible: active
            Layout.preferredWidth: active ? root.iconPx : 0
            Layout.preferredHeight: active ? root.iconPx : 0
            Layout.alignment: Qt.AlignVCenter
            sourceComponent: Spinner {
                width: root.iconPx
                height: root.iconPx
                color: root.foreground
            }
        }

        Image {
            visible: root.hasLeading && !root.loadingLeft
            Layout.preferredWidth: visible ? root.iconPx : 0
            Layout.preferredHeight: root.iconPx
            Layout.alignment: Qt.AlignVCenter
            source: root.leadingIconSource
            sourceSize.width: root.iconPx * 2
            sourceSize.height: root.iconPx * 2
            fillMode: Image.PreserveAspectFit
        }

        // label: truncate + font-medium. Kept visible when empty — Nuxt still
        // renders the <span>, which is what gives a labelless button its height.
        Label {
            Layout.fillWidth: !root.block
            text: root.label
            color: root.foreground
            font.pixelSize: root.fontPx
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Item {
            visible: root.block && root.hasTrailing
            Layout.fillWidth: true // trailingIcon: ms-auto
        }

        // Trailing side: the icon, or the loading spinner when `trailing`
        // moved it here (Nuxt useComponentIcons).
        Loader {
            active: root.loadingRight
            visible: active
            Layout.preferredWidth: active ? root.iconPx : 0
            Layout.preferredHeight: active ? root.iconPx : 0
            Layout.alignment: Qt.AlignVCenter
            sourceComponent: Spinner {
                width: root.iconPx
                height: root.iconPx
                color: root.foreground
            }
        }

        Image {
            visible: root.hasTrailing && !root.loadingRight
            Layout.preferredWidth: visible ? root.iconPx : 0
            Layout.preferredHeight: root.iconPx
            Layout.alignment: Qt.AlignVCenter
            source: root.trailingIconSource
            sourceSize.width: root.iconPx * 2
            sourceSize.height: root.iconPx * 2
            fillMode: Image.PreserveAspectFit
        }

        Item {
            visible: root.block && !root.hasTrailing
            Layout.fillWidth: true
        }
    }

    HoverHandler {
        id: hoverHandler
        cursorShape: root.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
    }

    TapHandler {
        id: pressHandler
        enabled: root.isEnabled
        onTapped: root.clicked()
    }
}
