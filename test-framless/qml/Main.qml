import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QWindowKit 1.0

// Empty frameless window.
//
// On macOS the native traffic lights and drag/resize behaviour are provided by
// QWindowKit's WindowAgent; the sidebar is a translucent surface over the
// native blur, so the desktop behind the window shows through.
Window {
    id: root

    width: 1100
    height: 720
    minimumWidth: 720
    minimumHeight: 480

    // No Qt.FramelessWindowHint on purpose: QWindowKit drives the native
    // frame and keeps the standard traffic lights usable on macOS.
    title: qsTr("Frameless")
    color: "transparent"
    visible: true

    readonly property bool isMac: Qt.platform.os === "osx"
    readonly property bool zoomed: visibility === Window.Maximized
                                   || visibility === Window.FullScreen
    readonly property int windowRadius: zoomed ? 0 : 12
    readonly property int sidebarWidth: 248
    readonly property int titleBarHeight: 52

    WindowAgent {
        id: windowAgent
    }

    // Opaque content surface: everything to the right of the sidebar.
    Rectangle {
        id: contentSurface

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: root.sidebarWidth
        anchors.right: parent.right

        topRightRadius: root.windowRadius
        bottomRightRadius: root.windowRadius
        color: "#ffffff"
    }

    Sidebar {
        id: sidebar

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: root.sidebarWidth
        radius: root.windowRadius
    }

    // Transparent drag strip spanning the title bar area. Declared before the
    // buttons so it sits underneath them; empty areas drag, buttons click.
    Item {
        id: titleBar

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.titleBarHeight
        z: 10
    }

    // Custom lights on non-macOS; macOS uses the native ones QWindowKit keeps.
    TrafficLights {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 18
        anchors.leftMargin: 18
        visible: !root.isMac
        z: 11

        windowActive: root.active

        onCloseRequested: root.close()
        onMinimizeRequested: root.showMinimized()
        onMaximizeRequested: root.toggleZoom()
    }

    function toggleZoom() {
        if (root.visibility === Window.Maximized)
            root.showNormal()
        else
            root.showMaximized()
    }

    Component.onCompleted: {
        windowAgent.setup(root)
        windowAgent.setTitleBar(titleBar)
        // Attributes touch the native window, so apply them once it exists.
        Qt.callLater(applyPlatformChrome)
    }

    function applyPlatformChrome() {
        if (!root.isMac)
            return
        // Liquid Glass (NSGlassEffectView) only exists on macOS 26+.
        // Fall back to the classic NSVisualEffectView blur elsewhere.
        var glass = windowAgent.setWindowAttribute("glass-effect", "regular")
        console.log("glass-effect =>", glass)
        if (glass) {
            windowAgent.setWindowAttribute("glass-corner-radius", root.windowRadius)
        } else {
            var blur = windowAgent.setWindowAttribute("blur-effect", "light")
            console.log("blur-effect =>", blur)
        }
    }
}
