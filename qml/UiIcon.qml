import QtQuick
import QtQuick.Effects

pragma ComponentBehavior: Bound

// Tinted monochrome icon (Nuxt UI Icon equivalent): the white source
// is recolored to `color` via a single-pass effect, alpha preserved.
Item {
    id: root

    property url source
    property color color: Theme.textPrimary
    property int size: Theme.iconSm

    implicitWidth: size
    implicitHeight: size

    Image {
        id: iconImage
        anchors.fill: parent
        source: root.source
        // 2x raster for Retina: a 12px item needs a 24px texture,
        // otherwise the GPU upscales and the glyph blurs.
        sourceSize.width: root.size * 2
        sourceSize.height: root.size * 2
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        visible: false
    }

    MultiEffect {
        anchors.fill: parent
        source: iconImage
        colorization: 1.0
        colorizationColor: root.color
    }
}
