import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// UiCheckBox — чекбокс в визуале Nuxt UI Checkbox: бокс 18px с радиусом,
// пустой с ring-границей; checked — заливка accent с чёрной галкой
// (check.png читается на accent в обеих темах, поэтому без -dark пары).
// Состояние хранит потребитель: checked — биндинг сверху, переключение
// уходит наружу через toggled (внутри checked не присваиваем, чтобы не
// рвать биндинг).
RowLayout {
    id: root
    spacing: 8

    property bool checked: false
    property string label: ""

    signal toggled(bool on)

    // Размер/радиус бокса — специфичны для компонента.
    readonly property int boxSize: 18
    readonly property int boxRadius: 4

    function flip() { root.toggled(!root.checked) }

    Rectangle {
        Layout.preferredWidth: root.boxSize
        Layout.preferredHeight: root.boxSize
        Layout.alignment: Qt.AlignVCenter
        radius: root.boxRadius
        color: root.checked ? Theme.accent : "transparent"
        border.color: root.checked ? Theme.accent : Theme.border
        border.width: 1

        Image {
            anchors.centerIn: parent
            width: 12
            height: 12
            visible: root.checked
            source: iconBaseUrl + "check.png"
            sourceSize.width: 24
            sourceSize.height: 24
            fillMode: Image.PreserveAspectFit
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.flip()
        }
    }

    Label {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignVCenter
        text: root.label
        color: Theme.textPrimary
        font.pixelSize: Theme.fontSizeMd

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.flip()
        }
    }

    Accessible.role: Accessible.CheckBox
    Accessible.name: root.label
    activeFocusOnTab: true
    Keys.onSpacePressed: root.flip()
}
