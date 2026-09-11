#include "qmltests.h"
#include "treemodel.h"
#include "projectlistmodel.h"
#include "projectservice.h"
#include "commandexecutor.h"
#include "logmodel.h"
#include "mcpagents.h"
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

void QmlTests::test_tree_model_creation()
{
    QmlTreeModel model;
    QCOMPARE(model.rowCount(), 0);
}

void QmlTests::test_add_project_package_json()
{
    QmlTreeModel model;
    model.setRootPath("/test");

    model.addProject(
        "/test/npm-project",
        "NpmProject",
        "An NPM project",
        "package.json",
        QVariant()
    );

    // One top-level folder "npm-project" holding the project
    QCOMPARE(model.rowCount(), 1);

    QModelIndex folderIdx = model.index(0, 0);
    QVERIFY(folderIdx.isValid());
    QCOMPARE(model.data(folderIdx, QmlTreeModel::ItemTypeRole).toString(), QString("folder"));
    QCOMPARE(model.rowCount(folderIdx), 1);

    QModelIndex idx = model.index(0, 0, folderIdx);
    QVERIFY(idx.isValid());

    QCOMPARE(model.data(idx, QmlTreeModel::ManifestRole).toString(), QString("package.json"));
    QCOMPARE(model.data(idx, QmlTreeModel::NameRole).toString(), QString("NpmProject"));
    QCOMPARE(model.data(idx, QmlTreeModel::ItemTypeRole).toString(), QString("project"));
    QCOMPARE(model.data(idx, QmlTreeModel::ProjectIdRole).toString(),
             QString("/test/npm-project/package.json"));
}

void QmlTests::test_add_project_cargo_toml()
{
    QmlTreeModel model;
    model.setRootPath("/test");

    model.addProject(
        "/test/rust-project",
        "RustProject",
        "A Rust project",
        "Cargo.toml",
        QVariant()
    );

    QCOMPARE(model.rowCount(), 1);

    QModelIndex folderIdx = model.index(0, 0);
    QVERIFY(folderIdx.isValid());
    QCOMPARE(model.rowCount(folderIdx), 1);

    QModelIndex idx = model.index(0, 0, folderIdx);
    QCOMPARE(model.data(idx, QmlTreeModel::ManifestRole).toString(), QString("Cargo.toml"));
    QCOMPARE(model.data(idx, QmlTreeModel::NameRole).toString(), QString("RustProject"));
}

void QmlTests::test_add_folder_implicit()
{
    QmlTreeModel model;
    model.setRootPath("/test");

    // Folders are created implicitly when adding a project to a nested path
    model.addProject(
        "/test/myfolder/subproject",
        "SubProject",
        "A sub project in folder",
        "package.json",
        QVariant()
    );

    // Root should have 1 folder ("myfolder")
    QCOMPARE(model.rowCount(), 1);

    QModelIndex myfolderIdx = model.index(0, 0);
    QVERIFY(myfolderIdx.isValid());
    QCOMPARE(model.data(myfolderIdx, QmlTreeModel::ItemTypeRole).toString(), QString("folder"));
    QCOMPARE(model.rowCount(myfolderIdx), 1);

    QModelIndex subIdx = model.index(0, 0, myfolderIdx);
    QVERIFY(subIdx.isValid());
    QCOMPARE(model.data(subIdx, QmlTreeModel::ItemTypeRole).toString(), QString("folder"));
    QCOMPARE(model.rowCount(subIdx), 1);

    QModelIndex projIdx = model.index(0, 0, subIdx);
    QVERIFY(projIdx.isValid());
    QCOMPARE(model.data(projIdx, QmlTreeModel::ItemTypeRole).toString(), QString("project"));
    QCOMPARE(model.data(projIdx, QmlTreeModel::NameRole).toString(), QString("SubProject"));
}

void QmlTests::test_folder_with_child_project()
{
    QmlTreeModel model;
    model.setRootPath("/test");

    // Two manifests in the same folder group under one folder node
    model.addProject(
        "/test/backend",
        "acme-js",
        "JS part",
        "package.json",
        QVariant()
    );
    model.addProject(
        "/test/backend",
        "acme-php",
        "PHP part",
        "composer.json",
        QVariant()
    );

    QCOMPARE(model.rowCount(), 1); // one folder "backend"

    QModelIndex folderIdx = model.index(0, 0);
    QVERIFY(folderIdx.isValid());
    QCOMPARE(model.data(folderIdx, QmlTreeModel::ItemTypeRole).toString(), QString("folder"));
    QCOMPARE(model.rowCount(folderIdx), 2);

    QModelIndex first = model.index(0, 0, folderIdx);
    QModelIndex second = model.index(1, 0, folderIdx);
    QVERIFY(first.isValid() && second.isValid());
    QCOMPARE(model.data(first, QmlTreeModel::ManifestRole).toString(), QString("package.json"));
    QCOMPARE(model.data(second, QmlTreeModel::ManifestRole).toString(), QString("composer.json"));
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

    // Sidebar background must come from the Theme singleton
    // (preset sidebar token, depends on the OS color scheme)
    const QColor bg = obj->property("color").value<QColor>();
    QVERIFY(bg == QColor("#fbfbf9") || bg == QColor("#1d1d16"));
    QCOMPARE(obj->property("radius").toInt(), 8);
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
    QmlTreeModel model;
    McpAgentManager mcpAgents;
    engine.rootContext()->setContextProperty("treeModel", &model);
    engine.rootContext()->setContextProperty("projectService", g_projectService);
    engine.rootContext()->setContextProperty("logModel", g_logModel);
    engine.rootContext()->setContextProperty("mcpAgents", &mcpAgents);
    engine.rootContext()->setContextProperty(
        "iconBaseUrl", QUrl::fromLocalFile(QDir::currentPath() + "/icons/").toString());

    QQmlComponent component(&engine,
        QUrl::fromLocalFile(sourceQmlPath("Sidebar.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errors().isEmpty()
        ? QString("not ready") : component.errors().first().toString()));

    QObject *obj = component.create();
    QVERIFY(obj != nullptr);
    QScopedPointer<QObject> guard(obj);

    QObject *menu = obj->findChild<QObject*>(QStringLiteral("mcpMenu"));
    QVERIFY(menu != nullptr);
    // Custom primitive-built menu: fixed size by construction
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

    // Check that default-32px.png exists
    QVERIFY(QFile::exists(iconDir + "/default-32px.png"));

    // Check that folder-32px.png exists
    QVERIFY(QFile::exists(iconDir + "/folder-32px.png"));

    // Check UI glyphs used by icon-only ghost buttons
    QVERIFY(QFile::exists(iconDir + "/square-chevron-down.png"));
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

    // Empty state: nothing selected -> grid with header only, detail hidden
    QTest::qWait(50);
    dumpGeometry(dashboard);

    QQuickItem *gridView = dashboard->findChild<QQuickItem*>("gridView");
    QVERIFY(gridView != nullptr);
    QQuickItem *detailPage = dashboard->findChild<QQuickItem*>("detailPage");
    QVERIFY(detailPage != nullptr);
    QVERIFY(!detailPage->isVisible());

    // Anchor-based sections: header, commands (no vertical layout engine)
    QVERIFY(gridView->childItems().size() >= 2);
    QQuickItem *header = gridView->childItems().at(0);
    QQuickItem *commands = gridView->childItems().at(1);

    // Header pinned to the top with 48px height
    QCOMPARE(header->y(), 0.0);
    QCOMPARE(header->height(), 48.0);

    // Empty state: commands collapsed below the header with a margin
    QCOMPARE(commands->y(), 56.0);
    QCOMPARE(commands->height(), 0.0);

    // Now with a project that has commands: commands section becomes visible
    // below the header, header stays pinned to the top
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
    QCOMPARE(commands->y(), 56.0);
    QVERIFY(commands->height() > 0.0);

    // All command cards share one size across grid rows
    QQuickItem *grid = nullptr;
    for (auto *c : commands->childItems()) {
        if (QString(c->metaObject()->className()).contains("GridLayout")) {
            grid = c;
            break;
        }
    }
    QVERIFY2(grid != nullptr, "commands GridLayout not found");
    QList<QQuickItem *> cards;
    for (auto *c : grid->childItems()) {
        // QML delegates are named <FileName>_QMLTYPE_*; skip the Repeater itself
        if (QString(c->metaObject()->className()).contains("CommandCard"))
            cards << c;
    }
    QCOMPARE(cards.size(), 4);
    const qreal cardW = cards.first()->width();
    const qreal cardH = cards.first()->height();
    QVERIFY(cardW > 0.0 && cardH > 0.0);
    // Cards must fit content + margins (collapsed layout ~= button height only)
    QVERIFY2(cardH >= 40.0, qPrintable(QString("card too short: %1").arg(cardH)));
    // Card content must fit exactly: layout height == its implicit height,
    // otherwise children overflow the card (regression guard)
    QQuickItem *firstLayout = nullptr;
    {
        QList<QQuickItem *> stack{cards.first()};
        while (!stack.isEmpty() && firstLayout == nullptr) {
            QQuickItem *cur = stack.takeFirst();
            if (QString(cur->metaObject()->className()).contains("ColumnLayout")) {
                firstLayout = cur;
                break;
            }
            for (auto *c : cur->childItems())
                stack << c;
        }
    }
    QVERIFY(firstLayout != nullptr);
    QCOMPARE(firstLayout->height(), firstLayout->implicitHeight());
    for (auto *card : cards) {
        QCOMPARE(card->width(), cardW);
        QCOMPARE(card->height(), cardH);
    }

    // Click path: card -> dashboard.selectCommand -> selected border.
    // Selection keys are project-scoped ("<projectId>|<cmdId>").
    QQuickItem *firstCard = cards.first();
    const QString expectedId = g_projectService->projects().first()["commands"]
        .toArray().first().toObject()["id"].toString();
    QVERIFY(!expectedId.isEmpty());
    const QString expectedKey =
        g_projectService->projects().first()["id"].toString() + u'|' + expectedId;
    QVERIFY(QMetaObject::invokeMethod(firstCard, "selectThis"));
    QTest::qWait(20);
    QCOMPARE(obj->property("selectedCommandId").toString(), expectedKey);
    QVERIFY(firstCard->property("selected").toBool());

    // Detail page: opens with a slide, closes back to hidden
    QVERIFY(QMetaObject::invokeMethod(obj, "openDetail", Q_ARG(QVariant, QVariant(expectedId))));
    // Starts off-screen with the slide-in engaged (anchors must not own x:
    // the old anchors.fill made the open snap instantly instead of sliding)
    QCOMPARE(detailPage->x(), 960.0);
    QObject *slideIn = dashboard->findChild<QObject*>("slideIn");
    QVERIFY(slideIn != nullptr);
    QVERIFY(slideIn->property("running").toBool());
    QTest::qWait(500);
    QCOMPARE(obj->property("detailCommandId").toString(), expectedKey);
    QVERIFY(detailPage->isVisible());
    // Slide-in needs render-loop ticks (absent headless), so the final x
    // is only asserted via running above; the real app docks it on screen.
    // Slide-out needs render-loop ticks (absent headless), so assert that
    // closeDetail starts the animation; the real app finishes it on screen.
    QObject *slideOut = dashboard->findChild<QObject*>("slideOut");
    QVERIFY(slideOut != nullptr);
    QVERIFY(QMetaObject::invokeMethod(obj, "closeDetail"));
    QVERIFY(slideOut->property("running").toBool());
}

#include "qmltests.moc"
