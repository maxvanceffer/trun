import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

Rectangle {
    id: statCard

    property string title: ""
    property string value: "—"
    property color valueColor: Theme.textPrimary
    property string sub: ""
    // 0..1 progress ring, negative hides it
    property real ringFraction: -1
    property color ringColor: Theme.primary
    property bool showDot: false
    property color dotColor: "transparent"
    property bool dotPulsing: false

    onDotPulsingChanged: {
        if (!dotPulsing) {
            pulseAnim.stop()
            statusDot.opacity = 1
        }
    }

    color: Theme.cardBackground
    border.color: Theme.border
    border.width: 1
    radius: 8
    implicitWidth: 150
    implicitHeight: 110

    // Status dot overlay in the top-right corner
    Rectangle {
        id: statusDot
        anchors.top: parent.top
        anchors.topMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        width: 8
        height: 8
        visible: statCard.showDot
        radius: 4
        color: statCard.dotColor

        SequentialAnimation on opacity {
            id: pulseAnim
            running: statCard.showDot && statCard.dotPulsing
            loops: Animation.Infinite
            NumberAnimation { from: 1; to: 0.3; duration: 600; easing.type: Easing.InOutQuad }
            NumberAnimation { from: 0.3; to: 1; duration: 600; easing.type: Easing.InOutQuad }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4

        Label {
            text: statCard.title
            color: Theme.textMuted
            font.pixelSize: 10
            font.bold: true
            Layout.fillWidth: true
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        // Bottom block: value with description under it, ring right
        RowLayout {
            spacing: 8
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignBottom

                Label {
                    text: statCard.value
                    color: statCard.valueColor
                    font.pixelSize: 22
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Label {
                    text: statCard.sub
                    color: Theme.textMuted
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            ProgressRing {
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                Layout.alignment: Qt.AlignBottom
                visible: statCard.ringFraction >= 0
                fraction: statCard.ringFraction
                ringColor: statCard.ringColor
                text: Math.round(Math.min(Math.max(statCard.ringFraction, 0), 1) * 100) + "%"
            }
        }
    }
}
