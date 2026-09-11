import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

// In-app Run Configuration dialog (PhpStorm-style, without tabs/runtime/before-launch).
// Rendered inside the window via Dialog, never as a separate Window.
Dialog {
    id: runConfigDialog

    // Storage key in Settings (JSON string) + defaults from the detected command
    property string configKey: ""
    property string defaultName: ""
    property string defaultExecutable: ""
    property string defaultArgsText: ""
    property string defaultWorkdir: ""
    property int defaultPort: 0
    // Create mode: blank form for a new custom command in createFolder.
    // Save emits commandCreated instead of writing a run configuration.
    property string createFolder: ""
    readonly property bool createMode: createFolder !== ""

    signal saved()
    signal commandCreated(var config)

    function loadConfig() {
        var raw = configKey !== "" ? Settings.get(configKey, "") : ""
        var cfg = {}
        if (raw !== "") {
            try { cfg = JSON.parse(raw.toString()) } catch (e) { cfg = {} }
        }
        nameField.text = cfg.name !== undefined ? cfg.name : defaultName
        exeField.text = cfg.executable !== undefined ? cfg.executable : defaultExecutable
        workdirField.text = cfg.workdir !== undefined ? cfg.workdir : defaultWorkdir
        argsField.text = cfg.argsText !== undefined ? cfg.argsText : defaultArgsText
        if (cfg.port !== undefined && cfg.port > 0)
            portField.text = String(cfg.port)
        else if (defaultPort > 0)
            portField.text = String(defaultPort)
        else
            portField.text = ""
        envArea.text = cfg.envText !== undefined ? cfg.envText : ""
        allowBox.checked = cfg.allowMultiple !== undefined ? cfg.allowMultiple : false
    }

    function currentConfig() {
        return {
            name: nameField.text,
            executable: exeField.text,
            workdir: workdirField.text,
            argsText: argsField.text,
            port: parseInt(portField.text, 10) || 0,
            envText: envArea.text,
            allowMultiple: allowBox.checked
        }
    }

    function openDialog() {
        createFolder = ""
        loadConfig()
        open()
    }

    function openCreate(folderPath) {
        createFolder = folderPath
        nameField.text = ""
        exeField.text = ""
        workdirField.text = folderPath
        argsField.text = ""
        portField.text = ""
        envArea.text = ""
        allowBox.checked = false
        open()
    }

    function saveDialog() {
        if (createMode) {
            var cfg = currentConfig()
            cfg.folderPath = createFolder
            commandCreated(cfg)
            close()
            return
        }
        if (configKey !== "")
            Settings.set(configKey, JSON.stringify(currentConfig()))
        saved()
        close()
    }

    anchors.centerIn: parent
    width: parent ? Math.min(parent.width - 64, 560) : 560
    height: parent ? Math.min(parent.height - 64, 520) : 520
    modal: true
    padding: 0

    background: Rectangle {
        color: Theme.cardBackground
        border.color: Theme.border
        radius: 8
    }

    contentItem: ColumnLayout {
        spacing: 10

        Label {
            text: runConfigDialog.createMode ? qsTr("New command") : qsTr("Run configuration")
            color: Theme.textPrimary
            font.bold: true
            font.pixelSize: 14
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.topMargin: 20
        }

        // Name + allow multiple instances
        RowLayout {
            spacing: 12
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20

            Label {
                text: qsTr("Name:")
                color: Theme.textMuted
                font.pixelSize: 12
                Layout.preferredWidth: 130
            }

            TextField {
                id: nameField
                Layout.fillWidth: true
                font.pixelSize: 12
                color: Theme.textPrimary
                background: Rectangle {
                    color: Theme.mutedSurface
                    border.color: Theme.border
                    radius: 6
                }
            }

            Item {
                id: allowBox
                property bool checked: false
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                Layout.alignment: Qt.AlignVCenter
                Accessible.role: Accessible.CheckBox
                Accessible.name: qsTr("Allow multiple instances")
                activeFocusOnTab: true

                Rectangle {
                    anchors.fill: parent
                    radius: 4
                    color: "transparent"
                    border.color: allowBox.checked ? Theme.accent : "#555"
                    border.width: 1

                    Label {
                        anchors.centerIn: parent
                        text: "✓"
                        color: Theme.accent
                        font.pixelSize: 12
                        visible: allowBox.checked
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: allowBox.checked = !allowBox.checked
                }

                Keys.onSpacePressed: allowBox.checked = !allowBox.checked
            }

            Label {
                text: qsTr("Allow multiple instances")
                color: Theme.textMuted
                font.pixelSize: 12
                Layout.alignment: Qt.AlignVCenter

                MouseArea {
                    anchors.fill: parent
                    onClicked: allowBox.checked = !allowBox.checked
                }
            }
        }

        // Executable
        RowLayout {
            spacing: 12
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20

            Label {
                text: qsTr("Executable:")
                color: Theme.textMuted
                font.pixelSize: 12
                Layout.preferredWidth: 130
            }

            TextField {
                id: exeField
                Layout.fillWidth: true
                font.pixelSize: 12
                font.family: "Menlo"
                color: Theme.textPrimary
                background: Rectangle {
                    color: Theme.mutedSurface
                    border.color: Theme.border
                    radius: 6
                }
            }
        }

        // Working directory + browse
        RowLayout {
            spacing: 12
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20

            Label {
                text: qsTr("Working directory:")
                color: Theme.textMuted
                font.pixelSize: 12
                Layout.preferredWidth: 130
            }

            TextField {
                id: workdirField
                Layout.fillWidth: true
                font.pixelSize: 12
                font.family: "Menlo"
                color: Theme.textPrimary
                background: Rectangle {
                    color: Theme.mutedSurface
                    border.color: Theme.border
                    radius: 6
                }
            }

            IconButton {
                Layout.alignment: Qt.AlignVCenter
                iconSource: iconBaseUrl + "folder-32px.png"
                tooltipText: qsTr("Browse")
                onClicked: workdirPicker.open()
            }
        }

        // Application parameters
        RowLayout {
            spacing: 12
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20

            Label {
                text: qsTr("Application parameters:")
                color: Theme.textMuted
                font.pixelSize: 12
                Layout.preferredWidth: 130
            }

            TextField {
                id: argsField
                Layout.fillWidth: true
                font.pixelSize: 12
                font.family: "Menlo"
                color: Theme.textPrimary
                placeholderText: qsTr("space-separated arguments")
                background: Rectangle {
                    color: Theme.mutedSurface
                    border.color: Theme.border
                    radius: 6
                }
            }
        }

        // Port (optional): checked before Run, offers Kill & Run when busy
        RowLayout {
            spacing: 12
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20

            Label {
                text: qsTr("Port:")
                color: Theme.textMuted
                font.pixelSize: 12
                Layout.preferredWidth: 130
            }

            TextField {
                id: portField
                Layout.fillWidth: true
                font.pixelSize: 12
                font.family: "Menlo"
                color: Theme.textPrimary
                placeholderText: defaultPort > 0
                    ? qsTr("detected :%1 — override or leave as is").arg(defaultPort)
                    : qsTr("optional, e.g. 5173")
                validator: IntValidator { bottom: 1; top: 65535 }
                background: Rectangle {
                    color: Theme.mutedSurface
                    border.color: Theme.border
                    radius: 6
                }
            }
        }

        // Environment variables
        RowLayout {
            spacing: 12
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20

            Label {
                text: qsTr("Environment variables:")
                color: Theme.textMuted
                font.pixelSize: 12
                Layout.preferredWidth: 130
                Layout.alignment: Qt.AlignTop
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 110

                TextArea {
                    id: envArea
                    font.pixelSize: 12
                    font.family: "Menlo"
                    color: Theme.textPrimary
                    placeholderText: qsTr("KEY=VALUE, one per line")
                    wrapMode: Text.Wrap
                    background: Rectangle {
                        color: Theme.mutedSurface
                        border.color: Theme.border
                        radius: 6
                    }
                }
            }
        }

        // Actions
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

                onClicked: runConfigDialog.close()
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
                    text: runConfigDialog.createMode ? qsTr("Add") : qsTr("Save")
                    color: Theme.primaryForeground
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: runConfigDialog.saveDialog()
            }
        }
    }

    FolderDialog {
        id: workdirPicker
        title: qsTr("Select Working Directory")

        onAccepted: {
            workdirField.text = selectedFolder.toString().replace(/^file:\/\//, "")
        }
    }
}
