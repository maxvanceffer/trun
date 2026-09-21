import QtQuick
import QtQuick.Controls.Basic

pragma ComponentBehavior: Bound

ToolTip {
    id: root

    property bool below: false

    visible: false
    delay: 500
    y: below && parent ? parent.height + 6 : -height - 6
    x: parent ? (parent.width - width) / 2 : 0

    onAboutToShow: {
        below = parent ? parent.mapToItem(null, 0, 0).y < height + 6 : false
    }

    background: Rectangle {
        color: Theme.cardBackground
        border.color: Theme.border
        radius: Theme.radiusSm
    }

    contentItem: Label {
        text: root.text
        color: Theme.textPrimary
        font.pixelSize: Theme.fontSizeSm
        leftPadding: Theme.spacingSm
        rightPadding: Theme.spacingSm
        topPadding: Theme.spacingXs
        bottomPadding: Theme.spacingXs
    }
}
