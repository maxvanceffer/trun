# trun — implementation plan

One line: a resident tray app (Tauri v2, Rust + web) that scans a folder, discovers projects via pluggable manifest plugins, and lets you run/stop their commands from the system tray, with a dashboard for CPU/RAM + logs.

## Settled spec (from the grill)

- **Runtime** — Tauri v2 (Rust backend + web frontend), persistent resident process. Tray icon (your `tray.png`) always present; hidden webview; wizard window on first run; dashboard window for CPU/RAM + logs.
- **Config** — one global file `~/.config/trun/config.toml`: `{ path, commands = { name = { on, cmd } } }`. No file → first-run wizard. One root folder for v1.
- **Discovery** — recursive walk over the user's path *by filename*; each plugin declares `marker_filename` + `scripts_field`; the program finds marker files, the plugin reads `scripts`. No folder-listing fed to plugins.
- **Commands** — the script keys found (`build`, `start`, `lint`, `dev`, `serve`, …). Wizard groups them by folder (folder name = project name), each with a checkbox + editable `cmd` field. Tray/dashboard show only checked commands.
- **Plugins** — built-in (`npm`: `package.json`+`scripts`, `composer`: `composer.json`+`scripts`) + dynamic manifest plugins loaded from `~/.config/trun/plugins/` on startup. `trun plugin install <github-url>` git-clones a plugin into that dir.
- **Run/Stop** — PID toggle. Run = spawn a new PID, Stop = SIGKILL it. Finite commands (build/lint) auto-transition to stopped on exit; long-lived ones (serve) stay running.
- **Lifecycle** — app close = SIGKILL all children.
- **Logs** — `~/.config/trun/logs/<cmd>.log`, append, survives restart.
- **Tray** — one item per checked command: state marker + submenu [Run|Stop] + Logs → dashboard on that command's logs.
- **Dashboard** — sidebar (projects → commands), main (CPU/RAM via sysinfo poll + live logs).

## Flagged risk (not a re-open)

- **Tray colored dots** — target is a green/red dot to the left of each command. macOS menu items support custom icons (Tauri can set per-item icons); Linux GTK tray-menu support is limited. Target = colored dot; fallback = state glyph (●/○) + text label. Confirm on real macOS during the tray phase.

## Phases & tasks

### Phase 0 — Skeleton
- **T.0.1 Scaffold Tauri v2 app** `trun`: Rust backend + web, hidden main window, tray icon on startup.
  - Done when: launching shows a tray icon; a hidden webview exists.
- **T.0.2 Tray stub** — load `icons/tray.png`; menu with a "Settings" item + one placeholder command item.
  - Done when: tray menu renders with items.

### Phase 1 — Config
- **T.1.1 Config types + load/save** `~/.config/trun/config.toml`; create `~/.config/trun/` if missing.
  - Done when: load/save round-trips; missing file detected.
- **T.1.2 First-run detection** — no config → wizard window on startup.
  - Done when: first launch shows wizard; after save, no wizard.

### Phase 2 — Scan + Plugin
- **T.2.1 Plugin struct** `{ name, marker_filename, scripts_field }` + registry list.
  - Done when: registry returns a list of plugins.
- **T.2.2 Built-in plugins** — npm (`package.json`, `scripts`), composer (`composer.json`, `scripts`).
  - Done when: parsing a sample manifest yields script names.
- **T.2.3 Recursive walk** over user path by filename → found marker files (npm + symfony-style `composer.json` both detected).
  - Done when: walk finds markers in nested folders.
- **T.2.4 Scan dispatch** — walk + plugin → found commands (project = folder, commands = scripts), grouped by folder.
  - Done when: scan of a test folder returns projects + their scripts.

### Phase 3 — Wizard (GUI)
- **T.3.1 Wizard UI** — path input → scan → preview (grouped by folder, per-command checkbox + editable `cmd`) → save.
  - Done when: user enters path, sees found commands with checkboxes, edits a `cmd`, saves.
- **T.3.2 Save** writes `config.toml` (path + checked commands + edited `cmd`s).
  - Done when: `config.toml` reflects the wizard selection.

### Phase 4 — Logging + Process
- **T.4.1 Per-command logging** — `logs/<cmd>.log`, append; spawn with stdout+stderr → file.
  - Done when: starting a command writes its output to the file.
- **T.4.2 Start command** — spawn child, record PID per command.
  - Done when: a command starts, PID tracked.
- **T.4.3 Stop** — SIGKILL the PID, clear PID.
  - Done when: stop kills the process.
- **T.4.4 Auto-transition** — poll child; finite exits → "Run", long-lived stays "Stop".
  - Done when: build exits → item returns to Run; serve stays Stop.
- **T.4.5 Kill-on-close** — SIGKILL all children when the app quits.
  - Done when: quitting the app kills all running commands.

### Phase 5 — Tray
- **T.5.1 Dynamic per-command menu** — one item per checked command: state marker + submenu [Run|Stop] + Logs.
  - Done when: tray shows one item per checked command with Run/Stop + Logs.
- **T.5.2 State reconciliation** — green/red (or marker) updated on run/stop.
  - Done when: run → marker changes; stop → marker changes.
- **T.5.3 Logs → dashboard** — Logs click opens/focuses the dashboard on that command's logs.
  - Done when: Logs click focuses dashboard + shows that command's log.

### Phase 6 — Dashboard
- **T.6.1 Sidebar** — projects → commands (from config + scan).
  - Done when: sidebar lists projects and their commands.
- **T.6.2 Main** — CPU/RAM (sysinfo poll per PID) + logs view (live tail + file).
  - Done when: dashboard shows a running command's CPU/RAM + live logs.

### Phase 7 — Dynamic plugins + CLI
- **T.7.1 Dynamic manifest plugins** — load `~/.config/trun/plugins/*.toml` on startup.
  - Done when: dropping a manifest plugin in `plugins/` makes its commands appear.
- **T.7.2 `trun plugin install <github-url>`** — git clone into the plugins dir (CLI subcommand).
  - Done when: install clones it + the next startup picks it up.

### Phase 8 — Cross-platform
- **T.8.1 macOS template tray icon** (NSImage template).
- **T.8.2 Linux tray + sysinfo CPU/RAM** on both platforms.
- **T.8.3 Signals** — SIGKILL/terminal signal handling + graceful close.
  - Done when: runs clean on macOS and Linux.
