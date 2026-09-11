#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QVariant>
#include <QVariantList>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QJsonDocument>
#include <QMap>
#include <QSet>
#include <QTimer>
#include "projectlistmodel.h"
#include "treemodel.h"

class Settings;
class CommandExecutor;

class ProjectService : public QObject {
    Q_OBJECT

    Q_PROPERTY(QmlTreeModel *treeModel READ treeModel CONSTANT)
    Q_PROPERTY(QJsonObject activeProject READ activeProject WRITE setActiveProject NOTIFY activeProjectChanged)
    Q_PROPERTY(QList<QJsonObject> activeProjectCommands READ activeProjectCommands NOTIFY activeProjectCommandsChanged)
    Q_PROPERTY(int projectCount READ projectCount NOTIFY projectsChanged)
    Q_PROPERTY(QVariantList pinnedCommands READ pinnedCommands NOTIFY pinnedChanged)
    Q_PROPERTY(QString rootPath READ rootPath NOTIFY projectsChanged)

public:
    explicit ProjectService(ProjectListModel *model, QObject *parent = nullptr);

    Q_INVOKABLE QmlTreeModel *treeModel() const { return m_treeModel; }
    QString rootPath() const { return m_rootPath; }

    void setSettings(Settings *settings);
    void setExecutor(CommandExecutor *executor) { m_executor = executor; }

    static QStringList splitArgs(const QString &text);

    Q_INVOKABLE void scanFolder(const QString &rootPath);
    Q_INVOKABLE bool restoreFromCache();
    Q_INVOKABLE void selectProject(const QString &projectId);
    // Adds a user-defined command to a folder. Attaches to the folder's
    // scanned project, or to a "custom" pseudo-project when the folder has
    // none. Returns "projectId|commandId", empty on failure.
    Q_INVOKABLE QString addCustomCommand(const QString &label, const QString &folderPath,
                                         const QString &executable, const QString &argsText,
                                         const QString &workdir, const QString &envText,
                                         bool allowMultiple, int port = 0);
    // Pinned commands (tray menu), persisted in Settings
    Q_INVOKABLE QVariantList pinnedCommands() const;
    Q_INVOKABLE bool isPinned(const QString &projectId, const QString &commandId) const;
    Q_INVOKABLE void setPinned(const QString &projectId, const QString &commandId, bool pinned);
    // Full launch: stored run configuration + overrides applied, then run.
    // Shared by the tray menu and the MCP server.
    Q_INVOKABLE bool runCommandEffective(const QString &projectRef, const QString &commandRef);
    int runResolved(const QString &commandId, const QString &executable,
                    const QStringList &args, const QString &workdir, const QString &label,
                    const QStringList &env, bool allowMultiple);
    void resolveRunConfig(const QString &projectId, const QString &commandId,
                          QString &executable, QStringList &args, QString &workdir,
                          QStringList &env, bool &allowMultiple);
    Q_INVOKABLE void runCommand(const QString &command, const QStringList &args, const QString &workingDir);
    Q_INVOKABLE void stopCommand(int pid);

    Q_INVOKABLE QList<QJsonObject> projects() const { return m_projects; }
    int projectCount() const { return m_projects.size(); }
    QJsonObject activeProject() const { return m_activeProject; }
    void setActiveProject(const QJsonObject &project);
    QList<QJsonObject> activeProjectCommands() const;

signals:
    void projectsChanged();
    void treeModelChanged();
    void pinnedChanged();
    void scanProgress(const QString &currentDir, int foundCount);
    void scanComplete(int projectCount);
    void activeProjectChanged();
    void activeProjectCommandsChanged();
    void projectSelected();
    void commandStarted(int pid, int memoryMb);
    void commandStopped(int pid);
    void commandFailed(int pid, const QString &error);
    void logMessage(const QString &level, const QString &target, const QString &message);

private:
    void recursiveScan(const QString &path, int depth = 0);
    QList<QJsonObject> detectCommands(const QString &manifestPath, const QString &projectPath);
    QJsonObject detectRunCommands(const QString &manifestPath, const QString &projectPath, const QString &manifestName);
    void processManifest(const QString &manifestPath, const QString &projectPath, QJsonObject &project, const QString &manifestName);
    QStringList extractScripts(const QString &jsonPath);
    QStringList extractRunCommands(const QString &tomlPath);

    QList<QJsonObject> m_projects;
    QList<QJsonObject> m_scanned; // scan results without custom commands
    QList<QJsonObject> m_customCommands; // user-added, merged on apply
    QList<QJsonObject> m_pins; // {projectId, commandId}, persisted in Settings
    QJsonObject m_activeProject;
    QList<QJsonObject> m_activeProjectCommands;
    QSet<QString> m_visitedDirs;
    void persistWorkspace(const QString &rootPath);
    void applyProjects(const QList<QJsonObject> &projects);
    void mergeCustomCommands();
    void loadCustomCommands();
    void persistCustomCommands();
    void loadPins();
    void persistPins();
    void prunePins();
    // Static program detection: {program, port} from an executable + script
    // text (e.g. "vite --port 3000" -> vite:3000). Empty object = unknown.
    static QJsonObject detectProgram(const QString &executable, const QString &scriptText);
    QString readManifestName(const QString &manifestPath, const QString &manifestName, const QString &fallback) const;

    ProjectListModel *m_projectListModel;
    QmlTreeModel *m_treeModel;
    Settings *m_settings = nullptr;
    CommandExecutor *m_executor = nullptr;
    QString m_rootPath;
    static const QStringList s_manifests;
    static const QStringList s_skipDirs;
};
