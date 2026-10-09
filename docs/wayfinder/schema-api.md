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
  "body": "",
  "auth": {"type": "none"},
  "lastStatus": 200,
  "lastAt": "2026-10-06T12:00:00Z",
  "updatedAt": "2026-10-06T12:00:00Z"
}
```

- `headers/body/auth` — reserved for the request page (out of scope); the T6 cURL
  parser already writes them, there is no UI yet.
- `lastStatus/lastAt` — the source of the status/time in the list row (closes the
  "Not yet specified" question: no join with history is needed).
- `method` — uppercase; `url` — raw, with `{{var}}` as is.

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
- Writer `appendHistory()` — a stub in the future ApiService; the real Send (second
  page) will write it later.

## ID and time

- id — `QUuid::createUuid().toString(QUuid::WithoutBraces)`.
- Time — ISO UTC (`Qt::ISODate`), relative labels via the existing `timeAgo()`.
