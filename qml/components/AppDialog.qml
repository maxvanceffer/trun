import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../"

Popup {
    id: root

    // ---- API ----
    property string title: ""
    property string subtitle: ""
    property bool showClose: true
    property bool showFooter: true
    property int dialogWidth: 480
    property int dialogHeight: 0             // 0 = высота по содержимому
    property int contentPadding: 20          // 0 для сложной вёрстки (например, список + форма)
    property bool closeOnOutsideClick: true  // false, если в диалоге есть несохранённые правки
    property bool acceptOnEnter: false       // Enter = accept()

    default property alias content: contentHost.data
    property alias footerLeft: footerLeftHost.data
    property alias footerRight: footerRightHost.data

    signal accepted()
    signal rejected()

    function accept() { _accepted = true; root.accepted(); root.close() }
    function reject() { root.close() }

    property bool _accepted: false
    onClosed: { if (!_accepted) root.rejected(); _accepted = false }

    // ---- позиция и размер ----
    // Hosting: the overlay only exists under an ApplicationWindow. The app's
    // main.qml is a plain Window — binding parent to Overlay.overlay there
    // resolves to null and open() silently no-ops, so the Save/Environments/
    // Import dialogs could never appear. Without an explicit parent QQC2
    // hosts the popup on its declaration item (a page in the app window),
    // which always has a window; center/closePolicy still work. Modal dim is
    // overlay-only, so it is a plain centered popup in the app.
    anchors.centerIn: parent
    width: Math.min(dialogWidth, parent ? parent.width - 48 : dialogWidth)
    height: dialogHeight > 0
            ? Math.min(dialogHeight, parent ? parent.height - 48 : dialogHeight)
            : Math.min(implicitHeight, parent ? parent.height - 48 : implicitHeight)

    modal: true
    focus: true
    padding: 0
    closePolicy: closeOnOutsideClick ? (Popup.CloseOnEscape | Popup.CloseOnPressOutside)
                                     : Popup.CloseOnEscape

    // modal.ts: overlay: 'bg-elevated/75'
    Overlay.modal: Rectangle {
        color: Qt.rgba(Theme.backgroundElevated.r, Theme.backgroundElevated.g,
                       Theme.backgroundElevated.b, 0.75)
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.98; to: 1; duration: 160; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120; easing.type: Easing.OutCubic }
    }

    background: Rectangle {
        color: Theme.backgroundDefault // modal content: bg-default
        radius: Theme.radiusLg
        border.width: 1
        border.color: Theme.border
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Theme.shadowColor
            shadowOpacity: 0.6
            shadowBlur: 1.0
            shadowVerticalOffset: 24
            blurMax: 48
        }
    }

    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: root.opened && root.acceptOnEnter
        onActivated: root.accept()
    }

    contentItem: ColumnLayout {
        spacing: 0

        // ---- header ----
        Item {
            Layout.fillWidth: true
            visible: root.title !== ""
            implicitHeight: headerRow.implicitHeight + 28

            RowLayout {
                id: headerRow
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 16
                anchors.topMargin: 14
                anchors.bottomMargin: 14
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Label {
                        Layout.fillWidth: true
                        text: root.title
                        font.pixelSize: 15
                        font.weight: Font.Bold
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: root.subtitle !== ""
                        text: root.subtitle
                        textFormat: Text.StyledText
                        font.pixelSize: 12
                        color: Theme.textMuted
                        wrapMode: Text.WordWrap
                    }
                }

                AbstractButton {
                    id: closeBtn
                    visible: root.showClose
                    Layout.alignment: Qt.AlignTop
                    implicitWidth: 28
                    implicitHeight: 28
                    hoverEnabled: true
                    onClicked: root.reject()
                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                    }
                    contentItem: Text {
                        text: "\u2715"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: closeBtn.hovered ? Theme.textPrimary : Theme.textMuted
                    }
                    background: Rectangle {
                        radius: Theme.radiusSm
                        color: closeBtn.hovered ? Theme.mutedSurface : "transparent"
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.border
            visible: root.title !== ""
        }

        // ---- content ----
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: root.dialogHeight > 0
            implicitHeight: contentHost.childrenRect.height + root.contentPadding * 2

            Item {
                id: contentHost
                anchors.fill: parent
                anchors.margins: root.contentPadding
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.border
            visible: root.showFooter
        }

        // ---- footer ----
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 16
            Layout.topMargin: 12
            Layout.bottomMargin: 12
            visible: root.showFooter
            spacing: 8

            Row { id: footerLeftHost; spacing: 8; Layout.alignment: Qt.AlignVCenter }
            Item { Layout.fillWidth: true }
            Row { id: footerRightHost; spacing: 8; Layout.alignment: Qt.AlignVCenter }
        }
    }
}
