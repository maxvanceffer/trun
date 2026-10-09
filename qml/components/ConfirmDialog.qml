import QtQuick
import QtQuick.Controls
import "../"
import "../Ui"

AppDialog {
    id: root
    property string message: ""
    property string confirmText: "Confirm"
    property bool destructive: false

    dialogWidth: 420
    acceptOnEnter: true

    Label {
        width: parent.width
        text: root.message
        wrapMode: Text.WordWrap
        font.pixelSize: 13
        lineHeight: 1.45
        color: Theme.textMuted
    }

    footerRight: [
        UiButton {
            label: "Cancel"
            color: "secondary"
            variant: "outline"
            size: "sm"
            onClicked: root.reject()
        },
        UiButton {
            label: root.confirmText
            color: root.destructive ? "error" : "primary"
            variant: "solid"
            size: "sm"
            onClicked: root.accept()
        }
    ]
}
