import QtQuick 6.5
import QtQuick.Controls 6.5
import QtQuick.Layouts 6.5
import QtQuick.Effects
import QtQuick.Dialogs

Rectangle {
    id: sidebar
    width: 240
    // Flat: no background of its own, melts into the root tint.
    // (Native OS traffic lights sit top-left; nothing custom here.)
    color: "transparent"

    // Square when maximized, rounded outer corners otherwise
    readonly property int cornerR: (Window.visibility === Window.Maximized
                                    || Window.visibility === Window.FullScreen) ? 0 : 10

    // Two-tone rounded backdrop: middle band + corner pieces.
    // Each outer corner = content-tone square with a sidebar-tone
    // circle over it, leaving a clean rounded notch.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: cornerR
        anchors.bottom: parent.bottom
        anchors.bottomMargin: cornerR
        color: Theme.sidebarBackground
    }

    Rectangle {
        x: 0
        y: 0
        width: cornerR
        height: cornerR
        color: Theme.windowBackground
        visible: cornerR > 0
    }

    Rectangle {
        x: 0
        y: 0
        width: cornerR * 2
        height: cornerR * 2
        radius: cornerR
        color: Theme.sidebarBackground
        visible: cornerR > 0
    }

    Rectangle {
        x: 0
        y: parent.height - cornerR
        width: cornerR
        height: cornerR
        color: Theme.windowBackground
        visible: cornerR > 0
    }

    Rectangle {
        x: 0
        y: parent.height - cornerR * 2
        width: cornerR * 2
        height: cornerR * 2
        radius: cornerR
        color: Theme.sidebarBackground
        visible: cornerR > 0
    }

    signal projectSelected(string projectId)
    signal mcpConfigureRequested()
    signal addCustomRequested(string folderPath)

    // Connect log messages
    Connections {
        target: projectService

        function onLogMessage(level, target, message) {
            logModel.add(level, target, message)
        }
    }

    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 48
        color: "transparent"

        // Native traffic lights float top-left (titled unified toolbar).
        Label {
            anchors.centerIn: parent
            text: "PROJECTS"
            font.pixelSize: 10
            font.bold: true
            color: Theme.textMuted
        }

        // Root folder: pick a new one (or the same) to rescan everything
        IconButton {
            id: rootFolderButton
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            iconSource: iconBaseUrl + "folder-32px.png"
            tooltipText: qsTr("Change root folder & rescan")
            onClicked: rootFolderPicker.open()
        }
    }

    FolderDialog {
        id: rootFolderPicker
        title: qsTr("Select Root Folder to Scan")
        currentFolder: projectService.rootPath !== undefined
            ? "file://" + projectService.rootPath : ""

        onAccepted: projectService.scanFolder(selectedFolder.toString())
    }

    ScrollView {
        anchors.top: header.bottom
        anchors.bottom: footer.top
        anchors.bottomMargin: 4
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8

        TreeView {
            id: tree
            anchors.fill: parent
            clip: true
            model: treeModel
            delegate: TreeItemDelegate {
                onProjectClicked: function(projectId) {
                    tree.selectedProjectId = projectId
                    sidebar.projectSelected(projectId)
                }
                onAddCustomRequested: function(folderPath) {
                    sidebar.addCustomRequested(folderPath)
                }
            }

            // Single column spans the full width so row highlight
            // covers the whole sidebar
            columnWidthProvider: function(column) { return tree.width }

            // Selected project id (path + "/" + manifest).
            // Expansion itself is owned by TreeView (toggleExpanded).
            property string selectedProjectId: ""
        }
    }

    // Footer with the MCP placeholder button
    Item {
        id: footer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.bottomMargin: 8
        height: 32

        IconButton {
            id: mcpButton
            objectName: "mcpButton"
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            iconSource: iconBaseUrl + (Theme.isDark ? "mcp-light-32px.png" : "mcp-dark-32px.png")
            tooltipText: qsTr("MCP servers")
            suppressTooltip: mcpMenu.visible
            onClicked: mcpMenu.visible = !mcpMenu.visible
        }
    }

    // Click-outside catcher: closes the menu, sits under it
    MouseArea {
        anchors.fill: parent
        visible: mcpMenu.visible
        z: 98
        onClicked: mcpMenu.visible = false
    }

    // Custom context menu: fixed size, theme colors, rounded, hairline
    // border, soft shadow.
    // (QtQuick.Controls Menu with a custom delegate collapses to ~0 width,
    // so the menu is built from primitives instead.)
    Item {
        id: mcpMenu
        objectName: "mcpMenu"
        visible: false
        z: 99
        // Right of the button (28px) with a 6px gap, bottom-aligned
        // with the footer. Anchored to footer (a sibling), not the button.
        anchors.left: footer.left
        anchors.leftMargin: 34
        anchors.bottom: footer.bottom
        width: 180
        height: menuColumn.implicitHeight + 12

        Rectangle {
            id: mcpMenuCard
            anchors.fill: parent
            radius: 8
            color: Theme.cardBackground
            border.color: Theme.border
            border.width: 1
        }

        MultiEffect {
            anchors.fill: mcpMenuCard
            source: mcpMenuCard
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.5)
            shadowBlur: 1.0
            shadowVerticalOffset: 4
        }

        Column {
            id: menuColumn
            anchors.fill: parent
            anchors.margins: 6
            spacing: 2

            Rectangle {
                width: parent.width
                height: 32
                radius: 6
                color: enableAllRow.hovered
                    ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                    : "transparent"

                Label {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    text: qsTr("Enable all")
                    color: Theme.textPrimary
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                }

                MouseArea {
                    id: enableAllRow
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        mcpMenu.visible = false
                        sidebar.setAllMcpEnabled(true)
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 32
                radius: 6
                color: disableAllRow.hovered
                    ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                    : "transparent"

                Label {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    text: qsTr("Disable all")
                    color: Theme.textPrimary
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                }

                MouseArea {
                    id: disableAllRow
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        mcpMenu.visible = false
                        sidebar.setAllMcpEnabled(false)
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.border
            }

            Rectangle {
                width: parent.width
                height: 32
                radius: 6
                color: configureRow.hovered
                    ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                    : "transparent"

                Label {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    text: qsTr("Configure…")
                    color: Theme.textPrimary
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                }

                MouseArea {
                    id: configureRow
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        mcpMenu.visible = false
                        sidebar.mcpConfigureRequested()
                    }
                }
            }
        }
    }

    // Controls inside the agent title-bar strip that must stay clickable.
    // Null-guarded: qwindowkit ASSERT-aborts the whole app on null.
    function markHitTest(agent) {
        if (rootFolderButton)
            agent.setHitTestVisible(rootFolderButton, true)
        else
            console.warn("markHitTest: rootFolderButton is null")
    }

    function setAllMcpEnabled(enabled) {        var agents = mcpAgents.scanAgents()
        var changed = 0
        for (var i = 0; i < agents.length; ++i) {
            if (!agents[i].installed) {
                if (enabled && agents[i].found && mcpAgents.installAgent(agents[i].id))
                    ++changed
            } else if (agents[i].enabled !== enabled) {
                if (mcpAgents.setAgentEnabled(agents[i].id, enabled))
                    ++changed
            }
        }
        logModel.add("info", "mcp", (enabled ? "Enabled " : "Disabled ") + changed + " MCP entries")
    }
}
