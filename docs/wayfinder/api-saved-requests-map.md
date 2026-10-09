## Destination

The first page of the trun API client: an API item in the global menu after Docker, a Saved requests page (groups by trun project, environment card on the right, history at the bottom, empty state with dev servers, search, Import cURL). Requests run on a separate thread, loading only on the first open of the page. Existing UI components only.

## Notes

- Domain: Qt 6 / QML, existing patterns DockerPage + DatabasesPage. Skills each session: qt-qml, qt-cpp-review, domain-modeling, grilling.
- Standing prefs: small steps; UI only from qml/Ui + components (missing component — stop, ask); qrc:// for resources; C++ network off the GUI thread; lazy-load on the first openApi; don't invent the unclear — ask.
- Glossary deltas: Saved request (named, with projectId), History entry (unsaved send, last 50), Environment (global set of variables), Environment variable (on/off, name, value, Secret→Keychain).

## Decisions so far

- [T1 — Threading, lazy-load and storage patterns](t1-patterns.md): findings on branch research/t1-patterns → docs/wayfinder/research-t1-patterns.md (QThread::create+Queued flow, lazy data without Loader, Settings JSON + Keychain, Sidebar→openX chain).
- [T3 — Shell: menu item, navigation, lazy Loader, icon](t3-shell.md): API item after Docker, openApi + breadcrumb + ApiPage stub, build green; icon api.png/-dark missing — temporarily terminal, asset needed.
- [T2 — Data schema: requests / environments / history](t2-schema.md): spec in schema-api.md (api/* keys, lastStatus inside request, secrets in Keychain trun.api.\<envId\>, history cap 50 in Settings).
- [T5 — Environment card + dialog](t5-environments.md): ApiService (env layer) + card + full dialog, round-trip verified, visual pass done, create-environment entry point (`+ New environment`) added.
- [T6 — Empty state + running servers + Import cURL](t6-empty-import.md): ApiService (requests layer + cURL parser), empty state in ApiPage with dev servers, import dialog, `test_api_import_curl`; visual pass done.
- [T4 — Saved requests list](t4-list.md): top row (search + Import cURL + New request), groups by project, rows (method-color/name/url/status-dot/time) and HISTORY on the shared `UiListElement` (card per row, hover ring; ADR-002); ApiService history (read + clear), tests `test_api_history_clear`, `test_api_page_list_groups`; fidelity feedback addressed.

## Not yet specified

None open.

- Resolved URL — moved to the second map: it is a URL-bar element (second page); the first page has no URL (in the design `→ {{ resolved }}` sits under the URL bar).
- Secret masking — settled in T5: the card renders `••••••••` / `(not set)`, the dialog uses password echoMode + `•••••••• (saved)`; nowhere else are values shown, only `{{name}}` names.
- Search fields: settled in T4 — name + url (as in the design), method is not searched.
- Settings/Keychain key exact format — settled by the schema (T2).

## Out of scope

- Second page (request page: URL bar, Params/Headers/Body/Auth, Send, response, Save dialog, breadcrumb rename, ⌘S) — a separate follow-up effort, not this map.
