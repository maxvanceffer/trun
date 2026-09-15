# 0001 — trun speaks MCP over stdio from the same binary

Date: 2026-09-10
Status: accepted

## Context

AI agents (Claude Code, Codex, opencode, Junie) need to see trun projects,
run commands and read logs. Every one of them supports MCP servers over stdio.

## Decision

- Transport: stdio with newline-delimited JSON-RPC (no HTTP port, no CORS).
- Location: `trun-app --mcp` flag in the same binary, reusing
  `ProjectService`, `CommandExecutor` and `Settings`.
- Tool surface: `list_projects`, `list_commands`, `status`, `run`, `stop`,
  `read_log`, `search_logs`. `run` applies the stored run configuration.
- Docker surface (same daemon the Docker page shows): `docker_status`,
  `docker_ps`, `docker_images`, `docker_logs`, `docker_control`
  (start|stop|restart|remove), `docker_stats`, `docker_prune`. Synchronous
  queries live on `DockerService` as statics shared with the GUI.

## Alternatives considered

- HTTP/SSE server: reachable from browsers, but needs a port, lifecycle
  management and CORS for zero agent benefit (all target agents do stdio).
- Separate `trun-mcp` binary: second artifact duplicating data access;
  configs could drift between GUI and server.

## Consequences

- Headless `main()` branch: no `QApplication`, no tray, no QML engine.
- Per-command log buffers live in the server (GUI `LogModel` is QML-side).
- Agent config writers are per-agent (Claude Code JSON, Codex TOML, …).
