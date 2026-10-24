import QtQuick
import QtQuick.Controls
import "../"

// UiTabs — tab bar after Nuxt UI v4 Tabs, `variant: link` (underline), md:
//   /tmp/nuxt-ui/src/theme/tabs.ts   (v4.11.3)
//   src/runtime/components/Tabs.vue
// Words only — no pill plate, no thumb behind the label. A hairline track
// (border-default, `list: 'border-b -mb-px'`) runs the full width of the
// bar, and the active trigger is marked by a solid bar riding that track:
// it animates position and width (Nuxt `indicator` +
// `transition-[translate,width]`, eased with the kit's motion tokens).
// The active label takes the same ink as the bar (link + color:
// `data-[state=active]:text-<color>`); idle labels go text-muted and
// brighten to text-default on hover. The per-item `badge` renders as UiTag
// (Nuxt's default Tabs badge is
// { size: 'sm', color: 'neutral', variant: 'outline' }).
//
// The right edge is a slot — Nuxt's `list-trailing` slot: anything declared
// as a child of UiTabs is laid out in a row pinned to the right, always
// space-between with the tabs. The request page drops the Body/Auth mode
// switches there.
//
// items: [{ label: string, badge?: int }]; selection is currentIndex,
// clicks emit activated(index). Arrow keys move the selection while the
// bar has focus. Plain Items + MouseArea — no TabBar, whose style
// paddings and Background made earlier attempts drift.
FocusScope {
    id: root

    property var items: []              // [{ label, badge? }]
    property int currentIndex: 0
    // link supports primary | neutral: only the marker's fill and the
    // active label's ink change (tabs.ts compoundVariants).
    property string color: "primary"
    // Right-hand slot (Nuxt `list-trailing`): child items land in a row
    // pinned to the right edge of the bar.
    default property alias trailingData: trailingRow.data

    signal activated(int index)

    readonly property color markerColor:
        root.color === "primary" ? Theme.primary : Theme.backgroundInverted
    readonly property color activeInk:
        root.color === "primary" ? Theme.primary : Theme.textPrimary

    // Geometry. The trigger band sits on a 1px track; the marker is a
    // 2px bar centred on that track (link's `-bottom-px h-px`, thickened).
    readonly property int triggerGap: 24
    readonly property int triggerH: 34
    readonly property int trackH: 1
    readonly property int markerH: 2

    activeFocusOnTab: true
    implicitWidth: triggersRow.implicitWidth
                   + (trailingRow.implicitWidth > 0
                      ? root.triggerGap * 2 + trailingRow.implicitWidth : 0)
    implicitHeight: root.triggerH + root.trackH

    function select(i) {
        if (i < 0 || i >= root.items.length || i === root.currentIndex)
            return
        root.currentIndex = i
        root.activated(i)
    }

    Keys.onLeftPressed: root.select(root.currentIndex - 1)
    Keys.onRightPressed: root.select(root.currentIndex + 1)

    // Triggers, in words. No horizontal padding of their own: the marker
    // is exactly as wide as the tab's content, and the first label lines
    // up with whatever the host insets the bar by.
    Row {
        id: triggersRow
        x: 0
        y: 0
        height: root.triggerH
        spacing: root.triggerGap

        Repeater {
            id: rep
            model: root.items

            delegate: Item {
                id: trigger
                required property int index
                required property var modelData

                implicitWidth: triggerRow.implicitWidth
                width: implicitWidth
                height: root.triggerH

                Row {
                    id: triggerRow
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6

                    Label {
                        text: trigger.modelData.label || ""
                        color: trigger.index === root.currentIndex
                            ? root.activeInk
                            : triggerMouse.containsMouse ? Theme.textPrimary
                                                         : Theme.textMuted
                        font.pixelSize: Theme.fontSizeLg
                        font.weight: Font.Medium
                        Behavior on color {
                            ColorAnimation { duration: Theme.animFast }
                        }
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

    // Trailing slot: pinned right, centred in the trigger band. The gap to
    // the last tab is whatever the bar's width leaves — space-between.
    Row {
        id: trailingRow
        anchors.right: parent.right
        anchors.verticalCenter: triggersRow.verticalCenter
        spacing: Theme.spacingSm
    }

    Rectangle {                              // hairline track, full width
        id: track
        x: 0
        y: root.triggerH
        width: root.width
        height: root.trackH
        color: Theme.border
    }

    Rectangle {                              // active marker, on the track
        id: marker
        readonly property Item target:
            rep.count > root.currentIndex ? rep.itemAt(root.currentIndex) : null
        x: target ? target.x : 0
        width: target ? target.width : 0
        y: root.triggerH - (root.markerH - root.trackH)
        height: root.markerH
        radius: height / 2
        color: root.markerColor
        visible: !!target
        Behavior on x {
            NumberAnimation { duration: Theme.animMedium; easing.type: Theme.easingStandard }
        }
        Behavior on width {
            NumberAnimation { duration: Theme.animMedium; easing.type: Theme.easingStandard }
        }
    }

    Rectangle {                              // focus ring, around the active trigger
        id: focusRing
        readonly property Item target: marker.target
        x: target ? target.x - 6 : 0
        width: target ? target.width + 12 : 0
        y: -4
        height: root.triggerH + 8
        radius: Theme.radiusSm
        color: "transparent"
        border.width: 2
        border.color: Theme.ring
        visible: root.activeFocus && !!target
        Behavior on x {
            NumberAnimation { duration: Theme.animMedium; easing.type: Theme.easingStandard }
        }
        Behavior on width {
            NumberAnimation { duration: Theme.animMedium; easing.type: Theme.easingStandard }
        }
    }
}
