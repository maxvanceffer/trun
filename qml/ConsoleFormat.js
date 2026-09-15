.pragma library

// Minimal SGR (ANSI color) → HTML for the Qt RichText console.
// Handles: reset (0 / empty), bold (1 / 22), fg 30-37 / 90-97,
// bg 40-47 / 100-107, default fg/bg (39 / 49). Unknown codes are ignored.

var FG = {
    30: "#9e9e9e", 31: "#ef4444", 32: "#22c55e", 33: "#eab308",
    34: "#60a5fa", 35: "#c084fc", 36: "#22d3ee", 37: "#e5e5e5",
    90: "#737373", 91: "#f87171", 92: "#4ade80", 93: "#facc15",
    94: "#93c5fd", 95: "#d8b4fe", 96: "#67e8f9", 97: "#ffffff"
}

var BG = {
    40: "#3f3f3f", 41: "#7f1d1d", 42: "#14532d", 43: "#713f12",
    44: "#1e3a8a", 45: "#581c87", 46: "#155e75", 47: "#e5e5e5",
    100: "#525252", 101: "#991b1b", 102: "#166534", 103: "#854d0e",
    104: "#1e40af", 105: "#6b21a8", 106: "#0e7490", 107: "#f5f5f5"
}

function escapeHtml(s) {
    return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
}

function currentSpan(fg, bg, bold) {
    if (fg === null && bg === null && !bold)
        return ""
    var st = ""
    if (fg !== null) st += "color:" + fg + ";"
    if (bg !== null) st += "background-color:" + bg + ";"
    if (bold) st += "font-weight:bold;"
    return '<span style="' + st + '">'
}

function ansiToHtml(input) {
    var out = ""
    var re = /\x1b\[([0-9;]*)m/g
    var last = 0
    var m = null
    var fg = null, bg = null, bold = false
    var open = false
    while ((m = re.exec(input)) !== null) {
        out += escapeHtml(input.substring(last, m.index).replace(/\x1b/g, ""))
        var parts = m[1] === "" ? [0] : m[1].split(";")
        for (var i = 0; i < parts.length; ++i) {
            var code = parseInt(parts[i], 10)
            if (isNaN(code)) continue
            if (code === 0) { fg = null; bg = null; bold = false }
            else if (code === 1) bold = true
            else if (code === 22) bold = false
            else if (code === 39) fg = null
            else if (code === 49) bg = null
            else if (FG[code] !== undefined) fg = FG[code]
            else if (BG[code] !== undefined) bg = BG[code]
        }
        last = m.index + m[0].length
        if (open) { out += "</span>"; open = false }
        var sp = currentSpan(fg, bg, bold)
        if (sp !== "") { out += sp; open = true }
    }
    out += escapeHtml(input.substring(last).replace(/\x1b/g, ""))
    if (open) out += "</span>"
    return out
}

// Full console row: "[timestamp] target: message" with the prefix in the
// level color and the message in its own ANSI colors. One zero-margin div
// per row: plain <br> rows drift apart once the document is re-parsed.
function formatLine(timestamp, target, message, prefixColor) {
    return '<div style="margin-top:0px; margin-bottom:0px;">'
        + '<span style="color:' + prefixColor + '">[' + escapeHtml(timestamp)
        + '] ' + escapeHtml(target) + ':</span> ' + ansiToHtml(message) + '</div>'
}
