import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Docker engine (colima / Desktop / OrbStack), containers and images.
// Logs open inline inside the container card and follow while open.
Item {
    id: dockerPage
    objectName: "dockerPage"

    readonly property var svc: (typeof dockerService !== "undefined") ? dockerService : null
    readonly property var engine: svc ? svc.engine : ({})
    readonly property var containers: svc ? svc.containers : []
    readonly property var images: svc ? svc.images : []
    readonly property var disk: svc ? svc.disk : ({})
    readonly property var liveStats: svc ? svc.stats : ({})
    readonly property bool busy: svc ? svc.busy : false

    // Set by Dashboard: stats poll only while the page is visible.
    property bool pageActive: false
    property string openLogsFor: ""
    property string logsText: ""
    // Id (image id or container name) whose removal is in flight.
    property string pendingRemove: ""

    function askRemove(kind, id, title) {
        confirmDialog.targetKind = kind
        confirmDialog.targetId = id
        confirmDialog.targetTitle = title
        confirmDialog.open()
    }

    function confirmRemove() {
        var kind = confirmDialog.targetKind
        var id = confirmDialog.targetId
        confirmDialog.close()
        if (!dockerPage.svc || id === "")
            return
        dockerPage.pendingRemove = id
        if (kind === "image")
            dockerPage.svc.removeImage(id)
        else
            dockerPage.svc.removeContainer(id)
    }

    function reload() {
        if (dockerPage.svc)
            dockerPage.svc.refresh()
    }

    function toggleLogs(name) {
        if (dockerPage.openLogsFor === name) {
            dockerPage.openLogsFor = ""
            dockerPage.logsText = ""
            return
        }
        dockerPage.openLogsFor = name
        dockerPage.logsText = qsTr("Loading…")
        if (dockerPage.svc)
            dockerPage.svc.fetchLogs(name, 300)
    }

    function engineTitle() {
        var e = dockerPage.engine
        if (!e || !e.available)
            return qsTr("Docker daemon not reachable")
        var parts = []
        if (e.runtime && e.runtime !== "")
            parts.push(e.runtime)
        if (e.version && e.version !== "")
            parts.push("v" + e.version)
        if (e.context && e.context !== "" && e.context !== e.runtime)
            parts.push("(" + e.context + ")")
        return parts.length > 0 ? parts.join(" ") : qsTr("Docker")
    }

    function containerMeta(ctr) {
        var parts = []
        if (ctr.image && ctr.image !== "")
            parts.push(ctr.image)
        if (ctr.status && ctr.status !== "")
            parts.push(ctr.status)
        return parts.join(" · ")
    }

    function imageTitle(img) {
        if (img.tag && img.tag !== "" && img.tag !== "<none>")
            return img.repository + ":" + img.tag
        return img.repository
    }

    function imageMeta(img) {
        var parts = []
        if (img.size && img.size !== "")
            parts.push(img.size)
        if (img.id && img.id !== "")
            parts.push(img.id.substring(0, 12))
        return parts.join(" · ")
    }

    function shortReclaim(s) {
        if (!s || s === "")
            return ""
        return s.split(" ")[0]
    }

    function diskSummary() {
        var d = dockerPage.disk
        if (!d || Object.keys(d).length === 0)
            return ""
        var parts = []
        function one(key, label) {
            var t = d[key]
            if (!t)
                return
            var s = label + " " + t.size
            var r = shortReclaim(t.reclaimable)
            if (r !== "" && r !== "0B")
                s += " (" + qsTr("reclaimable ") + r + ")"
            parts.push(s)
        }
        one("Images", qsTr("Images"))
        one("Containers", qsTr("Containers"))
        one("Local Volumes", qsTr("Volumes"))
        one("Build Cache", qsTr("Cache"))
        return parts.join(" · ")
    }

    // Compose projects (from the container label), sorted, unique.
    function projectNames() {
        var seen = {}
        var out = []
        for (var i = 0; i < dockerPage.containers.length; ++i) {
            var p = dockerPage.containers[i].project || ""
            if (p !== "" && !seen[p]) {
                seen[p] = true
                out.push(p)
            }
        }
        out.sort()
        return out
    }

    function containersOf(project) {
        var out = []
        for (var i = 0; i < dockerPage.containers.length; ++i) {
            if ((dockerPage.containers[i].project || "") === project)
                out.push(dockerPage.containers[i])
        }
        return out
    }

    function standaloneContainers() {
        return dockerPage.containersOf("")
    }

    function runningCount(list) {
        var n = 0
        for (var i = 0; i < list.length; ++i) {
            if (list[i].running)
                ++n
        }
        return n
    }

    function groupNames(list) {
        var out = []
        for (var i = 0; i < list.length; ++i)
            out.push(list[i].name)
        return out
    }

    function groupControl(list, action) {
        if (dockerPage.svc && !dockerPage.busy)
            dockerPage.svc.controlGroup(dockerPage.groupNames(list), action)
    }

    // First published host port ("0.0.0.0:9000->9000/tcp" -> 9000), else 0.
    function publishedPort(ports) {
        if (!ports || ports === "")
            return 0
        var m = /:(\d+)->/.exec(ports)
        return m ? parseInt(m[1], 10) : 0
    }

    function statsText(name) {
        var s = dockerPage.liveStats[name]
        if (!s)
            return ""
        return "CPU " + s.cpu + " · MEM " + s.mem
    }

    Connections {
        target: dockerPage.svc
        function onLogsReady(name, logs) {
            if (dockerPage.openLogsFor === name)
                dockerPage.logsText = logs === "" ? qsTr("(no output)") : logs
        }
        function onErrorMessage(message) {
            dockerPage.pendingRemove = ""
            logModel.add("error", "docker", message)
        }
        function onContainersChanged() {
            dockerPage.pendingRemove = ""
        }
        function onImagesChanged() {
            dockerPage.pendingRemove = ""
        }
    }

    // Follow open logs while the card stays expanded.
    Timer {
        interval: 3000
        repeat: true
        running: dockerPage.openLogsFor !== ""
        onTriggered: {
            if (dockerPage.svc && dockerPage.openLogsFor !== "")
                dockerPage.svc.fetchLogs(dockerPage.openLogsFor, 300)
        }
    }

    // Live CPU/RAM for running containers, only while the page is open.
    Timer {
        interval: 5000
        repeat: true
        running: dockerPage.pageActive && dockerPage.engine && dockerPage.engine.available === true
        triggeredOnStart: true
        onTriggered: {
            if (dockerPage.svc)
                dockerPage.svc.fetchStats()
        }
    }

    Dialog {
        id: confirmDialog
        objectName: "confirmDialog"
        anchors.centerIn: parent
        width: 400
        modal: true
        padding: 0

        property string targetKind: ""
        property string targetId: ""
        property string targetTitle: ""

        background: Rectangle {
            color: Theme.cardBackground
            border.color: Theme.border
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 12

            Label {
                text: qsTr("Remove %1?").arg(confirmDialog.targetTitle)
                color: Theme.textPrimary
                font.bold: true
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.topMargin: 20
            }

            Label {
                text: confirmDialog.targetKind === "image"
                    ? qsTr("Docker will delete this image. Running containers keep working, but the image must be pulled again to recreate them.")
                    : qsTr("Docker will force-remove this container. Its volumes are kept.")
                color: Theme.textMuted
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
            }

            RowLayout {
                spacing: 8
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.bottomMargin: 20

                Item { Layout.fillWidth: true }

                Button {
                    Layout.preferredWidth: 110
                    Layout.preferredHeight: 30
                    font.pixelSize: 12
                    onClicked: confirmDialog.close()

                    background: Rectangle {
                        radius: 6
                        color: "transparent"
                        border.color: Theme.border
                        border.width: 1
                    }

                    contentItem: Label {
                        text: qsTr("Cancel")
                        color: Theme.textPrimary
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    Layout.preferredWidth: 110
                    Layout.preferredHeight: 30
                    font.pixelSize: 12
                    onClicked: dockerPage.confirmRemove()

                    background: Rectangle {
                        radius: 6
                        color: "#dc2626"
                    }

                    contentItem: Label {
                        text: qsTr("Remove")
                        color: "white"
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingLg
        anchors.rightMargin: Theme.spacingLg
        anchors.topMargin: Theme.spacingMd
        anchors.bottomMargin: Theme.spacingLg
        clip: true

        Column {
            // Viewport-driven width: children of a ScrollView live on the
            // Flickable contentItem, whose width follows the content.
            // width: parent.width loops back to the implicit minimum and
            // collapses every card; availableWidth breaks the loop.
            width: scroll.availableWidth
            spacing: Theme.spacingMd

            // Section header shared by all three blocks.
            // (Single instance: each block inlines its own copy below.)
            RowLayout {
                width: parent.width
                spacing: Theme.spacingSm

                Label {
                    text: qsTr("ENGINE")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSm
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Item {
                    Layout.preferredWidth: Theme.sidebarRowHeight
                    Layout.preferredHeight: Theme.sidebarRowHeight

                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusSm
                        color: rescanMouse.containsMouse
                            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                            : "transparent"
                    }

                    Image {
                        id: rescanIcon
                        anchors.centerIn: parent
                        width: Theme.iconSm
                        height: Theme.iconSm
                        source: iconBaseUrl + (Theme.isDark ? "rotate-cw-dark.png" : "rotate-cw.png")
                        sourceSize.width: 32
                        sourceSize.height: 32
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        rotation: 0
                    }

                    MouseArea {
                        id: rescanMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: dockerPage.reload()
                    }

                    NumberAnimation {
                        id: spin
                        target: rescanIcon
                        property: "rotation"
                        from: 0
                        to: 360
                        duration: 900
                        easing.type: Easing.Linear
                        onFinished: {
                            if (dockerPage.busy) {
                                rescanIcon.rotation = 0
                                spin.start()
                            } else {
                                rescanIcon.rotation = 0
                            }
                        }
                    }

                    Connections {
                        target: dockerPage.svc
                        function onBusyChanged() {
                            if (dockerPage.busy && !spin.running) {
                                rescanIcon.rotation = 0
                                spin.start()
                            }
                        }
                    }
                }
            }

            // Engine status card.
            Rectangle {
                id: engineCardRect
                objectName: "engineCard"
                width: parent.width
                height: engineInner.implicitHeight + 2 * Theme.spacingMd
                radius: Theme.radiusMd
                color: Theme.cardBackground
                border.width: 1
                border.color: Theme.border

                ColumnLayout {
                    id: engineInner
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.spacingMd
                    spacing: Theme.spacingSm

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMd

                        Image {
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            Layout.alignment: Qt.AlignVCenter
                            source: iconBaseUrl + (Theme.isDark ? "docker-dark.png" : "docker.png")
                            sourceSize.width: 64
                            sourceSize.height: 64
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: dockerPage.engineTitle()
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeMd
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: {
                                    var e = dockerPage.engine
                                    if (!e || !e.available)
                                        return qsTr("Start colima or the Docker daemon, then Rescan")
                                    if (e.colimaInstalled)
                                        return e.colimaRunning ? qsTr("colima running") : qsTr("colima stopped")
                                    return qsTr("daemon reachable")
                                }
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSizeSm
                                elide: Text.ElideRight
                            }
                        }

                        Rectangle {
                            Layout.alignment: Qt.AlignVCenter
                            width: 8
                            height: 8
                            radius: 4
                            color: (dockerPage.engine && dockerPage.engine.available) ? "#22c55e" : Theme.destructive
                        }

                        Button {
                            Layout.alignment: Qt.AlignVCenter
                            implicitWidth: 88
                            implicitHeight: 30
                            visible: dockerPage.engine && dockerPage.engine.colimaInstalled === true
                            enabled: dockerPage.svc && !dockerPage.busy
                            onClicked: {
                                if (!dockerPage.svc)
                                    return
                                if (dockerPage.engine.colimaRunning)
                                    dockerPage.svc.stopEngine()
                                else
                                    dockerPage.svc.startEngine()
                            }

                            background: Rectangle {
                                radius: Theme.radiusSm
                                color: "transparent"
                                border.color: Theme.border
                                border.width: 1
                            }

                            contentItem: Label {
                                text: (dockerPage.engine && dockerPage.engine.colimaRunning) ? qsTr("Stop") : qsTr("Start")
                                color: Theme.secondaryForeground
                                font.pixelSize: Theme.fontSizeMd
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        Button {
                            Layout.alignment: Qt.AlignVCenter
                            implicitWidth: 88
                            implicitHeight: 30
                            visible: dockerPage.engine && dockerPage.engine.available === true
                            enabled: dockerPage.svc && !dockerPage.busy
                            onClicked: {
                                if (dockerPage.svc)
                                    dockerPage.svc.prune()
                            }

                            background: Rectangle {
                                radius: Theme.radiusSm
                                color: "transparent"
                                border.color: Theme.border
                                border.width: 1
                            }

                            contentItem: Label {
                                text: qsTr("Prune")
                                color: Theme.secondaryForeground
                                font.pixelSize: Theme.fontSizeMd
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }

                    Label {
                        visible: dockerPage.diskSummary() !== ""
                        Layout.fillWidth: true
                        text: dockerPage.diskSummary()
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSm
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Label {
                text: qsTr("CONTAINERS")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeSm
                font.bold: true
            }

            Label {
                visible: dockerPage.containers.length === 0
                text: (dockerPage.svc && dockerPage.svc.busy)
                    ? qsTr("Scanning…") : qsTr("No containers")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeMd
            }

            // Shared container card: one compose group or standalone list
            // instantiates it per container via the repeaters below.
            Component {
                id: containerDelegate

                Rectangle {
                    id: containerCard
                    required property var modelData
                    width: parent.width
                    height: inner.implicitHeight + 2 * Theme.spacingMd
                    radius: Theme.radiusMd
                    color: Theme.cardBackground
                    border.width: 1
                    border.color: Theme.border

                    ColumnLayout {
                        id: inner
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Theme.spacingMd
                        spacing: Theme.spacingSm

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingMd

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    Layout.fillWidth: true
                                    text: containerCard.modelData.name
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeMd
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: dockerPage.containerMeta(containerCard.modelData)
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeSm
                                    elide: Text.ElideRight
                                }
                                Label {
                                    visible: dockerPage.statsText(containerCard.modelData.name) !== ""
                                    Layout.fillWidth: true
                                    text: dockerPage.statsText(containerCard.modelData.name)
                                    color: "#22c55e"
                                    font.pixelSize: Theme.fontSizeSm
                                    elide: Text.ElideRight
                                }
                                RowLayout {
                                    visible: containerCard.modelData.ports !== ""
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSm

                                    Label {
                                        Layout.fillWidth: true
                                        text: containerCard.modelData.ports
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontSizeSm
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        visible: dockerPage.publishedPort(containerCard.modelData.ports) > 0
                                        Layout.alignment: Qt.AlignVCenter
                                        text: qsTr("Open")
                                        color: Theme.accent
                                        font.pixelSize: Theme.fontSizeSm

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: Qt.openUrlExternally("http://localhost:"
                                                + dockerPage.publishedPort(containerCard.modelData.ports))
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                Layout.alignment: Qt.AlignVCenter
                                width: 8
                                height: 8
                                radius: 4
                                color: containerCard.modelData.running ? "#22c55e" : Theme.destructive
                            }

                            Button {
                                Layout.alignment: Qt.AlignVCenter
                                implicitWidth: 76
                                implicitHeight: 30
                                enabled: dockerPage.svc && !dockerPage.busy
                                onClicked: {
                                    if (!dockerPage.svc)
                                        return
                                    if (containerCard.modelData.running)
                                        dockerPage.svc.stopContainer(containerCard.modelData.name)
                                    else
                                        dockerPage.svc.startContainer(containerCard.modelData.name)
                                }

                                background: Rectangle {
                                    radius: Theme.radiusSm
                                    color: "transparent"
                                    border.color: Theme.border
                                    border.width: 1
                                }

                                contentItem: Label {
                                    text: containerCard.modelData.running ? qsTr("Stop") : qsTr("Start")
                                    color: Theme.secondaryForeground
                                    font.pixelSize: Theme.fontSizeMd
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            Button {
                                Layout.alignment: Qt.AlignVCenter
                                implicitWidth: 76
                                implicitHeight: 30
                                enabled: dockerPage.svc && !dockerPage.busy
                                onClicked: dockerPage.toggleLogs(containerCard.modelData.name)

                                background: Rectangle {
                                    radius: Theme.radiusSm
                                    color: dockerPage.openLogsFor === containerCard.modelData.name
                                        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.25)
                                        : "transparent"
                                    border.color: Theme.border
                                    border.width: 1
                                }

                                contentItem: Label {
                                    text: qsTr("Logs")
                                    color: Theme.secondaryForeground
                                    font.pixelSize: Theme.fontSizeMd
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm
                            visible: containerCard.modelData.running

                            Item { Layout.fillWidth: true }

                            Button {
                                Layout.alignment: Qt.AlignVCenter
                                implicitWidth: 88
                                implicitHeight: 30
                                enabled: dockerPage.svc && !dockerPage.busy
                                    && dockerPage.pendingRemove === ""
                                onClicked: dockerPage.svc.restartContainer(containerCard.modelData.name)

                                background: Rectangle {
                                    radius: Theme.radiusSm
                                    color: "transparent"
                                    border.color: Theme.border
                                    border.width: 1
                                }

                                contentItem: Label {
                                    text: qsTr("Restart")
                                    color: Theme.secondaryForeground
                                    font.pixelSize: Theme.fontSizeMd
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            Button {
                                Layout.alignment: Qt.AlignVCenter
                                implicitWidth: 88
                                implicitHeight: 30
                                enabled: dockerPage.svc && !dockerPage.busy
                                    && dockerPage.pendingRemove === ""
                                onClicked: dockerPage.askRemove("container",
                                    containerCard.modelData.name, containerCard.modelData.name)

                                background: Rectangle {
                                    radius: Theme.radiusSm
                                    color: "transparent"
                                    border.color: Theme.border
                                    border.width: 1
                                }

                                contentItem: Row {
                                    anchors.centerIn: parent
                                    spacing: 6

                                    BusyIndicator {
                                        visible: dockerPage.pendingRemove === containerCard.modelData.name
                                        running: visible
                                        width: 14
                                        height: 14
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    Label {
                                        text: dockerPage.pendingRemove === containerCard.modelData.name
                                            ? qsTr("Removing") : qsTr("Remove")
                                        color: Theme.destructive
                                        font.pixelSize: Theme.fontSizeMd
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }
                        }

                        // Inline log viewer, follows while expanded.
                        ScrollView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 220
                            visible: dockerPage.openLogsFor === containerCard.modelData.name
                            clip: true

                            TextArea {
                                text: dockerPage.logsText
                                readOnly: true
                                wrapMode: Text.WrapAnywhere
                                font.family: "Menlo"
                                font.pixelSize: 11
                                color: Theme.textPrimary
                                background: Rectangle {
                                    color: Theme.windowBackground
                                    radius: Theme.radiusSm
                                }
                            }
                        }
                    }
                }
            }

            // Compose project groups (one block per project label).
            Repeater {
                model: dockerPage.projectNames()

                delegate: Column {
                    id: groupBlock
                    required property var modelData
                    width: parent.width
                    spacing: Theme.spacingSm

                    RowLayout {
                        width: parent.width
                        spacing: Theme.spacingSm

                        Label {
                            Layout.fillWidth: true
                            text: groupBlock.modelData
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeMd
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.alignment: Qt.AlignVCenter
                            text: dockerPage.runningCount(dockerPage.containersOf(groupBlock.modelData))
                                + "/" + dockerPage.containersOf(groupBlock.modelData).length
                                + qsTr(" running")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSm
                        }

                        Label {
                            Layout.alignment: Qt.AlignVCenter
                            text: qsTr("Start all")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSm

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                enabled: dockerPage.svc && !dockerPage.busy
                                onClicked: dockerPage.groupControl(
                                    dockerPage.containersOf(groupBlock.modelData), "start")
                            }
                        }

                        Label {
                            Layout.alignment: Qt.AlignVCenter
                            text: "·"
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSm
                        }

                        Label {
                            Layout.alignment: Qt.AlignVCenter
                            text: qsTr("Stop")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSm

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                enabled: dockerPage.svc && !dockerPage.busy
                                onClicked: dockerPage.groupControl(
                                    dockerPage.containersOf(groupBlock.modelData), "stop")
                            }
                        }

                        Label {
                            Layout.alignment: Qt.AlignVCenter
                            text: "·"
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSm
                        }

                        Label {
                            Layout.alignment: Qt.AlignVCenter
                            text: qsTr("Restart")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSm

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                enabled: dockerPage.svc && !dockerPage.busy
                                onClicked: dockerPage.groupControl(
                                    dockerPage.containersOf(groupBlock.modelData), "restart")
                            }
                        }
                    }

                    Repeater {
                        model: dockerPage.containersOf(groupBlock.modelData)
                        delegate: containerDelegate
                    }
                }
            }

            Label {
                visible: dockerPage.projectNames().length > 0
                    && dockerPage.standaloneContainers().length > 0
                text: qsTr("STANDALONE")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeSm
                font.bold: true
            }

            Repeater {
                model: dockerPage.standaloneContainers()
                delegate: containerDelegate
            }

            Label {
                text: qsTr("IMAGES")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeSm
                font.bold: true
            }

            Label {
                visible: dockerPage.images.length === 0
                text: (dockerPage.svc && dockerPage.svc.busy)
                    ? qsTr("Scanning…") : qsTr("No images")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeMd
            }

            Repeater {
                model: dockerPage.images

                delegate: Rectangle {
                    id: imageCard
                    required property var modelData
                    width: parent.width
                    height: 64
                    radius: Theme.radiusMd
                    color: Theme.cardBackground
                    border.width: 1
                    border.color: Theme.border

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingMd
                        spacing: Theme.spacingMd

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: dockerPage.imageTitle(imageCard.modelData)
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeMd
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: dockerPage.imageMeta(imageCard.modelData)
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSizeSm
                                elide: Text.ElideRight
                            }
                        }

                        Button {
                            Layout.alignment: Qt.AlignVCenter
                            implicitWidth: 88
                            implicitHeight: 30
                            enabled: dockerPage.svc && !dockerPage.busy
                                && dockerPage.pendingRemove === ""
                            onClicked: dockerPage.askRemove("image", imageCard.modelData.id,
                                dockerPage.imageTitle(imageCard.modelData))

                            background: Rectangle {
                                radius: Theme.radiusSm
                                color: "transparent"
                                border.color: Theme.border
                                border.width: 1
                            }

                            contentItem: Row {
                                anchors.centerIn: parent
                                spacing: 6

                                BusyIndicator {
                                    visible: dockerPage.pendingRemove === imageCard.modelData.id
                                    running: visible
                                    width: 14
                                    height: 14
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Label {
                                    text: dockerPage.pendingRemove === imageCard.modelData.id
                                        ? qsTr("Removing") : qsTr("Remove")
                                    color: Theme.destructive
                                    font.pixelSize: Theme.fontSizeMd
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }
                    }
                }
            }

            Item {
                width: parent.width
                height: Theme.spacingLg
            }
        }
    }

    Component.onCompleted: {
        if (dockerPage.svc && dockerPage.containers.length === 0 && !dockerPage.busy)
            dockerPage.svc.refresh()
    }
}
