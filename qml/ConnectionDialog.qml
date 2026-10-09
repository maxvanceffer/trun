import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects
import "Ui"
pragma ComponentBehavior: Bound

// Connection dialog for a database service: host/port/user/password,
// default database, sslmode (Postgres), "save in Keychain", test-connection
// feedback and Forget-saved. The dialog only talks in config maps + a plain
// password string; DatabaseService.saveConnection/forgetConnection/
// testServiceConnection do the actual work off the GUI thread.
// Standard in-app Dialog (dimmed backdrop + drop shadow), matching
// McpSetupDialog/RunConfigDialog/SettingsDialog/UpdateDialog — not a
// one-off Rectangle with a hand-rolled outside-click catcher.
Dialog {
    id: root
    objectName: "connectionDialog"
    anchors.centerIn: parent
    width: 460
    modal: true
    padding: Theme.spacingLg

    property var service: null
    // {host,user,database,sslmode,authMethod,hasSavedPassword}
    property var connectionCfg: ({})

    property bool testBusy: false
    property bool testShown: false
    property bool testOk: false
    property string testText: ""

    signal saveRequested(var config, var password)
    signal forgetRequested()
    signal testRequested()

    function prop(key, fallback) {
        if (!root.service)
            return fallback
        var v = root.service[key]
        return (v === undefined || v === null) ? fallback : v
    }

    // NB: named resetFields, not reset — Dialog already has a reset() signal
    // (emitted by the Dialog.Reset standard button); a local reset() would
    // override it and produce qt.qml.invalidOverride at startup.
    function resetFields() {
        hostField.text = root.connectionCfg.host || "127.0.0.1"
        userField.text = root.connectionCfg.user || ""
        databaseField.text = root.connectionCfg.database || ""
        passwordField.text = ""
        keychainCheck.checked = true
        root.testShown = false
        var sm = root.connectionCfg.sslmode || "prefer"
        var idx = sslModel.indexOf(sm)
        sslSelect.currentIndex = idx >= 0 ? idx : sslModel.indexOf("prefer")
    }

    function openDialog() {
        resetFields()
        open()
    }

    readonly property var sslModel: ["disable", "allow", "prefer", "require"]

    background: Item {
        Rectangle {
            id: dialogCard
            anchors.fill: parent
            radius: Theme.radiusLg
            color: Theme.backgroundDefault // modal content: bg-default
            border.color: Theme.border
            border.width: 1
        }

        MultiEffect {
            anchors.fill: dialogCard
            source: dialogCard
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.5)
            shadowBlur: 1.0
            shadowVerticalOffset: 6
        }
    }

    contentItem: ColumnLayout {
        id: content
        spacing: Theme.spacingMd

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label {
                    text: qsTr("Connection")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeLg
                    font.bold: true
                }
                Label {
                    text: root.prop("name", "") + " " + root.prop("version", "")
                        + " · " + root.prop("engine", "")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSm
                }
            }

        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingXs

            Label {
                text: qsTr("HOST")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeSm
                font.bold: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm
                UiInput {
                    id: hostField
                    Layout.fillWidth: true
                    placeholderText: "127.0.0.1"
                }
                UiInput {
                    id: portField
                    Layout.preferredWidth: 90
                    text: String(root.prop("port", ""))
                    validator: IntValidator { bottom: 1; top: 65535 }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingXs
            visible: root.prop("engine", "") !== "redis"

            Label {
                text: qsTr("USER")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeSm
                font.bold: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm
                UiInput {
                    id: userField
                    Layout.fillWidth: true
                    placeholderText: qsTr("system user (peer)")
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingXs

            Label {
                text: qsTr("PASSWORD")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeSm
                font.bold: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm
                UiInput {
                    id: passwordField
                    Layout.fillWidth: true
                    echoMode: showPassword ? TextInput.Normal : TextInput.Password
                    property bool showPassword: false
                    placeholderText: root.connectionCfg.hasSavedPassword
                        ? qsTr("•••••••••• (saved — leave blank to keep)")
                        : qsTr("none")
                }
                Label {
                    text: passwordField.showPassword ? qsTr("Hide") : qsTr("Show")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSm
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -4
                        cursorShape: Qt.PointingHandCursor
                        onClicked: passwordField.showPassword = !passwordField.showPassword
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            visible: root.prop("engine", "") === "postgresql"

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingXs
                Label {
                    text: qsTr("DEFAULT DATABASE")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSm
                    font.bold: true
                }
                UiInput {
                    id: databaseField
                    Layout.fillWidth: true
                    placeholderText: qsTr("(engine default)")
                }
            }

            ColumnLayout {
                Layout.preferredWidth: 120
                spacing: Theme.spacingXs
                Label {
                    text: qsTr("SSL MODE")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSm
                    font.bold: true
                }
                UiSelectMenu {
                    id: sslSelect
                    Layout.fillWidth: true
                    searchable: false
                    model: root.sslModel
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingXs
            visible: root.prop("engine", "") !== "postgresql"
                     && root.prop("engine", "") !== "redis"

            Label {
                text: qsTr("DEFAULT DATABASE")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeSm
                font.bold: true
            }
            UiInput {
                Layout.fillWidth: true
                placeholderText: qsTr("(engine default)")
                // Shares the same field/text as the Postgres row above.
                text: databaseField.text
                onTextChanged: databaseField.text = text
            }
        }

        UiCheckBox {
            id: keychainCheck
            Layout.fillWidth: true
            checked: true
            label: qsTr("Save password in Keychain — service \"trun.db.%1%2\"")
                .arg(root.prop("engine", ""))
                .arg(root.prop("version", "") !== "" ? "@" + root.prop("version", "") : "")
        }

        Rectangle {
            Layout.fillWidth: true
            visible: root.testShown
            radius: Theme.radiusMd
            color: root.testOk ? Qt.rgba(0.13, 0.77, 0.37, 0.08) : Qt.rgba(1, 0.39, 0.4, 0.08)
            border.color: root.testOk ? Qt.rgba(0.13, 0.77, 0.37, 0.28) : Qt.rgba(1, 0.39, 0.4, 0.3)
            border.width: 1
            implicitHeight: testLabel.implicitHeight + Theme.spacingSm * 2

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingSm
                spacing: Theme.spacingSm

                Rectangle {
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: 4
                    Layout.preferredWidth: 8
                    Layout.preferredHeight: 8
                    radius: 4
                    color: root.testOk ? Theme.success : Theme.error
                }

                Label {
                    id: testLabel
                    Layout.fillWidth: true
                    text: root.testText
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeMd
                    wrapMode: Text.WordWrap
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            Layout.topMargin: Theme.spacingXs

            UiButton {
                Layout.preferredWidth: 130
                enabled: !root.testBusy
                label: root.testBusy ? qsTr("Testing…") : qsTr("Test connection")
                color: "secondary"
                variant: "outline"
                size: "sm"
                onClicked: {
                    root.testShown = false
                    root.testRequested()
                }
            }

            Label {
                visible: root.connectionCfg.hasSavedPassword === true
                text: qsTr("Forget saved")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeSm
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.forgetRequested()
                }
            }

            Item { Layout.fillWidth: true }

            UiButton {
                Layout.preferredWidth: 90
                label: qsTr("Cancel")
                color: "secondary"
                variant: "outline"
                size: "sm"
                onClicked: root.close()
            }

            UiButton {
                Layout.preferredWidth: 90
                label: qsTr("Save")
                color: "primary"
                variant: "solid"
                size: "sm"
                onClicked: {
                    var config = {
                        host: hostField.text,
                        user: userField.text,
                        database: databaseField.text,
                        sslmode: root.sslModel[sslSelect.currentIndex >= 0 ? sslSelect.currentIndex : 0],
                        authMethod: userField.text === "" ? "peer" : "password",
                    }
                    if (passwordField.text === "")
                        root.saveRequested(config, undefined)
                    else
                        root.saveRequested(config, passwordField.text)
                }
            }
        }
    }
}
