# R6 — Send + Response panel

Status: closed · Branch: research/t1-patterns
Map: [api-request-page-map.md](api-request-page-map.md) (ticket "R6 — Send + Response panel")
Blocked-by: R1 (networking design, closed), R2 (storage, closed), R4 (URL bar, closed),
R5 (tabs, closed)

## Goal

Wire the R4 Send button to a real HTTP call and render the answer. The response
panel has four states — empty (nothing sent yet), loading (in flight), error
(could not connect / timeout), success (a status came back, 4xx/5xx included) —
and shows a status pill, elapsed ms, body size, and Body / Headers tabs with a
Copy. Every Send appends to history (the resolved URL) and stamps the stored
request's `lastStatus/lastAt` immediately (the R2 decision, so the list row stays
truthful even before Save).

## Scope

### 1. ApiService — Send slice (`src/apiservice.h/.cpp`)

R1 fixed the shape; this ticket lands it. The class stays on the GUI thread
(Settings / Keychain / storage); the HTTP I/O spawns a throwaway worker.

- `Q_INVOKABLE bool send(QVariantMap spec)` — resolves the URL and headers on the
  GUI thread (secret values are read from Keychain here — they must never travel
  through QML), snapshots the resolved request and starts a per-request
  `QThread::create` with a local `QNetworkAccessManager` + `QEventLoop`
  (blocking-in-worker, the Database/Docker pattern). Returns `false` while one is
  in flight (the design's one Send at a time).
- `Q_INVOKABLE void cancel()` — `QMetaObject::invokeMethod(reply, "abort",
  Qt::QueuedConnection)` on the in-flight reply.
- `Q_PROPERTY(bool sending …)` — the Send/Cancel bar state.
- `signal void sendFinished(QVariantMap result)` —
  `{ ok, canceled, status, reason, headers: [{name, value}], body, ms, error,
  errorCode }`.

Request building (verified in `research-r1-networking.md` §4):

- method → `get/post/put/delete(...)`; PATCH → `sendCustomRequest(request,
  "PATCH", body)`.
- headers list → `setRawHeader` each (enabled rows only).
- body: the editor's text; JSON body mode adds `Content-Type: application/json`
  unless the user set their own.
- `QNetworkRequest::setTransferTimeout(30000)` (the map's 30 s default).
- redirects: QNAM defaults (`NoLessSafeRedirectPolicy`), chain followed.

URL resolution (C++ mirror of the QML preview, but with real secrets):

- `{{var}}` substituted from the **active** environment; a secret var's value
  comes from Keychain (account = var name, service `trun.api.<envId>`) — the QML
  side only ever sees `••••••••`.
- enabled params with a name appended as a query string (`?a=1&b=2`), joined with
  `&` when the URL already has a `?`.
- an unset secret keeps its `{{name}}` placeholder (same as the preview);
  sending such a URL is the user's call.

Error taxonomy (R1): `NoError` + status → success (4xx/5xx are a *response*, not
an error); `OperationCanceledError` → canceled (no history entry);
`ConnectionRefusedError` → "could not connect"; `TimeoutError` → timeout;
`HostNotFoundError`/others → the same error state with their message.

### 2. ApiRequestPage.qml — wiring

- `startSend()`: `syncToRequestMap()` (R5 KV rows land in the map), build the
  spec (method, raw url, params, headers, body, auth, `activeEnvId`), call
  `svc.send(spec)`. `sending` becomes a read of `svc.sending` (the R4 local stub
  goes away).
- `cancelSend()` → `svc.cancel()`.
- `Connections { target: svc; function onSendFinished(result) }`:
  - `canceled` → back to idle, no history, no stamp (R2 already skips drafts and
    writes history on success only — keep that).
  - error → error state `{ host, message }`.
  - success → response state `{ status, reason, ms, headers, body }`;
    `updateRequestStatus(requestMap.id, status)` (no-op for drafts);
    `appendHistory({ method, url: resolved, status })`.
- Send stays button-only here: ⌘S / ⌘↩ are R7's shortcuts (so the R5 hint text is
  not wired yet).

### 3. Response panel

A new block under the R5 tab panel. **Layout: fixed vertical split** (decided) —
the request panel (URL bar + R5 tabs) keeps the top share, the response panel the
bottom share; each is its own `ScrollView` so they scroll independently. The
design has no resizer, so the proportion is fixed (roughly half/half, tuned in
the visual pass); the request panel is always expanded (no collapse — the design
has no chevron here).

- **States.** empty (a hint, "Send a request to see the response"), loading
  (spinner while `sending`), error (the design's "Could not connect" + host +
  message), success.
- **Status pill + meta row** (success): a pill with the status code + reason,
  band colour ≥500 → `error`, ≥400 → `warning`, else `success` (the map's
  mapping), then `ms` and size (`B` / `KB` / `MB`). Reuses `UiTag`/`UiButton`
  where the shape matches.
- **Body / Headers tabs**: a second `UiTabs` (Body | Headers). Body = the reply
  body rendered as code (below); Headers = the reply header list `name: value`
  (mono). The body tab label may carry the size, headers tab the count — confirm
  against the design.
- **Copy**: a Copy button per tab (Body → the raw body, Headers → the header
  list), `Theme.icon("clipboard")` (already in `qml.qrc`). Swaps to a "Copied"
  flash the way the Save button does.

### 4. Code rendering (JSON)

- A small formatter (proposed `qml/JsonFormat.js`) turns the body into
  `RichText`: pretty-printed JSON when it parses, plain mono text otherwise.
- Token colours — no new tokens, the map's mapping:

  | JSON token | Token |
  |---|---|
  | string | `Theme.primary` |
  | key | `Theme.textPrimary` |
  | number | `Theme.info` |
  | `true` / `false` / `null` | `Theme.error` |
  | punctuation | `Theme.textSubtle` |
  | line numbers | `Theme.textSubtle` |

- Line numbers in a gutter; the body scrolls (its own `ScrollView`, like the R5
  Body textarea).
- **The code span is `white-space: pre`.** Qt's RichText follows HTML whitespace
  rules: a run of spaces collapses to one and a literal newline to a space, so
  without the `pre` the body renders as one unindented line while the PlainText
  gutter stays multi-line. `white-space: pre` is in Qt's supported HTML subset
  (`richtext-html-subset.html`) and inherits into the nested colour spans, so
  indentation and breaks survive as written. Pinned by a qmltests assertion that
  parses the panel's own text with `QTextDocument` (the engine behind
  `Text.RichText`) and compares it to the exact pretty-printed string.
- `docs/ui-rules.md` gains a **Code** row/case for this mapping (the map's
  decision: "ui-rules gains a 'code' case").

## Decisions

- **Send resolves secrets in C++.** The QML preview keeps masking them; the real
  value is read from Keychain only in `send()`. (R4 + R1.)
- **History stores the resolved URL** (R2/schema), including the params query —
  with secret values **masked** (see the mask decision below).
- **4xx/5xx is a response, not an error** — the pill shows the band, the panel is
  the success layout.
- **Timeout 30 s, redirects followed** (the map's defaults).
- **Status stamp on Send** for a saved request; drafts write history only (R2).
- **⌘S / ⌘↩ stay R7** — R6 ships the button path and the panel.
- **Layout is a fixed vertical split** (decided): request panel on top, response
  panel below, each with its own scroll; no resizer (the design has none).
- **History masks secrets** (decided). The resolved URL can embed a secret (e.g.
  a token in the query); `api/history` is plain JSON in Settings, not Keychain,
  so what is written there is the **masked** URL (`••••••••`), and a replay
  resolves the real value again at send time. The R2 line "history stores the
  resolved URL" is therefore refined to "the resolved URL with secrets masked".

## Tests

- **Panel states** (qmltests, with `FakeApiService` extended with
  `send/cancel/sending/sendFinished`): empty → loading while in flight →
  success (pill band, ms, size, Body/Headers switch, Copy) → error; canceled
  returns to idle without a history entry.
- **Status band mapping** and **size/ms formatting** as small pure helpers.
- **JSON formatter**: pretty-print, token classification, non-JSON passthrough,
  and the rendered result (indentation + line breaks survive the RichText pass,
  height matches the gutter).
- **Wiring**: on `sendFinished` success → `updateRequestStatus` + `appendHistory`
  called with the resolved URL (spy on the fake); draft → history only.
- **Resolver** (C++, if testable without Keychain): `{{var}}` + params join;
  secrets masked/placeholder in the QML preview stay as R4.
- Optional integration: a `QTcpServer` echo reply to exercise the worker thread
  end to end (the R1 abort/queued path). Marked optional — the harness has no
  server today.
- Numbered green (no regression on the R1–R5 suites).

## Verification

1. `cmake --build build -j` clean (app + tests).
2. `qmllint -I qml qml/ApiRequestPage.qml` — 29 warnings, all pre-existing
   `unqualified access`, no new kinds.
3. Startup log: no QML error, no binding loop, no warning.
4. `./qmltests` — 49 passed, 0 failed (R1–R6, plus the R8/R9 suites added on
   this branch).
5. Live: the send path was driven end to end against the user's own stand —
   the motivating saved request answers 200 and the panel renders it
   (user-confirmed; the JSON body now shows indentation, highlighting and line
   numbers). The remaining panel states — empty → loading → error → success,
   Copy, a bad host, Cancel leaving no history row — are pinned by the
   `qmltests` suite, not by a separate manual pass. Two defects found and fixed
   on the way, both pinned by tests:
   - **4xx/5xx classified as a response, not a transport error.** Qt reports
     401 as `AuthenticationRequiredError`; the panel then showed "Request
     failed" for a perfectly good HTTP answer. `send()` now keys `isResponse`
     off `status > 0`.
   - **The JSON body rendered on one unindented line.** `JsonFormat.js` built
     the code with `"\n"`/spaces, which Qt's RichText collapses per HTML
     whitespace rules (§4); the PlainText gutter stayed multi-line, so the
     columns disagreed. Fixed with the `white-space: pre` wrapper and pinned by
     a `QTextDocument`-level assertion — it fails with `{ "a": 1 }` when the
     wrapper is removed.
6. Parity harness not needed (no `qml/Ui/*` or `Theme.qml` edit).

## Out of scope

OAuth and auth beyond the design's Bearer/Basic fields; response tabs beyond
Body / Headers (no raw/preview modes); request-panel collapse; body size limits;
csv/xml pretty-printing; websockets; multipart uploads; ⌘S/⌘↩, save-label flash
and history-replay opens (R7); environment editor (first map, T5).
