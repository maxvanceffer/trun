# trun

Desktop Qt/QML launcher that scans a folder for dev projects and runs their commands, keeping each command's console output.

## Language

**Project**:
A folder containing a recognized manifest file (package.json, Cargo.toml, go.mod, and more).
_Avoid_: repo, directory, workspace root

**Manifest**:
The file that marks a folder as a project and declares its commands (e.g. `scripts` in package.json).
_Avoid_: config, descriptor file

**Command**:
A runnable script declared by a manifest, identified as `<folder>/<manifest>` plus a script name (e.g. `backend/package.json` running `npm run build`).
_Avoid_: task, job, action

**Run**:
One live execution of a command. A command may have several live runs only when its run configuration allows multiple instances; otherwise starting it again restarts the running one.
_Avoid_: process instance, execution

**Orphan run**:
A run left behind by a previous app session ("leave running" on exit). Adopted on startup after verifying the pid is alive and still runs the same executable.
_Avoid_: zombie, detached process

**Console history**:
The output lines kept per command across reruns, shown for the selected command only.
_Avoid_: log buffer, terminal scrollback

**Detail page**:
The full view of one command (breadcrumb, stat widgets, Run/Stop, its console), opened from a grid card with a slide animation.
_Avoid_: command screen, modal

**Breadcrumb**:
The detail page header showing `project › command`; the project part navigates back to the grid.
_Avoid_: title bar, back button

**Run configuration**:
Per-command overrides edited in the in-app dialog (name, executable, working directory, arguments, environment, allow-multiple). Stored in Settings, applied on Run.
_Avoid_: run config file, launch profile

**MCP server (trun)**:
The app itself speaking MCP over stdio (`trun --mcp`), exposing projects, commands, run controls and logs to AI agents.
_Avoid_: plugin, extension

**MCP entry**:
The `trun` record inside one agent's config (e.g. an `mcpServers` item). Its presence means trun is installed for that agent.
_Avoid_: integration, connector

**Pinned command**:
A command flagged by the user (toggle on its card) to appear in the system tray menu with Run/Stop actions.
_Avoid_: favorite, bookmark

**Detected program/port**:
Static guess from a command's script text (e.g. `vite --port 3000` → vite:3000) or sniffed at runtime from printed URLs. Prefills Port, never overwrites the stored value.
_Avoid_: autodetect, magic port
