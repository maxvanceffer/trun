import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Ui"
import "components"

// API request page — the second level inside ApiPage. The design's request
// page opens with a top row (name, "unsaved changes", Save) and no in-page
// crumbs or back button: navigation lives in the shared Dashboard header
// (Dashboard / API / <name>) and Esc, matching DatabasesPage. R4 fills the
// body with the URL bar, R5 the tabs, R6 the response panel.
Item {
    id: reqRoot

    objectName: "apiRequestPage"

    // Working copy of the request, shared by reference with ApiPage (a
    // JS object in QML var storage). null = closed. Fields mutate it and
    // mark dirty.
    property var requestMap: null

    // A saved request was opened (false for drafts and history reopens).
    // Used instead of checking requestMap.id because mutating a field of a
    // QML var does not re-evaluate bindings — a plain bool flips the labels
    // the moment a Save succeeds.
    property bool saved: false

    // The name field is the only editable surface in R3; later tickets add
    // URL, params, headers, body, auth.
    property bool dirty: false

    property bool savedFlash: false

    // R4: an in-flight send. UI stub until R6 wires the network slice —
    // while true the URL bar locks and Send becomes Cancel.
    property bool sending: false

    // Live-refresh tick for the "→ resolved" line: edits land in QML-var
    // fields, which do not re-evaluate bindings, so typing in the URL field
    // bumps this counter to pull the preview.
    property int resolveTick: 0

    // ─── R5: tab panel state ───────────────────────────────────────────
    // KV rows are QML-var JS arrays — the codebase's mutate-and-tick idiom:
    // a text edit writes the row field without re-evaluating bindings (no
    // cursor fight inside the inputs), while resolveTick pulls the resolved
    // line on demand. Add/remove and the checkbox toggle reassign the array
    // (delegates re-read their state; typed values persist), which also
    // refreshes the UiTabs badges and the Repeaters. On Save the rows are
    // written back into requestMap.params / requestMap.headers.
    property var paramRows: []
    property var headerRows: []

    // The active tab (Params / Headers / Body / Auth) and the Body / Auth
    // panel modes. Mode state is page state, not persisted (the schema
    // stores body text and auth fields, not the mode): authType comes
    // back from the map; bodyMode derives from the content (the fields
    // would otherwise be invisible on a reopen).
    property int tabIndex: 0
    property string bodyMode: "none"      // none | json | form
    property string authType: "none"      // none | bearer | basic

    // Auth fields live as page state: the inputs bind to it (QML-var
    // mutation never re-evaluates, so typing is cursor-safe), the Loader
    // rebuilds them on every Auth-tab open so a reopen re-reads restored
    // values, and selectAuthType rebuilds the map from this object — so a
    // mode switch never silently drops what was typed.
    property var authDraft: ({ token: "", username: "", password: "" })

    // Guards the body textarea's restore on openWith: the binding write
    // must never mark a freshly opened request dirty.
    property bool bodyGuard: false

    readonly property bool showUnsaved: reqRoot.dirty || !reqRoot.saved

    // "Saved ✓" flash after a successful save (design's saveLabel).
    Timer {
        id: flashTimer
        interval: 1600
        onTriggered: reqRoot.savedFlash = false
    }

    readonly property var svc: (typeof apiService !== "undefined") ? apiService : null // qmllint disable unqualified
    readonly property var projSvc: projectService // qmllint disable unqualified

    signal renameOccurred()

    // ApiPage hands the map over on every open; null closes the page.
    function openWith(map, wasSaved) {
        // bodyGuard keeps the textarea's restore write from marking the
        // freshly opened request dirty (binding writes fire textChanged).
        reqRoot.bodyGuard = true
        requestMap = map
        saved = wasSaved
        dirty = false
        savedFlash = false
        tabIndex = 0
        reqRoot.rebuildKvRows()
        reqRoot.resetPanelModes()
        reqRoot.bodyGuard = false
        if (map)
            reqRoot.renameOccurred()
    }

    // ─── R5: KV rows (Params / Headers) ────────────────────────────────
    // Rows are { enabled, name, value }; QML-var fields don't re-evaluate
    // bindings, so the resolved line pulls the resolve tick explicitly,
    // while add/remove and the checkbox toggle reassign the array (which
    // refreshes the counts and re-reads the row state).
    readonly property var tabItems: [
        { label: qsTr("Params"), badge: reqRoot.paramRows.length },
        { label: qsTr("Headers"), badge: reqRoot.headerRows.length },
        { label: qsTr("Body"), badge: 0 },
        { label: qsTr("Auth"), badge: 0 }
    ]

    function kvEdited() {
        reqRoot.dirty = true
        reqRoot.resolveTick++
    }

    function normalizeKvRows(rows) {
        var out = []
        var src = rows || []
        for (var i = 0; i < src.length; ++i) {
            var r = src[i]
            out.push({
                enabled: r.enabled !== false,
                name: r.name || "",
                value: r.value || ""
            })
        }
        return out
    }

    function rebuildKvRows() {
        if (!reqRoot.requestMap) {
            reqRoot.paramRows = []
            reqRoot.headerRows = []
            return
        }
        reqRoot.paramRows = reqRoot.normalizeKvRows(reqRoot.requestMap.params)
        reqRoot.headerRows = reqRoot.normalizeKvRows(reqRoot.requestMap.headers)
    }

    function kvAddParam() {
        reqRoot.paramRows = reqRoot.paramRows.concat({
            enabled: true, name: "", value: ""
        })
        reqRoot.kvEdited()
    }

    function kvRemoveParam(index) {
        var rows = reqRoot.paramRows.concat()
        if (index < 0 || index >= rows.length)
            return
        rows.splice(index, 1)
        reqRoot.paramRows = rows
        reqRoot.kvEdited()
    }

    // Text edits mutate the row in place (no binding re-evaluation — the
    // field's own text already shows the keystroke).
    function kvParamSet(index, field, value) {
        if (reqRoot.sending || index < 0 || index >= reqRoot.paramRows.length)
            return
        reqRoot.paramRows[index][field] = value
        reqRoot.kvEdited()
    }

    // The enabled toggle reassigns the array so the checkbox's bound
    // `checked` re-reads its state (Mutation alone would leave it stale).
    function kvParamToggle(index, on) {
        var rows = reqRoot.paramRows.concat()
        if (index < 0 || index >= rows.length)
            return
        rows[index].enabled = on
        reqRoot.paramRows = rows
        reqRoot.kvEdited()
    }

    function kvAddHeader() {
        reqRoot.headerRows = reqRoot.headerRows.concat({
            enabled: true, name: "", value: ""
        })
        reqRoot.kvEdited()
    }

    function kvRemoveHeader(index) {
        var rows = reqRoot.headerRows.concat()
        if (index < 0 || index >= rows.length)
            return
        rows.splice(index, 1)
        reqRoot.headerRows = rows
        reqRoot.kvEdited()
    }

    function kvHeaderSet(index, field, value) {
        if (reqRoot.sending || index < 0 || index >= reqRoot.headerRows.length)
            return
        reqRoot.headerRows[index][field] = value
        reqRoot.kvEdited()
    }

    function kvHeaderToggle(index, on) {
        var rows = reqRoot.headerRows.concat()
        if (index < 0 || index >= rows.length)
            return
        rows[index].enabled = on
        reqRoot.headerRows = rows
        reqRoot.kvEdited()
    }

    // Writes the rows back into the working map — called before Save, since
    // the arrays are the live source until then. (A send/history in R6
    // syncs the same way.)
    function syncToRequestMap() {
        if (!reqRoot.requestMap)
            return
        reqRoot.requestMap.params = reqRoot.paramRows.map(function (r) {
            return { enabled: r.enabled, name: r.name, value: r.value }
        })
        reqRoot.requestMap.headers = reqRoot.headerRows.map(function (r) {
            return { enabled: r.enabled, name: r.name, value: r.value }
        })
    }

    // ─── R5: Body + Auth panel modes ───────────────────────────────────
    readonly property var bodyModes: ["none", "json", "form"]
    readonly property var authTypes: ["none", "bearer", "basic"]

    function bodyModeIndex() {
        var i = reqRoot.bodyModes.indexOf(reqRoot.bodyMode)
        return i < 0 ? 0 : i
    }

    function authTypeIndex() {
        var i = reqRoot.authTypes.indexOf(reqRoot.authType)
        return i < 0 ? 0 : i
    }

    // authType lives in the schema (auth.type) and returns from the map;
    // bodyMode has no stored counterpart, so a body with content opens as
    // json (Form is one click away, the text never disappears).
    function resetPanelModes() {
        var a = reqRoot.requestMap && reqRoot.requestMap.auth
            ? reqRoot.requestMap.auth : null
        var type = a ? (a.type || "none") : "none"
        reqRoot.authType = reqRoot.authTypes.indexOf(type) >= 0 ? type : "none"
        reqRoot.authDraft = {
            token: a ? (a.token || "") : "",
            username: a ? (a.username || "") : "",
            password: a ? (a.password || "") : ""
        }
        var body = reqRoot.requestMap ? (reqRoot.requestMap.body || "") : ""
        reqRoot.bodyMode = body.trim() === "" ? "none" : "json"
    }

    function selectBodyMode(index) {
        if (index < 0 || index >= reqRoot.bodyModes.length)
            return
        if (reqRoot.bodyMode !== reqRoot.bodyModes[index]) {
            reqRoot.bodyMode = reqRoot.bodyModes[index]
            reqRoot.dirty = true
        }
    }

    function bodyChanged(text) {
        if (!reqRoot.requestMap)
            return
        reqRoot.requestMap.body = text
        reqRoot.dirty = true
    }

    function selectAuthType(index) {
        if (index < 0 || index >= reqRoot.authTypes.length)
            return
        var t = reqRoot.authTypes[index]
        if (reqRoot.authType !== t) {
            reqRoot.authType = t
            // Rebuild the map's auth from the draft, keeping only the
            // fields the type uses (so a switch back restores them and
            // irrelevant ones don't persist).
            if (reqRoot.requestMap) {
                var a = { type: t }
                if (t === "bearer")
                    a.token = reqRoot.authDraft.token || ""
                if (t === "basic") {
                    a.username = reqRoot.authDraft.username || ""
                    a.password = reqRoot.authDraft.password || ""
                }
                reqRoot.requestMap.auth = a
            }
            reqRoot.dirty = true
        }
    }

    function setAuthField(field, value) {
        if (!reqRoot.requestMap)
            return
        var d = reqRoot.authDraft
        d[field] = value
        reqRoot.authDraft = d
        var a = reqRoot.requestMap.auth || { type: reqRoot.authType }
        a[field] = value
        reqRoot.requestMap.auth = a
        reqRoot.dirty = true
    }

    // Row colours, mirror of ApiPage.methodColor (single source until R4
    // gives the URL bar a shared place for it).
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

    function saveLabel() {
        if (reqRoot.savedFlash)
            return qsTr("Saved ✓")
        return reqRoot.saved ? qsTr("Save") : qsTr("Save…")
    }

    // ─── R4 URL bar helpers ───────────────────────────────────────────
    // Method options — the design's methodOpts.
    readonly property var methodOpts: ["GET", "POST", "PUT", "PATCH", "DELETE"]

    // Environments mirror ApiPage's: app-wide list + active id, so the
    // request page's switch and the Dashboard env card stay in sync.
    readonly property var envList: reqRoot.svc ? reqRoot.svc.environments : []

    function envById(id) {
        for (var i = 0; i < reqRoot.envList.length; ++i)
            if (reqRoot.envList[i].id === id)
                return reqRoot.envList[i]
        return null
    }

    function envNames() {
        var out = []
        for (var i = 0; i < reqRoot.envList.length; ++i)
            out.push(reqRoot.envList[i].name || reqRoot.envList[i].id)
        return out
    }

    function activeEnvIndex() {
        for (var i = 0; i < reqRoot.envList.length; ++i)
            if (reqRoot.envList[i].id === reqRoot.svc.activeEnvId)
                return i
        return 0
    }

    function methodIndex() {
        var m = reqRoot.requestMap ? (reqRoot.requestMap.method || "GET") : "GET"
        return reqRoot.methodOpts.indexOf(m)
    }

    // Plain functions instead of inline onActivated bodies so the harness
    // can drive them without opening the popups.
    function selectMethod(index) {
        if (!reqRoot.requestMap)
            return
        if (index < 0 || index >= reqRoot.methodOpts.length)
            return
        var m = reqRoot.methodOpts[index]
        if (reqRoot.requestMap.method !== m) {
            reqRoot.requestMap.method = m
            reqRoot.dirty = true
        }
    }

    function selectEnv(index) {
        var envs = reqRoot.envList
        if (!reqRoot.svc || index < 0 || index >= envs.length)
            return
        reqRoot.svc.setActiveEnv(envs[index].id)
    }

    function startSend() { reqRoot.sending = true }
    function cancelSend() { reqRoot.sending = false }

    // "→ {{ resolved }}" preview — the URL with active-environment variables
    // substituted. Secret values never reach QML (Keychain keeps them, the
    // var's value is empty by design), so they read as dots — the same
    // masking the environments list uses; R6's send resolves for real on the
    // C++ side. resolveTick keeps the preview live while typing.
    function resolvedUrl() {
        var tick = reqRoot.resolveTick // binding dependency
        if (!reqRoot.requestMap)
            return ""
        var url = reqRoot.requestMap.url || ""
        var env = reqRoot.envById(reqRoot.svc ? reqRoot.svc.activeEnvId : "")
        if (!env)
            return url
        var vars = env.vars || []
        for (var i = 0; i < vars.length; ++i) {
            var v = vars[i]
            var name = (v.name || "").trim()
            if (name === "")
                continue
            if (v.secret && !v.hasSavedValue)
                continue // not set — leave {{name}} visible
            var value = v.secret ? "••••••••" : (v.value || "")
            url = url.split("{{" + name + "}}").join(value)
        }
        // Enabled params with a name append as query — the resolved URL is
        // what Send would call (and R6 stores in history).
        var query = reqRoot.queryFromParams()
        if (query !== "")
            url += (url.indexOf("?") >= 0 ? "&" : "?") + query
        return url
    }

    // The resolved line pulls resolveTick explicitly — rows are QML-var
    // objects, so an edit bumps the tick instead of re-evaluating one.
    function queryFromParams() {
        var tick = reqRoot.resolveTick // binding dependency
        var rows = reqRoot.paramRows
        var parts = []
        for (var i = 0; i < rows.length; ++i) {
            var r = rows[i]
            if (r.enabled && (r.name || "") !== "")
                parts.push(r.name + "=" + (r.value || ""))
        }
        return parts.join("&")
    }

    // Persists the working map; adopts the id returned by the service so the
    // page binds to the stored row from the first save on.
    function doSave() {
        if (!reqRoot.svc || !reqRoot.requestMap)
            return false
        // The KV models are the live source until save (R5).
        reqRoot.syncToRequestMap()
        var id = reqRoot.svc.saveRequest(reqRoot.requestMap)
        if (id === "")
            return false
        reqRoot.requestMap.id = id
        reqRoot.dirty = false
        reqRoot.saved = true
        reqRoot.savedFlash = true
        flashTimer.restart()
        return true
    }

    // Save entry point shared by the Save button and Enter in the name
    // field: drafts open the dialog (suggested name, project), saved
    // requests persist straight away — no mouse needed for a rename.
    function save() {
        if (!reqRoot.requestMap)
            return
        if (reqRoot.saved)
            reqRoot.doSave()
        else
            saveDialog.openFor(reqRoot.requestMap)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        // Top row: the inline rename field. The design's tail ("unsaved
        // changes · Save") sits on the shared Dashboard header's breadcrumb
        // row, right-aligned; the page keeps the functional tail here —
        // just the name.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm

            UiInput {
                id: nameField
                objectName: "apiRequestNameField"
                Layout.preferredWidth: 282
                variant: "ghost"
                font.weight: Font.DemiBold
                placeholderText: qsTr("Untitled request")
                text: reqRoot.requestMap ? (reqRoot.requestMap.name || "") : ""
                onTextEdited: {
                    if (reqRoot.requestMap) {
                        reqRoot.requestMap.name = text
                        reqRoot.dirty = true
                        reqRoot.renameOccurred()
                    }
                }
                // Enter saves the edited name without reaching for the mouse,
                // then leaves edit mode: focus returns to the page root so the
                // input's ring drops and Esc keeps working at page level.
                onAccepted: {
                    reqRoot.save()
                    nameField.focus = false
                }
            }

            Item { Layout.fillWidth: true }
        }

        // R4 body: the URL bar (method · URL · environment · Send/Cancel)
        // and the resolved-URL line. R5 stacks the tab panel under it, R6
        // the response panel.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm

                // Method select: the design's methodOpts; the trigger tints
                // with the method colour, tracked from the selection itself
                // (QML-var field edits do not re-evaluate bindings).
                UiSelectMenu {
                    id: methodSelect
                    objectName: "apiMethodSelect"
                    Layout.preferredWidth: 108
                    model: reqRoot.methodOpts
                    placeholderText: qsTr("Method")
                    searchable: false
                    selectedColor: reqRoot.methodColor(
                        reqRoot.methodOpts[methodSelect.currentIndex])
                    currentIndex: reqRoot.methodIndex()
                    enabled: !reqRoot.sending
                    onActivated: function (index) { reqRoot.selectMethod(index) }
                }

                // URL field: mono (Theme.fontMono). Editing marks dirty and
                // refreshes the resolved line.
                UiInput {
                    id: urlField
                    objectName: "apiUrlField"
                    Layout.fillWidth: true
                    font.family: Theme.fontMono
                    placeholderText: qsTr("https://…")
                    text: reqRoot.requestMap ? (reqRoot.requestMap.url || "") : ""
                    enabled: !reqRoot.sending
                    onTextEdited: {
                        if (reqRoot.requestMap) {
                            reqRoot.requestMap.url = text
                            reqRoot.dirty = true
                            reqRoot.resolveTick++
                        }
                    }
                }

                // Environment switch: app-wide — the Dashboard env card and
                // the resolved line follow the same active environment.
                UiSelectMenu {
                    id: envSelect
                    objectName: "apiEnvSelect"
                    Layout.preferredWidth: 140
                    model: reqRoot.envNames()
                    placeholderText: qsTr("Environment")
                    searchable: false
                    currentIndex: reqRoot.activeEnvIndex()
                    enabled: !reqRoot.sending
                    onActivated: function (index) { reqRoot.selectEnv(index) }
                }

                // Send ⇄ Cancel: a running request turns the spinner on inside the
                // Send button (Nuxt `loading` — a loading button is disabled,
                // so Cancel is a separate control beside it); R6 wires the
                // actual network slice.
                UiButton {
                    objectName: "apiSendButton"
                    Layout.alignment: Qt.AlignVCenter
                    label: qsTr("Send")
                    loading: reqRoot.sending
                    color: "primary"
                    variant: "solid"
                    size: "md"
                    onClicked: reqRoot.startSend()
                }
                UiButton {
                    objectName: "apiCancelButton"
                    Layout.alignment: Qt.AlignVCenter
                    visible: reqRoot.sending
                    label: qsTr("Cancel")
                    color: "neutral"
                    variant: "outline"
                    size: "md"
                    onClicked: reqRoot.cancelSend()
                }
            }

            // Resolved-URL line: "→ {{ resolved }}" — env vars substituted.
            Label {
                objectName: "apiResolvedLine"
                Layout.fillWidth: true
                text: qsTr("→ %1").arg(reqRoot.resolvedUrl())
                color: Theme.textSubtle
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontSizeSm
                elide: Text.ElideRight
            }
        }

        // R5: tab bar (Params / Headers / Body / Auth — Nuxt Tabs pill,
        // counts ride on the KV rows) with the design's shortcut hint; the
        // active tab's panel sits underneath.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd

            UiTabs {
                id: apiTabs
                objectName: "apiTabs"
                color: "neutral"
                items: reqRoot.tabItems
                currentIndex: reqRoot.tabIndex
                onActivated: function (index) { reqRoot.tabIndex = index }
            }

            Label {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                horizontalAlignment: Text.AlignRight
                text: qsTr("⌘↩ send · ⌘S save")
                color: Theme.textSubtle
                font.pixelSize: Theme.fontSizeSm
            }
        }

        // Tab panel: all four stay alive (their rows keep uncommitted
        // state), only the active one shows.
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // ─── Params: KV rows (checkbox · key · value · ✕) ──────────
            ColumnLayout {
                id: paramsPanel
                objectName: "apiParamsPanel"
                anchors.fill: parent
                visible: reqRoot.tabIndex === 0
                spacing: Theme.spacingXs

                Repeater {
                    id: paramsRep
                    Layout.fillWidth: true
                    model: reqRoot.paramRows

                    delegate: RowLayout {
                        id: paramRow
                        required property int index
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: Theme.spacingXs

                        UiCheckBox {
                            id: paramToggle
                            objectName: "apiParamEnabled" + paramRow.index
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18
                            Layout.alignment: Qt.AlignVCenter
                            checked: paramRow.modelData.enabled
                            onToggled: function (on) {
                                reqRoot.kvParamToggle(paramRow.index, on)
                            }
                        }

                        UiInput {
                            objectName: "apiParamName" + paramRow.index
                            Layout.fillWidth: true
                            Layout.preferredWidth: 160
                            Layout.alignment: Qt.AlignVCenter
                            enabled: !reqRoot.sending
                            placeholderText: qsTr("key")
                            text: paramRow.modelData.name
                            onTextEdited:
                                reqRoot.kvParamSet(paramRow.index, "name", text)
                        }

                        UiInput {
                            objectName: "apiParamValue" + paramRow.index
                            Layout.fillWidth: true
                            Layout.preferredWidth: 260
                            Layout.alignment: Qt.AlignVCenter
                            enabled: !reqRoot.sending
                            placeholderText: qsTr("value")
                            text: paramRow.modelData.value
                            onTextEdited:
                                reqRoot.kvParamSet(paramRow.index, "value", text)
                        }

                        // ✕ in a 16x16 circle, hover fill — the house remove
                        // control (ConsoleParameters' option rows, UiAlert).
                        AbstractButton {
                            id: paramRemove
                            objectName: "apiParamRemove" + paramRow.index
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            Layout.alignment: Qt.AlignVCenter
                            hoverEnabled: true
                            enabled: !reqRoot.sending

                            background: Rectangle {
                                radius: 8
                                color: paramRemove.hovered
                                    ? Qt.rgba(Theme.primary.r, Theme.primary.g,
                                              Theme.primary.b, 0.2) : "transparent"
                            }
                            contentItem: Label {
                                text: "✕"
                                color: Theme.primary
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: reqRoot.kvRemoveParam(paramRow.index)
                        }
                    }
                }

                // "+ Add param": the house dashed pill (ConsoleParameters).
                AbstractButton {
                    id: addParamPill
                    objectName: "apiAddParam"
                    Layout.topMargin: Theme.spacingXs
                    implicitHeight: 24
                    implicitWidth: addParamRow.implicitWidth + 20
                    hoverEnabled: true
                    enabled: !reqRoot.sending
                    leftPadding: 10
                    rightPadding: 10
                    topPadding: 0
                    bottomPadding: 0

                    background: Item {
                        // Hover fill UNDER the dashed border (a child of
                        // the Canvas would paint over its dashes).
                        Rectangle {
                            anchors.fill: parent
                            radius: parent.height / 2
                            color: addParamPill.hovered ? Theme.mutedSurface
                                                       : "transparent"
                        }

                        Canvas {
                            id: paramDash
                            anchors.fill: parent
                            onPaint: {
                                var ctx = paramDash.getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                ctx.setLineDash([3, 3])
                                ctx.lineWidth = 1
                                ctx.strokeStyle = Theme.textMuted
                                var r = height / 2
                                ctx.beginPath()
                                ctx.moveTo(width - r, 0.5)
                                ctx.lineTo(r, 0.5)
                                ctx.arc(r, r, r - 0.5, Math.PI * 1.5,
                                        Math.PI * 0.5, true)
                                ctx.lineTo(width - r, height - 0.5)
                                ctx.arc(width - r, r, r - 0.5, Math.PI * 0.5,
                                        Math.PI * 1.5, true)
                                ctx.stroke()
                            }
                        }
                    }

                    contentItem: RowLayout {
                        id: addParamRow
                        spacing: 4

                        Label {
                            text: qsTr("+ Add param")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeMd
                            font.weight: Font.DemiBold
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                    onClicked: reqRoot.kvAddParam()
                }
            }

            // ─── Headers: same KV editor ──────────────────────────────
            ColumnLayout {
                id: headersPanel
                objectName: "apiHeadersPanel"
                anchors.fill: parent
                visible: reqRoot.tabIndex === 1
                spacing: Theme.spacingXs

                Repeater {
                    id: headersRep
                    Layout.fillWidth: true
                    model: reqRoot.headerRows

                    delegate: RowLayout {
                        id: headerRow
                        required property int index
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: Theme.spacingXs

                        UiCheckBox {
                            id: headerToggle
                            objectName: "apiHeaderEnabled" + headerRow.index
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18
                            Layout.alignment: Qt.AlignVCenter
                            checked: headerRow.modelData.enabled
                            onToggled: function (on) {
                                reqRoot.kvHeaderToggle(headerRow.index, on)
                            }
                        }

                        UiInput {
                            objectName: "apiHeaderName" + headerRow.index
                            Layout.fillWidth: true
                            Layout.preferredWidth: 220
                            Layout.alignment: Qt.AlignVCenter
                            enabled: !reqRoot.sending
                            placeholderText: qsTr("header")
                            text: headerRow.modelData.name
                            onTextEdited:
                                reqRoot.kvHeaderSet(headerRow.index, "name", text)
                        }

                        UiInput {
                            objectName: "apiHeaderValue" + headerRow.index
                            Layout.fillWidth: true
                            Layout.preferredWidth: 260
                            Layout.alignment: Qt.AlignVCenter
                            enabled: !reqRoot.sending
                            placeholderText: qsTr("value")
                            text: headerRow.modelData.value
                            onTextEdited:
                                reqRoot.kvHeaderSet(headerRow.index, "value", text)
                        }

                        AbstractButton {
                            id: headerRemove
                            objectName: "apiHeaderRemove" + headerRow.index
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            Layout.alignment: Qt.AlignVCenter
                            hoverEnabled: true
                            enabled: !reqRoot.sending

                            background: Rectangle {
                                radius: 8
                                color: headerRemove.hovered
                                    ? Qt.rgba(Theme.primary.r, Theme.primary.g,
                                              Theme.primary.b, 0.2) : "transparent"
                            }
                            contentItem: Label {
                                text: "✕"
                                color: Theme.primary
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: reqRoot.kvRemoveHeader(headerRow.index)
                        }
                    }
                }

                AbstractButton {
                    id: addHeaderPill
                    objectName: "apiAddHeader"
                    Layout.topMargin: Theme.spacingXs
                    implicitHeight: 24
                    implicitWidth: addHeaderRow.implicitWidth + 20
                    hoverEnabled: true
                    enabled: !reqRoot.sending
                    leftPadding: 10
                    rightPadding: 10
                    topPadding: 0
                    bottomPadding: 0

                    background: Item {
                        Rectangle {
                            anchors.fill: parent
                            radius: parent.height / 2
                            color: addHeaderPill.hovered ? Theme.mutedSurface
                                                        : "transparent"
                        }

                        Canvas {
                            id: headerDash
                            anchors.fill: parent
                            onPaint: {
                                var ctx = headerDash.getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                ctx.setLineDash([3, 3])
                                ctx.lineWidth = 1
                                ctx.strokeStyle = Theme.textMuted
                                var r = height / 2
                                ctx.beginPath()
                                ctx.moveTo(width - r, 0.5)
                                ctx.lineTo(r, 0.5)
                                ctx.arc(r, r, r - 0.5, Math.PI * 1.5,
                                        Math.PI * 0.5, true)
                                ctx.lineTo(width - r, height - 0.5)
                                ctx.arc(width - r, r, r - 0.5, Math.PI * 0.5,
                                        Math.PI * 1.5, true)
                                ctx.stroke()
                            }
                        }
                    }

                    contentItem: RowLayout {
                        id: addHeaderRow
                        spacing: 4

                        Label {
                            text: qsTr("+ Add header")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeMd
                            font.weight: Font.DemiBold
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                    onClicked: reqRoot.kvAddHeader()
                }
            }

            // ─── Body: mode segmented + mono textarea + hint ───────────
            ColumnLayout {
                id: bodyPanel
                objectName: "apiBodyPanel"
                anchors.fill: parent
                visible: reqRoot.tabIndex === 2
                spacing: Theme.spacingSm

                UiSegmented {
                    id: bodyModeSeg
                    objectName: "apiBodyMode"
                    model: ["None", "JSON", "Form"]
                    currentIndex: reqRoot.bodyModeIndex()
                    enabled: !reqRoot.sending
                    onActivated: function (index) { reqRoot.selectBodyMode(index) }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: reqRoot.bodyMode !== "none"
                    clip: true

                    TextArea {
                        id: bodyArea
                        objectName: "apiBodyArea"
                        font.family: Theme.fontMono
                        font.pixelSize: Theme.fontSizeMd
                        color: Theme.textPrimary
                        placeholderText: reqRoot.bodyMode === "form"
                            ? qsTr("key=value") : qsTr("{ }")
                        placeholderTextColor: Theme.textMuted
                        wrapMode: Text.Wrap
                        selectByMouse: true
                        enabled: !reqRoot.sending
                        text: reqRoot.requestMap
                            ? (reqRoot.requestMap.body || "") : ""
                        onTextChanged: {
                            if (reqRoot.bodyGuard)
                                return
                            reqRoot.bodyChanged(text)
                        }
                        background: Rectangle {
                            color: Theme.mutedSurface
                            border.color: bodyArea.activeFocus ? Theme.accent
                                                               : Theme.border
                            border.width: 1
                            radius: Theme.radiusSm
                        }
                    }
                }

                Label {
                    visible: reqRoot.bodyMode !== "none"
                    text: reqRoot.bodyMode === "form"
                        ? qsTr("key=value per line") : qsTr("application/json")
                    color: Theme.textSubtle
                    font.pixelSize: Theme.fontSizeSm
                }
            }

            // ─── Auth: mode segmented + token / user+pass + hint ────────
            ColumnLayout {
                id: authPanel
                objectName: "apiAuthPanel"
                anchors.fill: parent
                visible: reqRoot.tabIndex === 3
                spacing: Theme.spacingSm

                UiSegmented {
                    id: authModeSeg
                    objectName: "apiAuthMode"
                    model: ["None", "Bearer", "Basic"]
                    currentIndex: reqRoot.authTypeIndex()
                    enabled: !reqRoot.sending
                    onActivated: function (index) { reqRoot.selectAuthType(index) }
                }

                // The auth fields rebuild every time the tab opens (the
                // Loader keys on the active tab): their bindings read
                // QML-var state, which re-evaluates on construction rather
                // than on edit — this is what makes a restored token
                // visible again. Visibility inside follows the mode.
                Loader {
                    id: authFields
                    Layout.fillWidth: true
                    active: reqRoot.tabIndex === 3
                    visible: reqRoot.authType !== "none"

                    sourceComponent: ColumnLayout {
                        spacing: Theme.spacingXs

                        // Bearer: a single token.
                        Label {
                            visible: reqRoot.authType === "bearer"
                            text: qsTr("Token")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSm
                        }

                        UiInput {
                            objectName: "apiAuthToken"
                            Layout.fillWidth: true
                            visible: reqRoot.authType === "bearer"
                            enabled: !reqRoot.sending
                            text: reqRoot.authDraft.token
                            onTextEdited: reqRoot.setAuthField("token", text)
                        }

                        // Basic: username + password — the pair the client
                        // would base64 into the Authorization header.
                        Label {
                            visible: reqRoot.authType === "basic"
                            text: qsTr("Username")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSm
                        }

                        UiInput {
                            objectName: "apiAuthUsername"
                            Layout.fillWidth: true
                            visible: reqRoot.authType === "basic"
                            enabled: !reqRoot.sending
                            text: reqRoot.authDraft.username
                            onTextEdited: reqRoot.setAuthField("username", text)
                        }

                        Label {
                            visible: reqRoot.authType === "basic"
                            text: qsTr("Password")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSm
                        }

                        UiInput {
                            objectName: "apiAuthPassword"
                            Layout.fillWidth: true
                            visible: reqRoot.authType === "basic"
                            echoMode: TextInput.Password
                            enabled: !reqRoot.sending
                            text: reqRoot.authDraft.password
                            onTextEdited: reqRoot.setAuthField("password", text)
                        }
                    }
                }

                Label {
                    visible: reqRoot.authType !== "none"
                    text: reqRoot.authType === "basic"
                        ? qsTr("Sent as Authorization: Basic base64(username:password)")
                        : qsTr("Sent as Authorization: Bearer token")
                    color: Theme.textSubtle
                    font.pixelSize: Theme.fontSizeSm
                }
            }
        }
    }

    // Save dialog — the design's first-save flow: suggest a name from
    // method+path, pick a project, preview method+url. Confirm persists
    // through doSave().
    AppDialog {
        id: saveDialog
        objectName: "apiSaveDialog"
        title: qsTr("Save request")
        subtitle: qsTr("It will appear in API under the chosen project.")
        dialogWidth: 460

        function openFor(map) {
            var name = map ? (map.name || "") : ""
            // A name still on its default gets a suggestion (mock's first
            // save: suggestName("GET", url) -> "GET /path"); anything the
            // user typed is kept as-is.
            if (name === "" || name === qsTr("Untitled request"))
                name = reqRoot.svc ? reqRoot.svc.suggestName(map.method || "GET",
                                                            map.url || "")
                                   : name
            saveNameField.text = name

            var projects = reqRoot.projSvc ? reqRoot.projSvc.projects() : []
            var names = []
            for (var i = 0; i < projects.length; ++i)
                names.push(projects[i].name || projects[i].id || "")
            projectMenu.model = names
            projectMenu.currentIndex = -1
            if (map && map.projectId) {
                for (var j = 0; j < projects.length; ++j) {
                    if ((projects[j].id || "") === map.projectId) {
                        projectMenu.currentIndex = j
                        break
                    }
                }
            }
            if (projectMenu.currentIndex < 0)
                projectMenu.currentIndex = projects.length > 0 ? 0 : -1
            open()
        }

        function confirmSave() {
            var name = saveNameField.text.trim()
            if (name === "" || !reqRoot.requestMap)
                return
            reqRoot.requestMap.name = name
            var projects = reqRoot.projSvc ? reqRoot.projSvc.projects() : []
            reqRoot.requestMap.projectId =
                (projectMenu.currentIndex >= 0
                 && projectMenu.currentIndex < projects.length)
                    ? (projects[projectMenu.currentIndex].id || "")
                    : ""
            if (reqRoot.doSave())
                close()
        }

        onOpened: saveNameField.forceActiveFocus()

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacingSm

            Label {
                text: qsTr("Name")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeSm
            }

            UiInput {
                id: saveNameField
                objectName: "apiSaveNameField"
                Layout.fillWidth: true
                placeholderText: qsTr("e.g. Get campaign")
                selectByMouse: true
                onAccepted: saveDialog.confirmSave()
            }

            Label {
                text: qsTr("Project")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeSm
            }

            UiSelectMenu {
                id: projectMenu
                Layout.fillWidth: true
                placeholderText: qsTr("Choose a project")
                searchable: false
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingXs
                spacing: Theme.spacingSm

                Label {
                    text: reqRoot.requestMap ? (reqRoot.requestMap.method || "GET") : ""
                    color: reqRoot.methodColor(reqRoot.requestMap
                                               ? (reqRoot.requestMap.method || "GET") : "GET")
                    font.family: "Menlo"
                    font.pixelSize: Theme.fontSizeSm
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    text: reqRoot.requestMap ? (reqRoot.requestMap.url || "") : ""
                    color: Theme.textDimmed
                    font.family: "Menlo"
                    font.pixelSize: Theme.fontSizeSm
                    elide: Text.ElideRight
                }
            }
        }

        footerRight: [
            UiButton {
                label: qsTr("Cancel")
                color: "secondary"
                variant: "outline"
                size: "sm"
                onClicked: saveDialog.reject()
            },
            UiButton {
                label: qsTr("Save")
                color: "primary"
                variant: "solid"
                size: "sm"
                enabled: saveNameField.text.trim() !== ""
                onClicked: saveDialog.confirmSave()
            }
        ]
    }
}