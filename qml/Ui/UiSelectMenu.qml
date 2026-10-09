import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../"

pragma ComponentBehavior: Bound

// UiSelectMenu — searchable select styled after Nuxt UI SelectMenu
// (size md, variant outline, dark): a trigger button with a chevron, a
// popup as wide as the trigger with a search field on top, options with
// a checkmark for the selected one on the right. The Nuxt tokens
// (rounded-md 6, p-1.5, max-h 15rem) live here; global colors/radii
// come from Theme (bg-default→backgroundDefault,
// bg-elevated→mutedSurface, ring→border).
Item {
    id: root

    property var model: []
    property int currentIndex: -1
    property string placeholderText: qsTr("Select…")
    property bool searchable: true
    // Chevron source for the trigger; empty — draw the glyph (the
    // component is independent of Theme.icon, consumers like DetailPage
    // pass their own asset).
    property string chevronSource: ""
    // Icon source of the selected entry; empty — the "✓" glyph.
    property string checkIconSource: ""
    // Trigger label colour while a selection exists. Consumer-driven like
    // chevronSource/checkIconSource (the API URL bar passes the method
    // colour); the default keeps textPrimary for all existing call sites.
    property color selectedColor: Theme.textPrimary

    signal activated(int index)

    // Menu entries: recomputed by refreshMenu() (on open and while
    // typing in the search). A property, not an imperative model=,
    // because the search field's onCompleted fires before the ListView
    // is created.
    property var menuEntries: []

    function refreshMenu() {
        var q = searchField.text.toLowerCase()
        var out = []
        for (var i = 0; i < root.model.length; ++i) {
            if (q === "" || root.labelAt(i).toLowerCase().indexOf(q) >= 0)
                out.push({ label: root.labelAt(i), index: i })
        }
        root.menuEntries = out
    }

    // SelectMenu-specific geometry (no step for it in Theme).
    readonly property int itemPadding: 6
    readonly property int popupMaxHeight: 240

    function labelAt(i) {
        var v = root.model[i]
        if (v === undefined || v === null) return ""
        return (typeof v === "object") ? (v.label ?? "") : String(v)
    }

    readonly property string displayText: root.currentIndex >= 0
        ? root.labelAt(root.currentIndex) : root.placeholderText

    implicitWidth: 160
    implicitHeight: Theme.controlHeight

    function open() {
        searchField.text = ""
        menuPopup.open()
    }

    // Trigger: an outline button (bg-default, inset-ring, hover bg-elevated).
    Button {
        id: trigger
        anchors.fill: parent
        onClicked: root.open()

        background: Rectangle {
            radius: Theme.radiusSm
            color: trigger.hovered ? Theme.mutedSurface : Theme.backgroundDefault
            border.color: Theme.border
            border.width: 1
        }

        contentItem: RowLayout {
            spacing: 6

            Label {
                Layout.fillWidth: true
                text: root.displayText
                color: root.currentIndex >= 0 ? root.selectedColor : Theme.textMuted
                font.pixelSize: Theme.fontSizeLg
                elide: Text.ElideRight
            }

            Image {
                Layout.preferredWidth: Theme.iconSm
                Layout.preferredHeight: Theme.iconSm
                visible: root.chevronSource !== ""
                source: root.chevronSource
                // 2× decode (64px art → 32px texture in a 16px box): the
                // sharp-icon convention of the tree's iconImage — a 16px
                // texture is stretched to 32 physical px on Retina and
                // reads blurry.
                sourceSize.width: Theme.iconSm * 2
                sourceSize.height: Theme.iconSm * 2
                fillMode: Image.PreserveAspectFit
            }

            // Trigger chevron when no chevronSource is supplied: the shared
            // chevron icon downscaled from its 64px PNG source, drawn the
            // same way the project tree draws it, with the 2× decode so it
            // stays crisp on Retina.
            Image {
                visible: root.chevronSource === ""
                Layout.preferredWidth: Theme.iconSm
                Layout.preferredHeight: Theme.iconSm
                Layout.alignment: Qt.AlignVCenter
                source: Theme.icon("chevron-down")
                sourceSize.width: Theme.iconSm * 2
                sourceSize.height: Theme.iconSm * 2
                fillMode: Image.PreserveAspectFit
                smooth: true
            }
        }
    }

    // Popup: bg-default + ring + radius, as wide as the trigger.
    Popup {
        id: menuPopup
        parent: root
        x: 0
        y: root.height + 4
        width: root.width
        padding: 4
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: Theme.radiusSm
            color: Theme.backgroundDefault // content: bg-default
            border.color: Theme.border
            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 0

            // Search inside the popup (Nuxt-style: an input with a
            // bottom border).
            UiInput {
                id: searchField
                Layout.fillWidth: true
                visible: root.searchable
                placeholderText: qsTr("Search…")
                leftPadding: root.itemPadding
                rightPadding: root.itemPadding
                topPadding: 4
                bottomPadding: 4
                background: Rectangle {
                    color: "transparent"
                    border.color: "transparent"
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 1
                        color: Theme.border
                    }
                }
                onTextChanged: root.refreshMenu()
            }

            ListView {
                id: listView
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(root.popupMaxHeight, contentHeight)
                clip: true
                currentIndex: -1
                model: root.menuEntries
                ScrollBar.vertical: ScrollBar { active: true }

                delegate: Button {
                    id: rowBtn
                    required property var modelData
                    width: ListView.view.width
                    checkable: false
                    onClicked: {
                        root.currentIndex = rowBtn.modelData.index
                        menuPopup.close()
                        root.activated(rowBtn.modelData.index)
                    }

                    background: Rectangle {
                        radius: Theme.radiusSm
                        color: rowBtn.hovered || ListView.isCurrentItem
                            ? Qt.rgba(Theme.mutedSurface.r, Theme.mutedSurface.g,
                                      Theme.mutedSurface.b, 0.5)
                            : "transparent"
                    }

                    contentItem: RowLayout {
                        spacing: 6

                        Label {
                            Layout.fillWidth: true
                            leftPadding: root.itemPadding
                            text: rowBtn.modelData.label
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeLg
                            elide: Text.ElideRight
                        }

                        // Checkmark of the selected entry, right-aligned (ms-auto).
                        Image {
                            Layout.preferredWidth: Theme.iconSm
                            Layout.preferredHeight: Theme.iconSm
                            Layout.rightMargin: root.itemPadding
                            visible: rowBtn.modelData.index === root.currentIndex
                                      && root.checkIconSource !== ""
                            source: root.checkIconSource
                            sourceSize.width: Theme.iconSm * 2
                            sourceSize.height: Theme.iconSm * 2
                            fillMode: Image.PreserveAspectFit
                        }

                        Label {
                            rightPadding: root.itemPadding
                            visible: rowBtn.modelData.index === root.currentIndex
                                      && root.checkIconSource === ""
                            text: "✓"
                            color: Theme.accent
                            font.pixelSize: Theme.fontSizeLg
                            font.bold: true
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: listView.count === 0
                    text: qsTr("No results")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeMd
                }

                Keys.onUpPressed: listView.decrementCurrentIndex()
                Keys.onDownPressed: listView.incrementCurrentIndex()
                Keys.onReturnPressed: {
                    if (listView.currentIndex >= 0) {
                        var entry = listView.model[listView.currentIndex]
                        root.currentIndex = entry.index
                        menuPopup.close()
                        root.activated(entry.index)
                    }
                }
            }
        }

        onOpened: {
            root.refreshMenu()
            listView.currentIndex = -1
            if (root.searchable)
                searchField.forceActiveFocus()
            else
                listView.forceActiveFocus()
        }
    }

    Keys.onDownPressed: root.open()
}
