import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "Ui"

pragma ComponentBehavior: Bound

// Inline parameters panel for framework console commands (Symfony
// bin/console / Laravel artisan) on the detail page, per the design
// spec:
//
//   - Header: PARAMETERS + required badge + description + the global
//     --env / verbosity switches + collapse.
//   - Body: a fields grid (required args, optional args, added
//     value-options) and a flags row with only the ADDED flag chips
//     plus the "+ Add option" popover.
//   - Preview: the exact assembled command line + Copy.
//
// Values are keyed by the BARE arg name ("sql", "env", ...) and
// persisted under runConfig/<projectId>/<commandId> as `argsValues` +
// `addedOptions` — the object ProjectService::assembleConsoleArgs()
// consumes. Secret values (password/token) are deliberately NOT
// persisted; the run path passes them in memory via
// runCommandEffectiveWithValues().
Rectangle {
    id: panel

    // Context objects: absent in standalone/test harnesses. Plain
    // (non-readonly) properties so a harness can inject doubles.
    property var store: (typeof Settings !== "undefined") ? Settings : null

    property string projectId: ""
    property string commandId: ""
    property var command: ({})
    property string commandName: ""
    // Live run state from the detail page: "+ Add option" disables.
    property bool running: false

    // Per-arg values, bare names (string; "on" for flags). Reassigned,
    // never mutated in place, so bindings re-evaluate.
    property var values: ({})
    // Added option names, in the order they were added; persisted.
    property var addedOptions: []

    property bool collapsed: false
    property bool savedFlash: false
    property bool copiedFlash: false

    // ------------------------------------------------------------------
    // Meta helpers
    // ------------------------------------------------------------------

    readonly property var meta: (panel.command
        && panel.command.argsMeta !== undefined
        && panel.command.argsMeta !== null) ? panel.command.argsMeta : []

    readonly property bool hasCommand: panel.meta.length > 0

    // Global Symfony options (from the application definition, not the
    // command's). Excluded from the command's option list entirely:
    // --env and verbosity are header controls, -n is always appended
    // by C++, the rest are not exposed.
    readonly property var globalOptions: [
        "help", "quiet", "verbose", "version", "ansi", "no-ansi",
        "no-interaction", "env", "no-debug", "profile", "silent"
    ]

    // "--shell" and "shell" are one arg: values are keyed bare and
    // ProjectService::lookup() accepts either spelling.
    function bareName(name) {
        var s = (name === undefined || name === null) ? "" : name.toString()
        return s.indexOf("--") === 0 ? s.slice(2) : s
    }

    function storedValue(name) {
        var v = panel.values[panel.bareName(name)]
        return (v === undefined || v === null) ? "" : v.toString()
    }

    function setValue(name, value) {
        var key = panel.bareName(name)
        var next = (value === undefined || value === null) ? "" : value.toString()
        if (panel.storedValue(key) === next)
            return // identity change only on a real change: no loops
        var copy = {}
        for (var k in panel.values)
            copy[k] = panel.values[k]
        copy[key] = next
        panel.values = copy
        panel.savedFlash = false
    }

    function isSecret(name) {
        return /password|secret|token/i.test(panel.bareName(name))
    }

    // ─── Global switches (header): --env and verbosity ──────────────

    readonly property var envOption: panel.rawOptionMeta("env")
    readonly property var verboseOption: panel.rawOptionMeta("verbose")
    readonly property bool hasVerbosity: panel.verboseOption !== null

    function rawOptionMeta(bare) {
        var m = panel.meta
        for (var i = 0; i < m.length; ++i) {
            var e = m[i]
            if (e && e.kind === "option" && panel.bareName(e.name) === bare)
                return e
        }
        return null
    }

    function envDefault() {
        var o = panel.envOption
        if (o && o.default !== undefined && o.default !== null)
            return o.default.toString()
        return ""
    }

    // Env choices = the declared default (Symfony: "dev") + the
    // well-known names + whatever is saved. Without a declared default
    // the first entry is "(command default)" — selecting it stores "",
    // i.e. the bare `--env` C++ appends when nothing is stored (Laravel
    // takes its env from .env, forcing a name there would be wrong).
    property var envEntries: []

    function buildEnvEntries() {
        var out = []
        var seen = {}
        function add(label, value) {
            if (Object.prototype.hasOwnProperty.call(seen, value))
                return
            seen[value] = true
            out.push({ label: label, value: value })
        }
        var def = panel.envDefault()
        if (def === "")
            add(qsTr("(command default)"), "")
        else
            add(def, def)
        var known = ["dev", "test", "prod"]
        for (var i = 0; i < known.length; ++i)
            add(known[i], known[i])
        var saved = panel.storedValue("env")
        if (saved !== "")
            add(saved, saved)
        return out
    }

    function envIndex() {
        var entries = panel.envEntries
        var want = panel.storedValue("env")
        for (var i = 0; i < entries.length; ++i) {
            if (entries[i].value === want)
                return i
        }
        // Nothing stored: the declared default wins, else the bare entry.
        var def = panel.envDefault()
        if (def !== "") {
            for (var j = 0; j < entries.length; ++j) {
                if (entries[j].value === def)
                    return j
            }
        }
        return 0
    }

    // Segmented model: entry labels; the selected index follows the
    // stored env so the switch tracks both user picks and reloads.
    readonly property var envLabels: {
        var out = []
        for (var i = 0; i < panel.envEntries.length; ++i)
            out.push(panel.envEntries[i].label)
        return out
    }
    readonly property int envSelectedIndex: panel.envIndex()

    // Symfony declares the verbose shortcut as "v|-vv|-vvv": the ladder
    // shares the first token, levels come from the repeat count.
    readonly property string verboseShortcut: {
        var s = (panel.verboseOption && panel.verboseOption.shortcut)
            ? panel.verboseOption.shortcut.toString() : "v"
        var first = s.split("|")[0].trim()
        return first !== "" ? first : "v"
    }

    // Ladder labels/values: "" is the normal level; C++ maps "1"/"2"/"3"
    // to -v / -vv / -vvv (see assembleConsoleArgs). The legacy stored
    // "on" reads as the first level.
    readonly property var verbosityLabels: panel.hasVerbosity
        ? [qsTr("normal"), "-" + panel.verboseShortcut,
           "-" + panel.verboseShortcut + panel.verboseShortcut,
           "-" + panel.verboseShortcut + panel.verboseShortcut + panel.verboseShortcut]
        : []
    readonly property var verbosityValues: panel.hasVerbosity ? ["", "1", "2", "3"] : []
    readonly property int verbosityIndex: {
        var v = panel.storedValue("verbose")
        if (v === "on") v = "1" // legacy single-state toggle
        return Math.max(0, panel.verbosityValues.indexOf(v))
    }

    // Positionals, declaration order (args are always in the grid).
    readonly property var positionalFields: {
        var out = []
        var m = panel.meta
        for (var i = 0; i < m.length; ++i) {
            var e = m[i]
            if (!e || e.kind !== "positional" || !e.name)
                continue
            out.push({
                name: panel.bareName(e.name),
                required: e.required === true,
                multiple: e.multiple === true,
                description: (e.description !== undefined && e.description !== null)
                    ? e.description.toString() : "",
                defaultValue: (e.default !== undefined && e.default !== null)
                    ? e.default.toString() : ""
            })
        }
        return out
    }

    // The command's own options (globals stripped, hidden stripped),
    // classified per the spec:
    //   flag  — accept_value: false, takes no value at all (a chip)
    //   value — accept_value: true (an input field); covers both
    //           --name=VALUE and --name[=VALUE] (optional value)
    //   multi — accept_value: true + is_multiple (an input field,
    //           comma-separated values become --name=a --name=b)
    readonly property var optionDefs: {
        var out = []
        var m = panel.meta
        for (var i = 0; i < m.length; ++i) {
            var e = m[i]
            if (!e || e.kind !== "option" || !e.name || e.hidden === true)
                continue
            var bare = panel.bareName(e.name)
            if (panel.globalOptions.indexOf(bare) >= 0)
                continue
            var acceptValue = e.acceptValue === true
            var valueRequired = e.valueRequired === true
            out.push({
                name: bare,
                kind: acceptValue ? (e.multiple === true ? "multi" : "value") : "flag",
                description: (e.description !== undefined && e.description !== null)
                    ? e.description.toString() : "",
                defaultValue: (e.default !== undefined && e.default !== null
                               && e.default.toString() !== "null")
                    ? e.default.toString() : "",
                valueRequired: valueRequired
            })
        }
        return out
    }

    function optionDef(name) {
        var list = panel.optionDefs
        for (var i = 0; i < list.length; ++i)
            if (list[i].name === name)
                return list[i]
        return null
    }

    // Value-required options without a default (e.g. --campaign=CAMPAIGN):
    // the command cannot run sensibly without them, so they are always
    // shown as inputs and have no ✕ — like required positionals.
    function isAutoRequired(name) {
        var def = panel.optionDef(name)
        return def !== null && (def.kind === "value" || def.kind === "multi")
            && def.valueRequired === true && def.defaultValue === ""
    }

    // Options not added yet (the "+ Add option" list). Auto-required ones
    // are always shown, so they are never offered here.
    readonly property var availableOptions: {
        var out = []
        var list = panel.optionDefs
        for (var i = 0; i < list.length; ++i)
            if (panel.addedOptions.indexOf(list[i].name) < 0
                    && !panel.isAutoRequired(list[i].name))
                out.push(list[i])
        return out
    }

    // Added option definitions, in the order they were added.
    readonly property var addedOptionDefs: {
        var out = []
        for (var i = 0; i < panel.addedOptions.length; ++i) {
            var def = panel.optionDef(panel.addedOptions[i])
            if (def)
                out.push(def)
        }
        return out
    }

    // Value options that take effect, grid order: auto-required first,
    // then explicitly added ones.
    function effectiveValueOptions() {
        var out = []
        var defs = panel.optionDefs
        for (var i = 0; i < defs.length; ++i)
            if (panel.isAutoRequired(defs[i].name))
                out.push(defs[i])
        var added = panel.addedOptionDefs
        for (var j = 0; j < added.length; ++j) {
            if (added[j].kind === "flag")
                continue
            var dup = false
            for (var k = 0; k < out.length; ++k)
                if (out[k].name === added[j].name) { dup = true; break }
            if (!dup)
                out.push(added[j])
        }
        return out
    }

    // Grid model: required args -> optional args -> auto-required options
    // -> added value-options (in the order they were added).
    readonly property var gridFields: {
        var req = [], opt = [], auto = [], addedRows = []
        var args = panel.positionalFields
        for (var i = 0; i < args.length; ++i)
            (args[i].required ? req : opt).push({
                name: args[i].name,
                kind: "positional",
                required: args[i].required,
                locked: true,
                multiple: args[i].multiple,
                description: args[i].description,
                defaultValue: args[i].defaultValue
            })
        var defs = panel.optionDefs
        for (var a = 0; a < defs.length; ++a) {
            if (!panel.isAutoRequired(defs[a].name))
                continue
            auto.push({
                name: defs[a].name,
                kind: defs[a].kind,
                required: false,
                locked: true,
                multiple: defs[a].kind === "multi",
                description: defs[a].description,
                defaultValue: defs[a].defaultValue
            })
        }
        var added = panel.addedOptionDefs
        for (var j = 0; j < added.length; ++j) {
            if (added[j].kind === "flag")
                continue
            if (panel.isAutoRequired(added[j].name))
                continue // already shown above
            addedRows.push({
                name: added[j].name,
                kind: added[j].kind,
                required: false,
                locked: false,
                multiple: added[j].kind === "multi",
                description: added[j].description,
                defaultValue: added[j].defaultValue
            })
        }
        return req.concat(opt).concat(auto).concat(addedRows)
    }

    // Added flag chips.
    readonly property var addedFlags: {
        var out = []
        var added = panel.addedOptionDefs
        for (var i = 0; i < added.length; ++i)
            if (added[i].kind === "flag")
                out.push(added[i])
        return out
    }

    // The gate C++ applies in runCommandEffective(): a required
    // positional with no value blocks the launch.
    readonly property var missingRequired: {
        var out = []
        var f = panel.positionalFields
        for (var i = 0; i < f.length; ++i) {
            if (f[i].required && panel.storedValue(f[i].name) === "")
                out.push(f[i].name)
        }
        return out
    }
    readonly property bool canRun: panel.projectId !== ""
        && panel.commandId !== "" && panel.missingRequired.length === 0

    // ------------------------------------------------------------------
    // Add / remove options
    // ------------------------------------------------------------------

    function addOption(name) {
        if (panel.addedOptions.indexOf(name) >= 0)
            return
        if (panel.isAutoRequired(name))
            return // always shown; nothing to add
        var next = panel.addedOptions.slice()
        next.push(name)
        panel.addedOptions = next
        var def = panel.optionDef(name)
        // Flags switch on immediately; value fields prefill the declared
        // default ([=VALUE] / [default: X]) when one exists, so the
        // preview and the run show what Symfony will actually use.
        // Secrets never prefill.
        if (def && def.kind === "flag")
            panel.setValue(name, "on")
        else if (def && (def.kind === "value" || def.kind === "multi")
                 && panel.storedValue(name) === ""
                 && def.defaultValue !== "" && !panel.isSecret(name))
            panel.setValue(name, def.defaultValue)
        panel.persistValues()
    }

    function removeOption(name) {
        var next = panel.addedOptions.slice()
        var i = next.indexOf(name)
        if (i >= 0)
            next.splice(i, 1)
        panel.addedOptions = next
        // Removing an option wipes its value.
        panel.setValue(name, "")
        panel.persistValues()
    }

    // ------------------------------------------------------------------
    // Preview: a mirror of ProjectService::assembleConsoleArgs()
    // ------------------------------------------------------------------

    // The executable the record carries — the exact value C++ hands to
    // runResolved(); no hardcoded fallback, an empty record renders empty.
    readonly property string binaryLabel: (panel.command && panel.command.command)
        ? panel.command.command.toString() : ""

    // Shell-quote for the preview: bare tokens stay bare, everything
    // with spaces or specials gets single-quoted (POSIX rules).
    function shellQuote(v) {
        if (/^[A-Za-z0-9_@%+=:,./-]+$/.test(v))
            return v
        return "'" + v.replace(/'/g, "'\\''") + "'"
    }

    function assembledArgs() {
        var m = panel.meta
        var out = []
        var i, e, bare, value
        // Positionals, declaration order: filled values only.
        for (i = 0; i < m.length; ++i) {
            e = m[i]
            if (!e || e.kind !== "positional")
                continue
            value = panel.storedValue(e.name)
            if (value !== "")
                out.push(panel.shellQuote(value))
        }
        // Effective value options (auto-required first, then added): a
        // value equal to the default still passes (the option shows
        // explicitly). Secrets collapse to ••• in the display.
        var added = panel.effectiveValueOptions()
        for (i = 0; i < added.length; ++i) {
            e = added[i]
            bare = e.name
            value = panel.storedValue(bare)
            if (value === "")
                continue
            var display = panel.isSecret(bare) ? "•••" : panel.shellQuote(value)
            if (e.kind === "flag") {
                out.push("--" + bare)
                continue
            }
            if (e.kind === "multi" && /\s+/.test(value)) {
                var parts = value.split(/\s+/)
                for (var p = 0; p < parts.length; ++p)
                    if (parts[p] !== "")
                        out.push("--" + bare + "=" + panel.shellQuote(parts[p]))
                continue
            }
            out.push("--" + bare + "=" + display)
        }
        // Global verbosity ladder (header control): mirrors the C++
        // mapping ("1"/"2"/"3" to -v/-vv/-vvv, see assembleConsoleArgs).
        var verbosity = panel.storedValue("verbose")
        if (verbosity === "on") verbosity = "1" // legacy single-state toggle
        if (verbosity !== "") {
            var ladder = panel.verboseShortcut
            out.push("-" + ladder
                + (verbosity === "2" || verbosity === "3" ? ladder : "")
                + (verbosity === "3" ? ladder : ""))
        }
        // Trailer: --env (bare when unfilled) then -n, always last.
        var env = panel.storedValue("env")
        out.push("--env")
        if (env !== "")
            out.push(env)
        out.push("-n")
        return out
    }

    // The actual launch goes through the interpreter
    // (`php bin/console <name> ...`, see runCommandEffectiveImpl), so the
    // preview names it explicitly for console records.
    readonly property string preview: (panel.hasCommand ? "php " : "") + panel.binaryLabel
        + (panel.commandName !== "" ? " " + panel.commandName : "")
        + (panel.assembledArgs().length > 0 ? " " + panel.assembledArgs().join(" ") : "")

    // ------------------------------------------------------------------
    // Storage
    // ------------------------------------------------------------------

    function configKey() {
        return "runConfig/" + panel.projectId + "/" + panel.commandId
    }

    function readSaved() {
        var out = { values: {}, added: [] }
        if (!panel.store || panel.projectId === "" || panel.commandId === "")
            return out
        var raw = panel.store.get(panel.configKey(), "")
        if (raw === "")
            return out
        var cfg = {}
        try {
            cfg = JSON.parse(raw.toString())
        } catch (e) {
            return out
        }
        var saved = (cfg && cfg.argsValues && typeof cfg.argsValues === "object")
            ? cfg.argsValues : {}
        for (var k in saved) {
            out.values[panel.bareName(k)] = (saved[k] === undefined || saved[k] === null)
                ? "" : saved[k].toString()
        }
        if (cfg && cfg.addedOptions && cfg.addedOptions.length !== undefined)
            out.added = cfg.addedOptions.slice()
        return out
    }

    // Merge into whatever is stored: RunConfigDialog owns executable /
    // workdir / argsText in the same object. Secrets are NOT persisted
    // (the run path passes them in memory). Empties are dropped — a
    // stored "" blocks a required positional in C++ exactly like a
    // missing one.
    function persistValues() {
        if (!panel.store || panel.projectId === "" || panel.commandId === "")
            return false
        var key = panel.configKey()
        var raw = panel.store.get(key, "")
        var cfg = {}
        if (raw !== "") {
            try {
                cfg = JSON.parse(raw.toString())
            } catch (e) {
                cfg = {}
            }
            if (!cfg || typeof cfg !== "object")
                cfg = {}
        }
        var merged = {}
        var prev = (cfg.argsValues && typeof cfg.argsValues === "object")
            ? cfg.argsValues : {}
        for (var k in prev)
            merged[panel.bareName(k)] = prev[k]

        // Allowed keys: positionals + auto-required options + added
        // options + the env/verbose globals. Everything else (removed
        // options, stale values) is pruned so only explicitly added
        // options pass through.
        var allowed = {}
        var args = panel.positionalFields
        for (var i = 0; i < args.length; ++i)
            allowed[args[i].name] = true
        var defs = panel.optionDefs
        for (var d = 0; d < defs.length; ++d)
            if (panel.isAutoRequired(defs[d].name))
                allowed[defs[d].name] = true
        for (var j = 0; j < panel.addedOptions.length; ++j)
            allowed[panel.addedOptions[j]] = true
        allowed["env"] = true
        allowed["verbose"] = true

        for (var name in allowed) {
            var v = panel.storedValue(name)
            if (v === "" || panel.isSecret(name))
                delete merged[name]
            else
                merged[name] = v
        }
        // Stale keys that are no longer allowed disappear.
        for (var stale in merged)
            if (!(stale in allowed))
                delete merged[stale]

        cfg.argsValues = merged
        cfg.addedOptions = panel.addedOptions.slice()
        panel.store.set(key, JSON.stringify(cfg))
        return true
    }

    function savePreset() {
        if (!panel.persistValues())
            return
        panel.savedFlash = true
        presetTimer.restart()
    }

    // Everything the panel needs for a command: values from the stored
    // preset, the env choices, fresh field delegates.
    function reload() {
        var saved = panel.readSaved()
        var vals = saved.values
        // Flag-era residue: VALUE_OPTIONAL options once rendered as flag
        // chips storing "on". A literal "on" is never a real input value:
        // fall back to the declared default (or empty when there is
        // none). Globals are untouched ("on" is the legacy verbose level).
        for (var k in vals) {
            if (vals[k] !== "on")
                continue
            var def = panel.optionDef(k)
            if (def && (def.kind === "value" || def.kind === "multi"))
                vals[k] = def.defaultValue
        }
        panel.values = vals
        panel.addedOptions = saved.added
        panel.envEntries = panel.buildEnvEntries()
        // The env switch always has a selection; store it so the
        // preview and the run agree with what the switch shows.
        var envIdx = panel.envIndex()
        if (envIdx >= 0 && envIdx < panel.envEntries.length)
            panel.setValue("env", panel.envEntries[envIdx].value)
        panel.formToken++
        panel.savedFlash = false
        panel.copiedFlash = false
    }

    property int formToken: 0

    // qmllint disable missing-property
    function focusFirstRequired() {
        for (var i = 0; i < fieldsGrid.count; ++i) {
            var item = fieldsGrid.itemAt(i)
            if (item && item.fieldRequired && item.fieldEmpty) {
                item.focusInput()
                return true
            }
        }
        return false
    }
    // qmllint enable missing-property

    Timer {
        id: presetTimer
        interval: 1800
        onTriggered: panel.savedFlash = false
    }

    Timer {
        id: copiedTimer
        interval: 1200
        onTriggered: panel.copiedFlash = false
    }

    // ------------------------------------------------------------------
    // Field input (exact spec styling; UiInput is a different size)
    // ------------------------------------------------------------------

    component FieldInput: TextField {
        id: fieldInputRoot

        property bool error: false

        implicitHeight: 30
        color: Theme.textPrimary
        placeholderTextColor: Theme.textSubtle
        selectByMouse: true
        font.family: "Menlo"
        font.pixelSize: Theme.fontSizeMd            // 12
        leftPadding: 10
        rightPadding: 10
        topPadding: 0
        bottomPadding: 0
        verticalAlignment: TextInput.AlignVCenter

        background: Rectangle {
            radius: 6
            color: Theme.windowBackground
            border.color: fieldInputRoot.error
                ? Theme.error
                : (fieldInputRoot.activeFocus ? Theme.ring : Theme.border)
            border.width: 1
        }

        // Focus ring: 2px Theme.ring at 25% (design: not the fat yellow).
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: 9
            color: "transparent"
            border.width: 2
            border.color: Theme.ring
            opacity: fieldInputRoot.activeFocus ? 0.25 : 0
        }
    }

    // ------------------------------------------------------------------
    // Panel
    // ------------------------------------------------------------------

    // Expanded: content column + gap 4 + preview; collapsed: header only.
    // The bottom 8px padding; the top 8px lives in the header's margin.
    implicitHeight: (panel.collapsed || !panel.hasCommand)
        ? body.implicitHeight + 8
        : body.implicitHeight + 5 + previewRow.height
    color: Theme.cardBackground
    border.color: Theme.border
    border.width: 1
    radius: 8

    ColumnLayout {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 0

        // ─── Header (min 44) ─────────────────────────────────────────
        RowLayout {
            spacing: 10
            Layout.fillWidth: true
            Layout.minimumHeight: 44
            Layout.topMargin: 8
            Layout.leftMargin: 14
            Layout.rightMargin: 8

            Label {
                text: qsTr("PARAMETERS")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeSm            // 11
                font.bold: true
                font.letterSpacing: 1
                Layout.alignment: Qt.AlignVCenter
            }

            // Badge "N required": height 18, pill, primary outline.
            Rectangle {
                visible: panel.missingRequired.length > 0
                Layout.alignment: Qt.AlignVCenter
                implicitHeight: 18
                implicitWidth: reqBadgeText.implicitWidth + 14
                radius: 9
                color: "transparent"
                border.width: 1
                border.color: Qt.rgba(Theme.primary.r, Theme.primary.g,
                                      Theme.primary.b, 0.5)

                Label {
                    id: reqBadgeText
                    anchors.centerIn: parent
                    text: qsTr("%1 required").arg(panel.missingRequired.length)
                    color: Theme.primary
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            // Expanded: the description (one line). Collapsed: the
            // command line preview (mono), per the design.
            Label {
                visible: !panel.collapsed
                text: (panel.command && panel.command.description)
                    ? panel.command.description : ""
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeMd            // 12
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
            }

            Label {
                visible: panel.collapsed
                text: "$ " + panel.preview
                color: Theme.textMuted
                font.family: "Menlo"
                font.pixelSize: Theme.fontSizeSm
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
            }

            Label {
                visible: panel.savedFlash
                text: qsTr("Preset saved")
                color: Theme.success
                font.pixelSize: Theme.fontSizeSm
                Layout.alignment: Qt.AlignVCenter
            }

            // Global --env (dev/test/prod), header control. Hidden when
            // collapsed: the preview line already carries the values.
            UiLabeledSegmented {
                visible: panel.envOption !== null && !panel.collapsed
                label: "--env"
                model: panel.envLabels
                currentIndex: panel.envSelectedIndex
                Layout.alignment: Qt.AlignVCenter
                onActivated: function (index) {
                    var entry = panel.envEntries[index]
                    if (entry)
                        panel.setValue("env", entry.value)
                }
            }

            // Global verbosity ladder: normal / -v / -vv / -vvv (Menlo —
            // proportional font renders the hyphens as dashes). Hidden
            // when collapsed, like --env.
            UiSegmented {
                visible: panel.hasVerbosity && !panel.collapsed
                fontFamily: "Menlo"
                model: panel.verbosityLabels
                currentIndex: panel.verbosityIndex
                Layout.alignment: Qt.AlignVCenter
                onActivated: function (index) {
                    panel.setValue("verbose", panel.verbosityValues[index])
                }
            }

            // Collapse: ghost icon button with a chevron.
            IconButton {
                iconSource: Theme.icon(panel.collapsed ? "chevron-down" : "chevron-up")
                tooltipText: panel.collapsed ? qsTr("Expand") : qsTr("Collapse")
                Layout.alignment: Qt.AlignVCenter
                onClicked: panel.collapsed = !panel.collapsed
            }
        }

        // Section line: header | body
        Rectangle {
            visible: !panel.collapsed
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.border
        }

        // ─── Body ────────────────────────────────────────────────────
        ColumnLayout {
            visible: !panel.collapsed
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.bottomMargin: 12
            spacing: 12

            // Fields grid: required args -> optional args -> added
            // value-options, ~250px columns.
            GridLayout {
                visible: panel.gridFields.length > 0
                Layout.fillWidth: true
                columns: Math.max(1,
                    Math.floor((body.width - 28) / 262))
                columnSpacing: 12
                rowSpacing: 10

                Repeater {
                    id: fieldsGrid
                    model: panel.gridFields

                    delegate: ColumnLayout {
                        id: fieldCell

                        required property var modelData
                        readonly property string fieldName: modelData.name
                        readonly property bool fieldRequired: modelData.required
                        readonly property bool fieldEmpty:
                            panel.storedValue(fieldCell.fieldName) === ""
                        readonly property bool fieldError:
                            fieldCell.fieldRequired && fieldCell.fieldEmpty

                        Layout.fillWidth: true
                        Layout.minimumWidth: 250
                        spacing: 5

                        function focusInput() {
                            if (fieldCell.inputItem)
                                fieldCell.inputItem.forceActiveFocus()
                        }
                        // Exposed for focusFirstRequired().
                        readonly property alias inputItem: cellInput

                        // 1. Name row: name · tag · stretch · ✕ (options only)
                        RowLayout {
                            spacing: 6
                            Layout.fillWidth: true
                            Layout.preferredHeight: 16

                            Label {
                                text: fieldCell.modelData.kind === "positional"
                                    ? fieldCell.fieldName
                                    : "--" + fieldCell.fieldName
                                color: Theme.textPrimary
                                font.family: "Menlo"
                                font.pixelSize: Theme.fontSizeMd            // 12
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Label {
                                text: fieldCell.modelData.kind === "multi"
                                    ? "REPEATABLE"
                                    : fieldCell.modelData.required ? "REQUIRED"
                                    : fieldCell.modelData.kind === "positional" ? "OPTIONAL"
                                    : "OPTION"
                                color: fieldCell.modelData.required
                                    ? Theme.primary : Theme.textSubtle
                                font.pixelSize: 10
                                font.bold: true
                                Layout.alignment: Qt.AlignVCenter
                            }

                            // Remove button: options only — never for
                            // positionals or auto-required (locked) rows.
                            AbstractButton {
                                id: removeOptBtn
                                visible: fieldCell.modelData.locked !== true
                                Layout.alignment: Qt.AlignVCenter
                                implicitWidth: 16
                                implicitHeight: 16
                                hoverEnabled: true

                                background: Rectangle {
                                    radius: 8
                                    color: removeOptBtn.hovered ? Theme.mutedSurface : "transparent"
                                }
                                contentItem: Label {
                                    text: "✕"
                                    color: Theme.textSubtle
                                    font.pixelSize: 10
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: panel.removeOption(fieldCell.fieldName)
                            }
                        }

                        // 2. Input: 30px, Menlo, bg windowBackground.
                        // Secrets masked; placeholder = default (never
                        // the description).
                        FieldInput {
                            id: cellInput
                            Layout.fillWidth: true
                            text: panel.storedValue(fieldCell.fieldName)
                            error: fieldCell.fieldError
                            echoMode: panel.isSecret(fieldCell.fieldName)
                                ? TextInput.Password : TextInput.Normal
                            placeholderText: fieldCell.modelData.defaultValue
                            onTextChanged:
                                panel.setValue(fieldCell.fieldName, text)
                        }

                        // 3. Hint: description once; "Required" on error;
                        // repeatable/array suffixes.
                        Label {
                            text: fieldCell.fieldError
                                ? qsTr("Required")
                                : (fieldCell.modelData.description
                                   + (fieldCell.modelData.kind === "multi"
                                      ? qsTr(" · comma-separated") : "")
                                   + (fieldCell.modelData.multiple
                                      && fieldCell.modelData.kind === "positional"
                                      ? qsTr(" · space-separated") : ""))
                            color: fieldCell.fieldError
                                ? Theme.error : Theme.textSubtle
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            // Flags row: added flag chips + the "+ Add option" button.
            // Absent entirely when the command defines no options.
            Flow {
                visible: panel.optionDefs.length > 0
                Layout.fillWidth: true
                spacing: 6

                Repeater {
                    model: panel.addedFlags

                    delegate: Rectangle {
                        id: flagChip
                        required property var modelData

                        // Row margins (10 left + 4 right) are outside the
                        // layout's implicitWidth: without them the row
                        // squeezes 14px and the ✕ sticks out of the pill.
                        width: flagRow.implicitWidth + 14
                        height: 24
                        radius: 12
                        color: Qt.rgba(Theme.primary.r, Theme.primary.g,
                                       Theme.primary.b, 0.1)
                        border.width: 1
                        border.color: Qt.rgba(Theme.primary.r, Theme.primary.g,
                                              Theme.primary.b, 0.55)
                        UiTooltip {
                            text: flagChip.modelData.description
                            visible: flagMouse.containsMouse && text !== ""
                        }

                        MouseArea {
                            id: flagMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                        }

                        RowLayout {
                            id: flagRow
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 4
                            spacing: 4

                            Label {
                                text: "--" + flagChip.modelData.name
                                color: Theme.primary
                                font.family: "Menlo"
                                font.pixelSize: Theme.fontSizeSm    // 11
                                Layout.alignment: Qt.AlignVCenter
                            }

                            // ✕ in a 16x16 circle, hover fill.
                            AbstractButton {
                                id: flagRemoveBtn
                                implicitWidth: 16
                                implicitHeight: 16
                                hoverEnabled: true

                                background: Rectangle {
                                    radius: 8
                                    color: flagRemoveBtn.hovered
                                        ? Qt.rgba(Theme.primary.r, Theme.primary.g,
                                                  Theme.primary.b, 0.2) : "transparent"
                                }
                                contentItem: Label {
                                    text: "✕"
                                    color: Theme.primary
                                    font.pixelSize: 9
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: panel.removeOption(flagChip.modelData.name)
                            }
                        }
                    }
                }

                // "+ Add option": dashed pill with the count of the
                // still-unadded options.
                AbstractButton {
                    id: addOptionButton
                    visible: panel.availableOptions.length > 0
                    implicitHeight: 24
                    implicitWidth: addRow.implicitWidth + 20
                    hoverEnabled: true
                    enabled: !panel.running
                    leftPadding: 10
                    rightPadding: 10
                    topPadding: 0
                    bottomPadding: 0

                    background: Item {
                        // Hover fill UNDER the dashed border (a child of
                        // the Canvas would paint over its dashes).
                        Rectangle {
                            anchors.fill: parent
                            radius: parent.height / 2
                            color: addOptionButton.hovered ? Theme.mutedSurface : "transparent"
                        }

                        Canvas {
                            id: dashedBorder
                            anchors.fill: parent
                            onPaint: {
                                var ctx = dashedBorder.getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                ctx.setLineDash([3, 3])
                                ctx.lineWidth = 1
                                ctx.strokeStyle = Theme.textMuted
                                var r = height / 2
                                ctx.beginPath()
                                ctx.moveTo(width - r, 0.5)
                                ctx.lineTo(r, 0.5)
                                ctx.arc(r, r, r - 0.5, Math.PI * 1.5, Math.PI * 0.5, true)
                                ctx.lineTo(width - r, height - 0.5)
                                ctx.arc(width - r, r, r - 0.5, Math.PI * 0.5, Math.PI * 1.5, true)
                                ctx.stroke()
                            }
                        }
                    }

                    contentItem: RowLayout {
                        id: addRow
                        spacing: 4

                        Label {
                            text: qsTr("+ Add option")
                            color: Theme.textPrimary
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Label {
                            text: String(panel.availableOptions.length)
                            color: Theme.textSubtle
                            font.pixelSize: Theme.fontSizeSm
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }

                    onClicked: addOptionPopup.openAt()
                }
            }
        }

        // (No line below the body: the preview's windowBackground plate
        // already separates the sections.)

        // ─── Preview is anchored to the panel itself (1px inset), see
        // previewRow below.
    }

    // ─── Preview (min 36, windowBackground, 1px inset from the card) ──
    Rectangle {
        id: previewRow
        visible: !panel.collapsed && panel.hasCommand
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 1
        anchors.rightMargin: 1
        anchors.bottomMargin: 1
        height: Math.max(36, previewText.height + 12)
        color: Theme.windowBackground
        radius: 8
        // Square the top corners: only the bottom pair rounds into
        // the panel's corner.
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 8
            color: parent.color
        }

        RowLayout {
            anchors.fill: parent
            anchors.topMargin: 6
            anchors.bottomMargin: 6
            anchors.leftMargin: 14
            anchors.rightMargin: 8
            spacing: 8

            Text {
                id: previewText
                Layout.fillWidth: true
                text: "<span style=\"color:" + Theme.primary + "\">$ </span>"
                      + panel.preview
                textFormat: Text.RichText
                color: Theme.textMuted
                font.family: "Menlo"
                font.pixelSize: 12
                wrapMode: Text.WrapAnywhere
            }

            // Copy: icon-only button (IconButton); flashes to a
            // checkmark for 1.2s after copying.
            IconButton {
                id: copyBtn
                iconSource: panel.copiedFlash
                    ? Theme.icon("check") : Theme.icon("clipboard")
                tooltipText: qsTr("Copy")
                Layout.alignment: Qt.AlignVCenter
                onClicked: {
                    if ((typeof databaseService !== "undefined") && databaseService)
                        databaseService.copyToClipboard(panel.preview)
                    panel.copiedFlash = true
                    copiedTimer.restart()
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // "+ Add option" popover
    // ------------------------------------------------------------------

    function optionTypeLabel(kind) {
        return kind === "flag" ? "flag" : (kind === "multi" ? "value…" : "value")
    }

    Popup {
        id: addOptionPopup
        parent: addOptionButton
        x: 0
        y: addOptionButton.height + 6
        width: 400
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 0

        // Smooth enter/exit: the plate (and its shadow with it) fades
        // in and out instead of popping.
        enter: Transition {
            NumberAnimation {
                property: "opacity"; from: 0; to: 1
                duration: Theme.animFast; easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "y"; from: addOptionButton.height - 4
                duration: Theme.animFast; easing.type: Easing.OutCubic
            }
        }
        exit: Transition {
            NumberAnimation {
                property: "opacity"; from: 1; to: 0
                duration: Theme.animHover; easing.type: Easing.OutCubic
            }
        }

        function openAt() {
            searchField.text = ""
            highlightIndex = -1
            open()
            searchField.forceActiveFocus()
        }

        property int highlightIndex: -1

        // Filtered available options: by name (leading dashes ignored)
        // and by description.
        readonly property var filteredOptions: {
            var q = searchField.text.toLowerCase().replace(/^-+/, "")
            var out = []
            var list = panel.availableOptions
            for (var i = 0; i < list.length; ++i) {
                if (q === ""
                    || list[i].name.toLowerCase().indexOf(q) >= 0
                    || list[i].description.toLowerCase().indexOf(q) >= 0)
                    out.push(list[i])
            }
            return out
        }

        background: Item {
            implicitWidth: 400
            implicitHeight: popupBody.implicitHeight

            Rectangle {
                id: popupBg
                anchors.fill: parent
                radius: 8
                color: Theme.cardBackground
                border.width: 1
                border.color: Qt.rgba(Theme.ring.r, Theme.ring.g,
                                      Theme.ring.b, 0.35)
                visible: false
            }

            // Shadow under the popup plate.
            MultiEffect {
                anchors.fill: popupBg
                anchors.margins: -30
                source: popupBg
                shadowEnabled: true
                shadowBlur: 0.8
                shadowVerticalOffset: 18
                shadowOpacity: 0.55
                autoPaddingEnabled: false
            }
        }

        contentItem: ColumnLayout {
            id: popupBody
            spacing: 0

            // Search field, focused on open.
            TextField {
                id: searchField
                Layout.fillWidth: true
                Layout.margins: 8
                implicitHeight: 30
                color: Theme.textPrimary
                placeholderText: qsTr("Search options…")
                placeholderTextColor: Theme.textSubtle
                font.family: "Menlo"
                font.pixelSize: Theme.fontSizeMd                // 12
                selectByMouse: true
                leftPadding: 10
                rightPadding: 10
                topPadding: 0
                bottomPadding: 0
                verticalAlignment: TextInput.AlignVCenter

                background: Rectangle {
                    radius: 6
                    color: Theme.windowBackground
                    border.width: 1
                    border.color: searchField.activeFocus ? Theme.ring : Theme.border
                }

                Keys.onUpPressed: addOptionPopup.highlightIndex =
                    Math.max(0, addOptionPopup.highlightIndex - 1)
                Keys.onDownPressed: addOptionPopup.highlightIndex =
                    Math.min(addOptionPopup.filteredOptions.length - 1,
                             addOptionPopup.highlightIndex + 1)
                Keys.onReturnPressed: addOptionPopup.toggleHighlighted()
                Keys.onEnterPressed: addOptionPopup.toggleHighlighted()
            }

            // Divider between the search and the list.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
            }

            ListView {
                id: optionList
                Layout.fillWidth: true
                Layout.maximumHeight: 300
                implicitHeight: Math.min(contentHeight, 300)
                clip: true
                model: addOptionPopup.filteredOptions

                delegate: Item {
                    id: optionRow
                    required property int index
                    required property var modelData
                    readonly property bool added:
                        panel.addedOptions.indexOf(optionRow.modelData.name) >= 0

                    width: optionList.width - 8
                    x: 4
                    height: optionRowLayout.implicitHeight + 14

                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        color: optionRow.added
                            ? Qt.rgba(Theme.primary.r, Theme.primary.g,
                                      Theme.primary.b, 0.06)
                            : (optionRow.index === addOptionPopup.highlightIndex
                               || optionMouse.containsMouse
                                ? Theme.mutedSurface : "transparent")
                    }

                    MouseArea {
                        id: optionMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: addOptionPopup.toggleOption(optionRow.modelData.name)
                        onContainsMouseChanged: {
                            if (containsMouse)
                                addOptionPopup.highlightIndex = optionRow.index
                        }
                    }

                    ColumnLayout {
                        id: optionRowLayout
                        anchors.fill: parent
                        anchors.margins: 7
                        spacing: 2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Label {
                                text: optionRow.added ? "✓" : "+"
                                color: optionRow.added ? Theme.primary : Theme.textSubtle
                                font.pixelSize: Theme.fontSizeMd
                                Layout.preferredWidth: 18
                                Layout.alignment: Qt.AlignVCenter
                            }

                            Label {
                                text: optionRow.modelData.name
                                color: Theme.textPrimary
                                font.family: "Menlo"
                                font.pixelSize: Theme.fontSizeMd        // 12
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                            }

                            Label {
                                text: panel.optionTypeLabel(optionRow.modelData.kind)
                                color: Theme.textSubtle
                                font.family: "Menlo"
                                font.pixelSize: 10
                                Layout.alignment: Qt.AlignVCenter
                            }
                        }

                        Label {
                            visible: optionRow.modelData.description !== ""
                            text: optionRow.modelData.description
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSm            // 11
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            Layout.leftMargin: 18
                        }
                    }
                }

                // Empty state.
                Label {
                    anchors.centerIn: parent
                    visible: addOptionPopup.filteredOptions.length === 0
                    text: qsTr("No options match")
                    color: Theme.textSubtle
                    font.pixelSize: Theme.fontSizeMd                // 12
                }
            }

            // Footer.
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 0
                Layout.topMargin: 7
                Layout.bottomMargin: 7
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                spacing: 4

                Label {
                    text: qsTr("↩ add · Esc close")
                    color: Theme.textSubtle
                    font.pixelSize: 11
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: qsTr("global options are in the header")
                    color: Theme.textSubtle
                    font.pixelSize: 11
                }
            }
        }

        // Click toggles: an unadded option is added (flags switch on,
        // value fields appear with focus); an added one is removed.
        // The popup stays open for multi-add.
        function toggleOption(name) {
            if (panel.addedOptions.indexOf(name) >= 0) {
                panel.removeOption(name)
                return
            }
            panel.addOption(name)
            var def = panel.optionDef(name)
            if (def && def.kind !== "flag") {
                // Focus the freshly added field: it is the last
                // value-option in the grid.
                // qmllint disable missing-property
                Qt.callLater(function () {
                    for (var i = 0; i < fieldsGrid.count; ++i) {
                        var cell = fieldsGrid.itemAt(i)
                        if (cell && cell.fieldName === name) {
                            cell.focusInput()
                            return
                        }
                    }
                })
                // qmllint enable missing-property
            }
        }

        function toggleHighlighted() {
            var list = addOptionPopup.filteredOptions
            if (list.length === 0)
                return
            if (addOptionPopup.highlightIndex < 0
                    || addOptionPopup.highlightIndex >= list.length)
                addOptionPopup.highlightIndex = 0
            addOptionPopup.toggleOption(list[addOptionPopup.highlightIndex].name)
        }
    }
}
