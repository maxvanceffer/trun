import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects

// In-app dialog listing detected AI agents and their trun MCP entries.
// Rendered inside the window via Dialog, never as a separate Window.
Dialog {
    id: mcpSetupDialog

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
        agents = mcpAgents.scanAgents()
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
        if (!ok) errorText = mcpAgents.lastError()
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
            color: Qt.alpha(Theme.cardBackground, 0.96)
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
                model: mcpSetupDialog.agents
                delegate: Rectangle {
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
                                text: modelData.name
                                color: Theme.textPrimary
                                font.pixelSize: 12
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Label {
                                text: modelData.configPath
                                color: Theme.textMuted
                                font.pixelSize: 10
                                font.family: "Menlo"
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }

                            Label {
                                text: mcpSetupDialog.statusText(modelData)
                                color: modelData.enabled ? "#22c55e" : Theme.textMuted
                                font.pixelSize: 10
                                Layout.fillWidth: true
                            }
                        }

                        Button {
                            Layout.preferredWidth: 90
                            Layout.preferredHeight: 28
                            visible: modelData.found && !modelData.installed
                            font.pixelSize: 12

                            background: Rectangle {
                                radius: 6
                                color: Theme.primary
                            }

                            contentItem: Label {
                                text: qsTr("Install")
                                color: Theme.primaryForeground
                                font.pixelSize: 12
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: mcpSetupDialog.applyOk(
                                mcpAgents.installAgent(modelData.id))
                        }

                        Button {
                            Layout.preferredWidth: 90
                            Layout.preferredHeight: 28
                            visible: modelData.installed
                            font.pixelSize: 12

                            background: Rectangle {
                                radius: 6
                                color: "transparent"
                                border.color: Theme.border
                                border.width: 1
                            }

                            contentItem: Label {
                                text: modelData.enabled ? qsTr("Disable") : qsTr("Enable")
                                color: Theme.textPrimary
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: mcpSetupDialog.applyOk(
                                mcpAgents.setAgentEnabled(modelData.id, !modelData.enabled))
                        }
                    }
                }
            }
        }

        Label {
            text: mcpSetupDialog.errorText
            color: Theme.destructive
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

            Button {
                // Content-sized like Nuxt UI: icon + text + spacing + margins
                implicitWidth: rescanContent.implicitWidth + 24
                Layout.preferredHeight: 30
                font.pixelSize: 12
                enabled: !mcpSetupDialog.rescanning

                background: Rectangle {
                    radius: 6
                    color: "transparent"
                    border.color: Theme.border
                    border.width: 1
                }

                contentItem: Row {
                    id: rescanContent
                    anchors.centerIn: parent
                    spacing: 5

                    Image {
                        id: rescanIcon
                        anchors.verticalCenter: parent.verticalCenter
                        source: iconBaseUrl + (Theme.isDark ? "rotate-cw.png" : "rotate-cw-light.png")
                        sourceSize.width: 32
                        sourceSize.height: 32
                        width: 13
                        height: 13
                        fillMode: Image.PreserveAspectFit
                        smooth: true

                        RotationAnimator on rotation {
                            from: 0
                            to: 360
                            duration: 800
                            loops: Animation.Infinite
                            running: mcpSetupDialog.rescanning
                        }
                    }

                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Rescan")
                        color: Theme.textPrimary
                        font.pixelSize: 12
                    }
                }

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

            Button {
                Layout.preferredWidth: 110
                Layout.preferredHeight: 30
                font.pixelSize: 12

                background: Rectangle {
                    radius: 6
                    color: Theme.primary
                }

                contentItem: Label {
                    text: qsTr("Close")
                    color: Theme.primaryForeground
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: mcpSetupDialog.close()
            }
        }
    }
}
