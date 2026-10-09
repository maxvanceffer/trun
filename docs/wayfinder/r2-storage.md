# R2 — ApiService storage for the request page (task, AFK)

Map: docs/wayfinder/api-request-page-map.md
Label: wayfinder:task
Status: closed — resolved
Blocked-by: —

## Question

Which C++ surface must ApiService expose so the request page (R3–R7) can open
drafts, Save (create + update + inline rename), stamp the last send's
status/time, and append to history — without the page owning persistence logic?
How does `params` land in the persisted request JSON, and what does the design's
"Save writes the request into its collection / name renameable inline" mean for
storing an updated request (position kept, no duplicate)?

## Design-draft (ticketed outcomes)

- Storage is additive: the first page keeps its behavior; only the request page
  uses the new calls. `newUntitled()` stays for T4 until R3 rewires the entry
  points to open the page (then it is removed).
- `makeDraft(url)` → an unsaved request (id `""`): "Untitled request", GET,
  `{{baseUrl}}/` (or the pasted url — the running-server "Open →" case),
  empty headers/params/body, auth `{"type":"none"}`, `lastStatus 0`,
  `lastAt ""`, `updatedAt` now. Not persisted. Reuses the makeRequest body.
- `saveRequest(map)` → create (id empty → assigned, appended) or update (same
  id → replaced in place, position kept, no duplicate row); bumps `updatedAt`;
  persists; `requestsChanged`. Unknown non-empty id → `errorMessage` + false.
- `updateRequestStatus(id, status)` → sets `lastStatus/lastAt/updatedAt` on the
  stored request only (drafts/unknown → false, no error). Decision: Send stamps
  the stored request's status/time immediately (the list row / schema T2 reads
  `lastStatus/lastAt` — otherwise the list freezes at save-time values);
  content edits still persist only on Save.
- `appendHistory(entry)` → prepend (newest on top, like the design's history),
  stamp `at` when absent, cap 50 (oldest dropped at the end), persists,
  `historyChanged`. `url` is the **resolved** URL (what was sent — decided).
- `suggestName(method, url)` → exposes the existing save-dialog suggestion
  "METHOD /path" to QML (design's `save()` name suggest).
- Schema: Saved request gains `params: [{name, value, enabled}]` (same shape as
  headers; the mock's JS tuples are not our persisted format); `loadRequests`
  normalizes missing `params`/`auth`/`body` so old JSON opens cleanly.
- Tests (qmltests.cpp): draft factory + save-create + round-trip, update/rename
  without duplication + unknown-id error, status stamping + draft/unknown
  no-op, history prepend/stamp/cap-50/round-trip, suggestName forms.
- Docs: schema-api.md updated alongside (params field, resolved history URL,
  send-stamps-status note).

## Resolution

Filled by the executor (implementation in apiservice.h/.cpp + tests).