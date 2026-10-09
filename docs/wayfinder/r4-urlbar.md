# R4 — Request page: URL bar

Status: closed · Branch: research/t1-patterns

## Goal

The request bar under the top row: method select (coloured trigger), mono URL
field, environment switch, Send/Cancel with a loading state, and the
`→ {{ resolved }}` preview line with active-environment substitution.

## Scope

### ApiRequestPage.qml (URL bar)

- Method select: UiSelectMenu over the design's methodOpts
  (GET/POST/PUT/PATCH/DELETE); the trigger is tinted with the method colour
  (GET→success, POST→primary, PUT/PATCH→info, DELETE→error).
- URL field: UiInput, mono (`Theme.fontMono`); editing marks dirty and
  refreshes the preview live.
- Environment switch: UiSelectMenu over `ApiService.environments`; selects the
  app-wide active environment — the same one the Dashboard env card shows, so
  both follow `setActiveEnv`.
- Send ⇄ Cancel: while `sending` the bar locks (method/URL/env disabled), the
  Send button turns its own Nuxt `loading` spinner on inside (and is disabled),
  and a separate Cancel button appears beside it. UI stub — R6 wires the
  network slice.
- Resolved line: `→ {{ resolved }}` — the URL with active-environment
  variables substituted. Non-secret values inline; secrets that are set read
  as `••••••••` (Keychain values never reach QML — R6's send resolves for
  real on the C++ side); unset secrets keep their `{{name}}` placeholder.
  Typing in the URL field bumps a `resolveTick` because QML-var field edits
  do not re-evaluate bindings.

### Component edits (Nuxt-backed)

- `UiSelectMenu`: new `selectedColor` property — consumer-supplied trigger
  label colour while a selection exists (same extension pattern as
  chevronSource/checkIconSource; default `Theme.textPrimary`). Default trigger
  chevron is now the shared chevron icon (drawn like the project tree: 64px
  PNG downscaled, 16px decode) instead of a text glyph.
- `UiButton`: Nuxt `loading` renders the spinner inside the button — left by
  default, right with the new `trailing` prop (Button.vue/useComponentIcons);
  loading still disables the button.
- `Theme`: `fontMono = "Menlo"` — first font-family token; all mono surfaces
  (URL bar now, JSON body in R6) share one family.

## Decisions

- Secrets in the preview are masked, matching the environments list (the app
  never renders Keychain values); the value actually sent is resolved in C++
  during R6's send.
- Method/environment activation is exposed as plain functions
  (`selectMethod/selectEnv`); the harness drives them through the select's
  `activated` signal so `currentIndex` follows the user path.

## Tests

- `test_api_request_urlbar`: draft preset (GET + draft URL), resolved preview
  (baseUrl substituted, secret masked), method switch via the select row's
  `activated` (POST lands, dirty), environment switch (staging keeps raw
  placeholders, local restores), typed URL live-updates the preview, Send ⇄
  Cancel locks/restores the bar.

## Verification

1. `cmake --build build -j 8` clean.
2. `qmllint` on the touched QML (Theme, UiSelectMenu, ApiRequestPage) — clean.
3. Startup log: no error / binding loop.
4. `./qmltests` — 45/45 green.
5. Launch: open a request/draft — method select presets and recolours, URL
   editing is live in the preview, env switch flips the preview, Send shows
   the loading/Cancel state.