import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Ui"
import "components"

// Application settings on the shared AppDialog shell: sections live in
// the left sidebar (a single "General" for now — more sections plug in
// by adding a sidebar item and a pane), the active pane on the right.
// Every row writes through the `Settings` context property immediately,
// so there is no draft to commit — the footer only closes the dialog.
AppDialog {
    id: settingsDialog

    function openDialog() {
        open()
    }

    property string activeSection: "general"

    title: qsTr("Settings")
    subtitle: qsTr("Adjust the sidebar, branch display and exit behavior.")
    dialogWidth: 680
    dialogHeight: 440
    contentPadding: 0

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---- sections sidebar (200px, padding 10) ----
        ColumnLayout {
            Layout.preferredWidth: 200
            Layout.fillHeight: true
            Layout.topMargin: 10
            Layout.bottomMargin: 10
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            spacing: 2

            AbstractButton {
                id: generalItem
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                readonly property bool active: settingsDialog.activeSection === "general"
                hoverEnabled: true
                onClicked: settingsDialog.activeSection = "general"
                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }
                contentItem: Label {
                    text: qsTr("General")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeLg
                    leftPadding: 10
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: Theme.radiusSm
                    color: generalItem.pressed ? Qt.rgba(Theme.accent.r, Theme.accent.g,
                                                         Theme.accent.b, 0.35)
                         : generalItem.active ? Qt.rgba(Theme.accent.r, Theme.accent.g,
                                                        Theme.accent.b, 0.25)
                         : generalItem.hovered ? Qt.rgba(Theme.accent.r, Theme.accent.g,
                                                         Theme.accent.b, 0.18)
                                               : "transparent"
                }
            }

            Item { Layout.fillHeight: true }
        }

        Rectangle {
            Layout.fillHeight: true
            implicitWidth: 1
            color: Theme.border
        }

        // ---- General pane (padding 18/2, rows carry their own 12px) ----
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 2
            Layout.bottomMargin: 2
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            spacing: 0
            visible: settingsDialog.activeSection === "general"

            // Row: show git branch in the sidebar
            RowLayout {
                spacing: 12
                Layout.fillWidth: true
                Layout.topMargin: 12
                Layout.bottomMargin: 12

                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Git branch")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeMd
                        font.weight: Font.Medium
                        Layout.fillWidth: true
                    }

                    Label {
                        text: qsTr("Show the branch name next to repository folders in the sidebar.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSm
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                UiSelectMenu {
                    id: gitBranchMenu
                    Layout.preferredWidth: 160
                    Layout.alignment: Qt.AlignVCenter
                    searchable: false
                    model: [
                        { key: "show", label: qsTr("Show") },
                        { key: "hide", label: qsTr("Hide") }
                    ]
                    currentIndex: Settings.showGitBranch ? 0 : 1
                    onActivated: function(index) {
                        Settings.setShowGitBranch(index === 0)
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
            }

            // Row: where branch tags appear in the sidebar
            RowLayout {
                spacing: 12
                Layout.fillWidth: true
                Layout.topMargin: 12
                Layout.bottomMargin: 12

                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Git branch scope")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeMd
                        font.weight: Font.Medium
                        Layout.fillWidth: true
                    }

                    Label {
                        text: qsTr("Show branch tags only on top-level folders or on every repository folder.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSm
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                UiSelectMenu {
                    id: gitBranchScopeMenu
                    Layout.preferredWidth: 160
                    Layout.alignment: Qt.AlignVCenter
                    searchable: false
                    model: [
                        { key: "top", label: qsTr("Top folders only") },
                        { key: "all", label: qsTr("All folders") }
                    ]
                    currentIndex: Settings.gitBranchTopOnly ? 0 : 1
                    onActivated: function(index) {
                        Settings.setGitBranchTopOnly(index === 0)
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
            }

            // Row: on-exit behavior for running commands
            RowLayout {
                spacing: 12
                Layout.fillWidth: true
                Layout.topMargin: 12
                Layout.bottomMargin: 12

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
                    currentIndex: {
                        var saved = Settings.get("onExitRunningCommands", "").toString()
                        return saved === "kill" ? 1 : saved === "leave" ? 2 : 0
                    }
                    onActivated: function(index) {
                        var key = exitBehavior.model[index].key
                        if (key === "")
                            Settings.remove("onExitRunningCommands")
                        else
                            Settings.set("onExitRunningCommands", key)
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }

    footerRight: [
        UiButton {
            label: qsTr("Done")
            color: "primary"
            variant: "solid"
            size: "sm"
            onClicked: settingsDialog.close()
        }
    ]
}
