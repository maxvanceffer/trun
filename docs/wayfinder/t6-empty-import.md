# T6 — Empty state + running servers + Import cURL (task, AFK)

Map: docs/wayfinder/api-saved-requests-map.md
Label: wayfinder:task
Status: closed — resolved

## Resolution

Done, build green, 33/33 qmltests, startup 4 lines / 0 QML errors, the UiButton
harness light 312/312 + dark 312/312, qmllint clean (only the two pre-existing
unqualified warnings in the env Repeater).

- C++ (`src/apiservice.h/.cpp`): the `requests` slice — `Q_PROPERTY requests`,
  `loadRequests()`/`persistRequests()` under the `api/requests` key, the lazy
  `ensureLoaded()` now reads it too. T6 actions: `newUntitled(url = "{{baseUrl}}/")`
  and `importCurl(command)`; both return an id ("" + `errorMessage()` on failure),
  write the request and `persistRequests()`.
- cURL parser: a custom tokenizer (quotes, `\` line continuations) + a flag table.
  Parsed: `-X/--request`, `-H/--header` (multiple), `-d/--data*` (joined with `&`,
  implies POST), `-u/--user` → basic auth, `--url`, `-I/--head`, `-G/--get`;
  boolean flags (`--compressed`, `-sSL` …) and unknown flags with a parameter are
  dropped so they don't eat the URL. `{{var}}` is not substituted — it stays as is
  (substitution at Send). Name = method + path, like the Save dialog hint from the
  design (`PATCH /v1/campaigns/{{id}}`).
- QML (`qml/ApiPage.qml`): the left column is now the T6 empty state — a dashed
  plate (Canvas, 1px dashed radiusSm, repaints on theme change), a `{}` badge
  44×44, a 16 bold title, a 13 subtitle, the `+ New request` (primary solid) and
  `Import cURL` (neutral outline, both sm) buttons, the
  `OR START FROM A RUNNING SERVER` block (max 440, hidden when there are no
  servers): 32px rows with an 8px green dot, Menlo url, `project · command`,
  amber `Open →`, a hairline separator except on the last, a footnote.
  Source: `projectService.runningCommands` × `dashboard.detectedPorts` by runKey
  `projectId|commandId`, only those whose port is already caught;
  URL = `http://localhost:<port>/`.
- While T4 was not done: with `requests.length > 0` the column showed the old stub
  "Saved requests will appear here." (user decision).
- Import dialog: in the design the `Import cURL` button has no handler and no
  form — by user decision it is an AppDialog (560×320) with a mono TextArea,
  Cancel / Import; Import is disabled while the field is empty, on a parse error
  the dialog stays open (the text is not lost), the message goes to the log.
- `New request` / `Open →` do not navigate (no second page) — model + stub, as in
  the spec; the counter switches the column from the empty state to the stub.
- Test: `test_api_import_curl` in `src/qmltests.cpp` (ApiService + mackeychain
  added to the qmltests target under `if(APPLE)`). Covers a multi-line command,
  quotes, `-X/-H/-d/-u`, POST inference from `-d`, `{{var}}`, empty-URL and
  empty-command errors, newUntitled and the persist round-trip.

## Open

- Empty state visually passed (clicking a server row, hover, dark theme — the
  dashed line repaints via Connections on isDark) — no discrepancies.
- T4 (done, t4-list.md): the Saved requests list replaced the stub, and the search
  row with the Import cURL / New request buttons from the top of the design moved
  there too. The draft-before-Save question remains (there is no Save page).
