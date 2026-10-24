# UI rules

Read in full before any visual work: a new control, a component edit, a color
choice.

## Source

The single source is Nuxt UI v4 under this project's Carbon config. Slots,
variants, sizes, states and colors come from there; so does the right to say that
something is not in the project.

| What you need | Where to look |
|---|---|
| Variants/compound-variants, sizes, state classes | `/tmp/nuxt-ui/src/theme/<name>.ts` |
| Props, icon slots, element order | `/tmp/nuxt-ui/src/runtime/components/<Name>.vue` |
| What this compiles to with our config | `/tmp/nuxt-carbon/.output/public/_nuxt/*.css` |
| Palette and `--ui-*` semantics | `assets/main.css` (`@theme` + `:root,.light` + `.dark`) |
| Chosen accents | `assets/app.config.ts` — primary amber, secondary yellow, neutral carbon |
| Translation to QML | `qml/Theme.qml` — the only carrier of colors |

`/tmp/nuxt-ui` is a working copy of github.com/nuxt/ui, v4.11.3, commit `d856ec3d`.
The directory is temporary: if it is missing, clone it again and use the same
path. Values are not taken from memory — each is read from the table.

## Ready-made controls

Everything that is drawn is assembled from `qml/Ui/` (list its contents with a
command; not restated here) plus the shell components `AppDialog` / `ConfirmDialog`
in `qml/components/`.

If the needed type is not in `qml/Ui/` — **stop and name which Nuxt UI component
must be added**: give the path to `src/runtime/components/<Name>.vue` and
`src/theme/<name>.ts`, and list which variants/sizes will be needed. Work
continues after the user's decision; the new type is created following the
existing `qml/Ui/*.qml`.

The same procedure applies when a case is not covered by the color map below:
list the variants from `src/theme/<name>.ts`, show which token they correspond to
(or that there is no matching `--ui-*`), and wait for a choice.

If a control exists in `qml/Ui/` — edit it, not a copy of its markup at the usage
site.

List rows (Docker, Databases, API) use `qml/Ui/UiListElement.qml` (one card-row, a
hover border); do not introduce a bespoke card markup at the list site.

## Colors

One table per case: which kind of surface, text or status takes which token. A new
color is not introduced into a component — if there is no token for the case, the
procedure from the "Ready-made controls" section applies: show which `--ui-*` is
missing and wait for a choice. `Theme.qml` holds the values from `assets/main.css`.

### Surfaces

| Case (in Nuxt) | Token |
|---|---|
| page, modal content, tooltip, Select/Dropdown content, select trigger, input field, `bg-default` in general | `Theme.backgroundDefault` — an alias of `Theme.windowBackground` |
| card/panel (`bg-elevated/50` over the page) | `Theme.cardBackground` |
| row or control hover, off-state, `bg-elevated` | `Theme.backgroundElevated` — an alias of `Theme.mutedSurface` |
| hover step over `bg-elevated`, `bg-accented` | `Theme.mutedSurfaceHover` |
| modal scrim — `bg-elevated/75`; this is how `AppDialog` does it, bare `Dialog`s still use the standard Qt scrim | `Theme.backgroundElevated` @ 0.75 |
| solid neutral fill, `bg-inverted` | `Theme.backgroundInverted` |
| sidebar — the page tone; decision: do not change it, in Nuxt the sidebar is also `bg-default` | `Theme.sidebarBackground` (= `windowBackground`) |

### Text

| Case (in Nuxt) | Token |
|---|---|
| primary text, `text` | `Theme.textPrimary` |
| secondary text, `text-muted` | `Theme.textMuted` |
| captions, placeholders, `text-dimmed` | `Theme.textDimmed` |
| the faintest — unselected segments, `text-subtle` (no separate `--ui-*`) | `Theme.textSubtle` |
| text on `bg-inverted` | `Theme.textInverted` |

### Accents

| Case | Token |
|---|---|
| `bg-<color>` fill | `Theme.primary` / `secondary` / `success` / `info` / `warning` / `error` |
| outline / soft / subtle / ghost / link label (`text-<color>`) | `Theme.<color>` |
| the same, but `color: secondary` | `Theme.secondaryForeground` |
| the same, but `color: neutral` — there is no accent token | `Theme.textPrimary` (`text-default`) |
| solid-fill label, any color except neutral | `Theme.accentForeground` |
| solid-fill label, `color: neutral` | `Theme.textInverted` |

`accentForeground` = `#181818` in both modes. Nuxt writes `text-inverted` here, and
in light that is nearly white: 2.05:1 on amber, 1.83:1 on yellow, 2.13:1 on green,
3.61:1 on blue, 3.65:1 on red — none of the colors passes WCAG AA (4.5:1). Dark ink
gives 4.66–11.3:1 on all five and matches what the dark half of the theme already
uses.

`secondaryForeground` = `textPrimary`. It was needed because Nuxt gives
`text-secondary`: yellow text on a white page, 1.85:1.

### Statuses

| Case | Token |
|---|---|
| "running", running, node reachable | `Theme.success` |
| error, unavailable, destruction | `Theme.error` |
| warning | `Theme.warning` |
| info, neutral indicator | `Theme.info` |

In `qml/` outside `Theme.qml` there are currently ~45 hard-coded hex values, and
some of them are status colors that already have a token: `#22c55e` next to
`Theme.success` (`#00c950`), `#dc2626` next to `Theme.error` (`#fb2c36`). These are
leftovers from old markup; in new and edited code the status comes from the table,
not copied from a neighboring line. The platform chrome — the macOS window buttons
in `TrafficLights.qml` — is not subject to the table.

### Code

A read-only code/JSON viewer (the R6 response body) is the one place where a
syntax palette exists. It is still the same table, not a new one: the
tokenizer in `qml/JsonFormat.js` produces classes and the caller supplies the
colors — currently `qml/ApiResponsePanel.qml`.

| Syntax token class | Token |
|---|---|
| object key (`"key":`) | `Theme.textPrimary` |
| string value | `Theme.primary` |
| number | `Theme.info` |
| keyword (`true` / `false` / `null`) | `Theme.error` |
| punctuation (`{}[]:,`) and the line-number gutter | `Theme.textSubtle` |
| plain text (a non-JSON body) and whitespace | `Theme.textMuted` |

The font is `Theme.fontMono`; the gutter is padded in `JsonFormat.js` so the
mono line numbers right-align without a second layout. Highlight colors are
passed in as strings, since `Text` rich text needs a CSS color.

### Chrome and geometry

| Case | Value |
|---|---|
| borders, separators | `Theme.border` |
| accent border (selected, focus base) | `Theme.borderAccented` |
| hover on a card-row (`UiListElement`) | border `Theme.border` → `Theme.ring` over `Theme.animHover`; not a fill — a square-cornered fill breaks the rounding |
| radii | `Theme.radiusSm` 8 / `radiusMd` 12 / `radiusLg` 16 — `--ui-radius: 0.5rem` |
| disabled control | `opacity` 0.75; for `UiButton` the fill and border do not recolor in that state |
| size steps, padding | `Theme.spacingXs…Xl`, `controlHeight`, `iconXs…Lg` |

## Verification after an edit

1. `cmake --build build -j 8` — QML is copied into the bundle on every build, so an
   edit in `qml/` is not visible to the app without a rebuild.
2. `killall trun`, then launch `build/trun.app/Contents/MacOS/trun`: the startup
   log without `error`, `binding loop` and QML warnings.
3. After editing `qml/Ui/*.qml` or `qml/Theme.qml` — run the parity harness. It
   lives in the repository and contains no machine- or build-specific paths:

   ```bash
   cd build && ../qml/tests/run_uibutton.sh          # light theme
   cd build && ../qml/tests/run_uibutton.sh --dark    # dark theme
   cd build && ../qml/tests/run_uilistelement.sh          # light theme
   cd build && ../qml/tests/run_uilistelement.sh --dark    # dark theme
   ```

   `verify_uibutton.py` compares screenshots with the log (geometry, fills,
   rounding, focus ring, hover) and separately checks two decisions that are not in
   the config: `solid` + non-neutral must give `#181818`, and the log must contain
   no QML errors. `verify_uilistelement.py` checks the card-row `UiListElement`:
   rounding, the `cardBackground` fill, and the `border → ring` transition on
   hover. The `--dark` mode itself patches `isDark` in `qml/Theme.qml` and restores
   the file on exit — from QML, `QStyleHints::setColorScheme` cannot be set.
4. A discrepancy with the Nuxt source is a reason to stop and ask, not to bend the
   harness to what you saw.
