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
class FrameworkConsole;

class ProjectService : public QObject {
    Q_OBJECT

    Q_PROPERTY(QmlTreeModel *treeModel READ treeModel CONSTANT)
    Q_PROPERTY(QJsonObject activeProject READ activeProject WRITE setActiveProject NOTIFY activeProjectChanged)
    Q_PROPERTY(QList<QJsonObject> activeProjectCommands READ activeProjectCommands NOTIFY activeProjectCommandsChanged)
    // Selected folder ({path, name}) and its manifest projects for the folder page.
    Q_PROPERTY(QJsonObject activeFolder READ activeFolder NOTIFY activeFolderChanged)
    Q_PROPERTY(QList<QJsonObject> activeFolderProjects READ activeFolderProjects NOTIFY activeFolderProjectsChanged)
    Q_PROPERTY(int projectCount READ projectCount NOTIFY projectsChanged)
    Q_PROPERTY(QVariantList pinnedCommands READ pinnedCommands NOTIFY pinnedChanged)
    // Recently run commands (persisted), newest first.
    Q_PROPERTY(QVariantList recentCommands READ recentCommands NOTIFY recentCommandsChanged)
    // Commands running right now (joined with project metadata).
    Q_PROPERTY(QVariantList runningCommands READ runningCommands NOTIFY runningCommandsChanged)
    Q_PROPERTY(QString rootPath READ rootPath NOTIFY projectsChanged)

public:
    explicit ProjectService(ProjectListModel *model, QObject *parent = nullptr);

    // All workspace roots, primary first. Scanning a folder appends it;
    // rescanning one root replaces only that root's projects.
    Q_INVOKABLE QStringList rootPaths() const { return m_rootPaths; }

    Q_INVOKABLE QmlTreeModel *treeModel() const { return m_treeModel; }
    QString rootPath() const { return m_rootPath; }

    void setSettings(Settings *settings);
    void setExecutor(CommandExecutor *executor);

    static QStringList splitArgs(const QString &text);

    // Assembles run arguments for a console record from saved per-arg
    // values (runConfig "argsValues": {argName: stringValue}).
    // Records without argsMeta keep their flat args unchanged
    // (composer/npm behavior). Returns the assembled QStringList;
    // `missing` collects the required positional names that are absent
    // or empty (empty list when everything required is filled).
    static QStringList assembleConsoleArgs(const QJsonObject &cmd, const QJsonObject &values,
                                           QStringList &missing);
    // Flag-era residue cleanup for stored argsValues (in place): options
    // accepting a value (VALUE_REQUIRED/VALUE_OPTIONAL) that still hold
    // the old flag sentinel "on" fall back to their declared default
    // (or are dropped when there is none). Real flags and non-option
    // keys are untouched.
    static void sanitizeConsoleValues(const QJsonObject &cmd, QJsonObject &values);

    Q_INVOKABLE void scanFolder(const QString &rootPath);
    Q_INVOKABLE bool restoreFromCache();
    // True when the folder (or its parent/subfolder) already contributes
    // projects to the workspace — rescanning it would only duplicate.
    Q_INVOKABLE bool isFolderKnown(const QString &folderPath) const;
    Q_INVOKABLE void selectProject(const QString &projectId);
    // Selects a folder for the folder page (its manifest projects grouped
    // in sections). Independent from the active project used by detail pages.
    Q_INVOKABLE void selectFolder(const QString &folderPath);
    // Display name of a folder (git repo name, else folder name).
    Q_INVOKABLE QString folderDisplayName(const QString &folderPath) const;
    // Breadcrumb segments from the workspace root to the folder
    // ([{name, path}...]); a single segment when outside known roots.
    Q_INVOKABLE QVariantList folderCrumbs(const QString &folderPath) const;
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
    // Recently run commands, persisted; clear wipes the history.
    Q_INVOKABLE QVariantList recentCommands() const;
    Q_INVOKABLE void clearRecentCommands();
    // Commands running right now, with resolved project metadata.
    Q_INVOKABLE QVariantList runningCommands() const;
    // Full launch: stored run configuration + overrides applied, then run.
    // Shared by the tray menu and the MCP server.
    Q_INVOKABLE bool runCommandEffective(const QString &projectRef, const QString &commandRef);
    // Same, but merges an in-memory values map over the stored
    // runConfig argsValues before assembling console args. Used for
    // secret values (password/token), which are deliberately not
    // persisted.
    Q_INVOKABLE bool runCommandEffectiveWithValues(const QString &projectRef, const QString &commandRef,
                                                   const QVariantMap &extraValues);
    bool runCommandEffectiveImpl(const QString &projectRef, const QString &commandRef,
                                 const QVariantMap &extraValues);
    int runResolved(const QString &commandId, const QString &executable,
                    const QStringList &args, const QString &workdir, const QString &label,
                    const QStringList &env, bool allowMultiple);
    // Stored run config (settings key runConfig/<pid>/<cid>):
    // executable/argsText/workdir/envText/allowMultiple. The overload
    // also returns the saved per-argument values ("argsValues") used to
    // assemble console commands; empty object when absent.
    void resolveRunConfig(const QString &projectId, const QString &commandId,
                          QString &executable, QStringList &args, QString &workdir,
                          QStringList &env, bool &allowMultiple);
    void resolveRunConfig(const QString &projectId, const QString &commandId,
                          QString &executable, QStringList &args, QString &workdir,
                          QStringList &env, bool &allowMultiple, QJsonObject &values);
    Q_INVOKABLE void runCommand(const QString &command, const QStringList &args, const QString &workingDir);
    Q_INVOKABLE void stopCommand(int pid);

    Q_INVOKABLE QList<QJsonObject> projects() const { return m_projects; }
    int projectCount() const { return m_projects.size(); }
    QJsonObject activeProject() const { return m_activeProject; }
    void setActiveProject(const QJsonObject &project);
    QList<QJsonObject> activeProjectCommands() const;
    QJsonObject activeFolder() const { return m_activeFolder; }
    QList<QJsonObject> activeFolderProjects() const { return m_activeFolderProjects; }

signals:
    void projectsChanged();
    void treeModelChanged();
    void pinnedChanged();
    void recentCommandsChanged();
    void runningCommandsChanged();
    void scanProgress(const QString &currentDir, int foundCount);
    void scanComplete(int projectCount);
    void activeProjectChanged();
    void activeProjectCommandsChanged();
    void activeFolderChanged();
    void activeFolderProjectsChanged();
    void projectSelected();
    void commandStarted(int pid, int memoryMb);
    void commandStopped(int pid);
    void commandFailed(int pid, const QString &error);
    // A console command with required arguments was launched while some
    // of them were still empty: the run was blocked before starting.
    void runBlocked(const QString &commandId, const QStringList &missing);
    // An explicit port was found in the args of a launched console
    // command (`--port <value>` / `--port=<value>`), for runtime state.
    void consoleDetectedPort(const QString &commandId, int port);
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
    QList<QJsonObject> m_recents; // {projectId, commandId, label, ...}, persisted
    QJsonObject m_activeProject;
    QList<QJsonObject> m_activeProjectCommands;
    QJsonObject m_activeFolder; // {path, name} for the folder page
    QList<QJsonObject> m_activeFolderProjects; // manifest projects at that path
    void refreshActiveFolderProjects();
    QSet<QString> m_visitedDirs;
    void persistWorkspace(const QString &rootPath);
    void applyProjects(const QList<QJsonObject> &projects);
    // mtime of a file (0 when absent); used for console cache invalidation.
    static qulonglong mtimeOf(const QString &path);
    // Console discovery pass: for every composer.json project, keeps a
    // valid consoleCache (framework + composer.lock + command dir mtimes
    // all current), otherwise drops console records, marks consoleLoading
    // and starts a background discovery (skipped when one is in-flight).
    void refreshConsoleDiscovery();
    void onDiscoveryFinished(const QString &projectPath, const QVariantList &commands);
    void onDiscoveryFailed(const QString &projectPath, const QString &reason);
    // Index of the project at this path in m_projects, -1 when absent.
    int projectIndex(const QString &projectPath, const QString &manifest) const;
    // Copy of a project into the matching m_scanned entry (by id), so
    // rescans carry console cache + records over.
    void syncScanned(const QJsonObject &project);
    // Re-syncs m_activeProject/m_activeProjectCommands after a change.
    void refreshActiveSelection();
    void mergeCustomCommands();
    void loadCustomCommands();
    void persistCustomCommands();
    void loadPins();
    void persistPins();
    void prunePins();
    void loadRecents();
    void persistRecents();
    void recordRecent(const QString &projectId, const QString &commandId,
                      const QString &label, const QString &projectName,
                      const QString &projectPath);
    // Static program detection: {program, port} from an executable + script
    // text (e.g. "vite --port 3000" -> vite:3000). Empty object = unknown.
    static QJsonObject detectProgram(const QString &executable, const QString &scriptText);
    // Follows `npm [--prefix DIR] [run] <name>` delegation chains across
    // manifests (e.g. backend "npm --prefix ../frontend run serve" ->
    // frontend "vite serve"), so proxied scripts are detected as the real
    // program. Cycle-safe, returns the original text when unresolvable.
    static QString resolveNpmScript(const QString &dir, const QString &script,
                                    int depth, QStringList &visited);
    // Script value (string or string array) as plain text for detection.
    static QString scriptTextOf(const QJsonValue &value);
    QString readManifestName(const QString &manifestPath, const QString &manifestName, const QString &fallback) const;

    ProjectListModel *m_projectListModel;
    QmlTreeModel *m_treeModel;
    Settings *m_settings = nullptr;
    CommandExecutor *m_executor = nullptr;
    FrameworkConsole *m_console = nullptr;
    QSet<QString> m_consoleInFlight; // projects with a discovery running
    QString m_rootPath;
    QStringList m_rootPaths; // every scanned root, cleaned, deduplicated
    static const QStringList s_manifests;
    static const QStringList s_skipDirs;
    // True when path equals root or lives under it (both cleaned).
    static bool isUnderRoot(const QString &path, const QString &root);
    // Copy of a project with user-added (custom:) commands removed.
    static QJsonObject stripCustomCommands(const QJsonObject &project);
};
