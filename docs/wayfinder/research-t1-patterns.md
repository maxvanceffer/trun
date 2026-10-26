# T1 — Patterns for ApiService: findings

Ticket: `docs/wayfinder/t1-patterns.md`. Map: `docs/wayfinder/api-saved-requests-map.md`.
Facts only + `file:line` pointers. No implementation code here.

## 1. A C++ service that works off the GUI thread

The canonical pattern is `QThread::create` + `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` + the thread's `deleteLater`:

- `src/databaseservice.cpp:21-46` — `refresh()`: guard `if (m_busy) return`, set `busy`/`scanning` + emit, all blocking work (`detectDatabaseServices()`) in `QThread::create`, return to the GUI thread via `invokeMethod`, `connect(thread, &QThread::finished, thread, &QObject::deleteLater)` + `thread->start()`.
- `src/databaseservice.cpp:48-82` — `control(id, start)`: the same pattern + `controllingId` for a per-row busy indicator instead of a global spinner.
- `src/databaseservice.cpp:104-131` — `runQuery()`: a blocking call in the thread, the result out via the `queryResult` signal (signals declared in `src/databaseservice.h:47-58`).
- `src/databaseservice.cpp:133-158` — `fetchDetail()`: several backend calls (ports, DBs, connection config) collected into a single `QVariantMap detail` in one background pass, out via a single `detailReady(id, detail)` signal. A model for "collect request detail in one pass".
- `src/databaseservice.cpp:160-175`, `:193-213` — `saveConnection` / `testServiceConnection`: saving and testing also leave the GUI thread; the comment at `:41` pins the contract "password null = keep existing secret".
- `src/dockerservice.cpp:278-311` — `refresh()`: the same pattern; it alone raises `scanning`; `:289-303` — the "failed query keeps cached data" rule (on `!ok` old data is not overwritten, `errorMessage` goes out).
- `src/dockerservice.cpp:314-359` — `beginOp`/`endOp`/`control(args, target)`: one action off the GUI thread plus a full re-read. `target` is published as `controllingId` (`"start:web"`, `"rmi:<id>"`, `"prune"`, `"engine"`, `"start:group:<project>"`), and only `refresh()` raises `scanning` — so the header's Rescan icon spins only for a real rescan and each action button carries its own loader (`qml/DockerPage.qml:509`, `:665-666`, `:704`, `:716`, `:785-830`, `:915`).
- `src/dockerservice.cpp:484-504`, `:506-526` — `fetchStats`/`fetchLogs`: protection against overlapping polls via `std::atomic<bool>` (`m_statsBusy`/`m_logsBusy`, declared in `src/dockerservice.h:114-115`), `fetchStats` deliberately does not touch `busy` (`src/dockerservice.h:59-61`).
- `src/dockerservice.h:63-76` — static synchronous `queryEngine/queryContainers/...` for headless consumers (MCP); the comment requires calling them off the GUI thread.
- Contrast: `CommandExecutor` (`src/commandexecutor.h:18-23`, signals `:73-80`) does NOT use `QThread::create` — it wraps an asynchronous `QProcess` (signals `started/outputReceived/finished/failed`). For a networked `ApiService`, the relevant pattern is the `QThread::create` one from the Database/Docker services.
- The QML side subscribes via `Connections { target: svc }`: `qml/DockerPage.qml:198-217`, `qml/DatabasesPage.qml:238-276`.

## 2. Lazy creation / first refresh on page open

An important finding: **a Loader is NOT used for the Docker/Databases pages**. There is exactly one `Loader` in the app — `qml/main.qml:268-279` (a one-shot `WizardWindow` when `projectCount === 0`). Both pages are permanent instances inside `pageHost`, switched by visibility:

- `qml/Dashboard.qml:2153-2188` — the `databasesPageWrap` / `dockerPageWrap` wrappers: `enabled` + `visible: opacity > 0` + `opacity`/`x` with `Theme.animMedium`/`animSlide` animations. "Lazy" means lazy *data* loading, not QML creation.
- Data is warmed ahead: the constructors of both services call `refresh()` immediately (`src/databaseservice.cpp:15-19`, `src/dockerservice.cpp:253-257` — "Scan once at startup (off the GUI thread) so the page has data cached").
- `qml/DockerPage.qml:946-949` — `Component.onCompleted`: a conditional `refresh()` only if the list is empty and not `busy`.
- `qml/DockerPage.qml:25`, `:231-240` — `pageActive` (passed from Dashboard at `qml/Dashboard.qml:2186`): the 5-second `fetchStats` poll runs only while the page is open (`triggeredOnStart: true`); the neighboring timer `:220-228` keeps pulling logs while a card is open.
- `qml/DatabasesPage.qml:39-42`, `:70-88` — two-level navigation `selectedIndex` (-1 = list); `onSelectedIndexChanged` resets `detail` and calls `db.fetchDetail(svc.id)`; the comment `:77-81` explains reading `services[selectedIndex]` directly instead of a computed property (stale binding in the same tick).
- `qml/Dashboard.qml:494-505` — `openDatabases()` resets `databasesPage.selectedIndex = -1` (always returns to the list level), `openDocker()` only sets `activePage`. Both call `closeDetail()` on the first line.
- Conclusion for ApiService: reuse "permanent instance + lazy data" (constructor `refresh`, conditional refresh in `onCompleted`, `pageActive` gate for polls, reset of the internal index in `openApi`), rather than introducing a `Loader`.

## 3. Storing structures in Settings, secrets in Keychain

Three layers, from general to specific:

- `Settings` (`src/settings.h:9-46`, `src/settings.cpp:15-47`) — a thin wrapper over `QSettings` (IniFormat in tests — `:9-13`): `get/set/has/remove` are available from QML (`Q_INVOKABLE`), each `set/remove` calls `sync()` immediately.
- JSON-in-a-string is the standard way to store structures: `customCommands` (`src/projectservice.cpp:100-125`: `QJsonDocument(arr).toJson(Compact)` to a string on write, `QJsonDocument::fromJson(raw).array()` on read), `pinnedCommands` / `recentCommands` (`src/projectservice.cpp:699-744` by grep: keys `pinnedCommands`, `recentCommands`), `projects` as a `QByteArray` (`src/settings.cpp:99-113`), `orphanedProcesses` as a `QVariantList` (`src/main.cpp:125-142`).
- Keys like `runConfig/<projectId>/<cmdId>` (`src/projectservice.cpp:684`, `:919` by grep) — a model for namespaced keys; useful for the `api/...` namespace.
- QML access to Settings: `qml/main.qml:110-141` (`has/get/set/remove`, including persisting `orphanedProcesses` on exit), `qml/SettingsDialog.qml:129,179,229,237`.
- The non-secret connection config is a direct `QSettings` group (bypassing the `Settings` class): group `DatabaseConnections/<serviceId>` (`src/databaseservice_macos.cpp:244-248`), only non-secret fields `host/user/database/sslmode/authMethod` (`:268-279`), save `:973-988`, remove `:990-1003`.
- Secrets only in Keychain, never in Settings: the native Security framework without the `security` CLI (`src/mackeychain.h:1-14` — rationale: a password in argv is visible via `ps`), the wrappers `keychainFindPassword/Save/Delete` (`src/databaseservice_macos.cpp:283-296`), service label `trun.db.<engine>[@version]` (`:250-259`), account = user or `$USER` (`:261-265`), the resolve assembly `resolveConnection` (`:312-326`), the `hasSavedPassword` flag outward (`:962-971`).
- Linux stub: `src/databaseservice_linux.cpp:33` — `connectionConfig` there too per the contract `src/databaseservice_backend.h:38-58`; storing a password on Linux is a no-op until a Secret Service backend (contract `:44-47`).
- `src/databaseservice_backend.h:19-68` — the whole backend contract (detect/control/ports/databases/config/save/forget/test/query) as a model for splitting "QObject service ↔ platform backend".

## 4. Registering the service in main.cpp and exposing it to QML

- `#include` + static instances for the app lifetime: `src/main.cpp:21-22`, `:113-114` (`static auto databaseService = new DatabaseService()` — the services warm their cache in the constructor, see §2).
- Exposure: `engine.rootContext()->setContextProperty("databaseService", databaseService)` (`src/main.cpp:148-149`); `Settings` — under the name `"Settings"` (`:152`).
- The QML side takes context via `readonly property var` + a `typeof` guard for standalone loading in tests: `qml/DockerPage.qml:13-22` (`readonly property var svc: (typeof dbs !== "undefined") ? dbs : null`, then all reads via `svc ? ... : fallback`), the same guard in `qml/DatabasesPage.qml:16-25` and `qml/Dashboard.qml:47,50`.
- Non-GUI icon/path dependencies nearby: `iconsPath` resolution with a fallback chain (`src/main.cpp:156-177`), QML loading dev file → `qrc:/qml/main.qml` fallback (`:183-192`). Per AGENTS.md, resources only via `qrc:/`.

## 5. Extending Sidebar + Dashboard activePage + breadcrumb for a new section

A chain of 4 places (copy order for API):

1. `qml/Sidebar.qml:34-44` — the `databasesRequested` / `dockerRequested` signals; the `databasesHeader` (`:140-196`) and `dockerHeader` (`:198-254`) blocks — the reference template (Image `Theme.icon(...)` + Label + hover Rectangle + MouseArea). The anchor chain: `dashboardRow → databasesHeader → dockerHeader → sectionHeader` (`:256-263` — `sectionHeader` is anchored to `dockerHeader.bottom`; insert the new block between, and the chain shifts).
2. `qml/main.qml:304-311` — `onDatabasesRequested` / `onDockerRequested` → `root.activeView = "dashboard"` + `dashboardView.openDatabases()/openDocker()`; nearby `:290-303` the `onFolderSelected` pattern.
3. `qml/Dashboard.qml:444` — `activePage` with values `"dashboard"/"folder"/"detail"/"databases"/"docker"`; `openDatabases/openDocker` (`:494-505`); the page wrap modeled on `:2153-2188`; `markHitTest` (`:526-548`) — a new crumb Label must be added to `controls`, otherwise the title-bar agent eats clicks (a null-guard is mandatory — QWindowKit ASSERT-abort).
4. Breadcrumb: the `Databases` segments (`:870-910`, including the selected service's 3rd segment) and `Docker` (`:912-924`) with `visible: activePage === "..."`; a clickable return to the list (`:885-890`).
- Registering a new QML page in two places: `qml.qrc:29-31` (`DatabasesPage.qml`, `ConnectionDialog.qml`, `DockerPage.qml` under `<qresource prefix="/">`) and `CMakeLists.txt:87-89` (`QML_FILES` in `qt_add_qml_module`); the service's C++ files — in `qt_add_executable` (`:49-52`) + platform `.mm` (`:173-178`).
- Section icon: `Theme.icon(name)` (`qml/Theme.qml:112-131`) — `iconBaseUrl + name + (-dark).png`, single-variant in the `single` map; files in `qml.qrc:48-121` (`database(-dark).png` `:113-114`, `docker(-dark).png` `:115-116`); `Theme.icon("docker")` used in `qml/Sidebar.qml:230` and `qml/DockerPage.qml:447`.
