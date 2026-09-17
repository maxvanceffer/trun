import QtQuick
import QtQuick.Controls

// UiInput — текстовое поле в визуале Nuxt UI Input (size md, variant
// outline): bg-default, inset-ring, rounded-md, плейсхолдер dimmed;
// в фокусе кольцо красится в accent. Отступы px-2.5/py-1.5 —
// специфичны для компонента (в шаге Theme их нет).
TextField {
    id: root

    property string leadingIconSource: ""
    property string trailingIconSource: ""

    signal leadingClicked()
    signal trailingClicked()

    readonly property bool hasLeading: leadingIconSource !== ""
    readonly property bool hasTrailing: trailingIconSource !== ""

    implicitHeight: Theme.controlHeight
    font.pixelSize: Theme.fontSizeLg
    color: Theme.textPrimary
    placeholderTextColor: Theme.textMuted
    selectByMouse: true
    opacity: enabled ? 1 : 0.75

    leftPadding: root.hasLeading ? 28 : 10
    rightPadding: root.hasTrailing ? 28 : 10
    topPadding: 6
    bottomPadding: 6

    background: Item {
        // Focus ring: еле заметное внешнее кольцо (accent/25, как
        // outline-{color}/25 у Nuxt), видно только в фокусе.
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: Theme.radiusSm + 3
            color: "transparent"
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g,
                                  Theme.accent.b, 0.25)
            border.width: 2
            visible: root.activeFocus
        }

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusSm
            color: Theme.cardBackground
            border.color: root.activeFocus ? Theme.accent : Theme.border
            border.width: 1
        }
    }

    Image {
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.iconSm
        height: Theme.iconSm
        visible: root.hasLeading
        source: root.leadingIconSource
        sourceSize.width: Theme.iconSm
        sourceSize.height: Theme.iconSm
        fillMode: Image.PreserveAspectFit

        MouseArea {
            anchors.fill: parent
            anchors.margins: -4
            cursorShape: Qt.PointingHandCursor
            onClicked: root.leadingClicked()
        }
    }

    Image {
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.iconSm
        height: Theme.iconSm
        visible: root.hasTrailing
        source: root.trailingIconSource
        sourceSize.width: Theme.iconSm
        sourceSize.height: Theme.iconSm
        fillMode: Image.PreserveAspectFit

        MouseArea {
            anchors.fill: parent
            anchors.margins: -4
            cursorShape: Qt.PointingHandCursor
            onClicked: root.trailingClicked()
        }
    }
}
