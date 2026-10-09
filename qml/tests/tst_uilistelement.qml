import QtQuick
import QtTest
import ".."       // qmldir -> singleton Theme (this file sits in qml/tests/,
                  // so the implicit own-directory import does not reach it)
import "../Ui"

// tst_uilistelement — UiListElement render harness (Qt Quick Test).
//
//   QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic \
//     qmltestrunner -input qml/tests/tst_uilistelement.qml 2>&1 | tee uilistelement.log
//
//   python3 qml/tests/verify_uilistelement.py uilistelement.log \
//       uilistelement-base.png uilistelement-hot.png
//
// Renders the shared list card twice: idle and hovered. The verifier checks the
// rounded corners, the card fill, and that the 1px border flips from
// Theme.border to Theme.ring on hover — the exact thing the old row-fill hover
// broke (a square fill painting over the card's rounded corners).
TestCase {
    id: tc
    name: "UiListElement"
    width: 420
    height: 300
    when: windowShown
    visible: true

    readonly property string baseShot: "uilistelement-base.png"
    readonly property string hotShot: "uilistelement-hot.png"
    property int shots: 0

    Rectangle {
        anchors.fill: parent
        color: Theme.windowBackground
    }

    UiListElement {
        id: baseCard
        objectName: "card-base"
        x: 40
        y: 40
        width: 300
        height: 64
    }

    // Carries a control so the harness also exercises real content inside the
    // card (the click itself is pinned by the C++ click-through test).
    UiListElement {
        id: hotCard
        objectName: "card-hot"
        x: 40
        y: 150
        width: 300
        height: 64

        UiButton {
            objectName: "card-hot-btn"
            anchors.centerIn: parent
            label: "Act"
        }
    }

    function rgba(c) {
        return [c.r.toFixed(4), c.g.toFixed(4), c.b.toFixed(4), c.a.toFixed(4)].join(",")
    }

    function logCard(tag, c) {
        console.log("CARD " + tag + " " + c.objectName
            + " x=" + c.x.toFixed(1) + " y=" + c.y.toFixed(1)
            + " w=" + c.width.toFixed(1) + " h=" + c.height.toFixed(1)
            + " radius=" + c.radius
            + " fill=" + rgba(c.color)
            + " border=" + rgba(c.border.color)
            + " borderW=" + c.border.width
            + " hovered=" + c.hovered)
    }

    function grab(path) {
        tc.grabToImage(function (res) {
            var ok = res.saveToFile(path)
            console.log("SAVED " + path + " ok=" + ok
                + " hovered=" + hotCard.hovered
                + " border=" + rgba(hotCard.border.color))
            tc.shots = tc.shots + 1
        })
    }

    function test_a_capture() {
        wait(500)
        logCard("BASE", baseCard)
        grab(baseShot)
        tryVerify(function () { return tc.shots >= 1 }, 8000)

        mouseMove(hotCard, 12, 12)
        tryVerify(function () { return hotCard.hovered }, 2000)
        wait(500)
        logCard("HOT", hotCard)
        grab(hotShot)
        tryVerify(function () { return tc.shots >= 2 }, 8000)
    }
}
