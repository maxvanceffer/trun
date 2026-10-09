pragma Singleton
import QtQuick
import QtQuick.Controls

// Design tokens — Nuxt UI v4 "Carbon" theme, 1:1 with the project config:
//   assets/main.css     (@theme carbon palette + --ui-* semantics + --ui-radius)
//   assets/app.config.ts (primary: amber, secondary: yellow, neutral: carbon)
// oklch values converted to hex with culori; --ui-* mappings copied verbatim
// from main.css (:root,.light and .dark blocks). Accent shades: 500 in light,
// 400 in dark. Corner scale derives from --ui-radius: 0.5rem.
QtObject {
    readonly property bool isDark: Application.styleHints.colorScheme === Qt.ColorScheme.Dark

    // ─── Surfaces (--ui-bg-*) ─────────────────────────────────────────
    // bg-default: the page and the modal surface.
    readonly property color windowBackground: isDark ? "#292929" : "#fffaee"
    // bg-elevated: form controls, hover rows, off-state switches.
    readonly property color mutedSurface: isDark ? "#3d3d3d" : "#f0ccb3"
    // bg-accented: the hover step above bg-elevated.
    readonly property color mutedSurfaceHover: isDark ? "#555454" : "#b29a90"
    // Card subtle (the configured card variant) is `bg-elevated/50` over the
    // page; composited here so every consumer keeps an opaque surface.
    // Cards/panels only: Nuxt gives Modal, Tooltip, Select and Dropdown
    // content `bg-default` — those take backgroundDefault below.
    readonly property color cardBackground: isDark ? "#333333" : "#f8e3d1"
    // Sidebar: macOS glass tint, same tone as the page (no --ui- token).
    readonly property color sidebarBackground: windowBackground
    // bg-inverted: solid neutral fills.
    readonly property color backgroundInverted: isDark ? "#fffaee" : "#181818"

    // ─── Text (--ui-text-*) ───────────────────────────────────────────
    readonly property color textPrimary: isDark ? "#ffe1c1" : "#181818"   // text
    readonly property color textMuted: isDark ? "#f0ccb3" : "#292929"     // text-muted
    readonly property color textDimmed: isDark ? "#b29a90" : "#7a706d"    // text-dimmed
    // Faintest readable text (unselected segments, placeholders). One shade
    // below textDimmed on the carbon scale; no --ui- token exists.
    readonly property color textSubtle: isDark ? "#7a706d" : "#b29a90"
    readonly property color textInverted: isDark ? "#181818" : "#fffaee"  // text-inverted

    // ─── Accents (--ui-<color>): 500 light / 400 dark ─────────────────
    readonly property color primary: isDark ? "#ffb900" : "#fe9a00"       // amber
    readonly property color accent: primary                               // accent == primary
    readonly property color secondary: isDark ? "#fdc700" : "#f0b100"     // yellow
    readonly property color success: isDark ? "#05df72" : "#00c950"
    readonly property color info: isDark ? "#51a2ff" : "#2b7fff"
    readonly property color warning: isDark ? "#fdc700" : "#f0b100"       // yellow
    readonly property color error: isDark ? "#ff6467" : "#fb2c36"

    // ─── Foregrounds on accent fills ──────────────────────────────────
    // Solid buttons/badges/banners: Nuxt writes `text-inverted bg-<color>`,
    // but --ui-text-inverted is near-white in light mode — 1.83–3.65:1 on
    // this palette, so every solid colour fails WCAG AA (4.5:1), while the
    // dark half of the theme already gets #181818 (6.2–11.3:1). One token,
    // carbon ink in both modes: 4.66–11.3:1 on all five accents.
    // Neutral and inverse fills keep text-inverted — those are the dark fills.
    readonly property color accentForeground: isDark ? textInverted : textPrimary
    // Readable label for accent-coloured outline/ghost text. Not a Nuxt
    // token — Nuxt uses `text-<color>` there, which UiButton computes.
    readonly property color secondaryForeground: textPrimary
    // AppSwitch knob on a checked track: switch.ts thumb is `bg-default`.
    readonly property color accentInk: windowBackground

    // ─── Borders & chrome ─────────────────────────────────────────────
    readonly property color border: isDark ? "#555454" : "#09090b"        // border
    readonly property color borderAccented: isDark ? "#7a706d" : "#09090b" // border-accented
    // Focus ring. Nuxt draws it per-colour (`outline-<color>/25`); the app
    // keeps one neutral indicator, taken from the carbon mid steps.
    readonly property color ring: isDark ? "#b29a90" : "#7a706d"
    // Dialog shadow — no --ui- token, kept as a near-black carbon step.
    readonly property color shadowColor: isDark ? "#09090b" : "#181818"

    // ─── Aliases kept for existing call sites ─────────────────────────
    readonly property color backgroundDefault: windowBackground           // bg-default
    readonly property color backgroundElevated: mutedSurface              // bg-elevated

    // ─── Layout scale ──────────────────────────────────────────────────
    // Single source of truth for spacing/sizing so paddings stay uniform.
    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 12
    readonly property int spacingLg: 16
    readonly property int spacingXl: 24

    // --ui-radius: 0.5rem → rounded-sm 8 / rounded-md 12 / rounded-lg 16
    readonly property int radiusSm: 8
    readonly property int radiusMd: 12
    readonly property int radiusLg: 16

    readonly property int iconSm: 16
    readonly property int iconMd: 20
    readonly property int iconLg: 24
    readonly property int iconXs: 12

    // Form controls (UiInput, UiSelectMenu, …): one height across the kit.
    readonly property int controlHeight: 32

    // Sidebar geometry (shared by the sidebar and the main window layout)
    readonly property int sidebarWidth: 240
    readonly property int sidebarInset: 16     // left rail for titles and list
    readonly property int sidebarIndent: 16    // per-level tree indent
    readonly property int sidebarRowHeight: 28
    readonly property int titleBarHeight: 48

    // Typography
    readonly property int fontSizeSm: 11
    readonly property int fontSizeMd: 12
    readonly property int fontSizeLg: 13

    // ─── Motion ────────────────────────────────────────────────────────
    // Single source of truth for animation timings so hover/press fades
    // stay consistent across controls (IconButton ghost, rescan, fades).
    readonly property int animFast: 120
    readonly property int animHover: 160
    readonly property int animMedium: 220
    readonly property int animSlide: 260
    readonly property int animSlow: 350
    readonly property int easingStandard: Easing.OutCubic

    // ─── Icon resolution ───────────────────────────────────────────────
    // Single source of icon paths: logical name -> themed file.
    // No component knows isDark or file names; this function does.
    // Files follow name.png (light) / name-dark.png (dark); single-variant
    // icons (no dark twin) are listed explicitly.
    function icon(name) {
        var single = {
            "git-branch": "git-branch.png",
            "default": "default.png",
            "npm": "npm.png",
            "python": "python.png",
            "java": "java.png",
            "gradle": "gradle.png",
            "cmake": "cmake.png",
            "composer": "composer.png",
            "ruby": "ruby.png",
            "elixir": "elixir.png",
            "postgresql": "postgresql.png",
            "mysql": "mysql.png",
            "mongodb": "mongodb.png",
            "redis": "redis.png"
        }[name]
        if (single) return iconBaseUrl + single // qmllint disable unqualified
        return iconBaseUrl + name + (isDark ? "-dark.png" : ".png") // qmllint disable unqualified
    }
}
