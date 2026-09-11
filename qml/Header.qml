import QtQuick 6.5
import QtQuick.Controls 6.5

Rectangle {
    id: header
    width: parent.width
    height: 48
    color: Theme.windowBackground
    border.color: "#222"
    border.width: 1

    Column {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 16
        spacing: 2

        Row {
            spacing: 8

            Label {
                text: projectService.activeProject.name || "Dashboard"
                color: Theme.textPrimary
                font.bold: true
                font.pixelSize: 13
            }

            Rectangle {
                width: 6
                height: 6
                radius: 3
                color: "#22c55e"
            }

            Label {
                text: "Live"
                color: "#22c55e"
                font.pixelSize: 10
            }
        }
    }
}
