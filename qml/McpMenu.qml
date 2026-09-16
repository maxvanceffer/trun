import QtQuick
import QtQuick.Controls
import QtQuick.Effects

// Standalone MCP context menu: fixed size, theme colors, rounded,
// hairline border, soft shadow.
// (QtQuick.Controls Menu with a custom delegate collapses to ~0 width,
// so the menu is built from primitives instead.)
// Positioning and visibility are owned by the instantiator — this
// component only renders the card and emits signals.
Item {
    id: mcpMenu

    signal enableAllRequested()
    signal disableAllRequested()
    signal configureRequested()

    width: 180
    height: menuColumn.implicitHeight + 12

    Rectangle {
        id: menuCard
        anchors.fill: parent
        radius: Theme.radiusMd
        color: Theme.cardBackground
        border.color: Theme.border
        border.width: 1
    }

    MultiEffect {
        anchors.fill: menuCard
        source: menuCard
        shadowEnabled: true
        shadowColor: Qt.rgba(0, 0, 0, 0.5)
        shadowBlur: 1.0
        shadowVerticalOffset: 4
    }

    Column {
        id: menuColumn
        anchors.fill: parent
        anchors.margins: 6
        spacing: 2

        Rectangle {
            width: parent.width
            height: 32
            radius: Theme.radiusSm
            color: enableAllRow.hovered
                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0)

            Behavior on color {
                ColorAnimation {
                    duration: Theme.animHover
                    easing.type: Theme.easingStandard
                }
            }

            Label {
                anchors.fill: parent
                anchors.leftMargin: 10
                text: qsTr("Enable all")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeMd
                verticalAlignment: Text.AlignVCenter
            }

            MouseArea {
                id: enableAllRow
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: mcpMenu.enableAllRequested()
            }
        }

        Rectangle {
            width: parent.width
            height: 32
            radius: Theme.radiusSm
            color: disableAllRow.hovered
                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0)

            Behavior on color {
                ColorAnimation {
                    duration: Theme.animHover
                    easing.type: Theme.easingStandard
                }
            }

            Label {
                anchors.fill: parent
                anchors.leftMargin: 10
                text: qsTr("Disable all")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeMd
                verticalAlignment: Text.AlignVCenter
            }

            MouseArea {
                id: disableAllRow
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: mcpMenu.disableAllRequested()
            }
        }

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.border
        }

        Rectangle {
            width: parent.width
            height: 32
            radius: Theme.radiusSm
            color: configureRow.hovered
                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0)

            Behavior on color {
                ColorAnimation {
                    duration: Theme.animHover
                    easing.type: Theme.easingStandard
                }
            }

            Label {
                anchors.fill: parent
                anchors.leftMargin: 10
                text: qsTr("Configure…")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeMd
                verticalAlignment: Text.AlignVCenter
            }

            MouseArea {
                id: configureRow
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: mcpMenu.configureRequested()
            }
        }
    }
}
