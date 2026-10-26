# R7 — Save, shortcuts, history

Status: closed · Map: [api-request-page-map.md](api-request-page-map.md)
Blocked-by: R3 (shell, closed), R6 (Send, closed)

## Goal

One loop for getting a request out of the page and back: Save writes the working
request into its collection (dialog for a draft, direct write for a saved
request), the button's label says which of the three states it is in, ⌘S saves
without the mouse, ⌘↩ sends without the mouse, every Send appends to history and
a history row reopens the request.

## Scope

### 1. Save dialog — shipped with R3 (pulled forward)

`qml/components/SaveRequestDialog.qml`: suggested name (`METHOD /path` from the
C++ `suggestName`), project select (first workspace project by default), method
and URL preview, Enter saves, Esc cancels, Save disabled until the name is
non-empty. Opened by the Save button and by Enter in the name field, never by a
shortcut for a saved request.

### 2. Save-label states — shipped with R3

`saveLabel()` in `qml/ApiRequestPage.qml` reads `saved` / `dirty` / `savedFlash`:
"Save…" (draft), "Save" (saved, dirty), "Saved ✓" (flash after a successful
write, `flashTimer`). One-shot flash instead of a toast: the page's own button
is where the user is looking.

### 3. Shortcuts — closed here

`qml/ApiRequestPage.qml`:

- ⌘S → `save()` (not `doSave()`): the same route as the button, so a draft opens
  the dialog and a saved request writes straight through.
- ⌘↩ → `startSend()`.
- Two `Shortcut` objects with **`sequences:`**, not `sequence:` — `StandardKey.Save`
  maps to several bindings on macOS and the singular property logs "Only binding
  to one of multiple key bindings associated with 5". ⌘↩ is `["Ctrl+Return",
  "Ctrl+Enter"]` (`Ctrl` is Command on macOS, and the numeric keypad sends Enter).
- Both are window-context shortcuts, so both are gated twice: `pageActive`
  (handed down by `ApiPage` from the Dashboard — the actions must not fire from
  the Docker page while a request stays open) and an open request
  (`!!requestMap`). ⌘↩ also stands down while a send is in flight — Cancel owns
  that moment.
- ⌘S is not gated on `dirty`: re-saving a clean request is a harmless no-op write
  and makes the shortcut feel like the button.

### 4. History append + replay — shipped with R6

`onSendResult` → `appendHistory` with the **resolved** URL (what was actually
sent, secrets aside), cap 50, newest first; a history row opens the request page
with the recorded method/URL as an unsaved draft. A cancel writes nothing, a
network error writes nothing (see the R6 decision in the map).

## Verification

- `test_api_request_urlbar` (qmltests): both shortcuts exist, are disabled while
  the Dashboard has another page active, enable when `pageActive` flips, ⌘S runs
  the save path (dirty → clean), ⌘↩ starts a send and is disabled while it runs,
  and closing the request disables both.
- Suite: 49/49 green. App startup log after `killall trun` + relaunch: no QML
  errors or warnings.
- Visual pass of the whole request page (checkbox under the resolved line,
  shortcut hint under Send) pending in the live app.
