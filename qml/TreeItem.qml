import QtQuick 6.5
import QtQuick.Controls 6.5

Frame {
    id: delegateItem
    property string name: model.name || ""
    property string manifest: model.manifest || ""
    property var commands: model.commands || []
    
    Component.onCompleted: {
        console.log("[TreeItem] Delegate created: name=" + name + " manifest=" + manifest + " commands=" + commands)
    }

    signal clicked()

    width: 200
    height: 28

    background: Rectangle {
        color: mouseArea.pressed ? "#1a1a1a" : "transparent"
        radius: 4
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        onClicked: delegateItem.clicked()
    }

    Row {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12
        padding: 4

        Rectangle {
            width: 16
            height: 16
            radius: 3
            color: delegateItem.manifest === "package.json" ? "#f59e0b" :
                   delegateItem.manifest === "Cargo.toml" ? "#dedede" :
                   delegateItem.manifest === "go.mod" ? "#00add8" :
                   delegateItem.manifest === "Gemfile" ? "#701a16" :
                   delegateItem.manifest === "pyproject.toml" ? "#3b6cac" :
                   delegateItem.manifest === "CMakeLists.txt" ? "#4e84b4" :
                   "#888"
        }

        Label {
            text: delegateItem.name
            color: "#ddd"
            font.pixelSize: 12
            elide: Text.ElideRight
            width: delegateItem.width - 48
        }

        Label {
            text: String(delegateItem.commands ? delegateItem.commands.length : 0)
            color: "#444"
            font.pixelSize: 9
        }
    }
}
