# T2 — Data schema: requests / environments / history (task, AFK)

Map: docs/wayfinder/api-saved-requests-map.md
Label: wayfinder:task
Status: closed — resolved, spec in docs/wayfinder/schema-api.md
Blocked-by: T1

## Question

Pin down the JSON schema in Settings + Keychain keys: Saved request {id, name, method, url, projectId, headers?, updatedAt, lastStatus?, lastAt?}, Environment {id, name, vars[{name, value?, secret, enabled}]}, History entry {method, url, status, at}. Active env id. Limits: history 50. What do we persist, what stays in memory only?

## Constraints

- Settings + Keychain (round-1 decision), environments global, projectId = trun Project id.
- History writer (appendHistory) — stub in ApiService, the real Send comes later.
