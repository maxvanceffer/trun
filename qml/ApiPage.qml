import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Ui"
import "components"
pragma ComponentBehavior: Bound

// API client, first page: the saved-requests list (T4) grouped by project,
// with a search box and a History block, plus the two entry points up top.
// The T6 empty state stands in while nothing is saved yet. Right column: the
// environment card. Data loading stays lazy: the service reads Settings only
// on ensureLoaded.
Item {
    id: apiPage
    objectName: "apiPage"

    // Page-level focus scope: the page owns key handling (Esc pops the
    // request page) no matter where focus sits inside it. Plain Items never
    // receive Keys events — only items on the focus path do.
    focus: true
    activeFocusOnTab: false

    readonly property var svc: (typeof apiService !== "undefined") ? apiService : null // qmllint disable unqualified
    readonly property var lm: (typeof logModel !== "undefined") ? logModel : null // qmllint disable unqualified
    readonly property var projSvc: projectService // qmllint disable unqualified
    readonly property var dash: dashboard // qmllint disable unqualified

    readonly property var envList: svc ? svc.environments : []
    readonly property string activeId: svc ? svc.activeEnvId : ""
    readonly property int requestCount: svc ? svc.requests.length : 0
    readonly property var history: svc ? svc.history : []
    readonly property int historyCount: history.length

    // T4 search box. Matches on name + url (the design's filter), and is
    // cleared when the page is left, so a stale query never hides the list.
    property string searchQuery: ""

    // T6: dev servers trun runs right now. A command only counts once its
    // port has been sniffed out of the output (detectedPorts, runKey keyed);
    // anything still silent has no address to offer, so it is left out.
    readonly property var runningServers: {
        var out = []
        var running = projSvc.runningCommands
        var ports = dash.detectedPorts
        for (var i = 0; i < running.length; ++i) {
            var r = running[i]
            var port = ports[r.projectId + "|" + r.commandId]
            if (!port)
                continue
            out.push({
                url: "http://localhost:" + port + "/",
                from: (r.projectName ? r.projectName + " · " : "") + (r.label || "")
            })
        }
        return out
    }

    // projectId -> display name, from the workspace scan. Unknown ids fall
    // back to the raw id; an empty id is the design's "unsaved" bucket.
    readonly property var projectNames: {
        var names = ({})
        var all = projSvc ? projSvc.projects() : []
        for (var i = 0; i < all.length; ++i)
            names[all[i].id || ""] = all[i].name || all[i].id || ""
        return names
    }

    // Requests still shown after the search filter.
    readonly property var filteredRequests: {
        var q = apiPage.searchQuery.trim().toLowerCase()
        var src = apiPage.svc ? apiPage.svc.requests : []
        if (q === "")
            return src
        var out = []
        for (var i = 0; i < src.length; ++i) {
            var name = "" + (src[i].name || "")
            var url = "" + (src[i].url || "")
            if ((name + url).toLowerCase().indexOf(q) >= 0)
                out.push(src[i])
        }
        return out
    }

    // Saved requests grouped by project, in first-seen order; groups with no
    // match drop out (design: .filter(g => g.items.length)).
    readonly property var requestGroups: {
        var src = apiPage.filteredRequests
        var order = []
        var byKey = ({})
        for (var i = 0; i < src.length; ++i) {
            var key = src[i].projectId || ""
            if (byKey[key] === undefined) {
                byKey[key] = []
                order.push(key)
            }
            byKey[key].push(src[i])
        }
        var out = []
        for (var j = 0; j < order.length; ++j) {
            var k = order[j]
            out.push({
                title: k === "" ? qsTr("UNSAVED")
                                : (apiPage.projectNames[k] || k).toUpperCase(),
                count: byKey[k].length,
                items: byKey[k]
            })
        }
        return out
    }

    // Set by Dashboard: future polls load only while the page is visible.
    property bool pageActive: false

    // Second level: the open request page. requestMap is the working copy —
    // a draft (makeDraft) or a detached clone of a saved request — shared by
    // reference with the request pane; null means the list is showing.
    property var requestMap: null
    readonly property bool requestOpen: apiPage.requestMap !== null
    // The header's 3rd crumb follows renames live (mutating a field of a QML
    // var does not re-evaluate a binding on requestMap).
    property string requestName: ""

    // The header takes the request page's "unsaved changes · Save" tail (the
    // design's row was ← Back · crumbs · unsaved changes · Save — both the
    // crumbs and the tail live on the Dashboard breadcrumb row). These
    // passthroughs keep the header decoupled from the pane's internals:
    // requestPane's ids are not visible outside ApiPage.qml.
    readonly property bool requestUnsaved: requestOpen && requestPane.showUnsaved
    readonly property string requestSaveLabel: requestOpen ? requestPane.saveLabel() : ""

    function headerSave() {
        if (requestOpen)
            requestPane.save()
    }

    // "+ New request" and a running-server "Open →" open the request page
    // with a fresh draft; nothing is persisted until Save (T4's newUntitled
    // is gone). History cards reopen their entry as a draft too.
    function openDraft(url) {
        if (!svc)
            return
        requestMap = svc.makeDraft(url || "")
        requestPane.openWith(requestMap, false)
        apiPage.forceActiveFocus()
    }

    function openSavedRequest(map) {
        // Detach: the C++ list must not see edits until Save.
        requestMap = JSON.parse(JSON.stringify(map))
        requestPane.openWith(requestMap, true)
        apiPage.forceActiveFocus()
    }

    function openHistoryEntry(entry) {
        if (!svc)
            return
        requestMap = svc.makeDraft(entry.url || "")
        requestMap.method = entry.method || "GET"
        requestPane.openWith(requestMap, false)
        apiPage.forceActiveFocus()
    }

    // Leaving the request page discards unsaved edits (decided; no prompt).
    function closeRequest() {
        requestPane.openWith(null, false)
        requestMap = null
        requestName = ""
    }

    function importFromCurl() {
        if (!svc)
            return false
        return svc.importCurl(curlArea.text) !== ""
    }

    function envById(id) {
        for (var i = 0; i < apiPage.envList.length; ++i) {
            if (apiPage.envList[i].id === id)
                return apiPage.envList[i]
        }
        return null
    }

    function envNames() {
        var out = []
        for (var i = 0; i < apiPage.envList.length; ++i)
            out.push(apiPage.envList[i].name || apiPage.envList[i].id)
        return out
    }

    function activeEnvIndex() {
        for (var i = 0; i < apiPage.envList.length; ++i) {
            if (apiPage.envList[i].id === apiPage.activeId)
                return i
        }
        return 0
    }

    function secretText(v) {
        if (!v.secret)
            return v.value || ""
        return v.hasSavedValue ? "••••••••" : qsTr("(not set)")
    }

    // Row colours, 1:1 with the design's mapping on Theme tokens: verb ->
    // method colour, HTTP band -> status dot. lastStatus 0 means "never
    // sent", so it gets a neutral dot and an em dash, not a green "0".
    function methodColor(method) {
        switch (method) {
        case "GET":
            return Theme.success
        case "POST":
            return Theme.primary
        case "PUT":
        case "PATCH":
            return Theme.info
        case "DELETE":
            return Theme.error
        default:
            return Theme.textDimmed
        }
    }

    function statusColor(status) {
        var n = Number(status || 0)
        if (n <= 0)
            return Theme.textSubtle
        if (n >= 500)
            return Theme.error
        if (n >= 400)
            return Theme.warning
        return Theme.success
    }

    function statusText(status) {
        var n = Number(status || 0)
        return n > 0 ? String(n) : "—"
    }

    function whenText(iso) {
        return apiPage.dash ? apiPage.dash.timeAgo(iso || "") : ""
    }

    Connections {
        target: apiPage.svc
        function onErrorMessage(message) {
            if (apiPage.lm)
                apiPage.lm.add("error", "api", message)
        }
        function onActiveEnvIdChanged() {
            envSwitch.currentIndex = apiPage.activeEnvIndex()
        }
        function onEnvironmentsChanged() {
            envSwitch.currentIndex = apiPage.activeEnvIndex()
        }
    }

    Connections {
        target: requestPane
        function onRenameOccurred() {
            apiPage.requestName = apiPage.requestMap
                ? (apiPage.requestMap.name || "") : ""
        }
    }

    Component.onCompleted: {
        if (apiPage.svc)
            apiPage.svc.ensureLoaded()
        envSwitch.currentIndex = apiPage.activeEnvIndex()
    }

    // Manual two-level slide (no StackLayout), DatabasesPage pattern: the
    // list column slides out, the request page slides in from the right.
    // Esc pops back while a request is open (the header crumb is the mouse
    // path back).
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape && apiPage.requestOpen) {
            event.accepted = true
            apiPage.closeRequest()
        }
    }

    ColumnLayout {
        // The column animates by x/opacity instead of anchors.fill so the
        // slide needs no extra wrapper (margins baked into the geometry).
        width: parent.width - 2 * Theme.spacingLg
        height: parent.height - 2 * Theme.spacingLg
        x: (apiPage.requestOpen ? -parent.width : 0) + Theme.spacingLg
        opacity: apiPage.requestOpen ? 0 : 1
        visible: opacity > 0
        Behavior on x {
            NumberAnimation { duration: Theme.animSlide; easing.type: Theme.easingStandard }
        }
        Behavior on opacity {
            NumberAnimation { duration: Theme.animSlide; easing.type: Theme.easingStandard }
        }
        spacing: Theme.spacingLg

        // Top row: search + the two entry points. Shown on both the list and
        // the empty state (the design keeps it above the plate).
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm

            UiInput {
                id: searchField
                Layout.preferredWidth: 280
                Layout.alignment: Qt.AlignVCenter
                leadingIconSource: Theme.icon("filter")
                placeholderText: qsTr("Search requests…")
                text: apiPage.searchQuery
                onTextChanged: apiPage.searchQuery = text
            }

            Item { Layout.fillWidth: true }

            UiButton {
                Layout.alignment: Qt.AlignVCenter
                label: qsTr("Import cURL")
                color: "neutral"
                variant: "outline"
                size: "sm"
                onClicked: importDialog.openCurl()
            }

            UiButton {
                Layout.alignment: Qt.AlignVCenter
                label: qsTr("+ New request")
                color: "primary"
                variant: "solid"
                size: "sm"
                onClicked: apiPage.openDraft("")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingMd

            // Left: the list, or the T6 empty state while there is nothing.
            Item {
                id: leftColumn
                Layout.fillWidth: true
                Layout.fillHeight: true

                // T6 empty state.
                Rectangle {
                    id: emptyPlate
                    objectName: "apiEmptyPlate"
                    anchors.fill: parent
                    visible: apiPage.requestCount === 0
                    radius: Theme.radiusSm
                    color: "transparent"
                    clip: true

                    // 1px dashed plate border — a Rectangle can only stroke
                    // solid, so the dashes are drawn (same trick as the
                    // "+ Add option" pill in ConsoleParameters).
                    Canvas {
                        id: emptyBorder
                        anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.reset()
                            ctx.setLineDash([5, 4])
                            ctx.lineWidth = 1
                            ctx.strokeStyle = Theme.border
                            var x = 0.5
                            var y = 0.5
                            var w = width - 1
                            var h = height - 1
                            var r = Theme.radiusSm
                            ctx.beginPath()
                            ctx.moveTo(x + r, y)
                            ctx.lineTo(x + w - r, y)
                            ctx.arcTo(x + w, y, x + w, y + r, r)
                            ctx.lineTo(x + w, y + h - r)
                            ctx.arcTo(x + w, y + h, x + w - r, y + h, r)
                            ctx.lineTo(x + r, y + h)
                            ctx.arcTo(x, y + h, x, y + h - r, r)
                            ctx.lineTo(x, y + r)
                            ctx.arcTo(x, y, x + r, y, r)
                            ctx.stroke()
                        }
                    }

                    // Canvas keeps whatever stroke it last painted; a system
                    // theme flip has to be pushed through by hand.
                    Connections {
                        target: Theme
                        function onIsDarkChanged() {
                            emptyBorder.requestPaint()
                        }
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 2 * Theme.spacingXl, 460)
                        spacing: Theme.spacingLg

                        Rectangle {
                            Layout.preferredWidth: 44
                            Layout.preferredHeight: 44
                            Layout.alignment: Qt.AlignHCenter
                            radius: Theme.radiusSm
                            color: Theme.mutedSurface
                            border.width: 1
                            border.color: Theme.border

                            Label {
                                anchors.centerIn: parent
                                text: "{}"
                                font.family: "Menlo"
                                font.pixelSize: 16
                                font.bold: true
                                color: Theme.primary
                            }
                        }

                        // Heading block
                        ColumnLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: Theme.spacingXs

                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: qsTr("No saved requests yet")
                                color: Theme.textPrimary
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.maximumWidth: 360
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                text: qsTr("Send a request to a local server or any API. Save it to keep it here, grouped by project.")
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSizeLg
                            }
                        }

                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: Theme.spacingSm

                            UiButton {
                                label: qsTr("+ New request")
                                color: "primary"
                                variant: "solid"
                                size: "sm"
                                onClicked: apiPage.openDraft("")
                            }

                            UiButton {
                                label: qsTr("Import cURL")
                                color: "neutral"
                                variant: "outline"
                                size: "sm"
                                onClicked: importDialog.openCurl()
                            }
                        }

                        // Running dev servers — hidden while there is nothing
                        // to offer rather than showing an empty frame.
                        // preferredWidth (not fillWidth) keeps the block centred
                        // inside the plate the way the design's max-width:440 does.
                        ColumnLayout {
                            Layout.preferredWidth: Math.min(parent.width, 440)
                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: Theme.spacingSm
                            visible: apiPage.runningServers.length > 0
                            spacing: Theme.spacingXs

                            Label {
                                text: qsTr("OR START FROM A RUNNING SERVER")
                                color: Theme.textDimmed
                                font.pixelSize: Theme.fontSizeSm
                                font.bold: true
                                font.letterSpacing: 0.9
                            }

                            Rectangle {
                                id: serverBox
                                Layout.fillWidth: true
                                implicitHeight: serverRows.implicitHeight
                                radius: Theme.radiusSm
                                color: Theme.cardBackground
                                border.width: 1
                                border.color: Theme.border
                                clip: true

                                ColumnLayout {
                                    id: serverRows
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    spacing: 0

                                    Repeater {
                                        model: apiPage.runningServers

                                        delegate: Rectangle {
                                            id: serverRow
                                            required property var modelData
                                            required property int index

                                            Layout.fillWidth: true
                                            implicitHeight: 32
                                            color: rowHover.hovered ? Theme.mutedSurface : "transparent"

                                            Rectangle {
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.bottom: parent.bottom
                                                height: 1
                                                visible: serverRow.index < apiPage.runningServers.length - 1
                                                color: Theme.border
                                            }

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: Theme.spacingMd
                                                anchors.rightMargin: Theme.spacingMd
                                                spacing: Theme.spacingSm

                                                Rectangle {
                                                    Layout.preferredWidth: 8
                                                    Layout.preferredHeight: 8
                                                    radius: 4
                                                    color: Theme.success
                                                }

                                                Label {
                                                    Layout.fillWidth: true
                                                    Layout.minimumWidth: 0
                                                    text: serverRow.modelData.url
                                                    color: Theme.textPrimary
                                                    font.family: "Menlo"
                                                    font.pixelSize: Theme.fontSizeSm
                                                    elide: Text.ElideRight
                                                }

                                                Label {
                                                    Layout.minimumWidth: 0
                                                    Layout.maximumWidth: 150
                                                    text: serverRow.modelData.from
                                                    color: Theme.textMuted
                                                    font.pixelSize: Theme.fontSizeSm
                                                    elide: Text.ElideRight
                                                }

                                                Label {
                                                    text: qsTr("Open →")
                                                    color: Theme.primary
                                                    font.pixelSize: Theme.fontSizeSm
                                                }
                                            }

                                            HoverHandler {
                                                id: rowHover
                                                cursorShape: Qt.PointingHandCursor
                                            }

                                            TapHandler {
                                                onTapped: apiPage.openDraft(serverRow.modelData.url)
                                            }
                                        }
                                    }
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Taken from dev commands trun is running right now.")
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSizeSm
                            }
                        }
                    }
                }

                // T4 list: project groups + history. Scrolls on its own so
                // the environment card stays put.
                ScrollView {
                    id: listScroll
                    objectName: "apiListScroll"
                    anchors.fill: parent
                    visible: apiPage.requestCount > 0
                    clip: true

                    Column {
                        width: listScroll.availableWidth
                        spacing: 18

                        Repeater {
                            model: apiPage.requestGroups

                            delegate: Column {
                                id: groupCol
                                required property var modelData
                                width: parent.width
                                spacing: Theme.spacingSm

                                // Group header: TITLE · count · hairline.
                                RowLayout {
                                    width: parent.width
                                    spacing: Theme.spacingSm

                                    Label {
                                        text: groupCol.modelData.title
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontSizeSm
                                        font.bold: true
                                        font.letterSpacing: 0.9
                                    }

                                    Label {
                                        text: String(groupCol.modelData.count)
                                        color: Theme.textSubtle
                                        font.pixelSize: Theme.fontSizeSm
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 1
                                        color: Theme.border
                                    }
                                }

                                // One card per request (the shared list element).
                                Repeater {
                                    model: groupCol.modelData.items

                                    delegate: UiListElement {
                                        id: reqCard
                                        required property var modelData

                                        width: parent.width
                                        implicitHeight: reqRowLayout.implicitHeight + 2 * Theme.spacingMd
                                        clickable: true
                                        onClicked: apiPage.openSavedRequest(reqCard.modelData)

                                        RowLayout {
                                            id: reqRowLayout
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            anchors.margins: Theme.spacingMd
                                            spacing: Theme.spacingMd

                                            Label {
                                                Layout.preferredWidth: 52
                                                text: reqCard.modelData.method
                                                color: apiPage.methodColor(reqCard.modelData.method)
                                                font.family: "Menlo"
                                                font.pixelSize: Theme.fontSizeSm
                                                font.bold: true
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                spacing: 2

                                                Label {
                                                    Layout.fillWidth: true
                                                    text: reqCard.modelData.name
                                                    color: Theme.textPrimary
                                                    font.pixelSize: Theme.fontSizeLg
                                                    font.weight: Font.DemiBold
                                                    elide: Text.ElideRight
                                                }

                                                Label {
                                                    Layout.fillWidth: true
                                                    text: reqCard.modelData.url
                                                    color: Theme.textMuted
                                                    font.family: "Menlo"
                                                    font.pixelSize: Theme.fontSizeSm
                                                    elide: Text.ElideRight
                                                }
                                            }

                                            RowLayout {
                                                Layout.preferredWidth: 56
                                                Layout.alignment: Qt.AlignVCenter
                                                spacing: 6

                                                Rectangle {
                                                    Layout.preferredWidth: 8
                                                    Layout.preferredHeight: 8
                                                    radius: 4
                                                    color: apiPage.statusColor(reqCard.modelData.lastStatus)
                                                }

                                                Label {
                                                    text: apiPage.statusText(reqCard.modelData.lastStatus)
                                                    color: Theme.textMuted
                                                    font.family: "Menlo"
                                                    font.pixelSize: Theme.fontSizeSm
                                                }
                                            }

                                            Label {
                                                Layout.preferredWidth: 58
                                                Layout.alignment: Qt.AlignVCenter
                                                text: apiPage.whenText(reqCard.modelData.lastAt)
                                                color: Theme.textSubtle
                                                font.pixelSize: Theme.fontSizeSm
                                                horizontalAlignment: Text.AlignRight
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // History: unsaved sends, read-only in this map. The
                        // writer is the request page, so it starts (and stays)
                        // empty — the dashed frame marks it as transient.
                        Column {
                            width: parent.width
                            spacing: Theme.spacingSm

                            RowLayout {
                                width: parent.width
                                spacing: Theme.spacingSm

                                Label {
                                    text: qsTr("HISTORY")
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeSm
                                    font.bold: true
                                    font.letterSpacing: 0.9
                                }

                                Label {
                                    text: qsTr("unsaved · last 50")
                                    color: Theme.textSubtle
                                    font.pixelSize: Theme.fontSizeSm
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 1
                                    color: Theme.border
                                }

                                UiButton {
                                    Layout.alignment: Qt.AlignVCenter
                                    label: qsTr("Clear")
                                    color: "neutral"
                                    variant: "link"
                                    size: "xs"
                                    enabled: apiPage.historyCount > 0
                                    onClicked: if (apiPage.svc) apiPage.svc.clearHistory()
                                }
                            }

                            // One card per history entry. Nothing is written yet,
                            // so this list stays empty until Send lands.
                            Repeater {
                                model: apiPage.history

                                delegate: UiListElement {
                                    id: histCard
                                    required property var modelData

                                    width: parent.width
                                    implicitHeight: histRowLayout.implicitHeight + 2 * Theme.spacingSm
                                    clickable: true
                                    onClicked: apiPage.openHistoryEntry(histCard.modelData)

                                    RowLayout {
                                        id: histRowLayout
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: Theme.spacingSm
                                        spacing: Theme.spacingMd

                                        Label {
                                            Layout.preferredWidth: 52
                                            text: histCard.modelData.method
                                            color: apiPage.methodColor(histCard.modelData.method)
                                            font.family: "Menlo"
                                            font.pixelSize: Theme.fontSizeSm
                                            font.bold: true
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            text: histCard.modelData.url
                                            color: Theme.textDimmed
                                            font.family: "Menlo"
                                            font.pixelSize: Theme.fontSizeSm
                                            elide: Text.ElideRight
                                        }

                                        RowLayout {
                                            Layout.preferredWidth: 56
                                            Layout.alignment: Qt.AlignVCenter
                                            spacing: 6

                                            Rectangle {
                                                Layout.preferredWidth: 8
                                                Layout.preferredHeight: 8
                                                radius: 4
                                                color: apiPage.statusColor(histCard.modelData.status)
                                            }

                                            Label {
                                                text: apiPage.statusText(histCard.modelData.status)
                                                color: Theme.textMuted
                                                font.family: "Menlo"
                                                font.pixelSize: Theme.fontSizeSm
                                            }
                                        }

                                        Label {
                                            Layout.preferredWidth: 58
                                            Layout.alignment: Qt.AlignVCenter
                                            text: apiPage.whenText(histCard.modelData.at)
                                            color: Theme.textSubtle
                                            font.pixelSize: Theme.fontSizeSm
                                            horizontalAlignment: Text.AlignRight
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Right: environment card, hugging its content.
            Rectangle {
                Layout.preferredWidth: 300
                Layout.alignment: Qt.AlignTop
                implicitHeight: cardContent.implicitHeight
                radius: Theme.radiusMd
                color: Theme.cardBackground
                border.width: 1
                border.color: Theme.border

                ColumnLayout {
                    id: cardContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.spacingMd
                        Layout.rightMargin: Theme.spacingMd
                        Layout.topMargin: Theme.spacingMd
                        Layout.bottomMargin: Theme.spacingSm
                        spacing: Theme.spacingSm

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("ENVIRONMENT")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSm
                            font.bold: true
                        }

                        UiButton {
                            Layout.preferredWidth: 64
                            label: qsTr("Edit")
                            color: "secondary"
                            variant: "ghost"
                            size: "sm"
                            onClicked: environmentsDialog.openDialog()
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.border
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.spacingMd
                        Layout.rightMargin: Theme.spacingMd
                        Layout.topMargin: Theme.spacingMd
                        Layout.bottomMargin: Theme.spacingMd
                        spacing: 0

                        UiSegmented {
                            id: envSwitch
                            Layout.fillWidth: true
                            stretch: true
                            model: apiPage.envNames()
                            onActivated: function(index) {
                                if (apiPage.svc && index < apiPage.envList.length)
                                    apiPage.svc.setActiveEnv(apiPage.envList[index].id)
                            }
                        }

                        Item { Layout.fillWidth: true; Layout.preferredHeight: Theme.spacingMd }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingMd

                            Repeater {
                                model: apiPage.envById(apiPage.activeId) ? apiPage.envById(apiPage.activeId).vars : []

                                delegate: ColumnLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    spacing: 4
                                    opacity: modelData.enabled ? 1 : 0.45

                                    Label {
                                        Layout.fillWidth: true
                                        text: "{{" + modelData.name + "}}"
                                        color: Theme.primary
                                        font.family: "Menlo"
                                        font.pixelSize: Theme.fontSizeSm
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: apiPage.secretText(modelData)
                                        color: Theme.textMuted
                                        font.family: "Menlo"
                                        font.pixelSize: Theme.fontSizeSm
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.border
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.spacingMd
                        Layout.rightMargin: Theme.spacingMd
                        Layout.topMargin: Theme.spacingSm
                        Layout.bottomMargin: Theme.spacingMd
                        wrapMode: Text.WordWrap
                        textFormat: Text.StyledText
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSm
                        text: qsTr("Use them in requests as <font color=\"%1\"><code>{{baseUrl}}</code></font>. Secrets are stored in Keychain.").arg("#" + Theme.primary.toString().slice(-6))
                    }
                }
            }
        }
    }

    // Request page (second level). Slides in while a request is open; the
    // working copy lives in apiPage.requestMap and is shared by reference.
    ApiRequestPage {
        id: requestPane
        width: parent.width
        height: parent.height
        pageActive: apiPage.pageActive
        x: apiPage.requestOpen ? 0 : parent.width
        opacity: apiPage.requestOpen ? 1 : 0
        visible: opacity > 0
        Behavior on x {
            NumberAnimation { duration: Theme.animSlide; easing.type: Theme.easingStandard }
        }
        Behavior on opacity {
            NumberAnimation { duration: Theme.animSlide; easing.type: Theme.easingStandard }
        }
    }

    EnvironmentsDialog {
        id: environmentsDialog
    }

    // Import cURL: the design has no form for this (its button carries no
    // handler), so the paste surface is a plain dialog — AppDialog + a
    // TextArea, nothing from outside the kit.
    AppDialog {
        id: importDialog
        title: qsTr("Import cURL")
        subtitle: qsTr("Method, headers, data and credentials are read from the command. {{variables}} are kept as they are.")
        dialogWidth: 560
        dialogHeight: 320

        function openCurl() {
            curlArea.text = ""
            open()
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacingSm

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                TextArea {
                    id: curlArea
                    font.family: "Menlo"
                    font.pixelSize: Theme.fontSizeMd
                    color: Theme.textPrimary
                    placeholderText: qsTr("curl -X POST 'https://api.example.com/things' \\")
                    placeholderTextColor: Theme.textMuted
                    wrapMode: Text.Wrap
                    selectByMouse: true
                    background: Rectangle {
                        color: Theme.mutedSurface
                        border.color: curlArea.activeFocus ? Theme.accent : Theme.border
                        border.width: 1
                        radius: Theme.radiusSm
                    }
                }
            }
        }

        footerRight: [
            UiButton {
                label: qsTr("Cancel")
                color: "secondary"
                variant: "outline"
                size: "sm"
                onClicked: importDialog.reject()
            },
            UiButton {
                label: qsTr("Import")
                color: "primary"
                variant: "solid"
                size: "sm"
                enabled: curlArea.text.trim().length > 0
                onClicked: {
                    // A parse failure reports through svc.errorMessage and
                    // adds nothing, so the dialog stays open with the text.
                    if (apiPage.importFromCurl())
                        importDialog.accept()
                }
            }
        ]
    }
}
