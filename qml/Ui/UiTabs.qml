import QtQuick
import QtQuick.Controls
import "../"

// UiTabs — tab bar after Nuxt UI v4 Tabs (pill variant, size md, color
// neutral by default):
//   /tmp/nuxt-ui/src/runtime/components/Tabs.vue
//   /tmp/nuxt-ui/src/theme/tabs.ts
// A rounded list plate (bg-elevated, rounded-lg, p-1), an animated thumb
// behind the active trigger (the pill variant's indicator — 200ms
// translate/width) and triggers that shift text-muted → text-default on
// hover and to inverted ink while active. The optional per-item `badge`
// renders as UiTag — Nuxt's default Tabs badge is
// { size: 'sm', color: 'neutral', variant: 'outline' }.
//
// items: [{ label: string, badge?: int }]; selection is currentIndex,
// clicks emit activated(index). Arrow keys move the selection while the
// bar has focus. Plain Items + MouseArea — no TabBar, whose style
// paddings and Background made earlier attempts drift.
FocusScope {
    id: root

    property var items: []              // [{ label, badge? }]
    property int currentIndex: 0
    // pill supports primary | neutral: only the thumb fill and the active
    // label's inverted ink change (tabs.ts compoundVariants).
    property string color: "neutral"
    // Share the full width equally between triggers (wide bars).
    property bool stretch: false

    signal activated(int index)

    readonly property color thumbColor:
        root.color === "primary" ? Theme.accent : Theme.backgroundInverted
    readonly property color activeInk:
        root.color === "primary" ? Theme.accentForeground : Theme.textInverted

    activeFocusOnTab: true
    implicitWidth: plate.implicitWidth
    implicitHeight: plate.implicitHeight

    function select(i) {
        if (i < 0 || i >= root.items.length || i === root.currentIndex)
            return
        root.currentIndex = i
        root.activated(i)
    }

    Keys.onLeftPressed: root.select(root.currentIndex - 1)
    Keys.onRightPressed: root.select(root.currentIndex + 1)

    // tabs.ts pill: list p-1, trigger px-3 py-1.5 text-sm gap-1.5 rounded-md.
    // md text-sm lands at 32px with the kit's own scale (13px font).
    readonly property int pad: 4
    readonly property int triggerGap: 2
    readonly property int triggerH: 32
    readonly property int triggerPadX: 12

    Rectangle {
        id: plate
        anchors.fill: parent
        radius: Theme.radiusMd
        color: Theme.backgroundElevated
        implicitWidth: row.implicitWidth + root.pad * 2
        implicitHeight: root.triggerH + root.pad * 2
    }

    Rectangle {
        id: thumb
        property Item target:
            rep.count > root.currentIndex ? rep.itemAt(root.currentIndex) : null
        x: root.pad + (target ? target.x : 0)
        y: root.pad
        width: target ? target.width : 0
        height: root.triggerH
        radius: Theme.radiusSm
        color: root.thumbColor
        Behavior on x     { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
    }

    Row {
        id: row
        x: root.pad
        y: root.pad
        width: root.stretch ? parent.width - root.pad * 2 : implicitWidth
        height: root.triggerH
        spacing: root.triggerGap

        Repeater {
            id: rep
            model: root.items

            delegate: Item {
                id: trigger
                required property int index
                required property var modelData

                implicitWidth: triggerRow.implicitWidth + root.triggerPadX * 2
                width: root.stretch
                    ? Math.max(0, (row.width - root.triggerGap * (rep.count - 1))
                               / Math.max(1, rep.count))
                    : implicitWidth
                height: root.triggerH

                Row {
                    id: triggerRow
                    anchors.centerIn: parent
                    spacing: 6

                    Label {
                        text: trigger.modelData.label || ""
                        color: trigger.index === root.currentIndex
                            ? root.activeInk
                            : triggerMouse.containsMouse ? Theme.textPrimary
                                                         : Theme.textMuted
                        font.pixelSize: Theme.fontSizeLg
                        font.weight: Font.Medium
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }

                    UiTag {
                        visible: (trigger.modelData.badge || 0) > 0
                        label: String(trigger.modelData.badge || 0)
                        color: "neutral"
                        variant: "outline"
                        size: "sm"
                    }
                }

                MouseArea {
                    id: triggerMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.forceActiveFocus()
                        root.select(trigger.index)
                    }
                }
            }
        }
    }

    Rectangle {                              // focus ring
        anchors.fill: parent
        anchors.margins: -4
        radius: Theme.radiusSm + 4
        color: "transparent"
        border.width: 2
        border.color: Theme.ring
        visible: root.activeFocus
    }
}
