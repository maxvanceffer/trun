# R8 — Per-request TLS verification off (curl -k)

Status: closed · Map: [api-request-page-map.md](api-request-page-map.md)
Blocked-by: R4 (URL bar, closed), R6 (Send, closed)

## Goal

Let one request opt out of TLS certificate verification, the way `curl -k` /
`curl --insecure` does: self-signed or otherwise untrusted certs are accepted for
that request only. The switch is **per saved request** (a field in the request
map, persisted with it), never a global app setting — a hidden global "trust
everything" switch is exactly the footgun this feature is supposed to avoid.

## Scope

### 1. Schema — a new boolean field

`schema-api.md` / `api-saved-requests-map.md`: the Saved request gains

```json
"insecure": false
```

- `makeDraftImpl()` seeds `insecure: false`; `loadRequests()` needs no change
  (it keeps unknown/extra fields, and the QML side reads the field defensively
  — `=== true`).
- The field follows the request through Save / the detached clone
  (`openSavedRequest` deep-copies) — nothing to do there.

### 2. QML — the switch

`qml/ApiRequestPage.qml`, URL bar:

- A `UiCheckBox` (the kit's checkbox; no new component) directly under the
  `→ {{ resolved }}` line:
  `label: qsTr("Disable SSL verification")`.
- A **page-level mirror** `property bool insecure`, the way `bodyMode`/`authType`
  already mirror their map fields: `requestMap` is a QML-var object and mutating
  one of its fields never re-evaluates a binding, so `checked: reqRoot.insecure`
  is the only form that keeps the box truthful (the first draft bound `checked`
  straight to `requestMap.insecure === true` — the flag would have been written
  on click while the box stayed visually unticked). `openWith()` seeds the mirror
  from the map, so a reopened saved request comes back ticked.
- `onToggled: on => { insecure = on; requestMap.insecure = on; dirty = true }` —
  the state is owned by the page, the toggle comes back through the `toggled`
  signal (the component's contract: never assign to `checked` from inside).
- `enabled: !reqRoot.sending` — same lock as the URL field.
- `sendSpec()` adds `insecure: reqRoot.insecure` (the live mirror; the KV sync
  does not touch this field) so the flag reaches C++.
- No width constraint: Nuxt UI's checkbox root is a block-level flex row
  (`src/theme/checkbox.ts` — `root: 'relative flex items-start'`, `wrapper:
  'w-full'`), its label is the click target, and the row therefore spans the
  column in the source as well. Left alignment falls out of the layout — the
  same left edge as the URL field and the resolved line above it.

Visible for any URL (http included): it is a per-request transport switch, the
way Postman/Insomnia show theirs; hiding it by scheme would make it appear and
disappear while typing.

### 3. C++ — the transport

`src/apiservice.cpp` (the R6 send slice, unchanged shape otherwise):

- `resolveRequest()` passes the flag through:
  `out.insert("insecure", spec.value("insecure").toBool())`.
- In the worker (throwaway `QNAM` + `QEventLoop`), before sending, when the flag
  is set:
  - `QSslConfiguration cfg = QSslConfiguration::defaultConfiguration();
    cfg.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(cfg);` — the handshake stops checking the chain
    (this is the `curl -k` behaviour; a *certificate* requirement can stay);
  - plus a connection
    `QObject::connect(&nam, &QNetworkAccessManager::sslErrors,
    [](QNetworkReply *r, const QList<QSslError> &) { r->ignoreSslErrors(); })`
    as the backstop for a reply that still reports errors (notably a redirect:
    Qt builds a fresh request for the redirect and the per-request SSL
    configuration is not carried over).
- No other reply handling changes: a TLS failure keeps travelling as a network
  error (`SslHandshakeFailedError` → `errorString()`), so with the box unticked
  a self-signed host still reports an error, as today.

### 4. curl import (`-k` / `--insecure`)

`importCurl()` used to list `insecure` in `kLongBoolFlags` and drop it (and `-k`
was dropped with the other boolean short flags). Now that the request has
somewhere to put it, a pasted `curl -k …` sets the flag instead of silently
losing the intent:

- `insecure` leaves `kLongBoolFlags` for an explicit branch (`--insecure` has no
  value form, so it is intercepted before the drop-everything list);
- `bool insecure` is tracked in the import loop — the long form and the `k` short
  flag, including a combined run such as `-skS`;
- the request is returned with `if (insecure) request.insert("insecure", true)` —
  a conditional write so the draft's own default (`false`) stays the single
  source of the off value.

## Decisions

- **Per request, persisted** (not global, not per environment): the flag is part
  of the request map, so a saved request remembers it and history replay does
  not resurrect it.
- **`VerifyNone` on the request, `sslErrors` backstop on the manager** — the
  first is the actual `curl -k` semantics, the second covers Qt's redirect
  path where the request is rebuilt.
- **No new Theme token, no new component** — a `UiCheckBox` with the standard
  text token, so `ui-rules.md` needs no row.
- **No "insecure" badge in the response panel** (would be nice, but it is a new
  visual decision — ask first).

## Tests

Committed (in `src/qmltests.cpp`, all in existing slots — no new header slot):

- **curl import** (`test_api_import_curl`): `curl -k …`, `curl -skS …` and
  `curl --insecure …` each land on the request as `insecure: true`; the first
  command (no flag) and a plain `curl https://plain.test/…` stay `false`, and
  both survive a Settings reload.
- **Draft shape** (`test_api_draft_save`): the draft *contains* `insecure`
  (a `contains` guard, so a misspelled key cannot pass by reading back as
  `false`) and it is `false`; the round-tripped request keeps it.
- **URL bar** (`test_api_request_urlbar`): the box starts unticked and the first
  send carries `insecure: false`; the toggle writes the page mirror *and*
  `requestMap.insecure`, marks the request dirty and flips the box; the next
  send carries `true`; the box is locked while sending. Reopening a request with
  the field ticked comes back ticked and clean, reopening one without it comes
  back unticked — the two directions together pin down the `openWith` seeding.
- **`FakeApiService::makeDraft`** mirrors the real draft, so the fake cannot
  drift from the shipped shape.

Each new assertion was checked in both directions (temporarily breaking the
producer and watching the test fail), the project's habit here.

Manual, not committed: **end to end over real TLS.** A local HTTPS server
(`openssl req -x509` self-signed cert for `127.0.0.1`, `python3` `ssl` +
`http.server` on `:8443`) was driven through the real `ApiService::send()`:

```text
insecure=0 ok=0 status=0 error=SSL handshake failed: The certificate is self-signed, and untrusted
insecure=1 ok=1 status=200 error= body=hello-from-tls
```

i.e. exactly `curl` versus `curl -k`. The flag was then removed from
`resolveRequest()` on purpose and the run repeated: both sends failed, which
proves the C++ pass-through is what carries the behaviour. Kept manual because a
committed version needs a cert fixture / `openssl` / a server process inside a
suite that today is pure in-process Qt (the R6 ticket left this class of
integration test optional); promoting it is a separate decision.

## Verification

1. `cmake --build build -j 8` clean (app + tests).
2. `qmllint -I qml qml/ApiRequestPage.qml` — 29 warnings, all pre-existing
   `unqualified access`, no new kinds.
3. Startup log: no QML error, no binding loop, no warning.
4. `./qmltests` — 48 passed, 0 failed.
5. Live TLS (above), both directions, plus the geometry check (screenshots are
   not readable in this session, so the layout was measured numerically):
   URL field, resolved line and checkbox share `x=308.2`; the checkbox sits at
   `y=113` against the resolved line's `bottom=109`; it ends at `131` against the
   tabs at `155` (no overlap) and the response panel still gets
   `378px` ending at `784` inside a `800px` page.
6. Parity harness not needed (no `qml/Ui/*` or `Theme.qml` edit — `UiCheckBox`
   is used as is).

## Out of scope

- Custom CA / client certificates (`--cacert`, `--cert`, `--key` — parsed and
  dropped today; they would need a file-picker UI).
- A global "trust all" preference.
- An "insecure" indicator on the response/status area.
- Per-environment TLS policy.
