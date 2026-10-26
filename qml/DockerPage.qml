import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Ui"
pragma ComponentBehavior: Bound

// Docker engine (colima / Desktop / OrbStack), containers and images.
// Logs open inline inside the container card and follow while open.
Item {
    id: dockerPage
    objectName: "dockerPage"

    readonly property var dbs: dockerService // qmllint disable unqualified
    readonly property var lm: logModel // qmllint disable unqualified

    readonly property var svc: (typeof dbs !== "undefined") ? dbs : null
    readonly property var engine: svc ? svc.engine : ({})
    readonly property var containers: svc ? svc.containers : []
    readonly property var images: svc ? svc.images : []
    readonly property var disk: svc ? svc.disk : ({})
    readonly property var liveStats: svc ? svc.stats : ({})
    // Any operation in flight: refresh() or a single action. Gates all the
    // controls so a second action cannot be started on top of the first.
    readonly property bool busy: svc ? svc.busy : false
    // Full rescan only — the Rescan spinner follows this and nothing else.
    // A single action (start/stop/remove/prune) must not spin it.
    readonly property bool scanning: svc ? svc.scanning : false
    // What is in flight — "<action>:<id>" ("start:web", "rmi:def456",
    // "restart:group:shop"), or "engine" / "prune". Exactly one control shows
    // a per-action loader; the header's Rescan icon never reacts to it.
    readonly property string controllingId: svc ? svc.controllingId : ""
    // Container actions need a reachable daemon.
    readonly property bool engineUp: !!(engine && engine.available)

    // Set by Dashboard: stats poll only while the page is visible.
    property bool pageActive: false
    property string openLogsFor: ""
    property string logsText: ""

    // True when this very action on this id is the one in flight, so only its
    // own control shows the loader (two buttons on one row both keyed by a
    // container name would spin together otherwise).
    function inFlight(action, id) {
        return dockerPage.controllingId === action + ":" + id
    }

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

    function groupControl(list, action, project) {
        if (dockerPage.svc && !dockerPage.busy)
            dockerPage.svc.controlGroup(dockerPage.groupNames(list), action, project)
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
        function onLogsError(message) {
            dockerPage.lm.add("error", "docker", message)
        }
        function onErrorMessage(message) {
            dockerPage.lm.add("error", "docker", message)
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

                UiButton {
                    Layout.preferredWidth: 110
                    label: qsTr("Cancel")
                    color: "secondary"
                    variant: "outline"
                    size: "sm"
                    onClicked: confirmDialog.close()
                }

                UiButton {
                    Layout.preferredWidth: 110
                    label: qsTr("Remove")
                    color: "error"
                    variant: "solid"
                    size: "sm"
                    onClicked: dockerPage.confirmRemove()
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
                            : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0)

                        Behavior on color {
                            ColorAnimation {
                                duration: Theme.animHover
                                easing.type: Theme.easingStandard
                            }
                        }
                    }

                    Image {
                        id: rescanIcon
                        anchors.centerIn: parent
                        width: Theme.iconSm
                        height: Theme.iconSm
                        source: Theme.icon("refresh")
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
                        // Only one operation at a time: a rescan cannot start
                        // while an action runs, and vice versa.
                        enabled: !dockerPage.busy
                        onClicked: dockerPage.reload()
                    }

                    // Full rescan only. Actions (start/stop/remove/prune) show
                    // their own loader on the button instead of spinning this.
                    NumberAnimation {
                        id: spin
                        target: rescanIcon
                        property: "rotation"
                        from: 0
                        to: 360
                        duration: 900
                        easing.type: Easing.Linear
                        onFinished: {
                            if (dockerPage.scanning) {
                                rescanIcon.rotation = 0
                                spin.start()
                            } else {
                                rescanIcon.rotation = 0
                            }
                        }
                    }

                    Connections {
                        target: dockerPage.svc
                        function onScanningChanged() {
                            if (dockerPage.scanning && !spin.running) {
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
                            source: Theme.icon("docker")
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
                            Layout.preferredWidth: 8
                            Layout.preferredHeight: 8
                            radius: 4
                            color: (dockerPage.engine && dockerPage.engine.available) ? Theme.success : Theme.error
                        }

                        UiButton {
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: 88
                            visible: dockerPage.engine && dockerPage.engine.colimaInstalled === true
                            enabled: dockerPage.svc && !dockerPage.busy
                            // Engine start/stop: the loader lives in this button
                            // (colima start/stop can take up to two minutes).
                            loading: dockerPage.controllingId === "engine"
                            label: (dockerPage.engine && dockerPage.engine.colimaRunning) ? qsTr("Stop") : qsTr("Start")
                            color: "secondary"
                            variant: "outline"
                            size: "sm"
                            onClicked: {
                                if (!dockerPage.svc)
                                    return
                                if (dockerPage.engine.colimaRunning)
                                    dockerPage.svc.stopEngine()
                                else
                                    dockerPage.svc.startEngine()
                            }
                        }

                        UiButton {
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: 88
                            visible: dockerPage.engine && dockerPage.engine.available === true
                            enabled: dockerPage.svc && !dockerPage.busy
                            loading: dockerPage.controllingId === "prune"
                            label: qsTr("Prune")
                            color: "secondary"
                            variant: "outline"
                            size: "sm"
                            onClicked: {
                                if (dockerPage.svc)
                                    dockerPage.svc.prune()
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
                text: dockerPage.scanning
                    ? qsTr("Scanning…") : qsTr("No containers")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeMd
            }

            // Shared container card: one compose group or standalone list
            // instantiates it per container via the repeaters below.
            Component {
                id: containerDelegate

                UiListElement {
                    id: containerCard
                    required property var modelData
                    width: parent.width
                    height: inner.implicitHeight + 2 * Theme.spacingMd
                    clickable: false

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
                                    color: Theme.success
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
                                Layout.preferredWidth: 8
                                Layout.preferredHeight: 8
                                radius: 4
                                color: containerCard.modelData.running ? Theme.success : Theme.error
                            }

                            UiButton {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 76
                                // Also false while any other action runs — e.g.
                                // stopping the engine disables every container
                                // action for the duration.
                                enabled: dockerPage.svc && !dockerPage.busy
                                    && dockerPage.engineUp
                                loading: containerCard.modelData.running
                                    ? dockerPage.inFlight("stop", containerCard.modelData.name)
                                    : dockerPage.inFlight("start", containerCard.modelData.name)
                                label: containerCard.modelData.running ? qsTr("Stop") : qsTr("Start")
                                color: "secondary"
                                variant: "outline"
                                size: "sm"
                                onClicked: {
                                    if (!dockerPage.svc)
                                        return
                                    if (containerCard.modelData.running)
                                        dockerPage.svc.stopContainer(containerCard.modelData.name)
                                    else
                                        dockerPage.svc.startContainer(containerCard.modelData.name)
                                }
                            }

                            UiButton {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 76
                                enabled: dockerPage.svc && !dockerPage.busy
                                label: qsTr("Logs")
                                color: "secondary"
                                variant: "outline"
                                size: "sm"
                                onClicked: dockerPage.toggleLogs(containerCard.modelData.name)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm
                            visible: containerCard.modelData.running

                            Item { Layout.fillWidth: true }

                            UiButton {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 88
                                enabled: dockerPage.svc && !dockerPage.busy
                                loading: dockerPage.inFlight("restart", containerCard.modelData.name)
                                label: qsTr("Restart")
                                color: "secondary"
                                variant: "outline"
                                size: "sm"
                                onClicked: dockerPage.svc.restartContainer(containerCard.modelData.name)
                            }

                            UiButton {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 88
                                enabled: dockerPage.svc && !dockerPage.busy
                                loading: dockerPage.inFlight("rm", containerCard.modelData.name)
                                label: qsTr("Remove")
                                color: "error"
                                variant: "outline"
                                size: "sm"
                                onClicked: dockerPage.askRemove("container",
                                    containerCard.modelData.name, containerCard.modelData.name)
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

                        UiButton {
                            Layout.alignment: Qt.AlignVCenter
                            enabled: dockerPage.svc && !dockerPage.busy
                            loading: dockerPage.inFlight("start",
                                "group:" + groupBlock.modelData)
                            label: qsTr("Start all")
                            color: "neutral"
                            variant: "link"
                            size: "sm"
                            onClicked: dockerPage.groupControl(
                                dockerPage.containersOf(groupBlock.modelData), "start",
                                groupBlock.modelData)
                        }

                        Label {
                            Layout.alignment: Qt.AlignVCenter
                            text: "·"
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSm
                        }

                        UiButton {
                            Layout.alignment: Qt.AlignVCenter
                            enabled: dockerPage.svc && !dockerPage.busy
                            loading: dockerPage.inFlight("stop",
                                "group:" + groupBlock.modelData)
                            label: qsTr("Stop")
                            color: "neutral"
                            variant: "link"
                            size: "sm"
                            onClicked: dockerPage.groupControl(
                                dockerPage.containersOf(groupBlock.modelData), "stop",
                                groupBlock.modelData)
                        }

                        Label {
                            Layout.alignment: Qt.AlignVCenter
                            text: "·"
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSm
                        }

                        UiButton {
                            Layout.alignment: Qt.AlignVCenter
                            enabled: dockerPage.svc && !dockerPage.busy
                            loading: dockerPage.inFlight("restart",
                                "group:" + groupBlock.modelData)
                            label: qsTr("Restart")
                            color: "neutral"
                            variant: "link"
                            size: "sm"
                            onClicked: dockerPage.groupControl(
                                dockerPage.containersOf(groupBlock.modelData), "restart",
                                groupBlock.modelData)
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
                text: dockerPage.scanning
                    ? qsTr("Scanning…") : qsTr("No images")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeMd
            }

            Repeater {
                model: dockerPage.images

                delegate: UiListElement {
                    id: imageCard
                    required property var modelData
                    width: parent.width
                    height: 64
                    clickable: false

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

                        UiButton {
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: 88
                            enabled: dockerPage.svc && !dockerPage.busy
                            loading: dockerPage.inFlight("rmi", imageCard.modelData.id)
                            label: qsTr("Remove")
                            color: "error"
                            variant: "outline"
                            size: "sm"
                            onClicked: dockerPage.askRemove("image", imageCard.modelData.id,
                                dockerPage.imageTitle(imageCard.modelData))
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
