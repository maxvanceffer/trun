import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// UiAlert — Nuxt UI Alert equivalent: a full-width callout with a leading
// icon, a title + description stack, trailing actions and an optional
// close button. Color mapping mirrors UiTag (primary|success|info|
// warning|error|neutral, variant solid|outline|soft|subtle); orientation
// horizontal keeps actions in the row, vertical stacks them underneath.
Item {
    id: root

    property string title: ""
    property string description: ""
    property url iconSource
    property string color: "primary"
    property string variant: "subtle"
    property string orientation: "horizontal"
    property string actionText: ""
    property bool closable: false

    signal actionTriggered()
    signal closeRequested()

    function colorFor(name) {
        switch (name) {
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
    readonly property color contentFill: {
        if (variant === "solid")
            return color === "neutral" ? Theme.backgroundInverted : tint
        if (variant === "subtle" || variant === "soft")
            return color === "neutral" ? Theme.backgroundElevated : Qt.rgba(tint.r, tint.g, tint.b, 0.1)
        return color === "neutral" ? Theme.backgroundDefault : "transparent"
    }
    readonly property color contentStroke: {
        if (variant === "outline")
            return color === "neutral" ? Theme.borderAccented : Qt.rgba(tint.r, tint.g, tint.b, 0.5)
        if (variant === "subtle")
            return color === "neutral" ? Theme.borderAccented : Qt.rgba(tint.r, tint.g, tint.b, 0.25)
        return "transparent"
    }
    readonly property color descriptionColor: variant === "solid"
        ? Qt.rgba(foreground.r, foreground.g, foreground.b, 0.85) : Theme.textMuted

Rectangle {
        id: bg
        anchors.fill: parent
        radius: Theme.radiusMd
        color: root.contentFill
        border.color: root.contentStroke
        border.width: (root.variant === "outline" || root.variant === "subtle") ? 1 : 0
    }

    visible: root.title !== "" || root.description !== ""
    implicitHeight: body.implicitHeight + 14

    ColumnLayout {
        id: body
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingMd
        anchors.rightMargin: Theme.spacingMd
        anchors.topMargin: 7
        anchors.bottomMargin: 7
        spacing: Theme.spacingSm

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd

            UiIcon {
                Layout.alignment: Qt.AlignVCenter
                visible: root.iconSource.toString() !== ""
                source: root.iconSource
                color: root.foreground
                size: Theme.iconMd
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    visible: root.title !== ""
                    text: root.title
                    color: root.foreground
                    font.pixelSize: Theme.fontSizeLg
                    font.bold: true
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.description !== ""
                    text: root.description
                    color: root.descriptionColor
                    font.pixelSize: Theme.fontSizeMd
                    wrapMode: Text.WordWrap
                }
            }

            // Horizontal orientation: action + close live in the row.
            Button {
                Layout.alignment: Qt.AlignVCenter
                visible: root.orientation === "horizontal" && root.actionText !== ""
                Layout.preferredWidth: 110
                Layout.preferredHeight: 30
                text: root.actionText
                font.pixelSize: Theme.fontSizeMd
                background: Rectangle {
                    radius: Theme.radiusSm
                    color: root.variant === "solid" && root.color === "neutral"
                        ? Theme.backgroundInverted : root.tint
                }
                contentItem: Label {
                    text: parent.text
                    color: root.color === "neutral" ? Theme.textInverted : Theme.primaryForeground
                    font.pixelSize: Theme.fontSizeMd
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.actionTriggered()
            }

            Button {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                visible: root.orientation === "horizontal" && root.closable
                text: "✕"
                font.pixelSize: Theme.fontSizeMd
                background: Rectangle {
                    radius: Theme.radiusSm
                    color: "transparent"
                }
                contentItem: Label {
                    text: parent.text
                    color: root.foreground
                    font.pixelSize: Theme.fontSizeMd
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.closeRequested()
            }
        }

        // Vertical orientation: actions stack underneath the text.
        RowLayout {
            Layout.fillWidth: true
            visible: root.orientation === "vertical" && (root.actionText !== "" || root.closable)
            spacing: Theme.spacingSm

            Item { Layout.fillWidth: true }

            Button {
                visible: root.actionText !== ""
                Layout.preferredWidth: 110
                Layout.preferredHeight: 30
                text: root.actionText
                font.pixelSize: Theme.fontSizeMd
                background: Rectangle {
                    radius: Theme.radiusSm
                    color: root.tint
                }
                contentItem: Label {
                    text: parent.text
                    color: root.color === "neutral" ? Theme.textInverted : Theme.primaryForeground
                    font.pixelSize: Theme.fontSizeMd
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.actionTriggered()
            }

            Button {
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                visible: root.closable
                text: "✕"
                font.pixelSize: Theme.fontSizeMd
                background: Rectangle {
                    radius: Theme.radiusSm
                    color: "transparent"
                }
                contentItem: Label {
                    text: parent.text
                    color: root.foreground
                    font.pixelSize: Theme.fontSizeMd
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.closeRequested()
            }
        }
    }
}
