import QtQuick
import QtQuick.Controls 6.5
import QtQuick.Layouts 1.5
import "ToolPlugins.js" as ToolPlugins
import "Ui"
pragma ComponentBehavior: Bound

Rectangle {
    id: dashboard
    readonly property var sysStats: systemStats // qmllint disable unqualified
    readonly property var upd: updater // qmllint disable unqualified
    readonly property var projSvc: projectService // qmllint disable unqualified
    readonly property var exec: commandExecutor // qmllint disable unqualified
    readonly property var cmdLog: commandLog // qmllint disable unqualified
    readonly property var tm: treeModel // qmllint disable unqualified
    readonly property var settings: Settings // qmllint disable unqualified
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
    readonly property var stats: (typeof sysStats !== "undefined") ? sysStats : null

    // Updater presence (absent in tests that load this file standalone).
    readonly property bool hasUpdater: typeof updater !== "undefined"
    readonly property bool hasUpdate: dashboard.hasUpdater && upd.updateAvailable
    readonly property string updateVersion: dashboard.hasUpdater ? upd.latestVersion : ""

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
    readonly property var recents: projSvc.recentCommands
    readonly property var running: projSvc.runningCommands
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

    // Framework console records (Symfony/Laravel discovery output): the
    // categories that get the grouped bin/console section on folder pages.
    // Categories arrive as stored ("Symfony") or bare ("symfony").
    function isConsoleCategory(category) {
        if (category === undefined || category === null)
            return false
        var c = category.toString().toLowerCase()
        return c === "symfony" || c === "laravel"
    }

    function isConsoleRecord(cmd) {
        return cmd !== undefined && cmd !== null && isConsoleCategory(cmd.category)
    }

    // Owning project id for a bare command id: the active project first,
    // then every manifest project of the open folder (folder-page cards
    // belong to projects that were never selected). "" when unknown.
    function projectIdForCommand(bareId) {
        if (!bareId)
            return ""
        var id = bareId.toString()
        if (id.indexOf("|") >= 0)
            return id.slice(0, id.lastIndexOf("|"))
        var cmds = projSvc.activeProjectCommands
        for (var i = 0; i < cmds.length; ++i) {
            if (cmds[i] && cmds[i].id === id)
                return projSvc.activeProject.id || ""
        }
        var folder = projSvc.activeFolderProjects
        for (var f = 0; f < folder.length; ++f) {
            var p = folder[f]
            var list = (p && p.commands) || []
            for (var j = 0; j < list.length; ++j) {
                if (list[j] && list[j].id === id)
                    return (p && p.id) ? p.id : ""
            }
        }
        return ""
    }

    // A console run gated on missing required args ({commandId, label,
    // missing:[]}). Set by the framework cards and the C++ runBlocked
    // signal; the folder-page alert renders it until dismissed.
    property var blockedRun: ({})

    function reportRunBlocked(commandId, missing, label) {
        var found = commandByKey(commandId)
        blockedRun = { commandId: commandId || "",
                       label: label || ((found.command && found.command.label) || commandId || ""),
                       missing: missing || [] }
    }

    function clearRunBlocked() {
        blockedRun = ({})
    }

    function openActivity(projectId, commandId) {
        if (projectId && projectId !== "")
            projSvc.selectProject(projectId)
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
            projSvc.runCommandEffective(pid, cid)
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
        var cmds = projSvc.activeProjectCommands
        for (var i = 0; i < cmds.length; ++i) {
            if (cmds[i] && cmds[i].id === bare
                && (pid === "" || (projSvc.activeProject.id || "") === pid))
                return { command: cmds[i],
                         manifest: (projSvc.activeProject
                                    && projSvc.activeProject.manifest) || "" }
        }
        var folder = projSvc.activeFolderProjects
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
        var occupant = exec.pidOnPort(port)
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

    readonly property var activeProject: projSvc.activeProject
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
        : ((projSvc.activeFolder && projSvc.activeFolder.path) || "")
    readonly property var folderSegments: dashboard.contextFolderPath !== ""
        ? projSvc.folderCrumbs(dashboard.contextFolderPath) : []
    readonly property bool showFolderCrumbs: dashboard.folderSegments.length > 0
        && (activePage === "folder" || activePage === "detail")

    function goHome() {
        dashboard.closeDetail()
        dashboard.activePage = "dashboard"
    }

    function openFolder(folderPath) {
        dashboard.closeDetail()
        if (folderPath !== undefined && folderPath !== "")
            projSvc.selectFolder(folderPath)
        dashboard.activePage = "folder"
    }

    function openDatabases() {
        dashboard.closeDetail()
        // The sidebar entry navigates to the list level, so re-entering from
        // a detail view must drop the current service selection.
        databasesPage.selectedIndex = -1
        dashboard.activePage = "databases"
    }

    function openDocker() {
        dashboard.closeDetail()
        dashboard.activePage = "docker"
    }

    function openApi() {
        dashboard.closeDetail()
        dashboard.activePage = "api"
    }

    Connections {
        target: dashboard.projSvc
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
        return (projSvc.activeProject.id || "") + "|" + cmdId
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
        // Marked unconditionally: this function only runs when the window
        // appears / the theme changes, by which time activePage is not
        // necessarily "databases", so a visible-guard would never mark the
        // crumb in time.
        if (crumbDatabasesLabel)
            controls.push(crumbDatabasesLabel)
        // API crumb: marks the request-page back path (its MouseArea only
        // appears while a request is open, but the label must be registered
        // unconditionally for the same reason as the databases one).
        if (crumbApiLabel)
            controls.push(crumbApiLabel)
        // Request-page Save tail (R6): the button lives in the title-bar
        // strip too, so without this the native drag region swallows its
        // click (titleBar in main.qml sits above the whole header). Marked
        // unconditionally — the hit test re-reads visibility and geometry on
        // every press, so registering it while hidden is fine.
        if (headerSaveButton)
            controls.push(headerSaveButton)
        for (var j = 0; j < controls.length; ++j) {
            if (controls[j])
                agent.setHitTestVisible(controls[j], true)
            else
                console.warn("markHitTest: breadcrumb control " + j + " is null")
        }
    }

    // Run history per full run key: [{seq, startedAt, exitCode}] with
    // exitCode null/undefined while the run is live. The console shows
    // one run segment at a time; runSelection maps a key to the visible
    // seq (the latest one by default).
    property var commandRuns: ({})
    property var runSelection: ({})

    function runsFor(cmdId) {
        return commandRuns[fullKey(cmdId)] || []
    }

    function latestRunSeq(cmdId) {
        var runs = runsFor(cmdId)
        return runs.length > 0 ? runs[runs.length - 1].seq : 0
    }

    function selectedRunSeq(cmdId) {
        var key = fullKey(cmdId)
        if (key === "") return 0
        var sel = runSelection[key]
        return sel === undefined ? latestRunSeq(key) : sel
    }

    function runStartMs(cmdId, seq) {
        var key = fullKey(cmdId)
        var runs = commandRuns[key] || []
        for (var i = 0; i < runs.length; ++i) {
            if (runs[i].seq === seq)
                return runs[i].startedAt
        }
        // No segment yet (pre-run lines only): anchor to the first of
        // them so the gutter counts from visible output, not the epoch.
        var h = commandHistories[key] || []
        for (var j = 0; j < h.length; ++j) {
            if (h[j].run === seq || h[j].run === 0)
                return h[j].at
        }
        return Date.now()
    }

    // Selector entry: "#1 · 13:31:50 · exit 0" (or "· running" live).
    function runLabel(run) {
        var t = new Date(run.startedAt)
        function pad(n) { return ("0" + n).slice(-2) }
        var state = (run.exitCode === null || run.exitCode === undefined)
            ? qsTr("running") : qsTr("exit %1").arg(run.exitCode)
        return "#" + run.seq + " · " + pad(t.getHours()) + ":"
            + pad(t.getMinutes()) + ":" + pad(t.getSeconds()) + " · " + state
    }

    function runMenu(cmdId) {
        var out = []
        var runs = runsFor(cmdId)
        for (var i = 0; i < runs.length; ++i)
            out.push({ seq: runs[i].seq, label: runLabel(runs[i]) })
        return out
    }

    // Lines of one run segment (plus pre-run system lines, run 0, which
    // belong to no segment but must not vanish from any view).
    function runSlice(key, seq) {
        var out = []
        var h = commandHistories[key] || []
        for (var i = 0; i < h.length; ++i) {
            if (h[i].run === seq || h[i].run === 0)
                out.push(h[i])
        }
        return out
    }

    // Switches the console to one run segment (the run selector).
    function showRun(cmdId, seq) {
        var key = fullKey(cmdId)
        if (key === "") return
        var sel = runSelection
        sel[key] = seq
        runSelection = sel
        refillRun(key, seq)
    }

    // Refills the visible console with one segment. Only the selected
    // command has anything on screen: background runs just grow history.
    function refillRun(key, seq) {
        if (key === "") return
        selectedCommandId = key
        cmdLog.clear()
        // One C++ call: addBatch. Re-filling row by row (add() per line)
        // fires N signal storms and makes DetailPage re-render its RichText
        // console N times — a UI freeze on open for long histories.
        cmdLog.addBatch(runSlice(key, seq))
    }

    // A launch observed from the executor: opens a new run segment, echoes
    // the full command line, and switches the console to it.
    function startRun(cmdId, label, cmdline) {
        var key = fullKey(cmdId)
        var runs = runsFor(key).slice()
        var seq = runs.length > 0 ? runs[runs.length - 1].seq + 1 : 1
        runs.push({ seq: seq, startedAt: Date.now(), exitCode: null })
        var all = commandRuns
        all[key] = runs
        commandRuns = all
        var sel = runSelection
        sel[key] = seq
        runSelection = sel
        if (key !== "" && key === selectedCommandId)
            refillRun(key, seq)
        appendRunLine(key, "system", label, cmdline, seq, "echo")
    }

    // A finished launch closes its open segment. Clean exits live on in
    // the run selector ("#N · time · exit M"); only abnormal ends
    // (a tailLine) stay visible as a line.
    function finishRun(cmdId, label, exitCode, tailLine) {
        var key = fullKey(cmdId)
        var runs = runsFor(key).slice()
        var seq = 0
        for (var i = runs.length - 1; i >= 0; --i) {
            if (runs[i].exitCode === null || runs[i].exitCode === undefined) {
                runs[i].exitCode = exitCode
                seq = runs[i].seq
                var all = commandRuns
                all[key] = runs
                commandRuns = all
                break
            }
        }
        if (seq === 0)
            seq = latestRunSeq(key)
        if (tailLine !== undefined && tailLine !== "")
            appendRunLine(key, "system", label, tailLine, seq, "")
    }

    function appendCommandLine(cmdId, level, target, message, kind) {
        var key = fullKey(cmdId)
        appendRunLine(key, level, target, message, latestRunSeq(key), kind || "")
    }

    function appendRunLine(cmdId, level, target, message, run, kind) {
        var key = fullKey(cmdId)
        var at = Date.now()
        var histories = commandHistories
        var h = histories[key] || []
        h.push({level: level, target: target, message: message, at: at, run: run, kind: kind || ""})
        while (h.length > 1000) h.shift()
        histories[key] = h
        commandHistories = histories
        // Sniff the port frameworks print at startup ("Local:
        // http://localhost:5173/", "Listening on ...", "listening on
        // port 8040"). Patterns live in ToolPlugins.js; matching runs on
        // SGR-stripped text.
        var port = ToolPlugins.matchListening(
            ToolPlugins.stripSgr(message))
        if (port > 0) {
            var ports = detectedPorts
            ports[key] = port
            detectedPorts = ports
            // A busy-port report may have arrived before the port was known
            if (pendingBusy[key] !== undefined)
                dashboard.tryKillRun(key, message)
        }
        // Live lines reach the console only for the selected segment: a
        // newer run auto-switches via startRun, an older viewed segment
        // stays put while history keeps growing underneath.
        if (key !== "" && key === selectedCommandId && run === selectedRunSeq(key))
            cmdLog.add(level, target, message, at, kind || "")
    }

    function selectCommand(cmdId) {
        if (cmdId === "") {
            selectedCommandId = ""
            cmdLog.clear()
            return
        }
        // The latest run segment (or everything when nothing ran yet).
        showRun(cmdId, latestRunSeq(cmdId))
    }

    function clearSelectedCommand() {
        if (selectedCommandId === "") return
        var histories = commandHistories
        delete histories[selectedCommandId]
        commandHistories = histories
        var runs = commandRuns
        delete runs[selectedCommandId]
        commandRuns = runs
        var sel = runSelection
        delete sel[selectedCommandId]
        runSelection = sel
        cmdLog.clear()
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
        target: dashboard.projSvc

        function onActiveProjectCommandsChanged() {
            var cmds = dashboard.projSvc.activeProjectCommands
            for (var i = 0; i < cmds.length; ++i) {
                if (dashboard.fullKey(cmds[i].id) === dashboard.selectedCommandId) return
            }
            dashboard.selectCommand(cmds.length > 0 ? cmds[0].id : "")
        }
    }

    // Console run gating + explicit ports (ProjectService signals): the
    // folder-page framework section depends on both.
    Connections {
        target: dashboard.projSvc

        // A run gated on required args never started: surface the names.
        function onRunBlocked(commandId, missing) {
            dashboard.reportRunBlocked(commandId, missing)
        }

        // Explicit `--port <v>` in the launched args of a console run:
        // merged under the same run key as the sniffed ports, so cards
        // and the detail page read one map.
        function onConsoleDetectedPort(commandId, port) {
            var pid = dashboard.projectIdForCommand(commandId)
            if (pid === "" || !commandId)
                return
            var ports = dashboard.detectedPorts
            ports[pid + "|" + commandId] = port
            dashboard.detectedPorts = ports
        }
    }

    Connections {
        target: dashboard.exec

        function onStarted(id, pid, label, cmdline, commandId) {
            dashboard.startRun(commandId || label, label, cmdline)
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
            // Killed runs keep a visible "■ stopped" line; clean exits
            // live on in the run selector ("#N · time · exit M").
            dashboard.finishRun(commandId || label, label, exitCode,
                                exitCode < 0 ? "■ stopped" : "")
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
            dashboard.finishRun(commandId || label, label, -1, "■ failed: " + error)
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
                    id: crumbDatabasesLabel
                    visible: dashboard.activePage === "databases"
                    text: qsTr("Databases")
                    color: dashboard.activePage === "databases" && databasesPage.selectedService !== null
                        ? Theme.textMuted : Theme.textPrimary
                    font.pixelSize: Theme.fontSizeLg

                    MouseArea {
                        anchors.fill: parent
                        visible: databasesPage.selectedService !== null
                        cursorShape: Qt.PointingHandCursor
                        onClicked: databasesPage.selectedIndex = -1
                    }
                }

                // Selected-service crumb: DatabasesPage's own list/detail nav
                // is one level deeper than `activePage`, so it gets its own
                // conditional segment here rather than a duplicate in-page
                // breadcrumb (matches how the folder/detail trail below works).
                Label {
                    visible: dashboard.activePage === "databases" && databasesPage.selectedService !== null
                    text: "/"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeLg
                }

                Label {
                    visible: dashboard.activePage === "databases" && databasesPage.selectedService !== null
                    text: databasesPage.selectedService ? databasesPage.titleText(databasesPage.selectedService) : ""
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeLg
                    elide: Text.ElideRight
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

                Label {
                    visible: dashboard.activePage === "api"
                    text: "/"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeLg
                }

                Label {
                    id: crumbApiLabel
                    visible: dashboard.activePage === "api"
                    text: qsTr("API")
                    color: dashboard.activePage === "api" && apiPage.requestOpen
                        ? Theme.textMuted : Theme.textPrimary
                    font.pixelSize: Theme.fontSizeLg

                    MouseArea {
                        anchors.fill: parent
                        visible: apiPage.requestOpen
                        cursorShape: Qt.PointingHandCursor
                        onClicked: apiPage.closeRequest()
                    }
                }

                // Open-request crumb: the request page is one level deeper
                // than `activePage` (same as DatabasesPage's selected
                // service), so it gets its own conditional segment.
                Label {
                    visible: dashboard.activePage === "api" && apiPage.requestOpen
                    text: "/"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeLg
                }

                Label {
                    visible: dashboard.activePage === "api" && apiPage.requestOpen
                    text: apiPage.requestName
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeLg
                    elide: Text.ElideRight
                }

                // Folder crumbs (folder page and detail alike). Segments
                // navigate back to their folder; the manifest/command tail
                // on detail pages names the current location.
                Repeater {
                    id: segRepeater
                    model: dashboard.showFolderCrumbs ? dashboard.folderSegments : []

                    delegate: RowLayout {
                        id: crumbDelegate
                        required property var modelData
                        required property int index
                        spacing: Theme.spacingXs

                        Label {
                            text: "/"
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeLg
                        }

                        Label {
                            text: crumbDelegate.modelData.name || ""
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeLg
                            elide: Text.ElideRight

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: dashboard.openFolder(crumbDelegate.modelData.path || "")
                            }
                        }

                        // Per-folder git branch tag, shown next to each folder crumb.
                        // Reads gitBranchesVersion so it refreshes when any cached
                        // branch changes (a plain function call has no change
                        // tracking of its own).
                        UiTag {
                            readonly property string branchName: {
                                tm.gitBranchesVersion
                                return tm.gitBranchForPath(crumbDelegate.modelData.path || "")
                            }
                            visible: settings.showGitBranch && branchName !== ""
                            label: branchName
                            color: "primary"
                            variant: "subtle"
                            size: "sm"
                            iconSource: Theme.icon("git-branch")
                            Layout.alignment: Qt.AlignVCenter
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

                // Right tail of the breadcrumb row: the request page's
                // "unsaved changes · Save" (the design's request row was
                // ← Back · crumbs · unsaved changes · Save — the crumbs and
                // the tail both live in the header). Visible only while the
                // API page is active and there is something to save; drafts
                // are always unsaved, a saved request only after an edit.
                RowLayout {
                    objectName: "headerSaveTail"
                    visible: dashboard.activePage === "api" && apiPage.requestUnsaved
                    spacing: Theme.spacingSm
                    Layout.alignment: Qt.AlignVCenter

                    Label {
                        objectName: "headerUnsavedLabel"
                        text: qsTr("unsaved changes")
                        color: Theme.textSubtle
                        font.pixelSize: Theme.fontSizeSm
                    }

                    UiButton {
                        id: headerSaveButton
                        objectName: "headerSaveButton"
                        label: apiPage.requestSaveLabel
                        color: "neutral"
                        variant: "outline"
                        size: "sm"
                        leadingIconSource: Theme.icon("save")
                        onClicked: apiPage.headerSave()
                    }
                }
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
                iconSource: Theme.icon("refresh")
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
                            iconSource: Theme.icon("run-all")
                            tooltipText: qsTr("Run all commands")
                            onClicked: dashboard.runAllRecent()
                        }
                    }

                    Flow {
                        width: parent.width
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
                                            source: Theme.icon(ToolPlugins.manifestIcon(
                                                recentCard.manifest))
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
                                    color: Theme.success
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
                            Layout.preferredWidth: 8
                            Layout.preferredHeight: 8
                            radius: 4
                            color: Theme.success
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
                            iconSource: Theme.icon("stop-all")
                            tooltipText: qsTr("Stop all commands")
                            onClicked: dashboard.exec.killAll()
                        }
                    }

                    Flow {
                        width: parent.width
                        spacing: Theme.spacingSm

                        Repeater {
                            model: dashboard.projSvc.runningCommands

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
                                            source: Theme.icon(ToolPlugins.manifestIcon(
                                                runningCard.manifest))
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
                                            iconSource: Theme.icon("stop")
                                            tooltipText: qsTr("Stop")
                                            onClicked: dashboard.exec.stopCommand(
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
                                        color: Theme.success
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

                readonly property string folderPath: (dashboard.projSvc.activeFolder
                    && dashboard.projSvc.activeFolder.path) || ""
                // Current branch of the folder; tracks the model's
                // refresh counter since plain calls have no tracking.
                readonly property string folderBranch: {
                    dashboard.tm.gitBranchesVersion
                    return folderPage.folderPath !== ""
                        ? dashboard.tm.gitBranchForPath(folderPage.folderPath) : ""
                }
                // Manifest projects, excluding the custom-commands pseudo-project
                readonly property var manifestSections: {
                    var out = []
                    var list = dashboard.projSvc.activeFolderProjects
                    for (var i = 0; i < list.length; ++i) {
                        if (list[i] && list[i].manifest !== "custom")
                            out.push(list[i])
                    }
                    return out
                }
                readonly property var customSection: {
                    var list = dashboard.projSvc.activeFolderProjects
                    for (var i = 0; i < list.length; ++i) {
                        if (list[i] && list[i].manifest === "custom")
                            return list[i]
                    }
                    return null
                }
                readonly property var customCommands: folderPage.customSection
                    ? (folderPage.customSection.commands || []) : []

                // Console discovery finishes with projectsChanged only (no
                // folder signal), and activeFolderProjects is a snapshot:
                // a fresh stamp re-reads the live project list, which the
                // projects() getter serves current. Drives the console
                // section's loading -> filled transition.
                readonly property var projectsStamp: {
                    var _ = dashboard.projSvc.projectCount // NOTIFY projectsChanged
                    return { at: Date.now() }
                }

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

                        Repeater {
                            model: folderPage.manifestSections

                            delegate: ColumnLayout {
                                id: sectionDelegate
                                required property var modelData
                                property string sectionId: modelData.id || ""
                                // Manifest commands for the generic card grid:
                                // framework console records get their own
                                // grouped section below, never generic cards.
                                property var sectionCommands: {
                                    var all = modelData.commands || []
                                    var out = []
                                    for (var i = 0; i < all.length; ++i) {
                                        if (all[i] && dashboard.isConsoleRecord(all[i]))
                                            continue
                                        out.push(all[i])
                                    }
                                    return out
                                }
                                // Console records present, or discovery still
                                // running (header shows "loading…" meanwhile).
                                property bool hasConsole: {
                                    if (modelData.consoleLoading === true)
                                        return true
                                    var all = modelData.commands || []
                                    for (var i = 0; i < all.length; ++i) {
                                        if (all[i] && dashboard.isConsoleRecord(all[i]))
                                            return true
                                    }
                                    return false
                                }
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
                                            ToolPlugins.manifestHeaderHeight(sectionDelegate.modelData.manifest || "")
                                            * ToolPlugins.manifestAspect(sectionDelegate.modelData.manifest || ""))
                                        Layout.preferredHeight: ToolPlugins.manifestHeaderHeight(
                                            sectionDelegate.modelData.manifest || "")
                                        Layout.alignment: Qt.AlignVCenter
                                        source: Theme.icon(ToolPlugins.manifestIcon(
                                            sectionDelegate.modelData.manifest || ""))
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                    }

                                    Label {
                                        text: sectionDelegate.modelData.name || sectionDelegate.modelData.manifest || ""
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
                                        text: String(sectionDelegate.sectionCommands.length)
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
                                        model: sectionDelegate.sectionCommands

                                        delegate: CommandCard {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            projectId: sectionDelegate.sectionId
                                            command: modelData
                                        }
                                    }
                                }

                                // Framework console section (Symfony/Laravel):
                                // right below its own manifest cards, grouped by
                                // namespace. Live project data — the folder
                                // snapshot misses a discovery that finishes later.
                                ColumnLayout {
                                    id: consoleSection
                                    visible: sectionDelegate.hasConsole
                                             || consoleSection.loading
                                             || consoleSection.consoleCommands.length > 0
                                    Layout.fillWidth: true
                                    // Top margin up to the between-section gap:
                                    // delegate spacing (spacingSm) + this (spacingLg)
                                    // = spacingXl, matching the parent ColumnLayout.
                                    Layout.topMargin: Theme.spacingLg
                                    spacing: Theme.spacingSm

                                    readonly property var liveProject: {
                                        folderPage.projectsStamp
                                        var wanted = sectionDelegate.sectionId
                                        var all = dashboard.projSvc.projects()
                                        for (var i = 0; i < all.length; ++i) {
                                            if (all[i] && all[i].id === wanted)
                                                return all[i]
                                        }
                                        return sectionDelegate.modelData
                                    }
                                    readonly property bool loading:
                                        liveProject.consoleLoading === true
                                    readonly property var cache:
                                        liveProject.consoleCache || ({})
                                    // Records discovery produced; hidden ones
                                    // are never shown.
                                    readonly property var consoleCommands: {
                                        var all = liveProject.commands || []
                                        var out = []
                                        for (var i = 0; i < all.length; ++i) {
                                            var c = all[i]
                                            if (!c || !dashboard.isConsoleRecord(c)
                                                    || c.hidden === true)
                                                continue
                                            out.push(c)
                                        }
                                        return out
                                    }
                                    readonly property string framework: {
                                        var fw = cache.framework
                                        if (fw !== undefined && fw !== null
                                                && fw.toString() !== "")
                                            return fw.toString()
                                        var c = consoleCommands[0]
                                        return (c && c.category) ? c.category.toString() : ""
                                    }
                                    readonly property string frameworkName:
                                        framework === "" ? ""
                                        : framework.charAt(0).toUpperCase()
                                          + framework.slice(1).toLowerCase()
                                    readonly property string version:
                                        (cache.version !== undefined && cache.version !== null)
                                        ? cache.version.toString() : ""
                                    // env default, data-driven: the `env` option of
                                    // the first record that declares one; "—" when
                                    // it carries no default.
                                    readonly property string envDefault: {
                                        var cmds = consoleCommands
                                        for (var i = 0; i < cmds.length; ++i) {
                                            var meta = cmds[i].argsMeta || []
                                            for (var j = 0; j < meta.length; ++j) {
                                                var m = meta[j]
                                                if (!m || m.kind !== "option")
                                                    continue
                                                var n = (m.name || "").toString()
                                                if (n !== "env" && n !== "--env")
                                                    continue
                                                if (m.default !== undefined && m.default !== null
                                                        && m.default.toString() !== "")
                                                    return m.default.toString()
                                                return "—"
                                            }
                                        }
                                        return "—"
                                    }
                                    // No env in the header — it lives in the drawer's
                                    // --env field (envDefault still feeds it).
                                    readonly property string headerText:
                                        frameworkName !== ""
                                            ? qsTr("%1 console").arg(frameworkName)
                                            : (liveProject.name || "")
                                    // Meta tail of the header: "bin/console ·
                                    // Symfony 7.1 · env dev" — binary from the
                                    // first record, framework + version and the
                                    // default --env.
                                    readonly property string metaText: {
                                        var parts = []
                                        var binary = consoleCommands.length > 0
                                            ? (consoleCommands[0].command || "").toString() : ""
                                        if (binary !== "")
                                            parts.push(binary)
                                        if (frameworkName !== "")
                                            parts.push(frameworkName
                                                       + (version !== "" ? " " + version : ""))
                                        if (envDefault !== "—")
                                            parts.push(qsTr("env") + " " + envDefault)
                                        return parts.join(" · ")
                                    }
                                    // Records discovery produced but marked hidden
                                    // (framework-namespace commands folded away).
                                    readonly property int hiddenCount: {
                                        var all = liveProject.commands || []
                                        var n = 0
                                        for (var i = 0; i < all.length; ++i) {
                                            var c = all[i]
                                            if (c && dashboard.isConsoleRecord(c)
                                                    && c.hidden === true)
                                                n++
                                        }
                                        return n
                                    }
                                    readonly property string countText: loading
                                        ? qsTr("loading…")
                                        : (hiddenCount > 0
                                           ? qsTr("%1 · +%2 framework").arg(visibleCommands.length).arg(hiddenCount)
                                           : qsTr("%1 commands").arg(visibleCommands.length))
                                    // Name filter (case-insensitive substring
                                    // of the label) plus namespace toggles:
                                    // "All" + every distinct label prefix up to
                                    // the first colon (app, messenger, …).
                                    property string filterText: ""
                                    property var userNs: undefined
                                    readonly property var namespaces: {
                                        var seen = []
                                        var out = []
                                        for (var i = 0; i < consoleCommands.length; ++i) {
                                            var name = (consoleCommands[i].label || "").toString()
                                            var colon = name.indexOf(":")
                                            if (colon < 0)
                                                continue
                                            var ns = name.slice(0, colon)
                                            if (seen.indexOf(ns) < 0) {
                                                seen.push(ns)
                                                out.push(ns)
                                            }
                                        }
                                        return out
                                    }
                                    readonly property bool appPresent:
                                        namespaces.indexOf("app") >= 0
                                    // Effective selection: the user's choice, or
                                    // the app group by default (All when no app).
                                    readonly property var selectedNs: userNs === undefined
                                        ? (appPresent ? ["app"] : [])
                                        : userNs
                                    function toggleNs(ns) {
                                        var base = selectedNs
                                        if (base.indexOf(ns) >= 0)
                                            userNs = base.filter(function (x) { return x !== ns })
                                        else
                                            userNs = base.concat([ns])
                                    }
                                    // Commands passing the filter; commands with
                                    // no namespace prefix always show.
                                    readonly property var visibleCommands: {
                                        var q = (filterText || "").trim().toLowerCase()
                                        var out = []
                                        for (var i = 0; i < consoleCommands.length; ++i) {
                                            var c = consoleCommands[i]
                                            var label = (c.label || "").toString()
                                            if (q !== "" && label.toLowerCase().indexOf(q) < 0)
                                                continue
                                            var colon = label.indexOf(":")
                                            var ns = colon < 0 ? "" : label.slice(0, colon)
                                            if (ns !== "" && selectedNs.length > 0
                                                    && selectedNs.indexOf(ns) < 0)
                                                continue
                                            out.push(c)
                                        }
                                        return out
                                    }
                                    // The blocked-run alert belongs to this
                                    // section when its command lives here.
                                    readonly property bool blockedHere: {
                                        var b = dashboard.blockedRun
                                        if (!b || !b.commandId)
                                            return false
                                        for (var i = 0; i < consoleCommands.length; ++i) {
                                            if (consoleCommands[i].id === b.commandId)
                                                return true
                                        }
                                        return false
                                    }

                                    // Header: [framework icon] <Framework> <version>
                                    // — count (no binary prefix, no env)
                                    RowLayout {
                                        spacing: Theme.spacingSm
                                        Layout.fillWidth: true

                                        Image {
                                            id: consoleFrameworkIcon
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            Layout.alignment: Qt.AlignVCenter
                                            fillMode: Image.PreserveAspectFit
                                            smooth: true
                                            // qrc first (AGENTS.md); builds that ship
                                            // the icon directory next to the binary
                                            // fall back to it when no resource is
                                            // compiled in (file:// icons elsewhere).
                                            readonly property string iconFile:
                                                consoleSection.framework.toLowerCase() === "laravel"
                                                    ? "laravel.png"
                                                    : (Theme.isDark
                                                       ? "symfony-dark.png" : "symfony.png")
                                            readonly property url qrcSource:
                                                "qrc:/icons/" + iconFile
                                            property bool qrcMissing: false
                                            onStatusChanged: {
                                                if (status === Image.Error && !qrcMissing)
                                                    qrcMissing = true
                                            }
                                            source: qrcMissing
                                                ? (typeof iconBaseUrl !== "undefined"
                                                   ? iconBaseUrl + iconFile : qrcSource)
                                                : qrcSource
                                        }

                                        Label {
                                            text: consoleSection.headerText
                                            color: Theme.textPrimary
                                            font.pixelSize: Theme.fontSizeMd
                                            font.bold: true
                                            elide: Text.ElideRight
                                            Layout.alignment: Qt.AlignVCenter
                                            transform: Translate { y: -2 }
                                        }

                                        // Meta: "bin/console · Symfony 7.1 · env
                                        // dev" — monospace, dimmed (design token,
                                        // not Theme.textMuted).
                                        Label {
                                            visible: consoleSection.metaText !== ""
                                            text: consoleSection.metaText
                                            color: Theme.textDimmed
                                            font.family: "Menlo"
                                            font.pixelSize: Theme.fontSizeSm
                                            elide: Text.ElideRight
                                            Layout.alignment: Qt.AlignVCenter
                                            Layout.maximumWidth: Math.round(
                                                folderPage.width * 0.35)
                                        }

                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 1
                                            Layout.alignment: Qt.AlignVCenter
                                            color: Theme.border
                                        }

                                        Label {
                                            // "9 · +142 framework": the visible
                                            // count in normal text color, the
                                            // hidden-framework tail dimmed.
                                            text: consoleSection.countText
                                            color: Theme.textMuted
                                            font.pixelSize: Theme.fontSizeSm
                                            font.family: "Menlo"
                                            Layout.alignment: Qt.AlignVCenter
                                        }
                                    }

                                    // Filter input + namespace toggle chips on
                                    // one line (design): the input owns a
                                    // fixed 340px column, the chips flow in
                                    // their own column to the right and wrap
                                    // inside it, never under the input.
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.spacingSm

                                        UiInput {
                                            Layout.preferredWidth: 340
                                            Layout.alignment: Qt.AlignVCenter
                                            leadingIconSource: Theme.icon("filter")
                                            placeholderText: qsTr("Filter by name…")
                                            text: consoleSection.filterText
                                            onTextChanged: consoleSection.filterText = text
                                        }

                                        // Chips column. Flow inside a Layout
                                        // does not reliably propagate a
                                        // wrap-height change to the layout, so
                                        // the height is pinned explicitly.
                                        Item {
                                            Layout.fillWidth: true
                                            Layout.alignment: Qt.AlignVCenter
                                            Layout.preferredHeight: chipsFlow.implicitHeight

                                            Flow {
                                                id: chipsFlow
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                spacing: Theme.spacingXs

                                                UiTag {
                                                    label: "All"
                                                    size: "sm"
                                                    color: consoleSection.selectedNs.length === 0
                                                        ? "primary" : "neutral"
                                                    variant: consoleSection.selectedNs.length === 0
                                                        ? "soft" : "subtle"
                                                    MouseArea {
                                                        anchors.fill: parent
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: consoleSection.userNs = []
                                                    }
                                                }

                                                Repeater {
                                                    model: consoleSection.namespaces

                                                    delegate: UiTag {
                                                        required property var modelData
                                                        label: modelData
                                                        size: "sm"
                                                        color: consoleSection.selectedNs.indexOf(modelData) >= 0
                                                            ? "primary" : "neutral"
                                                        variant: consoleSection.selectedNs.indexOf(modelData) >= 0
                                                            ? "soft" : "subtle"
                                                        MouseArea {
                                                            anchors.fill: parent
                                                            cursorShape: Qt.PointingHandCursor
                                                            onClicked: consoleSection.toggleNs(modelData)
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    // Run gated on missing required args (from a
                                    // card click here or the C++ runBlocked signal).
                                    UiAlert {
                                        Layout.fillWidth: true
                                        visible: consoleSection.blockedHere
                                                && (dashboard.blockedRun.missing || []).length > 0
                                        closable: true
                                        color: "warning"
                                        variant: "soft"
                                        title: qsTr("Run blocked: missing required arguments")
                                        description: (dashboard.blockedRun.label || "")
                                            + " — " + (dashboard.blockedRun.missing || []).join(", ")
                                        onCloseRequested: dashboard.clearRunBlocked()
                                    }

                                    GridLayout {
                                        visible: consoleSection.visibleCommands.length > 0
                                        Layout.fillWidth: true
                                        columns: Math.max(1, Math.min(4,
                                            Math.floor((folderPage.width - 2 * Theme.spacingLg + 8) / 258)))
                                        columnSpacing: Theme.spacingSm
                                        rowSpacing: Theme.spacingSm
                                        uniformCellWidths: true
                                        uniformCellHeights: true

                                        Repeater {
                                            model: consoleSection.visibleCommands

                                            delegate: ConsoleCommandCard {
                                                required property var modelData
                                                Layout.fillWidth: true
                                                projectId: sectionDelegate.sectionId
                                                command: modelData
                                            }
                                        }
                                    }

                                    Label {
                                        // Empty state: the filter or the group
                                        // selection hides every command.
                                        visible: consoleSection.visibleCommands.length === 0
                                                 && consoleSection.consoleCommands.length > 0
                                        text: qsTr("No commands match")
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontSizeSm
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
                                    source: Theme.icon(ToolPlugins.manifestIcon("custom"))
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

                                UiButton {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 200
                                    label: qsTr("Add custom command")
                                    color: "primary"
                                    variant: "solid"
                                    size: "sm"
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

            // API page: shell only (T4/T5/T6 fill it). Permanent instance
            // like Docker/Databases; data loading stays lazy via pageActive.
            Item {
                id: apiPageWrap
                objectName: "apiPageWrap"
                anchors.fill: parent
                enabled: dashboard.activePage === "api"
                visible: opacity > 0
                opacity: dashboard.activePage === "api" ? 1 : 0
                x: dashboard.activePage === "api" ? 0 : Theme.spacingXl
                Behavior on opacity { NumberAnimation { duration: Theme.animMedium; easing.type: Theme.easingStandard } }
                Behavior on x { NumberAnimation { duration: Theme.animSlide; easing.type: Theme.easingStandard } }

                ApiPage {
                    id: apiPage
                    anchors.fill: parent
                    pageActive: dashboard.activePage === "api"
                }
            }
        }
    }
}
