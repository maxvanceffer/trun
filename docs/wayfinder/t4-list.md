# T4 — Saved requests list: groups, search, rows, history (prototype, HITL)

Map: docs/wayfinder/api-saved-requests-map.md
Label: wayfinder:prototype
Status: closed — resolved (list/history on the shared UiListElement, visual pass done)
Blocked-by: T2, T3

## Question

List prototype per screen 1 of the design: search (UiInput), PROJECT count groups, rows method-color/name/url/status-dot/time, history unsaved·last 50 + Clear, buttons Import cURL + New request. UiButton/UiInput/UiTag/UiSegmented/StatCard only. The user's fidelity reaction — what doesn't match?

## Assets

- Screen 1 (list), screen 2 (empty state) — in the original message.

## Resolution

Done, build green, 35/35 qmltests (added `test_api_history_clear` and
`test_api_page_list_groups`), startup 4 lines / 0 QML errors, qmllint clean
(only the two pre-existing unqualified warnings in the env Repeater), no hex
values in `ApiPage.qml`.

- C++ (`src/apiservice.h/.cpp`): history slice — `Q_PROPERTY history`,
  `loadHistory()`/`persistHistory()` under the `api/history` key, `clearHistory()`
  (empty → true without writing; otherwise clears + persists + `historyChanged`).
  The lazy `ensureLoaded()` reads history along with requests/envs. The writer
  (`appendHistory`) was deliberately not added — the second page's Send will
  write it; until then history is always empty.
- QML (`qml/ApiPage.qml`): root is a `ColumnLayout` (16) of two rows.
  - Top row (visible on both the list and the empty state — as in the design):
    UiInput 280 with a leading `filter` icon + spacer + `Import cURL` (neutral
    outline, sm) + `+ New request` (primary solid, sm).
  - Bottom row: list on the left, the existing 300-wide environment card on the right.
  - List (ScrollView, its own scroll so the card stays put):
    - groups by `projectId` (first-seen order; empty id → `UNSAVED` group — as in
      the design); title = project name from `projectService.projects()` in
      UPPERCASE, count, hairline; groups with no search matches drop out;
    - row: one `UiListElement` card per request (gap 8), method (Menlo 11 700,
      color by verb), name 13 DemiBold + url Menlo 11 muted, status 56 (dot 8 +
      code Menlo 11), time 58 right-aligned (`dashboard.timeAgo(lastAt)`),
      hover — `ring` border (ADR-002);
    - HISTORY: title + `unsaved · last 50` + hairline + `Clear`
      (UiButton link neutral xs, disabled while empty), below — one
      `UiListElement` card per entry (the dashed box was removed in ADR-002).
- Colors (per the design, on Theme tokens): method GET→success, POST→primary,
  PUT/PATCH→info, DELETE→error, other→textDimmed; status ≥500→error, ≥400→warning,
  else success.
- User decisions: (1) HISTORY is shown as an empty dashed box, `Clear` disabled;
  (2) an unsent request (`lastStatus 0`) — an em dash `—` and a neutral
  `textSubtle` dot, empty time.
- Search — by name + url (as in the design: `(r.name + r.url).toLowerCase()`).
- Shared list element: request and history rows, Docker (container/image) and
  Databases (service) all moved to `qml/Ui/UiListElement.qml` — a card with a
  hover ring; decisions and verification — `docs/adr/002-shared-list-element.md`.

## Open — awaiting fidelity reaction

- The `+ New request` / `Import cURL` buttons are duplicated: the top row and the
  empty-state plate (in the design the top row is unconditional and the plate
  repeats the buttons — the repeat is shown, but may be redundant).
- `Clear` — UiButton `variant: link` (xs, height 24), so the HISTORY title row is
  taller than the group titles (11): the row height is set by the button.
- The env card is 300 vs 280 in the design — the earlier T5 value was kept.
- POST → `Theme.primary` (the app accent) — the design uses amber accent
  `#f0b100`; an alternative is `Theme.warning`/`secondary` (same hex in light).
- UPPERCASE group titles and the `UNSAVED` group — the design mapped onto our
  data (`projectId` instead of a project row).
- The T6 discrepancy remains: `+ New request` / `Open →` immediately write the
  request to `api/requests` (in the design it is a draft until Save). There is no
  Save page, so a draft cannot be implemented; revisit with the second page.
- Clicking a row does not navigate (no second page) — hover only.
