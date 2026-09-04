# CLAUDE.md — Rules for the Trun project

## 🚀 CRITICAL: Testing RULE (READ BEFORE EVERY TEST)

### THE PROBLEM
Multiple Nuxt instances run simultaneously. Old instances serve stale content. When you open localhost:3000 in ego-browser, you see OLD code. Tauri shows real current code but browser testing shows garbage. **THIS IS THE #1 SOURCE OF CONFUSION.**

### THE RULE (EVERY SESSION, EVERY TIME)
Before testing ANYTHING, you MUST do this:

```bash
cd /Users/maxxxtraxxx/projects/trun
./scripts/dev-start.sh          # Kill ALL old instances
sleep 2                         # Let ports clear
cd frontend && npx nuxi dev --port 3000  # Start fresh
sleep 8                         # Wait for build
```

Then verify port 3000 is the new process:
```bash
lsof -i :3000 | head -3  # Should show ONE node process
```

### What dev-start.sh kills
1. Anything on port 3000
2. Tauri app binary (target/debug/app)
3. Tauri CLI (node *tauri dev*)
4. Nuxt dev processes (pgrep -f "nuxt dev")
5. Any trun-related node processes

### ⚠️ NEVER SKIP THIS STEP
If you test in a browser without doing this, you see stale content. Tests will fail falsely. This has caused real user frustration.

## 🏗️ Architecture

- **Frontend:** Nuxt 4 (SPA mode, `ssr: false`) — `frontend/`
- **Backend:** Tauri + Rust — `src-tauri/`
- **Communication:** Tauri events (`listen`/`invoke`) via `useTauri()` composable

### Key Constraints
- SSR must be disabled (`ssr: false`) — Tauri dev mode fails with HSR/SSR
- `useTauri` safely returns no-ops in browser mode, only works in Tauri shell
- All DOM access must be guarded by `if (import.meta.client)`

## 🌳 Sidebar Tree View

Uses Nuxt UI's `UTree` component — no custom tree/connector CSS.
- Items: `TreeItem[]` with `id`, `label`, `icon`, `children`, `projectCount`
- Auto-expansion via `expandedIds` state (folder paths only)
- Selection via `@select` → `onTreeSelect()` → `selectProject()`

## 📁 Key Files
- `frontend/app/pages/index.vue` — Dashboard
- `frontend/app/pages/wizard.vue` — Wizard
- `frontend/composables/useTauri.ts` — Tauri bridge
- `src-tauri/src/lib.rs` — Rust backend (scan, run, commands)
- `src-tauri/src/events.rs` — Event emission helpers

## 🎨 CSS Policy (STRICT)

### `assets/css/main.css` — NO OVERRIDES WITHOUT PERMISSION
- `main.css` must remain minimal: only `@import "tailwindcss"` and `@import "@nuxt/ui"`
- NEVER add CSS overrides in `main.css` without explicit user approval
- ALWAYS prefer Nuxt UI `:ui` prop over custom CSS
- ALWAYS prefer Nuxt UI component slots over custom CSS
- If a style tweak is needed, try `:ui` prop → slots → ask user
- Icon overrides: use Nuxt UI's icon system (`i-logos-*`, `i-lucide-*`) instead of SVG files

## 🔧 Common Issues
1. **Port 3000 in use** → run `./scripts/dev-start.sh` first
2. **Hydration errors** → ensure `ssr: false` in nuxt.config.ts
3. **Tauri IPC not working** → verify you're in Tauri shell, not browser
4. **Multiple app windows** → run `./scripts/dev-start.sh` to clean up
