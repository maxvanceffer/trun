import QtQuick

// Frosted sidebar.
//
// The native blur sits behind the whole window, so keeping this surface
// translucent lets the desktop show through as frosted glass. The solid
// content surface on the right is what makes the sidebar look clipped.
Rectangle {
    id: sidebar

    property int radius: 12

    topLeftRadius: radius
    bottomLeftRadius: radius

    // Translucent on macOS (reveals the blur), solid elsewhere.
    color: Qt.platform.os === "osx" ? Qt.rgba(1, 1, 1, 0.35) : "#ececf0"

    // Hairline between sidebar and content.
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Qt.rgba(0, 0, 0, 0.08)
    }
}
