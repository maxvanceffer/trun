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
- **ALWAYS kill existing instances** before launching a new build: `killall trun-app` or `pkill -f trun-app`
- **NEVER** leave old instances running — they confuse debugging since you can't tell which build is producing the output
- **RULE**: Every `open build/trun-app` must be preceded by `killall trun-app 2>/dev/null`

### Icon path discovery for Qt apps
- QCoreApplication::applicationDirPath() returns the directory containing the executable
- For flat build: `build/trun-app` → dir is `build/` → icons at `build/icons/` → `exePath + "/icons"`
- For macOS bundle: `trun-app.app/Contents/MacOS/trun-app` → dir is `Contents/MacOS/` → icons at `Contents/icons/` → `exePath + "/../icons"`
- Always try multiple paths with `QFile::exists()` fallback — don't guess

## Skills (Навыки) и MCP-серверы

### 1. Доступные скиллы (официальные Qt AI — `.opencode/skills/`)

| Скилл | Назначение |
|---|---|
| `qt-qml-review` | Проверка и глубокий аудит QML-кода (производительность, утечки, bindings) |
| `qt-qml` | Лучшие практики написания QML под Qt 6 |
| `qt-cpp-review` | Анализ C++ кода (потокобезопасность, память, Qt-модели) |
| `qt-cmake-project` | Настройка и рефакторинг CMake для Qt 6 |
| `qt-ui-design` | UI/UX стандарты и адаптация интерфейсов |
| `qt-qml-docs` / `qt-cpp-docs` | Генерация документации по исходникам |
| `qt-qml-test` / `qt-qml-test-run` | Создание и прогон QML-тестов |

### 2. Правила использования скиллов

- **Перед написанием, рефакторингом или ревью кода** ВСЕГДА обращайся к инструкциям соответствующего `SKILL.md`.
- Не используй устаревший синтаксис Qt 5 или qmake-подходы.
- Для Qt 6 используй актуальные паттерны и API.

### 3. MCP-сервер документации (`qt-docs`)

- Для получения актуальной справки по классам, сигналам и методам Qt делай запросы к MCP `qt-docs` (настроен в `opencode.json`), а не опирайся на свои догадки.
