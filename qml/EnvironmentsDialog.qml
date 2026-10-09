import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Ui"
import "components"

// Environments editor: Name row (env name + Duplicate/Delete), then the
// variables table (on/off, name, value, Secret switch). Edits the active
// environment (switch it on the card first); Duplicate selects the created
// env. Nothing is written until Save: the dialog edits a deep copy, secrets
// typed into the form travel separately and land in Keychain.
// The design's "+ New environment" pill sits under the env list on the left;
// it calls addEnv() (a new env gets its id on Save).
AppDialog {
    id: root

    readonly property var svc: (typeof apiService !== "undefined") ? apiService : null // qmllint disable unqualified

    property var draftEnvs: []
    property string selectedEnvId: ""

    // Fixed frame the whole table derives from: the outer row is anchored
    // (never layout-negotiated), so nothing below feeds back into it.
    // rightColW / nameW must only shrink when this frame shrinks.
    readonly property real rightColW: contentRow.width - 150 - 1 - 2 * Theme.spacingMd
    readonly property real nameW: Math.max(80, Math.floor((rightColW - 24 - 52 - 20 - 4 * Theme.spacingSm) / 3))

    function selectedEnv() {
        for (var i = 0; i < root.draftEnvs.length; ++i) {
            if (root.envKey(root.draftEnvs[i]) === root.selectedEnvId)
                return root.draftEnvs[i]
        }
        return null
    }

    function selectedIndex() {
        for (var i = 0; i < root.draftEnvs.length; ++i) {
            if (root.envKey(root.draftEnvs[i]) === root.selectedEnvId)
                return i
        }
        return -1
    }

    // New envs have no id yet (assigned on save); track them by tmpId.
    function envKey(env) {
        return env.id !== "" ? env.id : env.tmpId
    }

    function touch() {
        root.draftEnvs = root.draftEnvs.slice()
    }

    // ---- variable row edits ----
    // Repeater hands the delegate a *snapshot* of the var (modelData): writing
    // to it never reaches draftEnvs, so every edit must go through the source
    // object by index. Text edits only write (no touch()) so the row is not
    // rebuilt under the cursor; structural edits touch() to rebuild rows.
    function varRef(index) {
        var env = root.selectedEnv()
        if (!env || index < 0 || index >= env.vars.length)
            return null
        return env.vars[index]
    }

    function setVarName(index, text) {
        var v = root.varRef(index)
        if (v)
            v.name = text
    }

    function setVarValue(index, text) {
        var v = root.varRef(index)
        if (!v)
            return
        if (v.secret)
            v.typedValue = text
        else
            v.value = text
    }

    function setVarEnabled(index, on) {
        var v = root.varRef(index)
        if (!v)
            return
        v.enabled = on
        // checked is bound from above, so the row must be rebuilt to repaint.
        root.touch()
    }

    function setVarSecret(index, on) {
        var v = root.varRef(index)
        if (!v)
            return
        v.secret = on
        v.typedValue = ""
        root.touch()
    }

    function openDialog() {
        if (!root.svc)
            return
        var copy = JSON.parse(JSON.stringify(root.svc.environments))
        for (var i = 0; i < copy.length; ++i) {
            copy[i].tmpId = ""
            for (var j = 0; j < copy[i].vars.length; ++j)
                copy[i].vars[j].typedValue = ""
        }
        root.draftEnvs = copy
        var ids = {}
        for (var k = 0; k < copy.length; ++k)
            ids[root.envKey(copy[k])] = true
        var active = root.svc.activeEnvId
        root.selectedEnvId = ids[active] ? active
            : (copy.length > 0 ? root.envKey(copy[0]) : "")
        open()
    }

    function uniqueName(base) {
        var names = {}
        for (var i = 0; i < root.draftEnvs.length; ++i)
            names[root.draftEnvs[i].name] = true
        if (!names[base])
            return base
        var n = 2
        while (names[base + " " + n])
            ++n
        return base + " " + n
    }

    function addEnv() {
        var d = root.draftEnvs.slice()
        var env = { id: "", tmpId: "new-" + Date.now(), name: root.uniqueName("untitled"), vars: [] }
        d.push(env)
        root.draftEnvs = d
        root.selectedEnvId = env.tmpId
    }

    function duplicateEnv() {
        var src = root.selectedEnv()
        if (!src)
            return
        var d = root.draftEnvs.slice()
        var copy = JSON.parse(JSON.stringify(src))
        copy.id = ""
        copy.tmpId = "new-" + Date.now()
        copy.name = root.uniqueName(src.name + " copy")
        for (var j = 0; j < copy.vars.length; ++j)
            copy.vars[j].typedValue = ""
        d.push(copy)
        root.draftEnvs = d
        root.selectedEnvId = copy.tmpId
    }

    function deleteEnv() {
        var src = root.selectedEnv()
        if (!src)
            return
        deleteConfirm.envName = src.name
        deleteConfirm.open()
    }

    function doDeleteEnv() {
        var idx = root.selectedIndex()
        if (idx < 0)
            return
        var d = root.draftEnvs.slice()
        d.splice(idx, 1)
        root.draftEnvs = d
        root.selectedEnvId = d.length > 0 ? root.envKey(d[Math.max(0, idx - 1)]) : ""
    }

    function addVar() {
        var env = root.selectedEnv()
        if (!env)
            return
        env.vars.push({ name: "", value: "", secret: false, enabled: true,
                        hasSavedValue: false, typedValue: "" })
        root.touch()
    }

    function deleteVar(index) {
        var env = root.selectedEnv()
        if (!env || index < 0 || index >= env.vars.length)
            return
        env.vars.splice(index, 1)
        root.touch()
    }

    function doSave() {
        if (!root.svc)
            return
        var clean = []
        var secrets = {}
        for (var i = 0; i < root.draftEnvs.length; ++i) {
            var src = root.draftEnvs[i]
            var vars = []
            for (var j = 0; j < src.vars.length; ++j) {
                var v = src.vars[j]
                var name = (v.name || "").trim()
                if (name === "")
                    continue
                vars.push({ name: name,
                            value: v.secret ? "" : (v.value || ""),
                            secret: !!v.secret,
                            enabled: v.enabled !== false })
                if (v.secret && (v.typedValue || "") !== "")
                    secrets[root.envKey(src) + "/" + name] = v.typedValue
            }
            clean.push({ id: src.id || "", name: (src.name || "").trim() || "untitled", vars: vars })
        }
        if (root.svc.saveEnvironments(clean, secrets))
            root.close()
    }

    title: qsTr("Environments")
    subtitle: qsTr("Variables substitute as {{name}} in URL, params, headers, body and auth.")
    dialogWidth: 720
    dialogHeight: 520
    closeOnOutsideClick: false

    RowLayout {
        id: contentRow
        anchors.fill: parent
        spacing: Theme.spacingMd

        // ---- env list ----
        ColumnLayout {
            Layout.preferredWidth: 150
            Layout.fillHeight: true
            spacing: Theme.spacingXs

            Label {
                text: qsTr("ENVIRONMENTS")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeSm
                font.bold: true
            }

            Repeater {
                model: root.draftEnvs

                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    radius: Theme.radiusSm
                    color: root.envKey(modelData) === root.selectedEnvId
                        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.25)
                        : "transparent"

                    Label {
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.spacingSm
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.spacingSm
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.name
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeMd
                        elide: Text.ElideRight
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.selectedEnvId = root.envKey(modelData)
                    }
                }
            }

            // Design: dashed "+ New environment" pill directly under the list.
            // Closest in-kit is outline (no dashed button style), same choice
            // as "+ Add variable".
            UiButton {
                block: true
                Layout.topMargin: Theme.spacingXs
                label: qsTr("+ New environment")
                color: "secondary"
                variant: "outline"
                size: "sm"
                onClicked: root.addEnv()
            }

            Item { Layout.fillHeight: true }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: Theme.border
        }

        // ---- selected env ----
        ColumnLayout {
            Layout.fillWidth: true
            // The footnote's unwrapped implicit (≈807px) otherwise inflates
            // this column past rightColW: the env list shrinks to ~121px and
            // the 505px variable rows stop short of the column edge.
            Layout.maximumWidth: root.rightColW
            Layout.fillHeight: true
            spacing: Theme.spacingSm
            visible: root.selectedEnv() !== null

            Label {
                text: qsTr("Name")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeMd
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm

                UiInput {
                    // Design: input is fixed-width with the action buttons
                    // pushed to the right edge (not a full-width field).
                    // 269 + gap 60 = the row's free space (column is 505 wide).
                    Layout.preferredWidth: 269
                    text: root.selectedEnv() ? root.selectedEnv().name : ""
                    placeholderText: qsTr("Environment name")
                    onTextChanged: {
                        var env = root.selectedEnv()
                        if (env)
                            env.name = text
                    }
                }

                // Gap between the input and the buttons (design shows them apart).
                Item { Layout.fillWidth: true }

                UiButton {
                    Layout.preferredWidth: 90
                    label: qsTr("Duplicate")
                    color: "secondary"
                    variant: "ghost"
                    size: "sm"
                    enabled: root.selectedEnv() !== null
                    onClicked: root.duplicateEnv()
                }
                UiButton {
                    Layout.preferredWidth: 78
                    label: qsTr("Delete")
                    color: "error"
                    variant: "ghost"
                    size: "sm"
                    enabled: root.selectedEnv() !== null
                    onClicked: root.deleteEnv()
                }
            }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacingSm
            spacing: Theme.spacingSm

            Item { Layout.preferredWidth: 24 }
            Label { Layout.preferredWidth: root.nameW; text: qsTr("VARIABLE"); color: Theme.textDimmed; font.pixelSize: 10; font.bold: true }
            Label { Layout.fillWidth: true; text: qsTr("VALUE"); color: Theme.textDimmed; font.pixelSize: 10; font.bold: true }
            Label { Layout.preferredWidth: 52; horizontalAlignment: Text.AlignHCenter; text: qsTr("SECRET"); color: Theme.textDimmed; font.pixelSize: 10; font.bold: true }
            Item { Layout.preferredWidth: 20 }
        }

        Repeater {
            model: root.selectedEnv() ? root.selectedEnv().vars : []

            delegate: RowLayout {
                required property var modelData
                required property int index
                Layout.preferredWidth: root.rightColW
                spacing: Theme.spacingSm

                UiCheckBox {
                    Layout.preferredWidth: 24
                    Layout.minimumWidth: 24
                    Layout.maximumWidth: 24
                    label: ""
                    checked: modelData.enabled !== false
                    onToggled: function(on) {
                        root.setVarEnabled(index, on)
                    }
                }

                UiInput {
                    Layout.preferredWidth: root.nameW
                    text: modelData.name
                    placeholderText: qsTr("name")
                    font.family: "Menlo"
                    onTextChanged: root.setVarName(index, text)
                }

                UiInput {
                    Layout.preferredWidth: Math.max(80, root.rightColW - root.nameW - 24 - 52 - 20 - 4 * Theme.spacingSm)
                    text: modelData.secret ? modelData.typedValue : modelData.value
                    echoMode: modelData.secret ? TextInput.Password : TextInput.Normal
                    placeholderText: (modelData.secret && modelData.hasSavedValue)
                        ? qsTr("•••••••• (saved)")
                        : qsTr("value")
                    font.family: "Menlo"
                    onTextChanged: root.setVarValue(index, text)
                }

                Item {
                    Layout.preferredWidth: 52
                    Layout.preferredHeight: 17

                    AppSwitch {
                        anchors.centerIn: parent
                        checked: !!modelData.secret
                        onToggled: function(on) {
                            root.setVarSecret(index, on)
                        }
                    }
                }

                Label {
                    Layout.preferredWidth: 20
                    horizontalAlignment: Text.AlignHCenter
                    text: "×"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeLg
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.deleteVar(index)
                    }
                }
            }
        }

        // Closest in-kit to the dashed "+ Add variable" pill: outline button.
        UiButton {
            Layout.preferredWidth: 132
            label: qsTr("+ Add variable")
            color: "secondary"
            variant: "outline"
            size: "sm"
            onClicked: root.addVar()
        }

        Item { Layout.fillHeight: true }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Secret values are stored in the macOS Keychain and shown masked everywhere else. Non-secret values are stored in trun settings, never in project files.")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSm
        }
    } // right column
    } // outer row: env list + divider + selected env

    footerRight: [
        UiButton {
            label: qsTr("Cancel")
            color: "secondary"
            variant: "outline"
            size: "sm"
            onClicked: root.reject()
        },
        UiButton {
            label: qsTr("Save")
            color: "primary"
            variant: "solid"
            size: "sm"
            onClicked: root.doSave()
        }
    ]

    ConfirmDialog {
        id: deleteConfirm
        property string envName: ""
        message: qsTr("Delete environment \"%1\" and its variables? Saved secrets are removed from Keychain.").arg(envName)
        confirmText: qsTr("Delete")
        destructive: true
        onAccepted: root.doDeleteEnv()
    }
}
