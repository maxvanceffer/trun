import QtQuick 6.5
import QtQuick.Controls 6.5
import QtQuick.Layouts 1.5

Frame {
    id: card
    property var command: {}
    property bool selected: false

    // Running state is derived from the executor (covers live runs and
    // processes adopted from a previous session), never stored locally.
    // Full run key: bare command ids repeat across projects.
    readonly property string fullId: (projectService.activeProject.id || "")
        + "|" + (card.command.id || "")
    property bool running: commandExecutor.runningCommandIds.indexOf(card.fullId) >= 0

    width: 250
    // Explicit height: Frame does not derive implicit height from contentItem,
    // so a fitted height must be bound manually (a constant clipped the button).
    // padding 0: the layout margins (8) are the only chrome, so no overflow.
    padding: 0
    height: cardLayout.implicitHeight + 16

    background: Rectangle {
        radius: 8
        color: card.running ? "#1a1a1a" : Theme.cardBackground
        border.color: card.selected ? Theme.accent : Theme.border
        border.width: 1
    }

    property int cmdPid: 0
    property string memText: "—"

    function selectThis() {
        dashboard.selectCommand(card.command.id)
    }

    function refreshStats() {
        card.cmdPid = commandExecutor.pidForCommand(card.fullId)
        var kb = commandExecutor.memoryForCommand(card.fullId)
        if (kb < 0)
            card.memText = "—"
        else if (kb < 1024)
            card.memText = kb + " KB"
        else
            card.memText = (kb / 1024).toFixed(1) + " MB"
    }

    onRunningChanged: {
        if (!card.running) {
            pulseAnim.stop()
            statusDot.opacity = 1
        } else {
            card.refreshStats()
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: dashboard.openDetail(card.command.id)
    }

    Timer {
        interval: 2000
        repeat: true
        running: card.running
        onTriggered: card.refreshStats()
    }

    ColumnLayout {
        id: cardLayout
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4

        RowLayout {
            spacing: 8
            Layout.fillWidth: true

            Rectangle {
                id: statusDot
                Layout.preferredWidth: 8
                Layout.preferredHeight: 8
                Layout.alignment: Qt.AlignVCenter
                radius: 4
                color: card.running ? "#22c55e" : "#dc2626"

                SequentialAnimation on opacity {
                    id: pulseAnim
                    running: card.running
                    loops: Animation.Infinite
                    NumberAnimation { from: 1; to: 0.3; duration: 600; easing.type: Easing.InOutQuad }
                    NumberAnimation { from: 0.3; to: 1; duration: 600; easing.type: Easing.InOutQuad }
                }
            }

            Label {
                text: card.command.label || "Unknown"
                color: Theme.textPrimary
                font.pixelSize: 12
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Label {
                text: card.running && card.cmdPid > 0 ? String(card.cmdPid) : "—"
                color: Theme.textMuted
                font.family: "Menlo"
                font.pixelSize: 10
                Layout.alignment: Qt.AlignVCenter
            }
        }

        Label {
            text: (card.command.command || "") + " " + (card.command.args ? card.command.args.join(" ") : "")
            color: Theme.textMuted
            font.pixelSize: 10
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

    }
}
