import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1200
    height: 800
    minimumWidth: 900
    minimumHeight: 600
    // Shown deferred (native expanded-area setup first, iDescriptor order)
    visible: false
    title: "trun"
    // Native unified toolbar (iDescriptor recipe): Qt negotiates
    // full-size content with AppKit itself. No FramelessWindowHint,
    // no frame agent — both fought the native path.
    flags: Qt.Window | Qt.NoTitleBarBackgroundHint | Qt.ExpandedClientAreaHint
    // Transparent root: rounded backdrop below draws the window shape.
    color: "transparent"

    // Window backdrop: content tone, rounded unless maximized
    Rectangle {
        anchors.fill: parent
        radius: (Window.visibility === Window.Maximized
                 || Window.visibility === Window.FullScreen) ? 0 : 10
        color: Theme.windowBackground
    }

    // Drag strip (iDescriptor pattern): declared first (bottom of z),
    // buttons and rows above keep their clicks, empty areas drag.
    MouseArea {
        id: dragStrip
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 48
        acceptedButtons: Qt.LeftButton
        onPressed: function(mouse) { root.startSystemMove() }
        onDoubleClicked: {
            if (root.visibility === Window.Maximized)
                root.showNormal()
            else
                root.showMaximized()
        }
    }

    Component.onCompleted: {
        Qt.callLater(function() {
            root.visible = projectService.projectCount > 0
        })
    }

    property string activeView: "dashboard"

    // Full quit path (tray menu). With a tray, closing the window only hides it.
    function requestQuit() {
        if (commandExecutor.runningCount() === 0) {
            Qt.quit()
            return
        }
        var saved = Settings.has("onExitRunningCommands")
            ? Settings.get("onExitRunningCommands").toString() : ""
        if (saved === "kill") {
            Settings.remove("orphanedProcesses")
            commandExecutor.killAll()
            Qt.quit()
            return
        }
        if (saved === "leave") {
            Settings.set("orphanedProcesses", commandExecutor.runningProcesses())
            commandExecutor.detachAll()
            Qt.quit()
            return
        }
        root.visible = true
        quitDialog.pendingCount = commandExecutor.runningCount()
        quitDialog.rememberChoice = false
        quitDialog.open()
    }

    function quitKillRemembered() {
        if (quitDialog.rememberChoice)
            Settings.set("onExitRunningCommands", "kill")
        Settings.remove("orphanedProcesses")
        commandExecutor.killAll()
        Qt.quit()
    }

    function quitLeaveRemembered() {
        if (quitDialog.rememberChoice)
            Settings.set("onExitRunningCommands", "leave")
        Settings.set("orphanedProcesses", commandExecutor.runningProcesses())
        commandExecutor.detachAll()
        Qt.quit()
    }

    onClosing: function(close) {
        if (!trayAvailable)
            return // no tray: closing quits as before
        close.accepted = false
        root.visible = false
    }

    Dialog {
        id: quitDialog
        anchors.centerIn: parent
        width: 420
        modal: true
        padding: 0

        property int pendingCount: 0
        property alias rememberChoice: rememberBox.checked

        background: Rectangle {
            color: Theme.cardBackground
            border.color: Theme.border
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 12

            Label {
                text: "Commands still running"
                color: Theme.textPrimary
                font.bold: true
                font.pixelSize: 13
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.topMargin: 20
            }

            Label {
                text: "There " + (quitDialog.pendingCount === 1 ? "is 1 running command." : "are " + quitDialog.pendingCount + " running commands.")
                    + "\nKill them or leave them running in the background?"
                color: Theme.textMuted
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
            }

            RowLayout {
                spacing: 10
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20

                Rectangle {
                    id: rememberBox
                    property bool checked: false
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    Layout.alignment: Qt.AlignVCenter
                    radius: 4
                    color: "transparent"
                    border.color: rememberBox.checked ? Theme.accent : "#555"
                    border.width: 1

                    Label {
                        anchors.centerIn: parent
                        text: "✓"
                        color: Theme.accent
                        font.pixelSize: 12
                        visible: rememberBox.checked
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: rememberBox.checked = !rememberBox.checked
                    }
                }

                Label {
                    text: "Remember my choice"
                    color: Theme.textMuted
                    font.pixelSize: 12
                    Layout.alignment: Qt.AlignVCenter

                    MouseArea {
                        anchors.fill: parent
                        onClicked: rememberBox.checked = !rememberBox.checked
                    }
                }
            }

            RowLayout {
                spacing: 8
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.bottomMargin: 20

                Item { Layout.fillWidth: true }

                Button {
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
                        text: "Leave running"
                        color: Theme.textPrimary
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: root.quitLeaveRemembered()
                }

                Button {
                    Layout.preferredWidth: 110
                    Layout.preferredHeight: 30
                    font.pixelSize: 12

                    background: Rectangle {
                        radius: 6
                        color: "#dc2626"
                    }

                    contentItem: Label {
                        text: "Kill processes"
                        color: "white"
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: root.quitKillRemembered()
                }
            }
        }
    }

    Loader {
        id: wizardLoader
        active: projectService.projectCount === 0
        source: "WizardWindow.qml"

        onLoaded: {
            item.projectConfigured.connect(function() {
                wizardLoader.active = false
                root.visible = true
            })
        }
    }

    Sidebar {
        id: sidebar
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: 240

        // Real signal handler: the delegate emits sidebar.projectSelected,
        // and selecting another project must drop the open detail page.
        onProjectSelected: function(projectId) {
            logModel.add("info", "app", "Selecting project: " + projectId)
            dashboardView.closeDetail()
            root.activeView = "dashboard"
        }

        onMcpConfigureRequested: mcpSetupDialog.openDialog()
        onAddCustomRequested: function(folderPath) {
            newCommandDialog.openCreate(folderPath)
        }

        // Own traffic lights (untitled window has no native ones):
        // red hides to tray like closing, yellow minimizes, green maximizes.
        onCloseRequested: {
            if (!trayAvailable)
                Qt.quit()
            else
                root.visible = false
        }
        onMinimizeRequested: root.showMinimized()
        onMaximizeRequested: {
            if (root.visibility === Window.Maximized)
                root.showNormal()
            else
                root.showMaximized()
        }
    }

    McpSetupDialog {
        id: mcpSetupDialog
    }

    RunConfigDialog {
        id: newCommandDialog

        onCommandCreated: function(cfg) {
            var result = projectService.addCustomCommand(
                cfg.name, cfg.folderPath, cfg.executable, cfg.argsText,
                cfg.workdir, cfg.envText, cfg.allowMultiple, cfg.port || 0)
            if (result === "") {
                logModel.add("error", "custom", "Could not add command: fill in name and executable")
                return
            }
            var parts = result.split("|")
            logModel.add("info", "custom", "Added command: " + cfg.name)
            projectService.selectProject(parts[0])
            dashboardView.openDetail(parts[1])
        }
    }

    Item {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: sidebar.right
        anchors.right: parent.right

        // Soft divider shadow on the content's left edge
        Rectangle {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: 12
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.2) }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0) }
            }
        }

        Dashboard {
            id: dashboardView
            anchors.fill: parent
            visible: root.activeView === "dashboard"
        }

        // Any workspace change (rescan, new root) drops the open detail page:
        // its command may be gone or stale.
        Connections {
            target: projectService
            function onProjectsChanged() {
                dashboardView.closeDetail()
            }
        }
    }
}
