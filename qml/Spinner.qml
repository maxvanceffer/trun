import QtQuick

// Radial spoke spinner with a fading trail behind the leading spoke.
// The trail is a static opacity gradient; a single RotationAnimator
// (render thread) rotates the whole wheel.
Item {
    id: root

    property color color: Theme.textMuted
    property int spokes: 12
    property bool running: true
    // ms per revolution
    property int period: 900

    implicitWidth: 16
    implicitHeight: 16

    Item {
        id: wheel
        anchors.fill: parent

        Repeater {
            model: root.spokes

            Rectangle {
                readonly property real spokeAngle: index * (360 / root.spokes)
                width: Math.max(2, root.width * 0.14)
                height: root.height * 0.3
                radius: width / 2
                color: root.color
                opacity: 0.15 + 0.85 * (1 - index / root.spokes)
                x: (root.width - width) / 2
                y: root.height * 0.04
                transform: Rotation {
                    origin.x: width / 2
                    origin.y: root.height / 2 - y
                    angle: spokeAngle
                }
            }
        }

        RotationAnimator on rotation {
            from: 0
            to: 360
            duration: root.period
            loops: Animation.Infinite
            running: root.running && root.visible
        }
    }
}
