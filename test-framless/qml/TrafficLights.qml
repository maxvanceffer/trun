import QtQuick
import QtQuick.Controls

// macOS-style traffic lights for the custom (non-macOS) title bar.
// On macOS the native lights are used instead.
Row {
    id: lights

    signal closeRequested()
    signal minimizeRequested()
    signal maximizeRequested()

    property bool windowActive: true

    spacing: 8

    Repeater {
        model: [
            { color: "#ff5f57", glyph: "\u00d7", action: "close" },
            { color: "#febc2e", glyph: "\u2013", action: "minimize" },
            { color: "#28c840", glyph: "+", action: "maximize" }
        ]

        delegate: Rectangle {
            required property var modelData

            width: 12
            height: 12
            radius: 6
            color: lights.windowActive ? modelData.color : "#6d6d6d"

            Label {
                anchors.centerIn: parent
                text: modelData.glyph
                color: Qt.rgba(0, 0, 0, 0.55)
                font.pixelSize: 9
                font.bold: true
                visible: hover.containsMouse
            }

            MouseArea {
                id: hover
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                onClicked: {
                    if (modelData.action === "close")
                        lights.closeRequested()
                    else if (modelData.action === "minimize")
                        lights.minimizeRequested()
                    else
                        lights.maximizeRequested()
                }
            }
        }
    }
}
