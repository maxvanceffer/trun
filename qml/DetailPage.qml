import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "ConsoleFormat.js" as ConsoleFormat
import "Ui"
pragma ComponentBehavior: Bound

Item {
    id: detailRoot

    readonly property var projSvc: projectService // qmllint disable unqualified
    readonly property var exec: commandExecutor // qmllint disable unqualified
    readonly property var cmdLog: commandLog // qmllint disable unqualified
    readonly property var settings: Settings // qmllint disable unqualified
    readonly property var dash: dashboard // qmllint disable unqualified
    readonly property var tm: treeModel // qmllint disable unqualified

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
        // The owning project first (project-scoped run key), then the
        // active project. Folder-page cards belong to projects that were
        // never selected.
        var pid = detailRoot.projectId
        var all = projSvc.projects()
        for (var p = 0; p < all.length; ++p) {
            var proj = all[p]
            if (!proj || (pid !== "" && (proj.id || "") !== pid))
                continue
            var list = proj.commands || []
            for (var i = 0; i < list.length; ++i) {
                if (list[i] && list[i].id === bareId)
                    return list[i]
            }
        }
        var cmds = projSvc.activeProjectCommands
        for (var j = 0; j < cmds.length; ++j) {
            if (cmds[j] && cmds[j].id === bareId)
                return cmds[j]
        }
        return {}
    }
    // Owning project object (by the run key's project id); falls back to
    // the active project for bare ids.
    readonly property var owningProject: {
        var pid = detailRoot.projectId
        if (pid !== "") {
            var all = projSvc.projects()
            for (var p = 0; p < all.length; ++p) {
                if (all[p] && (all[p].id || "") === pid)
                    return all[p]
            }
        }
        return projSvc.activeProject
    }

    readonly property bool isRunning: commandId !== ""
        && exec.runningCommandIds.indexOf(commandId) >= 0

    // commandId is the project-scoped run key "<projectId>|<cmdId>":
    // bare command ids (npm:serve) repeat across projects.
    readonly property string projectId: {
        var i = commandId.lastIndexOf("|")
        return i < 0 ? "" : commandId.slice(0, i)
    }
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
    // overrides stored as JSON in settings per project+command)
    property string cfgName: ""
    property string cfgExecutable: ""
    property string cfgArgsText: ""
    property string cfgWorkdir: ""
    property string cfgEnvText: ""
    property bool cfgAllowMultiple: false
    property int cfgPort: 0

    readonly property string configKey: bareId !== ""
        && detailRoot.owningProject.id !== undefined
        ? "runConfig/" + (detailRoot.owningProject.id || "") + "/" + bareId : ""

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
        var defWorkdir = detailRoot.owningProject.project_path || ""
        cfgName = c.label || ""
        cfgExecutable = defExe
        cfgArgsText = defArgs
        cfgWorkdir = defWorkdir
        cfgEnvText = ""
        cfgAllowMultiple = false
        cfgPort = (c.detectedPort || 0) > 0 ? c.detectedPort : 0
        if (configKey === "") return
        var raw = settings.get(configKey, "")
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

    // Run persists the panel values first (secrets excluded from
    // storage), then passes the in-memory values — runCommandEffective
    // merges them over the saved argsValues and assembles the args.
    function runConsoleCommand() {
        if (!paramsPanel.canRun) {
            paramsPanel.focusFirstRequired()
            return
        }
        paramsPanel.persistValues()
        projSvc.runCommandEffectiveWithValues(detailRoot.owningProject.id || "",
                                              detailRoot.bareId, paramsPanel.values)
    }

    function requestRun() {
        if (detailRoot.isConsoleCommand) {
            runConsoleCommand()
            return
        }
        var sniffed = dash.detectedPorts[detailRoot.commandId] || 0
        if (cfgPort > 0 && sniffed > 0 && sniffed !== cfgPort) {
            dash.appendCommandLine(detailRoot.commandId, "system",
                detailRoot.command.label || "cmd",
                "Detected port " + sniffed + " differs from configured " + cfgPort
                + " — update it in Run configuration?")
        }
        var checkPort = sniffed > 0 ? sniffed : cfgPort
        if (checkPort > 0) {
            var occupant = exec.pidOnPort(checkPort)
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
            : detailRoot.owningProject.project_path || ""
        var label = cfgName !== "" ? cfgName : detailRoot.command.label || "cmd"
        if (detailRoot.isRunning && !cfgAllowMultiple)
            exec.stopCommand(detailRoot.commandId)
        exec.runWithEnv(exe, args, workdir, label,
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
    // Run-style rows: a relative elapsed gutter ("+0.42s" from the run
    // start) plus the message; the launch line renders in the echo color.
    function formatConsoleRow(e) {
        var msg = e.message || ""
        // Oversized lines (tens-of-KB SQL) are trimmed for readability;
        // the full body stays in the model and is copied via "Full lines".
        if (!detailRoot.showLongFull && msg.length > 500)
            msg = msg.slice(0, 500) + "… [обрезано]"
        var elapsed = (e.at || 0) - detailRoot.runStartMs
        return ConsoleFormat.formatLine(elapsed, msg, e.kind || "",
            Theme.textSubtle, Theme.warning)
    }

    function rebuildConsole() {
        var parts = []
        var n = cmdLog.count()
        for (var i = 0; i < n; ++i)
            parts.push(detailRoot.formatConsoleRow(cmdLog.get(i)))
        // Rebuild covers everything: drop rows buffered while off-page or
        // mid-reset so the flush timer cannot duplicate them.
        detailRoot.pendingRows = []
        detailRoot.consoleHtml = parts.join("")
    }

    // Chips recompute on a debounce: observedLevels()/observedChannels()
    // scan every entry, so recomputing them per countsChanged (i.e. per
    // streamed line) is O(N) per line on top of the console itself.
    property int chipsRev: 0

    Timer {
        id: chipsDebounce
        interval: 400
        onTriggered: detailRoot.chipsRev++
    }

    Connections {
        target: detailRoot.cmdLog
        function onCountsChanged() {
            chipsDebounce.restart()
        }
    }

    readonly property var observedLevelsChips:
        chipsRev >= 0 ? detailRoot.cmdLog.observedLevels() : []
    readonly property var observedChannelsChips:
        chipsRev >= 0 ? detailRoot.cmdLog.observedChannels() : []

    // Single HTML source for the console view: appending straight to
    // TextEdit.text round-trips through the document serializer and the
    // rows drift apart. One assignment point below instead.
    property string consoleHtml: ""
    onConsoleHtmlChanged: consoleTextEdit.text = consoleHtml

    // Console filters: search + chips from observed values. Engine lives
    // in LogModel; here only the panel.
    property string filterQuery: ""
    property var selLevels: []
    property var selChannels: []
    property bool showLongFull: false

    // Run selector ("#1 · 13:31:50 · exit 0"): the console shows one run
    // segment at a time. Null-guarded for standalone/test harnesses.
    readonly property var runEntries: {
        var d = detailRoot.dash
        return (d && detailRoot.commandId !== "") ? d.runMenu(detailRoot.commandId) : []
    }
    readonly property int currentRunSeq: {
        var d = detailRoot.dash
        return (d && detailRoot.commandId !== "") ? d.selectedRunSeq(detailRoot.commandId) : 0
    }
    readonly property int runIndex: {
        var entries = detailRoot.runEntries
        for (var i = 0; i < entries.length; ++i) {
            if (entries[i].seq === detailRoot.currentRunSeq)
                return i
        }
        return -1
    }
    // ms epoch: double, NOT int — a 32-bit int overflows epoch millis
    // and the gutter counts from ~1970 ("+29850022:52").
    readonly property double runStartMs: {
        var d = detailRoot.dash
        return (d && detailRoot.commandId !== "") ? d.runStartMs(detailRoot.commandId, detailRoot.currentRunSeq) : 0
    }

    function showRunSeq(seq) {
        var d = detailRoot.dash
        if (d && detailRoot.commandId !== "")
            d.showRun(detailRoot.commandId, seq)
        // showRun refills the model (clear + addBatch), which rebuilds
        // the console through onModelReset; sync the selector with it.
        detailRoot.syncRunBox()
    }

    function syncRunBox() {
        // Guarded: runs evaluate during creation, before the box exists.
        if (typeof runBox !== "undefined" && runBox)
            runBox.currentIndex = detailRoot.runIndex
    }

    // Runs open/close live (start/finish reassign the maps): keep the
    // selector on the selected segment without touching the model.
    onRunEntriesChanged: syncRunBox()
    onRunIndexChanged: syncRunBox()

    function toggleSel(arr, v) {
        var out = arr.slice()
        var i = out.indexOf(v)
        if (i < 0) out.push(v); else out.splice(i, 1)
        return out
    }

    function applyLogFilters() {
        cmdLog.setFilters(filterQuery, selLevels, selChannels, -1)
    }

    function resetLogFilters() {
        filterQuery = ""
        selLevels = []
        selChannels = []
        searchField.text = ""
        cmdLog.resetFilters()
    }

    function refreshStats() {
        if (commandId === "") return
        pidNum = exec.pidForCommand(commandId)
        var kb = exec.memoryForCommand(commandId)
        var cpu = exec.cpuForCommand(commandId)
        var total = exec.totalMemoryKb()
        memText = formatMem(kb)
        cpuText = formatCpu(cpu)
        cpuFraction = (cpu < 0) ? -1 : Math.min(cpu / 100, 1)
        uptimeText = formatUptime(exec.elapsedForCommand(commandId))
        ramFraction = (kb > 0 && total > 0) ? Math.min(kb / total, 1) : -1
    }

    onCommandIdChanged: {
        pidNum = -1
        memText = "—"
        cpuText = "—"
        uptimeText = "—"
        ramFraction = -1
        // A new command means a new stream: reset the (session-only)
        // filters.
        detailRoot.resetLogFilters()
        // The command object (and its default args) is derived from commandId
        // and only settles after bindings update, so load on the next tick.
        Qt.callLater(detailRoot.loadEffectiveConfig)
        detailRoot.rebuildConsole()
        detailRoot.syncRunBox()
        if (isRunning) refreshStats()
    }

    // Re-load whenever the resolved command changes (e.g. commands arrive).
    onCommandChanged: detailRoot.loadEffectiveConfig()

    onVisibleChanged: {
        if (visible) {
            loadEffectiveConfig()
            rebuildConsole()
            syncRunBox()
            refreshStats()
        }
    }

    // Live appends are buffered and flushed on a short timer: assigning
    // consoleHtml re-parses the whole RichText document in the TextEdit,
    // so doing it per line is O(N²) while a process streams output.
    property var pendingRows: []

    function flushPendingRows() {
        if (pendingRows.length === 0) return
        var parts = []
        for (var i = 0; i < pendingRows.length; ++i)
            parts.push(detailRoot.formatConsoleRow(pendingRows[i]))
        pendingRows = []
        detailRoot.consoleHtml += parts.join("")
    }

    Timer {
        id: appendFlush
        interval: 120
        onTriggered: detailRoot.flushPendingRows()
    }

    Connections {
        target: detailRoot.cmdLog
        function onRowsInserted(parent, first, last) {
            for (var i = first; i <= last; ++i)
                detailRoot.pendingRows.push(detailRoot.cmdLog.get(i))
            appendFlush.restart()
        }
        function onModelReset() {
            // A reset is a full reload (clear or addBatch): the flush
            // timer must not append buffered rows on top of the rebuild.
            detailRoot.pendingRows = []
            appendFlush.stop()
            detailRoot.rebuildConsole()
        }
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
            valueColor: detailRoot.isRunning ? Theme.success : Theme.error
            sub: (detailRoot.cfgExecutable || detailRoot.command.command || "")
                + (detailRoot.cfgArgsText !== "" ? " " + detailRoot.cfgArgsText
                   : detailRoot.command.args ? " " + detailRoot.command.args.join(" ") : "")
            showDot: true
            dotColor: detailRoot.isRunning ? Theme.success : Theme.error
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
            ringColor: Theme.success
            Layout.fillWidth: true
        }

        StatCard {
            title: "CPU"
            value: detailRoot.cpuText
            sub: "cpu usage"
            ringFraction: detailRoot.cpuFraction
            ringColor: Theme.info
            Layout.fillWidth: true
        }

        StatCard {
            title: "Uptime"
            value: detailRoot.uptimeText
            sub: "elapsed"
            Layout.fillWidth: true
        }
    }

    // Framework console commands (argsMeta present) get the inline
    // parameters panel between the stat cards and the actions row.
    readonly property bool isConsoleCommand: (command !== undefined
        && command !== null && command.argsMeta !== undefined
        && command.argsMeta !== null && command.argsMeta.length > 0)

    ConsoleParameters {
        id: paramsPanel
        visible: detailRoot.isConsoleCommand
        anchors.top: statsRow.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: Theme.spacingLg
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacingLg
        // Collapsed keeps the header row visible; expanded fits content.
        height: visible ? paramsPanel.implicitHeight : 0
        projectId: detailRoot.projectId
        commandId: detailRoot.bareId
        command: detailRoot.command
        commandName: detailRoot.command.label || detailRoot.bareId
        running: detailRoot.isRunning
        onCommandChanged: Qt.callLater(paramsPanel.reload)
    }

    // Actions row: run configuration (cog) + Run / Stop
    Item {
        id: actionsRow
        anchors.top: paramsPanel.visible ? paramsPanel.bottom : statsRow.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: Theme.spacingLg
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacingLg
        height: 40

        IconButton {
            id: configurationBtn
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            iconSource: Theme.icon("cog")
            tooltipText: qsTr("Run configuration")
            onClicked: runConfigDialog.openDialog()
        }

        IconButton {
            id: revealFolderBtn
            anchors.left: configurationBtn.right
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            iconSource: Theme.icon("folder-code")
            tooltipText: qsTr("Reveal in Finder")
            onClicked: detailRoot.exec.revealFolder(detailRoot.owningProject.project_path || "")
        }

        IconButton {
            id: pinBtn
            anchors.left: revealFolderBtn.right
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            iconSource: Theme.icon("pin")
            tooltipText: pinned ? qsTr("Unpin from tray menu") : qsTr("Pin to tray menu")
            active: pinned
            property string projectId: (detailRoot.owningProject.id || "").toString()
            // Depends on pinnedCommands so the tint follows pinnedChanged
            property var pinList: detailRoot.projSvc.pinnedCommands
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
                detailRoot.projSvc.setPinned(projectId, detailRoot.bareId, !pinned)
            }
        }

        // Run / Stop
        UiButton {
            id: runStopButton
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(160, (detailRoot.width - 32) / 3)
            label: detailRoot.isRunning ? "Stop" : "Run"
            color: detailRoot.isRunning ? "error" : "primary"
            variant: "solid"
            size: "md"
            // Run is gated on the required positionals (design: Run
            // stays dark until they are filled); Stop always works.
            enabled: detailRoot.isRunning
                || !(detailRoot.isConsoleCommand && !paramsPanel.canRun)
            onClicked: {
                if (detailRoot.isRunning && detailRoot.cfgAllowMultiple) {
                    detailRoot.runEffective()
                } else if (detailRoot.isRunning) {
                    detailRoot.exec.stopCommand(detailRoot.commandId)
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
        defaultWorkdir: detailRoot.owningProject.project_path || ""
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

                UiButton {
                    Layout.preferredWidth: 110
                    label: qsTr("Cancel")
                    color: "secondary"
                    variant: "outline"
                    size: "sm"
                    onClicked: portBusyDialog.close()
                }

                UiButton {
                    Layout.preferredWidth: 110
                    label: qsTr("Kill & Run")
                    color: "error"
                    variant: "solid"
                    size: "sm"
                    onClicked: {
                        detailRoot.exec.killExternal(portBusyDialog.pid)
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

                UiButton {
                    Layout.preferredWidth: 110
                    label: qsTr("Cancel")
                    color: "secondary"
                    variant: "outline"
                    size: "sm"
                    onClicked: depDialog.close()
                }

                UiButton {
                    Layout.preferredWidth: 110
                    label: qsTr("Run again")
                    color: "primary"
                    variant: "solid"
                    size: "sm"
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
        anchors.leftMargin: Theme.spacingLg
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacingLg
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
            height: Theme.controlHeight
            spacing: 8

            Label {
                text: "CONSOLE"
                color: Theme.textPrimary
                font.pixelSize: 10
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
            }

            UiInput {
                id: searchField
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                placeholderText: qsTr("Filter text…")
                onTextChanged: {
                    detailRoot.filterQuery = text
                    detailRoot.applyLogFilters()
                }
            }

            UiSelectMenu {
                id: runBox
                Layout.preferredWidth: 210
                Layout.alignment: Qt.AlignVCenter
                model: detailRoot.runEntries
                searchable: false
                placeholderText: qsTr("No runs yet")
                chevronSource: Theme.icon("chevron-down")
                checkIconSource: Theme.icon("check")
                onActivated: function(index) {
                    var entries = detailRoot.runEntries
                    if (index >= 0 && index < entries.length)
                        detailRoot.showRunSeq(entries[index].seq)
                }
            }

            UiCheckBox {
                Layout.alignment: Qt.AlignVCenter
                label: qsTr("Full lines")
                checked: detailRoot.showLongFull
                onToggled: function(on) {
                    detailRoot.showLongFull = on
                    detailRoot.rebuildConsole()
                }
            }

            Label {
                Layout.alignment: Qt.AlignVCenter
                color: Theme.textMuted
                font.pixelSize: 11
                text: qsTr("Shown %1 of %2").arg(detailRoot.cmdLog.shownCount).arg(detailRoot.cmdLog.totalCount)
            }

            IconButton {
                Layout.alignment: Qt.AlignVCenter
                iconSource: Theme.icon("filter")
                tooltipText: qsTr("Reset filters")
                onClicked: detailRoot.resetLogFilters()
            }

            IconButton {
                Layout.alignment: Qt.AlignVCenter
                iconSource: Theme.icon("clean")
                tooltipText: "Clear console"
                onClicked: detailRoot.clearRequested()
            }
        }

        // Chips row (levels/channels observed in the stream) as a second
        // line: they do not fit on the header line, and the chip count
        // grows with the stream.
        Flow {
            id: chipsFlow
            anchors.top: detailConsoleHeader.bottom
            anchors.topMargin: 4
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.right: parent.right
            anchors.rightMargin: 8
            spacing: 6

                Repeater {
                    id: levelsRepeater
                    model: detailRoot.observedLevelsChips
                    Button {
                        id: levelBtn
                        required property var modelData
                        text: levelBtn.modelData
                        checkable: true
                        checked: detailRoot.selLevels.indexOf(levelBtn.modelData) >= 0
                        font.pixelSize: 11
                        onClicked: {
                            detailRoot.selLevels = detailRoot.toggleSel(detailRoot.selLevels, levelBtn.modelData)
                            detailRoot.applyLogFilters()
                        }
                    }
                }

                Repeater {
                    id: channelsRepeater
                    model: detailRoot.observedChannelsChips
                    Button {
                        id: channelBtn
                        required property var modelData
                        text: channelBtn.modelData
                        checkable: true
                        checked: detailRoot.selChannels.indexOf(channelBtn.modelData) >= 0
                        font.pixelSize: 11
                        onClicked: {
                            detailRoot.selChannels = detailRoot.toggleSel(detailRoot.selChannels, channelBtn.modelData)
                            detailRoot.applyLogFilters()
                        }
                    }
                }
            }

        Label {
            anchors.top: chipsFlow.bottom
            anchors.topMargin: 12
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width - 32, 420)
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            color: Theme.textMuted
            font.pixelSize: 12
            visible: detailRoot.cmdLog.totalCount > 0 && detailRoot.cmdLog.shownCount === 0
            text: qsTr("No matches — loosen the filters.")
                + (detailRoot.cmdLog.hiddenUnparsedCount > 0
                    ? " " + qsTr("Hidden unparsed lines: %1.").arg(detailRoot.cmdLog.hiddenUnparsedCount) : "")
        }

        // Log viewport: recessed surface (darker than the card, like a
        // terminal inside it), rounded, with a hairline border.
        Rectangle {
            id: consoleViewport
            anchors.top: chipsFlow.bottom
            anchors.topMargin: 4
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.right: parent.right
            anchors.rightMargin: 8
            radius: Theme.radiusSm
            color: Theme.windowBackground
            border.color: Theme.border
            border.width: 1
        }

        Flickable {
            id: consoleFlick
            anchors.fill: consoleViewport
            anchors.margins: 8
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
