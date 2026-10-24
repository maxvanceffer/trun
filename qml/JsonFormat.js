.pragma library

// JSON pretty-printer + syntax highlighter for the R6 response body.
//
// Input is the raw response text; output is a pair of plain strings the panel
// renders with two mono Texts: `gutter` (right-aligned line numbers) and `code`
// (RichText spans). Both keep the same line count and NoWrap, so the gutter
// stays aligned with the code. Colours come from the caller (Theme tokens), so
// this file stays a pure function with no QML import.
//
// `code` is wrapped in a `white-space:pre` span: the panel renders it as
// RichText, and Qt's rich text follows HTML whitespace rules — a run of
// spaces collapses to one and a literal newline to a space, so the
// pretty-printed body would come out as one unindented line. `pre` is part of
// Qt's supported HTML subset ("white-space: normal | pre | nowrap | pre-wrap",
// richtext-html-subset.html) and inherits into the nested colour spans, so
// indentation and line breaks survive as written. Verified against
// QTextDocument (the engine behind the panel's Text): with plain RichText,
// "  \"a\"\n  \"b\"" parses back as `[ "a" "b" ]`; with the pre wrapper the
// two leading spaces and the break are both kept. `gutter` stays PlainText and
// keeps "\n".
//
// token classes -> panel colour map keys:
//   key     object keys            (textPrimary)
//   string  string values          (primary)
//   number  numbers                (info)
//   keyword true / false / null    (error)
//   punct   {}[]:,                 (textSubtle)

function escapeHtml(s) {
    return String(s)
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
}

// Whole-text tokenizer over pretty-printed JSON. Emits {t, s} tokens; `t` is
// "\n", "ws", "str", "num", "kw", "punct" or "plain" (fallback).
function tokenize(s) {
    var out = []
    var i = 0
    var n = s.length
    while (i < n) {
        var c = s.charAt(i)
        if (c === "\n") {
            out.push({ t: "\n" })
            ++i
            continue
        }
        if (c === " " || c === "\t" || c === "\r") {
            var j = i
            while (j < n && (s.charAt(j) === " " || s.charAt(j) === "\t"
                             || s.charAt(j) === "\r"))
                ++j
            out.push({ t: "ws", s: s.slice(i, j) })
            i = j
            continue
        }
        if (c === "\"") {
            var k = i + 1
            while (k < n) {
                if (s.charAt(k) === "\\") { k += 2; continue }
                if (s.charAt(k) === "\"") { ++k; break }
                ++k
            }
            out.push({ t: "str", s: s.slice(i, k) })
            i = k
            continue
        }
        if ("{}[]:,".indexOf(c) >= 0) {
            out.push({ t: "punct", s: c })
            ++i
            continue
        }
        if ((c >= "0" && c <= "9") || c === "-") {
            var m = i
            while (m < n && "0123456789.eE+-".indexOf(s.charAt(m)) >= 0)
                ++m
            out.push({ t: "num", s: s.slice(i, m) })
            i = m
            continue
        }
        var rest = s.substr(i, 5)
        if (rest.indexOf("true") === 0) { out.push({ t: "kw", s: "true" }); i += 4; continue }
        if (rest.indexOf("false") === 0) { out.push({ t: "kw", s: "false" }); i += 5; continue }
        if (rest.indexOf("null") === 0) { out.push({ t: "kw", s: "null" }); i += 4; continue }
        out.push({ t: "plain", s: c })
        ++i
    }
    return out
}

// Mark string tokens that are object keys (next non-whitespace token is ':').
function markKeys(tokens) {
    for (var i = 0; i < tokens.length; ++i) {
        if (tokens[i].t !== "str")
            continue
        var j = i + 1
        while (j < tokens.length
               && (tokens[j].t === "ws" || tokens[j].t === "\n"))
            ++j
        if (j < tokens.length && tokens[j].t === "punct" && tokens[j].s === ":")
            tokens[i].key = true
    }
    return tokens
}

function span(cls, text) {
    return "<span style=\"color:" + cls + "\">" + escapeHtml(text) + "</span>"
}

// render(text, colors) -> { isJson, gutter, code }
//   text   raw body; when it parses as JSON it is re-serialised (2-space
//          indent) before highlighting, otherwise shown as-is (plain).
//   colors { key, string, number, keyword, punct, plain }
function render(text, colors) {
    var raw = String(text === undefined || text === null ? "" : text)
    var isJson = false
    var pretty = raw
    if (raw.trim() !== "") {
        try {
            pretty = JSON.stringify(JSON.parse(raw), null, 2)
            isJson = true
        } catch (e) {
            isJson = false
            pretty = raw
        }
    }

    var tokens = markKeys(tokenize(pretty))
    var gutterLines = []
    var lineNo = 1
    var current = ""
    for (var i = 0; i < tokens.length; ++i) {
        var tok = tokens[i]
        if (tok.t === "\n") {
            gutterLines.push(String(lineNo++))
            current += "\n"
            continue
        }
        if (tok.t === "ws" || tok.t === "plain") {
            current += tok.s
        } else if (tok.t === "str") {
            current += span(tok.key ? colors.key : colors.string, tok.s)
        } else if (tok.t === "num") {
            current += span(colors.number, tok.s)
        } else if (tok.t === "kw") {
            current += span(colors.keyword, tok.s)
        } else {
            current += span(colors.punct, tok.s)
        }
    }
    gutterLines.push(String(lineNo))

    // Pad line numbers to a common width so they right-align inside the mono
    // gutter without an alignment box of its own.
    var width = String(gutterLines.length).length
    for (var g = 0; g < gutterLines.length; ++g) {
        while (gutterLines[g].length < width)
            gutterLines[g] = " " + gutterLines[g]
    }

    // The pre span: the panel's RichText renderer keeps this text as written
    // (see the file header) — indentation and line breaks included.
    return {
        isJson: isJson,
        gutter: gutterLines.join("\n"),
        code: "<span style=\"white-space:pre\">" + current + "</span>",
    }
}
