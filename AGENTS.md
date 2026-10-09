# AGENTS.md — trun

## QML Resources MUST be embedded in qrc

**All QML files, icons, and static assets MUST be loaded via qrc:// URLs — never via relative file paths.**

### Why
- The app is distributed as a compiled binary with bundled resources
- Relative paths (`icons/npm.svg`) resolve relative to the QML file's location, which breaks when resources are embedded
- The app will load QML from both local filesystem (development) and qrc (production)

### Rules

1. **Always use absolute qrc:// paths for resources:**
   ```qml
   // CORRECT
   source: "qrc:/icons/npm.svg"
   
   // WRONG — relative path (icons will be invisible)
   source: "icons/npm.svg"
   ```

2. **All icons go through qml.qrc:**
   ```xml
   <qresource prefix="/icons">
       <file>assets/icons/npm.svg</file>
       <file>assets/icons/rust.svg</file>
       <!-- etc. -->
   </qresource>
   ```

3. **When adding a new icon:**
   - Add the SVG to `assets/icons/`
   - Add it to `qml.qrc` in the `<qresource prefix="/icons">` section

4. **When adding a new QML file:**
   - Add it to `qml.qrc` in the `<qresource prefix="/">` section
   - Reference it as `qrc:/qml/YourFile.qml`

### Reference
- `qml.qrc` — the master resource file (all QML + icons)
- `assets/icons/` — source SVG files
- `CMakeLists.txt` — `qt_add_resources(trun-app "qml" "resources" qml.qrc)`

## Anti-patterns — DO NOT USE

### QtQuick TreeView
- **NO** `verticalScrollBarMode` / `horizontalScrollBarMode` — use `verticalScrollBarPolicy` / `horizontalScrollBarPolicy`
- **NO** `columnResizeable` / `columnStretchable` — these were removed in Qt6
- Use `Qt.ScrollBarAsNeeded` or `Qt.ScrollBarAlwaysOff`

### Delegate for TreeView
- Model role values are accessed via `model.<roleName>` inside the delegate
- **Do NOT** reference variables like `type`, `name`, `manifest` directly — they don't exist
- **Do** access as `model.item_type`, `model.name`, `model.manifest`, `model.folderName`, etc.

### File loading
- **NO** relative paths for icons/resources — they break when resources are embedded
- **DO** use `qrc:/icons/...` for bundled assets
- **DO** use `QUrl::fromLocalFile(...)` only for development file paths

### App management during development/testing
- **ALWAYS kill existing instances** before launching a new build: `killall trun` or `pkill -f "trun.app"`
- **NEVER** leave old instances running — they confuse debugging since you can't tell which build is producing the output
- **RULE**: Every `open build/trun.app` must be preceded by `killall trun 2>/dev/null`

### Icon path discovery for Qt apps
- QCoreApplication::applicationDirPath() returns the directory containing the executable
- For macOS bundle: `trun.app/Contents/MacOS/trun` → dir is `Contents/MacOS/` → icons at `Contents/MacOS/icons` → `exePath + "/icons"`
- Always try multiple paths with `QFile::exists()` fallback — don't guess

## UI — a single source (Nuxt UI)

All visuals and all colors come from Nuxt UI through `qml/Ui/*` and the tokens in
`qml/Theme.qml`.

Before an edit, read `docs/ui-rules.md` in full on branches involving: a new
control, a component edit, a color choice. If the needed UI type is not in
`qml/Ui/`, or the case is not covered by the color map — stop, name which Nuxt UI
component must be added (with paths to `src/runtime/components/...` and
`src/theme/...`), and wait for the user's decision.

## Skills and MCP servers

### 1. Available skills (official Qt AI — `.opencode/skills/`)

| Skill | Purpose |
|---|---|
| `qt-qml-review` | Review and deep audit of QML code (performance, leaks, bindings) |
| `qt-qml` | QML best practices for Qt 6 |
| `qt-cpp-review` | C++ code analysis (thread safety, memory, Qt models) |
| `qt-cmake-project` | Configuring and refactoring CMake for Qt 6 |
| `qt-ui-design` | UI/UX standards and interface adaptation |
| `qt-qml-docs` / `qt-cpp-docs` | Generating documentation from source |
| `qt-qml-test` / `qt-qml-test-run` | Creating and running QML tests |

### 2. Rules for using skills

- **Before writing, refactoring or reviewing code**, ALWAYS consult the instructions of the corresponding `SKILL.md`.
- Do not use outdated Qt 5 syntax or qmake approaches.
- For Qt 6, use current patterns and APIs.

### 3. Documentation MCP server (`qt-docs`)

- To get current reference on Qt classes, signals and methods, query the `qt-docs` MCP (configured in `opencode.json`) instead of relying on your own guesses.
