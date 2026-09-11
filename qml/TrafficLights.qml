import QtQuick
import QtQuick.Controls

// macOS traffic lights for the frameless window. Same style on all OSes.
Row {
    id: lights

    signal closeRequested()
    signal minimizeRequested()
    signal maximizeRequested()

    property bool windowActive: true

    spacing: 8

    Repeater {
        model: [
            { color: "#FF5F57", glyph: "×", action: "close" },
            { color: "#FEBF2E", glyph: "–", action: "minimize" },
            { color: "#28C840", glyph: "+", action: "maximize" }
        ]

        delegate: Rectangle {
            required property var modelData
            width: 12
            height: 12
            radius: 6
            color: lights.windowActive ? modelData.color : "#6D6D6D"

            Label {
                anchors.centerIn: parent
                text: modelData.glyph
                color: Qt.rgba(0, 0, 0, 0.55)
                font.pixelSize: 9
                font.bold: true
                visible: lightHover.containsMouse
            }

            MouseArea {
                id: lightHover
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (modelData.action === "close") lights.closeRequested()
                    else if (modelData.action === "minimize") lights.minimizeRequested()
                    else lights.maximizeRequested()
                }
            }
        }
    }
}
