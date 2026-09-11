import QtQuick 6.5
import QtQuick.Controls 6.5
import QtQuick.Layouts 1.5

Rectangle {
    id: dashboard
    width: parent.width
    height: parent.height
    color: Theme.windowBackground

    // Console shows output of the selected command only.
    // Histories are keyed by project-scoped run key "<projectId>|<cmdId>":
    // bare command ids (npm:serve) repeat across projects.
    property string selectedCommandId: ""
    property var commandHistories: ({})
    // Runtime-detected ports per run key (sniffed from output URLs)
    property var detectedPorts: ({})

    // Command shown on the detail page; empty means the grid is visible
    property string detailCommandId: ""

    // Bare descriptor id -> full run key. Executor callbacks already
    // carry full keys and pass through unchanged.
    function fullKey(cmdId) {
        if (cmdId === "") return ""
        if (cmdId.indexOf("|") >= 0) return cmdId
        return (projectService.activeProject.id || "") + "|" + cmdId
    }

    // Controls inside the agent title-bar strip that must stay clickable.
    // Null-guarded: qwindowkit ASSERT-aborts the whole app on null.
    function markHitTest(agent) {
        if (detailPage.backButton)
            agent.setHitTestVisible(detailPage.backButton, true)
        else
            console.warn("markHitTest: backButton is null")
        if (detailPage.projectLinkArea)
            agent.setHitTestVisible(detailPage.projectLinkArea, true)
        else
            console.warn("markHitTest: projectLinkArea is null")
    }

    function appendCommandLine(cmdId, level, target, message) {
        cmdId = fullKey(cmdId)
        var histories = commandHistories
        var h = histories[cmdId] || []
        h.push({level: level, target: target, message: message})
        while (h.length > 1000) h.shift()
        histories[cmdId] = h
        commandHistories = histories
        // Sniff framework URLs ("Local: http://localhost:5173/") for the port
        var m = /https?:\/\/(?:localhost|127\.0\.0\.1|\[::1\]):(\d+)/i.exec(message)
        if (m) {
            var ports = detectedPorts
            ports[cmdId] = parseInt(m[1], 10)
            detectedPorts = ports
        }
        if (cmdId !== "" && cmdId === selectedCommandId)
            commandLog.add(level, target, message)
    }

    function selectCommand(cmdId) {
        if (cmdId === "") {
            selectedCommandId = ""
            commandLog.clear()
            return
        }
        selectedCommandId = fullKey(cmdId)
        commandLog.clear()
        var h = commandHistories[selectedCommandId] || []
        for (var i = 0; i < h.length; ++i)
            commandLog.add(h[i].level, h[i].target, h[i].message)
    }

    function clearSelectedCommand() {
        if (selectedCommandId === "") return
        var histories = commandHistories
        delete histories[selectedCommandId]
        commandHistories = histories
        commandLog.clear()
    }

    function openDetail(cmdId) {
        if (cmdId === "") return
        slideOut.stop()
        detailCommandId = fullKey(cmdId)
        selectCommand(cmdId)
        if (!detailPage.visible) {
            detailPage.visible = true
            detailPage.x = detailPage.width
        }
        slideIn.from = detailPage.x
        slideIn.to = 0
        slideIn.start()
    }

    function closeDetail() {
        if (!detailPage.visible || slideOut.running) return
        slideIn.stop()
        slideOut.from = detailPage.x
        slideOut.to = detailPage.width
        slideOut.start()
    }

    XAnimator {
        id: slideIn
        objectName: "slideIn"
        target: detailPage
        duration: 300
        easing.type: Easing.OutQuad
    }

    XAnimator {
        id: slideOut
        objectName: "slideOut"
        target: detailPage
        duration: 300
        easing.type: Easing.InQuad
        onFinished: {
            detailPage.visible = false
            dashboard.detailCommandId = ""
        }
    }

    // Auto-select the first command when the project (and its commands) change
    Connections {
        target: projectService

        function onActiveProjectCommandsChanged() {
            var cmds = projectService.activeProjectCommands
            for (var i = 0; i < cmds.length; ++i) {
                if (dashboard.fullKey(cmds[i].id) === dashboard.selectedCommandId) return
            }
            dashboard.selectCommand(cmds.length > 0 ? cmds[0].id : "")
        }
    }

    Connections {
        target: commandExecutor

        function onStarted(id, pid, label, cmdline, commandId) {
            dashboard.appendCommandLine(commandId || label, "system", label, "▶ " + cmdline + " (PID " + pid + ")")
        }

        function onOutputReceived(id, label, isError, line, commandId) {
            dashboard.appendCommandLine(commandId || label, isError ? "stderr" : "stdout", label, line)
            if (/EADDRINUSE|address already in use/i.test(line))
                dashboard.appendCommandLine(commandId || label, "system", label,
                    "Port looks busy — set Port in Run configuration for a Kill & Run prompt")
            if (/already running/i.test(line))
                dashboard.appendCommandLine(commandId || label, "system", label,
                    "Something is already running outside trun — stop it there, or set Port in Run configuration for a Kill & Run prompt")
        }

        function onFinished(id, label, exitCode, commandId) {
            var text = exitCode < 0 ? "■ stopped" : "■ done, exit code " + exitCode
            dashboard.appendCommandLine(commandId || label, "system", label, text)
        }

        function onFailed(id, label, error, commandId) {
            dashboard.appendCommandLine(commandId || label, "error", label, "■ failed: " + error)
        }
    }

    // Command grid: header + cards
    Item {
        id: gridView
        objectName: "gridView"
        anchors.fill: parent

        // Header pinned to the top, flush with the console panel
        Rectangle {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.leftMargin: 4
            anchors.right: parent.right
            anchors.rightMargin: 4
            height: 48
            color: Theme.windowBackground
            border.color: Theme.border
            border.width: 1
            radius: 8

            Label {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 8
                text: projectService.activeProject.name || "Dashboard"
                color: Theme.textPrimary
                font.bold: true
                font.pixelSize: 13
            }

            Item {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 1
                height: 1
            }
        }

        // Commands below the header, height fits content
        Rectangle {
            id: commandsSection
            anchors.top: header.bottom
            anchors.topMargin: 8
            anchors.left: parent.left
            anchors.right: parent.right
            height: visible ? commandsGrid.implicitHeight + 32 : 0
            color: Theme.windowBackground
            visible: projectService.activeProjectCommands.length > 0
            clip: true

            GridLayout {
                id: commandsGrid
                anchors.fill: parent
                anchors.margins: 8
                // Responsive: card min width 250 + 8 spacing
                columns: Math.max(1, Math.min(4,
                    Math.floor((commandsSection.width - 8) / 258)))
                columnSpacing: 8
                rowSpacing: 8
                uniformCellWidths: true
                uniformCellHeights: true

                Repeater {
                    model: projectService.activeProjectCommands
                    delegate: CommandCard {
                        Layout.fillWidth: true
                        command: modelData
                        selected: dashboard.fullKey(modelData.id) === dashboard.selectedCommandId
                    }
                }
            }
        }
    }

    // Command detail page slides over the grid.
    // No anchors: XAnimator owns x, anchors would fight it.
    DetailPage {
        id: detailPage
        objectName: "detailPage"
        width: parent.width
        height: parent.height
        y: 0
        x: parent.width
        visible: false
        commandId: dashboard.detailCommandId

        onBackRequested: dashboard.closeDetail()
        onClearRequested: dashboard.clearSelectedCommand()
    }
}
