import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

Window {
    id: wizardWindow
    flags: Qt.Dialog | Qt.WindowCloseButtonHint
    modality: Qt.ApplicationModal
    visible: true
    width: 600
    height: 500
    title: qsTr("Configure Project Folder")
    color: Theme.windowBackground

    signal projectConfigured(string folderPath)

    property url pendingFolder

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 24
        width: parent.width

        Label {
            text: qsTr("Welcome to trun")
            font.pixelSize: 24
            font.bold: true
            color: Theme.textPrimary
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Select a folder to scan for projects. trun will detect package.json, Cargo.toml, go.mod, and more.")
            font.pixelSize: 13
            color: Theme.textMuted
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        Item { Layout.fillHeight: true }

        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 280
            text: qsTr("Choose Folder")
            font.bold: true
            font.pixelSize: 14

            background: Rectangle {
                implicitWidth: 280
                implicitHeight: 44
                radius: 6
                color: parent.down ? Theme.ring : Theme.primary
            }

            contentItem: Label {
                text: parent.text
                color: Theme.primaryForeground
                font.pixelSize: 14
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: folderPicker.open()
        }

        FolderDialog {
            id: folderPicker
            title: qsTr("Select Folder to Scan")

            onAccepted: {
                wizardWindow.pendingFolder = selectedFolder
                projectService.scanFolder(selectedFolder.toString())
            }
        }

        Label {
            text: "package.json · Cargo.toml · go.mod · Gemfile · pyproject.toml"
            color: Theme.textMuted
            font.pixelSize: 10
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            Layout.topMargin: 8
        }
    }

    Connections {
        target: projectService

        function onScanComplete(count) {
            if (count > 0) {
                wizardWindow.projectConfigured(wizardWindow.pendingFolder.toString())
                wizardWindow.close()
            }
        }

        function onLogMessage(level, target, message) {
            logModel.add(level, target, message)
        }
    }
}
