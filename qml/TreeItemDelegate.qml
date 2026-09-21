import QtQuick 6.5
import QtQuick.Controls 6.5
import QtQuick.Layouts 6.5

Item {
    id: delegateRoot
    // Root nodes breathe: a small gap separates top-level folders.
    height: Theme.sidebarRowHeight + (depth === 0 ? Theme.spacingXs : 0)
    implicitWidth: 200
    implicitHeight: Theme.sidebarRowHeight

    // Assigned by TreeView (see Qt6 TreeView docs for custom delegates)
    required property TreeView treeView
    required property bool expanded
    required property bool hasChildren
    required property int depth

    // Bind model role values to local properties
    readonly property string _type: model.item_type ?? ""
    readonly property string _folderName: model.folderName ?? ""
    readonly property string _folderPath: model.folderPath ?? ""
    readonly property bool _hasManifests: model.hasManifests ?? false
    readonly property string _gitBranch: model.gitBranch ?? ""

    property bool isFolder: _type === "folder"
    property bool isEntry: _type === "manifests"
    // Entries and leaf folders with manifests open the folder page;
    // hybrid and plain folders expand/collapse instead.
    property bool navigates: delegateRoot.isEntry
        || (delegateRoot.isFolder && delegateRoot._hasManifests && !delegateRoot.hasChildren)

    // The single root node starts expanded so projects are visible.
    Component.onCompleted: {
        if (delegateRoot.isFolder && delegateRoot.depth === 0 && delegateRoot.hasChildren)
            treeView.expand(row)
    }

    signal folderClicked(string folderPath)

    // Sidebar iconography: folder rows (roots included) use the stack,
    // manifest rows the file-terminal marker. No yellow anywhere.
    // iconBaseUrl is an absolute URL set from C++ (file:// in dev, bundle path in prod).
    property string iconSource: {
        if (delegateRoot.depth !== 0
            && (delegateRoot.isEntry || delegateRoot._hasManifests))
            return iconBaseUrl + (Theme.isDark ? "file-terminal-dark.png" : "file-terminal.png")
        return iconBaseUrl + (Theme.isDark ? "folders-dark.png" : "folders.png")
    }

    Rectangle {
        id: bgRect
        anchors.top: parent.top
        height: Theme.sidebarRowHeight
        anchors.left: contentRow.left
        anchors.leftMargin: -Theme.spacingXs
        anchors.right: parent.right
        color: {
            // Highlight belongs to rows that open a page (entries and
            // leaf folders with manifests). Plain container folders never
            // highlight yellow, pressed or selected.
            if (delegateRoot.navigates && mouseArea.pressed)
                return Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
            if (delegateRoot.navigates && delegateRoot._folderPath !== ""
                && treeView.selectedFolderPath === delegateRoot._folderPath)
                return Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.25)
            return "transparent"
        }
        radius: Theme.radiusSm
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        onClicked: {
            if (delegateRoot.navigates) {
                logModel.add("info", "sidebar", "Folder clicked: " + delegateRoot._folderPath)
                // Direct call: context properties resolve everywhere,
                // unlike cross-file id lookups
                projectService.selectFolder(delegateRoot._folderPath)
                delegateRoot.folderClicked(delegateRoot._folderPath)
            } else if (delegateRoot.isFolder) {
                treeView.toggleExpanded(row)
            }
        }
    }

    RowLayout {
        id: contentRow
        anchors.left: parent.left
        anchors.leftMargin: depth * Theme.sidebarIndent
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacingXs
        anchors.verticalCenter: bgRect.verticalCenter
        spacing: Theme.spacingXs

        // Expand/collapse indicator for folders (zero size for leaves):
        // chevron-right when collapsed, chevron-down when expanded.
        Image {
            id: chevron
            Layout.preferredWidth: (delegateRoot.isFolder && delegateRoot.hasChildren) ? Theme.iconXs : 0
            Layout.preferredHeight: (delegateRoot.isFolder && delegateRoot.hasChildren) ? Theme.iconXs : 0
            Layout.alignment: Qt.AlignVCenter
            source: delegateRoot.expanded
                ? iconBaseUrl + (Theme.isDark ? "chevron-down-dark.png" : "chevron-down.png")
                : iconBaseUrl + (Theme.isDark ? "chevron-right-dark.png" : "chevron-right.png")
            sourceSize.width: Theme.iconSm
            sourceSize.height: Theme.iconSm
            fillMode: Image.PreserveAspectFit
            smooth: true
            visible: delegateRoot.isFolder && delegateRoot.hasChildren
        }

        // Icon (32px source, aspect-fitted into a 16px box)
        Image {
            id: iconImage
            Layout.preferredWidth: Theme.iconSm
            Layout.preferredHeight: Theme.iconSm
            Layout.leftMargin: Theme.spacingXs
            source: iconSource
            sourceSize.width: 32
            sourceSize.height: 32
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        // Name
        Label {
            id: nameLabel
            text: _folderName
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeMd
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        UiTag {
            id: branchTag
            readonly property bool branchVisible: Settings.showGitBranch && _gitBranch !== ""
                && (!Settings.gitBranchTopOnly || depth === 0)
            visible: branchVisible
            label: _gitBranch
            color: "primary"
            variant: "subtle"
            size: "sm"
            iconSource: iconBaseUrl + "git-branch.png"
            Layout.fillWidth: false
            Layout.alignment: Qt.AlignVCenter
            Layout.maximumWidth: 112
        }
    }
}
