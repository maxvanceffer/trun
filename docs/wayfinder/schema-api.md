# API client — data schema (T2, resolved)

The storage pattern is the existing one: compact JSON as a string via
`Settings.get/set` (`src/projectservice.cpp:105-124`), secrets only in the native
Keychain (`src/mackeychain.h`, label like `trun.db.*` in
`src/databaseservice_macos.cpp:250-295`).

## Settings keys (all compact JSON)

- `api/requests` — array of Saved request
- `api/envs` — array of Environment
- `api/activeEnv` — string, id of the active env ("" = none)
- `api/history` — array of History entry, cap 50 (trim on append, oldest at the end)

## Saved request

```json
{
  "id": "9f3a… (QUuid without braces)",
  "name": "Get campaign",
  "method": "GET",
  "url": "{{baseUrl}}/api/campaigns/{{campaignId}}",
  "projectId": "<folder>/<manifest> (trun Project id)",
  "headers": [{"name": "", "value": "", "enabled": true}],
  "params": [{"name": "", "value": "", "enabled": true}],
  "body": "",
  "auth": {"type": "none"},
  "insecure": false,
  "lastStatus": 200,
  "lastAt": "2026-10-06T12:00:00Z",
  "updatedAt": "2026-10-06T12:00:00Z"
}
```

- `headers/body/auth/params` — the request page's editors (second map); the T6
  cURL parser already writes headers/body/auth, `params` lands in R2. The mock's
  JS tuples (`['include','stats',true]`) are not the persisted format — the
  stored shape is the same map as headers.
- `lastStatus/lastAt` — the source of the status/time in the list row (no join
  with history). Send stamps them on the stored request immediately (R2
  decision: the list shows the last execution even before Save); content edits
  persist only on Save.
- `method` — uppercase; `url` — raw, with `{{var}}` as is.
- `insecure` — per-request TLS switch (R8), false unless the request opted out of
  certificate verification the way `curl -k` does. It is a plain stored field:
  `makeDraft` seeds `false`, the curl importer writes `true` for `-k` /
  `--insecure`, and the send worker reads it (`QSslSocket::VerifyNone`); unknown
  fields survive `loadRequests()`, so a request saved before R8 keeps working and
  reads as `false`.

## Environment (global, one active)

```json
{
  "id": "local",
  "name": "local",
  "vars": [
    {"name": "baseUrl", "value": "http://localhost:8000", "secret": false, "enabled": true},
    {"name": "token", "value": "", "secret": true, "enabled": true, "hasSavedValue": true}
  ]
}
```

- Seed on first open: env `local` with `baseUrl=http://localhost:8000` (matches
  the design), `staging` empty.
- Secret: the value is NOT in Settings (`value:""` + `hasSavedValue`). Keychain:
  service `trun.api.<envId>`, account = variable name (like `trun.db.<engine>` +
  user).
- Linux: `databaseservice_linux.cpp:39-46` — Keychain stubs; for the API the
  fallback is to store the secret in Settings alongside (flagged, to revisit in
  T5: dev secrets of a local tool vs the strictness of the DB pattern).

## History entry (read-only + Clear in the first map)

```json
{"method": "GET", "url": "http://localhost:8000/api/campaigns?page=2",
 "status": 200, "at": "2026-10-06T12:00:00Z"}
```

- Persisted in Settings (needed for "2m ago" after a restart).
- Reader + `clearHistory()` are done in T4 (the list reads history and clears it).
- Writer `appendHistory()` (R2): prepends (newest on top, as the design), stamps
  `at` when absent, trims to cap 50 (oldest at the end), persists. `url` is the
  **resolved** URL — what was actually sent — so history rows and replay repeat
  the real network call (decided in the request-page map).

## ID and time

- id — `QUuid::createUuid().toString(QUuid::WithoutBraces)`.
- Time — ISO UTC (`Qt::ISODate`), relative labels via the existing `timeAgo()`.
