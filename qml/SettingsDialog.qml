import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Application settings dialog styled after the Nuxt UI "Notifications"
// block: one rounded card, header, divided setting rows, footer actions.
// Backed by the `Settings` context property (key-value store).
Dialog {
    id: settingsDialog

    function openDialog() {
        var saved = Settings.get("onExitRunningCommands", "").toString()
        exitBehavior.currentIndex = saved === "kill" ? 1 : saved === "leave" ? 2 : 0
        open()
    }

    anchors.centerIn: parent
    width: parent ? Math.min(parent.width - 64, 440) : 440
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
        spacing: 0

        // Header
        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true
            Layout.margins: 20

            Label {
                text: qsTr("Settings")
                color: Theme.textPrimary
                font.bold: true
                font.pixelSize: Theme.fontSizeLg
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Application preferences.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeMd
                Layout.fillWidth: true
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.preferredHeight: 1
            color: Theme.border
        }

        // Row: on-exit behavior for running commands
        RowLayout {
            spacing: Theme.spacingLg
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.topMargin: Theme.spacingLg
            Layout.bottomMargin: Theme.spacingLg

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true

                Label {
                    text: qsTr("Running commands on exit")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeMd
                    font.weight: Font.Medium
                    Layout.fillWidth: true
                }

                Label {
                    text: qsTr("What to do when quitting while commands are still running.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSm
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            UiSelectMenu {
                id: exitBehavior
                Layout.preferredWidth: 160
                Layout.alignment: Qt.AlignVCenter
                searchable: false
                model: [
                    { key: "", label: qsTr("Ask") },
                    { key: "kill", label: qsTr("Kill commands") },
                    { key: "leave", label: qsTr("Leave running") }
                ]
                onActivated: function(index) {
                    var key = exitBehavior.model[index].key
                    if (key === "")
                        Settings.remove("onExitRunningCommands")
                    else
                        Settings.set("onExitRunningCommands", key)
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.preferredHeight: 1
            color: Theme.border
        }

        // Footer
        RowLayout {
            spacing: 8
            Layout.fillWidth: true
            Layout.margins: 20

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Close")
                Layout.preferredWidth: 110
                Layout.preferredHeight: 30
                font.pixelSize: Theme.fontSizeMd
                background: Rectangle {
                    radius: 6
                    color: "transparent"
                    border.color: Theme.border
                    border.width: 1
                }
                contentItem: Label {
                    text: parent.text
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeMd
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: settingsDialog.close()
            }
        }
    }
}
