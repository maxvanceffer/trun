import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Locally installed database services (detected by DatabaseService).
// Scanning is manual (the Rescan button); it is not triggered on open.
Item {
    id: databasesPage
    objectName: "databasesPage"

    // The engine may be absent in standalone tests.
    readonly property var db: (typeof databaseService !== "undefined") ? databaseService : null
    readonly property var services: db ? db.services : []
    readonly property bool busy: db ? db.busy : false

    function reload() {
        if (databasesPage.db)
            databasesPage.db.refresh()
    }

    function iconFor(engine) {
        switch (engine) {
        case "postgresql": return iconBaseUrl + "postgresql-badge-64px.png"
        case "mysql":      return iconBaseUrl + "mysql-64px.png"
        case "mongodb":    return iconBaseUrl + "mongodb-64px.png"
        case "redis":      return iconBaseUrl + "redis-64px.png"
        default:           return iconBaseUrl + "default-32px.png"
        }
    }

    function titleText(svc) {
        return svc.version && svc.version !== "" ? svc.name + " " + svc.version : svc.name
    }

    function metaText(svc) {
        var parts = []
        if (svc.port > 0)
            parts.push(":" + svc.port)
        if (svc.source && svc.source !== "")
            parts.push(svc.source)
        return parts.join(" · ")
    }

    Column {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        RowLayout {
            width: parent.width
            spacing: Theme.spacingSm

            Label {
                text: qsTr("SERVICES")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeSm
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            // Rescan button: spins while a scan runs, always finishing at
            // least one full turn (the scan itself runs off the GUI thread).
            Item {
                id: rescanControl
                Layout.preferredWidth: Theme.sidebarRowHeight
                Layout.preferredHeight: Theme.sidebarRowHeight

                // Ghost hover (same as IconButton): transparent until hover.
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
                    onClicked: databasesPage.reload()
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
                        if (databasesPage.busy) {
                            rescanIcon.rotation = 0
                            spin.start()
                        } else {
                            rescanIcon.rotation = 0
                        }
                    }
                }

                Connections {
                    target: databasesPage.db
                    function onBusyChanged() {
                        if (databasesPage.busy && !spin.running) {
                            rescanIcon.rotation = 0
                            spin.start()
                        }
                        // On busy -> false the current turn is allowed to
                        // finish; onFinished resets the rotation to 0.
                    }
                }

                Component.onCompleted: {
                    if (databasesPage.busy) {
                        rescanIcon.rotation = 0
                        spin.start()
                    }
                }
            }
        }

        Label {
            visible: databasesPage.services.length === 0
            text: (databasesPage.db && databasesPage.db.busy)
                ? qsTr("Scanning…") : qsTr("No databases found")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeMd
        }

        Repeater {
            model: databasesPage.services

            delegate: Rectangle {
                id: serviceCard
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

                    Image {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignVCenter
                        source: databasesPage.iconFor(serviceCard.modelData.engine)
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
                            text: databasesPage.titleText(serviceCard.modelData)
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeMd
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: databasesPage.metaText(serviceCard.modelData)
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
                        color: serviceCard.modelData.running ? "#22c55e" : Theme.destructive
                    }

                    Button {
                        id: toggle
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: 88
                        implicitHeight: 30
                        enabled: serviceCard.modelData.controllable
                                 && !(databasesPage.db && databasesPage.db.busy)
                        onClicked: {
                            if (!databasesPage.db) return
                            if (serviceCard.modelData.running)
                                databasesPage.db.stopService(serviceCard.modelData.id)
                            else
                                databasesPage.db.startService(serviceCard.modelData.id)
                        }

                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: toggle.enabled
                                ? (toggle.pressed ? Qt.darker(Theme.secondary, 1.15) : Theme.secondary)
                                : "transparent"
                            border.color: Theme.border
                            border.width: 1
                        }

                        contentItem: Label {
                            text: serviceCard.modelData.running ? qsTr("Stop") : qsTr("Start")
                            color: Theme.secondaryForeground
                            font.pixelSize: Theme.fontSizeMd
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
        }
    }
}
