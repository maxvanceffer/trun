import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// UiSelectMenu — searchable select в визуале Nuxt UI SelectMenu
// (size md, variant outline, dark): триггер-кнопка с шевроном, попап шириной
// в триггер с поиском сверху, опции с чекмарком выбранного справа.
// Токены Nuxt (rounded-md 6, p-1.5, max-h 15rem) живут здесь; глобальные
// цвета/радиусы берутся из Theme (bg-default→cardBackground,
// bg-elevated→mutedSurface, ring→border).
Item {
    id: root

    property var model: []
    property int currentIndex: -1
    property string placeholderText: qsTr("Select…")
    property bool searchable: true
    // Источник шеврона триггера; пусто — рисуем глиф (компонент независим
    // от iconBaseUrl, потребитель вроде DetailPage передаёт ассет сам).
    property string chevronSource: ""
    // Источник иконки выбранного пункта; пусто — глиф "✓".
    property string checkIconSource: ""

    signal activated(int index)

    // Пункты меню: пересчитываются refreshMenu() (на открытие и на ввод
    // в поиск). Именно свойство, а не императивный model=, потому что
    // onCompleted поиска срабатывает раньше создания ListView.
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

    // SelectMenu-специфичная геометрия (в Theme такого шага нет).
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

    // Триггер: outline-кнопка (bg-default, inset-ring, hover bg-elevated).
    Button {
        id: trigger
        anchors.fill: parent
        onClicked: root.open()

        background: Rectangle {
            radius: Theme.radiusSm
            color: trigger.hovered ? Theme.mutedSurface : Theme.cardBackground
            border.color: Theme.border
            border.width: 1
        }

        contentItem: RowLayout {
            spacing: 6

            Label {
                Layout.fillWidth: true
                text: root.displayText
                color: root.currentIndex >= 0 ? Theme.textPrimary : Theme.textMuted
                font.pixelSize: Theme.fontSizeLg
                elide: Text.ElideRight
            }

            Image {
                Layout.preferredWidth: Theme.iconSm
                Layout.preferredHeight: Theme.iconSm
                visible: root.chevronSource !== ""
                source: root.chevronSource
                sourceSize.width: Theme.iconSm
                sourceSize.height: Theme.iconSm
                fillMode: Image.PreserveAspectFit
            }

            Label {
                visible: root.chevronSource === ""
                text: "▾"
                color: Theme.textMuted
                font.pixelSize: 11
            }
        }
    }

    // Попап: bg-default + ring + radius, ширина в триггер.
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
            color: Theme.cardBackground
            border.color: Theme.border
            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 0

            // Поиск внутри меню (как у Nuxt: input с нижней границей).
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
                    width: ListView.view.width
                    checkable: false
                    onClicked: {
                        root.currentIndex = modelData.index
                        menuPopup.close()
                        root.activated(modelData.index)
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
                            text: modelData.label
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeLg
                            elide: Text.ElideRight
                        }

                        // Чекмарк выбранного справа (ms-auto).
                        Image {
                            Layout.preferredWidth: Theme.iconSm
                            Layout.preferredHeight: Theme.iconSm
                            Layout.rightMargin: root.itemPadding
                            visible: modelData.index === root.currentIndex
                                     && root.checkIconSource !== ""
                            source: root.checkIconSource
                            sourceSize.width: Theme.iconSm
                            sourceSize.height: Theme.iconSm
                            fillMode: Image.PreserveAspectFit
                        }

                        Label {
                            rightPadding: root.itemPadding
                            visible: modelData.index === root.currentIndex
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
