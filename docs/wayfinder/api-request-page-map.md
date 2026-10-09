# API client — request page (map)

Follow-up to [api-saved-requests-map.md](api-saved-requests-map.md) (first page,
closed). Second map: the second page of the API client — the request page.

## Destination

The request page in trun: opening a saved request (or a history entry, or
"+ New request" / a running-server "Open →") navigates to it. It holds the
top row (editable name, "unsaved changes", Save), the
URL bar (method, URL, environment, Send), the resolved-URL line (`→ {{ resolved }}`),
the Params / Headers / Body / Auth tab panel, and the Response panel (status pill,
time/size, Body/Headers tabs, Copy). Requests run off the GUI thread; every Send
appends to history (cap 50); Save (⌘S) writes the request into its collection; ⌘↩
sends. Source screens: "API Client.dc.html" — the Request page screen + the Flow
list at the bottom; behaviors in its `<script>` (send/cancel, tab counts,
save-label, body/auth modes, JSON token colors, param↔URL sync).

## Notes

- Domain: Qt 6 / QML. Reuses ApiService (`src/apiservice.*`), the schema
  (`schema-api.md`), `qml/Ui/*` and `qml/components/*`, and the first page's
  navigation/tab patterns (DatabasesPage two-level `selectedIndex`, env switch).
  Skills each session: qt-qml, qt-cpp-review, domain-modeling, grilling.
- Standing prefs: small steps; UI only from qml/Ui + components (missing
  component — stop, ask, with paths to `src/runtime/components/...` and
  `src/theme/...`); qrc:// for resources; C++ network off the GUI thread;
  lazy-load (draft/mode state stays in memory, service I/O only on actions);
  don't invent the unclear — ask.
- Glossary deltas: Draft (a request not in `api/requests` yet: id `0` /
  "Untitled request", project "unsaved"); Request page (the editing surface for
  one request); Resolved URL (`{{var}}` substituted from the active environment
  + enabled params appended as query); Response (status pill, ms, size,
  body/headers); dirty (any edit on an opened request, or it is a draft).
- Icon needs: chevron-left (Back, in qml.qrc already), copy (Copy buttons),
  x / close, chevron-up/down (tab panel collapse is NOT in the scope — there is
  no collapse in the design for this page). Verify what qml.qrc has before
  adding.

## Tickets

- [R1 — HTTP off the GUI thread (research, AFK)](r1-networking.md): how to run a
  QNetworkAccessManager on a dedicated thread (event loop), signal surface,
  cancel, timeout, redirect/tls policy, error taxonomy (refused vs timeout vs
  HTTP). Findings → docs/wayfinder/r1-networking.md.
- [R2 — ApiService storage for the request page (task, AFK)](r2-storage.md):
  persist `params` in the request JSON; Save (name/project), rename, update on
  Send (`lastStatus/lastAt/updatedAt/name/project`), `appendHistory` writer
  (cap 50, resolved URL), draft factory. Tests in qmltests.
- [R3 — Shell and navigation (task, HITL)](r3-shell.md): implemented — two-level
  swap list ↔ request page (DatabasesPage pattern), open from list row / history
  row / "Open →" / "+ New request" (drafts, `newUntitled` removed); top row:
  inline name rename, "unsaved changes", Save. No Back button and no in-page
  crumbs (house pattern: the Dashboard header gets an `API / <name>` 3rd
  segment, the `API` crumb and Esc pop back). The **Save dialog was pulled from
  R7** so the Save button is real (suggested name, project select, method/url
  preview, Enter/Esc); R7 keeps ⌘S/⌘↩, append-to-history, replay, polish.
  Visual pass pending.
- [R4 — URL bar (task, HITL)](r4-urlbar.md): method select (UiSelectMenu,
  method color), URL field (UiInput, Menlo), environment switch, Send/Cancel
  button with loading state, resolved-URL line underneath. Visual pass.
- [R5 — Tabs: Params / Headers / Body / Auth (task, HITL)](r5-tabs.md): tab
  bar with counts + `⌘↩ send · ⌘S save` hint; KV editor rows (checkbox, key,
  value, ✕, dashed "+ Add param/header" pill) for Params and Headers; Body
  (mode segmented none/JSON/form + mono textarea + hint); Auth (mode segmented
  None/Bearer/Basic + token field + hint); param↔URL query sync. Visual pass.
- [R6 — Send + Response panel (task, HITL)](r6-response.md): Send wires R2/R4;
  response states (empty / loading / error / success), status pill (band colors),
  ms, size, Body (JSON, line numbers + token colors) / Headers tabs, Copy.
  Visual pass.
- [R7 — Save, shortcuts, history (task, HITL)](r7-save.md): Save dialog (name
  suggested from method+path, project select, method/url preview, Enter/Esc),
  ⌘S / ⌘↩, "Saved ✓" flash, save-label per state, append-to-history, replay
  opens. Tests + visual pass.

## Decisions so far

- **JSON token colors** (R6): map onto existing Theme tokens — strings→`Theme.primary`,
  keys→`Theme.textPrimary`, numbers→`Theme.info`, `true/false/null`→`Theme.error`,
  punctuation→`Theme.textSubtle`; line numbers `Theme.textSubtle`. No new tokens;
  ui-rules gains a "code" case.
- **History URL** (R2, schema): `appendHistory` stores the **resolved** URL —
  what was actually sent (`{{var}}` substituted, enabled params appended). Rows
  and replay repeat the real network call, as the design shows.
- **Leave with unsaved changes** (R3): silent discard, like the design — the
  `API` crumb / Esc always return to the list, no confirm; edits not saved are
  lost (the "unsaved changes" hint only informs).
- [R1 — HTTP off the GUI thread](r1-networking.md): closed. Send slice runs
  per-request `QThread::create` + local `QNAM` + `QEventLoop` (blocking-in-worker,
  the Database/Docker pattern); ApiService stays on the GUI thread; Cancel via
  queued `abort()`; result delivered on `sendFinished`; findings in
  research-r1-networking.md.
- [R2 — ApiService storage for the request page](r2-storage.md): closed.
  `makeDraft` / `saveRequest` (create + update-in-place, no duplicate) /
  `updateRequestStatus` / `appendHistory` (prepend, stamp `at`, cap 50) /
  `suggestName`; `params` added to the saved-request JSON (normalized on load);
  decision: Send stamps the stored request's `lastStatus/lastAt/updatedAt`
  immediately (the list row stays truthful), drafts write history only, content
  edits persist on Save. 5 new qmltests — 41/41 green.
- [R3 — Shell and navigation](r3-shell.md): closed. Two-level slide swap
  (DatabasesPage pattern, no StackLayout); entry points open the request page:
  list row (detached clone), history row, "Open →", "+ New request" (drafts;
  `newUntitled` removed); top row = inline rename (UiInput `variant: ghost`) +
  "unsaved changes" + Save ("Save…" for drafts, "Save", "Saved ✓" flash);
  no Back button / in-page crumbs (Dashboard header gets an `API / <name>`
  3rd segment, `API` crumb + Esc pop back; silent discard on leave); the Save
  dialog was pulled from R7 (suggested name, project select, method+url
  preview, Enter/Esc, Save disabled until a name); `saveRequest` returns the id
  so the page binds to the persisted row. 1 new qmltest — 42/42 green.

## Not yet specified — ask before implement

(empty — resolved by the decisions above; further questions land here as
tickets surface them)

## Defaults (state-only, veto freely)

- Methods: GET / POST / PUT / PATCH / DELETE (the design's methodOpts).
- Body "form" = a plain textarea, hint "key=value per line" (no KV grid).
- Send timeout 30 s; redirects followed; method color / status band per the
  first page's mapping (GET→success, POST→primary, PUT/PATCH→info, DELETE→error;
  status ≥500→error, ≥400→warning, else success).
- Draft name "Untitled request", url `{{baseUrl}}/`; Save on a draft opens the
  dialog with name suggested `METHOD /path`; project defaults to the first
  workspace project.
- Dirty on any edit; `dirty || isDraft` shows "unsaved changes".

## Out of scope

- OAuth flows and auth beyond the design's Bearer/Basic token fields.
- Response tabs beyond Body / Headers (no raw/preview/pretty modes).
- Collapse of the request panel (the design has no chevron here) and body size
  limits, csv/xml pretty-printing, websockets, multipart uploads, request
  history beyond replay-opening, environment editor (first map, T5).