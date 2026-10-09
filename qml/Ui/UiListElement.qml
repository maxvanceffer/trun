import QtQuick
import "../"

pragma ComponentBehavior: Bound

// UiListElement — the one "list element" card every list renders a row into
// (Docker containers/images, Databases services, API saved requests/history).
//
// It is chrome only: rounded surface, border, hover ring, optional click. The
// page owns the content and its insets — drop anything into the default slot.
//
// Nuxt UI grounding:
//   surface — Card's `soft` variant (src/runtime/components/Card.vue,
//   src/theme/card.ts: bg-elevated/50) → Theme.cardBackground.
//   Ring — the project's existing list-card hover (DatabasesPage serviceCard):
//   border → Theme.ring over Theme.animHover. Nuxt has no hover-ring variant
//   on Card; this is the app convention layered on the same surface.
//
// props:  interactive  hover ring + pointer (default true)
//         clickable    emit clicked() on tap (default true)
// signal: clicked()
// default property content — children are laid out inside the card.
Rectangle {
    id: root
    objectName: "uiListElement"

    property bool interactive: true
    property bool clickable: true
    readonly property bool hovered: hoverHandler.hovered

    signal clicked()

    default property alias content: contentHost.data

    radius: Theme.radiusMd
    color: Theme.cardBackground
    border.width: 1
    border.color: (root.interactive && root.hovered) ? Theme.ring : Theme.border
    Behavior on border.color {
        ColorAnimation { duration: Theme.animHover; easing.type: Theme.easingStandard }
    }

    HoverHandler {
        id: hoverHandler
        enabled: root.interactive
        cursorShape: root.clickable ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    TapHandler {
        id: tapHandler
        enabled: root.clickable
        onTapped: root.clicked()
    }

    // Content host: fills the card. Kept last so children declared by the page
    // sit above the handlers and child controls receive clicks first.
    Item {
        id: contentHost
        anchors.fill: parent
    }
}
