import QtQuick 6.5
import QtQuick.Controls 6.5
import QtQuick.Layouts 1.5
import QtQuick.Dialogs

Rectangle {
    id: wizard
    width: parent.width
    height: parent.height
    color: "#0a0a0a"

    signal goToDashboard()
    
    Component.onCompleted: {
        console.log("[Wizard] Created, visible=" + visible)
    }

    // Connect log messages
    Connections {
        target: projectService

        function onLogMessage(level, target, message) {
            logModel.add(level, target, message)
        }
    }

    // Центрирование по вертикали и горизонтали
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 24
        width: parent.width

        // Title
        Label {
            text: "Welcome to trun"
            font.pixelSize: 24
            font.bold: true
            color: "white"
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        Label {
            text: "Select a folder to scan for projects. trun will detect package.json, Cargo.toml, go.mod, and more."
            font.pixelSize: 13
            color: "#888"
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true }

        // Choose Folder button
        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 280
            text: "Choose Folder"
            font.bold: true
            font.pixelSize: 14

            background: Rectangle {
                implicitWidth: 280
                implicitHeight: 44
                radius: 6
                color: "#3b82f6"
            }

            contentItem: Label {
                text: parent.text
                color: "white"
                font.pixelSize: 14
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: {
                console.log("=== Button clicked, opening folder picker ===")
                logModel.add("info", "wizard", "Opening folder picker...")
                folderPicker.open()
            }
        }

        FolderDialog {
            id: folderPicker
            title: "Select Folder to Scan"

            onAccepted: {
                console.log("=== Folder selected: " + selectedFolder + " ===")
                logModel.add("info", "folder", "Folder selected: " + selectedFolder)
                logModel.add("info", "scan", "Starting projectService.scanFolder()...")
                projectService.scanFolder(selectedFolder)
                
                // Small delay to let scan complete
                Qt.callLater(function() {
                    var count = projectService.projectCount()
                    console.log("=== Projects found: " + count + " ===")
                    logModel.add("info", "scan", "Scan complete: " + count + " projects found")
                    if (count > 0) {
                        logModel.add("info", "wizard", "Navigating to dashboard...")
                        wizard.goToDashboard()
                    } else {
                        logModel.add("warn", "scan", "No projects found in selected folder")
                    }
                })
            }

            onRejected: {
                console.log("=== Folder picker cancelled ===")
                logModel.add("info", "folder", "Folder picker cancelled")
            }
        }

        // Footer
        Label {
            text: "package.json · Cargo.toml · go.mod · Gemfile · pyproject.toml"
            color: "#444"
            font.pixelSize: 10
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            Layout.topMargin: 8
        }
    }
}
