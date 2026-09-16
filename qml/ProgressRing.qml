import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

// Circular progress: dimmed disc, 3px gap, dimmed track, full-color arc.
Item {
    id: ring

    property real fraction: 0
    property color ringColor: "#22c55e"
    property string text: ""

    readonly property real clamped: Math.min(Math.max(fraction, 0), 0.999)
    readonly property color dimmed: Qt.rgba(ringColor.r, ringColor.g, ringColor.b, 0.1)

    implicitWidth: 48
    implicitHeight: 48

    // Inner disc (3px gap to the ring)
    Rectangle {
        anchors.centerIn: parent
        width: 32
        height: 32
        radius: 16
        color: ring.dimmed
    }

    // Track: full circle in the dimmed color
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: ring.dimmed
        border.width: 5
        radius: 24
    }

    // Progress arc in full color, concentric with the track (r=21.5).
    // Hidden at zero: a degenerate arc would leave a round-cap dot.
    // MSAA layer: Shape flattens curves into facets; multisampling keeps
    // the arc smooth instead of blocky. Cheap at 48px.
    Shape {
        id: arcShape
        anchors.fill: parent
        antialiasing: true
        layer.enabled: true
        layer.samples: 4
        visible: ring.clamped > 0

        readonly property real ringRadius: 21.5
        readonly property real sweep: ring.clamped * 360
        readonly property real endAngle: -90 + sweep
        readonly property real endX: 24 + ringRadius * Math.cos(endAngle * Math.PI / 180)
        readonly property real endY: 24 + ringRadius * Math.sin(endAngle * Math.PI / 180)

                ShapePath {
                    strokeColor: ring.ringColor
                    strokeWidth: 5
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap

                    PathMove { x: 24; y: 24 - arcShape.ringRadius }
                    PathArc {
                        x: arcShape.endX
                        y: arcShape.endY
                        radiusX: arcShape.ringRadius
                        radiusY: arcShape.ringRadius
                    useLargeArc: arcShape.sweep > 180
                    }
                }
            }

    // Center value
    Label {
        anchors.centerIn: parent
        text: ring.text
        color: Theme.textPrimary
        font.pixelSize: 10
        font.bold: true
    }
}
