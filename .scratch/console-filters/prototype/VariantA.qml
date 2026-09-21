import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "LogParse.js" as LogParse

// PROTOTYPE variant A «Тулбар»: одна строка контролов + плоская лента чипсов.
// Вопрос прототипа: три структурно разных панели фильтров на одном экране.
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
    property var result: ({ shown: [], total: 0, hiddenUnparsed: 0 })

    property var timeModel: [
        { label: "Всё время", v: -1 },
        { label: "5 мин", v: 5 },
        { label: "10 мин", v: 10 },
        { label: "30 мин", v: 30 }
    ]

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
        timeBox.currentIndex = 0
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
            ComboBox {
                id: timeBox
                model: ["Всё время", "5 мин", "10 мин", "30 мин"]
                font.pixelSize: 12
                onCurrentIndexChanged: { root.sinceMin = root.timeModel[currentIndex].v; root.refresh() }
            }
            Button {
                text: "Сбросить"
                font.pixelSize: 12
                onClicked: root.reset()
            }
            Label {
                color: "#abab9c"
                font.pixelSize: 11
                text: "Показано " + root.result.shown.length + " из " + root.result.total
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: LogParse.observed(root.items).levels
                Button {
                    text: modelData
                    checkable: true
                    font.pixelSize: 11
                    onClicked: { root.selLevels = root.toggle(root.selLevels, modelData); root.refresh() }
                }
            }
            Repeater {
                model: LogParse.observed(root.items).channels
                Button {
                    text: modelData
                    checkable: true
                    font.pixelSize: 11
                    onClicked: { root.selChannels = root.toggle(root.selChannels, modelData); root.refresh() }
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
