import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QWindowKit

Window {
    id: wizardWindow
    flags: Qt.Dialog | Qt.WindowCloseButtonHint
    modality: Qt.ApplicationModal
    visible: true
    width: 600
    height: 500
    // Fixed size: the artwork and the absolutely positioned captions
    // are laid out for exactly 600x500, resizing only misaligns them.
    minimumWidth: 600
    maximumWidth: 600
    minimumHeight: 500
    maximumHeight: 500
    title: qsTr("Configure Project Folder")
    // Plate ground fallback; the artwork covers the window fully.
    color: "#1d1d16"

    signal projectConfigured(string folderPath)

    property url pendingFolder

    WindowAgent {
        id: wizardAgent
    }

    // Onboarding artwork (dark plate + dot grid + step pills + footer rule).
    // All captions below are live QML text, positioned per the spec sheet.
    // Colors are pinned to the dark-theme values: the plate is dark-only,
    // so Theme-aware colors would go unreadable in light mode.
    Image {
        anchors.fill: parent
        source: "qrc:/assets/wizard/wizard-bg.png"
        fillMode: Image.Stretch
    }

    Item {
        id: wizardTitleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 40
    }

    Component.onCompleted: {
        wizardAgent.setup(wizardWindow)
        wizardAgent.setTitleBar(wizardTitleBar)
    }

    // App mark: 280, 60, 40x40.
    Image {
        x: 280
        y: 60
        width: 40
        height: 40
        source: iconBaseUrl + "app-icon-1024.png"
        sourceSize.width: 80
        sourceSize.height: 80
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    Label {
        x: 60
        y: 104
        width: 480
        height: 40
        text: qsTr("Welcome to trun")
        font.pixelSize: 24
        font.bold: true
        color: "#fbfbf9" // Theme.textPrimary (dark)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Label {
        x: 100
        y: 150
        width: 400
        text: qsTr("Select a folder to scan. trun finds every project and its run scripts — ready to launch from the menu bar.")
        font.pixelSize: 13
        color: "#abab9c" // Theme.textMuted (dark)
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }

    // Step labels, centered on the plate pills (y 213, h 38).
    // Pills: x 24 w 150 · x 184 w 196 · x 390 w 186.
    Label {
        x: 24
        y: 213
        width: 150
        height: 38
        textFormat: Text.StyledText
        text: "<font color=\"#f0b100\"><b>1</b></font> <font color=\"#abab9c\">Choose a code folder</font>"
        font.pixelSize: 11
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Label {
        x: 184
        y: 213
        width: 196
        height: 38
        textFormat: Text.StyledText
        text: "<font color=\"#f0b100\"><b>2</b></font> <font color=\"#abab9c\">trun finds projects + commands</font>"
        font.pixelSize: 11
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Label {
        x: 390
        y: 213
        width: 186
        height: 38
        textFormat: Text.StyledText
        text: "<font color=\"#f0b100\"><b>3</b></font> <font color=\"#abab9c\">Run anything from the menu bar</font>"
        font.pixelSize: 11
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Button {
        x: 160
        y: 330
        width: 280
        height: 44
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
            var folder = selectedFolder.toString()
            // Already in the workspace: nothing to add, just close.
            if (projectService.isFolderKnown(folder)) {
                wizardWindow.close()
                return
            }
            wizardWindow.pendingFolder = selectedFolder
            projectService.scanFolder(folder)
        }
    }

    Label {
        x: 60
        y: 450
        width: 480
        text: "package.json · Cargo.toml · go.mod · Gemfile · pyproject.toml"
        color: "#8a8a7c"
        font.pixelSize: 10
        font.family: "Menlo"
        horizontalAlignment: Text.AlignHCenter
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
