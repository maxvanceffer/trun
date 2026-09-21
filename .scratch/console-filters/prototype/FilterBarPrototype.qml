import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "SampleData.js" as SampleData
import "LogParse.js" as LogParse

// PROTOTYPE (throwaway): три варианта панели фильтров Console на одном экране.
// Запуск: qml FilterBarPrototype.qml   (стрелки ←/→ или кнопки снизу)
// Вопрос: какой вариант забираем в DetailPage (рамка со скриншота)?
// Допущения: палитра захардкожена из Theme (dark); время строк — мок ageMin;
// в прод НЕ тащить как есть — только проверенное решение.
ApplicationWindow {
    id: win
    visible: true
    width: 920
    height: 720
    minimumWidth: 640
    minimumHeight: 480
    title: "PROTOTYPE — фильтры Console (throwaway)"
    color: "#0c0c09"

    property int variant: 0
    property var variants: [
        { key: "A", name: "Тулбар" },
        { key: "B", name: "Секции" },
        { key: "C", name: "Компакт" }
    ]
    // Парсим мок один раз, варианты делят данные и движок (общее — только это).
    property var items: {
        var raw = SampleData.entries(), out = []
        for (var i = 0; i < raw.length; ++i)
            out.push({ raw: raw[i].raw, ageMin: raw[i].ageMin, p: LogParse.parseLine(raw[i].raw) })
        return out
    }

    function step(d) {
        win.variant = (win.variant + d + win.variants.length) % win.variants.length
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        anchors.bottomMargin: 64
        spacing: 8

        Label {
            color: "#abab9c"
            font.pixelSize: 11
            text: "Вариант " + win.variants[win.variant].key + " — " + win.variants[win.variant].name
                + "  ·  12 мок-строк из корпуса (файл + консоль + vite/npm/next)"
        }

        VariantA { Layout.fillWidth: true; Layout.fillHeight: true; visible: win.variant === 0; items: win.items }
        VariantB { Layout.fillWidth: true; Layout.fillHeight: true; visible: win.variant === 1; items: win.items }
        VariantC { Layout.fillWidth: true; Layout.fillHeight: true; visible: win.variant === 2; items: win.items }
    }

    // Плавающий переключатель вариантов (часть прототипа, не дизайна).
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        width: switchRow.implicitWidth + 32
        height: 44
        radius: 22
        color: "#f0b100"
        Row {
            id: switchRow
            anchors.centerIn: parent
            spacing: 12
            Button { text: "←"; font.pixelSize: 14; flat: true; onClicked: win.step(-1) }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: 13
                font.bold: true
                color: "#733e0a"
                text: win.variants[win.variant].key + " (" + win.variants[win.variant].name + ")"
            }
            Button { text: "→"; font.pixelSize: 14; flat: true; onClicked: win.step(1) }
        }
    }

    Shortcut { sequence: "Left"; onActivated: win.step(-1) }
    Shortcut { sequence: "Right"; onActivated: win.step(1) }
}
