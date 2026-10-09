import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects
import "Ui"
pragma ComponentBehavior: Bound

// In-app dialog listing detected AI agents and their trun MCP entries.
// Rendered inside the window via Dialog, never as a separate Window.
Dialog {
    id: mcpSetupDialog

    readonly property var mcp: mcpAgents // qmllint disable unqualified

    property var agents: []
    property string errorText: ""
    property bool rescanning: false
    property bool scanDone: false
    property bool minElapsed: false

    function tryFinishRescan() {
        if (scanDone && minElapsed)
            rescanning = false
    }

    function refresh() {
        agents = mcp.scanAgents()
        errorText = ""
    }

    function openDialog() {
        refresh()
        open()
    }

    function statusText(agent) {
        if (!agent.found) return qsTr("Agent not found")
        if (!agent.installed) return qsTr("Not installed")
        return agent.enabled ? qsTr("Installed · enabled") : qsTr("Installed · disabled")
    }

    function applyOk(ok) {
        if (!ok) errorText = mcp.lastError()
        else errorText = ""
        refresh()
    }

    anchors.centerIn: parent
    width: parent ? Math.min(parent.width - 64, 560) : 560
    height: parent ? Math.min(parent.height - 64, 480) : 480
    modal: true
    padding: 0

    Timer {
        id: scanTimer
        interval: 30
        onTriggered: {
            mcpSetupDialog.refresh()
            mcpSetupDialog.scanDone = true
            mcpSetupDialog.tryFinishRescan()
        }
    }

    // Minimum spin: one full 800ms circle even when the scan is instant
    Timer {
        id: minSpinTimer
        interval: 850
        onTriggered: {
            mcpSetupDialog.minElapsed = true
            mcpSetupDialog.tryFinishRescan()
        }
    }

    background: Item {
        Rectangle {
            id: dialogCard
            anchors.fill: parent
            radius: 8
            color: Qt.alpha(Theme.backgroundDefault, 0.96) // modal: bg-default
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
        spacing: 10

        Label {
            text: qsTr("MCP servers")
            color: Theme.textPrimary
            font.bold: true
            font.pixelSize: 14
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.topMargin: 20
        }

        Label {
            text: qsTr("Scans your home folder for AI agents and writes the trun MCP entry into their configs (with backup).")
            color: Theme.textMuted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
        }

        Column {
            id: agentsColumn
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            spacing: 8

            Repeater {
                id: agentsRepeater
                model: mcpSetupDialog.agents
                delegate: Rectangle {
                    id: agentRow
                    required property var modelData
                    width: agentsColumn.width
                    height: 64
                    color: Theme.mutedSurface
                    border.color: Theme.border
                    radius: 6

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        ColumnLayout {
                            spacing: 2
                            Layout.fillWidth: true

                            Label {
                                text: agentRow.modelData.name
                                color: Theme.textPrimary
                                font.pixelSize: 12
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Label {
                                text: agentRow.modelData.configPath
                                color: Theme.textMuted
                                font.pixelSize: 10
                                font.family: "Menlo"
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }

                            Label {
                                text: mcpSetupDialog.statusText(agentRow.modelData)
                                color: agentRow.modelData.enabled ? Theme.success : Theme.textMuted
                                font.pixelSize: 10
                                Layout.fillWidth: true
                            }
                        }

                        UiButton {
                            Layout.preferredWidth: 90
                            visible: agentRow.modelData.found && !agentRow.modelData.installed
                            label: qsTr("Install")
                            color: "primary"
                            variant: "solid"
                            size: "sm"
                            onClicked: mcpSetupDialog.applyOk(
                                mcpSetupDialog.mcp.installAgent(agentRow.modelData.id))
                        }

                        UiButton {
                            Layout.preferredWidth: 90
                            visible: agentRow.modelData.installed
                            label: agentRow.modelData.enabled ? qsTr("Disable") : qsTr("Enable")
                            color: "secondary"
                            variant: "outline"
                            size: "sm"
                            onClicked: mcpSetupDialog.applyOk(
                                mcpSetupDialog.mcp.setAgentEnabled(agentRow.modelData.id, !agentRow.modelData.enabled))
                        }
                    }
                }
            }
        }

        Label {
            text: mcpSetupDialog.errorText
            color: Theme.error
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            visible: mcpSetupDialog.errorText !== ""
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

            UiButton {
                label: qsTr("Rescan")
                leadingIconSource: Theme.icon("refresh")
                loading: mcpSetupDialog.rescanning
                color: "secondary"
                variant: "outline"
                size: "sm"
                enabled: !mcpSetupDialog.rescanning
                onClicked: {
                    if (mcpSetupDialog.rescanning) return
                    mcpSetupDialog.rescanning = true
                    mcpSetupDialog.scanDone = false
                    mcpSetupDialog.minElapsed = false
                    scanTimer.start()
                    minSpinTimer.start()
                }
            }

            Item { Layout.fillWidth: true }

            UiButton {
                Layout.preferredWidth: 110
                label: qsTr("Close")
                color: "primary"
                variant: "solid"
                size: "sm"
                onClicked: mcpSetupDialog.close()
            }
        }
    }
}
