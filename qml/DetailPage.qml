import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "ConsoleFormat.js" as ConsoleFormat

Item {
    id: detailRoot

    // Opaque backdrop: without it the grid shows through the page
    Rectangle {
        anchors.fill: parent
        color: Theme.windowBackground
    }

    // Swallow mouse events: without it clicks fall through
    // to the CommandCards under the page
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onClicked: mouse => mouse.accepted = true
    }

    property string commandId: ""

    signal backRequested()
    signal clearRequested()

    // Ids are file-scoped: expose title-bar controls for hit-testing
    function hitTestControls() {
        return []
    }

    readonly property var command: {
        var cmds = projectService.activeProjectCommands
        for (var i = 0; i < cmds.length; ++i) {
            if (cmds[i] && cmds[i].id === bareId)
                return cmds[i]
        }
        return {}
    }

    readonly property bool isRunning: commandId !== ""
        && commandExecutor.runningCommandIds.indexOf(commandId) >= 0

    // commandId is the project-scoped run key "<projectId>|<cmdId>":
    // bare command ids (npm:serve) repeat across projects.
    readonly property string bareId: {
        var i = commandId.lastIndexOf("|")
        return i < 0 ? commandId : commandId.slice(i + 1)
    }

    property int pidNum: -1
    property string memText: "—"
    property string cpuText: "—"
    property real cpuFraction: -1
    property string uptimeText: "—"
    property real ramFraction: -1

    // Effective run configuration (defaults from the detected command,
    // overrides stored as JSON in Settings per project+command)
    property string cfgName: ""
    property string cfgExecutable: ""
    property string cfgArgsText: ""
    property string cfgWorkdir: ""
    property string cfgEnvText: ""
    property bool cfgAllowMultiple: false
    property int cfgPort: 0

    readonly property string configKey: bareId !== ""
        && projectService.activeProject.id !== undefined
        ? "runConfig/" + projectService.activeProject.id + "/" + bareId : ""

    function splitArgs(text) {
        var args = []
        var cur = ""
        var quote = ""
        for (var i = 0; i < text.length; ++i) {
            var ch = text[i]
            if (quote !== "") {
                if (ch === quote) quote = ""
                else cur += ch
            } else if (ch === '"' || ch === "'") {
                quote = ch
            } else if (ch === " " || ch === "\t") {
                if (cur !== "") { args.push(cur); cur = "" }
            } else {
                cur += ch
            }
        }
        if (cur !== "") args.push(cur)
        return args
    }

    function splitEnv(text) {
        var out = []
        var lines = text.split("\n")
        for (var i = 0; i < lines.length; ++i) {
            var line = lines[i].trim()
            if (line === "" || line.indexOf("=") <= 0) continue
            out.push(line)
        }
        return out
    }

    function loadEffectiveConfig() {
        var c = detailRoot.command
        var defExe = c.command || ""
        var defArgs = c.args ? c.args.join(" ") : ""
        var defWorkdir = projectService.activeProject.project_path || ""
        cfgName = c.label || ""
        cfgExecutable = defExe
        cfgArgsText = defArgs
        cfgWorkdir = defWorkdir
        cfgEnvText = ""
        cfgAllowMultiple = false
        cfgPort = (c.detectedPort || 0) > 0 ? c.detectedPort : 0
        if (configKey === "") return
        var raw = Settings.get(configKey, "")
        if (raw === "") return
        try {
            var cfg = JSON.parse(raw.toString())
            if (cfg.name !== undefined) cfgName = cfg.name
            if (cfg.executable !== undefined) cfgExecutable = cfg.executable
            if (cfg.argsText !== undefined) cfgArgsText = cfg.argsText
            if (cfg.workdir !== undefined) cfgWorkdir = cfg.workdir
            if (cfg.envText !== undefined) cfgEnvText = cfg.envText
            if (cfg.allowMultiple !== undefined) cfgAllowMultiple = cfg.allowMultiple
            if (cfg.port !== undefined) cfgPort = cfg.port
        } catch (e) {}
    }

    function requestRun() {
        var sniffed = dashboard.detectedPorts[detailRoot.commandId] || 0
        if (cfgPort > 0 && sniffed > 0 && sniffed !== cfgPort) {
            dashboard.appendCommandLine(detailRoot.commandId, "system",
                detailRoot.command.label || "cmd",
                "Detected port " + sniffed + " differs from configured " + cfgPort
                + " — update it in Run configuration?")
        }
        var checkPort = sniffed > 0 ? sniffed : cfgPort
        if (checkPort > 0) {
            var occupant = commandExecutor.pidOnPort(checkPort)
            if (occupant > 0 && occupant !== pidNum) {
                portBusyDialog.port = checkPort
                portBusyDialog.pid = occupant
                portBusyDialog.open()
                return
            }
        }
        runEffective()
    }

    function runEffective() {
        var exe = cfgExecutable !== "" ? cfgExecutable : detailRoot.command.command || ""
        var args = splitArgs(cfgArgsText)
        var workdir = cfgWorkdir !== "" ? cfgWorkdir
            : projectService.activeProject.project_path || ""
        var label = cfgName !== "" ? cfgName : detailRoot.command.label || "cmd"
        if (detailRoot.isRunning && !cfgAllowMultiple)
            commandExecutor.stopCommand(detailRoot.commandId)
        commandExecutor.runWithEnv(exe, args, workdir, label,
                                   detailRoot.commandId, splitEnv(cfgEnvText))
    }

    // Offer to kill whoever holds the port and re-run (used on EADDRINUSE).
    function offerKillRun(port, pid) {
        portBusyDialog.port = port
        portBusyDialog.pid = pid
        portBusyDialog.open()
    }

    function formatMem(kb) {
        if (kb < 0) return "—"
        if (kb < 1024) return kb + " KB"
        return (kb / 1024).toFixed(1) + " MB"
    }

    function formatUptime(totalSeconds) {
        if (totalSeconds < 0) return "—"
        function pad(n) { return (n < 10 ? "0" : "") + n }
        return pad(Math.floor(totalSeconds / 3600)) + ":"
            + pad(Math.floor((totalSeconds % 3600) / 60)) + ":"
            + pad(totalSeconds % 60)
    }

    function formatCpu(pct) {
        return pct < 0 ? "—" : pct.toFixed(1) + " %"
    }

    // Single-text console: one TextEdit, so selection and copy can span
    // multiple lines (per-delegate TextEdits only select one row).
    function consoleColorFor(level) {
        if (level === "error" || level === "stderr") return Theme.destructive
        if (level === "warn") return "#ff9800"
        if (level === "system") return "#9c27b0"
        return Theme.textPrimary
    }

    function formatConsoleRow(e) {
        return ConsoleFormat.formatLine(e.timestamp || "", e.target || "",
            e.message || "", consoleColorFor(e.level || ""))
    }

    function rebuildConsole() {
        var parts = []
        var n = commandLog.count()
        for (var i = 0; i < n; ++i)
            parts.push(detailRoot.formatConsoleRow(commandLog.get(i)))
        detailRoot.consoleHtml = parts.join("")
    }

    // Single HTML source for the console view: appending straight to
    // TextEdit.text round-trips through the document serializer and the
    // rows drift apart. One assignment point below instead.
    property string consoleHtml: ""
    onConsoleHtmlChanged: consoleTextEdit.text = consoleHtml

    function refreshStats() {
        if (commandId === "") return
        pidNum = commandExecutor.pidForCommand(commandId)
        var kb = commandExecutor.memoryForCommand(commandId)
        var cpu = commandExecutor.cpuForCommand(commandId)
        var total = commandExecutor.totalMemoryKb()
        memText = formatMem(kb)
        cpuText = formatCpu(cpu)
        cpuFraction = (cpu < 0) ? -1 : Math.min(cpu / 100, 1)
        uptimeText = formatUptime(commandExecutor.elapsedForCommand(commandId))
        ramFraction = (kb > 0 && total > 0) ? Math.min(kb / total, 1) : -1
    }

    onCommandIdChanged: {
        pidNum = -1
        memText = "—"
        cpuText = "—"
        uptimeText = "—"
        ramFraction = -1
        // The command object (and its default args) is derived from commandId
        // and only settles after bindings update, so load on the next tick.
        Qt.callLater(detailRoot.loadEffectiveConfig)
        detailRoot.rebuildConsole()
        if (isRunning) refreshStats()
    }

    // Re-load whenever the resolved command changes (e.g. commands arrive).
    onCommandChanged: detailRoot.loadEffectiveConfig()

    onVisibleChanged: {
        if (visible) {
            loadEffectiveConfig()
            rebuildConsole()
            refreshStats()
        }
    }

    Connections {
        target: commandLog
        function onRowsInserted(parent, first, last) {
            var parts = []
            for (var i = first; i <= last; ++i)
                parts.push(detailRoot.formatConsoleRow(commandLog.get(i)))
            detailRoot.consoleHtml += parts.join("")
        }
        function onModelReset() { detailRoot.rebuildConsole() }
    }

    Timer {
        interval: 1000
        repeat: true
        running: detailRoot.visible && detailRoot.isRunning
        triggeredOnStart: true
        onTriggered: detailRoot.refreshStats()
    }

    // Stat widgets: responsive grid, wraps into fewer columns
    // (down to two rows) when the window gets narrow
    GridLayout {
        id: statsRow
        anchors.top: parent.top
        anchors.topMargin: Theme.spacingLg
        anchors.left: parent.left
        anchors.leftMargin: Theme.spacingLg
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacingLg
        columns: Math.max(1, Math.min(5, Math.floor((detailRoot.width - 8) / 158)))
        columnSpacing: 8
        rowSpacing: 8
        uniformCellWidths: true

        StatCard {
            title: "Status"
            value: detailRoot.isRunning ? "Running" : "Stopped"
            valueColor: detailRoot.isRunning ? "#22c55e" : Theme.destructive
            sub: (detailRoot.cfgExecutable || detailRoot.command.command || "")
                + (detailRoot.cfgArgsText !== "" ? " " + detailRoot.cfgArgsText
                   : detailRoot.command.args ? " " + detailRoot.command.args.join(" ") : "")
            showDot: true
            dotColor: detailRoot.isRunning ? "#22c55e" : Theme.destructive
            dotPulsing: detailRoot.isRunning
            Layout.fillWidth: true
        }

        StatCard {
            title: "PID"
            value: detailRoot.pidNum > 0 ? String(detailRoot.pidNum) : "—"
            sub: "process id"
            Layout.fillWidth: true
        }

        StatCard {
            title: "RAM"
            value: detailRoot.memText
            sub: "resident"
            ringFraction: detailRoot.ramFraction
            ringColor: "#22c55e"
            Layout.fillWidth: true
        }

        StatCard {
            title: "CPU"
            value: detailRoot.cpuText
            sub: "cpu usage"
            ringFraction: detailRoot.cpuFraction
            ringColor: "#60a5fa"
            Layout.fillWidth: true
        }

        StatCard {
            title: "Uptime"
            value: detailRoot.uptimeText
            sub: "elapsed"
            Layout.fillWidth: true
        }
    }

    // Actions row: run configuration (cog) + Run / Stop
    Item {
        id: actionsRow
        anchors.top: statsRow.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        height: 40

        IconButton {
            id: configurationBtn
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            iconSource: iconBaseUrl + (Theme.isDark ? "cog-dark.png" : "cog.png")
            tooltipText: qsTr("Run configuration")
            onClicked: runConfigDialog.openDialog()
        }

        IconButton {
            id: revealFolderBtn
            anchors.left: configurationBtn.right
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            iconSource: iconBaseUrl + "folder-32px.png"
            tooltipText: qsTr("Reveal in Finder")
            onClicked: commandExecutor.revealFolder(projectService.activeProject.project_path || "")
        }

        IconButton {
            id: pinBtn
            anchors.left: revealFolderBtn.right
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            iconSource: iconBaseUrl + (Theme.isDark ? "square-menu-dark.png" : "square-menu.png")
            tooltipText: pinned ? qsTr("Unpin from tray menu") : qsTr("Pin to tray menu")
            active: pinned
            property string projectId: (projectService.activeProject.id || "").toString()
            // Depends on pinnedCommands so the tint follows pinnedChanged
            property var pinList: projectService.pinnedCommands
            property bool pinned: {
                if (projectId === "" || detailRoot.bareId === "") return false
                for (var i = 0; i < pinList.length; ++i) {
                    if (pinList[i].projectId === projectId
                        && pinList[i].commandId === detailRoot.bareId)
                        return true
                }
                return false
            }
            onClicked: {
                if (projectId === "" || detailRoot.bareId === "") return
                projectService.setPinned(projectId, detailRoot.bareId, !pinned)
            }
        }

        // Run / Stop
        Button {
            id: runStopButton
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(160, (detailRoot.width - 32) / 3)
            height: 40
            text: detailRoot.isRunning ? "Stop" : "Run"
            font.pixelSize: 13

            background: Rectangle {
                radius: 8
                color: detailRoot.isRunning ? Theme.destructive : Theme.primary
            }

            contentItem: Label {
                text: parent.text
                color: detailRoot.isRunning ? "white" : Theme.primaryForeground
                font.pixelSize: 13
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: {
                if (detailRoot.isRunning && detailRoot.cfgAllowMultiple) {
                    detailRoot.runEffective()
                } else if (detailRoot.isRunning) {
                    commandExecutor.stopCommand(detailRoot.commandId)
                } else {
                    detailRoot.requestRun()
                }
            }
        }
    }

    RunConfigDialog {
        id: runConfigDialog
        configKey: detailRoot.configKey
        defaultName: detailRoot.command.label || ""
        defaultExecutable: detailRoot.command.command || ""
        defaultArgsText: detailRoot.command.args ? detailRoot.command.args.join(" ") : ""
        defaultWorkdir: projectService.activeProject.project_path || ""
        defaultPort: (detailRoot.command.detectedPort || 0) > 0 ? detailRoot.command.detectedPort : 0
        onSaved: detailRoot.loadEffectiveConfig()
    }

    Timer {
        id: killRunTimer
        interval: 800
        onTriggered: detailRoot.runEffective()
    }

    // Refused dependency (ECONNREFUSED in the log): nothing to kill, just
    // re-run once the dependency is up. Decided by the log line alone.
    property string depHost: ""
    property int depPort: 0
    property string depSvc: ""

    function offerRefusedRun(host, port, svcName) {
        depHost = host
        depPort = port
        depSvc = svcName
        depDialog.open()
    }

    Dialog {
        id: portBusyDialog
        property int port: 0
        property int pid: 0
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 420)
        modal: true
        padding: 0

        background: Rectangle {
            color: Theme.cardBackground
            border.color: Theme.border
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 12

            Label {
                text: qsTr("Port %1 is busy").arg(portBusyDialog.port)
                color: Theme.textPrimary
                font.bold: true
                font.pixelSize: 13
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.topMargin: 20
            }

            Label {
                text: qsTr("Occupied by PID %1. Kill it and run anyway?")
                    .arg(portBusyDialog.pid)
                color: Theme.textMuted
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
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
                        text: qsTr("Cancel")
                        color: Theme.textPrimary
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: portBusyDialog.close()
                }

                Button {
                    Layout.preferredWidth: 110
                    Layout.preferredHeight: 30
                    font.pixelSize: 12

                    background: Rectangle {
                        radius: 6
                        color: Theme.destructive
                    }

                    contentItem: Label {
                        text: qsTr("Kill & Run")
                        color: "white"
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        commandExecutor.killExternal(portBusyDialog.pid)
                        portBusyDialog.close()
                        killRunTimer.start()
                    }
                }
            }
        }
    }

    Dialog {
        id: depDialog
        objectName: "depDialog"
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 420)
        modal: true
        padding: 0

        background: Rectangle {
            color: Theme.cardBackground
            border.color: Theme.border
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 12

            Label {
                text: qsTr("Connection refused")
                color: Theme.textPrimary
                font.bold: true
                font.pixelSize: 13
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.topMargin: 20
            }

            Label {
                text: detailRoot.depSvc !== ""
                    ? qsTr("%1 at %2 refused the connection. Start it, then run again.")
                        .arg(detailRoot.depSvc).arg(detailRoot.depHost + ":" + detailRoot.depPort)
                    : qsTr("%1 refused the connection. Start the dependency, then run again.")
                        .arg(detailRoot.depHost + ":" + detailRoot.depPort)
                color: Theme.textMuted
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
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
                        text: qsTr("Cancel")
                        color: Theme.textPrimary
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: depDialog.close()
                }

                Button {
                    Layout.preferredWidth: 110
                    Layout.preferredHeight: 30
                    font.pixelSize: 12

                    background: Rectangle {
                        radius: 6
                        color: Theme.primary
                    }

                    contentItem: Label {
                        text: qsTr("Run again")
                        color: Theme.primaryForeground
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        depDialog.close()
                        detailRoot.runEffective()
                    }
                }
            }
        }
    }

    // Console of the selected command
    Rectangle {
        anchors.top: actionsRow.bottom
        anchors.topMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        color: Theme.cardBackground
        border.color: Theme.border
        border.width: 1
        radius: 8

        RowLayout {
            id: detailConsoleHeader
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            anchors.topMargin: 8
            height: 28
            spacing: 8

            Label {
                text: "CONSOLE"
                color: Theme.textPrimary
                font.pixelSize: 10
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
            }

            Item { Layout.fillWidth: true }

            IconButton {
                Layout.alignment: Qt.AlignVCenter
                iconSource: iconBaseUrl + (Theme.isDark ? "brush-cleaning-dark.png" : "brush-cleaning.png")
                tooltipText: "Clear console"
                onClicked: detailRoot.clearRequested()
            }
        }

        Flickable {
            id: consoleFlick
            anchors.top: detailConsoleHeader.bottom
            anchors.topMargin: 4
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.right: parent.right
            anchors.rightMargin: 8
            contentWidth: width
            contentHeight: consoleTextEdit.height
            clip: true
            ScrollBar.vertical: ScrollBar {
                active: true
            }

            TextEdit {
                id: consoleTextEdit
                width: consoleFlick.width
                textFormat: Text.RichText
                readOnly: true
                selectByMouse: true
                selectByKeyboard: true
                wrapMode: TextEdit.Wrap
                font.family: "Menlo"
                font.pixelSize: 11
                color: Theme.textPrimary
                selectionColor: Qt.rgba(Theme.accent.r, Theme.accent.g,
                                        Theme.accent.b, 0.35)
                selectedTextColor: Theme.textPrimary
            }

            onContentHeightChanged: {
                if (contentHeight > height)
                    contentY = contentHeight - height
            }
        }
    }
}
