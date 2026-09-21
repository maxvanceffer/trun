import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "LogParse.js" as LogParse

// PROTOTYPE variant C «Компакт»: один ряд (поиск + кнопка «Фильтры (n)» +
// счётчик), всё остальное — в раскрывающейся панели (progressive disclosure).
Rectangle {
    id: root
    color: "#1d1d16"
    radius: 8
    border.color: "#242422"
    border.width: 1

    property var items: []
    property string query: ""
    property var selLevels: []
    property var selChannels: []
    property int sinceMin: -1
    property bool panelOpen: false
    property var result: ({ shown: [], total: 0, hiddenUnparsed: 0 })
    readonly property int activeCount: root.selLevels.length + root.selChannels.length + (root.sinceMin >= 0 ? 1 : 0)

    function refresh() {
        root.result = LogParse.applyFilters(root.items,
            { query: root.query, levels: root.selLevels, channels: root.selChannels, sinceMin: root.sinceMin })
    }
    function toggle(arr, v) {
        var i = arr.indexOf(v), out = arr.slice()
        if (i < 0) out.push(v); else out.splice(i, 1)
        return out
    }
    function reset() {
        root.query = ""
        root.selLevels = []
        root.selChannels = []
        root.sinceMin = -1
        searchField.text = ""
        root.refresh()
    }
    Component.onCompleted: root.refresh()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label { text: "CONSOLE"; color: "#fbfbf9"; font.pixelSize: 10; font.bold: true }
            TextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: "Фильтр-текст (substring)…"
                font.pixelSize: 12
                onTextChanged: { root.query = text; root.refresh() }
            }
            Button {
                font.pixelSize: 12
                highlighted: root.activeCount > 0
                text: root.activeCount > 0 ? "Фильтры (" + root.activeCount + ")" : "Фильтры"
                onClicked: root.panelOpen = !root.panelOpen
            }
            Label {
                color: "#abab9c"
                font.pixelSize: 11
                text: root.result.shown.length + " / " + root.result.total
            }
        }

        Rectangle {
            Layout.fillWidth: true
            visible: root.panelOpen
            color: "#2b2b22"
            radius: 6
            border.color: "#242422"
            border.width: 1
            implicitHeight: panelCol.implicitHeight + 16

            ColumnLayout {
                id: panelCol
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                Label { text: "Уровень"; color: "#abab9c"; font.pixelSize: 10; font.bold: true }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: LogParse.observed(root.items).levels
                        Button {
                            text: modelData
                            checkable: true
                            checked: root.selLevels.indexOf(modelData) >= 0
                            font.pixelSize: 11
                            onClicked: { root.selLevels = root.toggle(root.selLevels, modelData); root.refresh() }
                        }
                    }
                }
                Label { text: "Канал (из потока)"; color: "#abab9c"; font.pixelSize: 10; font.bold: true }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: LogParse.observed(root.items).channels
                        Button {
                            text: modelData
                            checkable: true
                            checked: root.selChannels.indexOf(modelData) >= 0
                            font.pixelSize: 11
                            onClicked: { root.selChannels = root.toggle(root.selChannels, modelData); root.refresh() }
                        }
                    }
                }
                RowLayout {
                    spacing: 6
                    Label { text: "Время:"; color: "#abab9c"; font.pixelSize: 11 }
                    Repeater {
                        model: [{ label: "Всё", v: -1 }, { label: "5 мин", v: 5 }, { label: "10 мин", v: 10 }, { label: "30 мин", v: 30 }]
                        Button {
                            text: modelData.label
                            checkable: true
                            checked: root.sinceMin === modelData.v
                            font.pixelSize: 11
                            onClicked: { root.sinceMin = modelData.v; root.refresh() }
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Button { text: "Сбросить"; font.pixelSize: 11; onClicked: root.reset() }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.result.shown.length === 0
            wrapMode: Text.WordWrap
            color: "#abab9c"
            font.pixelSize: 12
            text: "Нет совпадений — ослабь фильтры."
                + (root.result.hiddenUnparsed > 0 ? " Скрыто нераспознанных строк: " + root.result.hiddenUnparsed + "." : "")
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: root.result.shown
            delegate: LogRow { entry: modelData }
            ScrollBar.vertical: ScrollBar { active: true }
        }
    }
}
