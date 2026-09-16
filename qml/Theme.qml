pragma Singleton
import QtQuick
import QtQuick.Controls

// Design tokens adapted from the shadcn-vue preset:
// style "reka-mira", base color "olive", Geist Mono (font not bundled).
// oklch values converted to hex; white overlays composited over dark bg.
// Deviation from the preset: corners stay 8px (preset specifies radius 0).
QtObject {
    readonly property bool isDark: Application.styleHints.colorScheme === Qt.ColorScheme.Dark

    // Window: light #ffffff / dark #0c0c09
    readonly property color windowBackground: isDark ? "#0c0c09" : "#ffffff"
    // Sidebar: light #fbfbf9 / dark #1d1d16
    readonly property color sidebarBackground: isDark ? "#1d1d16" : "#fbfbf9"
    // Cards / console / dialogs: light #ffffff / dark #1d1d16
    readonly property color cardBackground: isDark ? "#1d1d16" : "#ffffff"

    // Text on theme-aware surfaces
    readonly property color textPrimary: isDark ? "#fbfbf9" : "#0c0c09"
    readonly property color textMuted: isDark ? "#abab9c" : "#7c7c67"

    // primary: light #fdc700 / dark #f0b100, on-primary #733e0a
    readonly property color primary: isDark ? "#f0b100" : "#fdc700"
    readonly property color primaryForeground: "#733e0a"
    // accent == primary in this preset
    readonly property color accent: isDark ? "#f0b100" : "#fdc700"
    readonly property color accentForeground: "#733e0a"
    // secondary
    readonly property color secondary: isDark ? "#27272a" : "#f4f4f5"
    readonly property color secondaryForeground: isDark ? "#fafafa" : "#18181b"
    // muted surfaces
    readonly property color mutedSurface: isDark ? "#2b2b22" : "#f4f4f0"
    // destructive: light #e7000b / dark #ff6467
    readonly property color destructive: isDark ? "#ff6467" : "#e7000b"
    // hairlines: light #e8e8e3 / dark white 10% (#242422)
    readonly property color border: isDark ? "#242422" : "#e8e8e3"
    // ring
    readonly property color ring: isDark ? "#abab9c" : "#7c7c67"

    // ─── Layout scale ──────────────────────────────────────────────────
    // Single source of truth for spacing/sizing so paddings stay uniform.
    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 12
    readonly property int spacingLg: 16
    readonly property int spacingXl: 24

    readonly property int radiusSm: 6
    readonly property int radiusMd: 8
    readonly property int radiusLg: 10

    readonly property int iconSm: 16
    readonly property int iconMd: 20
    readonly property int iconLg: 24
    readonly property int iconXs: 12

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
}
