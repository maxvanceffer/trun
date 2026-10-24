import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Ui"
import "JsonFormat.js" as JsonFormat

// R6 response panel — the lower half of the request page's fixed split. Four
// phases, driven entirely by the page's `phase`:
//   empty    nothing sent yet — a short hint
//   loading  a request is in flight — spinner + "Sending…"
//   error    a network-level failure (4xx/5xx are NOT errors — they are a
//            success with a status) — host + message, no tabs
//   success  a response came back: status pill (colour by class), time, size,
//            Body/Headers tabs and Copy. Body is pretty-printed + highlighted
//            JSON (qml/JsonFormat.js) with a line-number gutter; Headers is a
//            name/value list.
//
// The panel owns no service: Copy emits copyRequested(text) so the page can
// route it to the clipboard (QML has none of its own).
Item {
    id: panelRoot

    objectName: "apiResponsePanel"

    property string phase: "empty"     // empty | loading | error | success
    property int status: 0
    property string reason: ""
    property int elapsedMs: 0
    property int bodySize: 0
    property string body: ""
    property var headers: []
    property string errorText: ""
    property string host: ""

    signal copyRequested(string text)

    property int responseTab: 0        // 0 Body · 1 Headers

    readonly property bool isSuccess: panelRoot.phase === "success"
    readonly property bool isLoading: panelRoot.phase === "loading"
    readonly property bool isError: panelRoot.phase === "error"
    readonly property bool isEmpty: panelRoot.phase === "empty"

    // Theme colours for the highlighter. Nuxt token -> JSON token class, the
    // map written in docs/ui-rules.md (case "Code").
    readonly property var codeColors: ({
        key: "" + Theme.textPrimary,
        string: "" + Theme.primary,
        number: "" + Theme.info,
        keyword: "" + Theme.error,
        punct: "" + Theme.textSubtle,
        plain: "" + Theme.textMuted
    })

    readonly property var formatted: JsonFormat.render(panelRoot.body, panelRoot.codeColors)

    function statusColor(code) {
        if (code >= 200 && code < 300)
            return "success"
        if (code >= 300 && code < 400)
            return "info"
        if (code >= 400 && code < 500)
            return "warning"
        if (code >= 500)
            return "error"
        return "neutral"
    }

    function statusLabel() {
        if (panelRoot.status <= 0)
            return ""
        return panelRoot.reason === ""
            ? String(panelRoot.status)
            : panelRoot.status + " " + panelRoot.reason
    }

    function formatMs(ms) {
        if (ms < 1000)
            return ms + " ms"
        return (ms / 1000).toFixed(2) + " s"
    }

    function formatSize(bytes) {
        if (bytes < 1024)
            return bytes + " B"
        if (bytes < 1024 * 1024)
            return (bytes / 1024).toFixed(1) + " KB"
        return (bytes / (1024 * 1024)).toFixed(1) + " MB"
    }

    // Copy follows the visible tab: the raw body, or "name: value" lines.
    function copy() {
        if (panelRoot.responseTab === 1) {
            var lines = []
            for (var i = 0; i < panelRoot.headers.length; ++i) {
                var h = panelRoot.headers[i]
                lines.push((h.name || "") + ": " + (h.value || ""))
            }
            panelRoot.copyRequested(lines.join("\n"))
        } else {
            panelRoot.copyRequested(panelRoot.body)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.cardBackground
        border.color: Theme.border
        border.width: 1
        radius: Theme.radiusMd

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingMd
            spacing: Theme.spacingSm

            // ─── Header: title · status pill · time · size · Copy ──────
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm

                Label {
                    text: qsTr("Response")
                    color: Theme.textDimmed
                    font.pixelSize: Theme.fontSizeSm
                    font.weight: Font.DemiBold
                    Layout.alignment: Qt.AlignVCenter
                }

                UiTag {
                    id: statusPill
                    objectName: "apiStatusPill"
                    visible: panelRoot.isSuccess
                    label: panelRoot.statusLabel()
                    color: panelRoot.statusColor(panelRoot.status)
                    variant: "soft"
                    size: "sm"
                    Layout.alignment: Qt.AlignVCenter
                }

                Label {
                    objectName: "apiResponseTime"
                    visible: panelRoot.isSuccess
                    text: panelRoot.formatMs(panelRoot.elapsedMs)
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSm
                    Layout.alignment: Qt.AlignVCenter
                }

                Label {
                    objectName: "apiResponseSize"
                    visible: panelRoot.isSuccess
                    text: panelRoot.formatSize(panelRoot.bodySize)
                    color: Theme.textSubtle
                    font.pixelSize: Theme.fontSizeSm
                    Layout.alignment: Qt.AlignVCenter
                }

                Item { Layout.fillWidth: true }

                BusyLabel {
                    objectName: "apiResponseLoading"
                    visible: panelRoot.isLoading
                    layout: "horizontal"
                    text: qsTr("Sending…")
                    textColor: Theme.textMuted
                    Layout.alignment: Qt.AlignVCenter
                }

                UiButton {
                    id: copyButton
                    objectName: "apiResponseCopy"
                    visible: panelRoot.isSuccess
                    label: qsTr("Copy")
                    color: "neutral"
                    variant: "outline"
                    size: "sm"
                    leadingIconSource: Theme.icon("clipboard")
                    Layout.alignment: Qt.AlignVCenter
                    onClicked: panelRoot.copy()
                }
            }

            // ─── Content ────────────────────────────────────────────────
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                // empty
                Label {
                    objectName: "apiResponseEmpty"
                    anchors.centerIn: parent
                    visible: panelRoot.isEmpty
                    text: qsTr("Send a request to see the response")
                    color: Theme.textSubtle
                    font.pixelSize: Theme.fontSizeSm
                }

                // error
                ColumnLayout {
                    objectName: "apiResponseError"
                    anchors.centerIn: parent
                    width: Math.min(parent.width, 480)
                    visible: panelRoot.isError
                    spacing: Theme.spacingXs

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Request failed")
                        color: Theme.error
                        font.pixelSize: Theme.fontSizeMd
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Label {
                        objectName: "apiResponseErrorMessage"
                        Layout.fillWidth: true
                        text: panelRoot.errorText
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSm
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: panelRoot.host !== ""
                        text: panelRoot.host
                        color: Theme.textSubtle
                        font.family: Theme.fontMono
                        font.pixelSize: Theme.fontSizeSm
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                // success: Body / Headers
                ColumnLayout {
                    anchors.fill: parent
                    visible: panelRoot.isSuccess
                    spacing: Theme.spacingSm

                    UiTabs {
                        id: respTabs
                        objectName: "apiResponseTabs"
                        Layout.fillWidth: true
                        color: "primary"
                        items: [{ label: qsTr("Body") }, { label: qsTr("Headers") }]
                        currentIndex: panelRoot.responseTab
                        onActivated: function (index) { panelRoot.responseTab = index }
                    }

                    // Body: line-number gutter + highlighted code, one scroll.
                    ScrollView {
                        id: bodyScroll
                        objectName: "apiResponseBody"
                        visible: panelRoot.responseTab === 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AsNeeded
                        ScrollBar.vertical.policy: ScrollBar.AsNeeded

                        Row {
                            spacing: Theme.spacingMd

                            Text {
                                objectName: "apiResponseGutter"
                                text: panelRoot.formatted.gutter
                                color: Theme.textSubtle
                                font.family: Theme.fontMono
                                font.pixelSize: Theme.fontSizeMd
                                textFormat: Text.PlainText
                            }

                            Text {
                                objectName: "apiResponseCode"
                                text: panelRoot.formatted.code
                                textFormat: Text.RichText
                                color: Theme.textMuted
                                font.family: Theme.fontMono
                                font.pixelSize: Theme.fontSizeMd
                                wrapMode: Text.NoWrap
                            }
                        }
                    }

                    // Headers: name/value list, one scroll.
                    ScrollView {
                        id: headersScroll
                        objectName: "apiResponseHeaders"
                        visible: panelRoot.responseTab === 1
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        ScrollBar.vertical.policy: ScrollBar.AsNeeded

                        ColumnLayout {
                            width: headersScroll.availableWidth
                            spacing: Theme.spacingXs

                            Repeater {
                                model: panelRoot.headers

                                RowLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSm

                                    Label {
                                        text: (modelData.name || "") + ":"
                                        color: Theme.textPrimary
                                        font.family: Theme.fontMono
                                        font.pixelSize: Theme.fontSizeMd
                                        Layout.minimumWidth: 140
                                        Layout.alignment: Qt.AlignTop
                                    }
                                    Label {
                                        text: modelData.value || ""
                                        color: Theme.textMuted
                                        font.family: Theme.fontMono
                                        font.pixelSize: Theme.fontSizeMd
                                        wrapMode: Text.Wrap
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
