# R9 — `{{var}}` substitution beyond the URL (params, headers, auth, body)

Status: closed · Map: [api-request-page-map.md](api-request-page-map.md)
Blocked-by: R6 (Send, closed)

## Goal

`{{var}}` from the active environment resolves everywhere a request carries
text, not only in the URL: query-param values, header values, the auth block
(Bearer token, Basic username/password) and the body. A user who keeps a token
in an environment variable (the R5/R6 habit: a secret var + the Auth tab) can
write `{{token}}` in the Token field — or in an `Authorization` header row, or
in a JSON body — and the wire carries the real value, the way Postman and
Insomnia do.

Today `resolveRequest()` substitutes only inside the URL; every other field is
snapshotted verbatim, so `Bearer {{token}}` goes out literally and the server
answers 401. This is not hypothetical: the saved request that motivated this
spec held the real token in the Auth tab and a stale literal
`Authorization: Bearer <токен>` row in the Headers tab — and neither side
resolved anything.

## Scope

### 1. Resolver — one substitution pass, every text field

`src/apiservice.cpp`, `resolveRequest()`:

- The existing var loop stops being URL-only. It still resolves **lazily per
  field** — a secret's Keychain entry is read only when the field being
  resolved actually contains that placeholder — and applies the
  placeholder → value map to:
  - each **enabled param's value** (param names stay literal — a name is an
    identifier);
  - each **enabled header's value** (header names stay literal; the
    empty-name skip is unchanged);
  - the **auth block**: `token` (Bearer), `username`/`password` (Basic) —
    Basic keeps base64-ing the resolved pair;
  - the **body** (all body modes);
  - the URL, as today.
- Unresolved stays unresolved: a name that is not in the active environment,
  an unset secret, or a secret whose Keychain entry is gone leaves the
  `{{name}}` placeholder in place — the same three cases as the URL today.
- Replacement is plain literal substitution in variable-list order, exactly
  the URL loop's semantics — no escaping, no promise beyond that.

Excluded from substitution: the method, header/param names, the request name.

### 2. QML — no change; preview contract unchanged

`resolvedUrl()` keeps resolving the URL only (its secret-masking contract is
R4's). Header/token/body fields are drafts — they show exactly what the user
typed (`{{token}}` stays visible); the wire resolves in C++. The
"Mirrors qml/ApiRequestPage.qml resolvedUrl()" comment on `resolveRequest()`
is narrowed to the URL line so it stops implying preview parity that only the
URL has.

### 3. Testability

`resolveRequest()` becomes `Q_INVOKABLE` (public const) so the suite can call
it directly. No committed test covers it today — the R6 spec left
"Resolver (C++, if testable without Keychain)" open; this spec closes it, and
every assertion below runs without touching Keychain.

## Decisions

- **An explicit `Authorization` header row still wins over the Auth tab**
  (`hasAuth` precedence stays): fill both, get the header — Postman behaves
  the same way, and the user resolves the conflict by deleting the row. R9
  fixes resolution, not precedence; a merge/warning would be a separate UX
  decision.
- **No new previews**: no resolved-header/token line under the fields. Adding
  one is a visual decision (a `ui-rules.md` row + a design pass) — ask first;
  the URL line exists because R4 made it exist.
- **Body included**: `{{campaignId}}` in a JSON payload is the same need as
  in the URL. The literal-`{{…}}` collision (a template engine's payload) is
  accepted for the same reason it is accepted in URLs today — the escape
  hatch is not to write placeholders there.
- **Lazy Keychain unchanged, widened**: a request that uses no secret costs
  no Keychain read; now that holds per field, not just the URL.
- **No new secret surface**: history and every QML preview carry only the
  (masked) URL; resolved header/auth/body values travel to the wire, never to
  storage.

## Tests

New slot `test_api_resolve_vars` (`src/qmltests.h` + `src/qmltests.cpp`),
driving the now-invokable `resolveRequest()` against an env with a non-secret
var and a secret var:

- a header value `Bearer {{name}}` (non-secret) resolves; an unset secret
  keeps the placeholder; a secret marked `hasSavedValue` whose Keychain entry
  does not exist keeps it too (deterministic — the lookup misses and the
  value stays empty);
- the Bearer token, Basic username and Basic password resolve / keep the
  placeholder the same way, and Basic still base64s the *resolved* pair;
- a param value resolves, a param name does not;
- the body resolves;
- the URL keeps resolving as before (R6 regression guard);
- headers win over the auth block, with both sides resolved;
- a var absent from the active environment stays a placeholder in every
  field.

Existing slots stay untouched: 48 → 49 passed.

## Verification

1. `cmake --build build -j` clean (app + tests).
2. `./qmltests` — 49 passed, 0 failed.
3. `qmllint -I qml qml/ApiRequestPage.qml` — no new warning kinds.
4. Live, manual (the R6/R8 habit): the motivating saved request with the
   stale header row deleted answers 200 against the user's own stand —
   user-confirmed. The `{{token}}`-from-env form of the same path is pinned
   by `test_api_resolve_vars` (a secret var is the secret branch of the same
   resolver).

## Out of scope

- Substitution in the method, header/param names, the request name.
- Preview/masking UI for resolved headers, auth and body values.
- Per-var transformations (`{{token | base64}}`) and nested/defaulting
  syntax (`{{token ?? fallback}}`).
- Resolving `{{…}}` inside the curl importer itself — it already hands the
  values to the resolver as ordinary fields.
