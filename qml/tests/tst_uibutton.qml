import QtQuick
import QtTest
import ".."       // qmldir -> singleton Theme (this file sits in qml/tests/,
                  // so the implicit own-directory import does not reach it)
import "../Ui"

// tst_uibutton — UiButton Nuxt-UI parity harness (Qt Quick Test).
//
// Run from anywhere, nothing here is machine- or build-tree specific:
//
//   QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic \
//     qmltestrunner -input qml/tests/tst_uibutton.qml 2>&1 | tee uibutton.log
//
//   python3 qml/tests/verify_uibutton.py uibutton.log uibutton-base.png uibutton-hot.png
//
// The two screenshots are written to the runner's working directory, so pass
// matching paths to the verifier.
//
// Root must be a TestCase (an Item) so qmltestrunner's QQuickView can show it
// and expose the window; mouse simulation is refused until windowShown.
//
// The test logs every button's rect plus the computed fill/foreground/stroke,
// grabs a base screenshot, then presses one button and focuses another
// (keyboard reason, so focus-visible applies) and grabs again. A PIL script
// checks both PNGs against the log.
TestCase {
    id: tc
    name: "UiButton"
    width: 1120
    height: 780
    when: windowShown
    // QML's TestCase sets visible:false by default (TestCase.qml). Qt Quick's
    // hit-test then skips the whole subtree (eventTargets: !child->isVisible()),
    // so no simulated pointer event would ever reach a button. grabToImage()
    // still renders it, which is why the screenshots looked fine.
    visible: true

    // This file lives in <repo>/qml/tests/, so the source icons are two levels
    // up. Resolving against the test file keeps the harness runnable from any
    // working directory and on any machine.
    readonly property string iconDir: Qt.resolvedUrl("../../assets/icons/").toString().replace("file://", "")
    readonly property string baseShot: "uibutton-base.png"
    readonly property string hotShot: "uibutton-hot.png"
    property int shots: 0

    Rectangle {
        anchors.fill: parent
        color: Theme.windowBackground
    }

    // icon probe: proves whether leadingIconSource resolves and paints at all
    Image {
        id: iconProbe
        source: iconDir + "refresh.png"
        onStatusChanged: console.log("ICON status=" + status
            + " sourceSize=" + sourceSize.width + "x" + sourceSize.height
            + " size=" + width + "x" + height)
    }

    Component.onCompleted: console.log("BOOT shown=" + windowShown
        + " iconDir=" + iconDir)

    Timer {
        interval: 1500
        running: true
        onTriggered: console.log("TICK shots=" + tc.shots + " shown=" + tc.windowShown)
    }

    Column {
        x: 44
        y: 34
        spacing: 26

        // 1. sizes, icon + label
        Row {
            spacing: 24
            UiButton { id: sXs; objectName: "s-xs"; size: "xs"; label: "Deploy"; leadingIconSource: iconDir + "refresh.png" }
            UiButton { id: sSm; objectName: "s-sm"; size: "sm"; label: "Deploy"; leadingIconSource: iconDir + "refresh.png" }
            UiButton { id: sMd; objectName: "s-md"; size: "md"; label: "Deploy"; leadingIconSource: iconDir + "refresh.png" }
            UiButton { id: sLg; objectName: "s-lg"; size: "lg"; label: "Deploy"; leadingIconSource: iconDir + "refresh.png" }
            UiButton { id: sXl; objectName: "s-xl"; size: "xl"; label: "Deploy"; leadingIconSource: iconDir + "refresh.png" }
        }

        // 2. sizes, icon only -> equal padding (square), side != height because
        //    an empty label still takes part in the row gap, exactly as Nuxt's
        //    empty <span> does inside a flex row with gap.
        Row {
            spacing: 24
            UiButton { id: qXs; objectName: "q-xs"; size: "xs"; leadingIconSource: iconDir + "refresh.png" }
            UiButton { id: qSm; objectName: "q-sm"; size: "sm"; leadingIconSource: iconDir + "refresh.png" }
            UiButton { id: qMd; objectName: "q-md"; size: "md"; leadingIconSource: iconDir + "refresh.png" }
            UiButton { id: qLg; objectName: "q-lg"; size: "lg"; leadingIconSource: iconDir + "refresh.png" }
            UiButton { id: qXl; objectName: "q-xl"; size: "xl"; leadingIconSource: iconDir + "refresh.png" }
        }

        // 3. sizes, label only -> text width tiers: xs==sm (12px), md==lg (14px)
        Row {
            spacing: 24
            UiButton { id: tXs; objectName: "t-xs"; size: "xs"; label: "Deploy" }
            UiButton { id: tSm; objectName: "t-sm"; size: "sm"; label: "Deploy" }
            UiButton { id: tMd; objectName: "t-md"; size: "md"; label: "Deploy" }
            UiButton { id: tLg; objectName: "t-lg"; size: "lg"; label: "Deploy" }
            UiButton { id: tXl; objectName: "t-xl"; size: "xl"; label: "Deploy" }
        }

        // 4. variants (color: primary)
        Row {
            spacing: 24
            UiButton { id: v1; objectName: "v-solid"; variant: "solid"; label: "Button" }
            UiButton { id: v2; objectName: "v-outline"; variant: "outline"; label: "Button" }
            UiButton { id: v3; objectName: "v-soft"; variant: "soft"; label: "Button" }
            UiButton { id: v4; objectName: "v-subtle"; variant: "subtle"; label: "Button" }
            UiButton { id: v5; objectName: "v-ghost"; variant: "ghost"; label: "Button" }
            UiButton { id: v6; objectName: "v-link"; variant: "link"; label: "Button" }
        }

        // 5. colours (variant: outline)
        Row {
            spacing: 24
            UiButton { id: c1; objectName: "c-primary"; color: "primary"; variant: "outline"; label: "Button" }
            UiButton { id: c2; objectName: "c-secondary"; color: "secondary"; variant: "outline"; label: "Button" }
            UiButton { id: c3; objectName: "c-neutral"; color: "neutral"; variant: "outline"; label: "Button" }
            UiButton { id: c4; objectName: "c-success"; color: "success"; variant: "outline"; label: "Button" }
            UiButton { id: c5; objectName: "c-info"; color: "info"; variant: "outline"; label: "Button" }
            UiButton { id: c6; objectName: "c-warning"; color: "warning"; variant: "outline"; label: "Button" }
            UiButton { id: c7; objectName: "c-error"; color: "error"; variant: "outline"; label: "Button" }
        }

        // 5b. solid ink across every colour — Nuxt writes `text-inverted`,
        //     which is white in light mode (1.83–3.65:1, all fail WCAG AA)
        //     and was #ffe1c1 on #fdc700 for secondary in dark (1.26:1, the
        //     "Open" button in DatabasesPage). Both runs must show #181818.
        Row {
            spacing: 24
            UiButton { id: so1; objectName: "s-primary"; color: "primary"; variant: "solid"; label: "Button" }
            UiButton { id: so2; objectName: "s-secondary"; color: "secondary"; variant: "solid"; label: "Open" }
            UiButton { id: so3; objectName: "s-success"; color: "success"; variant: "solid"; label: "Button" }
            UiButton { id: so4; objectName: "s-info"; color: "info"; variant: "solid"; label: "Button" }
            UiButton { id: so5; objectName: "s-warning"; color: "warning"; variant: "solid"; label: "Button" }
            UiButton { id: so6; objectName: "s-error"; color: "error"; variant: "solid"; label: "Button" }
            // neutral's fill is bg-inverted, so it keeps text-inverted
            UiButton { id: so7; objectName: "s-neutral"; color: "neutral"; variant: "solid"; label: "Button" }
        }

        // 6. disabled (opacity 0.75, no hover response, base classes kept)
        Row {
            spacing: 24
            UiButton { id: d1; objectName: "d-on"; label: "Button" }
            UiButton { id: d2; objectName: "d-off"; label: "Button"; enabled: false }
            UiButton { id: d3; objectName: "d-off-outline"; variant: "outline"; label: "Button"; enabled: false }
            UiButton { id: d4; objectName: "d-off-ghost"; variant: "ghost"; label: "Button"; enabled: false }
        }

        // 7. press target (hover:/active: share one value in button.ts)
        Row {
            spacing: 24
            UiButton { id: hov; objectName: "hover"; label: "Hover me"; onClicked: console.log("CLICKED") }
        }

        // 8. focus target (focus-visible:outline-3 outline-<c>/25)
        Row {
            spacing: 24
            UiButton { id: foc; objectName: "focus"; variant: "outline"; label: "Focus me" }
        }

        // 9. loading: same geometry as the idle twin, spinner replaces leading icon
        Row {
            spacing: 24
            UiButton { id: l0; objectName: "load-idle"; size: "sm"; color: "secondary"; variant: "outline"; label: "Rescan"; leadingIconSource: iconDir + "refresh.png" }
            UiButton { id: l1; objectName: "load-busy"; size: "sm"; color: "secondary"; variant: "outline"; label: "Rescan"; leadingIconSource: iconDir + "refresh.png"; loading: true }
        }

        // 10. raw input probe — separates "events never arrive" from
        //     "UiButton's handlers are wrong". Not part of the parity checks.
        Row {
            spacing: 24
            MouseArea {
                id: probe
                objectName: "probe"
                width: 64
                height: 32
                onPressed: console.log("PROBE PRESS")
                onReleased: console.log("PROBE RELEASE")
                onPositionChanged: console.log("PROBE MOVE")
            }
        }
    }

    readonly property var all: [sXs, sSm, sMd, sLg, sXl,
        qXs, qSm, qMd, qLg, qXl,
        tXs, tSm, tMd, tLg, tXl,
        v1, v2, v3, v4, v5, v6,
        c1, c2, c3, c4, c5, c6, c7,
        so1, so2, so3, so4, so5, so6, so7,
        d1, d2, d3, d4, hov, foc, l0, l1]

    function rgba(c) {
        return [c.r.toFixed(4), c.g.toFixed(4), c.b.toFixed(4), c.a.toFixed(4)].join(",")
    }

    function logRects(tag) {
        for (var i = 0; i < all.length; ++i) {
            var b = all[i]
            var p = b.mapToItem(tc, 0, 0)
            console.log("RECT " + tag + " " + b.objectName
                + " x=" + p.x.toFixed(1) + " y=" + p.y.toFixed(1)
                + " w=" + b.width.toFixed(1) + " h=" + b.height.toFixed(1)
                + " fill=" + rgba(b.fill)
                + " fg=" + rgba(b.foreground)
                + " stroke=" + rgba(b.stroke)
                + " enabled=" + b.isEnabled
                // colour + variant let the PIL check assert the ink choice
                // itself (solid non-neutral must be the carbon ink), instead
                // of only confirming the render matches what we computed.
                + " color=" + b.color + " variant=" + b.variant)
        }
    }

    function grab(path) {
        tc.grabToImage(function (res) {
            var ok = res.saveToFile(path)
            console.log("SAVED " + path + " ok=" + ok)
            tc.shots = tc.shots + 1
        })
    }

    function test_a_capture() {
        console.log("TEST ENTER shown=" + windowShown)
        wait(700)
        console.log("ICONPROBE status=" + iconProbe.status + " size=" + iconProbe.width + "x" + iconProbe.height
            + " srcW=" + iconProbe.sourceSize.width + " srcH=" + iconProbe.sourceSize.height)
        console.log("DIAG iconPx=" + sXs.iconPx + " hasLeading=" + sXs.hasLeading
            + " size=" + sXs.size + " isSquare=" + sXs.isSquare
            + " padX=" + sXs.padX + " gap=" + sXs.gapPx + " loading=" + sXs.loading)
        console.log("VIS tc=" + tc.visible + " row=" + sXs.parent.visible
            + " col=" + sXs.parent.parent.visible
            + " sXs=" + sXs.visible + " contentItem=" + sXs.contentItem.visible)
        var kids = sXs.contentItem.children
        for (var ki = 0; ki < kids.length; ++ki) {
            var k = kids[ki]
            var pw = "NA"
            try { pw = k.Layout.preferredWidth } catch (e) { pw = "ERR" }
            console.log("KID " + ki + " " + k + " vis=" + k.visible
                + " w=" + k.width + " h=" + k.height
                + " iw=" + (k.implicitWidth !== undefined ? k.implicitWidth : "-")
                + " pw=" + pw
                + " src=" + (k.source !== undefined ? String(k.source) : "-"))
        }
        logRects("BASE")
        grab(tc.baseShot)
        tryVerify(function () { return tc.shots >= 1 }, 8000)

        // probe first: does ANY mouse input reach an item?
        console.log("GEOM win=" + tc.Window.window.width + "x" + tc.Window.window.height
            + " tc=" + tc.width + "x" + tc.height
            + " probeScene=" + JSON.stringify(probe.mapToItem(null, 0, 0))
            + " probeWin=" + JSON.stringify(probe.mapToItem(tc.Window.window.contentItem, 0, 0)))
        mouseMove(probe, 32, 16)
        wait(250)
        console.log("PROBE MOVE containsMouse=" + probe.containsMouse + " pressed=" + probe.pressed)
        mousePress(probe, 32, 16, Qt.LeftButton)
        wait(250)
        console.log("PROBE pressed=" + probe.pressed + " containsMouse=" + probe.containsMouse)
        mouseRelease(probe, 32, 16)
        wait(250)

        mouseMove(hov, hov.width / 2, hov.height / 2)
        wait(250)
        console.log("HOVERMOVE hovered=" + hov.hovered + " fill=" + rgba(hov.fill))
        mousePress(hov, hov.width / 2, hov.height / 2, Qt.LeftButton)
        wait(450)
        console.log("PRESS fill=" + rgba(hov.fill) + " hovered=" + hov.hovered)

        foc.forceActiveFocus(Qt.TabFocusReason)
        wait(350)
        console.log("FOCUS activeFocus=" + foc.activeFocus + " visualFocus=" + foc.visualFocus)

        logRects("HOT")
        grab(tc.hotShot)
        tryVerify(function () { return tc.shots >= 2 }, 8000)

        mouseRelease(hov, hov.width / 2, hov.height / 2)
        wait(350)
        console.log("HOVER hovered=" + hov.hovered + " fill=" + rgba(hov.fill))

        // full click: press+release without moving focus in between
        mouseClick(hov, hov.width / 2, hov.height / 2, Qt.LeftButton)
        wait(400)
        console.log("POSTCLICK activeFocus=" + hov.activeFocus)
        console.log("H15 DONE")
    }
}
