#!/usr/bin/env bash
# Run the UiButton parity harness (qml/tests/tst_uibutton.qml) together with
# its PIL verifier (qml/tests/verify_uibutton.py).
#
#   cd build && ../qml/tests/run_uibutton.sh          # light theme
#   cd build && ../qml/tests/run_uibutton.sh --dark    # dark theme
#
# Run it from build/ or any scratch directory: the log and the two screenshots
# are written to the current working directory. Needs qmltestrunner on PATH and
# python3 with Pillow.
#
# --dark forces `isDark`, because QStyleHints::setColorScheme cannot be set
# from QML (Qt 6.11). That means patching qml/Theme.qml — which is restored on
# exit, including when the run fails, and the patch is verified to have landed
# so a silent no-op cannot masquerade as a dark run.
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
mode=light
if [ "${1:-}" = "--dark" ]; then mode=dark; fi

if [ "$mode" = "dark" ]; then
    bak="$(mktemp)"
    cp "$repo/qml/Theme.qml" "$bak"
    trap 'cp "$bak" "$repo/qml/Theme.qml"; rm -f "$bak"; echo "Theme.qml restored"' EXIT
    sed -i '' 's/readonly property bool isDark: Application.styleHints.colorScheme === Qt.ColorScheme.Dark/readonly property bool isDark: true/' "$repo/qml/Theme.qml"
    if ! grep -q 'readonly property bool isDark: true' "$repo/qml/Theme.qml"; then
        echo "FATAL: could not force isDark in qml/Theme.qml — not a dark run" >&2
        exit 1
    fi
fi

log="uibutton-$mode.log"
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic \
    qmltestrunner -input "$repo/qml/tests/tst_uibutton.qml" 2>&1 | tee "$log"

python3 "$repo/qml/tests/verify_uibutton.py" "$log" uibutton-base.png uibutton-hot.png
