import QtQuick
import QtQuick.Controls 6.5
import QtQuick.Layouts 6.5
pragma ComponentBehavior: Bound

Rectangle {
    id: sidebar
    width: Theme.sidebarWidth
    color: "transparent"

    readonly property var mcp: mcpAgents // qmllint disable unqualified
    readonly property var lm: logModel // qmllint disable unqualified
    readonly property var projSvc: projectService // qmllint disable unqualified
    readonly property var tm: treeModel // qmllint disable unqualified
    readonly property var upd: updater // qmllint disable unqualified

    // Square when maximized, rounded outer corners otherwise
    readonly property int cornerR: (Window.visibility === Window.Maximized
                                    || Window.visibility === Window.FullScreen) ? 0 : Theme.radiusLg

    // Frosted band: translucent over the native blur on macOS so the
    // desktop shows through, solid theme tone elsewhere. The solid
    // content surface on the right is what clips it to the sidebar.
    Rectangle {
        anchors.fill: parent
        topLeftRadius: sidebar.cornerR
        bottomLeftRadius: sidebar.cornerR
        color: Qt.platform.os === "osx"
            ? Qt.rgba(Theme.sidebarBackground.r, Theme.sidebarBackground.g,
                      Theme.sidebarBackground.b, Theme.isDark ? 0.62 : 0.55)
            : Theme.sidebarBackground
    }

    signal folderSelected(string folderPath)
    signal dashboardRequested()
    signal databasesRequested()
    signal dockerRequested()
    signal apiRequested()
    signal mcpConfigureRequested()
    signal updateCheckRequested()
    signal addProjectsRequested()
    signal settingsRequested()
    signal closeRequested()
    signal minimizeRequested()
    signal maximizeRequested()

    // Connect log messages
    Connections {
        target: sidebar.projSvc

        function onLogMessage(level, target, message) {
            sidebar.lm.add(level, target, message)
        }
    }

    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.titleBarHeight
        color: "transparent"

        // Own traffic lights: macOS uses the native ones kept by
        // QWindowKit; other platforms draw their own.
        TrafficLights {
            id: trafficLights
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            visible: Qt.platform.os !== "osx"
            windowActive: Window.active
            onCloseRequested: sidebar.closeRequested()
            onMinimizeRequested: sidebar.minimizeRequested()
            onMaximizeRequested: sidebar.maximizeRequested()
        }
    }

    // Static Dashboard entry, always present above the project list.
    Item {
        id: dashboardRow
        objectName: "dashboardRow"
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.sidebarRowHeight

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 1
            anchors.bottomMargin: 1
            anchors.leftMargin: Theme.spacingSm
            anchors.rightMargin: Theme.spacingSm
            radius: Theme.radiusSm
            color: dashMouse.pressed
                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
                : (dashMouse.containsMouse
                    ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                    : "transparent")
        }

        RowLayout {
            anchors.left: parent.left
            anchors.leftMargin: Theme.sidebarInset
            anchors.right: parent.right
            anchors.rightMargin: Theme.sidebarInset
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.spacingSm

            Image {
                Layout.preferredWidth: Theme.iconSm
                Layout.preferredHeight: Theme.iconSm
                Layout.alignment: Qt.AlignVCenter
                source: Theme.icon("dashboard")
                sourceSize.width: 32
                sourceSize.height: 32
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Label {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: qsTr("Dashboard")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeMd
                elide: Text.ElideRight
            }
        }

        MouseArea {
            id: dashMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: sidebar.dashboardRequested()
        }
    }

    // Static sections, right below Dashboard.
    Item {
        id: databasesHeader
        objectName: "databasesHeader"
        anchors.top: dashboardRow.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.sidebarRowHeight

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 1
            anchors.bottomMargin: 1
            anchors.leftMargin: Theme.spacingSm
            anchors.rightMargin: Theme.spacingSm
            radius: Theme.radiusSm
            color: databasesMouse.containsMouse
                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                : "transparent"
        }

        RowLayout {
            anchors.left: parent.left
            anchors.leftMargin: Theme.sidebarInset
            anchors.right: parent.right
            anchors.rightMargin: Theme.spacingSm
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.spacingSm

            Image {
                Layout.preferredWidth: Theme.iconSm
                Layout.preferredHeight: Theme.iconSm
                Layout.alignment: Qt.AlignVCenter
                source: Theme.icon("database")
                sourceSize.width: 32
                sourceSize.height: 32
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Label {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: qsTr("Databases")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeMd
                elide: Text.ElideRight
            }
        }

        MouseArea {
            id: databasesMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: sidebar.databasesRequested()
        }
    }

    Item {
        id: dockerHeader
        objectName: "dockerHeader"
        anchors.top: databasesHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.sidebarRowHeight

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 1
            anchors.bottomMargin: 1
            anchors.leftMargin: Theme.spacingSm
            anchors.rightMargin: Theme.spacingSm
            radius: Theme.radiusSm
            color: dockerMouse.containsMouse
                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                : "transparent"
        }

        RowLayout {
            anchors.left: parent.left
            anchors.leftMargin: Theme.sidebarInset
            anchors.right: parent.right
            anchors.rightMargin: Theme.spacingSm
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.spacingSm

            Image {
                Layout.preferredWidth: Theme.iconSm
                Layout.preferredHeight: Theme.iconSm
                Layout.alignment: Qt.AlignVCenter
                source: Theme.icon("docker")
                sourceSize.width: 32
                sourceSize.height: 32
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Label {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: qsTr("Docker")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeMd
                elide: Text.ElideRight
            }
        }

        MouseArea {
            id: dockerMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: sidebar.dockerRequested()
        }
    }

    Item {
        id: apiHeader
        objectName: "apiHeader"
        anchors.top: dockerHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.sidebarRowHeight

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 1
            anchors.bottomMargin: 1
            anchors.leftMargin: Theme.spacingSm
            anchors.rightMargin: Theme.spacingSm
            radius: Theme.radiusSm
            color: apiMouse.containsMouse
                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                : "transparent"
        }

        RowLayout {
            anchors.left: parent.left
            anchors.leftMargin: Theme.sidebarInset
            anchors.right: parent.right
            anchors.rightMargin: Theme.spacingSm
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.spacingSm

            Image {
                Layout.preferredWidth: Theme.iconSm
                Layout.preferredHeight: Theme.iconSm
                Layout.alignment: Qt.AlignVCenter
                source: Theme.icon("globe-code")
                sourceSize.width: 32
                sourceSize.height: 32
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Label {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: qsTr("API")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeMd
                elide: Text.ElideRight
            }
        }

        MouseArea {
            id: apiMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: sidebar.apiRequested()
        }
    }

    // Fixed section title above the dynamically added projects.
    Item {
        id: sectionHeader
        anchors.top: apiHeader.bottom
        anchors.topMargin: Theme.spacingMd
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.sidebarRowHeight

        Label {
            anchors.left: parent.left
            anchors.leftMargin: Theme.sidebarInset
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Projects")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSm
            font.bold: true
        }
    }

    ScrollView {
        anchors.top: sectionHeader.bottom
        anchors.topMargin: Theme.spacingXs
        anchors.bottom: footer.top
        anchors.bottomMargin: Theme.spacingXs
        anchors.left: parent.left
        anchors.leftMargin: Theme.sidebarInset
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacingSm

        TreeView {
            id: tree
            anchors.fill: parent
            clip: true
            model: sidebar.tm
            delegate: TreeItemDelegate {
                onFolderClicked: function(folderPath) {
                    tree.selectedFolderPath = folderPath
                    sidebar.folderSelected(folderPath)
                }
            }

            // Single column spans the full width so row highlight
            // covers the whole sidebar
            columnWidthProvider: function(column) { return tree.width }

            // Selected folder path (entries share their parent's path,
            // so only entry rows ever match it for highlight).
            // Expansion itself is owned by TreeView (toggleExpanded).
            property string selectedFolderPath: ""
        }
    }

    // Drops the highlight, e.g. when leaving the folder page.
    function clearFolderSelection() {
        tree.selectedFolderPath = ""
    }

    // Footer: MCP dialog shortcut, add projects, update check
    Item {
        id: footer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.sidebarInset
        anchors.rightMargin: Theme.spacingSm
        anchors.bottomMargin: Theme.spacingSm
        height: Theme.sidebarRowHeight

        IconButton {
            id: mcpButton
            objectName: "mcpButton"
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            iconSource: Theme.icon("mcp")
            tooltipText: qsTr("MCP servers")
            onClicked: sidebar.mcpConfigureRequested()
        }

        IconButton {
            id: addProjectsButton
            objectName: "addProjectsButton"
            anchors.left: mcpButton.right
            anchors.leftMargin: Theme.spacingXs
            anchors.verticalCenter: parent.verticalCenter
            iconSource: Theme.icon("folder-plus")
            tooltipText: qsTr("Add projects")
            onClicked: sidebar.addProjectsRequested()
        }

        IconButton {
            id: settingsButton
            objectName: "settingsButton"
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            iconSource: Theme.icon("cog")
            tooltipText: qsTr("Settings")
            onClicked: sidebar.settingsRequested()
        }

        IconButton {
            id: updateButton
            objectName: "updateButton"
            // macOS uses the native app menu (trun → Check for Updates…);
            // the footer button remains for platforms without one.
            visible: Qt.platform.os !== "osx"
            anchors.right: settingsButton.left
            anchors.rightMargin: Theme.spacingXs
            anchors.verticalCenter: parent.verticalCenter
            iconSource: Theme.icon("refresh")
            tooltipText: sidebar.upd.updateAvailable ? qsTr("Update available") : qsTr("Check for updates")
            active: sidebar.upd.updateAvailable
            onClicked: sidebar.updateCheckRequested()
        }
    }

    // Controls inside the agent title-bar strip that must stay clickable.
    // Null-guarded: qwindowkit ASSERT-aborts the whole app on null.
    function markHitTest(agent) {
        // Native lights on macOS are clickable by themselves; only the
        // custom lights need to be handed to the agent.
        if (Qt.platform.os === "osx")
            return
        for (var i = 0; i < 3; ++i) {
            var b = trafficLights.buttonAt(i)
            if (b)
                agent.setHitTestVisible(b, true)
            else
                console.warn("markHitTest: traffic button " + i + " is null")
        }
    }

    function setAllMcpEnabled(enabled) {        var agents = mcp.scanAgents()
        var changed = 0
        for (var i = 0; i < agents.length; ++i) {
            if (!agents[i].installed) {
                if (enabled && agents[i].found && mcp.installAgent(agents[i].id))
                    ++changed
            } else if (agents[i].enabled !== enabled) {
                if (mcp.setAgentEnabled(agents[i].id, enabled))
                    ++changed
            }
        }
        lm.add("info", "mcp", (enabled ? "Enabled " : "Disabled ") + changed + " MCP entries")
    }
}
