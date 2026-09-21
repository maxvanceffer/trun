import QtQuick
import QtQuick.Controls 6.5
import QtQuick.Layouts 1.5
import "ToolPlugins.js" as ToolPlugins

Rectangle {
    id: dashboard
    width: parent.width
    height: parent.height
    color: Theme.windowBackground

    // Outer window corners on the content side (rounded unless zoomed).
    // Defaults to square for embedded/test use.
    property int cornerRadius: 0
    topRightRadius: cornerRadius
    bottomRightRadius: cornerRadius

    // Console shows output of the selected command only.
    // Histories are keyed by project-scoped run key "<projectId>|<cmdId>":
    // bare command ids (npm:serve) repeat across projects.
    property string selectedCommandId: ""
    property var commandHistories: ({})
    // Runtime-detected ports per run key (sniffed from output URLs)
    property var detectedPorts: ({})
    // Busy-port reports whose port is not known yet (symfony prints
    // "[WARNING] The local web server is already running" before
    // "Listening on https://127.0.0.1:8000"). Keyed by full run key.
    property var pendingBusy: ({})
    // Refused dependencies per run key ({host, port}): reported once, when
    // a run fails after ECONNREFUSED (there is nobody to kill — the
    // dependency itself is down).
    property var pendingRefused: ({})

    // Command shown on the detail page; empty means the grid is visible
    property string detailCommandId: ""

    // Host machine metrics (absent in tests that load this file standalone).
    readonly property var stats: (typeof systemStats !== "undefined") ? systemStats : null

    // Updater presence (absent in tests that load this file standalone).
    readonly property bool hasUpdater: typeof updater !== "undefined"
    readonly property bool hasUpdate: dashboard.hasUpdater && updater.updateAvailable
    readonly property string updateVersion: dashboard.hasUpdater ? updater.latestVersion : ""

    signal updateRequested()

    readonly property string cpuText: (stats && stats.cpuUsage >= 0)
        ? Math.round(stats.cpuUsage * 100) + "%" : "—"
    // CPU topology as the big value, with a static caption below:
    // the ring already graphs the usage percentage, so repeating it as
    // text would duplicate the graph.
    readonly property string cpuValueText: {
        if (!stats) return cpuText
        var cores = stats.cpuCores || 0
        var threads = stats.cpuThreads || 0
        if (cores > 0 && threads > 0) return qsTr("%1 core / %2 threads").arg(cores).arg(threads)
        if (threads > 0) return qsTr("%1 threads").arg(threads)
        if (cores > 0) return qsTr("%1 core").arg(cores)
        return cpuText
    }
    readonly property string cpuSubText: {
        if (!stats) return qsTr("usage")
        var cores = stats.cpuCores || 0
        var threads = stats.cpuThreads || 0
        if (cores > 0 || threads > 0) return qsTr("cpu information")
        return qsTr("usage")
    }
    readonly property string ramUsedText: stats ? kbToText(stats.memoryUsedKb) : "—"
    readonly property string ramTotalText: stats ? kbToText(stats.memoryTotalKb) : "—"
    // Same style as the CPU card: used/total on top, static caption below.
    readonly property string ramValueText: {
        if (!stats || stats.memoryUsedKb < 0 || stats.memoryTotalKb <= 0)
            return ramUsedText
        return kbToText(stats.memoryUsedKb) + " / " + kbToText(stats.memoryTotalKb)
    }
    readonly property string ramSubText: {
        if (!stats || stats.memoryTotalKb <= 0)
            return qsTr("of ") + ramTotalText
        return qsTr("ram information")
    }

    function kbToText(kb) {
        if (kb === undefined || kb < 0) return "—"
        var gb = kb / 1048576
        return (gb >= 10 ? Math.round(gb) : gb.toFixed(1)) + " GB"
    }

    // Activity lists (recent persists across launches, running is live).
    readonly property var recents: projectService.recentCommands
    readonly property var running: projectService.runningCommands
    readonly property bool hasRecents: recents !== undefined && recents !== null
                                        && recents.length > 0
    readonly property bool hasRunning: running !== undefined && running !== null
                                        && running.length > 0
    readonly property bool activityEmpty: !hasRecents && !hasRunning

    // Drives relative-time labels without per-row timers.
    property int nowTick: 0

    Timer {
        interval: 30000
        repeat: true
        running: true
        onTriggered: dashboard.nowTick++
    }

    // A busy-port report that never resolves to a port still gets its
    // "set Port" hint, just delayed — the port may arrive lines later.
    Timer {
        interval: 2000
        repeat: true
        running: true
        onTriggered: {
            var p = dashboard.pendingBusy
            var changed = false
            var now = Date.now()
            for (var key in p) {
                if (now - p[key].at < 6000)
                    continue
                var e = p[key]
                dashboard.appendCommandLine(e.cmdId, "system", e.label,
                    e.prefix + " — " + e.reason)
                delete p[key]
                changed = true
            }
            if (changed)
                dashboard.pendingBusy = p
        }
    }

    function timeAgo(iso) {
        var _ = dashboard.nowTick
        var t = Date.parse(iso)
        if (isNaN(t)) return ""
        var s = Math.max(0, (Date.now() - t) / 1000)
        if (s < 60) return qsTr("just now")
        if (s < 3600) return Math.floor(s / 60) + qsTr("m ago")
        if (s < 86400) return Math.floor(s / 3600) + qsTr("h ago")
        return Math.floor(s / 86400) + qsTr("d ago")
    }

    function elapsedText(iso) {
        var _ = dashboard.nowTick
        var t = Date.parse(iso)
        if (isNaN(t)) return ""
        var s = Math.max(0, (Date.now() - t) / 1000)
        var h = Math.floor(s / 3600)
        var m = Math.floor((s % 3600) / 60)
        var sec = Math.floor(s % 60)
        var mm = ("0" + m).slice(-2)
        var ss = ("0" + sec).slice(-2)
        return h > 0 ? h + ":" + mm + ":" + ss : m + ":" + ss
    }

    // Manifest file from a project id ("<path>/<manifest>"), "" when unknown.
    // Activity cards (recent, running) show the manifest icon from it.
    function manifestOf(projectId) {
        var p = (projectId || "").toString()
        var i = p.lastIndexOf("/")
        return i < 0 ? "" : p.slice(i + 1)
    }

    function openActivity(projectId, commandId) {        if (projectId && projectId !== "")
            projectService.selectProject(projectId)
        openDetail(commandId)
    }

    // At most the six most recent commands, as a card row.
    function recentCards() {
        return (dashboard.recents !== undefined && dashboard.recents !== null)
            ? dashboard.recents.slice(0, 6) : []
    }

    // True when the (projectId, commandId) pair is currently running.
    function isRunningCommand(projectId, commandId) {
        var run = dashboard.running
        if (run === undefined || run === null)
            return false
        var pid = (projectId || "").toString()
        var cid = (commandId || "").toString()
        for (var i = 0; i < run.length; ++i) {
            var r = run[i]
            if ((r["projectId"] || "").toString() === pid
                    && (r["commandId"] || "").toString() === cid)
                return true
        }
        return false
    }

    // Re-run every command in the recent list, skipping ones already running.
    function runAllRecent() {
        var list = dashboard.recentCards()
        for (var i = 0; i < list.length; ++i) {
            var item = list[i]
            var pid = (item["projectId"] || item["projectPath"] || "").toString()
            var cid = (item["commandId"] || "").toString()
            if (pid === "" || cid === "")
                continue
            if (dashboard.isRunningCommand(item["projectId"] || pid, cid))
                continue
            projectService.runCommandEffective(pid, cid)
        }
    }

    // Port from an "address already in use" line (0 when it can't be read).
    // SGR color codes are stripped first: matching runs on plain text.
    function portFromError(line) {
        line = line.replace(/\x1b\[[0-9;:?]*[ -/]*[@-~]/g, "")
        var m = /\b\d{1,3}(?:\.\d{1,3}){3}:(\d{2,5})\b/.exec(line)
        if (!m) m = /port[:\s]+(\d{2,5})/i.exec(line)
        if (!m) m = /:(\d{2,5})\b/.exec(line)
        return m ? parseInt(m[1], 10) : 0
    }

    // On a busy port, offer to kill the occupant and re-run (detail page).
    // Returns true when the Kill & Run dialog was opened.
    function resolveBusyPort(cmdId, line) {
        var port = dashboard.portFromError(line)
        if (port > 0)
            return port
        // Many "already running" lines (e.g. `symfony serve`) carry no port,
        // so fall back to the sniffed / configured / detected port.
        var key = fullKey(cmdId)
        var sniffed = dashboard.detectedPorts[key] || 0
        if (sniffed > 0)
            return sniffed
        if (typeof detailPage !== "undefined" && detailPage
                && detailPage.commandId === key) {
            if (detailPage.cfgPort > 0)
                return detailPage.cfgPort
            if (detailPage.command && (detailPage.command.detectedPort || 0) > 0)
                return detailPage.command.detectedPort
        }
        return 0
    }

    // Command descriptor for a full run key (bare ids repeat across projects).
    // Searches the active project first, then every manifest project of the
    // active folder (folder page cards belong to many projects at once).
    // Returns {command, manifest}; both empty when unknown.
    function commandByKey(key) {
        var sep = key.lastIndexOf("|")
        var pid = sep < 0 ? "" : key.slice(0, sep)
        var bare = key.slice(sep + 1)
        var cmds = projectService.activeProjectCommands
        for (var i = 0; i < cmds.length; ++i) {
            if (cmds[i] && cmds[i].id === bare
                && (pid === "" || (projectService.activeProject.id || "") === pid))
                return { command: cmds[i],
                         manifest: (projectService.activeProject
                                    && projectService.activeProject.manifest) || "" }
        }
        var folder = projectService.activeFolderProjects
        for (var f = 0; f < folder.length; ++f) {
            var p = folder[f]
            if (!p || (pid !== "" && p.id !== pid))
                continue
            var list = p.commands || []
            for (var j = 0; j < list.length; ++j) {
                if (list[j] && list[j].id === bare)
                    return { command: list[j], manifest: p.manifest || "" }
            }
        }
        return { command: {}, manifest: "" }
    }

    // Tool-aware busy-port detection: the owning plugin (vite, symfony, ...)
    // adds its own log triggers on top of the generic ones, filtered by
    // manifest (see ToolPlugins.js). On a match, opens Kill & Run when the
    // port and its occupant are known, otherwise parks a pending report.
    function checkBusyLine(cmdId, label, line) {
        var plain = ToolPlugins.stripSgr(line)
        var found = dashboard.commandByKey(dashboard.fullKey(cmdId))
        var cmd = found.command || {}
        var m = ToolPlugins.matchLine(cmd.detectedProgram || "", found.manifest || "", plain)
        if (!m.hit.matched)
            return
        var plugin = m.plugin
        var prefix = (m.tool && plugin && plugin.hint) ? plugin.hint
            : /already running/i.test(plain)
              ? "Something is already running outside trun — stop it there"
              : "Port looks busy"
        dashboard.handleBusyPort(cmdId, label, plain, prefix)
    }

    // Client-side connect failure: remember the first refused endpoint of
    // the run; onFinished reports it once when the run actually fails.
    function checkRefusedLine(cmdId, line) {
        var key = fullKey(cmdId)
        if (pendingRefused[key] !== undefined)
            return
        var r = ToolPlugins.parseRefused(ToolPlugins.stripSgr(line))
        if (!r || r.port <= 0)
            return
        var p = pendingRefused
        p[key] = { host: r.host, port: r.port }
        pendingRefused = p
    }

    // Busy-port failure: open the Kill & Run dialog when the port and its
    // occupant are known. Otherwise remember the report — the port often
    // arrives lines later ("Listening on ...") and the dialog opens then.
    // Stays silent throughout: the process' own error line is already in
    // the console; only a port that never appears gets one hint (via Timer),
    // naming the exact reason. Returns true when the dialog was opened.
    function handleBusyPort(cmdId, label, line, prefix) {
        var info = dashboard.killRunInfo(cmdId, line)
        if (info.status === "ready") {
            detailPage.offerKillRun(info.port, info.occupant)
            var q = pendingBusy
            delete q[fullKey(cmdId)]
            pendingBusy = q
            return true
        }
        var reason = info.status === "page" ? "open the command detail page and run again"
            : info.status === "port" ? "set Port in Run configuration for a Kill & Run prompt"
            : "no process found on port " + info.port + " (lsof)"
        var p = pendingBusy
        p[fullKey(cmdId)] = {cmdId: cmdId, label: label, prefix: prefix,
                             reason: reason, at: Date.now()}
        pendingBusy = p
        return false
    }

    // Kill & Run feasibility for a busy report (detail page only).
    // Returns {status, port, occupant}; status is one of
    // "ready" | "page" | "port" | "occupant".
    function killRunInfo(cmdId, line) {
        var key = fullKey(cmdId)
        if (dashboard.activePage !== "detail"
                || typeof detailPage === "undefined" || !detailPage
                || detailPage.commandId !== key)
            return { status: "page" }
        var port = dashboard.resolveBusyPort(cmdId, line)
        if (port <= 0)
            return { status: "port" }
        var occupant = commandExecutor.pidOnPort(port)
        if (occupant <= 0)
            return { status: "occupant", port: port }
        return { status: "ready", port: port, occupant: occupant }
    }

    // Opens the Kill & Run dialog when the port and its occupant are known
    // (detail page only). Clears any pending busy report for the command.
    function tryKillRun(cmdId, line) {
        var info = dashboard.killRunInfo(cmdId, line)
        if (info.status !== "ready")
            return false
        detailPage.offerKillRun(info.port, info.occupant)
        var p = pendingBusy
        delete p[fullKey(cmdId)]
        pendingBusy = p
        return true
    }

    // Refused dependency dialog, decided by the log line alone
    // ({host, port} from ECONNREFUSED): no service lookup, nothing to kill.
    // Returns true when the dialog was opened.
    function offerRefusedRun(key, refused) {
        if (dashboard.activePage !== "detail"
                || typeof detailPage === "undefined" || !detailPage
                || detailPage.commandId !== key)
            return false
        detailPage.offerRefusedRun(refused.host, refused.port,
                                   ToolPlugins.serviceName(refused.port))
        return true
    }

    function refusedHint(refused) {
        var svc = ToolPlugins.serviceName(refused.port)
        var where = refused.host + ":" + refused.port
        return (svc !== "" ? svc + " at " + where : where)
            + " refused the connection — start "
            + (svc !== "" ? "it" : "the dependency") + " and run again"
    }

    // ─── Pages: root Dashboard / folder manifests / command detail ───
    property string activePage: "dashboard"
    property string detailReturnPage: "dashboard"

    signal addCustomRequested(string folderPath)

    readonly property var activeProject: projectService.activeProject
    readonly property bool hasProject: activeProject !== undefined && activeProject !== null
                                       && activeProject.id !== undefined
                                       && activeProject.id !== ""

    // Folder crumbs for the current context: the open folder itself, or
    // the folder owning the open detail command.
    readonly property string detailProjectId: {
        var k = dashboard.detailCommandId || ""
        var i = k.lastIndexOf("|")
        return i < 0 ? k : k.slice(0, i)
    }
    readonly property string detailFolderPath: {
        var p = dashboard.detailProjectId
        var i = p.lastIndexOf("/")
        return i < 0 ? p : p.slice(0, i)
    }
    readonly property string detailManifest: {
        var p = dashboard.detailProjectId
        return p.slice(p.lastIndexOf("/") + 1)
    }
    readonly property string detailLabel: {
        var found = dashboard.commandByKey(dashboard.detailCommandId)
        return ((found && found.command && found.command.label) || "").toString()
    }
    readonly property string contextFolderPath: dashboard.activePage === "detail"
        ? dashboard.detailFolderPath
        : ((projectService.activeFolder && projectService.activeFolder.path) || "")
    readonly property var folderSegments: dashboard.contextFolderPath !== ""
        ? projectService.folderCrumbs(dashboard.contextFolderPath) : []
    readonly property bool showFolderCrumbs: dashboard.folderSegments.length > 0
        && (activePage === "folder" || activePage === "detail")

    function goHome() {
        dashboard.closeDetail()
        dashboard.activePage = "dashboard"
    }

    function openFolder(folderPath) {
        dashboard.closeDetail()
        if (folderPath !== undefined && folderPath !== "")
            projectService.selectFolder(folderPath)
        dashboard.activePage = "folder"
    }

    function openDatabases() {
        dashboard.closeDetail()
        dashboard.activePage = "databases"
    }

    function openDocker() {
        dashboard.closeDetail()
        dashboard.activePage = "docker"
    }

    Connections {
        target: projectService
        function onActiveProjectChanged() {
            if (!dashboard.hasProject)
                dashboard.activePage = "dashboard"
        }
    }

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
        var controls = [crumbDashboardLabel]
        for (var i = 0; i < segRepeater.count; ++i) {
            var seg = segRepeater.itemAt(i)
            if (seg)
                controls.push(seg)
            else
                console.warn("markHitTest: breadcrumb segment " + i + " is null")
        }
        if (crumbManifestLabel && crumbManifestLabel.visible)
            controls.push(crumbManifestLabel)
        for (var j = 0; j < controls.length; ++j) {
            if (controls[j])
                agent.setHitTestVisible(controls[j], true)
            else
                console.warn("markHitTest: breadcrumb control " + j + " is null")
        }
    }

    function appendCommandLine(cmdId, level, target, message) {
        cmdId = fullKey(cmdId)
        var histories = commandHistories
        var h = histories[cmdId] || []
        h.push({level: level, target: target, message: message})
        while (h.length > 1000) h.shift()
        histories[cmdId] = h
        commandHistories = histories
        // Sniff the port frameworks print at startup ("Local:
        // http://localhost:5173/", "Listening on ...", "listening on
        // port 8040"). Patterns live in ToolPlugins.js; matching runs on
        // SGR-stripped text.
        var port = ToolPlugins.matchListening(
            ToolPlugins.stripSgr(message))
        if (port > 0) {
            var ports = detectedPorts
            ports[cmdId] = port
            detectedPorts = ports
            // A busy-port report may have arrived before the port was known
            if (pendingBusy[cmdId] !== undefined)
                dashboard.tryKillRun(cmdId, message)
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
        dashboard.detailReturnPage = dashboard.activePage === "folder" ? "folder" : "dashboard"
        dashboard.detailCommandId = fullKey(cmdId)
        selectCommand(cmdId)
        dashboard.activePage = "detail"
    }

    function closeDetail() {
        if (dashboard.activePage !== "detail") return
        dashboard.activePage = dashboard.detailReturnPage
        dashboard.detailCommandId = ""
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
            var pr = dashboard.pendingRefused
            delete pr[dashboard.fullKey(commandId || label)]
            dashboard.pendingRefused = pr
        }

        function onOutputReceived(id, label, isError, line, commandId) {
            dashboard.appendCommandLine(commandId || label, isError ? "stderr" : "stdout", label, line)
            dashboard.checkBusyLine(commandId || label, label, line)
            dashboard.checkRefusedLine(commandId || label, line)
        }

        function onFinished(id, label, exitCode, commandId) {
            var text = exitCode < 0 ? "■ stopped" : "■ done, exit code " + exitCode
            dashboard.appendCommandLine(commandId || label, "system", label, text)
            // Failed after ECONNREFUSED: dialog from the log line alone,
            // text hint when off the command page.
            var key = dashboard.fullKey(commandId || label)
            var refused = dashboard.pendingRefused[key]
            var pr = dashboard.pendingRefused
            delete pr[key]
            dashboard.pendingRefused = pr
            if (exitCode > 0 && refused !== undefined) {
                if (!dashboard.offerRefusedRun(key, refused))
                    dashboard.appendCommandLine(commandId || label, "system", label,
                        dashboard.refusedHint(refused))
            }
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

        // Global header with breadcrumb: Dashboard / project / config.
        Rectangle {
            id: header
            objectName: "header"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.titleBarHeight
            color: Theme.windowBackground

            RowLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.spacingLg
                anchors.rightMargin: Theme.spacingLg
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.spacingXs

                Label {
                    id: crumbDashboardLabel
                    text: qsTr("Dashboard")
                    color: dashboard.activePage === "dashboard" ? Theme.textPrimary : Theme.textMuted
                    font.bold: true
                    font.pixelSize: Theme.fontSizeLg

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: dashboard.goHome()
                    }
                }

                Label {
                    visible: dashboard.activePage === "databases"
                    text: "/"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeLg
                }

                Label {
                    visible: dashboard.activePage === "databases"
                    text: qsTr("Databases")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeLg
                }

                Label {
                    visible: dashboard.activePage === "docker"
                    text: "/"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeLg
                }

                Label {
                    visible: dashboard.activePage === "docker"
                    text: qsTr("Docker")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeLg
                }

                // Folder crumbs (folder page and detail alike). Segments
                // navigate back to their folder; the manifest/command tail
                // on detail pages names the current location.
                Repeater {
                    id: segRepeater
                    model: dashboard.showFolderCrumbs ? dashboard.folderSegments : []

                    delegate: RowLayout {
                        required property var modelData
                        required property int index
                        spacing: Theme.spacingXs

                        Label {
                            text: "/"
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeLg
                        }

                        Label {
                            text: modelData.name || ""
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeLg
                            elide: Text.ElideRight

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: dashboard.openFolder(modelData.path || "")
                            }
                        }
                    }
                }

                Label {
                    visible: dashboard.activePage === "detail" && dashboard.detailManifest !== ""
                    text: "/"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeLg
                }

                Label {
                    id: crumbManifestLabel
                    visible: dashboard.activePage === "detail" && dashboard.detailManifest !== ""
                    text: dashboard.detailManifest
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeLg
                    elide: Text.ElideRight

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: dashboard.openFolder(dashboard.detailFolderPath)
                    }
                }

                Label {
                    visible: dashboard.activePage === "detail" && dashboard.detailLabel !== ""
                    text: "/"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeLg
                }

                Label {
                    id: crumbCommandLabel
                    visible: dashboard.activePage === "detail" && dashboard.detailLabel !== ""
                    text: dashboard.detailLabel
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeLg
                    elide: Text.ElideRight
                }

                Item { Layout.fillWidth: true }
            }
        }

        // Page host: cross-slide + fade between root dashboard and project.
        Item {
            id: pageHost
            objectName: "pageHost"
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true

            // Root dashboard: system stats + recent/running activity.
            Item {
                id: rootPage
                objectName: "rootPage"
                anchors.fill: parent
                enabled: dashboard.activePage === "dashboard"
                // Hidden pages must leave the scene entirely: opacity 0 alone
                // keeps full-fill MouseAreas (e.g. DetailPage's click swallower)
                // on top, stealing hover from the page below.
                visible: opacity > 0
                opacity: dashboard.activePage === "dashboard" ? 1 : 0
                x: dashboard.activePage === "dashboard" ? 0 : -Theme.spacingXl
                Behavior on opacity { NumberAnimation { duration: Theme.animMedium; easing.type: Theme.easingStandard } }
                Behavior on x { NumberAnimation { duration: Theme.animSlide; easing.type: Theme.easingStandard } }

                // Host machine metrics: hostname, CPU and RAM.
                RowLayout {
                    id: systemStatsRow
                    objectName: "systemStatsRow"
                    anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.spacingLg
            anchors.rightMargin: Theme.spacingLg
            anchors.topMargin: Theme.spacingMd
            spacing: Theme.spacingMd

            StatCard {
                objectName: "hostStatCard"
                title: qsTr("HOST")
                value: dashboard.stats ? dashboard.stats.hostName : "—"
                sub: dashboard.stats ? dashboard.stats.platform : ""
                Layout.fillWidth: true
            }

            StatCard {
                objectName: "cpuStatCard"
                title: qsTr("CPU")
                value: dashboard.cpuValueText
                sub: dashboard.cpuSubText
                ringFraction: dashboard.stats ? dashboard.stats.cpuUsage : -1
                ringColor: Theme.primary
                Layout.fillWidth: true
            }

            StatCard {
                objectName: "ramStatCard"
                title: qsTr("RAM")
                value: dashboard.ramValueText
                sub: dashboard.ramSubText
                ringFraction: dashboard.stats ? dashboard.stats.memoryUsage : -1
                ringColor: Theme.primary
                Layout.fillWidth: true
            }
        }

        // Update alert between stats and activity: full width, same side
        // margins as the cards and equal spacing above and below.
        // Collapses to zero height when no update is available.
        Item {
            id: updateAlertSlot
            objectName: "updateAlertSlot"
            anchors.top: systemStatsRow.bottom
            anchors.topMargin: Theme.spacingMd
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.spacingLg
            anchors.rightMargin: Theme.spacingLg
            height: dashboard.hasUpdate ? 44 : 0
            visible: dashboard.hasUpdate
            clip: true

            UpdateAlert {
                anchors.fill: parent
                messageText: qsTr("Update to v%1 available").arg(dashboard.updateVersion)
                actionText: qsTr("Update")
                iconSource: iconBaseUrl + (Theme.isDark ? "rotate-cw-dark.png" : "rotate-cw.png")
                onActionTriggered: dashboard.updateRequested()
            }
        }

        // Activity below the stats: recently run + currently running commands.
        Item {
            id: activityView
            objectName: "activityView"
            anchors.top: updateAlertSlot.bottom
            anchors.topMargin: Theme.spacingMd
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.spacingLg
            anchors.rightMargin: Theme.spacingLg
            anchors.bottomMargin: Theme.spacingLg

            Column {
                id: activitySections
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: Theme.spacingXl

                // Recent commands (persisted, newest first): up to 6 cards
                Column {
                    width: parent.width
                    spacing: Theme.spacingSm
                    visible: dashboard.hasRecents

                    RowLayout {
                        width: parent.width
                        spacing: Theme.spacingSm

                        Label {
                            text: qsTr("RECENT COMMANDS")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSm
                            font.bold: true
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: Theme.border
                        }

                        IconButton {
                            objectName: "runAllRecentButton"
                            Layout.alignment: Qt.AlignVCenter
                            iconSource: iconBaseUrl + (Theme.isDark ? "run-all-dark.png" : "run-all.png")
                            tooltipText: qsTr("Run all commands")
                            onClicked: dashboard.runAllRecent()
                        }
                    }

                    Row {
                        spacing: Theme.spacingSm

                        Repeater {
                            model: dashboard.recentCards()

                            delegate: Rectangle {
                                id: recentCard
                                required property var modelData
                                readonly property string manifest: dashboard.manifestOf(modelData.projectId)
                                width: 200
                                height: 68
                                radius: Theme.radiusMd
                                color: Theme.cardBackground
                                border.width: 1
                                border.color: recentMouse.containsMouse ? Theme.accent : Theme.border

                                MouseArea {
                                    id: recentMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: dashboard.openActivity(recentCard.modelData.projectId,
                                                                      recentCard.modelData.commandId)
                                }

                                Column {
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingSm
                                    spacing: 2

                                    RowLayout {
                                        width: parent.width
                                        spacing: Theme.spacingXs

                                        Image {
                                            Layout.preferredWidth: Math.round(
                                                (recentCard.manifest === "composer.json" ? 24 : 11)
                                                * ToolPlugins.manifestAspect(recentCard.manifest))
                                            Layout.preferredHeight: recentCard.manifest === "composer.json" ? 24 : 11
                                            Layout.alignment: Qt.AlignVCenter
                                            source: iconBaseUrl + ToolPlugins.manifestIcon(
                                                recentCard.manifest, Theme.isDark)
                                            fillMode: Image.PreserveAspectFit
                                            smooth: true
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            Layout.alignment: Qt.AlignVCenter
                                            text: recentCard.modelData.label
                                            color: Theme.textPrimary
                                            font.pixelSize: Theme.fontSizeMd
                                            font.bold: true
                                            elide: Text.ElideRight
                                            transform: Translate { y: -2 }
                                        }
                                    }
                                    Label {
                                        width: parent.width
                                        text: recentCard.modelData.projectName
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontSizeSm
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        width: parent.width
                                        text: dashboard.timeAgo(recentCard.modelData.lastUsedAt)
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontSizeSm
                                    }
                                }

                                // Green dot while this command is running.
                                // Declared after the text so it paints on top.
                                Rectangle {
                                    anchors.top: parent.top
                                    anchors.right: parent.right
                                    anchors.topMargin: Theme.spacingSm
                                    anchors.rightMargin: Theme.spacingSm
                                    width: 8
                                    height: 8
                                    radius: 4
                                    z: 2
                                    color: "#22c55e"
                                    visible: dashboard.isRunningCommand(
                                        recentCard.modelData.projectId,
                                        recentCard.modelData.commandId)
                                }

                                // Hover glow outside the card (see CommandCard):
                                // the row keeps side spacing so the ring
                                // never touches the viewport edge.
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -3
                                    radius: Theme.radiusMd + 3
                                    color: "transparent"
                                    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g,
                                                          Theme.accent.b, 0.45)
                                    border.width: 2
                                    opacity: recentMouse.containsMouse ? 1 : 0

                                    Behavior on opacity {
                                        NumberAnimation {
                                            duration: 250
                                            easing.type: Theme.easingStandard
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Running commands
                Column {
                    width: parent.width
                    spacing: Theme.spacingSm
                    visible: dashboard.hasRunning

                    RowLayout {
                        width: parent.width
                        spacing: Theme.spacingSm
                        Rectangle {
                            Layout.alignment: Qt.AlignVCenter
                            width: 8
                            height: 8
                            radius: 4
                            color: "#22c55e"
                        }
                        Label {
                            Layout.alignment: Qt.AlignVCenter
                            text: qsTr("RUNNING COMMANDS")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSm
                            font.bold: true
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: Theme.border
                        }
                        IconButton {
                            Layout.alignment: Qt.AlignVCenter
                            iconSource: iconBaseUrl + (Theme.isDark ? "stop-all-dark.png" : "stop-all.png")
                            tooltipText: qsTr("Stop all commands")
                            onClicked: commandExecutor.killAll()
                        }
                    }

                    Row {
                        spacing: Theme.spacingSm

                        Repeater {
                            model: projectService.runningCommands

                            delegate: Rectangle {
                                id: runningCard
                                required property var modelData
                                readonly property string manifest: dashboard.manifestOf(modelData.projectId)
                                width: 200
                                height: 68
                                radius: Theme.radiusMd
                                color: Theme.cardBackground
                                border.width: 1
                                border.color: runningMouse.containsMouse ? Theme.accent : Theme.border

                                // Hover glow outside the card (see CommandCard).
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -3
                                    radius: Theme.radiusMd + 3
                                    color: "transparent"
                                    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g,
                                                          Theme.accent.b, 0.45)
                                    border.width: 2
                                    opacity: runningMouse.containsMouse ? 1 : 0
                                    Behavior on opacity {
                                        NumberAnimation { duration: Theme.animFast; easing.type: Theme.easingStandard }
                                    }
                                }

                                MouseArea {
                                    id: runningMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: dashboard.openActivity(runningCard.modelData.projectId,
                                                                      runningCard.modelData.commandId)
                                }

                                Column {
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingSm
                                    spacing: 2

                                    RowLayout {
                                        width: parent.width
                                        spacing: Theme.spacingXs

                                        Image {
                                            Layout.preferredWidth: Math.round(
                                                (runningCard.manifest === "composer.json" ? 24 : 11)
                                                * ToolPlugins.manifestAspect(runningCard.manifest))
                                            Layout.preferredHeight: runningCard.manifest === "composer.json" ? 24 : 11
                                            Layout.alignment: Qt.AlignVCenter
                                            source: iconBaseUrl + ToolPlugins.manifestIcon(
                                                runningCard.manifest, Theme.isDark)
                                            fillMode: Image.PreserveAspectFit
                                            smooth: true
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            Layout.alignment: Qt.AlignVCenter
                                            text: runningCard.modelData.label
                                            color: Theme.textPrimary
                                            font.pixelSize: Theme.fontSizeMd
                                            font.bold: true
                                            elide: Text.ElideRight
                                            transform: Translate { y: -2 }
                                        }

                                        IconButton {
                                            Layout.alignment: Qt.AlignVCenter
                                            implicitWidth: 20
                                            implicitHeight: 20
                                            padding: 2
                                            iconSource: iconBaseUrl + (Theme.isDark ? "square-menu-dark.png" : "square-menu.png")
                                            tooltipText: qsTr("Stop")
                                            onClicked: commandExecutor.stopCommand(
                                                runningCard.modelData.projectId + "|" + runningCard.modelData.commandId)
                                        }
                                    }

                                    Label {
                                        width: parent.width
                                        text: runningCard.modelData.projectName
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontSizeSm
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        width: parent.width
                                        text: dashboard.elapsedText(runningCard.modelData.startedAt)
                                        color: "#22c55e"
                                        font.pixelSize: Theme.fontSizeSm
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Empty state: nothing recent and nothing running
            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width * 0.7, 420)
                spacing: Theme.spacingSm
                visible: dashboard.activityEmpty

                Label {
                    width: parent.width
                    text: qsTr("No commands yet")
                    color: Theme.textPrimary
                    font.pixelSize: 16
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    width: parent.width
                    text: qsTr("Run a command from a project — it will appear here.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeMd
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
            }

            // Folder page: manifest sections of the selected folder, each
            // with its command cards, plus the custom-commands section.
            Item {
                id: folderPage
                objectName: "folderPage"
                anchors.fill: parent
                enabled: dashboard.activePage === "folder"
                visible: opacity > 0
                opacity: dashboard.activePage === "folder" ? 1 : 0
                x: dashboard.activePage === "folder" ? 0 : Theme.spacingXl
                Behavior on opacity { NumberAnimation { duration: Theme.animMedium; easing.type: Theme.easingStandard } }
                Behavior on x { NumberAnimation { duration: Theme.animSlide; easing.type: Theme.easingStandard } }

                readonly property string folderPath: (projectService.activeFolder
                    && projectService.activeFolder.path) || ""
                // Current branch of the folder; tracks the model's
                // refresh counter since plain calls have no tracking.
                readonly property string folderBranch: {
                    treeModel.gitBranchesVersion
                    return folderPage.folderPath !== ""
                        ? treeModel.gitBranchForPath(folderPage.folderPath) : ""
                }
                // Manifest projects, excluding the custom-commands pseudo-project
                readonly property var manifestSections: {
                    var out = []
                    var list = projectService.activeFolderProjects
                    for (var i = 0; i < list.length; ++i) {
                        if (list[i] && list[i].manifest !== "custom")
                            out.push(list[i])
                    }
                    return out
                }
                readonly property var customSection: {
                    var list = projectService.activeFolderProjects
                    for (var i = 0; i < list.length; ++i) {
                        if (list[i] && list[i].manifest === "custom")
                            return list[i]
                    }
                    return null
                }
                readonly property var customCommands: folderPage.customSection
                    ? (folderPage.customSection.commands || []) : []

                ScrollView {
                    id: folderScroll
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingLg
                    anchors.rightMargin: Theme.spacingLg
                    anchors.topMargin: Theme.spacingMd
                    anchors.bottomMargin: Theme.spacingLg
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: folderScroll.availableWidth
                        spacing: Theme.spacingXl

                        // Current git branch of the folder, when known.
                        StatCard {
                            visible: Settings.showGitBranch && folderPage.folderBranch !== ""
                            title: qsTr("Git branch")
                            value: folderPage.folderBranch
                            sub: qsTr("Current branch")
                        }

                        Repeater {
                            model: folderPage.manifestSections

                            delegate: ColumnLayout {
                                required property var modelData
                                property string sectionId: modelData.id || ""
                                property var sectionCommands: modelData.commands || []
                                spacing: Theme.spacingSm
                                Layout.fillWidth: true
                                // Inner padding: hover glow rings stick out 3px,
                                // keep them off the scroll viewport edge.
                                Layout.leftMargin: 4
                                Layout.rightMargin: 4

                                // Section header: [icon] name ——— count
                                RowLayout {
                                    spacing: Theme.spacingSm
                                    Layout.fillWidth: true

                                    Image {
                                        // Sized by height from the painted aspect:
                                        // the icon runs slightly above and below
                                        // the text, which stays centered in it.
                                        Layout.preferredWidth: Math.round(
                                            ToolPlugins.manifestHeaderHeight(modelData.manifest || "")
                                            * ToolPlugins.manifestAspect(modelData.manifest || ""))
                                        Layout.preferredHeight: ToolPlugins.manifestHeaderHeight(
                                            modelData.manifest || "")
                                        Layout.alignment: Qt.AlignVCenter
                                        source: iconBaseUrl + ToolPlugins.manifestIcon(
                                            modelData.manifest || "", Theme.isDark)
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                    }

                                    Label {
                                        text: modelData.name || modelData.manifest || ""
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontSizeMd
                                        font.bold: true
                                        elide: Text.ElideRight
                                        Layout.alignment: Qt.AlignVCenter
                                        // Optical centering vs the icon (integer
                                        // only: fractional y blurs the text)
                                        transform: Translate { y: -2 }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 1
                                        Layout.alignment: Qt.AlignVCenter
                                        color: Theme.border
                                    }

                                    Label {
                                        text: String(sectionCommands.length)
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontSizeSm
                                        Layout.alignment: Qt.AlignVCenter
                                    }
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: Math.max(1, Math.min(4,
                                        Math.floor((folderPage.width - 2 * Theme.spacingLg + 8) / 258)))
                                    columnSpacing: Theme.spacingSm
                                    rowSpacing: Theme.spacingSm
                                    uniformCellWidths: true
                                    uniformCellHeights: true

                                    Repeater {
                                        model: sectionCommands

                                        delegate: CommandCard {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            projectId: sectionId
                                            command: modelData
                                        }
                                    }
                                }
                            }
                        }

                        // Custom commands: always present, splash when empty
                        ColumnLayout {
                            spacing: Theme.spacingSm
                            Layout.fillWidth: true
                            Layout.leftMargin: 4
                            Layout.rightMargin: 4

                            RowLayout {
                                spacing: Theme.spacingSm
                                Layout.fillWidth: true

                                Image {
                                    Layout.preferredWidth: Math.round(
                                        ToolPlugins.manifestHeaderHeight("custom")
                                        * ToolPlugins.manifestAspect("custom"))
                                    Layout.preferredHeight: ToolPlugins.manifestHeaderHeight("custom")
                                    Layout.alignment: Qt.AlignVCenter
                                    source: iconBaseUrl + ToolPlugins.manifestIcon("custom", Theme.isDark)
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                }

                                Label {
                                    text: qsTr("Custom commands")
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeMd
                                    font.bold: true
                                    elide: Text.ElideRight
                                    Layout.alignment: Qt.AlignVCenter
                                    transform: Translate { y: -2 }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 1
                                    Layout.alignment: Qt.AlignVCenter
                                    color: Theme.border
                                }

                                Label {
                                    text: String(folderPage.customCommands.length)
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeSm
                                    Layout.alignment: Qt.AlignVCenter
                                }
                            }

                            GridLayout {
                                visible: folderPage.customCommands.length > 0
                                Layout.fillWidth: true
                                columns: Math.max(1, Math.min(4,
                                    Math.floor((folderPage.width - 2 * Theme.spacingLg + 8) / 258)))
                                columnSpacing: Theme.spacingSm
                                rowSpacing: Theme.spacingSm
                                uniformCellWidths: true
                                uniformCellHeights: true

                                Repeater {
                                    model: folderPage.customSection
                                        ? (folderPage.customSection.commands || []) : []
                                    delegate: CommandCard {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        projectId: folderPage.customSection
                                            ? (folderPage.customSection.id || "") : ""
                                        command: modelData
                                    }
                                }
                            }

                            // Splash with the Add button when nothing custom yet
                            Column {
                                visible: folderPage.customCommands.length === 0
                                Layout.fillWidth: true
                                Layout.topMargin: 18
                                spacing: Theme.spacingSm

                                Label {
                                    width: parent.width
                                    text: qsTr("No custom commands yet")
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeMd
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                Label {
                                    width: parent.width
                                    text: qsTr("Add one-off launchers for this folder — any executable, any args.")
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeSm
                                    wrapMode: Text.WordWrap
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                Button {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 200
                                    height: 34
                                    font.pixelSize: 12

                                    background: Rectangle {
                                        radius: 6
                                        color: Theme.primary
                                    }

                                    contentItem: Label {
                                        text: qsTr("Add custom command")
                                        color: Theme.primaryForeground
                                        font.pixelSize: 12
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    onClicked: dashboard.addCustomRequested(folderPage.folderPath)
                                }
                            }

                            // Bottom breathing room: last-row glow rings stick
                            // out 3px below the content.
                            Item {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 4
                            }
                        }
                    }
                }
            }

            // Command detail page: same cross-slide transition as the rest.
            Item {
                id: detailPageWrap
                objectName: "detailPageWrap"
                anchors.fill: parent
                enabled: dashboard.activePage === "detail"
                visible: opacity > 0
                opacity: dashboard.activePage === "detail" ? 1 : 0
                x: dashboard.activePage === "detail" ? 0 : Theme.spacingXl
                Behavior on opacity { NumberAnimation { duration: Theme.animMedium; easing.type: Theme.easingStandard } }
                Behavior on x { NumberAnimation { duration: Theme.animSlide; easing.type: Theme.easingStandard } }

                DetailPage {
                    id: detailPage
                    objectName: "detailPage"
                    anchors.fill: parent
                    commandId: dashboard.detailCommandId

                    onBackRequested: dashboard.closeDetail()
                    onClearRequested: dashboard.clearSelectedCommand()
                }
            }

            // Databases page: local database services.
            Item {
                id: databasesPageWrap
                objectName: "databasesPageWrap"
                anchors.fill: parent
                enabled: dashboard.activePage === "databases"
                visible: opacity > 0
                opacity: dashboard.activePage === "databases" ? 1 : 0
                x: dashboard.activePage === "databases" ? 0 : Theme.spacingXl
                Behavior on opacity { NumberAnimation { duration: Theme.animMedium; easing.type: Theme.easingStandard } }
                Behavior on x { NumberAnimation { duration: Theme.animSlide; easing.type: Theme.easingStandard } }

                DatabasesPage {
                    id: databasesPage
                    anchors.fill: parent
                }
            }

            // Docker page: engine status, containers and images.
            Item {
                id: dockerPageWrap
                objectName: "dockerPageWrap"
                anchors.fill: parent
                enabled: dashboard.activePage === "docker"
                visible: opacity > 0
                opacity: dashboard.activePage === "docker" ? 1 : 0
                x: dashboard.activePage === "docker" ? 0 : Theme.spacingXl
                Behavior on opacity { NumberAnimation { duration: Theme.animMedium; easing.type: Theme.easingStandard } }
                Behavior on x { NumberAnimation { duration: Theme.animSlide; easing.type: Theme.easingStandard } }

                DockerPage {
                    id: dockerPage
                    anchors.fill: parent
                    pageActive: dashboard.activePage === "docker"
                }
            }
        }
    }
}
