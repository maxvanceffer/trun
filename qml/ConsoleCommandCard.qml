import QtQuick 6.5
import QtQuick.Controls 6.5
import QtQuick.Layouts 1.5
import "Ui"

// Console command card (Symfony bin/console / Laravel artisan record):
// CommandCard's visual language with a positional signature, a
// "N required" badge and a run on click. Commands with required
// positionals do not run — they open the args drawer, which collects
// the missing values and gates the launch on them (T6).
Frame {
    id: card
    property var command: ({})
    // Owning project id ("<path>/<manifest>"), always set by the
    // folder-page section that owns the record.
    property string projectId: ""

    readonly property string commandId: (card.command && card.command.id) ? card.command.id : ""
    readonly property string commandName: (card.command && card.command.label)
        ? card.command.label : ""
    // Title is the bare command name (no binary prefix).
    readonly property string title: card.commandName
    readonly property string description: (card.command && card.command.description)
        ? card.command.description : ""
    // Positional signature + required names, from argsMeta declaration
    // order (required as <arg>, optional as [arg]).
    readonly property var signature: card.parseSignature(card.command ? card.command.argsMeta : undefined)
    readonly property string signatureText: card.signature.text
    readonly property var requiredArgs: card.signature.required

    // Owning project for the run: the section always sets it, the active
    // project is only the single-project fallback (DetailPage parity).
    readonly property string effectiveProjectId: card.projectId !== ""
        ? card.projectId : (projectService.activeProject.id || "")

    // Running state comes from the executor (covers live runs and adopted
    // processes), never stored locally; bare ids repeat across projects.
    readonly property string fullId: card.effectiveProjectId + "|" + card.commandId
    property bool running: commandExecutor.runningCommandIds.indexOf(card.fullId) >= 0

    property int cmdPid: 0

    function parseSignature(meta) {
        var parts = []
        var required = []
        if (!meta)
            return { text: "", required: required }
        for (var i = 0; i < meta.length; ++i) {
            var m = meta[i]
            if (!m || m.kind !== "positional" || !m.name)
                continue
            if (m.required) {
                parts.push("<" + m.name + ">")
                required.push(m.name)
            } else {
                parts.push("[" + m.name + "]")
            }
        }
        return { text: parts.join(" "), required: required }
    }

    function refreshStats() {
        card.cmdPid = commandExecutor.pidForCommand(card.fullId)
    }

    onRunningChanged: {
        if (!card.running) {
            pulseAnim.stop()
            statusDot.opacity = 1
        } else {
            card.refreshStats()
        }
    }

    width: 250
    // Explicit height: Frame does not derive implicit height from
    // contentItem (see CommandCard); padding 0 — the layout margins (8)
    // are the only chrome.
    padding: 0
    height: cardLayout.implicitHeight + 16

    background: Rectangle {
        radius: 8
        // Running flips the card to Nuxt UI's bg-inverted pair: #181818 in the
        // light theme, #fffaee in the dark one — the ink below flips with it.
        color: card.running ? Theme.backgroundInverted : Theme.cardBackground
        border.color: card.hovered ? Theme.accent : Theme.border
        border.width: 1
    }

    // Glow ring OUTSIDE the card (margins -3): the folder-page grid keeps
    // a matching inner padding so the ring never touches the viewport.
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

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            // Every console command opens the detail page; parameters are
            // edited there (inline panel), not in a modal.
            dashboard.openActivity(card.effectiveProjectId, card.commandId)
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
                color: card.running ? Theme.success : Theme.error

                SequentialAnimation on opacity {
                    id: pulseAnim
                    running: card.running
                    loops: Animation.Infinite
                    NumberAnimation { from: 1; to: 0.3; duration: 600; easing.type: Easing.InOutQuad }
                    NumberAnimation { from: 0.3; to: 1; duration: 600; easing.type: Easing.InOutQuad }
                }
            }

            Label {
                text: card.title
                color: card.running ? Theme.textInverted : Theme.textPrimary
                font.pixelSize: 12
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            UiTag {
                visible: card.requiredArgs.length > 0
                Layout.alignment: Qt.AlignVCenter
                label: qsTr("%1 required").arg(card.requiredArgs.length)
                color: "warning"
                variant: "soft"
                size: "sm"
            }
        }

        RowLayout {
            spacing: 8
            Layout.fillWidth: true
            Layout.leftMargin: 16

            Label {
                // Positional signature; commands without positionals show
                // their description instead.
                text: card.signatureText !== "" ? card.signatureText : card.description
                color: card.running ? Theme.textInverted : Theme.textMuted
                font.pixelSize: 10
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
            }

            Label {
                visible: card.running && card.cmdPid > 0
                text: card.running && card.cmdPid > 0 ? String(card.cmdPid) : ""
                color: card.running ? Theme.textInverted : Theme.textMuted
                font.family: "Menlo"
                font.pixelSize: 10
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }
}
