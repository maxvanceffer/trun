# R3 — Request page: shell and navigation

Status: closed · Branch: research/t1-patterns

## Goal

Second level inside ApiPage: list ⇄ request page. Open a request from the
list, from history, from a running server, or start a new draft; rename it
inline, see "unsaved changes", save it. Plus a visual pass on the two-level
behavior.

## User corrections (this ticket)

- **No Back button.** "такого паттерна поведения у нас нету". Navigation
  follows the house pattern of DatabasesPage (comment at DatabasesPage.qml
  L599–603): breadcrumb/back lives in the shared top header in Dashboard.qml
  and gets a 3rd segment for the open entity; `Esc` is the keyboard path back.
  No in-page crumb row, no back button.

## Design (mock) vs. scope

The mock's request-page top row is `← Back · API / project / name · unsaved
changes · Save`. After the correction the page keeps only the functional tail:
inline name field, "unsaved changes", Save. The `API / project` crumbs are the
global header's job (it already shows `Dashboard / API`; a 3rd segment with the
request name appears while a request is open, and the `API` crumb clicks back).

## Scope

### Navigation and entry points (ApiPage.qml)

- Two-level slide swap (DatabasesPage pattern, no StackLayout): one pane holds
  the current list content (header row + search + list + env card), the other
  holds the request page; x/opacity Behaviors.
- Entry points all open the request page:
  - list row click → open a deep clone of that saved request
  - history row click → new draft with the entry's `url` + `method`
  - running-server `Open →` → new draft with its url
  - `+ New request` (top row + empty plate) → new draft
- `createRequest()` (T4, persisted via `newUntitled`) is replaced by
  `openDraft()/openSavedRequest()/openHistoryEntry()/closeRequest()`; the
  working request is `requestMap` (null = list). `newUntitled` is removed from
  ApiService.
- `Esc` pops back to the list (page-level `Keys.onPressed`, same place as
  DatabasesPage). Leaving with unsaved edits is a silent discard (decided).

### Request page (`qml/ApiRequestPage.qml`, new)

- Breadcrumb tail row: inline name field (UiInput, new Nuxt-backed variant),
  spacer, "unsaved changes" hint, Save button.
- The working request is shared by reference with ApiPage; the page mutates it
  and tracks `dirty`. A draft is "always unsaved" (`saved` flag, not the id, so
  the label flips the moment a Save succeeds — mutating a QML var's field does
  not re-evaluate a binding).
- Save button label per state: `Saved ✓` flash (1.6 s) after a save, `Save…`
  for a draft, `Save` for a saved request.
- Save dialog (pulled from R7 so the button is real): name suggested by
  `suggestName(method, url)` when the name is still "Untitled request", project
  select (UiSelectMenu, first project preselected, `UNSAVED` fallback), preview
  row `method + url`, Save disabled until a name is entered, Cancel/Esc.
  Save = `saveRequest(requestMap)`; ApiService returns the id now so the page
  can bind the working map to the persisted row.
- R4+ fills the body (URL bar, tabs, response) — the page ships with just the
  top row + empty body.

### Shared header (Dashboard.qml)

- `API` crumb dims and becomes clickable while a request is open.
- A 3rd segment `API / <name>` is appended (mirrors the Databases 3rd segment).

### UiInput variant (component edit, Nuxt-backed)

- `variant: outline | ghost | none` from `/tmp/nuxt-ui/src/theme/input.ts`
  (outline current behavior; ghost = transparent + hover/focus bg-elevated +
  focus ring; none = fully bare). The name field uses `ghost` — the mock's
  hover-border on the name has no Nuxt counterpart (none/ghost only tint the
  bg), so the affordance is the Nuxt one; flag for the visual pass.

### Storage

- `saveRequest()` returns `QString id` ("" on failure) so the page can adopt
  the persisted row's id. `newUntitled()` removed (no callers after R3).

### Tests

- FakeApiService: drop `newUntitled`, add `makeDraft`/`saveRequest`(returns
  id)/`suggestName`.
- Fix `test_api_import_curl` (newUntitled block removed, 5 → 3 requests) and
  the `saveRequest` bool assertions (→ `!isEmpty()`).
- New `test_api_page_open_request`: openDraft / openSavedRequest (clone does
  not touch the C++ list) / openHistoryEntry (method+url carried) /
  closeRequest (silent discard).

## Out of scope

URL bar, methods/params/headers/body/auth tabs, Send, response panel, ⌘S/⌘↩,
append-to-history on send, replay-open (R4–R7).

## Verification

1. `cmake --build build -j 8` clean.
2. `qmllint` on the touched QML.
3. Startup log: no error / binding loop.
4. `./qmltests` — all green (existing + new).
5. Launch app: open a request from list / history / Open → / + New request;
   rename; Save (flash, "Saved ✓"); API crumb + Esc back; unsaved edits
   discarded silently.