.pragma library

// Tool plugins: per-tool log patterns for busy-port detection and port
// sniffing (vite, symfony, ...).
//
// Generic patterns ALWAYS apply as a safety net. A tool plugin only ADDS
// its own triggers, and only for commands it owns:
//   programs  — detectedProgram values it handles (ProjectService::detectProgram)
//   manifests — manifest files it applies to; empty = program match alone decides
//
// Why manifests: vite's "Port N is in use, trying another one..." only makes
// sense for package.json commands — a composer.json/Cargo.toml command is
// never vite's dev server, so vite's triggers must not fire there. (Hard
// failures like EADDRINUSE are still caught by the generic patterns.)

var GENERIC_BUSY = [
    { re: /EADDRINUSE/i, group: 0 },
    { re: /address already in use/i, group: 0 },
    { re: /already running/i, group: 0 }
]

var GENERIC_LISTENING = [
    /https?:\/\/(?:localhost|127\.0\.0\.1|\[::1\]):(\d+)/i,
    /listening on\s+(?:https?:\/\/)?(?:localhost|127\.0\.0\.1|0\.0\.0\.0|\[::1\]):(\d+)/i,
    /listening on\s+[a-z ]*port\s+(\d+)/i
]

var PLUGINS = [
    { id: "vite", programs: ["vite"], manifests: ["package.json"],
      hint: "Port busy, vite hops to another port",
      busy: [{ re: /Port (\d+) is in use/i, group: 1 }],
      listening: [] },
    { id: "symfony", programs: ["symfony", "php"], manifests: ["composer.json"],
      hint: "",
      busy: [],
      listening: [] },
    { id: "next", programs: ["next"], manifests: ["package.json"],
      hint: "", busy: [], listening: [] },
    { id: "nuxt", programs: ["nuxt"], manifests: ["package.json"],
      hint: "", busy: [], listening: [] },
    { id: "angular", programs: ["ng"], manifests: ["package.json"],
      hint: "", busy: [], listening: [] },
    { id: "astro", programs: ["astro"], manifests: ["package.json"],
      hint: "", busy: [], listening: [] },
    { id: "cra", programs: ["react-scripts"], manifests: ["package.json"],
      hint: "", busy: [], listening: [] },
    { id: "rails", programs: ["rails"], manifests: ["Gemfile"],
      hint: "", busy: [], listening: [] },
    { id: "django", programs: ["django"], manifests: [],
      hint: "", busy: [], listening: [] }
]

// Owning plugin for a command, or null (→ generic patterns only).
function pluginFor(detectedProgram, manifest) {
    if (!detectedProgram)
        return null
    for (var i = 0; i < PLUGINS.length; ++i) {
        var p = PLUGINS[i]
        if (p.programs.indexOf(detectedProgram) < 0)
            continue
        if (p.manifests.length > 0 && p.manifests.indexOf(manifest) < 0)
            continue
        return p
    }
    return null
}

// Ownership for one output line: the command-owned plugin first, then any
// plugin scoped to this manifest whose own triggers match. The fallback
// covers proxied scripts (`npm --prefix ../frontend run serve`,
// `concurrently`, ...) that static detection cannot see through.
// Returns {plugin, hit, tool} with hit = {matched, port}; tool is true only
// when the match came from the plugin's own triggers (its hint applies).
function matchLine(detectedProgram, manifest, line) {
    var owned = pluginFor(detectedProgram, manifest)
    var pool = []
    if (owned)
        pool.push(owned)
    for (var i = 0; i < PLUGINS.length; ++i) {
        var p = PLUGINS[i]
        if (p === owned || p.busy.length === 0)
            continue
        if (p.manifests.length > 0 && p.manifests.indexOf(manifest) < 0)
            continue
        pool.push(p)
    }
    for (var j = 0; j < pool.length; ++j) {
        var triggers = pool[j].busy
        for (var k = 0; k < triggers.length; ++k) {
            var t = triggers[k]
            var m = t.re.exec(line)
            if (!m)
                continue
            var port = 0
            if (t.group > 0 && m[t.group])
                port = parseInt(m[t.group], 10) || 0
            return { plugin: pool[j], hit: { matched: true, port: port }, tool: true }
        }
    }
    var ghit = matchBusy(GENERIC_BUSY, line)
    if (ghit.matched)
        return { plugin: owned, hit: ghit, tool: false }
    return { plugin: null, hit: { matched: false, port: 0 }, tool: false }
}

// All busy triggers for a command: tool-specific first, generic last.
// Entry: {re, group} — group 0 means "port not in the line, resolve it
// from the error text / sniffed / configured port instead".
function busyTriggers(plugin) {
    if (plugin)
        return plugin.busy.concat(GENERIC_BUSY)
    return GENERIC_BUSY.slice()
}

// {matched, port} — port is 0 when the trigger carries none.
function matchBusy(triggers, line) {
    for (var i = 0; i < triggers.length; ++i) {
        var t = triggers[i]
        var m = t.re.exec(line)
        if (!m)
            continue
        var port = 0
        if (t.group > 0 && m[t.group])
            port = parseInt(m[t.group], 10) || 0
        return { matched: true, port: port }
    }
    return { matched: false, port: 0 }
}

// Sniffed port from a log line (generic + every plugin's patterns), 0 if none.
function matchListening(line) {
    var all = GENERIC_LISTENING.slice()
    for (var i = 0; i < PLUGINS.length; ++i)
        all = all.concat(PLUGINS[i].listening)
    for (var j = 0; j < all.length; ++j) {
        var m = all[j].exec(line)
        if (m) {
            for (var g = 1; g < m.length; ++g) {
                if (m[g])
                    return parseInt(m[g], 10) || 0
            }
        }
    }
    return 0
}

// Painted width/height ratio of each manifest icon (transparent margins
// excluded), so headers can size icons by height: text stays vertically
// centered while the icon runs slightly above and below it.
function manifestAspect(manifest) {
    switch (manifest) {
    case "package.json": return 2.43
    case "go.mod": return 2.67
    case "composer.json": return 0.76
    case "pom.xml": return 0.75
    case "build.gradle": return 1.33
    case "CMakeLists.txt": return 1.0
    case "pyproject.toml": return 1.0
    case "Gemfile": return 1.0
    case "mix.exs": return 0.69
    case "Cargo.toml": return 1.0
    case "custom": return 0.88
    default: return 1.0
    }
}

// Header icon height: narrow glyphs (composer, custom) get a taller box
// to hold visual weight, wide ones stay compact. Default 14.
function manifestHeaderHeight(manifest) {
    switch (manifest) {
    case "composer.json": return 28
    case "custom": return 24
    default: return 14
    }
}
// Manifest icons head the folder-page sections (sidebar uses the
// file-terminal marker instead). "custom" uses the bash glyph (dual-theme,
// no variants), unknown manifests the default glyph.
// dark picks the light variant.
function manifestIcon(manifest, dark) {
    switch (manifest) {
    case "package.json": return "npm-32px.png"
    case "Cargo.toml": return dark ? "rust-dark-32px.png" : "rust-light-32px.png"
    case "go.mod": return dark ? "go-dark-32px.png" : "go-light-32px.png"
    case "pyproject.toml": return "python-32px.png"
    case "pom.xml": return "java-32px.png"
    case "build.gradle": return "gradle-32px.png"
    case "CMakeLists.txt": return "cmake-32px.png"
    case "composer.json": return "composer-32px.png"
    case "Gemfile": return "ruby-32px.png"
    case "mix.exs": return "elixir-32px.png"
    case "custom": return dark ? "bash-dark.png" : "bash.png"
    default: return "default-32px.png"
    }
}

function stripSgr(s) {
    return s.replace(/\x1b\[[0-9;:?]*[ -/]*[@-~]/g, "")
}

// Client-side connect failure: `connect ECONNREFUSED 127.0.0.1:6379`.
// Unlike a busy port there is nobody to kill — the dependency itself is
// down. Returns {host, port} or null. Tool-agnostic (node, python, ...).
function parseRefused(line) {
    var m = /ECONNREFUSED\s+([^\s:]+):(\d+)/i.exec(line)
    if (!m)
        return null
    return { host: m[1], port: parseInt(m[2], 10) || 0 }
}

// Well-known service behind a refused port, "" when unknown.
function serviceName(port) {
    switch (port) {
    case 6379: return "Redis"
    case 5432: return "PostgreSQL"
    case 3306: return "MySQL/MariaDB"
    case 27017: return "MongoDB"
    case 9200: return "Elasticsearch"
    case 11211: return "Memcached"
    case 5672: return "RabbitMQ"
    case 9092: return "Kafka"
    default: return ""
    }
}
