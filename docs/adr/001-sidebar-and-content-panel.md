# ADR-001: Sidebar Tree and Content Panel Architecture

**Status**: Implemented
**Date**: 2025-06-29
**Context**: Restructuring the app with a shared sidebar layout, dashboard page, and clean separation between native commands (project scripts) and custom commands (user-defined).

## Decisions

### 1. Content panel shows only native project commands
- Native commands come from scripts in package.json, Cargo.toml, go.mod, etc.
- Custom commands stored in localStorage are **not** shown in content panel.
- Custom commands appear only as tree items. Click on them = direct execution.

### 2. Project items open content panel
- Clicking a project entry selects the project and shows its native commands.
- Content panel shows: command cards with Run/Stop buttons, PID, memory usage.

### 3. Folder items only expand/collapse
- Clicking a folder does NOT change the content panel.
- Current selection persists until the user clicks a different project item.

### 4. Custom commands don't overlap with native scripts
- Custom commands are a separate namespace from project scripts.
- A custom command named `dev` coexists with package.json's `dev` script.

### 5. Dashboard is a separate view with global sidebar link
- `dashboard.vue` is a standalone page at `/dashboard`.
- A permanent "Dashboard" item in the tree is always first, before project nodes.
- Clicking Dashboard navigates to `/dashboard`.

### 6. Shared tree state via composable
- `composables/useTreeState.ts` exports a function `useTreeState()` that returns shared refs.
- Both layout (`app/layouts/default.vue`) and pages (`index.vue`, `dashboard.vue`) call it.
- Singletons: expandedIds, activeProject, activeProjectCommands, customCommands, runningProcesses.

### 7. Nuxt layout pattern
- `app/layouts/default.vue` wraps all pages with sidebar + `<NuxtPage />`.
- `app.vue` contains only `<UApp><NuxtLayout><NuxtPage /></NuxtLayout></UApp>`.
- Layout owns sidebar, tree, custom command modal.
- Pages own only their content panel.

## Files

- `app/layouts/default.vue` — Layout with sidebar, tree, custom command modal
- `app/pages/index.vue` — Project dashboard (commands + logs)
- `app/pages/dashboard.vue` — Global running commands summary (empty placeholder)
- `app/composables/useTreeState.ts` — Shared reactive state singleton
- `docs/adr/001-sidebar-and-content-panel.md` — This document

### 8. Wizard has no sidebar (layout: 'empty')
- `wizard.vue` uses `definePageMeta({ layout: 'empty' })` to bypass the default layout.
- `app/layouts/empty.vue` is a minimal wrapper without sidebar.
- Wizard takes full screen — no sidebar, no extra chrome.
