#include "qmltests.h"
#include "treemodel.h"
#include "projectlistmodel.h"
#include "projectservice.h"
#include "commandexecutor.h"
#include "logmodel.h"
#include "mcpagents.h"
#include "settings.h"
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
            Q_ARG(QVariant, QVariant(QString::fromUtf8(msg))));
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

void QmlTests::test_console_format_colors()
{
    QQmlEngine engine;
    const QString jsUrl = QUrl::fromLocalFile(sourceQmlPath("ConsoleFormat.js")).toString();
    const QString qml = QStringLiteral("import QtQml\nimport \"%1\" as CF\nQtObject {\n"
        "property string plain: CF.formatLine(\"09:00:00.000\", \"dev\", \"hello\", \"#fff\")\n"
        "property string colored: CF.ansiToHtml(\"\\u001b[31mred\\u001b[0m ok\")\n"
        "property string escaped: CF.formatLine(\"t\", \"<x>\", \"a&b\", \"#fff\")\n"
        "}").arg(jsUrl);
    QQmlComponent component(&engine);
    component.setData(qml.toUtf8(), QUrl());
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));
    QObject *obj = component.create();
    QVERIFY(obj != nullptr);
    QScopedPointer<QObject> guard(obj);

    const QString plain = obj->property("plain").toString();
    QVERIFY(plain.contains(QStringLiteral("[09:00:00.000]")));
    QVERIFY(plain.contains(QStringLiteral("dev:")));
    QVERIFY(plain.contains(QStringLiteral("hello")));
    const QString colored = obj->property("colored").toString();
    QVERIFY(colored.contains(QStringLiteral("color:#ef4444")));
    QVERIFY(colored.contains(QStringLiteral("red")));
    QVERIFY(colored.contains(QStringLiteral("ok")));
    QVERIFY(!colored.contains(QStringLiteral("\x1b")));
    const QString escaped = obj->property("escaped").toString();
    QVERIFY(escaped.contains(QStringLiteral("&lt;x&gt;")));
    QVERIFY(escaped.contains(QStringLiteral("a&amp;b")));
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
        Q_ARG(QVariant, QVariant(QString::fromUtf8("  ➜  Local:   http://localhost:5173/")))));
    const QVariantMap ports = obj->property("detectedPorts").toMap();
    // No active project in this harness: key is "|npm:serve"
    QCOMPARE(ports.value(QStringLiteral("|npm:serve")).toInt(), 5173);

    // Symfony CLI "Listening on" form, incl. schemeless host:port
    QVERIFY(QMetaObject::invokeMethod(obj, "appendCommandLine",
        Q_ARG(QVariant, QVariant(QStringLiteral("composer:serve"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("stdout"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("t"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("Listening on 127.0.0.1:8000")))));
    const QVariantMap ports2 = obj->property("detectedPorts").toMap();
    QCOMPARE(ports2.value(QStringLiteral("|composer:serve")).toInt(), 8000);

    // Bull-style "listening on port N" with no host at all
    QVERIFY(QMetaObject::invokeMethod(obj, "appendCommandLine",
        Q_ARG(QVariant, QVariant(QStringLiteral("npm:dev"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("stdout"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("t"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("Bull admin listening on port 8040")))));
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

    // Check that npm-32px.png exists (referenced by package.json projects)
    QVERIFY(QFile::exists(iconDir + "/npm-32px.png"));

    // Check that rust light/dark exist (referenced by Cargo.toml projects)
    QVERIFY(QFile::exists(iconDir + "/rust-light-32px.png"));
    QVERIFY(QFile::exists(iconDir + "/rust-dark-32px.png"));

    // Check that go light/dark exist (referenced by go.mod projects)
    QVERIFY(QFile::exists(iconDir + "/go-light-32px.png"));
    QVERIFY(QFile::exists(iconDir + "/go-dark-32px.png"));

    // Check that python-32px.png exists (referenced by pyproject.toml projects)
    QVERIFY(QFile::exists(iconDir + "/python-32px.png"));

    // Check that composer-32px.png exists (referenced by composer.json projects)
    QVERIFY(QFile::exists(iconDir + "/composer-32px.png"));

    // Check that default-32px.png exists
    QVERIFY(QFile::exists(iconDir + "/default-32px.png"));

    // Check that the folders stack exists (folder rows incl. roots, Browse button)
    QVERIFY(QFile::exists(iconDir + "/folders.png"));
    QVERIFY(QFile::exists(iconDir + "/folders-dark.png"));

    // Check that the folders stack exists (plain folder rows, Browse button)
    QVERIFY(QFile::exists(iconDir + "/folders.png"));
    QVERIFY(QFile::exists(iconDir + "/folders-dark.png"));

    // Check bash.png (custom-commands section header)
    QVERIFY(QFile::exists(iconDir + "/bash.png"));

    // Sidebar iconography: file-terminal marker for manifest rows,
    // stack for plain folders, each with a light variant for dark theme
    QVERIFY(QFile::exists(iconDir + "/file-terminal.png"));
    QVERIFY(QFile::exists(iconDir + "/file-terminal-dark.png"));
    QVERIFY(QFile::exists(iconDir + "/folders.png"));
    QVERIFY(QFile::exists(iconDir + "/folders-dark.png"));

    // Check UI glyphs used by icon-only ghost buttons
    QVERIFY(QFile::exists(iconDir + "/brush-cleaning.png"));
    QVERIFY(QFile::exists(iconDir + "/chevron-down.png"));
    QVERIFY(QFile::exists(iconDir + "/chevron-down-dark.png"));
    QVERIFY(QFile::exists(iconDir + "/chevron-up.png"));
    QVERIFY(QFile::exists(iconDir + "/chevron-up-dark.png"));
    QVERIFY(QFile::exists(iconDir + "/cog.png"));
    QVERIFY(QFile::exists(iconDir + "/cog-dark.png"));
    QVERIFY(QFile::exists(iconDir + "/terminal-light-32px.png"));
    QVERIFY(QFile::exists(iconDir + "/terminal-dark-32px.png"));
    QVERIFY(QFile::exists(iconDir + "/mcp-light-32px.png"));
    QVERIFY(QFile::exists(iconDir + "/mcp-dark-32px.png"));
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

    // Verify absolute icon URLs are used (qrc:/icons/ or iconBaseUrl-based),
    // never relative "icons/" paths (those break when resources are embedded)
    QRegularExpression absPattern(QStringLiteral("(qrc:/icons/|iconBaseUrl)"));
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

#include "qmltests.moc"
