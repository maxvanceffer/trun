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
}
