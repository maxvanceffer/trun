#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSystemTrayIcon>
#include <QQuickWindow>
#include <QMenu>
#include <QDebug>
#include <QFile>
#include <QDir>
#include "projectservice.h"
#include "commandexecutor.h"
#include "logmodel.h"
#include "projectlistmodel.h"
#include "treemodel.h"
#include "settings.h"
#include "mcpserver.h"
#include "mcpagents.h"

int main(int argc, char *argv[])
{
    // Headless MCP server: no GUI, no tray, no QML engine.
    for (int i = 1; i < argc; ++i) {
        if (QString::fromUtf8(argv[i]) == QStringLiteral("--mcp")) {
            QCoreApplication app(argc, argv);
            app.setApplicationName("trun");
            app.setApplicationVersion("0.1.0");
            app.setOrganizationName("trun");

            static Settings settings;
            static ProjectListModel projectListModel;
            static ProjectService projectService(&projectListModel);
            static CommandExecutor commandExecutor;
            projectService.setSettings(&settings);
            projectService.restoreFromCache();

            static McpServer server(&projectService, &commandExecutor, &settings);
            server.startTransport();
            return app.exec();
        }
    }

    QApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");

    app.setApplicationName("trun");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("trun");

    qWarning() << "[App] Starting trun v" << app.applicationVersion();

    QQmlApplicationEngine engine;

    qmlRegisterType<QmlTreeItem>("Trun.Models", 1, 0, "QmlTreeItem");
    qmlRegisterType<QmlTreeModel>("Trun.Models", 1, 0, "QmlTreeModel");

    static Settings *settings = new Settings();
    static auto logModel = new LogModel();
    static auto commandLog = new LogModel();
    static auto projectListModel = new ProjectListModel();
    static auto projectService = new ProjectService(projectListModel);
    static auto commandExecutor = new CommandExecutor();
    static auto mcpAgents = new McpAgentManager();
    projectService->setSettings(settings);
    projectService->setExecutor(commandExecutor);
    projectService->restoreFromCache();

    engine.rootContext()->setContextProperty("logModel", logModel);
    engine.rootContext()->setContextProperty("commandLog", commandLog);

    // Reattach to processes left running by a previous session ("leave running").
    // Dead or recycled pids are dropped; survivors are written back.
    {
        const QVariantList orphans = settings->get("orphanedProcesses").toList();
        QVariantList survivors;
        for (const QVariant &entry : orphans) {
            const QVariantMap m = entry.toMap();
            if (commandExecutor->adoptExternal(m.value("pid").toInt(),
                                               m.value("command").toString(),
                                               m.value("args").toStringList(),
                                               m.value("workingDir").toString(),
                                               m.value("label").toString(),
                                               m.value("commandId").toString(),
                                               m.value("startedAt").toString())) {
                survivors << entry;
            }
        }
        if (survivors.size() != orphans.size())
            settings->set("orphanedProcesses", survivors);
    }
    engine.rootContext()->setContextProperty("projectListModel", projectListModel);
    engine.rootContext()->setContextProperty("projectService", projectService);
    engine.rootContext()->setContextProperty("commandExecutor", commandExecutor);
    engine.rootContext()->setContextProperty("mcpAgents", mcpAgents);
    engine.rootContext()->setContextProperty("treeModel", projectService->treeModel());
    engine.rootContext()->setContextProperty("Settings", settings);
    engine.rootContext()->setContextProperty(
        "trayAvailable", QSystemTrayIcon::isSystemTrayAvailable());

    QString exePath = QCoreApplication::applicationDirPath();
    QString iconsPath;
    QString devIconsPath = QDir::cleanPath(exePath + "/icons");
    if (QFile::exists(devIconsPath + "/npm-32px.png")) {
        iconsPath = devIconsPath;
    } else {
        QString bundleIconsPath = QDir::cleanPath(exePath + "/../../icons");
        if (QFile::exists(bundleIconsPath + "/npm-32px.png")) {
            iconsPath = bundleIconsPath;
        } else {
            QString deepIconsPath = QDir::cleanPath(exePath + "/../../../../Resources/icons");
            if (QFile::exists(deepIconsPath + "/npm-32px.png"))
                iconsPath = deepIconsPath;
        }
    }

    if (!iconsPath.isEmpty()) {
        engine.rootContext()->setContextProperty("iconBaseUrl", "file://" + iconsPath + "/");
        qWarning() << "[App] Icon base URL:" << "file://" + iconsPath + "/";
    } else {
        qWarning() << "[App] WARNING: Icons directory not found!";
    }

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);

    QString qmlPath = QDir::cleanPath(exePath + "/../qml/main.qml");
    if (!QFile::exists(qmlPath))
        qmlPath = QDir::cleanPath(exePath + "/qml/main.qml");

    qWarning() << "[App] Loading QML from:" << qmlPath;

    if (QFile::exists(qmlPath))
        engine.load(QUrl::fromLocalFile(qmlPath));
    else
        engine.load(QUrl("qrc:/qml/main.qml"));

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "[App] FAILED to load QML engine!";
        return -1;
    }

    QObject *rootObj = engine.rootObjects().first();

    // System tray: closing the window hides it, Quit lives in the tray menu.
    // Leaked intentionally for the app lifetime.
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        auto *tray = new QSystemTrayIcon(&app);
        QIcon trayIcon(iconsPath + "/terminal-dark-32px.png");
        trayIcon.setIsMask(true); // macOS: adapt the glyph to menu bar theme
        tray->setIcon(trayIcon);
        tray->setToolTip("trun");

        // Leaked intentionally: lives as long as the app, like the tray itself
        auto *menu = new QMenu();

        // Pinned commands section, rebuilt on every relevant change.
        // Leaked intentionally: actions are owned by the menu.
        auto rebuildTrayMenu = [=]() {
            menu->clear();
            QAction *showAction = menu->addAction(QStringLiteral("Show"));
            QObject::connect(showAction, &QAction::triggered, [rootObj]() {
                if (auto *window = qobject_cast<QQuickWindow*>(rootObj)) {
                    window->show();
                    window->raise();
                    window->requestActivate();
                }
            });

            const QVariantList pins = projectService->pinnedCommands();
            const QStringList running = commandExecutor->runningCommandIds();
            bool sectionAdded = false;
            for (const QVariant &entry : pins) {
                const QVariantMap pin = entry.toMap();
                const QString projectId = pin.value(QStringLiteral("projectId")).toString();
                const QString commandId = pin.value(QStringLiteral("commandId")).toString();
                QString projectName;
                QString label;
                for (const QJsonObject &p : projectService->projects()) {
                    if (p.value(QStringLiteral("id")).toString() != projectId)
                        continue;
                    projectName = p.value(QStringLiteral("name")).toString();
                    for (const QJsonValue &v :
                         p.value(QStringLiteral("commands")).toArray()) {
                        const QJsonObject c = v.toObject();
                        if (c.value(QStringLiteral("id")).toString() == commandId)
                            label = c.value(QStringLiteral("label")).toString();
                    }
                }
                if (label.isEmpty())
                    continue; // pruned on next scan; skip meanwhile
                if (!sectionAdded) {
                    menu->addSeparator();
                    sectionAdded = true;
                }
                // Runs are tracked by project-scoped key
                const QString runKey = projectId + u'|' + commandId;
                const bool isRunning = running.contains(runKey);
                QAction *pinAction = menu->addAction(
                    QStringLiteral("%1 %2 (%3)")
                        .arg(isRunning ? QStringLiteral("■") : QStringLiteral("▶"), label,
                             projectName));
                QObject::connect(pinAction, &QAction::triggered, [=]() {
                    if (commandExecutor->runningCommandIds().contains(runKey))
                        commandExecutor->stopCommand(runKey);
                    else
                        projectService->runCommandEffective(projectId, commandId);
                });
            }

            menu->addSeparator();
            QAction *quitAction = menu->addAction(QStringLiteral("Quit"));
            QObject::connect(quitAction, &QAction::triggered, [rootObj]() {
                QMetaObject::invokeMethod(rootObj, "requestQuit");
            });
        };
        rebuildTrayMenu();
        QObject::connect(projectService, &ProjectService::projectsChanged,
                         menu, rebuildTrayMenu);
        QObject::connect(projectService, &ProjectService::pinnedChanged, menu,
                         rebuildTrayMenu);
        QObject::connect(commandExecutor, &CommandExecutor::runningCommandsChanged, menu,
                         rebuildTrayMenu);
        tray->setContextMenu(menu);
        tray->show();
        qWarning() << "[App] Tray icon ready";
    } else {
        qWarning() << "[App] WARNING: no system tray, window close will quit";
    }

    return app.exec();
}
