import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Capsule callout: themed icon + bold accent label + accent action button.
// Sizes itself to the content; collapses when messageText is empty.
// Emits actionTriggered() from both the button and the capsule body.
Item {
    id: alertRoot

    property string messageText: ""
    property string actionText: ""
    property url iconSource

    signal actionTriggered()

    implicitWidth: capsule.width
    implicitHeight: 44
    visible: messageText !== ""

    Rectangle {
        id: capsule
        anchors.centerIn: parent
        width: bannerRow.implicitWidth + 32
        height: 44
        radius: 22
        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.45)
        border.width: 1

        RowLayout {
            id: bannerRow
            anchors.centerIn: parent
            spacing: Theme.spacingSm

            Image {
                Layout.preferredWidth: Theme.iconSm
                Layout.preferredHeight: Theme.iconSm
                Layout.alignment: Qt.AlignVCenter
                source: alertRoot.iconSource
                sourceSize.width: 32
                sourceSize.height: 32
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Label {
                Layout.alignment: Qt.AlignVCenter
                text: alertRoot.messageText
                color: Theme.accent
                font.pixelSize: Theme.fontSizeMd
                font.bold: true
            }

            Button {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 110
                Layout.preferredHeight: 30
                text: alertRoot.actionText
                font.pixelSize: 12
                background: Rectangle {
                    radius: 6
                    color: Theme.accent
                }
                contentItem: Label {
                    text: parent.text
                    color: "white"
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: alertRoot.actionTriggered()
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: alertRoot.actionTriggered()
        }
    }
}
