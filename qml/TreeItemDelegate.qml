import QtQuick 6.5
import QtQuick.Controls 6.5
import QtQuick.Layouts 6.5

Item {
    id: delegateRoot
    height: 32
    implicitWidth: 200
    implicitHeight: 32

    // Assigned by TreeView (see Qt6 TreeView docs for custom delegates)
    required property TreeView treeView
    required property bool expanded
    required property bool hasChildren
    required property int depth

    // Bind model role values to local properties
    readonly property string _type: model.item_type ?? ""
    readonly property string _folderName: model.folderName ?? ""
    readonly property string _folderPath: model.folderPath ?? ""
    readonly property string _name: model.name ?? ""
    readonly property string _projectPath: model.project_id ?? ""
    readonly property string _manifest: model.manifest ?? ""
    readonly property string _description: model.description ?? ""
    readonly property var _commands: model.commands ?? []

    property bool isFolder: _type === "folder"
    property bool isProject: _type === "project"

    signal projectClicked(string projectId)
    signal addCustomRequested(string folderPath)

    // Icon source based on manifest type (for projects) or folder (default).
    // iconBaseUrl is an absolute URL set from C++ (file:// in dev, bundle path in prod).
    property string iconSource: {
        if (_type === "project") {
            switch (_manifest) {
                case "package.json":    return iconBaseUrl + "npm-32px.png"
                case "Cargo.toml":      return iconBaseUrl + (Theme.isDark ? "rust-dark-32px.png" : "rust-light-32px.png")
                case "go.mod":          return iconBaseUrl + (Theme.isDark ? "go-dark-32px.png" : "go-light-32px.png")
                case "pyproject.toml":  return iconBaseUrl + "python-32px.png"
                case "pom.xml":         return iconBaseUrl + "java-32px.png"
                case "build.gradle":    return iconBaseUrl + "gradle-32px.png"
                case "CMakeLists.txt":  return iconBaseUrl + "cmake-32px.png"
                case "composer.json":   return iconBaseUrl + (Theme.isDark ? "php-dark-32px.png" : "php-32px.png")
                case "Gemfile":         return iconBaseUrl + "ruby-32px.png"
                case "mix.exs":         return iconBaseUrl + "elixir-32px.png"
                default:                return iconBaseUrl + "default-32px.png"
            }
        }
        return iconBaseUrl + "folder-32px.png"
    }

    Rectangle {
        id: bgRect
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: contentRow.left
        anchors.leftMargin: -4
        anchors.right: parent.right
        color: {
            if (mouseArea.pressed) return Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
            if (delegateRoot.isProject && treeView.selectedProjectId === delegateRoot._projectPath) return Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.25)
            return "transparent"
        }
        radius: 6
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        onClicked: {
            if (delegateRoot.isFolder) {
                treeView.toggleExpanded(row)
            } else {
                logModel.add("info", "sidebar", "Project clicked: " + delegateRoot._projectPath)
                // Direct call: context properties resolve everywhere,
                // unlike cross-file id lookups
                projectService.selectProject(delegateRoot._projectPath)
                delegateRoot.projectClicked(delegateRoot._projectPath)
            }
        }
    }

    RowLayout {
        id: contentRow
        anchors.left: parent.left
        anchors.leftMargin: depth * 20
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4

        // Expand/collapse indicator for folders (zero size for leaves)
        Image {
            Layout.preferredWidth: (delegateRoot.isFolder && delegateRoot.hasChildren) ? 12 : 0
            Layout.preferredHeight: (delegateRoot.isFolder && delegateRoot.hasChildren) ? 12 : 0
            Layout.alignment: Qt.AlignVCenter
            source: delegateRoot.expanded
                ? iconBaseUrl + (Theme.isDark ? "chevron-up-dark.png" : "chevron-up.png")
                : iconBaseUrl + (Theme.isDark ? "chevron-down-dark.png" : "chevron-down.png")
            sourceSize.width: 16
            sourceSize.height: 16
            fillMode: Image.PreserveAspectFit
            smooth: true
            visible: delegateRoot.isFolder && delegateRoot.hasChildren
        }

        // Icon (32px source, aspect-fitted into 16px box)
        Image {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            Layout.leftMargin: 4
            source: iconSource
            sourceSize.width: 32
            sourceSize.height: 32
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        // Name
        Label {
            text: delegateRoot.isFolder ? _folderName : _name
            color: delegateRoot.isFolder ? Theme.textMuted : Theme.textPrimary
            font.pixelSize: 12
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        // Command count (only for projects)
        Label {
            text: delegateRoot.isProject && _commands
                ? String(_commands.length)
                : ""
            color: Theme.textMuted
            font.pixelSize: 9
            Layout.leftMargin: 4
            visible: delegateRoot.isProject && _commands && _commands.length > 0
        }

        // Add custom command (folders only, right-aligned, on hover)
        IconButton {
            id: addButton
            Layout.alignment: Qt.AlignVCenter
            visible: delegateRoot.isFolder
                && (mouseArea.containsMouse || hovered)
            iconSource: iconBaseUrl + (Theme.isDark ? "square-plus-dark.png" : "square-plus.png")
            tooltipText: qsTr("Add custom command")
            onClicked: delegateRoot.addCustomRequested(delegateRoot._folderPath)
        }
    }
}
