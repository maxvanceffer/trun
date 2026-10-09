import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform
import "Ui"
pragma ComponentBehavior: Bound

// Locally installed database services (detected by DatabaseService).
// Scanning runs once at startup and on the Rescan button; it is not periodic.
// Two levels: a services list, and a detail view (ports, per-database
// detail, a read-only-by-default SQL console) reached via a card's Open.
Item {
    id: databasesPage
    objectName: "databasesPage"

    readonly property var dbs: databaseService // qmllint disable unqualified
    readonly property var db: (typeof dbs !== "undefined") ? dbs : null
    readonly property var services: db ? db.services : []
    readonly property bool busy: db ? db.busy : false
    // Full rescan only — distinct from `busy`, which also covers a single
    // start/stop and must not blank the whole list for that.
    readonly property bool scanning: db ? db.scanning : false
    // Id of the service currently being started/stopped, so only that row
    // shows a busy indicator.
    readonly property string controllingId: db ? db.controllingId : ""

    // Header row summaries.
    readonly property int totalEngines: services.length
    readonly property int runningCount:
        services.filter(function(s) { return prop(s, "running", false) }).length
    property var lastScanAt: 0
    property var scanTick: Date.now()
    property string scannedText:
        lastScanAt > 0
            ? qsTr("scanned %1 min ago").arg(Math.max(0, Math.round((scanTick - lastScanAt) / 60000)))
            : qsTr("not scanned yet")

    // Two-level navigation: -1 = list, >=0 = detail of services[index].
    property int selectedIndex: -1
    readonly property var selectedService:
        selectedIndex >= 0 && selectedIndex < services.length ? services[selectedIndex] : null
    readonly property bool serviceRunning: selectedService ? prop(selectedService, "running", false) : false

    // Detail: ports + per-database detail + saved connection config, fetched
    // together whenever the selected service changes.
    property var detail: ({ ports: [], databases: [], databasesError: "", databasesAuthError: false, connection: {} })
    readonly property bool authRequired: detail.databasesAuthError === true
    readonly property bool detailErrorState:
        !authRequired && (detail.databasesError || "") !== ""

    // Query console state.
    property string sqlText: ""
    property string queryError: ""
    property bool queryAuthError: false
    property real queryElapsedMs: 0
    property var queryRows: []
    property var queryColumns: []
    property string selectedDatabase: ""

    property bool testBusy: false
    property bool testShown: false
    property bool testOk: false
    property string testText: ""

    function reload() {
        if (db)
            db.refresh()
    }

    onSelectedIndexChanged: {
        databasesPage.detail = { ports: [], databases: [], databasesError: "", databasesAuthError: false, connection: {} }
        databasesPage.queryRows = []
        databasesPage.queryColumns = []
        databasesPage.queryError = ""
        databasesPage.sqlText = ""
        databasesPage.testShown = false
        connectionDialog.close()
        // Read services[selectedIndex] directly rather than through the
        // selectedService computed property: that binding can still be
        // observed stale (index -1) inside this same-tick change handler.
        if (databasesPage.selectedIndex >= 0
            && databasesPage.selectedIndex < databasesPage.services.length
            && databasesPage.db) {
            var svc = databasesPage.services[databasesPage.selectedIndex]
            if (svc)
                databasesPage.db.fetchDetail(svc.id)
        }
    }

    // Defensive field access: the service map may lack optional keys.
    function prop(obj, key, fallback) {
        if (obj === null || obj === undefined)
            return fallback
        var v = obj[key]
        if (v === undefined || v === null)
            return fallback
        return v
    }

    function iconFor(engine) {
        switch (engine) {
        case "postgresql": return Theme.icon("postgresql")
        case "mysql":      return Theme.icon("mysql")
        case "mongodb":    return Theme.icon("mongodb")
        case "redis":      return Theme.icon("redis")
        default:           return Theme.icon("default")
        }
    }

    function titleText(svc) {
        var version = databasesPage.prop(svc, "version", "")
        var name = databasesPage.prop(svc, "name", "—")
        return version !== "" ? name + " " + version : name
    }

    function metaText(svc) {
        var parts = []
        var port = databasesPage.prop(svc, "port", 0)
        if (port > 0)
            parts.push(":" + String(port))
        var source = databasesPage.prop(svc, "source", "")
        if (source !== "")
            parts.push(source)
        return parts.join(" · ")
    }

    function portText(svc) {
        var p = databasesPage.prop(svc, "port", 0)
        return p > 0 ? String(p) : "—"
    }

    function databasesText(svc) {
        var d = databasesPage.prop(svc, "databases", [])
        if (d.length === 0)
            return "—"
        return d.join(", ")
    }

    function statusText(svc) {
        return databasesPage.prop(svc, "running", false) ? qsTr("Running") : qsTr("Stopped")
    }

    function metricsText(svc) {
        if (!databasesPage.prop(svc, "running", false))
            return "—"
        var cpu = databasesPage.prop(svc, "pcpu", 0)
        var rss = databasesPage.prop(svc, "rss", 0)
        return qsTr("CPU %1 · MEM %2").arg(cpu.toFixed(1) + "%").arg(Math.round(rss) + " MB")
    }

    function ramText(svc) {
        if (!databasesPage.prop(svc, "running", false))
            return "—"
        return Math.round(databasesPage.prop(svc, "rss", 0)) + " MB"
    }

    function cpuText(svc) {
        if (!databasesPage.prop(svc, "running", false))
            return "—"
        return databasesPage.prop(svc, "pcpu", 0).toFixed(1) + "%"
    }

    function uptimeText(svc) {
        var u = databasesPage.prop(svc, "uptime", "")
        return u !== "" ? u : "—"
    }

    function countText(svc) {
        if (!databasesPage.prop(svc, "running", false))
            return "—"
        var count = databasesPage.prop(svc, "dbCount", 0)
        var engine = databasesPage.prop(svc, "engine", "")
        if (engine === "redis")
            return qsTr("%1 keys").arg(count)
        if (engine === "postgresql" || engine === "mysql" || engine === "mongodb")
            return qsTr("%1 dbs").arg(count)
        return "—"
    }

    function connectionSummary(svc) {
        var parts = []
        var eng = databasesPage.prop(svc, "engine", "")
        var ver = databasesPage.prop(svc, "version", "")
        parts.push(eng + (ver !== "" ? "@" + ver : ""))
        var source = databasesPage.prop(svc, "source", "")
        if (source !== "")
            parts.push(source)
        return parts.join(" · ")
    }

    function accountLabel() {
        var user = databasesPage.detail.connection ? databasesPage.detail.connection.user : ""
        return (user && user !== "") ? user : qsTr("system user")
    }

    function resultCells() {
        var cols = databasesPage.queryColumns
        var out = []
        for (var i = 0; i < cols.length; i++)
            out.push({ text: cols[i], header: true })
        for (var r = 0; r < databasesPage.queryRows.length; r++) {
            var row = databasesPage.queryRows[r]
            for (var c = 0; c < cols.length; c++)
                out.push({ text: c < row.length ? String(row[c]) : "", header: false })
        }
        return out
    }

    function resultMetaText() {
        if (databasesPage.queryError !== "")
            return databasesPage.queryError
        return qsTr("%1 rows · %2 ms · %3 columns")
            .arg(databasesPage.queryRows.length)
            .arg(Math.round(databasesPage.queryElapsedMs))
            .arg(databasesPage.queryColumns.length)
    }

    function copyResults() {
        if (!db)
            return
        var lines = []
        lines.push(databasesPage.queryColumns.join("\t"))
        for (var i = 0; i < databasesPage.queryRows.length; i++)
            lines.push(databasesPage.queryRows[i].join("\t"))
        db.copyToClipboard(lines.join("\n"))
    }

    function executeQuery() {
        if (!db || !selectedService)
            return
        var sql = sqlText.trim()
        if (sql === "" || busy)
            return
        queryError = ""
        db.runQuery(selectedService.id, databasesPage.selectedDatabase, sql)
    }

    Connections {
        target: databasesPage.db
        function onServicesChanged() {
            databasesPage.lastScanAt = Date.now()
        }
        function onDetailReady(id, detail) {
            if (!databasesPage.selectedService || databasesPage.selectedService.id !== id)
                return
            databasesPage.detail = detail
            var conn = detail.connection || {}
            var firstDb = (detail.databases && detail.databases.length > 0) ? detail.databases[0].name : ""
            databasesPage.selectedDatabase = conn.database || firstDb || ""
        }
        function onQueryResult(rows, columns, error, authError, elapsedMs) {
            databasesPage.queryRows = rows
            databasesPage.queryColumns = columns
            databasesPage.queryError = error
            databasesPage.queryAuthError = authError
            databasesPage.queryElapsedMs = elapsedMs
        }
        function onConnectionTestResult(id, ok, detailText, error, authError) {
            if (!databasesPage.selectedService || databasesPage.selectedService.id !== id)
                return
            databasesPage.testBusy = false
            databasesPage.testShown = true
            databasesPage.testOk = ok
            databasesPage.testText = ok ? detailText : (error !== "" ? error : qsTr("Connection failed"))
        }
        function onConnectionSaved(id) {
            if (databasesPage.selectedService && databasesPage.selectedService.id === id) {
                connectionDialog.close()
                databasesPage.db.fetchDetail(id)
            }
        }
        function onConnectionForgotten(id) {
            if (databasesPage.selectedService && databasesPage.selectedService.id === id)
                databasesPage.db.fetchDetail(id)
        }
    }

    Timer {
        interval: 30000
        running: true
        onTriggered: databasesPage.scanTick = Date.now()
    }

    // Manual two-level slide (no StackLayout).
    Item {
        anchors.fill: parent
        clip: true

        // Keyboard: Esc pops back to the list, Enter runs the console query.
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Escape && databasesPage.selectedIndex >= 0)
                databasesPage.selectedIndex = -1
            if (event.key === Qt.Key_Return && databasesPage.selectedIndex >= 0)
                databasesPage.executeQuery()
        }

        // List of all services.
        Item {
            id: listView
            width: parent.width
            height: parent.height
            x: databasesPage.selectedIndex < 0 ? 0 : -parent.width
            opacity: databasesPage.selectedIndex < 0 ? 1 : 0
            visible: opacity > 0
            Behavior on x {
                NumberAnimation { duration: Theme.animSlide; easing.type: Theme.easingStandard }
            }
            Behavior on opacity {
                NumberAnimation { duration: Theme.animSlide; easing.type: Theme.easingStandard }
            }

            Column {
                id: listColumn
                anchors.fill: parent
                anchors.margins: Theme.spacingLg
                spacing: Theme.spacingMd

                RowLayout {
                    width: parent.width
                    spacing: Theme.spacingSm

                    Label {
                        text: qsTr("Services")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSm
                        font.bold: true
                    }

                    Label {
                        text: qsTr("%1 engines · %2 running").arg(totalEngines).arg(runningCount)
                        color: Theme.textDimmed
                        font.pixelSize: Theme.fontSizeSm
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.border
                    }

                    Label {
                        text: scannedText
                        color: Theme.textDimmed
                        font.pixelSize: Theme.fontSizeSm
                    }

                    // Rescan: ghost IconButton; disabled while a scan runs.
                    IconButton {
                        id: rescanButton
                        enabled: !databasesPage.busy
                        iconSource: Theme.icon("refresh")
                        tooltipText: qsTr("Rescan")
                        onClicked: databasesPage.reload()
                    }
                }

                // Empty state (no services, not scanning).
                Label {
                    visible: !databasesPage.scanning && databasesPage.services.length === 0
                    text: qsTr("No databases found")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeMd
                }

                // Full-rescan splash only — a single start/stop must not blank the list.
                Item {
                    id: busySplash
                    visible: databasesPage.scanning
                    width: parent.width
                    height: parent.height
                    Column {
                        anchors.centerIn: parent
                        spacing: Theme.spacingSm
                        Item {
                            width: 48
                            height: 48
                            Spinner {
                                width: 48
                                height: 48
                            }
                        }
                        Label {
                            text: qsTr("Scanning for engines")
                            color: Theme.textPrimary
                            font.bold: true
                            font.pixelSize: Theme.fontSizeLg
                        }
                        Label {
                            text: qsTr("Looking through Homebrew services and system launch daemons")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeMd
                        }
                    }
                }

                Repeater {
                    id: servicesRepeater
                    Layout.fillWidth: true
                    model: databasesPage.services

                delegate: ColumnLayout {
                    width: listColumn.width
                    required property var modelData
                    required property int index

                        // Card: hover ring, click opens the detail.
                        UiListElement {
                            id: serviceCard
                            Layout.fillWidth: true
                            Layout.preferredHeight: 58
                            onClicked: databasesPage.selectedIndex = index

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingMd
                                spacing: Theme.spacingMd

                                // 1. Engine icon (32x32).
                                Image {
                                    Layout.preferredWidth: 32
                                    Layout.preferredHeight: 32
                                    Layout.alignment: Qt.AlignVCenter
                                    source: databasesPage.iconFor(databasesPage.prop(modelData, "engine", ""))
                                    sourceSize.width: 64
                                    sourceSize.height: 64
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                }

                                // 2. Identity (230px): name + version·port·source.
                                ColumnLayout {
                                    Layout.preferredWidth: 230
                                    Layout.alignment: Qt.AlignVCenter
                                    spacing: 2

                                    Label {
                                        Layout.fillWidth: true
                                        // Version included: two installs of the same engine
                                        // (e.g. "mysql" and "mysql@8.4") otherwise show as
                                        // identical, indistinguishable rows.
                                        text: databasesPage.titleText(modelData)
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontSizeMd
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: databasesPage.metaText(modelData)
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontSizeSm
                                        elide: Text.ElideRight
                                    }
                                }

                                // 3. Metrics (Menlo, 150px): CPU + RSS.
                                Label {
                                    Layout.preferredWidth: 150
                                    Layout.alignment: Qt.AlignVCenter
                                    text: databasesPage.metricsText(modelData)
                                    color: Theme.textMuted
                                    font.family: "Menlo"
                                    font.pixelSize: Theme.fontSizeSm
                                    elide: Text.ElideRight
                                }

                                // 4. Extra fact (120px): db/key count.
                                Label {
                                    Layout.preferredWidth: 120
                                    Layout.alignment: Qt.AlignVCenter
                                    text: databasesPage.countText(modelData)
                                    color: Theme.textDimmed
                                    font.pixelSize: Theme.fontSizeSm
                                    elide: Text.ElideRight
                                }

                                // 5. Spacer.
                                Item { Layout.fillWidth: true }

                                // 6. Status (96px): dot + label.
                                RowLayout {
                                    Layout.preferredWidth: 96
                                    Layout.alignment: Qt.AlignVCenter
                                    spacing: Theme.spacingSm

                                    Rectangle {
                                        Layout.alignment: Qt.AlignVCenter
                                        Layout.preferredWidth: 8
                                        Layout.preferredHeight: 8
                                        radius: 4
                                        color: databasesPage.prop(modelData, "running", false) ? Theme.success : Theme.error
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: databasesPage.statusText(modelData)
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontSizeSm
                                        elide: Text.ElideRight
                                    }
                                }

                                // 7. Actions (176px, right-aligned): ghost Stop/Start + Open.
                                RowLayout {
                                    Layout.preferredWidth: 176
                                    Layout.alignment: Qt.AlignVCenter
                                    spacing: Theme.spacingSm

                                    Item { Layout.fillWidth: true }

                                    // Stop/Start, or "system · view only".
                                    UiButton {
                                        Layout.alignment: Qt.AlignVCenter
                                        Layout.preferredWidth: 76
                                        visible: databasesPage.prop(modelData, "controllable", false)
                                            && databasesPage.controllingId !== modelData.id
                                        label: databasesPage.prop(modelData, "running", false) ? qsTr("Stop") : qsTr("Start")
                                        color: databasesPage.prop(modelData, "running", false) ? "error" : "secondary"
                                        variant: "outline"
                                        size: "sm"
                                        onClicked: {
                                            if (!databasesPage.db || databasesPage.busy) return
                                            if (databasesPage.prop(modelData, "running", false))
                                                databasesPage.db.stopService(modelData.id)
                                            else
                                                databasesPage.db.startService(modelData.id)
                                        }
                                    }

                                    BusyLabel {
                                        Layout.alignment: Qt.AlignVCenter
                                        Layout.preferredWidth: 76
                                        visible: databasesPage.controllingId === modelData.id
                                        layout: "horizontal"
                                        text: databasesPage.prop(modelData, "running", false) ? qsTr("Stopping") : qsTr("Starting")
                                        textColor: Theme.textMuted
                                    }

                                    Label {
                                        Layout.alignment: Qt.AlignVCenter
                                        Layout.preferredWidth: 76
                                        visible: !databasesPage.prop(modelData, "controllable", false)
                                        text: qsTr("system · view only")
                                        color: Theme.textDimmed
                                        font.pixelSize: Theme.fontSizeSm
                                        horizontalAlignment: Text.AlignHCenter
                                    }

                                    UiButton {
                                        Layout.alignment: Qt.AlignVCenter
                                        Layout.preferredWidth: 76
                                        label: qsTr("Open")
                                        trailingIconSource: Theme.icon("chevron-right")
                                        color: "secondary"
                                        variant: "solid"
                                        size: "sm"
                                        onClicked: databasesPage.selectedIndex = index
                                    }
                                }
                            }
                        }

                        // 8px gap between cards.
                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 8
                        }
                    }
                }

            }
        }

        // Detail of the selected service.
        Item {
            id: detailView
            width: parent.width
            height: parent.height
            x: databasesPage.selectedIndex >= 0 ? 0 : parent.width
            opacity: databasesPage.selectedIndex >= 0 ? 1 : 0
            visible: opacity > 0
            Behavior on x {
                NumberAnimation { duration: Theme.animSlide; easing.type: Theme.easingStandard }
            }
            Behavior on opacity {
                NumberAnimation { duration: Theme.animSlide; easing.type: Theme.easingStandard }
            }

            ScrollView {
                id: detailScroll
                anchors.fill: parent
                anchors.margins: Theme.spacingLg
                clip: true

                Column {
                    width: detailScroll.availableWidth
                    spacing: Theme.spacingMd

                    // Breadcrumb/back live in the app's shared top header
                    // (Dashboard.qml) — it already shows "Dashboard / Databases"
                    // one level up and gets a 3rd segment for the selected
                    // service; no in-page duplicate here. Esc (Keys.onPressed
                    // above) is the keyboard path back.

                    // 5 live metrics, matching the mockup's Status/Port/RAM/CPU/Uptime set.
                    RowLayout {
                        width: parent.width
                        spacing: Theme.spacingSm

                        StatCard {
                            Layout.fillWidth: true
                            title: qsTr("STATUS")
                            value: databasesPage.selectedService ? databasesPage.statusText(databasesPage.selectedService) : "—"
                            sub: databasesPage.selectedService ? databasesPage.prop(databasesPage.selectedService, "source", "") : ""
                            showDot: true
                            dotColor: databasesPage.serviceRunning ? Theme.success : Theme.error
                        }
                        StatCard {
                            Layout.fillWidth: true
                            title: qsTr("PORT")
                            value: databasesPage.selectedService ? databasesPage.portText(databasesPage.selectedService) : "—"
                            sub: "127.0.0.1"
                        }
                        StatCard {
                            Layout.fillWidth: true
                            title: qsTr("RAM")
                            value: databasesPage.selectedService ? databasesPage.ramText(databasesPage.selectedService) : "—"
                            sub: qsTr("resident")
                        }
                        StatCard {
                            Layout.fillWidth: true
                            title: qsTr("CPU")
                            value: databasesPage.selectedService ? databasesPage.cpuText(databasesPage.selectedService) : "—"
                            sub: qsTr("cpu usage")
                        }
                        StatCard {
                            Layout.fillWidth: true
                            title: qsTr("UPTIME")
                            value: databasesPage.selectedService ? databasesPage.uptimeText(databasesPage.selectedService) : "—"
                            sub: qsTr("elapsed")
                        }
                    }

                    // Connection info bar + actions.
                    RowLayout {
                        width: parent.width
                        spacing: Theme.spacingSm

                        Rectangle {
                            // Hugs its own content (matches the design's
                            // self-contained capsule) instead of stretching
                            // across the whole row.
                            Layout.preferredWidth: connInner.implicitWidth + Theme.spacingSm * 2
                            Layout.preferredHeight: connInner.implicitHeight + Theme.spacingSm * 2
                            radius: Theme.radiusMd
                            color: Theme.cardBackground
                            border.color: Theme.border
                            border.width: 1

                            RowLayout {
                                id: connInner
                                anchors.fill: parent
                                anchors.margins: Theme.spacingSm
                                spacing: Theme.spacingSm

                                Label {
                                    text: databasesPage.selectedService ? databasesPage.connectionSummary(databasesPage.selectedService) : ""
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeSm
                                }
                                Label { text: "|"; color: Theme.border; font.pixelSize: Theme.fontSizeSm }
                                Label {
                                    text: "127.0.0.1:" + (databasesPage.selectedService ? databasesPage.portText(databasesPage.selectedService) : "—")
                                    color: Theme.textMuted
                                    font.family: "Menlo"
                                    font.pixelSize: Theme.fontSizeSm
                                }
                                Label { text: "|"; color: Theme.border; font.pixelSize: Theme.fontSizeSm }
                                RowLayout {
                                    spacing: Theme.spacingXs
                                    // Green = last auth attempt with these credentials
                                    // succeeded; red = server rejected the login.
                                    Rectangle {
                                        Layout.alignment: Qt.AlignVCenter
                                        width: 8; height: 8; radius: 4
                                        color: databasesPage.authRequired ? Theme.error : Theme.success
                                    }
                                    Label {
                                        text: qsTr("as %1").arg(databasesPage.accountLabel())
                                            + ((databasesPage.detail.connection && databasesPage.detail.connection.hasSavedPassword) ? qsTr(" · Keychain") : "")
                                            + (databasesPage.authRequired ? qsTr(" · credentials required") : "")
                                        color: databasesPage.authRequired ? Theme.error : Theme.textMuted
                                        font.pixelSize: Theme.fontSizeSm
                                    }
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }

                        UiButton {
                            Layout.preferredWidth: 100
                            label: qsTr("Connection")
                            color: "secondary"
                            variant: "outline"
                            size: "sm"
                            onClicked: connectionDialog.openDialog()
                        }
                        UiButton {
                            Layout.preferredWidth: 90
                            enabled: databasesPage.selectedService !== null
                            label: qsTr("Refresh")
                            color: "secondary"
                            variant: "outline"
                            size: "sm"
                            onClicked: databasesPage.selectedService && databasesPage.db.fetchDetail(databasesPage.selectedService.id)
                        }
                        UiButton {
                            Layout.preferredWidth: 84
                            visible: databasesPage.selectedService
                                && databasesPage.prop(databasesPage.selectedService, "controllable", false)
                            label: databasesPage.serviceRunning ? qsTr("Stop") : qsTr("Start")
                            color: databasesPage.serviceRunning ? "error" : "secondary"
                            variant: "outline"
                            size: "sm"
                            onClicked: {
                                if (!databasesPage.selectedService || !databasesPage.db) return
                                if (databasesPage.serviceRunning)
                                    databasesPage.db.stopService(databasesPage.selectedService.id)
                                else
                                    databasesPage.db.startService(databasesPage.selectedService.id)
                            }
                        }
                    }

                    // ---- No access: engine not running ----------------------------
                    Rectangle {
                        width: parent.width
                        visible: databasesPage.selectedService !== null && !databasesPage.serviceRunning
                        height: 150
                        radius: Theme.radiusMd
                        color: Theme.cardBackground
                        border.color: Theme.border
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: Theme.spacingSm
                            width: parent.width - 40

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 26; height: 26; radius: 13
                                color: "transparent"
                                border.color: Theme.warning
                                border.width: 2
                            }
                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: qsTr("Engine not running")
                                color: Theme.warning
                                font.bold: true
                                font.pixelSize: Theme.fontSizeLg
                            }
                            Label {
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                text: qsTr("Start %1 to read ports, metrics and run queries.")
                                    .arg(databasesPage.selectedService ? databasesPage.prop(databasesPage.selectedService, "name", "") : "")
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSizeSm
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    // ---- Auth required ---------------------------------------------
                    Rectangle {
                        width: parent.width
                        visible: databasesPage.serviceRunning && databasesPage.authRequired
                        height: 170
                        radius: Theme.radiusMd
                        color: Theme.cardBackground
                        border.color: Theme.border
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: Theme.spacingSm
                            width: parent.width - 40

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 26; height: 26; radius: 13
                                color: "transparent"
                                border.color: Theme.primary
                                border.width: 2
                            }
                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: qsTr("Sign in to %1")
                                    .arg(databasesPage.selectedService ? databasesPage.prop(databasesPage.selectedService, "name", "") : "")
                                color: Theme.textPrimary
                                font.bold: true
                                font.pixelSize: Theme.fontSizeLg
                            }
                            Label {
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                text: databasesPage.detail.databasesError || qsTr("Add credentials to read databases and run queries.")
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSizeSm
                                wrapMode: Text.WordWrap
                            }
                            UiButton {
                                anchors.horizontalCenter: parent.horizontalCenter
                                label: qsTr("Set credentials…")
                                color: "primary"
                                variant: "solid"
                                size: "sm"
                                onClicked: connectionDialog.openDialog()
                            }
                        }
                    }

                    // ---- Generic backend/scan error --------------------------------
                    Rectangle {
                        width: parent.width
                        visible: databasesPage.serviceRunning && databasesPage.detailErrorState
                        height: 150
                        radius: Theme.radiusMd
                        color: Theme.cardBackground
                        border.color: Theme.border
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: Theme.spacingSm
                            width: parent.width - 40

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: qsTr("Couldn't read database detail")
                                color: Theme.error
                                font.bold: true
                                font.pixelSize: Theme.fontSizeLg
                            }
                            Label {
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                text: databasesPage.detail.databasesError || ""
                                color: Theme.textMuted
                                font.family: "Menlo"
                                font.pixelSize: Theme.fontSizeSm
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    // ---- Normal content: ports + databases + query console --------
                    ColumnLayout {
                        width: parent.width
                        spacing: Theme.spacingMd
                        visible: databasesPage.serviceRunning && !databasesPage.authRequired && !databasesPage.detailErrorState

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingMd

                            // Listening ports.
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: portsCol.implicitHeight + Theme.spacingSm * 2
                                radius: Theme.radiusMd
                                color: Theme.cardBackground
                                border.color: Theme.border
                                border.width: 1

                                ColumnLayout {
                                    id: portsCol
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: Theme.spacingSm
                                    spacing: 0

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.bottomMargin: Theme.spacingXs
                                        Label {
                                            Layout.fillWidth: true
                                            text: qsTr("LISTENING PORTS")
                                            color: Theme.textMuted
                                            font.pixelSize: Theme.fontSizeSm
                                            font.bold: true
                                        }
                                        Label {
                                            text: String(databasesPage.detail.ports.length)
                                            color: Theme.textDimmed
                                            font.pixelSize: Theme.fontSizeSm
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.topMargin: Theme.spacingXs
                                        Layout.bottomMargin: 4
                                        Label { Layout.preferredWidth: 56; text: qsTr("Port"); color: Theme.textDimmed; font.pixelSize: 10; font.bold: true }
                                        Label { Layout.preferredWidth: 92; text: qsTr("Address"); color: Theme.textDimmed; font.pixelSize: 10; font.bold: true }
                                        Label { Layout.preferredWidth: 54; text: qsTr("PID"); color: Theme.textDimmed; font.pixelSize: 10; font.bold: true }
                                        Label { Layout.fillWidth: true; text: qsTr("Process"); color: Theme.textDimmed; font.pixelSize: 10; font.bold: true }
                                    }
                                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

                                    Label {
                                        visible: databasesPage.detail.ports.length === 0
                                        Layout.topMargin: Theme.spacingSm
                                        text: qsTr("No listeners found")
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontSizeSm
                                    }

                                    Repeater {
                                        model: databasesPage.detail.ports
                                        delegate: RowLayout {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            Layout.topMargin: 4
                                            Layout.bottomMargin: 4
                                            Label { Layout.preferredWidth: 56; text: String(modelData.port); color: Theme.primary; font.family: "Menlo"; font.pixelSize: Theme.fontSizeSm }
                                            Label { Layout.preferredWidth: 92; text: modelData.addr; color: Theme.textMuted; font.family: "Menlo"; font.pixelSize: Theme.fontSizeSm; elide: Text.ElideRight }
                                            Label { Layout.preferredWidth: 54; text: modelData.pid; color: Theme.textMuted; font.family: "Menlo"; font.pixelSize: Theme.fontSizeSm }
                                            Label { Layout.fillWidth: true; text: modelData.proc; color: Theme.textMuted; font.family: "Menlo"; font.pixelSize: Theme.fontSizeSm; elide: Text.ElideRight }
                                        }
                                    }
                                }
                            }

                            // Databases (hidden for engines with no such concept, e.g. Redis).
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: dbsCol.implicitHeight + Theme.spacingSm * 2
                                visible: databasesPage.selectedService
                                    && databasesPage.prop(databasesPage.selectedService, "engine", "") !== "redis"
                                radius: Theme.radiusMd
                                color: Theme.cardBackground
                                border.color: Theme.border
                                border.width: 1

                                ColumnLayout {
                                    id: dbsCol
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: Theme.spacingSm
                                    spacing: 0

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.bottomMargin: Theme.spacingXs
                                        Label {
                                            Layout.fillWidth: true
                                            text: qsTr("DATABASES")
                                            color: Theme.textMuted
                                            font.pixelSize: Theme.fontSizeSm
                                            font.bold: true
                                        }
                                        Label {
                                            text: String(databasesPage.detail.databases.length)
                                            color: Theme.textDimmed
                                            font.pixelSize: Theme.fontSizeSm
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.topMargin: Theme.spacingXs
                                        Layout.bottomMargin: 4
                                        Label { Layout.fillWidth: true; text: qsTr("Name"); color: Theme.textDimmed; font.pixelSize: 10; font.bold: true }
                                        Label { Layout.preferredWidth: 56; text: qsTr("Tables"); color: Theme.textDimmed; font.pixelSize: 10; font.bold: true }
                                        Label { Layout.preferredWidth: 70; text: qsTr("Size"); color: Theme.textDimmed; font.pixelSize: 10; font.bold: true }
                                        Label { Layout.preferredWidth: 76; text: qsTr("Conns"); color: Theme.textDimmed; font.pixelSize: 10; font.bold: true }
                                    }
                                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

                                    Label {
                                        visible: databasesPage.detail.databases.length === 0
                                        Layout.topMargin: Theme.spacingSm
                                        text: qsTr("No databases")
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontSizeSm
                                    }

                                    Repeater {
                                        model: databasesPage.detail.databases
                                        delegate: RowLayout {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            Layout.topMargin: 4
                                            Layout.bottomMargin: 4
                                            Label { Layout.fillWidth: true; text: modelData.name; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSm; elide: Text.ElideRight }
                                            Label { Layout.preferredWidth: 56; text: modelData.tables; color: Theme.textMuted; font.family: "Menlo"; font.pixelSize: Theme.fontSizeSm }
                                            Label { Layout.preferredWidth: 70; text: modelData.size; color: Theme.textMuted; font.family: "Menlo"; font.pixelSize: Theme.fontSizeSm }
                                            Label { Layout.preferredWidth: 76; text: modelData.conns; color: Theme.textMuted; font.family: "Menlo"; font.pixelSize: Theme.fontSizeSm }
                                        }
                                    }
                                }
                            }
                        }

                        // Query console.
                        Rectangle {
                            id: queryConsole
                            Layout.fillWidth: true
                            Layout.preferredHeight: consoleCol.implicitHeight + Theme.spacingMd * 2
                            radius: Theme.radiusMd
                            color: Theme.cardBackground
                            border.width: 1
                            border.color: Theme.border

                            ColumnLayout {
                                id: consoleCol
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: Theme.spacingMd
                                spacing: Theme.spacingSm

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSm

                                    Label {
                                        text: qsTr("QUERY")
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontSizeSm
                                        font.bold: true
                                    }

                                    Item { Layout.fillWidth: true }

                                    UiSelectMenu {
                                        id: dbSelect
                                        Layout.preferredWidth: 160
                                        searchable: false
                                        model: {
                                            var names = []
                                            for (var i = 0; i < databasesPage.detail.databases.length; i++)
                                                names.push(databasesPage.detail.databases[i].name)
                                            return names
                                        }
                                        currentIndex: model.indexOf(databasesPage.selectedDatabase)
                                        onActivated: function(idx) { databasesPage.selectedDatabase = model[idx] }
                                    }

                                    UiButton {
                                        Layout.preferredWidth: 76
                                        enabled: databasesPage.db !== null
                                                 && databasesPage.selectedService !== null
                                                 && !databasesPage.busy
                                        label: qsTr("Run")
                                        color: "primary"
                                        variant: "solid"
                                        size: "sm"
                                        onClicked: databasesPage.executeQuery()
                                    }
                                }

                                TextArea {
                                    id: sqlField
                                    Layout.fillWidth: true
                                    implicitHeight: 84
                                    text: databasesPage.sqlText
                                    placeholderText: qsTr("SELECT * FROM ... LIMIT 10;")
                                    color: Theme.textPrimary
                                    font.family: "Menlo"
                                    background: Rectangle {
                                        color: Theme.windowBackground
                                        border.color: Theme.border
                                        border.width: 1
                                        radius: Theme.radiusMd
                                    }
                                    font.pixelSize: Theme.fontSizeSm
                                    onTextChanged: databasesPage.sqlText = text
                                }

                                RowLayout {
                                    width: parent.width
                                    spacing: Theme.spacingMd

                                    Label {
                                        text: databasesPage.resultMetaText()
                                        color: databasesPage.queryError !== "" ? Theme.error : Theme.textMuted
                                        font.pixelSize: Theme.fontSizeSm
                                        elide: Text.ElideRight
                                    }

                                    Item { Layout.fillWidth: true }

                                    Label {
                                        text: qsTr("Copy")
                                        color: Theme.textDimmed
                                        font.pixelSize: Theme.fontSizeSm
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: databasesPage.copyResults() }
                                    }
                                    Label { text: "·"; color: Theme.textDimmed; font.pixelSize: Theme.fontSizeSm }
                                    Label {
                                        text: qsTr("Export CSV")
                                        color: Theme.textDimmed
                                        font.pixelSize: Theme.fontSizeSm
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: exportDialog.open() }
                                    }
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: Math.max(1, databasesPage.queryColumns.length)
                                    columnSpacing: Theme.spacingMd
                                    rowSpacing: 4
                                    visible: databasesPage.queryColumns.length > 0

                                    Repeater {
                                        model: databasesPage.resultCells()
                                        delegate: Label {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            text: modelData.text
                                            font.bold: modelData.header
                                            font.family: "Menlo"
                                            font.pixelSize: Theme.fontSizeSm
                                            color: modelData.header ? Theme.textDimmed : Theme.textPrimary
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        ConnectionDialog {
            id: connectionDialog
            service: databasesPage.selectedService
            connectionCfg: databasesPage.detail.connection || {}
            testBusy: databasesPage.testBusy
            testShown: databasesPage.testShown
            testOk: databasesPage.testOk
            testText: databasesPage.testText

            onSaveRequested: function(config, password) {
                if (databasesPage.selectedService)
                    databasesPage.db.saveConnection(databasesPage.selectedService.id, config, password)
            }
            onForgetRequested: {
                if (databasesPage.selectedService)
                    databasesPage.db.forgetConnection(databasesPage.selectedService.id)
            }
            onTestRequested: {
                if (!databasesPage.selectedService)
                    return
                databasesPage.testBusy = true
                databasesPage.db.testServiceConnection(databasesPage.selectedService.id)
            }
        }

        FileDialog {
            id: exportDialog
            onAccepted: {
                if (databasesPage.db && exportDialog.fileUrl.toString().length > 0) {
                    var rows = []
                    for (var i = 0; i < databasesPage.queryRows.length; i++)
                        rows.push(databasesPage.queryRows[i])
                    databasesPage.db.exportToFile(exportDialog.fileUrl.toString(), rows)
                }
            }
        }
    }
}
