#include "qmltests.h"
#include "treemodel.h"
#include "projectlistmodel.h"
#include "projectservice.h"
#include "commandexecutor.h"
#include "logmodel.h"
#include "mcpagents.h"
#include "settings.h"
#include "apiservice.h"
#include <QSignalSpy>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickItem>
#include <QTemporaryDir>
#include <QTest>
#include <QFile>
#include <QDir>
#include <QRegularExpression>
#include <QFileInfo>
#include <QTextStream>
#include <QColor>
#include <QScopedPointer>

// Shared context properties (defined in qmltests.cpp)
LogModel* g_logModel = nullptr;
ProjectListModel* g_projectListModel = nullptr;
ProjectService* g_projectService = nullptr;

static QString sourceQmlPath(const QString &fileName);

void QmlTests::test_tree_model_creation()
{
    QmlTreeModel model;
    QCOMPARE(model.rowCount(), 0);
}

void QmlTests::test_add_project_package_json()
{
    QmlTreeModel model;
    model.setRootPath("/test");

    model.addProjectManifest("/test/npm-project", "package.json");

    // Visible root node "test", then one leaf folder "npm-project".
    // Leaf folders with manifests navigate directly: no children, no entry.
    QCOMPARE(model.rowCount(), 1);

    QModelIndex rootIdx = model.index(0, 0);
    QVERIFY(rootIdx.isValid());
    QCOMPARE(model.data(rootIdx, QmlTreeModel::ItemTypeRole).toString(), QString("folder"));
    QCOMPARE(model.data(rootIdx, QmlTreeModel::HasManifestsRole).toBool(), false);
    QCOMPARE(model.rowCount(rootIdx), 1);

    QModelIndex folderIdx = model.index(0, 0, rootIdx);
    QVERIFY(folderIdx.isValid());
    QCOMPARE(model.data(folderIdx, QmlTreeModel::ItemTypeRole).toString(), QString("folder"));
    QCOMPARE(model.data(folderIdx, QmlTreeModel::FolderPathRole).toString(),
             QString("/test/npm-project"));
    QCOMPARE(model.data(folderIdx, QmlTreeModel::HasManifestsRole).toBool(), true);
    QCOMPARE(model.data(folderIdx, QmlTreeModel::ManifestRole).toString(),
             QString("package.json"));
    QCOMPARE(model.rowCount(folderIdx), 0);
}

void QmlTests::test_add_project_cargo_toml()
{
    QmlTreeModel model;
    model.setRootPath("/test");

    model.addProjectManifest("/test/rust-project", "Cargo.toml");

    QCOMPARE(model.rowCount(), 1);

    QModelIndex rootIdx = model.index(0, 0);
    QVERIFY(rootIdx.isValid());
    QCOMPARE(model.rowCount(rootIdx), 1);

    QModelIndex folderIdx = model.index(0, 0, rootIdx);
    QVERIFY(folderIdx.isValid());
    QCOMPARE(model.data(folderIdx, QmlTreeModel::ItemTypeRole).toString(), QString("folder"));
    QCOMPARE(model.data(folderIdx, QmlTreeModel::HasManifestsRole).toBool(), true);
    QCOMPARE(model.rowCount(folderIdx), 0);
}

void QmlTests::test_add_folder_implicit()
{
    QmlTreeModel model;
    model.setRootPath("/test");

    // Folders are created implicitly when adding a manifest to a nested path
    model.addProjectManifest("/test/myfolder/subproject", "package.json");

    // Visible root, then 1 folder ("myfolder")
    QCOMPARE(model.rowCount(), 1);

    QModelIndex rootIdx = model.index(0, 0);
    QVERIFY(rootIdx.isValid());
    QCOMPARE(model.rowCount(rootIdx), 1);

    QModelIndex myfolderIdx = model.index(0, 0, rootIdx);
    QVERIFY(myfolderIdx.isValid());
    QCOMPARE(model.data(myfolderIdx, QmlTreeModel::ItemTypeRole).toString(), QString("folder"));
    QCOMPARE(model.data(myfolderIdx, QmlTreeModel::HasManifestsRole).toBool(), false);
    QCOMPARE(model.rowCount(myfolderIdx), 1);

    QModelIndex subIdx = model.index(0, 0, myfolderIdx);
    QVERIFY(subIdx.isValid());
    QCOMPARE(model.data(subIdx, QmlTreeModel::ItemTypeRole).toString(), QString("folder"));
    QCOMPARE(model.data(subIdx, QmlTreeModel::HasManifestsRole).toBool(), true);
    QCOMPARE(model.rowCount(subIdx), 0);
}

void QmlTests::test_folder_with_child_project()
{
    QmlTreeModel model;
    model.setRootPath("/test");

    // Two manifests in the same folder: still a leaf, no entry child
    model.addProjectManifest("/test/backend", "package.json");
    model.addProjectManifest("/test/backend", "composer.json");

    QCOMPARE(model.rowCount(), 1); // visible root

    QModelIndex rootIdx = model.index(0, 0);
    QVERIFY(rootIdx.isValid());
    QCOMPARE(model.rowCount(rootIdx), 1); // one folder "backend"

    QModelIndex folderIdx = model.index(0, 0, rootIdx);
    QVERIFY(folderIdx.isValid());
    QCOMPARE(model.data(folderIdx, QmlTreeModel::ItemTypeRole).toString(), QString("folder"));
    QCOMPARE(model.data(folderIdx, QmlTreeModel::HasManifestsRole).toBool(), true);
    QCOMPARE(model.rowCount(folderIdx), 0);
}

void QmlTests::test_hybrid_folder_gets_manifests_entry()
{
    QmlTreeModel model;
    model.setRootPath("/test");

    // A folder with both a manifest and a subfolder grows one entry at row 0
    model.addProjectManifest("/test/mixed", "package.json");
    model.addProjectManifest("/test/mixed/sub", "go.mod");

    QModelIndex rootIdx = model.index(0, 0);
    QVERIFY(rootIdx.isValid());

    QModelIndex mixedIdx = model.index(0, 0, rootIdx);
    QVERIFY(mixedIdx.isValid());
    QCOMPARE(model.data(mixedIdx, QmlTreeModel::ItemTypeRole).toString(), QString("folder"));
    QCOMPARE(model.rowCount(mixedIdx), 2);

    QModelIndex entryIdx = model.index(0, 0, mixedIdx);
    QVERIFY(entryIdx.isValid());
    QCOMPARE(model.data(entryIdx, QmlTreeModel::ItemTypeRole).toString(), QString("manifests"));
    QCOMPARE(model.data(entryIdx, QmlTreeModel::FolderNameRole).toString(), QString("mixed"));
    QCOMPARE(model.data(entryIdx, QmlTreeModel::FolderPathRole).toString(), QString("/test/mixed"));
    QCOMPARE(model.data(entryIdx, QmlTreeModel::ManifestRole).toString(), QString("package.json"));
    QCOMPARE(model.rowCount(entryIdx), 0);

    QModelIndex subIdx = model.index(1, 0, mixedIdx);
    QVERIFY(subIdx.isValid());
    QCOMPARE(model.data(subIdx, QmlTreeModel::ItemTypeRole).toString(), QString("folder"));
}

void QmlTests::test_top_level_root_gets_manifests_entry()
{
    QmlTreeModel model;
    model.setRootPath("/test");

    // Manifests directly in the root: the root stays a container row
    // and exposes them through an entry, like a hybrid folder
    model.addProjectManifest("/test", "package.json");

    QCOMPARE(model.rowCount(), 1);

    QModelIndex rootIdx = model.index(0, 0);
    QVERIFY(rootIdx.isValid());
    QCOMPARE(model.data(rootIdx, QmlTreeModel::ItemTypeRole).toString(), QString("folder"));
    QCOMPARE(model.data(rootIdx, QmlTreeModel::HasManifestsRole).toBool(), true);
    QCOMPARE(model.rowCount(rootIdx), 1);

    QModelIndex entryIdx = model.index(0, 0, rootIdx);
    QVERIFY(entryIdx.isValid());
    QCOMPARE(model.data(entryIdx, QmlTreeModel::ItemTypeRole).toString(), QString("manifests"));
    QCOMPARE(model.data(entryIdx, QmlTreeModel::FolderPathRole).toString(), QString("/test"));
    QCOMPARE(model.data(entryIdx, QmlTreeModel::ManifestRole).toString(), QString("package.json"));
}

void QmlTests::test_manifests_entry_stack_on_second_manifest()
{
    QmlTreeModel model;
    model.setRootPath("/test");

    model.addProjectManifest("/test/mixed", "package.json");
    model.addProjectManifest("/test/mixed/sub", "go.mod");
    // Second manifest in the hybrid folder: entry icon becomes the stack
    model.addProjectManifest("/test/mixed", "composer.json");
    // Duplicate registration is a no-op
    model.addProjectManifest("/test/mixed", "package.json");

    QModelIndex mixedIdx = model.index(0, 0, model.index(0, 0));
    QVERIFY(mixedIdx.isValid());
    QCOMPARE(model.rowCount(mixedIdx), 2);

    QModelIndex entryIdx = model.index(0, 0, mixedIdx);
    QVERIFY(entryIdx.isValid());
    QCOMPARE(model.data(entryIdx, QmlTreeModel::ItemTypeRole).toString(), QString("manifests"));
    QCOMPARE(model.data(entryIdx, QmlTreeModel::ManifestRole).toString(), QString());
}

void QmlTests::test_manifests_entry_when_subfolder_comes_first()
{
    QmlTreeModel model;
    model.setRootPath("/test");

    // Subfolder first, the folder's own manifest later: same hybrid shape
    model.addProjectManifest("/test/mixed/sub", "go.mod");
    model.addProjectManifest("/test/mixed", "package.json");

    QModelIndex mixedIdx = model.index(0, 0, model.index(0, 0));
    QVERIFY(mixedIdx.isValid());
    QCOMPARE(model.rowCount(mixedIdx), 2);

    QModelIndex entryIdx = model.index(0, 0, mixedIdx);
    QVERIFY(entryIdx.isValid());
    QCOMPARE(model.data(entryIdx, QmlTreeModel::ItemTypeRole).toString(), QString("manifests"));
    QCOMPARE(model.data(entryIdx, QmlTreeModel::FolderPathRole).toString(), QString("/test/mixed"));
    QCOMPARE(model.data(entryIdx, QmlTreeModel::ManifestRole).toString(), QString("package.json"));

    QModelIndex subIdx = model.index(1, 0, mixedIdx);
    QVERIFY(subIdx.isValid());
    QCOMPARE(model.data(subIdx, QmlTreeModel::ItemTypeRole).toString(), QString("folder"));
}

void QmlTests::test_qml_sidebar_component_loads()
{
    QQmlEngine engine;

    // Create QmlTreeModel in C++
    QmlTreeModel model;

    // Set as context property
    engine.rootContext()->setContextProperty("treeModel", &model);
    engine.rootContext()->setContextProperty("projectService", g_projectService);
    engine.rootContext()->setContextProperty("logModel", g_logModel);

    // Use file path for test
    QString buildDir = QDir::currentPath();
    QString qmlPath = buildDir + "/qml/Sidebar.qml";

    QQmlComponent component(&engine, QUrl::fromLocalFile(qmlPath));

    // Print errors for debugging
    for (const auto &err : component.errors()) {
        qWarning() << "QML Error:" << err.toString();
    }

    QVERIFY2(component.isReady(), qPrintable(QString("Component should be ready. Errors: %1").arg(
        component.errors().isEmpty() ? "none" : component.errors().first().toString())));

    if (component.isReady()) {
        QObject *obj = component.create();

        if (obj) {
            // Verify sidebar has width and height
            QQuickItem *sidebar = qobject_cast<QQuickItem*>(obj);
            QVERIFY(sidebar != nullptr);
            QCOMPARE(sidebar->width(), 240);

            obj->deleteLater();
        }
    }
}

void QmlTests::test_qml_tree_item_delegate_loads()
{
    QQmlEngine engine;

    // Use file path for test
    QString buildDir = QDir::currentPath();
    QString qmlPath = buildDir + "/qml/TreeItemDelegate.qml";

    QQmlComponent component(&engine, QUrl::fromLocalFile(qmlPath));

    // Print errors for debugging
    for (const auto &err : component.errors()) {
        qWarning() << "QML Error:" << err.toString();
    }

    // The component should load (may have warnings about undefined model properties
    // when created standalone, but that's expected)
    QVERIFY2(component.isReady(), qPrintable(QString("Component should be ready. Errors: %1").arg(
        component.errors().isEmpty() ? "none" : component.errors().first().toString())));
}

void QmlTests::test_sidebar_uses_theme()
{
    QQmlEngine engine;
    QmlTreeModel model;
    engine.rootContext()->setContextProperty("treeModel", &model);
    engine.rootContext()->setContextProperty("projectService", g_projectService);
    engine.rootContext()->setContextProperty("logModel", g_logModel);

    QString buildDir = QDir::currentPath();
    QString qmlPath = QDir::cleanPath(buildDir + "/../qml/Sidebar.qml");
    if (!QFile::exists(qmlPath))
        qmlPath = buildDir + "/qml/Sidebar.qml";

    QQmlComponent component(&engine, QUrl::fromLocalFile(qmlPath));
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));

    QObject *obj = component.create();
    QVERIFY(obj != nullptr);
    QScopedPointer<QObject> guard(obj);

    // Flat sidebar by design: transparent over the root tint, no radius.
    // (Previously a sidebarBackground card with radius 8.)
    const QColor bg = obj->property("color").value<QColor>();
    QVERIFY(bg == QColor(0, 0, 0, 0));
    QCOMPARE(obj->property("radius").toInt(), 0);
}

static QObject *loadDashboard(QQmlEngine &engine, const QString &buildDir)
{
    QString qmlPath = QDir::cleanPath(buildDir + "/../qml/Dashboard.qml");
    if (!QFile::exists(qmlPath))
        qmlPath = buildDir + "/qml/Dashboard.qml";
    QQmlComponent *component = new QQmlComponent(&engine, QUrl::fromLocalFile(qmlPath), &engine);
    if (!component->isReady()) {
        qWarning() << "Dashboard not ready:" << component->errors().first().toString();
        return nullptr;
    }
    return component->create();
}

void QmlTests::test_console_shows_selected_command()
{
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("projectService", g_projectService);
    engine.rootContext()->setContextProperty("logModel", g_logModel);
    LogModel commandLog;
    engine.rootContext()->setContextProperty("commandLog", &commandLog);
    static CommandExecutor commandExecutor;
    engine.rootContext()->setContextProperty("commandExecutor", &commandExecutor);
    engine.rootContext()->setContextProperty(
        "iconBaseUrl", QUrl::fromLocalFile(QDir::currentPath() + "/icons/").toString());

    QObject *obj = loadDashboard(engine, QDir::currentPath());
    QVERIFY(obj != nullptr);
    QScopedPointer<QObject> guard(obj);

    auto append = [&](const char *cmd, const char *level, const char *msg) {
        return QMetaObject::invokeMethod(obj, "appendCommandLine",
            Q_ARG(QVariant, QVariant(QString::fromUtf8(cmd))),
            Q_ARG(QVariant, QVariant(QString::fromUtf8(level))),
            Q_ARG(QVariant, QVariant(QString::fromUtf8("t"))),
            Q_ARG(QVariant, QVariant(QString::fromUtf8(msg))),
            Q_ARG(QVariant, QVariant(QStringLiteral(""))));
    };
    auto select = [&](const char *cmd) {
        return QMetaObject::invokeMethod(obj, "selectCommand",
            Q_ARG(QVariant, QVariant(QString::fromUtf8(cmd))));
    };
    auto messages = [&]() {
        QStringList out;
        for (int i = 0; i < commandLog.rowCount(); ++i)
            out << commandLog.data(commandLog.index(i, 0), LogModel::MessageRole).toString();
        return out;
    };

    QVERIFY(append("npm:build", "stdout", "build-line"));
    QVERIFY(append("npm:test", "stdout", "test-line"));
    QVERIFY(append("npm:build", "stdout", "build-line-2"));

    // Nothing selected yet: console stays empty
    QCOMPARE(commandLog.rowCount(), 0);

    QVERIFY(select("npm:test"));
    QCOMPARE(messages(), QStringList({"test-line"}));

    QVERIFY(select("npm:build"));
    QCOMPARE(messages(), QStringList({"build-line", "build-line-2"}));
}

void QmlTests::test_console_run_segments()
{
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("projectService", g_projectService);
    engine.rootContext()->setContextProperty("logModel", g_logModel);
    LogModel commandLog;
    engine.rootContext()->setContextProperty("commandLog", &commandLog);
    static CommandExecutor commandExecutor;
    engine.rootContext()->setContextProperty("commandExecutor", &commandExecutor);
    engine.rootContext()->setContextProperty(
        "iconBaseUrl", QUrl::fromLocalFile(QDir::currentPath() + "/icons/").toString());

    QObject *obj = loadDashboard(engine, QDir::currentPath());
    QVERIFY(obj != nullptr);
    QScopedPointer<QObject> guard(obj);

    auto append = [&](const char *cmd, const char *level, const char *msg) {
        return QMetaObject::invokeMethod(obj, "appendCommandLine",
            Q_ARG(QVariant, QVariant(QString::fromUtf8(cmd))),
            Q_ARG(QVariant, QVariant(QString::fromUtf8(level))),
            Q_ARG(QVariant, QVariant(QString::fromUtf8("t"))),
            Q_ARG(QVariant, QVariant(QString::fromUtf8(msg))),
            Q_ARG(QVariant, QVariant(QStringLiteral(""))));
    };
    auto startRun = [&](const char *cmd, const char *label, const char *line) {
        return QMetaObject::invokeMethod(obj, "startRun",
            Q_ARG(QVariant, QVariant(QString::fromUtf8(cmd))),
            Q_ARG(QVariant, QVariant(QString::fromUtf8(label))),
            Q_ARG(QVariant, QVariant(QString::fromUtf8(line))));
    };
    auto finishRun = [&](const char *cmd, int exitCode) {
        return QMetaObject::invokeMethod(obj, "finishRun",
            Q_ARG(QVariant, QVariant(QString::fromUtf8(cmd))),
            Q_ARG(QVariant, QVariant(QStringLiteral("label"))),
            Q_ARG(QVariant, QVariant(exitCode)),
            Q_ARG(QVariant, QVariant(QStringLiteral(""))));
    };
    auto showRun = [&](const char *cmd, int seq) {
        return QMetaObject::invokeMethod(obj, "showRun",
            Q_ARG(QVariant, QVariant(QString::fromUtf8(cmd))),
            Q_ARG(QVariant, QVariant(seq)));
    };
    auto select = [&](const char *cmd) {
        return QMetaObject::invokeMethod(obj, "selectCommand",
            Q_ARG(QVariant, QVariant(QString::fromUtf8(cmd))));
    };
    auto messages = [&]() {
        QStringList out;
        for (int i = 0; i < commandLog.rowCount(); ++i)
            out << commandLog.data(commandLog.index(i, 0), LogModel::MessageRole).toString();
        return out;
    };
    auto menu = [&](const char *cmd) -> QVariantList {
        QVariant ret;
        if (!QMetaObject::invokeMethod(obj, "runMenu",
                Q_RETURN_ARG(QVariant, ret),
                Q_ARG(QVariant, QVariant(QString::fromUtf8(cmd)))))
            return {};
        return ret.toList();
    };

    // A pre-run system line belongs to no segment but stays visible.
    QVERIFY(append("npm:build", "system", "pre-run hint"));
    QVERIFY(startRun("npm:build", "build", "npm run build"));
    QVERIFY(append("npm:build", "stdout", "first output"));

    QVERIFY(select("npm:build"));
    QCOMPARE(messages(), QStringList({"pre-run hint", "npm run build", "first output"}));

    QVERIFY(finishRun("npm:build", 0));
    QCOMPARE(menu("npm:build").size(), 1);
    QVERIFY(menu("npm:build").first().toMap().value("label").toString().startsWith(QStringLiteral("#1 · ")));

    // A second run segments the console: only its own lines show.
    QVERIFY(startRun("npm:build", "build", "npm run build"));
    QVERIFY(append("npm:build", "stdout", "second output"));
    QCOMPARE(messages(), QStringList({"pre-run hint", "npm run build", "second output"}));

    // Back to the first segment.
    QVERIFY(showRun("npm:build", 1));
    QCOMPARE(messages(), QStringList({"pre-run hint", "npm run build", "first output"}));
}

void QmlTests::test_console_parameters_option_kinds()
{
    QQmlEngine engine;

    // Fake Settings: plain key/value store.
    QQmlComponent storeComponent(&engine);
    storeComponent.setData("import QtQml\nQtObject {\n"
                           "property var data: ({})\n"
                           "function get(key, def) { return data[key] !== undefined ? data[key] : def }\n"
                           "function set(key, value) { data[key] = value }\n}",
                           QUrl());
    QVERIFY2(storeComponent.isReady(), "store harness not ready");
    QObject *store = storeComponent.create();
    QVERIFY(store != nullptr);
    QScopedPointer<QObject> guardStore(store);

    auto optMeta = [](const char *name, bool valueRequired, bool acceptValue,
                      const char *def, bool multiple = false) {
        QVariantMap m{{QStringLiteral("kind"), QStringLiteral("option")},
                      {QStringLiteral("name"), QString::fromUtf8(name)},
                      {QStringLiteral("required"), false},
                      {QStringLiteral("valueRequired"), valueRequired},
                      {QStringLiteral("acceptValue"), acceptValue},
                      {QStringLiteral("hidden"), false},
                      {QStringLiteral("description"), QStringLiteral("d %1").arg(QString::fromUtf8(name))}};
        if (def != nullptr)
            m.insert(QStringLiteral("default"), QString::fromUtf8(def));
        if (multiple)
            m.insert(QStringLiteral("multiple"), true);
        return m;
    };
    QVariantList meta{
        QVariantMap{{QStringLiteral("kind"), QStringLiteral("positional")},
                    {QStringLiteral("name"), QStringLiteral("target")},
                    {QStringLiteral("required"), true},
                    {QStringLiteral("description"), QStringLiteral("t")}},
        optMeta("campaign", true, true, nullptr), // VALUE_REQUIRED, no default: auto-required
        optMeta("weeks", false, true, "5"), // VALUE_OPTIONAL: input, not a flag
        optMeta("visitors", false, true, "30"),
        optMeta("outliers", false, false, nullptr), // VALUE_NONE: flag chip
    };
    const QVariantMap command{{QStringLiteral("command"), QStringLiteral("bin/console")},
                              {QStringLiteral("label"), QStringLiteral("ply:populate:campaign")},
                              {QStringLiteral("description"), QStringLiteral("Populate")},
                              {QStringLiteral("argsMeta"), meta}};

    // Stored flag-era residue: weeks was a chip storing "on".
    store->setProperty("data", QVariantMap{
        {QStringLiteral("runConfig/p/c"),
         QStringLiteral(R"({"argsValues":{"weeks":"on","campaign":"15"},"addedOptions":["weeks","outliers"]})")},
    });

    QQmlComponent component(&engine, QUrl::fromLocalFile(sourceQmlPath("ConsoleParameters.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));
    QObject *panel = component.create();
    QVERIFY(panel != nullptr);
    QScopedPointer<QObject> guard(panel);
    QVERIFY(panel->setProperty("store", QVariant::fromValue(store)));
    QVERIFY(panel->setProperty("projectId", QStringLiteral("p")));
    QVERIFY(panel->setProperty("commandId", QStringLiteral("c")));
    QVERIFY(panel->setProperty("command", command));
    QVERIFY(QMetaObject::invokeMethod(panel, "reload"));

    // Stale "on" migrates to the declared default; real values are kept.
    const QVariantMap values = panel->property("values").toMap();
    QCOMPARE(values.value("weeks").toString(), QStringLiteral("5"));
    QCOMPARE(values.value("campaign").toString(), QStringLiteral("15"));

    // Classification: required/optional values are inputs, VALUE_NONE a flag.
    auto kinds = [&]() {
        QVariantMap out;
        const QVariantList defs = panel->property("optionDefs").toList();
        for (const QVariant &v : defs)
            out.insert(v.toMap().value("name").toString(),
                       v.toMap().value("kind").toString());
        return out;
    };
    const QVariantMap k = kinds();
    QCOMPARE(k.value("campaign").toString(), QStringLiteral("value"));
    QCOMPARE(k.value("weeks").toString(), QStringLiteral("value"));
    QCOMPARE(k.value("outliers").toString(), QStringLiteral("flag"));

    // campaign is auto-required: always in the grid (locked), never in
    // the "+ Add option" list.
    QVariant ret;
    QVERIFY(QMetaObject::invokeMethod(panel, "isAutoRequired",
        Q_RETURN_ARG(QVariant, ret),
        Q_ARG(QVariant, QVariant(QStringLiteral("campaign")))));
    QVERIFY(ret.toBool());
    QVERIFY(QMetaObject::invokeMethod(panel, "isAutoRequired",
        Q_RETURN_ARG(QVariant, ret),
        Q_ARG(QVariant, QVariant(QStringLiteral("weeks")))));
    QVERIFY(!ret.toBool());
    bool campaignLocked = false, campaignInGrid = false, campaignOffered = false;
    for (const QVariant &v : panel->property("gridFields").toList()) {
        if (v.toMap().value("name").toString() == QStringLiteral("campaign")) {
            campaignInGrid = true;
            campaignLocked = v.toMap().value("locked").toBool();
        }
    }
    QVERIFY(campaignInGrid);
    QVERIFY(campaignLocked);
    for (const QVariant &v : panel->property("availableOptions").toList()) {
        if (v.toMap().value("name").toString() == QStringLiteral("campaign"))
            campaignOffered = true;
    }
    QVERIFY(!campaignOffered);

    // Adding an option with a default prefills it; flags switch "on".
    QVERIFY(QMetaObject::invokeMethod(panel, "addOption",
        Q_ARG(QVariant, QVariant(QStringLiteral("visitors")))));
    QCOMPARE(panel->property("values").toMap().value("visitors").toString(),
             QStringLiteral("30"));
}

void QmlTests::test_console_format_colors()
{
    QQmlEngine engine;
    const QString jsUrl = QUrl::fromLocalFile(sourceQmlPath("ConsoleFormat.js")).toString();
    const QString qml = QStringLiteral("import QtQml\nimport \"%1\" as CF\nQtObject {\n"
        "property string plain: CF.formatLine(420, \"hello\", \"\", \"#888\", \"#eab308\")\n"
        "property string echo: CF.formatLine(0, \"php bin/console foo\", \"echo\", \"#888\", \"#eab308\")\n"
        "property string long: CF.formatLine(125000, \"later\", \"\", \"#888\", \"#eab308\")\n"
        "property string colored: CF.ansiToHtml(\"\\u001b[31mred\\u001b[0m ok\")\n"
        "property string escaped: CF.formatLine(0, \"a<b>&c\", \"\", \"#888\", \"#eab308\")\n"
        "}").arg(jsUrl);
    QQmlComponent component(&engine);
    component.setData(qml.toUtf8(), QUrl());
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));
    QObject *obj = component.create();
    QVERIFY(obj != nullptr);
    QScopedPointer<QObject> guard(obj);

    const QString plain = obj->property("plain").toString();
    QVERIFY(plain.contains(QStringLiteral("+0.42s")));
    QVERIFY(plain.contains(QStringLiteral("hello")));
    QVERIFY(!plain.contains(QStringLiteral("color:#eab308")));
    const QString echo = obj->property("echo").toString();
    QVERIFY(echo.contains(QStringLiteral("+0.00s")));
    QVERIFY(echo.contains(QStringLiteral("color:#eab308")));
    QVERIFY(echo.contains(QStringLiteral("php bin/console foo")));
    const QString longRow = obj->property("long").toString();
    QVERIFY(longRow.contains(QStringLiteral("+2:05.00")));
    const QString colored = obj->property("colored").toString();
    QVERIFY(colored.contains(QStringLiteral("color:#ef4444")));
    QVERIFY(colored.contains(QStringLiteral("red")));
    QVERIFY(colored.contains(QStringLiteral("ok")));
    QVERIFY(!colored.contains(QStringLiteral("\x1b")));
    const QString escaped = obj->property("escaped").toString();
    QVERIFY(escaped.contains(QStringLiteral("a&lt;b&gt;&amp;c")));
}

void QmlTests::test_tool_plugins()
{
    QQmlEngine engine;
    const QString jsUrl = QUrl::fromLocalFile(sourceQmlPath("ToolPlugins.js")).toString();
    const QString qml = QStringLiteral("import QtQml\nimport \"%1\" as TP\nQtObject {\n"
        "property var vitePkg: TP.pluginFor(\"vite\", \"package.json\")\n"
        "property var viteComposer: TP.pluginFor(\"vite\", \"composer.json\")\n"
        "property var symfony: TP.pluginFor(\"symfony\", \"composer.json\")\n"
        "property var unknown: TP.pluginFor(\"webpack\", \"package.json\")\n"
        "property var empty: TP.pluginFor(\"\", \"package.json\")\n"
        "property var viteHit: TP.matchBusy(TP.busyTriggers(TP.pluginFor(\"vite\", \"package.json\")), \"Port 5173 is in use, trying another one...\")\n"
        "property var genericHit: TP.matchBusy(TP.busyTriggers(null), \"Error: listen EADDRINUSE: address already in use :::3000\")\n"
        "property var missHit: TP.matchBusy(TP.busyTriggers(null), \"Server started ok\")\n"
        "property int listenBull: TP.matchListening(\"Bull admin listening on port 8040\")\n"
        "property int listenSymfony: TP.matchListening(\"Listening on https://127.0.0.1:8000\")\n"
        "property int listenNone: TP.matchListening(\"Error: connect ECONNREFUSED 127.0.0.1:6379\")\n"
        "property string stripped: TP.stripSgr(\"a\\u001b[31mb\\u001b[0mc\")\n"
        "property var refused: TP.parseRefused(\"Error: connect ECONNREFUSED 127.0.0.1:6379\")\n"
        "property var refusedNone: TP.parseRefused(\"Server started ok\")\n"
        "property string svcRedis: TP.serviceName(6379)\n"
        "property string svcUnknown: TP.serviceName(1234)\n"
        "property var lineOwned: TP.matchLine(\"vite\", \"package.json\", \"Port 5173 is in use, trying another one...\")\n"
        "property var lineProxy: TP.matchLine(\"\", \"package.json\", \"Port 5173 is in use, trying another one...\")\n"
        "property var lineWrongManifest: TP.matchLine(\"\", \"composer.json\", \"Port 5173 is in use, trying another one...\")\n"
        "property var lineOwnedGeneric: TP.matchLine(\"vite\", \"package.json\", \"Error: listen EADDRINUSE: address already in use :::3000\")\n"
        "}").arg(jsUrl);
    QQmlComponent component(&engine);
    component.setData(qml.toUtf8(), QUrl());
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));
    QObject *obj = component.create();
    QVERIFY(obj != nullptr);
    QScopedPointer<QObject> guard(obj);

    // Ownership: vite only for package.json, symfony for composer.json
    QCOMPARE(obj->property("vitePkg").toMap().value("id").toString(), QString("vite"));
    QVERIFY(obj->property("viteComposer").isNull());
    QCOMPARE(obj->property("symfony").toMap().value("id").toString(), QString("symfony"));
    QVERIFY(obj->property("unknown").isNull());
    QVERIFY(obj->property("empty").isNull());

    // Triggers: vite busy line carries its port, generic does not
    const QVariantMap viteHit = obj->property("viteHit").toMap();
    QVERIFY(viteHit.value("matched").toBool());
    QCOMPARE(viteHit.value("port").toInt(), 5173);
    const QVariantMap genericHit = obj->property("genericHit").toMap();
    QVERIFY(genericHit.value("matched").toBool());
    QCOMPARE(genericHit.value("port").toInt(), 0);
    QVERIFY(!obj->property("missHit").toMap().value("matched").toBool());

    // Sniffing incl. host-less form, excluding client errors
    QCOMPARE(obj->property("listenBull").toInt(), 8040);
    QCOMPARE(obj->property("listenSymfony").toInt(), 8000);
    QCOMPARE(obj->property("listenNone").toInt(), 0);
    QCOMPARE(obj->property("stripped").toString(), QString("abc"));

    // Line ownership: owned command, proxied script, wrong manifest
    const QVariantMap lineOwned = obj->property("lineOwned").toMap();
    QVERIFY(lineOwned.value("tool").toBool());
    QCOMPARE(lineOwned.value("plugin").toMap().value("id").toString(), QString("vite"));
    QCOMPARE(lineOwned.value("hit").toMap().value("port").toInt(), 5173);
    const QVariantMap lineProxy = obj->property("lineProxy").toMap();
    QVERIFY(lineProxy.value("tool").toBool());
    QCOMPARE(lineProxy.value("plugin").toMap().value("id").toString(), QString("vite"));
    const QVariantMap lineWrongManifest = obj->property("lineWrongManifest").toMap();
    QVERIFY(!lineWrongManifest.value("hit").toMap().value("matched").toBool());
    // Generic trigger on an owned command: no tool hint
    const QVariantMap lineOwnedGeneric = obj->property("lineOwnedGeneric").toMap();
    QVERIFY(lineOwnedGeneric.value("hit").toMap().value("matched").toBool());
    QVERIFY(!lineOwnedGeneric.value("tool").toBool());

    // Refused dependencies with well-known service names
    const QVariantMap refused = obj->property("refused").toMap();
    QCOMPARE(refused.value("host").toString(), QString("127.0.0.1"));
    QCOMPARE(refused.value("port").toInt(), 6379);
    QVERIFY(obj->property("refusedNone").isNull());
    QCOMPARE(obj->property("svcRedis").toString(), QString("Redis"));
    QVERIFY(obj->property("svcUnknown").toString().isEmpty());
}

void QmlTests::test_dep_offer_opens_dialog()
{
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("projectService", g_projectService);
    engine.rootContext()->setContextProperty("logModel", g_logModel);
    LogModel commandLog;
    engine.rootContext()->setContextProperty("commandLog", &commandLog);
    static CommandExecutor commandExecutor;
    engine.rootContext()->setContextProperty("commandExecutor", &commandExecutor);
    engine.rootContext()->setContextProperty(
        "iconBaseUrl", QUrl::fromLocalFile(QDir::currentPath() + "/icons/").toString());
    // NOTE: no databaseService here on purpose — the offer is decided by
    // the log line alone, with no service lookup involved.

    QObject *obj = loadDashboard(engine, QDir::currentPath());
    QVERIFY(obj != nullptr);
    QScopedPointer<QObject> guard(obj);

    QQuickItem *detailPage = obj->findChild<QQuickItem*>(QStringLiteral("detailPage"));
    QVERIFY(detailPage != nullptr);
    QVERIFY(obj->setProperty("activePage", QStringLiteral("detail")));
    QVariant key;
    QVERIFY(QMetaObject::invokeMethod(obj, "fullKey",
        Q_RETURN_ARG(QVariant, key),
        Q_ARG(QVariant, QVariant(QStringLiteral("npm:dev")))));
    QVERIFY(detailPage->setProperty("commandId", key));

    // Refused Redis line → dialog, props straight from the line
    QVariantMap refused{{QStringLiteral("host"), QStringLiteral("127.0.0.1")},
                        {QStringLiteral("port"), 6379}};
    QVariant ret;
    QVERIFY(QMetaObject::invokeMethod(obj, "offerRefusedRun",
        Q_RETURN_ARG(QVariant, ret),
        Q_ARG(QVariant, key),
        Q_ARG(QVariant, QVariant::fromValue(refused))));
    QVERIFY2(ret.toBool(), "refused offer must open from the log line alone");
    QCOMPARE(detailPage->property("depHost").toString(), QString("127.0.0.1"));
    QCOMPARE(detailPage->property("depPort").toInt(), 6379);
    QCOMPARE(detailPage->property("depSvc").toString(), QString("Redis"));

    // Off the detail page: no dialog, hint path instead
    QVERIFY(obj->setProperty("activePage", QStringLiteral("dashboard")));
    QVariant ret2;
    QVERIFY(QMetaObject::invokeMethod(obj, "offerRefusedRun",
        Q_RETURN_ARG(QVariant, ret2),
        Q_ARG(QVariant, key),
        Q_ARG(QVariant, QVariant::fromValue(refused))));
    QVERIFY(!ret2.toBool());
}

static QString sourceQmlPath(const QString &fileName)
{
    const QString buildDir = QDir::currentPath();
    const QString src = QDir::cleanPath(buildDir + "/../qml/" + fileName);
    if (QFile::exists(src))
        return src;
    return buildDir + "/qml/" + fileName;
}

void QmlTests::test_sniff_detects_port_from_url()
{
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("projectService", g_projectService);
    engine.rootContext()->setContextProperty("logModel", g_logModel);
    LogModel commandLog;
    engine.rootContext()->setContextProperty("commandLog", &commandLog);
    static CommandExecutor commandExecutor;
    engine.rootContext()->setContextProperty("commandExecutor", &commandExecutor);
    engine.rootContext()->setContextProperty(
        "iconBaseUrl", QUrl::fromLocalFile(QDir::currentPath() + "/icons/").toString());

    QObject *obj = loadDashboard(engine, QDir::currentPath());
    QVERIFY(obj != nullptr);
    QScopedPointer<QObject> guard(obj);

    QVERIFY(QMetaObject::invokeMethod(obj, "appendCommandLine",
        Q_ARG(QVariant, QVariant(QStringLiteral("npm:serve"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("stdout"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("t"))),
        Q_ARG(QVariant, QVariant(QString::fromUtf8("  ➜  Local:   http://localhost:5173/"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("")))));
    const QVariantMap ports = obj->property("detectedPorts").toMap();
    // No active project in this harness: key is "|npm:serve"
    QCOMPARE(ports.value(QStringLiteral("|npm:serve")).toInt(), 5173);

    // Symfony CLI "Listening on" form, incl. schemeless host:port
    QVERIFY(QMetaObject::invokeMethod(obj, "appendCommandLine",
        Q_ARG(QVariant, QVariant(QStringLiteral("composer:serve"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("stdout"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("t"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("Listening on 127.0.0.1:8000"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("")))));
    const QVariantMap ports2 = obj->property("detectedPorts").toMap();
    QCOMPARE(ports2.value(QStringLiteral("|composer:serve")).toInt(), 8000);

    // Bull-style "listening on port N" with no host at all
    QVERIFY(QMetaObject::invokeMethod(obj, "appendCommandLine",
        Q_ARG(QVariant, QVariant(QStringLiteral("npm:dev"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("stdout"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("t"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("Bull admin listening on port 8040"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("")))));
    const QVariantMap ports3 = obj->property("detectedPorts").toMap();
    QCOMPARE(ports3.value(QStringLiteral("|npm:dev")).toInt(), 8040);
}

void QmlTests::test_detail_page_loads()
{
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("projectService", g_projectService);
    engine.rootContext()->setContextProperty("logModel", g_logModel);
    static LogModel commandLog;
    engine.rootContext()->setContextProperty("commandLog", &commandLog);
    static CommandExecutor commandExecutor;
    engine.rootContext()->setContextProperty("commandExecutor", &commandExecutor);
    engine.rootContext()->setContextProperty(
        "iconBaseUrl", QUrl::fromLocalFile(QDir::currentPath() + "/icons/").toString());

    QQmlComponent component(&engine,
        QUrl::fromLocalFile(sourceQmlPath("DetailPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));

    QObject *obj = component.create();
    QVERIFY(obj != nullptr);
    QScopedPointer<QObject> guard(obj);
    // Empty command id renders the idle state without errors
    QCOMPARE(obj->property("commandId").toString(), QString(""));
}

void QmlTests::test_run_config_dialog_loads()
{
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(
        "iconBaseUrl", QUrl::fromLocalFile(QDir::currentPath() + "/icons/").toString());

    QQmlComponent component(&engine,
        QUrl::fromLocalFile(sourceQmlPath("RunConfigDialog.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));

    QObject *obj = component.create();
    QVERIFY(obj != nullptr);
    QScopedPointer<QObject> guard(obj);
    // Defaults propagate, nothing stored yet
    QCOMPARE(obj->property("defaultName").toString(), QString(""));
    QVERIFY(obj->property("configKey").toString().isEmpty());
}

void QmlTests::test_mcp_setup_dialog_loads()
{
    QQmlEngine engine;
    McpAgentManager mcpAgents;
    engine.rootContext()->setContextProperty("mcpAgents", &mcpAgents);

    QQmlComponent component(&engine,
        QUrl::fromLocalFile(sourceQmlPath("McpSetupDialog.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));

    QObject *obj = component.create();
    QVERIFY(obj != nullptr);
    QScopedPointer<QObject> guard(obj);
    QVERIFY(obj->property("agents").toList().isEmpty());
}

void QmlTests::test_mcp_menu_geometry()
{
    QQmlEngine engine;

    QQmlComponent component(&engine,
        QUrl::fromLocalFile(sourceQmlPath("McpMenu.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));

    QObject *menu = component.create();
    QVERIFY(menu != nullptr);
    QScopedPointer<QObject> guard(menu);

    // Standalone menu component: fixed size by construction
    QCOMPARE(menu->property("width").toReal(), 180.0);
    menu->setProperty("visible", true);
    QTest::qWait(50);
    qDebug() << "menu opened:" << menu->property("visible").toBool()
             << menu->property("width").toReal()
             << "x" << menu->property("height").toReal();
    QVERIFY(menu->property("visible").toBool());
    QVERIFY2(menu->property("width").toReal() >= 150.0,
             qPrintable(QString("menu too narrow: %1").arg(menu->property("width").toReal())));
    QVERIFY(menu->property("height").toReal() >= 60.0);
    // Action signals for the instantiator to wire up
    const QMetaObject *meta = menu->metaObject();
    QVERIFY(meta->indexOfSignal("enableAllRequested()") != -1);
    QVERIFY(meta->indexOfSignal("disableAllRequested()") != -1);
    QVERIFY(meta->indexOfSignal("configureRequested()") != -1);
}

void QmlTests::test_stat_card_loads()
{
    QQmlEngine engine;
    QQmlComponent component(&engine,
        QUrl::fromLocalFile(sourceQmlPath("StatCard.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));

    QObject *obj = component.create();
    QVERIFY(obj != nullptr);
    QScopedPointer<QObject> guard(obj);
    QCOMPARE(obj->property("value").toString(), QString("—"));
    QCOMPARE(obj->property("ringFraction").toDouble(), -1.0);
}

void QmlTests::test_icons_in_assets()
{
    // Verify all expected icon files exist: dev copy is build/icons,
    // source lives in <repo>/assets/icons
    const QString buildDir = QDir::currentPath();
    QString iconDir = buildDir + "/icons";
    if (!QFile::exists(iconDir + "/npm.svg"))
        iconDir = QDir::cleanPath(buildDir + "/../assets/icons");

    // Check that npm.png exists (referenced by package.json projects)
    QVERIFY(QFile::exists(iconDir + "/npm.png"));

    // Check that rust light/dark exist (referenced by Cargo.toml projects)
    QVERIFY(QFile::exists(iconDir + "/rust.png"));
    QVERIFY(QFile::exists(iconDir + "/rust-dark.png"));

    // Check that go light/dark exist (referenced by go.mod projects)
    QVERIFY(QFile::exists(iconDir + "/go.png"));
    QVERIFY(QFile::exists(iconDir + "/go-dark.png"));

    // Check that python.png exists (referenced by pyproject.toml projects)
    QVERIFY(QFile::exists(iconDir + "/python.png"));

    // Check that composer.png exists (referenced by composer.json projects)
    QVERIFY(QFile::exists(iconDir + "/composer.png"));

    // Check that default.png exists
    QVERIFY(QFile::exists(iconDir + "/default.png"));

    // Check that the folders stack exists (folder rows incl. roots, Browse button)
    QVERIFY(QFile::exists(iconDir + "/folders.png"));
    QVERIFY(QFile::exists(iconDir + "/folders-dark.png"));

    // Check that the folders stack exists (plain folder rows, Browse button)
    QVERIFY(QFile::exists(iconDir + "/folders.png"));
    QVERIFY(QFile::exists(iconDir + "/folders-dark.png"));

    // Check bash.png (custom-commands section header)
    QVERIFY(QFile::exists(iconDir + "/bash.png"));

    // Sidebar iconography: file marker for manifest rows,
    // stack for plain folders, each with a light variant for dark theme
    QVERIFY(QFile::exists(iconDir + "/file.png"));
    QVERIFY(QFile::exists(iconDir + "/file-dark.png"));
    QVERIFY(QFile::exists(iconDir + "/folders.png"));
    QVERIFY(QFile::exists(iconDir + "/folders-dark.png"));

    // Check UI glyphs used by icon-only ghost buttons (clean, chevron, cog)
    QVERIFY(QFile::exists(iconDir + "/clean.png"));
    QVERIFY(QFile::exists(iconDir + "/chevron-down.png"));
    QVERIFY(QFile::exists(iconDir + "/chevron-down-dark.png"));
    QVERIFY(QFile::exists(iconDir + "/chevron-up.png"));
    QVERIFY(QFile::exists(iconDir + "/chevron-up-dark.png"));
    QVERIFY(QFile::exists(iconDir + "/cog.png"));
    QVERIFY(QFile::exists(iconDir + "/cog-dark.png"));
    QVERIFY(QFile::exists(iconDir + "/terminal.png"));
    QVERIFY(QFile::exists(iconDir + "/terminal-dark.png"));
    QVERIFY(QFile::exists(iconDir + "/mcp.png"));
    QVERIFY(QFile::exists(iconDir + "/mcp-dark.png"));
}

void QmlTests::test_qrc_icon_paths_in_qml()
{
    // Verify that TreeItemDelegate.qml uses qrc:/icons/ paths (not relative paths)
    // Prefer the source file: build/qml copy is only refreshed at configure time
    QString buildDir = QDir::currentPath();
    QString qmlPath = QDir::cleanPath(buildDir + "/../qml/TreeItemDelegate.qml");
    if (!QFile::exists(qmlPath))
        qmlPath = buildDir + "/qml/TreeItemDelegate.qml";

    QFile file(qmlPath);
    QVERIFY(file.exists());
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    // Verify absolute icon URLs are used (qrc:/icons/, iconBaseUrl or Theme.icon-based),
    // never relative "icons/" paths (those break when resources are embedded)
    QRegularExpression absPattern(QStringLiteral("(qrc:/icons/|iconBaseUrl|Theme\\.icon)"));
    QRegularExpressionMatchIterator it = absPattern.globalMatch(content);
    int matchCount = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        matchCount++;
    }

    // There should be multiple absolute icon references
    QVERIFY(matchCount > 0);

    // Verify NO relative icon paths (icons/foo.svg without qrc:/ prefix)
    QRegularExpression relativePattern(QStringLiteral("[\"']icons/[\\w-]+\\.svg[\"']"));
    QRegularExpressionMatchIterator it2 = relativePattern.globalMatch(content);
    int relativeCount = 0;
    while (it2.hasNext()) {
        QRegularExpressionMatch match = it2.next();
        relativeCount++;
    }

    QCOMPARE(relativeCount, 0);
}

static void dumpGeometry(QObject *obj, int indent = 0)
{
    if (auto *item = qobject_cast<QQuickItem*>(obj)) {
        qDebug().noquote().nospace()
            << QString(indent * 2, u' ')
            << item->metaObject()->className()
            << " y=" << item->y() << " h=" << item->height()
            << " w=" << item->width() << " vis=" << item->isVisible();
    }
    for (auto *c : obj->children())
        dumpGeometry(c, indent + 1);
}

void QmlTests::test_dashboard_layout_geometry()
{
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("projectService", g_projectService);
    engine.rootContext()->setContextProperty("logModel", g_logModel);
    static LogModel commandLog;
    engine.rootContext()->setContextProperty("commandLog", &commandLog);
    static CommandExecutor commandExecutor;
    engine.rootContext()->setContextProperty("commandExecutor", &commandExecutor);

    // Prefer source QML (build copy refreshes only at configure time)
    QString buildDir = QDir::currentPath();
    engine.rootContext()->setContextProperty(
        "iconBaseUrl", QUrl::fromLocalFile(buildDir + "/icons/").toString());
    QString qmlPath = QDir::cleanPath(buildDir + "/../qml/Dashboard.qml");
    if (!QFile::exists(qmlPath))
        qmlPath = buildDir + "/qml/Dashboard.qml";

    QQmlComponent component(&engine, QUrl::fromLocalFile(qmlPath));
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));

    // Host with a fixed size: Dashboard binds width/height to parent
    QQuickItem host;
    host.setWidth(960);
    host.setHeight(800);

    QObject *obj = component.createWithInitialProperties({}, engine.rootContext());
    QVERIFY(obj != nullptr);
    auto *dashboard = qobject_cast<QQuickItem*>(obj);
    QVERIFY(dashboard != nullptr);
    dashboard->setParentItem(&host);

    // Layout sanity: nothing selected, no activity yet
    QTest::qWait(50);
    dumpGeometry(dashboard);

    QQuickItem *gridView = dashboard->findChild<QQuickItem*>("gridView");
    QVERIFY(gridView != nullptr);
    QQuickItem *detailPage = dashboard->findChild<QQuickItem*>("detailPage");
    QVERIFY(detailPage != nullptr);
    QVERIFY(!detailPage->isEnabled());

    QQuickItem *header = gridView->childItems().at(0);
    QQuickItem *stats = gridView->findChild<QQuickItem*>("systemStatsRow");
    QQuickItem *activity = gridView->findChild<QQuickItem*>("activityView");
    QVERIFY(stats != nullptr);
    QVERIFY(activity != nullptr);

    // Header pinned to the top with 48px height; stats in the page body
    QCOMPARE(header->y(), 0.0);
    QCOMPARE(header->height(), 48.0);
    QVERIFY(stats->height() > 0.0);

    // Activity view sits below the stats with a 24px gap
    QCOMPARE(activity->y(), stats->y() + stats->height() + 24.0);

    // Nothing recent, nothing running: empty state is shown
    QVERIFY(obj->property("activityEmpty").toBool());

    // Scanning a project keeps the header/stats pinned
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir().mkpath(tmp.path() + "/app");
    QFile manifest(tmp.path() + "/app/package.json");
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    manifest.write(R"({"name":"probe","scripts":{"build":"tsc","test":"jest","lint":"eslint","serve":"vite"}})");
    manifest.close();

    g_projectService->scanFolder(tmp.path());
    QVERIFY(g_projectService->projectCount() > 0);
    g_projectService->selectProject(g_projectService->projects().first()["id"].toString());
    QTest::qWait(50);
    dumpGeometry(dashboard);

    QCOMPARE(header->y(), 0.0);
    QCOMPARE(header->height(), 48.0);
    QCOMPARE(activity->y(), stats->y() + stats->height() + 24.0);

    // Navigation: selecting a folder switches to the folder page
    QVERIFY(QMetaObject::invokeMethod(obj, "openFolder",
        Q_ARG(QVariant, QVariant(tmp.path() + "/app"))));
    QCOMPARE(obj->property("activePage").toString(), QString("folder"));
    QVERIFY(QMetaObject::invokeMethod(obj, "goHome"));
    QCOMPARE(obj->property("activePage").toString(), QString("dashboard"));

    // Detail page: opens with a slide, closes back to hidden
    const QString expectedId = g_projectService->projects().first()["commands"]
        .toArray().first().toObject()["id"].toString();
    QVERIFY(!expectedId.isEmpty());
    const QString expectedKey =
        g_projectService->projects().first()["id"].toString() + u'|' + expectedId;

    // Port parsing for the "port busy / Kill & Run" prompt
    {
        QVariant ret;
        QVERIFY(QMetaObject::invokeMethod(obj, "portFromError",
            Q_RETURN_ARG(QVariant, ret),
            Q_ARG(QVariant, QVariant(QStringLiteral(
                "Error: listen EADDRINUSE: address already in use 0.0.0.0:8050")))));
        QCOMPARE(ret.toInt(), 8050);
        QVERIFY(QMetaObject::invokeMethod(obj, "portFromError",
            Q_RETURN_ARG(QVariant, ret),
            Q_ARG(QVariant, QVariant(QStringLiteral("  port: 3000")))));
        QCOMPARE(ret.toInt(), 3000);
    }

    QVERIFY(QMetaObject::invokeMethod(obj, "openDetail", Q_ARG(QVariant, QVariant(expectedId))));
    // Detail is a page like the others (cross-slide, no separate overlay)
    QCOMPARE(obj->property("activePage").toString(), QString("detail"));
    QCOMPARE(obj->property("detailCommandId").toString(), expectedKey);

    // Regression guard: the command's default args must be loaded into the
    // run config, otherwise Run launches the bare executable (e.g. "npm").
    QTest::qWait(50);
    QVERIFY2(!detailPage->property("cfgArgsText").toString().isEmpty(),
             qPrintable(QString("run args not loaded (cfgArgsText empty)")));

    QVERIFY(QMetaObject::invokeMethod(obj, "closeDetail"));
    QCOMPARE(obj->property("activePage").toString(), QString("dashboard"));
    QCOMPARE(obj->property("detailCommandId").toString(), QString(""));
}

void QmlTests::test_run_all_recent()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir().mkpath(tmp.path() + "/app"));
    QFile manifest(tmp.path() + "/app/package.json");
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    manifest.write(R"({"name":"probe","scripts":{"serve":"vite"}})");
    manifest.close();

    Settings settings(tmp.path() + "/trun.ini");
    ProjectListModel model;
    ProjectService service(&model);
    service.setSettings(&settings);
    CommandExecutor executor;
    service.setExecutor(&executor);
    service.scanFolder(tmp.path());
    QVERIFY(service.projectCount() > 0);

    const QString folder = tmp.path() + "/app";
    const QString result = service.addCustomCommand(
        QStringLiteral("work"), folder, QStringLiteral("/bin/sleep"),
        QStringLiteral("30"), QStringLiteral(""), QStringLiteral(""), false);
    QVERIFY(!result.isEmpty());
    const QString projectId = result.split(u'|').first();
    const QString commandId = result.split(u'|').last();

    QQmlEngine engine;
    engine.rootContext()->setContextProperty("projectService", &service);
    engine.rootContext()->setContextProperty("logModel", g_logModel);
    static LogModel commandLog;
    engine.rootContext()->setContextProperty("commandLog", &commandLog);
    engine.rootContext()->setContextProperty("commandExecutor", &executor);
    QString buildDir = QDir::currentPath();
    engine.rootContext()->setContextProperty(
        "iconBaseUrl", QUrl::fromLocalFile(buildDir + "/icons/").toString());
    QString qmlPath = QDir::cleanPath(buildDir + "/../qml/Dashboard.qml");
    if (!QFile::exists(qmlPath))
        qmlPath = buildDir + "/qml/Dashboard.qml";

    QQmlComponent component(&engine, QUrl::fromLocalFile(qmlPath));
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));

    QQuickItem host;
    host.setWidth(960);
    host.setHeight(800);
    QObject *obj = component.createWithInitialProperties({}, engine.rootContext());
    QVERIFY(obj != nullptr);
    auto *dashboard = qobject_cast<QQuickItem *>(obj);
    QVERIFY(dashboard != nullptr);
    dashboard->setParentItem(&host);

    QSignalSpy startedSpy(&executor, &CommandExecutor::started);
    QVERIFY(service.runCommandEffective(projectId, commandId));
    QVERIFY2(startedSpy.wait(5000), "seed start never arrived");
    QTRY_COMPARE(service.recentCommands().size(), 1);
    executor.killAll();
    QTRY_COMPARE(service.runningCommands().size(), 0);
    startedSpy.clear();

    QVERIFY(obj->property("hasRecents").toBool());
    QVERIFY(QMetaObject::invokeMethod(obj, "runAllRecent"));
    QVERIFY2(startedSpy.wait(5000), "runAllRecent did not start commands");
    QVERIFY(service.runningCommands().size() > 0);
    executor.killAll();
    delete obj;
}

// Canned docker daemon for DockerPage geometry tests (no daemon needed).
class FakeDockerService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap engine READ engine CONSTANT)
    Q_PROPERTY(QVariantList containers READ containers CONSTANT)
    Q_PROPERTY(QVariantList images READ images CONSTANT)
    Q_PROPERTY(QVariantMap disk READ disk CONSTANT)
    Q_PROPERTY(QVariantMap stats READ stats CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit FakeDockerService(QObject *parent = nullptr) : QObject(parent) {}

    QVariantMap engine() const
    {
        return QVariantMap{{QStringLiteral("available"), true},
                           {QStringLiteral("version"), QStringLiteral("99.0")},
                           {QStringLiteral("context"), QStringLiteral("colima")},
                           {QStringLiteral("runtime"), QStringLiteral("colima")},
                           {QStringLiteral("colimaInstalled"), true},
                           {QStringLiteral("colimaRunning"), true}};
    }
    QVariantList containers() const
    {
        return QVariantList{
            QVariantMap{{QStringLiteral("id"), QStringLiteral("abc123")},
                        {QStringLiteral("name"), QStringLiteral("web")},
                        {QStringLiteral("image"), QStringLiteral("nginx:latest")},
                        {QStringLiteral("status"), QStringLiteral("Up 2 hours")},
                        {QStringLiteral("running"), true},
                        {QStringLiteral("ports"), QStringLiteral("0.0.0.0:8080->80/tcp")},
                        {QStringLiteral("project"), QStringLiteral("")}}};
    }
    QVariantList images() const
    {
        return QVariantList{
            QVariantMap{{QStringLiteral("id"), QStringLiteral("def456")},
                        {QStringLiteral("repository"), QStringLiteral("nginx")},
                        {QStringLiteral("tag"), QStringLiteral("latest")},
                        {QStringLiteral("size"), QStringLiteral("74.5MB")},
                        {QStringLiteral("created"), QStringLiteral("yesterday")}}};
    }
    QVariantMap disk() const { return {}; }
    QVariantMap stats() const { return {}; }
    bool busy() const { return false; }

    Q_INVOKABLE void refresh() {}
    Q_INVOKABLE void fetchLogs(const QString &, int) {}
    Q_INVOKABLE void fetchStats() {}
    Q_INVOKABLE bool prune() { return true; }
    Q_INVOKABLE bool startContainer(const QString &) { return true; }
    Q_INVOKABLE bool stopContainer(const QString &) { return true; }
    Q_INVOKABLE bool removeImage(const QString &id)
    {
        m_removed << id;
        return true;
    }
    Q_INVOKABLE bool removeContainer(const QString &name)
    {
        m_removed << name;
        return true;
    }
    QStringList removed() const { return m_removed; }

signals:
    void busyChanged();
    void logsReady(const QString &name, const QString &logs);
    void logsError(const QString &message);
    void errorMessage(const QString &message);

private:
    QStringList m_removed;
};

void QmlTests::test_docker_page_cards_span_width()
{
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("logModel", g_logModel);
    FakeDockerService docker;
    engine.rootContext()->setContextProperty("dockerService", &docker);
    const QString buildDir = QDir::currentPath();
    engine.rootContext()->setContextProperty(
        "iconBaseUrl", QUrl::fromLocalFile(buildDir + "/icons/").toString());

    QQmlComponent component(&engine,
        QUrl::fromLocalFile(sourceQmlPath("DockerPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));

    QQuickItem host;
    host.setWidth(900);
    host.setHeight(700);
    QObject *obj = component.createWithInitialProperties({}, engine.rootContext());
    QVERIFY(obj != nullptr);
    auto *page = qobject_cast<QQuickItem *>(obj);
    QVERIFY(page != nullptr);
    page->setParentItem(&host);
    // Production sizes the page via anchors.fill from Dashboard;
    // the harness sets the geometry explicitly instead.
    page->setWidth(900);
    page->setHeight(700);
    QTest::qWait(200);

    // Regression guard: cards inside a ScrollView must span the viewport.
    // `width: parent.width` on the content Column loops back to the
    // contentItem's implicit minimum and collapses every card (~120px).
    QQuickItem *engineCard = page->findChild<QQuickItem *>(QStringLiteral("engineCard"));
    QVERIFY(engineCard != nullptr);
    qDebug() << "engineCard width:" << engineCard->width();
    QVERIFY2(engineCard->width() > 700.0,
             qPrintable(QString("engine card collapsed to %1px").arg(engineCard->width())));

    // Remove asks for confirmation: target props land on the dialog
    QVERIFY(QMetaObject::invokeMethod(obj, "askRemove",
        Q_ARG(QVariant, QVariant(QStringLiteral("image"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("def456"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("nginx:latest")))));
    QObject *confirmDialog = obj->findChild<QObject *>(QStringLiteral("confirmDialog"));
    QVERIFY(confirmDialog != nullptr);
    QCOMPARE(confirmDialog->property("targetKind").toString(), QString("image"));
    QCOMPARE(confirmDialog->property("targetId").toString(), QString("def456"));
    QCOMPARE(confirmDialog->property("targetTitle").toString(), QString("nginx:latest"));

    // Confirming hides the Remove button (pendingRemove set, service called)
    QVERIFY(QMetaObject::invokeMethod(obj, "confirmRemove"));
    QCOMPARE(obj->property("pendingRemove").toString(), QString("def456"));
    QVERIFY(docker.removed().contains(QString("def456")));

    delete obj;
}

void QmlTests::test_busy_label_loads()
{
    QQmlEngine engine;
    const QString buildDir = QDir::currentPath();
    engine.rootContext()->setContextProperty(
        "iconBaseUrl", QUrl::fromLocalFile(buildDir + "/icons/").toString());

    for (const char *layout : {"horizontal", "vertical"}) {
        QQmlComponent component(&engine,
            QUrl::fromLocalFile(sourceQmlPath("BusyLabel.qml")));
        QVERIFY2(component.isReady(), layout);
        QObject *obj = component.create();
        QVERIFY(obj != nullptr);
        QScopedPointer<QObject> guard(obj);
        QVERIFY(obj->setProperty("layout", QString::fromUtf8(layout)));
        auto *item = qobject_cast<QQuickItem *>(obj);
        QVERIFY(item != nullptr);
        QTest::qWait(50);
        QVERIFY2(item->implicitWidth() > 0.0, layout);
        QVERIFY2(item->implicitHeight() > 0.0, layout);
    }
}

// T6: the cURL importer is plain C++ with no QML around it, so the parse
// contract (method, url, -H, -d, -u, {{var}} kept verbatim) is pinned here.
void QmlTests::test_api_import_curl()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    Settings settings(tmp.path() + "/trun.ini");
    ApiService svc(&settings);

    // Multi-line command with quotes, backslash continuations and unknown
    // flags that must be skipped rather than eaten as the URL.
    const QString cmd = QStringLiteral(
        "curl --location -X PATCH 'https://api.example.com/v1/campaigns/{{id}}' \\\n"
        "  -H 'Content-Type: application/json' \\\n"
        "  -H \"Authorization: Bearer {{token}}\" \\\n"
        "  -u alice:s3cret \\\n"
        "  --data '{\"name\":\"x\"}' \\\n"
        "  --compressed -sSL -o /tmp/out.json");
    QVERIFY(!svc.importCurl(cmd).isEmpty());
    QCOMPARE(svc.requests().size(), 1);

    const QVariantMap r = svc.requests().at(0).toMap();
    QCOMPARE(r.value(QStringLiteral("method")).toString(), QStringLiteral("PATCH"));
    // {{var}} survives: no substitution happens at import time.
    QCOMPARE(r.value(QStringLiteral("url")).toString(),
             QStringLiteral("https://api.example.com/v1/campaigns/{{id}}"));
    QCOMPARE(r.value(QStringLiteral("body")).toString(), QStringLiteral("{\"name\":\"x\"}"));
    // Name follows the Save dialog suggestion: method + path.
    QCOMPARE(r.value(QStringLiteral("name")).toString(),
             QStringLiteral("PATCH /v1/campaigns/{{id}}"));

    const QVariantList headers = r.value(QStringLiteral("headers")).toList();
    QCOMPARE(headers.size(), 2);
    QCOMPARE(headers.at(0).toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Content-Type"));
    QCOMPARE(headers.at(0).toMap().value(QStringLiteral("value")).toString(),
             QStringLiteral("application/json"));
    QCOMPARE(headers.at(1).toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Authorization"));
    QVERIFY(headers.at(0).toMap().value(QStringLiteral("enabled")).toBool());

    const QVariantMap auth = r.value(QStringLiteral("auth")).toMap();
    QCOMPARE(auth.value(QStringLiteral("type")).toString(), QStringLiteral("basic"));
    QCOMPARE(auth.value(QStringLiteral("user")).toString(), QStringLiteral("alice"));
    QCOMPARE(auth.value(QStringLiteral("password")).toString(), QStringLiteral("s3cret"));

    // No -X: -d promotes GET to POST; a bare command stays GET.
    QVERIFY(!svc.importCurl(QStringLiteral("curl -d a=1 https://example.test/x")).isEmpty());
    QCOMPARE(svc.requests().at(1).toMap().value(QStringLiteral("method")).toString(),
             QStringLiteral("POST"));
    QVERIFY(!svc.importCurl(QStringLiteral("curl https://example.test/ping")).isEmpty());
    QCOMPARE(svc.requests().at(2).toMap().value(QStringLiteral("method")).toString(),
             QStringLiteral("GET"));

    // Failures report through errorMessage() and add nothing.
    QSignalSpy errors(&svc, &ApiService::errorMessage);
    QVERIFY(svc.importCurl(QString()).isEmpty());
    QVERIFY(svc.importCurl(QStringLiteral("curl")).isEmpty());
    QCOMPARE(errors.count(), 2);
    QCOMPARE(svc.requests().size(), 3);

    // Round-trip: a fresh service over the same Settings sees every request
    // (R3: imports persist; "+ New request"/"Open →" open drafts in the
    // request page instead of saving, so there is nothing more to reload).
    ApiService reloaded(&settings);
    reloaded.ensureLoaded();
    QCOMPARE(reloaded.requests().size(), 3);
    QCOMPARE(reloaded.requests().at(0).toMap().value(QStringLiteral("url")).toString(),
             r.value(QStringLiteral("url")).toString());
}

// T4: the history slice is read-only for the list; Clear must drop every
// entry and leave Settings empty for the next start.
void QmlTests::test_api_history_clear()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    Settings settings(tmp.path() + "/trun.ini");

    settings.set(QStringLiteral("api/history"),
                 QStringLiteral("[{\"method\":\"GET\",\"url\":\"http://localhost:8000/a\","
                                "\"status\":200,\"at\":\"2026-10-06T12:00:00Z\"},"
                                "{\"method\":\"POST\",\"url\":\"http://localhost:8000/b\","
                                "\"status\":401,\"at\":\"2026-10-06T12:05:00Z\"}]"));

    ApiService svc(&settings);
    svc.ensureLoaded();
    QCOMPARE(svc.history().size(), 2);
    QCOMPARE(svc.history().at(0).toMap().value(QStringLiteral("method")).toString(),
             QStringLiteral("GET"));
    QCOMPARE(svc.history().at(1).toMap().value(QStringLiteral("status")).toInt(), 401);

    QSignalSpy changed(&svc, &ApiService::historyChanged);
    QVERIFY(svc.clearHistory());
    QCOMPARE(svc.history().size(), 0);
    QCOMPARE(changed.count(), 1);

    // Round-trip: a fresh service over the same Settings sees it wiped.
    ApiService reloaded(&settings);
    reloaded.ensureLoaded();
    QCOMPARE(reloaded.history().size(), 0);

    // Clearing an already-empty history succeeds without re-persisting.
    QVERIFY(reloaded.clearHistory());
    QCOMPARE(changed.count(), 1);
}

// R2: the request-page storage slice. Drafts are unsaved (id ""), the first
// saveRequest() assigns the id and persists, an update keeps its position.
void QmlTests::test_api_draft_save()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    Settings settings(tmp.path() + "/trun.ini");
    ApiService svc(&settings);

    // Draft factory: unsaved, design defaults, nothing persisted.
    const QVariantMap draft = svc.makeDraft();
    QVERIFY(draft.value(QStringLiteral("id")).toString().isEmpty());
    QCOMPARE(draft.value(QStringLiteral("name")).toString(),
             QStringLiteral("Untitled request"));
    QCOMPARE(draft.value(QStringLiteral("method")).toString(), QStringLiteral("GET"));
    QCOMPARE(draft.value(QStringLiteral("url")).toString(), QStringLiteral("{{baseUrl}}/"));
    QCOMPARE(draft.value(QStringLiteral("headers")).toList().size(), 0);
    QCOMPARE(draft.value(QStringLiteral("params")).toList().size(), 0);
    QCOMPARE(draft.value(QStringLiteral("body")).toString(), QString());
    QCOMPARE(draft.value(QStringLiteral("auth")).toMap().value(QStringLiteral("type")).toString(),
             QStringLiteral("none"));
    QCOMPARE(draft.value(QStringLiteral("lastStatus")).toInt(), 0);
    QVERIFY(!draft.value(QStringLiteral("updatedAt")).toString().isEmpty());

    // A running-server "Open →" pastes its URL into the same draft.
    QCOMPARE(svc.makeDraft(QStringLiteral("http://localhost:5173/"))
                 .value(QStringLiteral("url")).toString(),
             QStringLiteral("http://localhost:5173/"));
    QCOMPARE(svc.requests().size(), 0);

    // Saving the draft assigns the id and persists the request.
    svc.ensureLoaded(); // first, so the spy below only sees the save's emit
    QSignalSpy created(&svc, &ApiService::requestsChanged);
    QVariantMap toSave = draft;
    toSave.insert(QStringLiteral("projectId"), QStringLiteral("p/one"));
    const QString returned = svc.saveRequest(toSave);
    QVERIFY(!returned.isEmpty());
    QCOMPARE(svc.requests().size(), 1);
    const QString id = svc.requests().at(0).toMap().value(QStringLiteral("id")).toString();
    QVERIFY(!id.isEmpty());
    QCOMPARE(returned, id);
    QCOMPARE(svc.requests().at(0).toMap().value(QStringLiteral("projectId")).toString(),
             QStringLiteral("p/one"));
    QCOMPARE(created.count(), 1);

    // Round-trip: a fresh service over the same Settings sees the request.
    ApiService reloaded(&settings);
    reloaded.ensureLoaded();
    QCOMPARE(reloaded.requests().size(), 1);
    QCOMPARE(reloaded.requests().at(0).toMap().value(QStringLiteral("id")).toString(), id);
    QCOMPARE(reloaded.requests().at(0).toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Untitled request"));
    QCOMPARE(reloaded.requests().at(0).toMap().value(QStringLiteral("params")).toList().size(),
             0);
}

// R2: Save on an existing request = update (rename + edited fields) — the same
// row, in place, never a duplicate; unknown ids are reported and ignored.
void QmlTests::test_api_save_request_update()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    Settings settings(tmp.path() + "/trun.ini");
    ApiService svc(&settings);

    QVariantMap r = svc.makeDraft();
    r.insert(QStringLiteral("params"),
             QVariantList{QVariantMap{{QStringLiteral("name"), QStringLiteral("include")},
                                      {QStringLiteral("value"), QStringLiteral("stats")},
                                      {QStringLiteral("enabled"), true}}});
    QVERIFY(!svc.saveRequest(r).isEmpty());
    const QString id = svc.requests().at(0).toMap().value(QStringLiteral("id")).toString();
    QVERIFY(!id.isEmpty());

    // Inline rename + edited params: same id, still one row.
    QVariantMap edited = svc.requests().at(0).toMap();
    edited.insert(QStringLiteral("name"), QStringLiteral("List campaigns"));
    QVERIFY(!svc.saveRequest(edited).isEmpty());
    QCOMPARE(svc.requests().size(), 1);
    QCOMPARE(svc.requests().at(0).toMap().value(QStringLiteral("id")).toString(), id);
    QCOMPARE(svc.requests().at(0).toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("List campaigns"));
    QCOMPARE(svc.requests().at(0).toMap().value(QStringLiteral("params")).toList().size(), 1);

    // An unknown non-empty id is reported and changes nothing.
    QSignalSpy errors(&svc, &ApiService::errorMessage);
    QVariantMap ghost = svc.makeDraft();
    ghost.insert(QStringLiteral("id"), QStringLiteral("nope"));
    QVERIFY(svc.saveRequest(ghost).isEmpty());
    QCOMPARE(errors.count(), 1);
    QCOMPARE(svc.requests().size(), 1);

    // An incomplete map (no url) is rejected too.
    QVariantMap broken{{QStringLiteral("method"), QStringLiteral("GET")}};
    QVERIFY(svc.saveRequest(broken).isEmpty());
    QCOMPARE(errors.count(), 2);
}

// R2: Send stamps lastStatus/lastAt/updatedAt on the stored request only;
// drafts and unknown ids are silent no-ops.
void QmlTests::test_api_update_request_status()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    Settings settings(tmp.path() + "/trun.ini");
    ApiService svc(&settings);

    QVERIFY(!svc.saveRequest(svc.makeDraft()).isEmpty());
    const QString id = svc.requests().at(0).toMap().value(QStringLiteral("id")).toString();
    QCOMPARE(svc.requests().at(0).toMap().value(QStringLiteral("lastStatus")).toInt(), 0);
    QVERIFY(svc.requests().at(0).toMap().value(QStringLiteral("lastAt")).toString().isEmpty());

    QVERIFY(svc.updateRequestStatus(id, 404));
    QCOMPARE(svc.requests().at(0).toMap().value(QStringLiteral("lastStatus")).toInt(), 404);
    QVERIFY(!svc.requests().at(0).toMap().value(QStringLiteral("lastAt")).toString().isEmpty());
    QVERIFY(!svc.requests().at(0).toMap().value(QStringLiteral("updatedAt")).toString().isEmpty());

    // Drafts ("") and unknown ids: no error, nothing stored.
    QSignalSpy errors(&svc, &ApiService::errorMessage);
    QVERIFY(!svc.updateRequestStatus(QString(), 200));
    QVERIFY(!svc.updateRequestStatus(QStringLiteral("missing"), 200));
    QCOMPARE(errors.count(), 0);

    // Round-trip keeps the stamp.
    ApiService reloaded(&settings);
    reloaded.ensureLoaded();
    QCOMPARE(reloaded.requests().at(0).toMap().value(QStringLiteral("lastStatus")).toInt(),
             404);
}

// R2: the history writer — prepend (newest on top), `at` stamped when absent,
// cap 50 with the oldest dropped at the end.
void QmlTests::test_api_append_history()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    Settings settings(tmp.path() + "/trun.ini");
    ApiService svc(&settings);

    QVariantMap first{{QStringLiteral("method"), QStringLiteral("GET")},
                      {QStringLiteral("url"), QStringLiteral("http://localhost:8000/a")},
                      {QStringLiteral("status"), 200}};
    svc.appendHistory(first);
    QCOMPARE(svc.history().size(), 1);
    // `at` is stamped when absent, so the list can render "just now".
    QVERIFY(!svc.history().at(0).toMap().value(QStringLiteral("at")).toString().isEmpty());

    QVariantMap second{{QStringLiteral("method"), QStringLiteral("POST")},
                       {QStringLiteral("url"), QStringLiteral("http://localhost:8000/b")},
                       {QStringLiteral("status"), 401}};
    svc.appendHistory(second);
    // Newest first, as the design's history list.
    QCOMPARE(svc.history().size(), 2);
    QCOMPARE(svc.history().at(0).toMap().value(QStringLiteral("url")).toString(),
             QStringLiteral("http://localhost:8000/b"));

    // Cap 50: the oldest (the very first send) falls off the end.
    for (int i = 2; i < 55; ++i) {
        QVariantMap e{{QStringLiteral("method"), QStringLiteral("GET")},
                      {QStringLiteral("url"),
                       QStringLiteral("http://localhost:8000/u%1").arg(i)},
                      {QStringLiteral("status"), 200}};
        svc.appendHistory(e);
    }
    QCOMPARE(svc.history().size(), 50);
    QCOMPARE(svc.history().at(0).toMap().value(QStringLiteral("url")).toString(),
             QStringLiteral("http://localhost:8000/u54"));
    bool firstSurvived = false;
    for (const QVariant &h : svc.history()) {
        if (h.toMap().value(QStringLiteral("url")).toString()
            == QStringLiteral("http://localhost:8000/a"))
            firstSurvived = true;
    }
    QVERIFY(!firstSurvived);

    // Round-trip keeps the capped, newest-first list.
    ApiService reloaded(&settings);
    reloaded.ensureLoaded();
    QCOMPARE(reloaded.history().size(), 50);
    QCOMPARE(reloaded.history().at(0).toMap().value(QStringLiteral("url")).toString(),
             QStringLiteral("http://localhost:8000/u54"));
}

// R2: the Save-dialog name suggestion "METHOD /path" (design's save()).
void QmlTests::test_api_suggest_name()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    Settings settings(tmp.path() + "/trun.ini");
    ApiService svc(&settings);

    QCOMPARE(svc.suggestName(QStringLiteral("GET"),
                             QStringLiteral("http://localhost:8000/api/campaigns")),
             QStringLiteral("GET /api/campaigns"));
    // A {{var}} origin is skipped like a real origin.
    QCOMPARE(svc.suggestName(QStringLiteral("POST"), QStringLiteral("{{baseUrl}}/api/events")),
             QStringLiteral("POST /api/events"));
    // Root path: just the method and a slash.
    QCOMPARE(svc.suggestName(QStringLiteral("GET"), QStringLiteral("http://localhost:8000/")),
             QStringLiteral("GET /"));
}

// Fakes for the ApiPage list test: only the surfaces the page reads.
class FakeApiService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList requests READ requests NOTIFY requestsChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QVariantList environments READ environments NOTIFY environmentsChanged)
    Q_PROPERTY(QString activeEnvId READ activeEnvId NOTIFY activeEnvIdChanged)

public:
    QVariantList requests() const { return m_requests; }
    QVariantList history() const { return m_history; }
    QVariantList environments() const { return m_envs; }
    QString activeEnvId() const { return m_active; }
    void setRequests(const QVariantList &r)
    {
        m_requests = r;
        emit requestsChanged();
    }
    void setEnvironments(const QVariantList &e)
    {
        m_envs = e;
        emit environmentsChanged();
    }
    void setActiveEnvId(const QString &id)
    {
        m_active = id;
        emit activeEnvIdChanged();
    }
    Q_INVOKABLE void ensureLoaded() {}
    Q_INVOKABLE bool setActiveEnv(const QString &id)
    {
        m_active = id;
        emit activeEnvIdChanged();
        return true;
    }
    Q_INVOKABLE QVariantMap makeDraft(const QString &url = QString())
    {
        return QVariantMap{
            {QStringLiteral("id"), QString()},
            {QStringLiteral("name"), QStringLiteral("Untitled request")},
            {QStringLiteral("method"), QStringLiteral("GET")},
            {QStringLiteral("url"), url.isEmpty() ? QStringLiteral("{{baseUrl}}/") : url},
            {QStringLiteral("projectId"), QString()},
            {QStringLiteral("headers"), QVariantList{}},
            {QStringLiteral("params"), QVariantList{}},
            {QStringLiteral("body"), QString()},
            {QStringLiteral("auth"),
             QVariantMap{{QStringLiteral("type"), QStringLiteral("none")}}},
            {QStringLiteral("lastStatus"), 0},
            {QStringLiteral("lastAt"), QString()}};
    }
    Q_INVOKABLE QString saveRequest(const QVariantMap &request)
    {
        QVariantMap stored = request;
        QString id = stored.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            id = QStringLiteral("fake-request-id");
            stored.insert(QStringLiteral("id"), id);
            m_requests.append(stored);
        } else {
            for (int i = 0; i < m_requests.size(); ++i) {
                if (m_requests.at(i).toMap().value(QStringLiteral("id")).toString() == id)
                    m_requests.replace(i, stored);
            }
        }
        emit requestsChanged();
        return id;
    }
    Q_INVOKABLE QString suggestName(const QString &method, const QString &url) const
    {
        QString path = url;
        const int scheme = path.indexOf(QLatin1String("//"));
        const int slash = path.indexOf(QLatin1Char('/'), scheme + 2);
        if (slash >= 0)
            path = path.mid(slash);
        return method + QLatin1Char(' ') + path;
    }
    Q_INVOKABLE QString importCurl(const QString &command)
    {
        Q_UNUSED(command);
        return QString();
    }
    Q_INVOKABLE bool clearHistory() { return true; }

signals:
    void requestsChanged();
    void historyChanged();
    void environmentsChanged();
    void activeEnvIdChanged();
    void errorMessage(const QString &message);

private:
    QVariantList m_requests;
    QVariantList m_history;
    QVariantList m_envs;
    QString m_active;
};

class FakeProjectServiceForApi : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList runningCommands READ runningCommands NOTIFY runningCommandsChanged)

public:
    QVariantList runningCommands() const { return {}; }
    Q_INVOKABLE QVariantList projects() const { return m_projects; }
    void setProjects(const QVariantList &p) { m_projects = p; }

signals:
    void runningCommandsChanged();

private:
    QVariantList m_projects;
};

class FakeDashboardForApi : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap detectedPorts READ detectedPorts NOTIFY detectedPortsChanged)

public:
    QVariantMap detectedPorts() const { return {}; }
    Q_INVOKABLE QString timeAgo(const QString &iso) const
    {
        Q_UNUSED(iso);
        return QStringLiteral("2m ago");
    }

signals:
    void detectedPortsChanged();
};

// T4: the list is pure QML over the service slices. Pins the grouping
// (project name uppercased, empty id -> UNSAVED) and the name+url search.
void QmlTests::test_api_page_list_groups()
{
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("logModel", g_logModel);
    const QString buildDir = QDir::currentPath();
    engine.rootContext()->setContextProperty(
        "iconBaseUrl", QUrl::fromLocalFile(buildDir + "/icons/").toString());

    FakeApiService api;
    const QVariantList reqs{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("1")},
                    {QStringLiteral("name"), QStringLiteral("Get campaign")},
                    {QStringLiteral("method"), QStringLiteral("GET")},
                    {QStringLiteral("url"), QStringLiteral("{{baseUrl}}/api/campaigns")},
                    {QStringLiteral("projectId"), QStringLiteral("p/one")},
                    {QStringLiteral("lastStatus"), 200},
                    {QStringLiteral("lastAt"), QStringLiteral("2026-10-06T12:00:00Z")},
                    {QStringLiteral("headers"), QVariantList{}}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("2")},
                    {QStringLiteral("name"), QStringLiteral("Create visitor")},
                    {QStringLiteral("method"), QStringLiteral("POST")},
                    {QStringLiteral("url"), QStringLiteral("{{baseUrl}}/api/events")},
                    {QStringLiteral("projectId"), QStringLiteral("p/one")},
                    {QStringLiteral("lastStatus"), 0},
                    {QStringLiteral("lastAt"), QString()},
                    {QStringLiteral("headers"), QVariantList{}}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("3")},
                    {QStringLiteral("name"), QStringLiteral("Health")},
                    {QStringLiteral("method"), QStringLiteral("GET")},
                    {QStringLiteral("url"), QStringLiteral("http://localhost:4317/health")},
                    {QStringLiteral("projectId"), QString()},
                    {QStringLiteral("lastStatus"), 200},
                    {QStringLiteral("lastAt"), QStringLiteral("2026-10-06T11:00:00Z")},
                    {QStringLiteral("headers"), QVariantList{}}},
    };
    api.setRequests(reqs);
    engine.rootContext()->setContextProperty("apiService", &api);

    FakeProjectServiceForApi proj;
    proj.setProjects(QVariantList{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("p/one")},
                    {QStringLiteral("name"), QStringLiteral("personizely-backend")}}});
    engine.rootContext()->setContextProperty("projectService", &proj);

    FakeDashboardForApi dash;
    engine.rootContext()->setContextProperty("dashboard", &dash);

    QQmlComponent component(&engine,
        QUrl::fromLocalFile(sourceQmlPath("ApiPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));

    QQuickItem host;
    host.setWidth(900);
    host.setHeight(700);
    QObject *obj = component.createWithInitialProperties({}, engine.rootContext());
    QVERIFY(obj != nullptr);
    auto *page = qobject_cast<QQuickItem *>(obj);
    QVERIFY(page != nullptr);
    page->setParentItem(&host);
    page->setWidth(900);
    page->setHeight(700);
    QTest::qWait(100);

    // Two groups: the known project (uppercased name) and the unsaved bucket.
    const QVariantList groups = page->property("requestGroups").toList();
    QCOMPARE(groups.size(), 2);
    QCOMPARE(groups.at(0).toMap().value(QStringLiteral("title")).toString(),
             QStringLiteral("PERSONIZELY-BACKEND"));
    QCOMPARE(groups.at(0).toMap().value(QStringLiteral("count")).toInt(), 2);
    QCOMPARE(groups.at(1).toMap().value(QStringLiteral("title")).toString(),
             QStringLiteral("UNSAVED"));
    QCOMPARE(groups.at(1).toMap().value(QStringLiteral("count")).toInt(), 1);

    // The empty plate gives way to the list once something is saved.
    QQuickItem *plate = page->findChild<QQuickItem *>(QStringLiteral("apiEmptyPlate"));
    QVERIFY(plate != nullptr);
    QVERIFY(!plate->property("visible").toBool());
    QQuickItem *scroll = page->findChild<QQuickItem *>(QStringLiteral("apiListScroll"));
    QVERIFY(scroll != nullptr);
    QVERIFY(scroll->property("visible").toBool());

    // Search covers name + url and drops emptied groups.
    page->setProperty("searchQuery", "events");
    QTest::qWait(20);
    const QVariantList filtered = page->property("requestGroups").toList();
    QCOMPARE(filtered.size(), 1);
    QCOMPARE(filtered.at(0).toMap().value(QStringLiteral("count")).toInt(), 1);

    page->setProperty("searchQuery", "localhost:4317");
    QTest::qWait(20);
    const QVariantList byUrl = page->property("requestGroups").toList();
    QCOMPARE(byUrl.size(), 1);
    QCOMPARE(byUrl.at(0).toMap().value(QStringLiteral("title")).toString(),
             QStringLiteral("UNSAVED"));

    page->setProperty("searchQuery", "");
    QTest::qWait(20);
    QCOMPARE(page->property("requestGroups").toList().size(), 2);

    delete obj;
}

// R3: the request page is the list's second level. Drafts (new / Open → /
// history), saved requests as detached clones, rename reaching the header
// crumb, silent discard on close; the first Save binds the working map to
// the persisted id.
void QmlTests::test_api_page_open_request()
{
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("logModel", g_logModel);
    const QString buildDir = QDir::currentPath();
    engine.rootContext()->setContextProperty(
        "iconBaseUrl", QUrl::fromLocalFile(buildDir + "/icons/").toString());

    FakeApiService api;
    const QVariantMap campaign{
        {QStringLiteral("id"), QStringLiteral("1")},
        {QStringLiteral("name"), QStringLiteral("Get campaign")},
        {QStringLiteral("method"), QStringLiteral("GET")},
        {QStringLiteral("url"), QStringLiteral("{{baseUrl}}/api/campaigns")},
        {QStringLiteral("projectId"), QStringLiteral("p/one")},
        {QStringLiteral("lastStatus"), 200},
        {QStringLiteral("lastAt"), QStringLiteral("2026-10-06T12:00:00Z")},
        {QStringLiteral("headers"), QVariantList{}}};
    api.setRequests(QVariantList{campaign});
    engine.rootContext()->setContextProperty("apiService", &api);

    FakeProjectServiceForApi proj;
    proj.setProjects(QVariantList{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("p/one")},
                    {QStringLiteral("name"), QStringLiteral("personizely-backend")}}});
    engine.rootContext()->setContextProperty("projectService", &proj);

    FakeDashboardForApi dash;
    engine.rootContext()->setContextProperty("dashboard", &dash);

    QQmlComponent component(&engine,
        QUrl::fromLocalFile(sourceQmlPath("ApiPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));

    QQuickItem host;
    host.setWidth(900);
    host.setHeight(700);
    QObject *obj = component.createWithInitialProperties({}, engine.rootContext());
    QVERIFY(obj != nullptr);
    auto *page = qobject_cast<QQuickItem *>(obj);
    QVERIFY(page != nullptr);
    page->setParentItem(&host);
    page->setWidth(900);
    page->setHeight(700);
    QTest::qWait(100);

    QQuickItem *pane = page->findChild<QQuickItem *>(QStringLiteral("apiRequestPage"));
    QVERIFY(pane != nullptr);

    // "+ New request" / "Open →" open a fresh draft; the list hides.
    QVERIFY(QMetaObject::invokeMethod(page, "openDraft",
        Q_ARG(QVariant, QStringLiteral("http://localhost:5173/"))));
    QVERIFY(page->property("requestOpen").toBool());
    QCOMPARE(page->property("requestName").toString(),
             QStringLiteral("Untitled request"));
    QCOMPARE(pane->property("saved").toBool(), false);
    QCOMPARE(pane->property("requestMap").toMap().value(QStringLiteral("url")).toString(),
             QStringLiteral("http://localhost:5173/"));

    // First Save persists the draft (appended after the seeded campaign) and
    // adopts the returned id.
    QVERIFY(QMetaObject::invokeMethod(pane, "doSave"));
    QCOMPARE(pane->property("saved").toBool(), true);
    QCOMPARE(api.requests().size(), 2);
    QCOMPARE(pane->property("requestMap").toMap().value(QStringLiteral("id")).toString(),
             api.requests().at(1).toMap().value(QStringLiteral("id")).toString());

    // A list row opens the saved request as an editable, detached copy.
    QVERIFY(QMetaObject::invokeMethod(page, "openSavedRequest",
        Q_ARG(QVariant, QVariant(campaign))));
    QVERIFY(page->property("requestOpen").toBool());
    QCOMPARE(page->property("requestName").toString(), QStringLiteral("Get campaign"));
    QCOMPARE(pane->property("saved").toBool(), true);
    QCOMPARE(pane->property("requestMap").toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("1"));

    // A history entry reopens as a draft carrying method + url.
    const QVariantMap entry{
        {QStringLiteral("method"), QStringLiteral("POST")},
        {QStringLiteral("url"), QStringLiteral("http://localhost:8000/events")},
        {QStringLiteral("status"), 401},
        {QStringLiteral("at"), QStringLiteral("2026-10-06T12:05:00Z")}};
    QVERIFY(QMetaObject::invokeMethod(page, "openHistoryEntry", Q_ARG(QVariant, QVariant(entry))));
    const QVariantMap replayed = pane->property("requestMap").toMap();
    QCOMPARE(replayed.value(QStringLiteral("method")).toString(), QStringLiteral("POST"));
    QCOMPARE(replayed.value(QStringLiteral("url")).toString(),
             QStringLiteral("http://localhost:8000/events"));
    QCOMPARE(pane->property("saved").toBool(), false);

    // Close discards silently (draft stays persisted only after Save).
    QVERIFY(QMetaObject::invokeMethod(page, "closeRequest"));
    QVERIFY(!page->property("requestOpen").toBool());
    QCOMPARE(page->property("requestName").toString(), QString());

    delete obj;
}

// R3, window-level: the real input paths, not just the JS functions. Enter
// on the name field must open the Save dialog for a draft (and save on
// confirm), Esc must close the dialog first and then pop back to the list.
// Runs in a real window so the key events exercise the focus chain.
void QmlTests::test_api_page_keys()
{
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("logModel", g_logModel);
    const QString buildDir = QDir::currentPath();
    engine.rootContext()->setContextProperty(
        "iconBaseUrl", QUrl::fromLocalFile(buildDir + "/icons/").toString());

    FakeApiService api;
    engine.rootContext()->setContextProperty("apiService", &api);

    FakeProjectServiceForApi proj;
    proj.setProjects(QVariantList{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("p/one")},
                    {QStringLiteral("name"), QStringLiteral("personizely-backend")}}});
    engine.rootContext()->setContextProperty("projectService", &proj);

    FakeDashboardForApi dash;
    engine.rootContext()->setContextProperty("dashboard", &dash);

    QQmlComponent component(&engine,
        QUrl::fromLocalFile(sourceQmlPath("ApiPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));

    QQuickWindow window;
    window.resize(1000, 800);
    QObject *obj = component.createWithInitialProperties({}, engine.rootContext());
    QVERIFY(obj != nullptr);
    auto *page = qobject_cast<QQuickItem *>(obj);
    QVERIFY(page != nullptr);
    page->setParentItem(window.contentItem());
    page->setPosition(QPointF(0, 0));
    page->setSize(QSizeF(1000, 800));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(50);

    QQuickItem *pane = page->findChild<QQuickItem *>(QStringLiteral("apiRequestPage"));
    QVERIFY(pane != nullptr);
    QQuickItem *nameField = pane->findChild<QQuickItem *>(QStringLiteral("apiRequestNameField"));
    QVERIFY(nameField != nullptr);
    // AppDialog is a Popup (not an Item) — it lives outside the item tree,
    // so it must be located as a plain QObject child of the pane.
    QObject *saveDialog = pane->findChild<QObject *>(QStringLiteral("apiSaveDialog"));
    QVERIFY(saveDialog != nullptr);

    // Open a draft ("+ New request" / "Open →" share this path).
    QVERIFY(QMetaObject::invokeMethod(page, "openDraft",
        Q_ARG(QVariant, QStringLiteral("http://localhost:5173/"))));
    QVERIFY(page->property("requestOpen").toBool());

    // Enter on the name field must reach save() — the dialog's name field
    // gets the suggested name, which proves openFor ran. (The dialog itself
    // cannot open on a bare QQuickWindow: a Popup needs the ApplicationWindow
    // overlay, so the opened-state is asserted via its effects instead.)
    nameField->forceActiveFocus();
    QTest::qWait(20);
    QVERIFY(nameField->hasActiveFocus());
    QTest::keyClick(&window, Qt::Key_Return);
    QTest::qWait(50);
    QQuickItem *dialogName = saveDialog->findChild<QQuickItem *>(
        QStringLiteral("apiSaveNameField"));
    QVERIFY(dialogName != nullptr);
    QCOMPARE(dialogName->property("text").toString(),
             QStringLiteral("GET /")); // suggested name = openFor ran

    // Confirm through the dialog persists the draft.
    QVERIFY(QMetaObject::invokeMethod(saveDialog, "confirmSave"));
    QTest::qWait(50);
    QVERIFY(page->property("requestOpen").toBool());
    QCOMPARE(pane->property("saved").toBool(), true);
    QCOMPARE(api.requests().size(), 1);

    // --- Esc: fresh draft, no dialog involvement ---------------------------
    // (The harness cannot open the Popup, so closing it above drops focus to
    // the window root — an artifact. Esc is validated on a clean page, with
    // focus on the name field — the state the user is in when typing.)
    QVERIFY(QMetaObject::invokeMethod(page, "openDraft",
        Q_ARG(QVariant, QStringLiteral("http://localhost:5173/"))));
    QVERIFY(page->property("requestOpen").toBool());
    QCOMPARE(pane->property("saved").toBool(), false);

    nameField->forceActiveFocus();
    QTest::qWait(20);
    QVERIFY(nameField->hasActiveFocus());

    // Esc with focus inside the page (the typing-user case).
    QTest::keyClick(&window, Qt::Key_Escape);
    QTest::qWait(50);
    QVERIFY2(!page->property("requestOpen").toBool(),
             "Esc must pop the request page back to the list");

    // --- Esc with no focus inside the page: the page root (focus scope)
    // owns the key after openDraft's forceActiveFocus ---------------------
    QVERIFY(QMetaObject::invokeMethod(page, "openDraft",
        Q_ARG(QVariant, QStringLiteral("http://localhost:5173/"))));
    QQuickItem *pageFocused = window.activeFocusItem();
    QVERIFY(pageFocused != nullptr);
    QTest::keyClick(&window, Qt::Key_Escape);
    QTest::qWait(50);
    QVERIFY2(!page->property("requestOpen").toBool(),
             "Esc must work even when no control inside the page has focus");

    delete obj;
}

// R4: the URL bar. openDraft presets the method select and the mono URL
// field; the "→ resolved" line substitutes active-environment variables in
// live (non-secret values inline, secrets masked with dots — QML never sees
// Keychain values); the method/environment selects drive the working request;
// Send ⇄ Cancel swaps and locks the bar while "in flight" (UI stub, R6 wires
// the network).
void QmlTests::test_api_request_urlbar()
{
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("logModel", g_logModel);
    const QString buildDir = QDir::currentPath();
    engine.rootContext()->setContextProperty(
        "iconBaseUrl", QUrl::fromLocalFile(buildDir + "/icons/").toString());

    FakeApiService api;
    api.setEnvironments(QVariantList{
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("local")},
            {QStringLiteral("name"), QStringLiteral("local")},
            {QStringLiteral("vars"), QVariantList{
                QVariantMap{
                    {QStringLiteral("name"), QStringLiteral("baseUrl")},
                    {QStringLiteral("value"), QStringLiteral("http://localhost:8000")},
                    {QStringLiteral("secret"), false},
                },
                QVariantMap{
                    {QStringLiteral("name"), QStringLiteral("token")},
                    {QStringLiteral("value"), QString()},
                    {QStringLiteral("secret"), true},
                    {QStringLiteral("hasSavedValue"), true},
                },
            }},
        },
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("staging")},
            {QStringLiteral("name"), QStringLiteral("staging")},
            {QStringLiteral("vars"), QVariantList{}},
        },
    });
    api.setActiveEnvId(QStringLiteral("local"));
    engine.rootContext()->setContextProperty("apiService", &api);

    FakeProjectServiceForApi proj;
    proj.setProjects(QVariantList{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("p/one")},
                    {QStringLiteral("name"), QStringLiteral("personizely-backend")}}});
    engine.rootContext()->setContextProperty("projectService", &proj);

    FakeDashboardForApi dash;
    engine.rootContext()->setContextProperty("dashboard", &dash);

    QQmlComponent component(&engine,
        QUrl::fromLocalFile(sourceQmlPath("ApiPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));

    QQuickWindow window;
    window.resize(1000, 800);
    QObject *obj = component.createWithInitialProperties({}, engine.rootContext());
    QVERIFY(obj != nullptr);
    auto *page = qobject_cast<QQuickItem *>(obj);
    QVERIFY(page != nullptr);
    page->setParentItem(window.contentItem());
    page->setPosition(QPointF(0, 0));
    page->setSize(QSizeF(1000, 800));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(50);

    // A draft whose URL mixes the local environment's baseUrl with a secret.
    QVERIFY(QMetaObject::invokeMethod(page, "openDraft",
        Q_ARG(QVariant, QStringLiteral("{{baseUrl}}/api/{{token}}/campaigns"))));
    QVERIFY(page->property("requestOpen").toBool());
    QTest::qWait(50);

    QQuickItem *pane = page->findChild<QQuickItem *>(QStringLiteral("apiRequestPage"));
    QVERIFY(pane != nullptr);
    QQuickItem *methodSelect = pane->findChild<QQuickItem *>(QStringLiteral("apiMethodSelect"));
    QVERIFY(methodSelect != nullptr);
    QQuickItem *urlField = pane->findChild<QQuickItem *>(QStringLiteral("apiUrlField"));
    QVERIFY(urlField != nullptr);
    QQuickItem *envSelect = pane->findChild<QQuickItem *>(QStringLiteral("apiEnvSelect"));
    QVERIFY(envSelect != nullptr);
    QQuickItem *sendBtn = pane->findChild<QQuickItem *>(QStringLiteral("apiSendButton"));
    QVERIFY(sendBtn != nullptr);
    QQuickItem *cancelBtn = pane->findChild<QQuickItem *>(QStringLiteral("apiCancelButton"));
    QVERIFY(cancelBtn != nullptr);
    QQuickItem *resolved = pane->findChild<QQuickItem *>(QStringLiteral("apiResolvedLine"));
    QVERIFY(resolved != nullptr);

    // Draft preset: GET, the draft URL; the preview substitutes baseUrl and
    // masks the secret with dots.
    QCOMPARE(methodSelect->property("currentIndex").toInt(), 0);
    QCOMPARE(urlField->property("text").toString(),
             QStringLiteral("{{baseUrl}}/api/{{token}}/campaigns"));
    QCOMPARE(resolved->property("text").toString(),
             QStringLiteral("→ http://localhost:8000/api/••••••••/campaigns"));

    // Method switch: simulate the select's row click — the delegate sets
    // currentIndex, then emits activated -> selectMethod. POST lands in the
    // working request and marks it dirty.
    methodSelect->setProperty("currentIndex", 1);
    QVERIFY(QMetaObject::invokeMethod(methodSelect, "activated", Q_ARG(int, 1)));
    QTest::qWait(20);
    QCOMPARE(pane->property("dirty").toBool(), true);
    QCOMPARE(methodSelect->property("currentIndex").toInt(), 1);
    QVariantMap working = pane->property("requestMap").value<QVariantMap>();
    QCOMPARE(working.value(QStringLiteral("method")).toString(),
             QStringLiteral("POST"));

    // Environment switch to "staging" (no vars): the preview keeps the raw
    // {{...}} placeholders; back to "local" restores the substitution.
    envSelect->setProperty("currentIndex", 1);
    QVERIFY(QMetaObject::invokeMethod(envSelect, "activated", Q_ARG(int, 1)));
    QTest::qWait(20);
    QCOMPARE(api.activeEnvId(), QStringLiteral("staging"));
    QCOMPARE(resolved->property("text").toString(),
             QStringLiteral("→ {{baseUrl}}/api/{{token}}/campaigns"));
    envSelect->setProperty("currentIndex", 0);
    QVERIFY(QMetaObject::invokeMethod(envSelect, "activated", Q_ARG(int, 0)));
    QTest::qWait(20);
    QCOMPARE(resolved->property("text").toString(),
             QStringLiteral("→ http://localhost:8000/api/••••••••/campaigns"));

    // Typing in the URL field updates the request and the preview live.
    urlField->forceActiveFocus();
    QTest::qWait(20);
    QVERIFY(urlField->hasActiveFocus());
    // "/live" — keyClicks is a QWidget-API; QWindow needs per-key clicks.
    const QString typed = QStringLiteral("/live");
    for (const QChar &c : typed) {
        Qt::Key key = (c == u'/') ? Qt::Key_Slash
            : static_cast<Qt::Key>(c.toUpper().unicode());
        QTest::keyClick(&window, key);
    }
    QTest::qWait(50);
    QCOMPARE(urlField->property("text").toString(),
             QStringLiteral("{{baseUrl}}/api/{{token}}/campaigns/live"));
    QCOMPARE(resolved->property("text").toString(),
             QStringLiteral("→ http://localhost:8000/api/••••••••/campaigns/live"));
    working = pane->property("requestMap").value<QVariantMap>();
    QCOMPARE(working.value(QStringLiteral("url")).toString(),
             QStringLiteral("{{baseUrl}}/api/{{token}}/campaigns/live"));

    // Send ⇄ Cancel (UI stub): a Nuxt loading button stays visible with the
    // spinner inside it and is disabled; Cancel appears beside it; fields
    // lock. Cancel restores everything.
    QVERIFY(QMetaObject::invokeMethod(pane, "startSend"));
    QTest::qWait(20);
    QCOMPARE(pane->property("sending").toBool(), true);
    QCOMPARE(sendBtn->property("loading").toBool(), true);
    QCOMPARE(sendBtn->property("visible").toBool(), true);
    QCOMPARE(cancelBtn->property("visible").toBool(), true);
    QCOMPARE(urlField->property("enabled").toBool(), false);
    QCOMPARE(methodSelect->property("enabled").toBool(), false);
    QCOMPARE(envSelect->property("enabled").toBool(), false);
    QVERIFY(QMetaObject::invokeMethod(pane, "cancelSend"));
    QTest::qWait(20);
    QCOMPARE(pane->property("sending").toBool(), false);
    QCOMPARE(sendBtn->property("loading").toBool(), false);
    QCOMPARE(cancelBtn->property("visible").toBool(), false);
    QCOMPARE(urlField->property("enabled").toBool(), true);

    delete obj;
}

// TEMP diagnostic: ApiRequestPage loaded directly (no ApiPage host).
// R5: the tab panel, driven at page level. The harness window never renders
// frames, so delegate-model incubation stalls and Repeater delegate items
// (a KV row's inputs) never appear — the urlbar test stays page-level items
// for the same reason. UiTabs' objectNames plus the page's functions and
// properties cover the whole panel without delegate items: add/remove rows,
// per-tab badge counts, KV row content (kvParamSet/Toggle/Remove), enabled
// params appending to the "→ resolved" line, Body/Auth modes writing
// requestMap.body/.auth, and syncToRequestMap committing rows on save.
// Delegate rendering itself is verified live in the app (standalone repros
// with the real kit host the same layout fine).
void QmlTests::test_api_request_tabs()
{
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("logModel", g_logModel);
    const QString buildDir = QDir::currentPath();
    engine.rootContext()->setContextProperty(
        "iconBaseUrl", QUrl::fromLocalFile(buildDir + "/icons/").toString());

    FakeApiService api;
    api.setEnvironments(QVariantList{
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("local")},
            {QStringLiteral("name"), QStringLiteral("local")},
            {QStringLiteral("vars"), QVariantList{
                QVariantMap{
                    {QStringLiteral("name"), QStringLiteral("baseUrl")},
                    {QStringLiteral("value"), QStringLiteral("http://localhost:8000")},
                    {QStringLiteral("secret"), false},
                },
            }},
        },
    });
    api.setActiveEnvId(QStringLiteral("local"));
    engine.rootContext()->setContextProperty("apiService", &api);

    FakeProjectServiceForApi proj;
    proj.setProjects(QVariantList{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("p/one")},
                    {QStringLiteral("name"), QStringLiteral("personizely-backend")}}});
    engine.rootContext()->setContextProperty("projectService", &proj);

    FakeDashboardForApi dash;
    engine.rootContext()->setContextProperty("dashboard", &dash);

    QQmlComponent component(&engine,
        QUrl::fromLocalFile(sourceQmlPath("ApiPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));

    QQuickWindow window;
    window.resize(1000, 800);
    QObject *obj = component.createWithInitialProperties({}, engine.rootContext());
    QVERIFY(obj != nullptr);
    auto *page = qobject_cast<QQuickItem *>(obj);
    QVERIFY(page != nullptr);
    page->setParentItem(window.contentItem());
    page->setPosition(QPointF(0, 0));
    page->setSize(QSizeF(1000, 800));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(50);

    QVERIFY(QMetaObject::invokeMethod(page, "openDraft",
        Q_ARG(QVariant, QStringLiteral("{{baseUrl}}/api/campaigns"))));
    QVERIFY(page->property("requestOpen").toBool());
    // The request column slides in (animSlide, visible: opacity > 0) — the
    // panel settles before the test drives it.
    QTest::qWait(600);

    QQuickItem *pane = page->findChild<QQuickItem *>(QStringLiteral("apiRequestPage"));
    QVERIFY(pane != nullptr);
    QQuickItem *tabs = pane->findChild<QQuickItem *>(QStringLiteral("apiTabs"));
    QVERIFY(tabs != nullptr);
    QQuickItem *resolved = pane->findChild<QQuickItem *>(QStringLiteral("apiResolvedLine"));
    QVERIFY(resolved != nullptr);
    QQuickItem *addParam = pane->findChild<QQuickItem *>(QStringLiteral("apiAddParam"));
    QVERIFY(addParam != nullptr);

    // Fresh draft: Params active, four tabs, no counts; modes reset to
    // "none" (the schema stores body text and auth fields, not the mode).
    QCOMPARE(tabs->property("currentIndex").toInt(), 0);
    QCOMPARE(pane->property("tabIndex").toInt(), 0);
    QCOMPARE(pane->property("bodyMode").toString(), QStringLiteral("none"));
    QCOMPARE(pane->property("authType").toString(), QStringLiteral("none"));
    QCOMPARE(resolved->property("text").toString(),
             QStringLiteral("→ http://localhost:8000/api/campaigns"));
    QVariantList items = tabs->property("items").toList();
    QCOMPARE(items.size(), 4);
    QCOMPARE(items.at(0).toMap().value("badge").toInt(), 0);
    QCOMPARE(items.at(1).toMap().value("badge").toInt(), 0);

    // "+ Add param" (the page-level dashed pill): one row, badge follows.
    QVERIFY(QMetaObject::invokeMethod(addParam, "clicked"));
    QTest::qWait(20);
    items = tabs->property("items").toList();
    QCOMPARE(items.at(0).toMap().value("badge").toInt(), 1);
    QCOMPARE(pane->property("paramRows").toList().size(), 1);

    // Key/value edits land in the row and pull the resolved line's query —
    // kvParamSet is the exact handler the row inputs' onTextEdited calls.
    QVERIFY(QMetaObject::invokeMethod(pane, "kvParamSet",
        Q_ARG(QVariant, 0), Q_ARG(QVariant, QStringLiteral("name")),
        Q_ARG(QVariant, QStringLiteral("page"))));
    QVERIFY(QMetaObject::invokeMethod(pane, "kvParamSet",
        Q_ARG(QVariant, 0), Q_ARG(QVariant, QStringLiteral("value")),
        Q_ARG(QVariant, QStringLiteral("2"))));
    QTest::qWait(20);
    QVariantList prows = pane->property("paramRows").toList();
    QCOMPARE(prows.size(), 1);
    QCOMPARE(prows.at(0).toMap().value("name").toString(), QStringLiteral("page"));
    QCOMPARE(prows.at(0).toMap().value("value").toString(), QStringLiteral("2"));
    QCOMPARE(resolved->property("text").toString(),
             QStringLiteral("→ http://localhost:8000/api/campaigns?page=2"));

    // The enabled toggle re-reads its state on save; disabled rows leave the
    // preview, enabled ones come back (the badge count is unaffected).
    QVERIFY(QMetaObject::invokeMethod(pane, "kvParamToggle",
        Q_ARG(QVariant, 0), Q_ARG(QVariant, false)));
    QTest::qWait(20);
    QCOMPARE(pane->property("paramRows").toList().at(0).toMap()
                 .value("enabled").toBool(), false);
    QCOMPARE(resolved->property("text").toString(),
             QStringLiteral("→ http://localhost:8000/api/campaigns"));
    QVERIFY(QMetaObject::invokeMethod(pane, "kvParamToggle",
        Q_ARG(QVariant, 0), Q_ARG(QVariant, true)));
    QTest::qWait(20);
    QCOMPARE(resolved->property("text").toString(),
             QStringLiteral("→ http://localhost:8000/api/campaigns?page=2"));

    // A second row, then remove it with the ✕ handler; only the first stays.
    QVERIFY(QMetaObject::invokeMethod(addParam, "clicked"));
    QTest::qWait(20);
    items = tabs->property("items").toList();
    QCOMPARE(items.at(0).toMap().value("badge").toInt(), 2);
    QVERIFY(QMetaObject::invokeMethod(pane, "kvRemoveParam", Q_ARG(QVariant, 1)));
    QTest::qWait(20);
    items = tabs->property("items").toList();
    QCOMPARE(items.at(0).toMap().value("badge").toInt(), 1);
    QCOMPARE(resolved->property("text").toString(),
             QStringLiteral("→ http://localhost:8000/api/campaigns?page=2"));

    // Headers tab: the same editor on its own rows; panels swap visibility.
    QVERIFY(QMetaObject::invokeMethod(tabs, "activated", Q_ARG(int, 1)));
    QTest::qWait(20);
    QCOMPARE(tabs->property("currentIndex").toInt(), 1);
    QCOMPARE(pane->property("tabIndex").toInt(), 1);
    QQuickItem *paramsPanel = pane->findChild<QQuickItem *>(QStringLiteral("apiParamsPanel"));
    QQuickItem *headersPanel = pane->findChild<QQuickItem *>(QStringLiteral("apiHeadersPanel"));
    QVERIFY(paramsPanel != nullptr);
    QVERIFY(headersPanel != nullptr);
    QCOMPARE(paramsPanel->property("visible").toBool(), false);
    QCOMPARE(headersPanel->property("visible").toBool(), true);

    QQuickItem *addHeader = pane->findChild<QQuickItem *>(QStringLiteral("apiAddHeader"));
    QVERIFY(addHeader != nullptr);
    QVERIFY(QMetaObject::invokeMethod(addHeader, "clicked"));
    QTest::qWait(20);
    items = tabs->property("items").toList();
    QCOMPARE(items.at(1).toMap().value("badge").toInt(), 1);
    QVERIFY(QMetaObject::invokeMethod(pane, "kvHeaderSet",
        Q_ARG(QVariant, 0), Q_ARG(QVariant, QStringLiteral("name")),
        Q_ARG(QVariant, QStringLiteral("Authorization"))));
    QVERIFY(QMetaObject::invokeMethod(pane, "kvHeaderSet",
        Q_ARG(QVariant, 0), Q_ARG(QVariant, QStringLiteral("value")),
        Q_ARG(QVariant, QStringLiteral("Bearer q"))));
    QTest::qWait(20);
    QVariantMap h0 = pane->property("headerRows").toList().at(0).toMap();
    QCOMPARE(h0.value(QStringLiteral("name")).toString(), QStringLiteral("Authorization"));
    QCOMPARE(h0.value(QStringLiteral("value")).toString(), QStringLiteral("Bearer q"));

    // Body tab: JSON mode bares the mono textarea; the text commits into
    // requestMap.body via textChanged (TextArea has no textEdited signal).
    QVERIFY(QMetaObject::invokeMethod(tabs, "activated", Q_ARG(int, 2)));
    QTest::qWait(20);
    QCOMPARE(pane->property("tabIndex").toInt(), 2);
    QQuickItem *bodyMode = pane->findChild<QQuickItem *>(QStringLiteral("apiBodyMode"));
    QVERIFY(bodyMode != nullptr);
    QCOMPARE(bodyMode->property("currentIndex").toInt(), 0); // none initially
    QVERIFY(QMetaObject::invokeMethod(pane, "selectBodyMode", Q_ARG(QVariant, 1)));
    QTest::qWait(20);
    QCOMPARE(pane->property("bodyMode").toString(), QStringLiteral("json"));
    QCOMPARE(bodyMode->property("currentIndex").toInt(), 1);
    QQuickItem *bodyArea = pane->findChild<QQuickItem *>(QStringLiteral("apiBodyArea"));
    QVERIFY(bodyArea != nullptr);
    QCOMPARE(bodyArea->property("visible").toBool(), true);
    bodyArea->setProperty("text", QStringLiteral("{\"a\":1}"));
    QTest::qWait(20);

    // Auth tab: mode from the segmented control; fields live in page state
    // (authDraft) and survive mode switches — switching back rebuilds the
    // map from the draft, so a restored token never silently disappears.
    QVERIFY(QMetaObject::invokeMethod(tabs, "activated", Q_ARG(int, 3)));
    QTest::qWait(20);
    QCOMPARE(pane->property("tabIndex").toInt(), 3);
    QQuickItem *authMode = pane->findChild<QQuickItem *>(QStringLiteral("apiAuthMode"));
    QVERIFY(authMode != nullptr);
    QVERIFY(QMetaObject::invokeMethod(pane, "selectAuthType", Q_ARG(QVariant, 1)));
    QTest::qWait(20);
    QCOMPARE(pane->property("authType").toString(), QStringLiteral("bearer"));
    QCOMPARE(authMode->property("currentIndex").toInt(), 1);
    QVERIFY(QMetaObject::invokeMethod(pane, "setAuthField",
        Q_ARG(QVariant, QStringLiteral("token")), Q_ARG(QVariant, QStringLiteral("tok"))));
    QVERIFY(QMetaObject::invokeMethod(pane, "selectAuthType", Q_ARG(QVariant, 2)));
    QTest::qWait(20);
    QCOMPARE(pane->property("authType").toString(), QStringLiteral("basic"));
    QVERIFY(QMetaObject::invokeMethod(pane, "setAuthField",
        Q_ARG(QVariant, QStringLiteral("username")), Q_ARG(QVariant, QStringLiteral("user"))));
    QVERIFY(QMetaObject::invokeMethod(pane, "setAuthField",
        Q_ARG(QVariant, QStringLiteral("password")), Q_ARG(QVariant, QStringLiteral("pw"))));
    // Back to Bearer: the draft (page state) still holds the token.
    QVERIFY(QMetaObject::invokeMethod(pane, "selectAuthType", Q_ARG(QVariant, 1)));
    QTest::qWait(20);
    QVariantMap authDraft = pane->property("authDraft").value<QVariantMap>();
    QCOMPARE(authDraft.value(QStringLiteral("token")).toString(), QStringLiteral("tok"));
    QVariantMap auth = pane->property("requestMap").value<QVariantMap>()
                           .value(QStringLiteral("auth")).toMap();
    QCOMPARE(auth.value(QStringLiteral("type")).toString(), QStringLiteral("bearer"));
    QCOMPARE(auth.value(QStringLiteral("token")).toString(), QStringLiteral("tok"));
    // Back to Basic for the save: user+pass in place.
    QVERIFY(QMetaObject::invokeMethod(pane, "selectAuthType", Q_ARG(QVariant, 2)));
    QTest::qWait(20);

    // Saving syncs the rows and the body/auth state into the working map.
    QVERIFY(QMetaObject::invokeMethod(pane, "syncToRequestMap"));
    QTest::qWait(20);
    QCOMPARE(pane->property("dirty").toBool(), true);
    QVariantMap working = pane->property("requestMap").value<QVariantMap>();
    QVariantList params = working.value(QStringLiteral("params")).toList();
    QCOMPARE(params.size(), 1);
    QVariantMap p0 = params.at(0).toMap();
    QCOMPARE(p0.value(QStringLiteral("name")).toString(), QStringLiteral("page"));
    QCOMPARE(p0.value(QStringLiteral("value")).toString(), QStringLiteral("2"));
    QCOMPARE(p0.value(QStringLiteral("enabled")).toBool(), true);
    QVariantList headers = working.value(QStringLiteral("headers")).toList();
    QCOMPARE(headers.size(), 1);
    QVariantMap hh0 = headers.at(0).toMap();
    QCOMPARE(hh0.value(QStringLiteral("name")).toString(), QStringLiteral("Authorization"));
    QCOMPARE(hh0.value(QStringLiteral("value")).toString(), QStringLiteral("Bearer q"));
    QCOMPARE(working.value(QStringLiteral("body")).toString(), QStringLiteral("{\"a\":1}"));
    QVariantMap a0 = working.value(QStringLiteral("auth")).toMap();
    QCOMPARE(a0.value(QStringLiteral("type")).toString(), QStringLiteral("basic"));
    QCOMPARE(a0.value(QStringLiteral("username")).toString(), QStringLiteral("user"));
    QCOMPARE(a0.value(QStringLiteral("password")).toString(), QStringLiteral("pw"));

    // Sending locks the panel (page-level surfaces), like the URL bar.
    QQuickItem *urlField = pane->findChild<QQuickItem *>(QStringLiteral("apiUrlField"));
    QQuickItem *methodSelect = pane->findChild<QQuickItem *>(QStringLiteral("apiMethodSelect"));
    QVERIFY(urlField != nullptr);
    QVERIFY(methodSelect != nullptr);
    QVERIFY(QMetaObject::invokeMethod(pane, "startSend"));
    QTest::qWait(20);
    QCOMPARE(pane->property("sending").toBool(), true);
    QCOMPARE(urlField->property("enabled").toBool(), false);
    QCOMPARE(methodSelect->property("enabled").toBool(), false);
    QCOMPARE(addParam->property("enabled").toBool(), false);
    QCOMPARE(bodyMode->property("enabled").toBool(), false);
    QCOMPARE(authMode->property("enabled").toBool(), false);
    QVERIFY(QMetaObject::invokeMethod(pane, "cancelSend"));
    QTest::qWait(20);
    QCOMPARE(pane->property("sending").toBool(), false);
    QCOMPARE(urlField->property("enabled").toBool(), true);
    QCOMPARE(addParam->property("enabled").toBool(), true);

    delete obj;
}

// The "unsaved changes · Save" tail lives on the Dashboard header's breadcrumb
// row, right-aligned (the design's request row was ← Back · crumbs · unsaved
// changes · Save; both the crumbs and the tail are the header's). Drive the
// wiring end to end: hidden until a request is open, appears for a draft, its
// click reaches the pane's save() (dialog name field gets the suggested name),
// and it collapses again once the request is saved clean.
void QmlTests::test_header_save_tail()
{
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("projectService", g_projectService);
    engine.rootContext()->setContextProperty("logModel", g_logModel);
    static LogModel commandLog;
    engine.rootContext()->setContextProperty("commandLog", &commandLog);
    static CommandExecutor commandExecutor;
    engine.rootContext()->setContextProperty("commandExecutor", &commandExecutor);
    FakeApiService api;
    engine.rootContext()->setContextProperty("apiService", &api);

    const QString buildDir = QDir::currentPath();
    engine.rootContext()->setContextProperty(
        "iconBaseUrl", QUrl::fromLocalFile(buildDir + "/icons/").toString());
    QString qmlPath = QDir::cleanPath(buildDir + "/../qml/Dashboard.qml");
    if (!QFile::exists(qmlPath))
        qmlPath = buildDir + "/qml/Dashboard.qml";

    QQmlComponent component(&engine, QUrl::fromLocalFile(qmlPath));
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));

    QQuickWindow window;
    window.resize(960, 800);
    QObject *obj = component.createWithInitialProperties({}, engine.rootContext());
    QVERIFY(obj != nullptr);
    auto *dashboard = qobject_cast<QQuickItem *>(obj);
    QVERIFY(dashboard != nullptr);
    dashboard->setParentItem(window.contentItem());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(50);

    QQuickItem *tail = dashboard->findChild<QQuickItem *>("headerSaveTail");
    QVERIFY(tail != nullptr);
    QQuickItem *hint = dashboard->findChild<QQuickItem *>("headerUnsavedLabel");
    QVERIFY(hint != nullptr);
    QQuickItem *saveButton = dashboard->findChild<QQuickItem *>("headerSaveButton");
    QVERIFY(saveButton != nullptr);

    // Nothing open: the tail stays hidden (visible is bound to
    // apiPage.requestUnsaved).
    QCOMPARE(tail->property("visible").toBool(), false);
    QCOMPARE(hint->property("visible").toBool(), false);
    QCOMPARE(saveButton->property("visible").toBool(), false);

    // Open a draft: drafts are always unsaved -> the tail appears, the
    // button carries the Lucide save icon (themed through Theme.icon).
    QQuickItem *apiPage = dashboard->findChild<QQuickItem *>("apiPage");
    QVERIFY(apiPage != nullptr);
    QVERIFY(QMetaObject::invokeMethod(apiPage, "openDraft",
        Q_ARG(QVariant, QStringLiteral("http://localhost:8000/"))));
    QVERIFY(apiPage->property("requestOpen").toBool());
    QTest::qWait(50);
    QCOMPARE(tail->property("visible").toBool(), true);
    QCOMPARE(hint->property("visible").toBool(), true);
    QCOMPARE(saveButton->property("visible").toBool(), true);
    QVERIFY(saveButton->property("enabled").toBool());
    QVERIFY(!saveButton->property("leadingIconSource").toUrl().isEmpty());

    // Clicking the header button must reach the pane's save(): the save
    // dialog cannot open on a bare QQuickWindow (Popup needs the overlay), so
    // assert the effect — openFor prefills the dialog's name field.
    QVERIFY(QMetaObject::invokeMethod(saveButton, "clicked"));
    QTest::qWait(50);
    QObject *saveDialog = dashboard->findChild<QObject *>("apiSaveDialog");
    QVERIFY(saveDialog != nullptr);
    QQuickItem *dialogName = saveDialog->findChild<QQuickItem *>(
        QStringLiteral("apiSaveNameField"));
    QVERIFY(dialogName != nullptr);
    QCOMPARE(dialogName->property("text").toString(), QStringLiteral("GET /"));

    // Confirm through the dialog persists the draft; once saved clean the
    // tail collapses even though the request stays open.
    QVERIFY(QMetaObject::invokeMethod(saveDialog, "confirmSave"));
    QTest::qWait(50);
    QVERIFY(apiPage->property("requestOpen").toBool());
    QCOMPARE(api.requests().size(), 1);
    QCOMPARE(tail->property("visible").toBool(), false);
    QCOMPARE(saveButton->property("visible").toBool(), false);

    delete obj;
}

// AppDialog (and any AppDialog-based dialog) must open under both hosting
// scenarios the project uses: a plain Window (main.qml — the overlay does
// not exist there, so binding `parent: Overlay.overlay` resolved to null and
// open() silently no-oped: the Save dialog "never appeared", which read as
// "Enter does not save") and an ApplicationWindow (overlay present).
// Regression for that hosting path. Note: with enter/exit transitions the
// `opened` flag flips only after the enter animation finishes, so the
// assertions wait well past it.
void QmlTests::test_app_dialog_hosting()
{
    const QString dialogQml = QStringLiteral(
        "import QtQuick\n"
        "import QtQuick.Controls\n"
        "import QtQuick.Effects\n"
        "import \"components\"\n"
        "%1 {\n"
        "    id: win\n"
        "    width: 640\n"
        "    height: 480\n"
        "    visible: true\n"
        "    Popup {\n"
        "        id: nativePopup\n"
        "        objectName: \"nativePopup\"\n"
        "        width: 120\n"
        "        height: 70\n"
        "        modal: true\n"
        "        focus: true\n"
        "        anchors.centerIn: parent\n"
        "        enter: Transition {\n"
        "            NumberAnimation { property: \"opacity\"; from: 0; to: 1; duration: 160 }\n"
        "        }\n"
        "        exit: Transition {\n"
        "            NumberAnimation { property: \"opacity\"; from: 1; to: 0; duration: 120 }\n"
        "        }\n"
        "        Rectangle { anchors.fill: parent; color: \"#ff0000\" }\n"
        "    }\n"
        "    AppDialog {\n"
        "        id: appDlg\n"
        "        objectName: \"appDlg\"\n"
        "        title: \"X\"\n"
        "        Rectangle { width: 200; height: 80; color: \"#00ff00\" }\n"
        "    }\n"
        "    AppDialog {\n"
        "        id: appDlgMin\n"
        "        objectName: \"appDlgMin\"\n"
        "    }\n"
        "    ComboBox {\n"
        "        id: nativeCombo\n"
        "        objectName: \"nativeCombo\"\n"
        "        x: 420\n"
        "        y: 300\n"
        "        width: 120\n"
        "        model: [\"a\", \"b\"]\n"
        "    }\n"
        "    Component.onCompleted: Qt.callLater(function() {\n"
        "        console.log(\"probe appDlg parent =\", appDlg.parent,\n"
        "                    \"size =\", appDlg.width, \"x\", appDlg.height)\n"
        "        nativePopup.open()\n"
        "        appDlg.open()\n"
        "        appDlgMin.open()\n"
        "        nativeCombo.popup.open()\n"
        "        console.log(\"probe nativePopup.opened =\", nativePopup.opened)\n"
        "        console.log(\"probe appDlg.opened =\", appDlg.opened)\n"
        "        console.log(\"probe appDlgMin.opened =\", appDlgMin.opened)\n"
        "        console.log(\"probe nativeCombo.popup.opened =\", nativeCombo.popup.opened)\n"
        "    })\n"
        "}\n");

    // Main.qml hosts dialogs in a plain Window (not ApplicationWindow).
    // This is the scenario that must work for the app.
    for (const QString &hostType : {QStringLiteral("ApplicationWindow"),
                                    QStringLiteral("Window")}) {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        const QUrl base = QUrl::fromLocalFile(
            QDir::cleanPath(QDir::currentPath() + "/../qml/TestDialogHost.qml"));
        component.setData(dialogQml.arg(hostType).toUtf8(), base);
        QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
            ? QString("not ready") : component.errors().first().toString()));
        QScopedPointer<QObject> root(component.create());
        QVERIFY(root != nullptr);
        auto *win = qobject_cast<QQuickWindow *>(root.data());
        QVERIFY(win != nullptr);
        QVERIFY(QTest::qWaitForWindowExposed(win));
        QTest::qWait(400);

        QObject *nativePopup = root->findChild<QObject *>(QStringLiteral("nativePopup"));
        QVERIFY(nativePopup != nullptr);
        QVERIFY2(nativePopup->property("opened").toBool(),
                 qPrintable(QStringLiteral("QCQC2 Popup must open under %1")
                            .arg(hostType)));
        QObject *appDlg = root->findChild<QObject *>(QStringLiteral("appDlg"));
        QVERIFY(appDlg != nullptr);
        QVERIFY2(appDlg->property("opened").toBool(),
                 qPrintable(QStringLiteral("AppDialog must open under %1")
                            .arg(hostType)));
        QObject *combo = root->findChild<QObject *>(QStringLiteral("nativeCombo"));
        QVERIFY(combo != nullptr);
        QObject *comboPopup = combo->property("popup").value<QObject *>();
        QVERIFY(comboPopup != nullptr);
        QVERIFY2(comboPopup->property("opened").toBool(),
                 qPrintable(QStringLiteral("ComboBox popup must open under %1")
                            .arg(hostType)));
    }
}

// UiListElement is the shared list card. Its click handler must not swallow
// taps aimed at controls placed inside it (Docker/Databases cards carry
// buttons); only taps on the empty chrome count as a card click.
void QmlTests::test_ui_list_element_click_through()
{
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(
        "iconBaseUrl", QUrl::fromLocalFile(QDir::currentPath() + "/icons/").toString());

    const QString qml = QStringLiteral(
        "import QtQuick\n"
        "import \"Ui\"\n"
        "UiListElement {\n"
        "    id: card\n"
        "    width: 300\n"
        "    height: 60\n"
        "    UiButton {\n"
        "        objectName: \"innerBtn\"\n"
        "        anchors.centerIn: parent\n"
        "        label: \"Act\"\n"
        "    }\n"
        "}\n");

    QQmlComponent component(&engine);
    // Base the (in-memory) instance in the source qml dir so `import "Ui"`
    // resolves against the real qml/Ui tree, not the stale build copy.
    const QUrl base = QUrl::fromLocalFile(
        QDir::cleanPath(QDir::currentPath() + "/../qml/TestUiListElement.qml"));
    component.setData(qml.toUtf8(), base);
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));

    QScopedPointer<QObject> root(component.create());
    QVERIFY(root != nullptr);
    auto *card = qobject_cast<QQuickItem *>(root.data());
    QVERIFY(card != nullptr);
    QCOMPARE(card->property("radius").toInt(), 12);
    QVERIFY(card->property("clickable").toBool());

    QQuickItem *btn = card->findChild<QQuickItem *>(QStringLiteral("innerBtn"));
    QVERIFY(btn != nullptr);

    QQuickWindow window;
    window.resize(300, 60);
    card->setParentItem(window.contentItem());
    card->setPosition(QPointF(0, 0));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(50);

    QSignalSpy cardSpy(root.data(), SIGNAL(clicked()));
    QSignalSpy btnSpy(btn, SIGNAL(clicked()));
    QVERIFY(cardSpy.isValid());
    QVERIFY(btnSpy.isValid());

    // Tap on the inner button: the button fires, the card must not.
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(150, 30));
    QTest::qWait(50);
    QCOMPARE(btnSpy.count(), 1);
    QCOMPARE(cardSpy.count(), 0);

    // Tap on the empty card chrome: the card fires.
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(8, 8));
    QTest::qWait(50);
    QCOMPARE(cardSpy.count(), 1);
}

#include "qmltests.moc"
