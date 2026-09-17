import QtQuick 6.5
import QtQuick.Controls 6.5
import QtQuick.Layouts 1.5

Frame {
    id: card
    property var command: {}
    // Owning project id ("<path>/<manifest>"). Empty means the active
    // project (single-project contexts); the folder page always sets it.
    property string projectId: ""

    // Running state is derived from the executor (covers live runs and
    // processes adopted from a previous session), never stored locally.
    // Full run key: bare command ids repeat across projects.
    readonly property string effectiveProjectId: card.projectId !== ""
        ? card.projectId : (projectService.activeProject.id || "")
    readonly property string fullId: card.effectiveProjectId
        + "|" + (card.command.id || "")
    property bool running: commandExecutor.runningCommandIds.indexOf(card.fullId) >= 0

    // Frame follows hover only (Recent parity): no selection memory, so no
    // stuck frame after returning from a detail page. card.hovered is the
    // built-in Control property.

    width: 250
    // Explicit height: Frame does not derive implicit height from contentItem,
    // so a fitted height must be bound manually (a constant clipped the button).
    // padding 0: the layout margins (8) are the only chrome, so no overflow.
    padding: 0
    height: cardLayout.implicitHeight + 16

    background: Rectangle {
        radius: 8
        color: card.running ? "#1a1a1a" : Theme.cardBackground
        border.color: card.hovered ? Theme.accent : Theme.border
        border.width: 1
    }

    // Glow ring OUTSIDE the card (margins -3): grid content keeps a
    // matching inner padding so the ring never touches the scroll
    // viewport edge (see folderPage grids). Fades in/out with the hover.
    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: 11
        color: "transparent"
        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.45)
        border.width: 2
        opacity: card.hovered ? 1 : 0

        Behavior on opacity {
            NumberAnimation {
                duration: 250
                easing.type: Theme.easingStandard
            }
        }
    }

    property int cmdPid: 0
    property string memText: "—"

    function selectThis() {
        dashboard.selectCommand(card.fullId)
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
        onClicked: {
            // Detail pages resolve the command through the active project,
            // so select ours first (synchronous) before opening the detail.
            projectService.selectProject(card.effectiveProjectId)
            dashboard.openDetail(card.fullId)
        }
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
            // Same x as the title above: status dot width + row spacing
            Layout.leftMargin: 16
        }

    }
}
