# ADR-002: Shared list element (UiListElement)

**Status**: Implemented
**Date**: 2026-10-09
**Context**: Every list drew its own "row card" with separate markup: Docker
(containers/images), Databases (services), API (requests and history). In the API
list, hover was a row fill inside a shared card; `clip: true` clips to a
rectangle, not to the radius, so a fill on the edge rows squared over the card's
rounded corners. User: extract a single list element into
`qml/Ui/UiListElement.qml` modeled on Databases and reuse it, fixing hover along
the way.

## Decisions

### 1. A single card component — `qml/Ui/UiListElement.qml`
- A pure container: `radiusMd` (12), `Theme.cardBackground`, a 1px border, a
  hover border, a content slot (`default property`), a `clicked()` signal, and
  `interactive` / `clickable` flags.
- Nuxt UI rationale: the surface is the `soft` variant of the Card component
  (`src/runtime/components/Card.vue`, `src/theme/card.ts`: `bg-elevated/50`) →
  `Theme.cardBackground`. Nuxt's Card has no hover border; this is an app
  convention on top of the same surface.
- "Clean, data-free": the component knows nothing about data; a page puts its own
  content inside and sets its own margins (as Databases/Docker used to set
  `anchors.margins`).

### 2. Hover is a border, not a fill
- `border.color` transitions `Theme.border → Theme.ring` over `Theme.animHover`.
  Rounded corners are preserved by construction; a square-cornered fill covering
  the radius is ruled out.

### 3. One card per row
- The API list and history were rebuilt from "one card per group + inner rows" to
  "one card = one row" with an 8px gap. Groups remain as titles. The dashed
  HISTORY box was removed.
- A deliberate divergence from the mock (there the rows sit in one card): a single
  card look was chosen, as in Databases/Docker.

### 4. Radius
- `radiusMd` (12) everywhere; the API list was brought from 8 to 12 for
  consistency.

### 5. Click and click-through
- A click on empty chrome emits `clicked()`; clicks on buttons inside the card are
  intercepted by the buttons (instrumented test
  `test_ui_list_element_click_through`).

## Scope
- Migrated: `DatabasesPage.serviceCard`, `DockerPage.containerCard` +
  `imageCard`, API request/history rows.
- Not migrated: the other cards (the API env card, Databases connection-info/steps,
  the empty plate, dialog blocks).

## Verification
- `qmltests`: `test_ui_list_element_click_through` (36/36).
- Harness `qml/tests/run_uilistelement.sh` (+ `--dark`): 14/14 in both themes
  (rounding, fill, border → ring on hover).
- `UiListElement` added to `QML_FILES` in `CMakeLists.txt` — otherwise the build
  did not track the edits and the type did not compile into the module.

## Files
- `qml/Ui/UiListElement.qml` — the component
- `qml/ApiPage.qml`, `qml/DatabasesPage.qml`, `qml/DockerPage.qml` — usage
- `qml/tests/tst_uilistelement.qml`, `qml/tests/verify_uilistelement.py`,
  `qml/tests/run_uilistelement.sh` — the harness
- `CMakeLists.txt` — `QML_FILES`
- `docs/adr/002-shared-list-element.md` — this document
