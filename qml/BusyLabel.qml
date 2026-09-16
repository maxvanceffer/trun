import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Busy loader with a caption, shown in place of an action button while
// the action is in flight. layout is "horizontal" or "vertical".
Item {
    id: root

    property string layout: "horizontal"
    property string text: qsTr("Removing")
    property color textColor: Theme.destructive
    property bool running: true

    readonly property bool vertical: root.layout === "vertical"

    implicitWidth: root.vertical ? verticalBox.implicitWidth : horizontalBox.implicitWidth
    implicitHeight: root.vertical ? verticalBox.implicitHeight : horizontalBox.implicitHeight

    RowLayout {
        id: horizontalBox
        anchors.centerIn: parent
        visible: !root.vertical
        spacing: 6

        Spinner {
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            Layout.alignment: Qt.AlignVCenter
            running: root.running && horizontalBox.visible
            color: root.textColor
        }

        Label {
            Layout.alignment: Qt.AlignVCenter
            text: root.text
            color: root.textColor
            font.pixelSize: Theme.fontSizeMd
        }
    }

    ColumnLayout {
        id: verticalBox
        anchors.centerIn: parent
        visible: root.vertical
        spacing: 4

        Spinner {
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            Layout.alignment: Qt.AlignHCenter
            running: root.running && verticalBox.visible
            color: root.textColor
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: root.text
            color: root.textColor
            font.pixelSize: Theme.fontSizeSm
        }
    }
}
