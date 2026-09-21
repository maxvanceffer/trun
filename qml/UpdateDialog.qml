import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Software update dialog driven by the `updater` context object.
// Rendered inside the window via Dialog, never as a separate Window.
Dialog {
    id: updateDialog

    function openDialog() {
        updater.checkForUpdates()
        open()
    }

    anchors.centerIn: parent
    width: parent ? Math.min(parent.width - 64, 480) : 480
    height: parent ? Math.min(parent.height - 64, 460) : 460
    modal: true
    padding: 0

    background: Item {
        Rectangle {
            anchors.fill: parent
            radius: 8
            color: Qt.alpha(Theme.cardBackground, 0.96)
            border.color: Theme.border
            border.width: 1
        }
    }

    contentItem: ColumnLayout {
        spacing: 12

        Label {
            text: qsTr("Software Update")
            color: Theme.textPrimary
            font.bold: true
            font.pixelSize: 13
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.topMargin: 20
        }

        Label {
            text: qsTr("Current version: %1").arg(updater.currentVersion)
            color: Theme.textMuted
            font.pixelSize: 12
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
        }

        // Checking / idle
        Label {
            visible: updater.state === "checking" || updater.state === "idle"
            text: qsTr("Checking for updates…")
            color: Theme.textMuted
            font.pixelSize: 12
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
        }

        ProgressBar {
            visible: updater.state === "checking" || updater.state === "idle"
            from: 0
            to: 0
            value: 0
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
        }

        // Up to date
        Label {
            visible: updater.state === "upToDate"
            text: qsTr("You're up to date.")
            color: Theme.textPrimary
            font.pixelSize: 12
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
        }

        // New version available
        Label {
            visible: updater.state === "available"
            || updater.state === "downloading"
            || updater.state === "ready"
            text: qsTr("Version %1 is available.").arg(updater.latestVersion)
            color: Theme.textPrimary
            font.pixelSize: 12
            font.bold: true
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
        }

        ScrollView {
            visible: updater.state === "available" && updater.releaseNotes !== ""
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.minimumHeight: 80
            clip: true

            TextArea {
                text: updater.releaseNotes
                textFormat: Text.MarkdownText
                readOnly: true
                wrapMode: Text.Wrap
                color: Theme.textMuted
                font.pixelSize: 12
                background: Rectangle {
                    color: "transparent"
                }
            }
        }

        // Downloading
        Label {
            visible: updater.state === "downloading"
            text: qsTr("Downloading… %1%").arg(Math.round(updater.downloadProgress * 100))
            color: Theme.textMuted
            font.pixelSize: 12
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
        }

        ProgressBar {
            visible: updater.state === "downloading"
            from: 0
            to: 1
            value: updater.downloadProgress
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
        }

        // Ready to install
        Label {
            visible: updater.state === "ready" && updater.canInstall
            text: qsTr("The update is downloaded. Restart trun to install it.\nRunning commands will be left running and reattached.")
            color: Theme.textMuted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
        }

        Label {
            visible: updater.state === "ready" && !updater.canInstall
            text: qsTr("The update is downloaded. Replace the app bundle with the downloaded version, or grab the installer from the release page.")
            color: Theme.textMuted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
        }

        // Installing
        Label {
            visible: updater.state === "installing"
            text: qsTr("Installing… trun will restart automatically.")
            color: Theme.textMuted
            font.pixelSize: 12
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
        }

        // Error
        Label {
            visible: updater.state === "error"
            text: updater.errorString
            color: "#f87171"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            spacing: 8
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 20

            Item { Layout.fillWidth: true }

            Button {
                visible: updater.state === "downloading"
                text: qsTr("Cancel")
                Layout.preferredWidth: 110
                Layout.preferredHeight: 30
                font.pixelSize: 12
                background: Rectangle {
                    radius: 6
                    color: "transparent"
                    border.color: Theme.border
                    border.width: 1
                }
                contentItem: Label {
                    text: parent.text
                    color: Theme.textPrimary
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: updater.cancelDownload()
            }

            Button {
                visible: updater.state === "error"
                    || (updater.state === "available" && updater.downloadUrl === "")
                    || (updater.state === "ready" && !updater.canInstall)
                text: qsTr("Open release page")
                Layout.preferredWidth: 140
                Layout.preferredHeight: 30
                font.pixelSize: 12
                background: Rectangle {
                    radius: 6
                    color: "transparent"
                    border.color: Theme.border
                    border.width: 1
                }
                contentItem: Label {
                    text: parent.text
                    color: Theme.textPrimary
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: updater.openReleasesPage()
            }

            Button {
                visible: updater.state === "available" && updater.downloadUrl !== ""
                text: qsTr("Download update")
                Layout.preferredWidth: 140
                Layout.preferredHeight: 30
                font.pixelSize: 12
                background: Rectangle {
                    radius: 6
                    color: Theme.accent
                }
                contentItem: Label {
                    text: parent.text
                    color: Theme.accentForeground
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: updater.downloadUpdate()
            }

            Button {
                visible: updater.state === "ready" && updater.canInstall
                text: qsTr("Restart & install")
                Layout.preferredWidth: 140
                Layout.preferredHeight: 30
                font.pixelSize: 12
                background: Rectangle {
                    radius: 6
                    color: Theme.accent
                }
                contentItem: Label {
                    text: parent.text
                    color: Theme.accentForeground
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: updater.installAndRestart()
            }

            Button {
                visible: updater.state === "error"
                text: qsTr("Retry")
                Layout.preferredWidth: 110
                Layout.preferredHeight: 30
                font.pixelSize: 12
                background: Rectangle {
                    radius: 6
                    color: Theme.accent
                }
                contentItem: Label {
                    text: parent.text
                    color: Theme.accentForeground
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: updater.checkForUpdates()
            }

            Button {
                visible: updater.state !== "downloading" && updater.state !== "installing"
                text: qsTr("Close")
                Layout.preferredWidth: 110
                Layout.preferredHeight: 30
                font.pixelSize: 12
                background: Rectangle {
                    radius: 6
                    color: "transparent"
                    border.color: Theme.border
                    border.width: 1
                }
                contentItem: Label {
                    text: parent.text
                    color: Theme.textPrimary
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: updateDialog.close()
            }
        }
    }
}
