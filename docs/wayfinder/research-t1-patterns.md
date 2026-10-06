# T1 — Паттерны для ApiService: findings

Тикет: `docs/wayfinder/t1-patterns.md`. Map: `docs/wayfinder/api-saved-requests-map.md`.
Только факты + указатели `файл:строка`. Кода реализации здесь нет.

## 1. C++-сервис с работой вне GUI-потока

Канонический паттерн — `QThread::create` + `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` + `deleteLater` потока:

- `src/databaseservice.cpp:21-46` — `refresh()`: guard `if (m_busy) return`, выставление `busy`/`scanning` + emit, вся блокирующая работа (`detectDatabaseServices()`) в `QThread::create`, возврат в GUI-поток через `invokeMethod`, `connect(thread, &QThread::finished, thread, &QObject::deleteLater)` + `thread->start()`.
- `src/databaseservice.cpp:48-82` — `control(id, start)`: тот же паттерн + `controllingId` для per-row busy-индикатора вместо глобального спиннера.
- `src/databaseservice.cpp:104-131` — `runQuery()`: блокирующий вызов в потоке, результат наружу через сигнал `queryResult` (сигналы объявлены `src/databaseservice.h:47-58`).
- `src/databaseservice.cpp:133-158` — `fetchDetail()`: несколько backend-вызовов (порты, БД, connection config) собираются в один `QVariantMap detail` в одном фоновом проходе, наружу — один сигнал `detailReady(id, detail)`. Образец для «собрать request detail за один проход».
- `src/databaseservice.cpp:160-175`, `:193-213` — `saveConnection` / `testServiceConnection`: сохранение и тест тоже уходят с GUI-потока; комментарий `:41` фиксирует контракт «password null = keep existing secret».
- `src/dockerservice.cpp:278-308` — `refresh()`: тот же паттерн; `:285-304` — правило «failed query keeps cached data» (при `!ok` старые данные не затираются, наружу `errorMessage`).
- `src/dockerservice.cpp:310-343` — `control(args, quiet=false)`: `quiet`-операции (`removeImage`, `src/dockerservice.cpp:377-381`) пропускают глобальный `busy`, UI трекает их per-item через `pendingRemove` (`qml/DockerPage.qml:28-29`).
- `src/dockerservice.cpp:468-488`, `:490-510` — `fetchStats`/`fetchLogs`: защита от наложения поллов через `std::atomic<bool>` (`m_statsBusy`/`m_logsBusy`, объявлены `src/dockerservice.h:90-91`), `fetchStats` deliberately не трогает `busy` (`src/dockerservice.h:44-47`).
- `src/dockerservice.h:49-62` — статические синхронные `queryEngine/queryContainers/...` для headless-потребителей (MCP); комментарий требует вызывать их вне GUI-потока.
- Контраст: `CommandExecutor` (`src/commandexecutor.h:18-23`, сигналы `:73-80`) НЕ использует `QThread::create` — он обёрнут вокруг асинхронного `QProcess` (сигналы `started/outputReceived/finished/failed`). Для сетевого `ApiService` релевантен именно `QThread::create`-паттерн Database/Docker-сервисов.
- QML-сторона подписывается через `Connections { target: svc }`: `qml/DockerPage.qml:198-217`, `qml/DatabasesPage.qml:238-276`.

## 2. Lazy-создание / первый refresh при открытии страницы

Важная находка: **Loader для Docker/Databases-страниц НЕ используется**. `Loader` в приложении один — `qml/main.qml:268-279` (одноразовый `WizardWindow` при `projectCount === 0`). Обе страницы — постоянные инстансы внутри `pageHost`, переключаются видимостью:

- `qml/Dashboard.qml:2153-2188` — обёртки `databasesPageWrap` / `dockerPageWrap`: `enabled` + `visible: opacity > 0` + `opacity`/`x` с анимациями `Theme.animMedium`/`animSlide`. «Lazy» означает ленивую загрузку *данных*, а не создание QML.
- Данные греются заранее: конструкторы обоих сервисов сразу зовут `refresh()` (`src/databaseservice.cpp:15-19`, `src/dockerservice.cpp:253-257` — «Scan once at startup (off the GUI thread) so the page has data cached»).
- `qml/DockerPage.qml:946-949` — `Component.onCompleted`: условный `refresh()` только если список пуст и не `busy`.
- `qml/DockerPage.qml:25`, `:231-240` — `pageActive` (прокинут из Dashboard `qml/Dashboard.qml:2186`): 5-секундный `fetchStats`-полл идёт только пока страница открыта (`triggeredOnStart: true`); соседний таймер `:220-228` дотягивает логи пока открыта карточка.
- `qml/DatabasesPage.qml:39-42`, `:70-88` — двухуровневая навигация `selectedIndex` (-1 = список); `onSelectedIndexChanged` сбрасывает `detail` и зовёт `db.fetchDetail(svc.id)`; комментарий `:77-81` объясняет чтение `services[selectedIndex]` напрямую вместо computed-свойства (stale binding в том же тике).
- `qml/Dashboard.qml:494-505` — `openDatabases()` сбрасывает `databasesPage.selectedIndex = -1` (возврат всегда на уровень списка), `openDocker()` только ставит `activePage`. Обе зовут `closeDetail()` первой строкой.
- Вывод для ApiService: переиспользовать «постоянный инстанс + ленивые данные» (конструктор-`refresh`, условный refresh в `onCompleted`, `pageActive`-гейт для поллов, сброс внутреннего индекса в `openApi`), а не вводить `Loader`.

## 3. Хранение структур в Settings, секретов в Keychain

Три слоя, от общего к частному:

- `Settings` (`src/settings.h:9-46`, `src/settings.cpp:15-47`) — тонкая обёртка над `QSettings` (IniFormat в тестах — `:9-13`): `get/set/has/remove` доступны из QML (`Q_INVOKABLE`), каждый `set/remove` делает `sync()` сразу.
- JSON-в-строке — стандартный способ хранить структуры: `customCommands` (`src/projectservice.cpp:100-125`: `QJsonDocument(arr).toJson(Compact)` в строку при записи, `QJsonDocument::fromJson(raw).array()` при чтении), `pinnedCommands` / `recentCommands` (`src/projectservice.cpp:699-744` по grep: ключи `pinnedCommands`, `recentCommands`), `projects` как `QByteArray` (`src/settings.cpp:99-113`), `orphanedProcesses` как `QVariantList` (`src/main.cpp:125-142`).
- Ключи вида `runConfig/<projectId>/<cmdId>` (`src/projectservice.cpp:684`, `:919` по grep) — образец namespaced-ключей; пригодится для `api/...` неймспейса.
- QML-доступ к Settings: `qml/main.qml:110-141` (`has/get/set/remove`, включая персист `orphanedProcesses` при выходе), `qml/SettingsDialog.qml:129,179,229,237`.
- Несекретный connection config — прямой `QSettings`-group (мимо `Settings`-класса): группа `DatabaseConnections/<serviceId>` (`src/databaseservice_macos.cpp:244-248`), только не-секретные поля `host/user/database/sslmode/authMethod` (`:268-279`), сохранение `:973-988`, удаление `:990-1003`.
- Секреты — только Keychain, никогда в Settings: нативный Security-framework без `security` CLI (`src/mackeychain.h:1-14` — обоснование: пароль в argv виден через `ps`), обёртки `keychainFindPassword/Save/Delete` (`src/databaseservice_macos.cpp:283-296`), service label `trun.db.<engine>[@version]` (`:250-259`), account = user или `$USER` (`:261-265`), сборка резолва `resolveConnection` (`:312-326`), флаг `hasSavedPassword` наружу (`:962-971`).
- Linux-заглушка: `src/databaseservice_linux.cpp:33` — `connectionConfig` там же по контракту `src/databaseservice_backend.h:38-58`; хранение пароля на Linux — no-op до Secret Service backend (контракт `:44-47`).
- `src/databaseservice_backend.h:19-68` — весь backend-контракт (detect/control/ports/databases/config/save/forget/test/query) как образец разделения «QObject-сервис ↔ платформенный backend».

## 4. Регистрация сервиса в main.cpp и проброс в QML

- `#include` + статические инстансы на время жизни приложения: `src/main.cpp:21-22`, `:113-114` (`static auto databaseService = new DatabaseService()` — сервисы сами греют кэш в конструкторе, см. п.2).
- Проброс: `engine.rootContext()->setContextProperty("databaseService", databaseService)` (`src/main.cpp:148-149`); `Settings` — под именем `"Settings"` (`:152`).
- QML-сторона берёт контекст через `readonly property var` + `typeof`-guard для standalone-загрузки в тестах: `qml/DockerPage.qml:13-22` (`readonly property var svc: (typeof dbs !== "undefined") ? dbs : null`, дальше все чтения через `svc ? ... : fallback`), тот же guard в `qml/DatabasesPage.qml:16-25` и `qml/Dashboard.qml:47,50`.
- Не-GUI зависимости иконок/путей рядом: резолв `iconsPath` с fallback-цепочкой (`src/main.cpp:156-177`), загрузка QML dev-файл → `qrc:/qml/main.qml` fallback (`:183-192`). По AGENTS.md ресурсы только через `qrc:/`.

## 5. Расширение Sidebar + Dashboard activePage + breadcrumb под новый раздел

Цепочка из 4 мест (порядок копирования под API):

1. `qml/Sidebar.qml:34-44` — сигналы `databasesRequested` / `dockerRequested`; блоки `databasesHeader` (`:140-196`) и `dockerHeader` (`:198-254`) — эталонный шаблон (Image `Theme.icon(...)` + Label + hover-Rectangle + MouseArea). Якорная цепочка: `dashboardRow → databasesHeader → dockerHeader → sectionHeader` (`:256-263` — `sectionHeader` привязан к `dockerHeader.bottom`, новый блок вставляется между, цепочка сдвигается).
2. `qml/main.qml:304-311` — `onDatabasesRequested` / `onDockerRequested` → `root.activeView = "dashboard"` + `dashboardView.openDatabases()/openDocker()`; рядом `:290-303` паттерн `onFolderSelected`.
3. `qml/Dashboard.qml:444` — `activePage` со значениями `"dashboard"/"folder"/"detail"/"databases"/"docker"`; `openDatabases/openDocker` (`:494-505`); page wrap по образцу `:2153-2188`; `markHitTest` (`:526-548`) — новый crumb-Label нужно добавить в `controls`, иначе клики съест title-bar agent (null-guard обязателен — QWindowKit ASSERT-abort).
4. Breadcrumb: сегменты `Databases` (`:870-910`, включая 3-й сегмент выбранного сервиса) и `Docker` (`:912-924`) с `visible: activePage === "..."`; кликабельный возврат в список (`:885-890`).
- Регистрация новой QML-страницы в двух местах: `qml.qrc:29-31` (`DatabasesPage.qml`, `ConnectionDialog.qml`, `DockerPage.qml` в `<qresource prefix="/">`) и `CMakeLists.txt:87-89` (`QML_FILES` в `qt_add_qml_module`); C++-файлы сервиса — в `qt_add_executable` (`:49-52`) + платформенные `.mm` (`:173-178`).
- Иконка раздела: `Theme.icon(name)` (`qml/Theme.qml:112-131`) — `iconBaseUrl + name + (-dark).png`, одно-вариантные в `single`-map; файлы в `qml.qrc:48-121` (`database(-dark).png` `:113-114`, `docker(-dark).png` `:115-116`); `Theme.icon("docker")` используется в `qml/Sidebar.qml:230` и `qml/DockerPage.qml:447`.
