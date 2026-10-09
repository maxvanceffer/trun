# T5 — Environment card + full dialog (task, HITL)

Map: docs/wayfinder/api-saved-requests-map.md
Label: wayfinder:task
Status: closed — resolved

## Resolution

Done, build green, app starts without QML errors or binding loops.
- C++: new src/apiservice.h/.cpp (environments only, no network), apiService
  registered in main.cpp, lazy ensureLoaded() from ApiPage. Secrets: macOS
  Keychain `trun.api.<envId>` + variable name; Linux fallback — value inline in
  Settings.
- QML: ENVIRONMENT card on the right in ApiPage (UiSegmented switch, {{name}}
  amber mono, secrets as dots, hint) + EnvironmentsDialog (AppDialog shell: env
  list on the left, variables on/off+name+value+Secret on the right,
  New/Duplicate/Delete with ConfirmDialog, Save applies; env name via a field at
  the top, which was not in the spec text — without it New is meaningless).
- Verified: the local/staging seed was written to real Settings at startup; the
  secrets round-trip through Keychain (save → independent read matched → record
  deleted, Keychain clean); getters are clean (binding loop fixed).
- Visual pass over the card/dialog done (Edit, Duplicate/Delete, Secret, Add
  variable, dark theme) — no discrepancies.
- Fidelity fixes per the screenshot: Edit → ghost (no outline); UiSegmented got
  an optional stretch (segments split the width in half, off by default);
  {{name}} amber mono Sm DemiBold; the hint is a single StyledText with amber
  {{baseUrl}} + a hairline separator above.
- Dialog rebuilt to the design: the env list on the left is gone; a Name row +
  New/Duplicate/Delete; VARIABLE/VALUE/SECRET headers; SECRET — a new kit
  component AppSwitch 30×17 (track mutedSurface/accent, handle textMuted/accentInk,
  hover/focus/animations 120ms; `enabled` renamed to `interactive` to avoid
  shadowing warnings); + Add variable — UiButton outline (no dashed style in the
  kit); Show/Hide removed; the subtitle and the Keychain footnote come from the
  design. Theme: accentInk + mutedSurfaceHover.
- Restored the env-list sidebar on the left: New/Duplicate/Delete moved to the
  Name row on the right, the rest of the layout untouched.
- Row-width fix: UiCheckBox with an empty label keeps a minimum of 56px and broke
  the layout (+32px past the edges); the cell is clamped to min/max 24. The 1/3+2/3
  ratio is now computed from the dialog's fixed width (no feedback loop),
  verified in an offscreen harness on the real dialog: rows 505 = 24+125+252+52+20.
- Bug fix: on macOS saveEnvironments wiped the values of ALL variables (wrote ""
  for non-secrets too); now the inline value is preserved, "" only for secrets.
  Verified by a rename-preserves-value test. Note: a baseUrl value wiped by the
  earlier save does not restore itself — retype it once.
- Bug fix: variable edits in the dialog were not saved — the Repeater hands the
  delegate a copy (`modelData`), writes to it never reached draftEnvs, so doSave
  saw the old/empty name and dropped the row. All row edits now go through
  index-based setters into the source object (setVarName/Value/Enabled/Secret);
  text edits without touch() (the row is not rebuilt → focus preserved),
  structural ones (add/delete/Secret/on-off) with touch() so the `checked`/`text`
  bindings re-read the snapshot. Verified in an offscreen harness on the real
  dialog: rename+value+new row reach saveEnvironments, Secret goes into the
  secrets map, deletion applies, rebuild-while-typing = false.
- Name-row fidelity per pixel measurements of the design: around the buttons is
  exactly the dialog background → they are ghost buttons, Duplicate text
  (250,250,250) = secondaryForeground, Delete (255,100,103) = destructive (exact
  token match). New removed from the Name row (the Name row has none in the
  design), Duplicate/Delete → variant ghost, size sm; the input is fixed at 269
  (= 358 design ×0.75) and the buttons are pushed right, gap 60.
  Incidentally (agreed) the right-column width was fixed: the footnote with an
  implicit ≈807 inflated it to 534 — the sidebar shrank to 121, and the table rows
  (505) fell short of the edge (this was the old "row not full width" bug).
  Added Layout.maximumWidth: rightColW → column 505, sidebar 150, rows and headers
  flush. Measured in the app: inputW=269 gap=60 delRight=505 rowFit=true
  dupFill=#00000000 dupFg=#fafafa delFg=#ff6467, binding loops 0, TEMP dump removed.
- Create-environment entry point (the deferred T5 step): the design does have the
  button — a dashed `+ New environment` pill under the env list. Added as a
  `UiButton` outline secondary sm (`block`, full width), calling `addEnv()`; a new
  env gets its id on Save. The earlier "no New" only concerned the Name row.
Blocked-by: T2

## Question

ENVIRONMENT card on the right (local/staging switch via UiSegmented, {{name}}
variables → value/secret as dots, Edit) + a full dialog: env list on the left,
variables on the right (on/off, name, value, Secret), New/Duplicate/Delete, Save
applies. Secrets in Keychain. Round-1 decision: the full dialog right away, not a
stub.

## Constraints

- Reference: qml/ConnectionDialog.qml (Keychain pattern) + AppDialog.
