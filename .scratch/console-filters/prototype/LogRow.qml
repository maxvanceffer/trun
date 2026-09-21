import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// PROTOTYPE (throwaway): одна строка Console history.
// Жирная строка (>500) показана обрезанной с кнопкой «развернуть».
ColumnLayout {
    id: row
    width: ListView.view ? ListView.view.width - 16 : 400
    spacing: 2

    property var entry: null
    property bool expanded: false
    readonly property int cutAt: 500
    readonly property string fullMsg: entry && entry.p.ok ? entry.p.message : (entry ? entry.raw : "")
    readonly property bool isLong: fullMsg.length > cutAt

    function levelColor(lv) {
        if (lv === "ERROR" || lv === "CRITICAL" || lv === "ALERT" || lv === "EMERGENCY") return "#ff6467"
        if (lv === "WARNING") return "#ff9800"
        if (lv === "DEBUG") return "#abab9c"
        return "#fbfbf9"
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        font.family: "Menlo"
        font.pixelSize: 11
        color: row.entry && row.entry.p.ok ? row.levelColor(row.entry.p.level) : "#fbfbf9"
        text: {
            if (!row.entry) return ""
            if (!row.entry.p.ok) return "[не распознано] " + row.entry.raw
            return "[" + row.entry.p.level + "] [" + row.entry.p.channel + "] " + row.entry.p.target
        }
    }
    Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        font.family: "Menlo"
        font.pixelSize: 11
        color: "#fbfbf9"
        text: row.expanded || !row.isLong ? row.fullMsg : row.fullMsg.slice(0, row.cutAt) + "…"
    }
    Button {
        id: expandBtn
        visible: row.isLong
        flat: true
        font.pixelSize: 11
        text: row.expanded ? "свернуть" : "развернуть (+" + (row.fullMsg.length - row.cutAt) + " симв.)"
        contentItem: Label {
            text: expandBtn.text
            color: "#f0b100"
            font.pixelSize: 11
        }
        onClicked: row.expanded = !row.expanded
    }
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: "#242422"
    }
}
