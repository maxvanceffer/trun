import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Ui"
import "components"
pragma ComponentBehavior: Bound

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
        requestMap = map
        saved = wasSaved
        dirty = false
        savedFlash = false
        if (map)
            reqRoot.renameOccurred()
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

    // Persists the working map; adopts the id returned by the service so the
    // page binds to the stored row from the first save on.
    function doSave() {
        if (!reqRoot.svc || !reqRoot.requestMap)
            return false
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

        // Top row: inline name · "unsaved changes" · Save. The tail of the
        // design's breadcrumb without the crumbs — the API/project trail is
        // the Dashboard header's job.
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

            // "unsaved changes" (design): drafts are always unsaved, saved
            // requests show it while an edit is pending.
            Label {
                visible: reqRoot.showUnsaved
                text: qsTr("unsaved changes")
                color: Theme.textSubtle
                font.pixelSize: Theme.fontSizeSm
            }

            UiButton {
                Layout.alignment: Qt.AlignVCenter
                // Shown only while there is something to save: drafts are
                // always unsaved, a saved request only after an edit — the
                // same predicate as the "unsaved changes" label.
                visible: reqRoot.showUnsaved
                label: reqRoot.saveLabel()
                color: "neutral"
                variant: "outline"
                size: "sm"
                onClicked: reqRoot.save()
            }
        }

        // R4+ body (URL bar, tabs, response panel) lands here.
        Item { Layout.fillWidth: true; Layout.fillHeight: true }
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