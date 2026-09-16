#include "projectservice.h"
#include "commandexecutor.h"
#include "settings.h"
#include <QUrl>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>
#include <vector>

const QStringList ProjectService::s_manifests = {
    "package.json", "Cargo.toml", "go.mod", "Gemfile",
    "pyproject.toml", "pom.xml", "build.gradle",
    "CMakeLists.txt", "mix.exs", "composer.json"
};

const QStringList ProjectService::s_skipDirs = {
    ".git", "node_modules", "vendor", ".venv", "venv", "__pycache__",
    ".idea", ".vscode", "target", "build", "dist", "out", ".next", ".nuxt"
};

ProjectService::ProjectService(ProjectListModel *model, QObject *parent)
    : QObject(parent), m_projectListModel(model), m_treeModel(new QmlTreeModel(this)) {}

void ProjectService::setSettings(Settings *settings)
{
    m_settings = settings;
    loadCustomCommands();
    loadPins();
    loadRecents();
}

void ProjectService::setExecutor(CommandExecutor *executor)
{
    m_executor = executor;
    if (!m_executor)
        return;
    connect(m_executor, &CommandExecutor::runningCommandsChanged,
            this, &ProjectService::runningCommandsChanged);
    // Record every started command (any launch path) into the recent list.
    connect(m_executor, &CommandExecutor::started, this,
            [this](int, int, const QString &label, const QString &, const QString &runKey) {
                const int sep = runKey.lastIndexOf(u'|');
                if (sep < 0)
                    return;
                const QString projectId = runKey.left(sep);
                const QString commandId = runKey.mid(sep + 1);
                QString projectName;
                QString projectPath;
                for (const auto &p : m_projects) {
                    if (p.value(QStringLiteral("id")).toString() == projectId) {
                        projectName = p.value(QStringLiteral("name")).toString();
                        projectPath = p.value(QStringLiteral("project_path")).toString();
                        break;
                    }
                }
                recordRecent(projectId, commandId, label, projectName, projectPath);
            });
}

void ProjectService::loadCustomCommands()
{
    m_customCommands.clear();
    if (!m_settings)
        return;
    const QByteArray raw = m_settings->get(QStringLiteral("customCommands")).toByteArray();
    if (raw.isEmpty())
        return;
    for (const QJsonValue &v : QJsonDocument::fromJson(raw).array()) {
        const QJsonObject o = v.toObject();
        if (!o.value(QStringLiteral("label")).toString().isEmpty()
            && !o.value(QStringLiteral("folderPath")).toString().isEmpty())
            m_customCommands.append(o);
    }
}

void ProjectService::persistCustomCommands()
{
    if (!m_settings)
        return;
    QJsonArray arr;
    for (const auto &c : m_customCommands)
        arr.append(c);
    m_settings->set(QStringLiteral("customCommands"),
                    QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

QStringList ProjectService::splitArgs(const QString &text)
{
    QStringList out;
    QString cur;
    QChar quote;
    for (QChar ch : text) {
        if (!quote.isNull()) {
            if (ch == quote)
                quote = QChar();
            else
                cur += ch;
        } else if (ch == u'"' || ch == u'\'') {
            quote = ch;
        } else if (ch == u' ' || ch == u'\t') {
            if (!cur.isEmpty()) {
                out << cur;
                cur.clear();
            }
        } else {
            cur += ch;
        }
    }
    if (!cur.isEmpty())
        out << cur;
    return out;
}

void ProjectService::scanFolder(const QString &rootPath) {
    m_scanned.clear();
    m_visitedDirs.clear();
    m_treeModel->clear();

    QString actualPath = QUrl(rootPath).toLocalFile();
    if (actualPath.isEmpty()) actualPath = rootPath;
    const QString clean = QDir::cleanPath(actualPath);
    // Multi-root: a new folder is appended, rescanning a known one
    // replaces only that root's projects.
    if (!clean.isEmpty() && !m_rootPaths.contains(clean))
        m_rootPaths.append(clean);
    m_rootPath = m_rootPaths.value(0, clean);
    logMessage("info", "scan", "Starting scan: " + actualPath);
    recursiveScan(actualPath);
    // Merge: keep scanned projects from the other roots, drop customs here
    // (they re-merge from m_customCommands in applyProjects).
    QList<QJsonObject> merged;
    QSet<QString> seenIds;
    for (const QJsonObject &p : m_projects) {
        if (p.value(QStringLiteral("manifest")).toString() == QStringLiteral("custom"))
            continue;
        if (isUnderRoot(p.value(QStringLiteral("project_path")).toString(), clean))
            continue;
        merged.append(stripCustomCommands(p));
        seenIds.insert(p.value(QStringLiteral("id")).toString());
    }
    for (const QJsonObject &p : m_scanned) {
        const QString id = p.value(QStringLiteral("id")).toString();
        if (seenIds.contains(id))
            continue;
        seenIds.insert(id);
        merged.append(p);
    }
    m_scanned = merged;
    applyProjects(m_scanned);
    if (!m_projects.isEmpty())
        persistWorkspace(clean);
    emit scanComplete(m_projects.size());
    logMessage("info", "scan", "Scan complete: " + QString::number(m_projects.size()) + " projects found");
}

bool ProjectService::isUnderRoot(const QString &path, const QString &root)
{
    const QString cleanPath = QDir::cleanPath(path);
    const QString cleanRoot = QDir::cleanPath(root);
    if (cleanPath.isEmpty() || cleanRoot.isEmpty())
        return false;
    return cleanPath == cleanRoot
        || cleanPath.startsWith(cleanRoot + QLatin1Char('/'));
}

QJsonObject ProjectService::stripCustomCommands(const QJsonObject &project)
{
    QJsonObject out = project;
    QJsonArray cmds;
    for (const QJsonValue &v : project.value(QStringLiteral("commands")).toArray()) {
        if (!v.toObject().value(QStringLiteral("id")).toString().startsWith(
                QStringLiteral("custom:")))
            cmds.append(v);
    }
    out[QStringLiteral("commands")] = cmds;
    return out;
}

namespace {
constexpr int kCacheVersion = 3; // bump when scan output format changes
}

bool ProjectService::isFolderKnown(const QString &folderPath) const
{
    QString actualPath = QUrl(folderPath).toLocalFile();
    if (actualPath.isEmpty())
        actualPath = folderPath;
    const QString clean = QDir::cleanPath(actualPath);
    if (clean.isEmpty())
        return false;
    for (const QJsonObject &project : m_projects) {
        const QString known = QDir::cleanPath(
            project.value(QStringLiteral("project_path")).toString());
        if (known.isEmpty())
            continue;
        if (clean == known
            || clean.startsWith(known + QLatin1Char('/'))
            || known.startsWith(clean + QLatin1Char('/')))
            return true;
    }
    return false;
}

bool ProjectService::restoreFromCache()
{
    if (!m_settings)
        return false;

    if (!m_settings->hasWorkspace() && m_settings->has(QStringLiteral("configuredFolder"))) {
        scanFolder(m_settings->get(QStringLiteral("configuredFolder")).toString());
        return !m_projects.isEmpty();
    }

    const QString root = m_settings->rootFolder();
    QStringList roots = m_settings->rootFolders();
    if (roots.isEmpty() && !root.isEmpty())
        roots.append(QDir::cleanPath(root)); // legacy single-root cache
    // Drop roots that vanished from disk; keep the rest.
    for (int i = roots.size() - 1; i >= 0; --i) {
        if (!QDir(roots.at(i)).exists())
            roots.removeAt(i);
    }
    if (roots.isEmpty()) {
        logMessage("warn", "cache", "Root folder missing, cache discarded");
        m_settings->clearWorkspace();
        return false;
    }

    if (m_settings->cacheVersion() != kCacheVersion) {
        logMessage("info", "cache",
                   "Cache format changed, rescanning: " + roots.join(QStringLiteral(", ")));
        m_rootPaths.clear();
        for (const QString &r : roots)
            scanFolder(r);
        return !m_projects.isEmpty();
    }

    QList<QJsonObject> kept;
    const QJsonArray cached = m_settings->projects();
    for (const auto &value : cached) {
        QJsonObject project = value.toObject();
        const QString path = project.value(QStringLiteral("project_path")).toString();
        const QString manifest = project.value(QStringLiteral("manifest")).toString();
        if (path.isEmpty() || manifest.isEmpty())
            continue;
        if (QFile::exists(path + QLatin1Char('/') + manifest)) {
            // Migrate old caches where id was the bare path
            project[QStringLiteral("id")] = path + QLatin1Char('/') + manifest;
            kept.append(project);
        } else {
            logMessage("warn", "cache", "Project gone: " + path);
        }
    }

    if (kept.isEmpty()) {
        logMessage("warn", "cache", "No cached projects remain on disk");
        m_settings->clearWorkspace();
        return false;
    }

    if (kept.size() != cached.size()) {
        QJsonArray remaining;
        for (const auto &project : kept)
            remaining.append(project);
        m_settings->setProjects(remaining);
    }

    m_rootPaths = roots;
    m_rootPath = m_rootPaths.value(0);
    applyProjects(kept);
    logMessage("info", "cache", "Restored " + QString::number(kept.size()) + " projects");
    return true;
}

void ProjectService::persistWorkspace(const QString &rootPath)
{
    if (!m_settings)
        return;
    const QString clean = QDir::cleanPath(rootPath);
    if (!clean.isEmpty() && !m_rootPaths.contains(clean))
        m_rootPaths.append(clean);
    m_rootPath = m_rootPaths.value(0, clean);
    m_settings->setRootFolder(m_rootPath);
    m_settings->setRootFolders(m_rootPaths);
    m_settings->setCacheVersion(kCacheVersion);
    QJsonArray arr;
    for (auto project : m_projects) {
        if (project.value(QStringLiteral("manifest")).toString() == QStringLiteral("custom"))
            continue; // user commands persist separately, never in the scan cache
        arr.append(stripCustomCommands(project));
    }
    m_settings->setProjects(arr);
}

void ProjectService::applyProjects(const QList<QJsonObject> &projects)
{
    m_scanned = projects;
    mergeCustomCommands();
    prunePins();
    // Drop the selection when its project vanished (e.g. after a rescan)
    if (!m_activeProject.isEmpty()) {
        const QString activeId = m_activeProject.value(QStringLiteral("id")).toString();
        bool alive = false;
        for (const auto &p : m_projects) {
            if (p.value(QStringLiteral("id")).toString() == activeId) {
                m_activeProject = p; // refresh (commands may have changed)
                alive = true;
                break;
            }
        }
        if (!alive)
            m_activeProject = QJsonObject();
        m_activeProjectCommands.clear();
        if (alive) {
            const QJsonArray cmds = m_activeProject.value(QStringLiteral("commands")).toArray();
            for (const auto &v : cmds)
                m_activeProjectCommands.append(v.toObject());
        }
        emit activeProjectChanged();
        emit activeProjectCommandsChanged();
    }
    m_treeModel->clear();
    m_treeModel->setRootPaths(m_rootPaths);
    for (const auto &project : m_projects) {
        m_treeModel->addProject(
            project.value(QStringLiteral("project_path")).toString(),
            project.value(QStringLiteral("name")).toString(),
            project.value(QStringLiteral("description")).toString(),
            project.value(QStringLiteral("manifest")).toString(),
            QVariant::fromValue(project.value(QStringLiteral("commands")).toArray())
        );
    }
    m_projectListModel->setProjects(m_projects);
    emit projectsChanged();
    emit treeModelChanged();
}

void ProjectService::mergeCustomCommands()
{
    m_projects = m_scanned;
    for (const QJsonObject &custom : m_customCommands) {
        const QString folder = custom.value(QStringLiteral("folderPath")).toString();
        const QString cmdId = custom.value(QStringLiteral("commandId")).toString();
        if (folder.isEmpty() || cmdId.isEmpty())
            continue;
        int target = -1;
        for (int i = 0; i < m_projects.size(); ++i) {
            if (m_projects.at(i).value(QStringLiteral("project_path")).toString() == folder) {
                target = i;
                break;
            }
        }
        if (target < 0) {
            if (!QDir(folder).exists())
                continue;
            QJsonObject pseudo{
                {QStringLiteral("id"), folder + QLatin1Char('/') + QStringLiteral("custom")},
                {QStringLiteral("project_path"), folder},
                {QStringLiteral("manifest"), QStringLiteral("custom")},
                {QStringLiteral("name"), QFileInfo(folder).fileName()},
                {QStringLiteral("commands"), QJsonArray()},
            };
            m_projects.append(pseudo);
            target = m_projects.size() - 1;
        }
        QJsonObject project = m_projects.at(target);
        QJsonArray cmds = project.value(QStringLiteral("commands")).toArray();
        bool clash = false;
        for (const QJsonValue &v : cmds) {
            if (v.toObject().value(QStringLiteral("id")).toString() == cmdId) {
                clash = true;
                break;
            }
        }
        if (clash)
            continue;
        cmds.append(QJsonObject{
            {QStringLiteral("id"), cmdId},
            {QStringLiteral("category"), QStringLiteral("custom")},
            {QStringLiteral("label"), custom.value(QStringLiteral("label")).toString()},
            {QStringLiteral("command"),
             custom.value(QStringLiteral("executable")).toString()},
            {QStringLiteral("args"), QJsonArray::fromStringList(
                                         splitArgs(custom.value(QStringLiteral("argsText"))
                                                           .toString()))},
        });
        project[QStringLiteral("commands")] = cmds;
        m_projects[target] = project;
    }
}

QString ProjectService::addCustomCommand(const QString &label, const QString &folderPath,
                                         const QString &executable, const QString &argsText,
                                         const QString &workdir, const QString &envText,
                                         bool allowMultiple, int port)
{
    const QString cleanLabel = label.trimmed();
    const QString folder = QDir::cleanPath(folderPath);
    if (cleanLabel.isEmpty() || executable.trimmed().isEmpty() || folder.isEmpty()
        || !QDir(folder).exists()) {
        return {};
    }
    // Unique command id within the target project
    QString base = QStringLiteral("custom:") + cleanLabel;
    base.replace(u' ', u'-');
    const QJsonObject *targetProject = nullptr;
    for (const QJsonObject &p : m_projects) {
        if (p.value(QStringLiteral("project_path")).toString() == folder) {
            targetProject = &p;
            break;
        }
    }
    QSet<QString> taken;
    if (targetProject) {
        for (const QJsonValue &v :
             targetProject->value(QStringLiteral("commands")).toArray())
            taken.insert(v.toObject().value(QStringLiteral("id")).toString());
    }
    QString cmdId = base;
    for (int n = 2; taken.contains(cmdId); ++n)
        cmdId = base + u'-' + QString::number(n);

    const QString projectId = targetProject
        ? targetProject->value(QStringLiteral("id")).toString()
        : folder + QLatin1Char('/') + QStringLiteral("custom");
    QJsonObject custom{
        {QStringLiteral("commandId"), cmdId},
        {QStringLiteral("label"), cleanLabel},
        {QStringLiteral("folderPath"), folder},
        {QStringLiteral("executable"), executable.trimmed()},
        {QStringLiteral("argsText"), argsText},
        {QStringLiteral("workdir"), workdir.isEmpty() ? folder : QDir::cleanPath(workdir)},
        {QStringLiteral("envText"), envText},
        {QStringLiteral("allowMultiple"), allowMultiple},
        {QStringLiteral("port"), port},
    };
    m_customCommands.append(custom);
    persistCustomCommands();

    // Stored run configuration so Run/Detail/MCP reuse the values
    if (m_settings) {
        const QJsonObject cfg{
            {QStringLiteral("name"), cleanLabel},
            {QStringLiteral("executable"), executable.trimmed()},
            {QStringLiteral("workdir"), custom.value(QStringLiteral("workdir")).toString()},
            {QStringLiteral("argsText"), argsText},
            {QStringLiteral("port"), port},
            {QStringLiteral("envText"), envText},
            {QStringLiteral("allowMultiple"), allowMultiple},
        };
        m_settings->set(QStringLiteral("runConfig/%1/%2").arg(projectId, cmdId),
                        QString::fromUtf8(QJsonDocument(cfg).toJson(QJsonDocument::Compact)));
    }

    applyProjects(m_scanned);
    logMessage(QStringLiteral("info"), QStringLiteral("custom"),
               QStringLiteral("Added command: %1 (%2)").arg(cleanLabel, cmdId));
    return projectId + u'|' + cmdId;
}

void ProjectService::loadPins()
{
    m_pins.clear();
    if (!m_settings)
        return;
    const QByteArray raw = m_settings->get(QStringLiteral("pinnedCommands")).toByteArray();
    if (raw.isEmpty())
        return;
    for (const QJsonValue &v : QJsonDocument::fromJson(raw).array()) {
        const QJsonObject o = v.toObject();
        if (!o.value(QStringLiteral("projectId")).toString().isEmpty()
            && !o.value(QStringLiteral("commandId")).toString().isEmpty())
            m_pins.append(o);
    }
}

void ProjectService::persistPins()
{
    if (!m_settings)
        return;
    QJsonArray arr;
    for (const auto &p : m_pins)
        arr.append(p);
    m_settings->set(QStringLiteral("pinnedCommands"),
                    QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void ProjectService::loadRecents()
{
    m_recents.clear();
    if (!m_settings)
        return;
    const QByteArray raw = m_settings->get(QStringLiteral("recentCommands")).toByteArray();
    if (raw.isEmpty())
        return;
    for (const QJsonValue &v : QJsonDocument::fromJson(raw).array()) {
        const QJsonObject o = v.toObject();
        if (!o.value(QStringLiteral("projectId")).toString().isEmpty()
            && !o.value(QStringLiteral("commandId")).toString().isEmpty())
            m_recents.append(o);
    }
}

void ProjectService::persistRecents()
{
    if (!m_settings)
        return;
    QJsonArray arr;
    for (const auto &r : m_recents)
        arr.append(r);
    m_settings->set(QStringLiteral("recentCommands"),
                    QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void ProjectService::recordRecent(const QString &projectId, const QString &commandId,
                                  const QString &label, const QString &projectName,
                                  const QString &projectPath)
{
    if (projectId.isEmpty() || commandId.isEmpty())
        return;
    // Move an existing entry to the front instead of duplicating it.
    m_recents.erase(std::remove_if(m_recents.begin(), m_recents.end(),
                                   [&](const QJsonObject &r) {
                                       return r.value(QStringLiteral("projectId")).toString() == projectId
                                           && r.value(QStringLiteral("commandId")).toString() == commandId;
                                   }),
                    m_recents.end());
    m_recents.prepend(QJsonObject{
        {QStringLiteral("projectId"), projectId},
        {QStringLiteral("commandId"), commandId},
        {QStringLiteral("label"), label},
        {QStringLiteral("projectName"), projectName},
        {QStringLiteral("projectPath"), projectPath},
        {QStringLiteral("lastUsedAt"),
         QDateTime::currentDateTime().toString(Qt::ISODateWithMs)},
    });
    while (m_recents.size() > 20)
        m_recents.removeLast();
    persistRecents();
    emit recentCommandsChanged();
}

QVariantList ProjectService::recentCommands() const
{
    QVariantList out;
    for (const auto &r : m_recents)
        out << r.toVariantMap();
    return out;
}

void ProjectService::clearRecentCommands()
{
    if (m_recents.isEmpty())
        return;
    m_recents.clear();
    persistRecents();
    emit recentCommandsChanged();
}

QVariantList ProjectService::runningCommands() const
{
    QVariantList out;
    if (!m_executor)
        return out;
    const QVariantList processes = m_executor->runningProcesses();
    for (const QVariant &v : processes) {
        const QVariantMap rp = v.toMap();
        const QString runKey = rp.value(QStringLiteral("commandId")).toString();
        QString projectId;
        QString commandId = runKey;
        const int sep = runKey.lastIndexOf(u'|');
        if (sep >= 0) {
            projectId = runKey.left(sep);
            commandId = runKey.mid(sep + 1);
        }
        QString projectName;
        for (const auto &p : m_projects) {
            if (p.value(QStringLiteral("id")).toString() == projectId) {
                projectName = p.value(QStringLiteral("name")).toString();
                break;
            }
        }
        out << QVariantMap{
            {QStringLiteral("projectId"), projectId},
            {QStringLiteral("commandId"), commandId},
            {QStringLiteral("label"), rp.value(QStringLiteral("label"))},
            {QStringLiteral("pid"), rp.value(QStringLiteral("pid"))},
            {QStringLiteral("startedAt"), rp.value(QStringLiteral("startedAt"))},
            {QStringLiteral("projectName"), projectName},
        };
    }
    return out;
}

QVariantList ProjectService::pinnedCommands() const
{
    QVariantList out;
    for (const auto &p : m_pins)
        out << p.toVariantMap();
    return out;
}

bool ProjectService::isPinned(const QString &projectId, const QString &commandId) const
{
    for (const auto &p : m_pins) {
        if (p.value(QStringLiteral("projectId")).toString() == projectId
            && p.value(QStringLiteral("commandId")).toString() == commandId)
            return true;
    }
    return false;
}

void ProjectService::setPinned(const QString &projectId, const QString &commandId, bool pinned)
{
    if (projectId.isEmpty() || commandId.isEmpty())
        return;
    const bool has = isPinned(projectId, commandId);
    if (has == pinned)
        return;
    if (pinned) {
        m_pins.append(QJsonObject{{QStringLiteral("projectId"), projectId},
                                  {QStringLiteral("commandId"), commandId}});
    } else {
        m_pins.erase(std::remove_if(m_pins.begin(), m_pins.end(),
                                    [&](const QJsonObject &p) {
                                        return p.value(QStringLiteral("projectId")).toString()
                                                   == projectId
                                            && p.value(QStringLiteral("commandId")).toString()
                                                   == commandId;
                                    }),
                     m_pins.end());
    }
    persistPins();
    emit pinnedChanged();
}

void ProjectService::prunePins()
{
    if (m_pins.isEmpty())
        return;
    const int before = m_pins.size();
    m_pins.erase(std::remove_if(m_pins.begin(), m_pins.end(),
                                [&](const QJsonObject &p) {
                                    const QString pid =
                                        p.value(QStringLiteral("projectId")).toString();
                                    const QString cid =
                                        p.value(QStringLiteral("commandId")).toString();
                                    for (const auto &project : m_projects) {
                                        if (project.value(QStringLiteral("id")).toString() != pid)
                                            continue;
                                        for (const QJsonValue &v :
                                             project.value(QStringLiteral("commands")).toArray()) {
                                            if (v.toObject()
                                                    .value(QStringLiteral("id"))
                                                    .toString()
                                                == cid)
                                                return false;
                                        }
                                        return true;
                                    }
                                    return true;
                                }),
                 m_pins.end());
    if (m_pins.size() != before) {
        persistPins();
        emit pinnedChanged();
    }
}

void ProjectService::resolveRunConfig(const QString &projectId, const QString &commandId,
                                      QString &executable, QStringList &args, QString &workdir,
                                      QStringList &env, bool &allowMultiple)
{
    if (!m_settings)
        return;
    const QString raw =
        m_settings->get(QStringLiteral("runConfig/%1/%2").arg(projectId, commandId)).toString();
    if (raw.isEmpty())
        return;
    const QJsonObject cfg = QJsonDocument::fromJson(raw.toUtf8()).object();
    if (cfg.isEmpty())
        return;
    if (cfg.contains(QStringLiteral("executable")))
        executable = cfg.value(QStringLiteral("executable")).toString();
    if (cfg.contains(QStringLiteral("argsText")))
        args = splitArgs(cfg.value(QStringLiteral("argsText")).toString());
    if (cfg.contains(QStringLiteral("workdir")))
        workdir = cfg.value(QStringLiteral("workdir")).toString();
    if (cfg.contains(QStringLiteral("envText"))) {
        env.clear();
        for (QString line : cfg.value(QStringLiteral("envText")).toString().split(u'\n')) {
            line = line.trimmed();
            if (!line.isEmpty() && line.indexOf(u'=') > 0)
                env << line;
        }
    }
    if (cfg.contains(QStringLiteral("allowMultiple")))
        allowMultiple = cfg.value(QStringLiteral("allowMultiple")).toBool();
}

int ProjectService::runResolved(const QString &commandId, const QString &executable,
                                const QStringList &args, const QString &workdir,
                                const QString &label, const QStringList &env, bool allowMultiple)
{
    if (!m_executor || commandId.isEmpty())
        return 0;
    if (!allowMultiple)
        m_executor->stopCommand(commandId);
    return m_executor->runWithEnv(executable, args, workdir,
                                  label.isEmpty() ? executable : label, commandId, env);
}

bool ProjectService::runCommandEffective(const QString &projectRef, const QString &commandRef)
{
    QJsonObject project;
    for (const auto &p : m_projects) {
        if (p.value(QStringLiteral("id")).toString() == projectRef
            || p.value(QStringLiteral("project_path")).toString() == projectRef) {
            project = p;
            break;
        }
    }
    if (project.isEmpty())
        return false;
    QJsonObject cmd;
    for (const QJsonValue &v : project.value(QStringLiteral("commands")).toArray()) {
        const QJsonObject c = v.toObject();
        if (c.value(QStringLiteral("id")).toString() == commandRef
            || c.value(QStringLiteral("label")).toString() == commandRef) {
            cmd = c;
            break;
        }
    }
    if (cmd.isEmpty())
        return false;
    const QString projectId = project.value(QStringLiteral("id")).toString();
    const QString commandId = cmd.value(QStringLiteral("id")).toString();
    QString executable = cmd.value(QStringLiteral("command")).toString();
    QStringList args;
    for (const QJsonValue &a : cmd.value(QStringLiteral("args")).toArray())
        args << a.toString();
    QString workdir = project.value(QStringLiteral("project_path")).toString();
    QString label = cmd.value(QStringLiteral("label")).toString();
    QStringList env;
    bool allowMultiple = false;
    resolveRunConfig(projectId, commandId, executable, args, workdir, env, allowMultiple);
    // Runs are tracked by project-scoped key: bare command ids
    // (npm:serve) repeat across projects. Recent history is recorded from
    // CommandExecutor::started, so every launch path is covered.
    const QString runKey = projectId + u'|' + commandId;
    return runResolved(runKey, executable, args, workdir, label, env, allowMultiple) > 0;
}
QString ProjectService::scriptTextOf(const QJsonValue &value)
{
    if (value.isString())
        return value.toString();
    if (value.isArray()) {
        QStringList parts;
        for (const QJsonValue &a : value.toArray()) {
            if (a.isString())
                parts << a.toString();
        }
        return parts.join(u' ');
    }
    return {};
}

QString ProjectService::resolveNpmScript(const QString &dir, const QString &script,
                                         int depth, QStringList &visited)
{
    if (depth > 4 || script.isEmpty())
        return script;
    // Pure delegation: `npm [--prefix DIR] [run] <name>`
    // (also yarn/pnpm/bun, --cwd/--dir/-C). Anything else is the real text.
    static const QRegularExpression chainRe(
        QStringLiteral("^\\s*(npm|pnpm|yarn|bun)\\s+(?:(?:--prefix|--cwd|--dir|-C)\\s+(\\S+)\\s+)?(?:run\\s+)?([^\\s]+)\\s*$"));
    const auto m = chainRe.match(script);
    if (!m.hasMatch())
        return script;
    const QString targetDir =
        m.captured(2).isEmpty() ? dir : QDir::cleanPath(dir + u'/' + m.captured(2));
    const QString key = targetDir + u'|' + m.captured(3);
    if (visited.contains(key))
        return script; // cycle: keep the original text
    visited << key;
    QFile f(targetDir + QStringLiteral("/package.json"));
    if (!f.open(QIODevice::ReadOnly))
        return script;
    const QString next =
        scriptTextOf(QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("scripts")).toObject().value(m.captured(3)));
    if (next.isEmpty())
        return script;
    return resolveNpmScript(targetDir, next, depth + 1, visited);
}

QJsonObject ProjectService::detectProgram(const QString &executable, const QString &scriptText)
{

    struct Detector {
        const char *program;
        std::vector<const char *> exes; // string literals only: static lifetime
        int defaultPort;
        const char *portFlag; // regex with one digit group, matched against scriptText
    };
    static const std::vector<Detector> kDetectors = {
        {"vite", {"vite"}, 5173, "--port[=\\s]+(\\d+)"},
        {"next", {"next"}, 3000, "(?:-p|--port)[=\\s]+(\\d+)"},
        {"nuxt", {"nuxt"}, 3000, "--port[=\\s]+(\\d+)"},
        {"ng", {"ng"}, 4200, "--port[=\\s]+(\\d+)"},
        {"astro", {"astro"}, 4321, "--port[=\\s]+(\\d+)"},
        {"react-scripts", {"react-scripts"}, 3000, nullptr},
        {"symfony", {"symfony"}, 8000, "--port[=\\s]+(\\d+)"},
        {"rails", {"rails"}, 3000, "-p[=\\s]+(\\d+)"},
        {"django", {"django-admin", "manage.py"}, 8000, "runserver\\s+(?:[\\d.]+:)?(\\d+)"},
    };

    // Env wrappers hide the real program (`cross-env NODE_ENV=x vite`):
    // drop them (plus VAR=x pairs and their flags) before the first token.
    QStringList tokens = scriptText.split(u' ', Qt::SkipEmptyParts);
    bool strippedWrapper = false;
    bool skipNext = false;
    while (!tokens.isEmpty()) {
        if (skipNext) {
            tokens.removeFirst();
            skipNext = false;
            continue;
        }
        const QString &t = tokens.first();
        const bool isWrapper = t == QLatin1String("cross-env") || t == QLatin1String("env")
            || t == QLatin1String("dotenv") || t == QLatin1String("dotenv-cli") || t == QLatin1String("--");
        if (isWrapper || t.contains(u'=') || (strippedWrapper && t.startsWith(u'-'))) {
            if (t == QLatin1String("-e") || t == QLatin1String("--env"))
                skipNext = true;
            strippedWrapper = strippedWrapper || isWrapper;
            tokens.removeFirst();
            continue;
        }
        break;
    }
    const QString firstToken = tokens.value(0);
    QString base = QFileInfo(firstToken).fileName().toLower();
    if (base.isEmpty())
        base = QFileInfo(executable).fileName().toLower();
    if (base.endsWith(QStringLiteral(".cmd")) || base.endsWith(QStringLiteral(".exe")))
        base.chop(4);

    // php -S <addr>:<port> carries the port in its arguments
    if (base == QStringLiteral("php") && scriptText.contains(QStringLiteral("-S"))) {
        static const QRegularExpression addrRe(
            QStringLiteral("(?:localhost|127\\.0\\.0\\.1|0\\.0\\.0\\.0|\\[::1\\]|\\*):(\\d+)"));
        const auto m = addrRe.match(scriptText);
        QJsonObject out{{QStringLiteral("program"), QStringLiteral("php")}};
        out.insert(QStringLiteral("port"), m.hasMatch() ? m.captured(1).toInt() : 8000);
        return out;
    }

    for (const Detector &d : kDetectors) {
        bool hit = false;
        for (const char *exe : d.exes) {
            if (base == QString::fromUtf8(exe)) {
                hit = true;
                break;
            }
        }
        if (!hit)
            continue;
        int port = d.defaultPort;
        if (d.portFlag) {
            const auto m = QRegularExpression(QString::fromUtf8(d.portFlag)).match(scriptText);
            if (m.hasMatch())
                port = m.captured(1).toInt();
        }
        return QJsonObject{{QStringLiteral("program"), QString::fromUtf8(d.program)},
                           {QStringLiteral("port"), port}};
    }
    return {};
}

void ProjectService::recursiveScan(const QString &path, int depth) {
    if (depth > 20) return;

    QDir dir(path);
    if (!dir.exists()) return;

    // Check manifest in the current directory
    for (const QString &manifest : s_manifests) {
        QString manifestPath = path + "/" + manifest;
        if (QFile::exists(manifestPath)) {
            QJsonObject project;
            processManifest(manifestPath, path, project, manifest);
            if (project.contains("id")) {
                m_scanned.append(project);
                logMessage("debug", "scan", "Found project: " + project["name"].toString() + " at " + path);
            }
        }
    }

    auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &entry : entries) {
        QString entryName = entry.fileName();
        if (s_skipDirs.contains(entryName)) continue;

        emit scanProgress(entry.absoluteFilePath(), m_scanned.size());

        recursiveScan(entry.absoluteFilePath(), depth + 1);
    }
}

QString ProjectService::readManifestName(const QString &manifestPath, const QString &manifestName, const QString &fallback) const
{
    QFile f(manifestPath);
    if (!f.open(QIODevice::ReadOnly))
        return fallback;
    const QByteArray data = f.readAll();

    if (manifestName == QLatin1String("package.json") || manifestName == QLatin1String("composer.json")) {
        const QJsonObject obj = QJsonDocument::fromJson(data).object();
        const QString name = obj.value(QStringLiteral("name")).toString();
        return name.isEmpty() ? fallback : name;
    }

    if (manifestName == QLatin1String("Cargo.toml") || manifestName == QLatin1String("pyproject.toml")) {
        const QString section = manifestName == QLatin1String("Cargo.toml")
            ? QStringLiteral("[package]")
            : QStringLiteral("[project]");
        bool inSection = false;
        for (QString line : QString::fromUtf8(data).split(QLatin1Char('\n'))) {
            line = line.trimmed();
            if (line.startsWith(QLatin1Char('['))) {
                inSection = (line == section);
                continue;
            }
            if (!inSection || !line.startsWith(QLatin1String("name")))
                continue;
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq < 0)
                continue;
            QString value = line.mid(eq + 1).trimmed();
            value.remove(QLatin1Char('"'));
            value.remove(QLatin1Char('\''));
            return value.isEmpty() ? fallback : value;
        }
        return fallback;
    }

    if (manifestName == QLatin1String("go.mod")) {
        for (QString line : QString::fromUtf8(data).split(QLatin1Char('\n'))) {
            line = line.trimmed();
            if (!line.startsWith(QLatin1String("module ")))
                continue;
            const QString module = line.mid(7).trimmed();
            const int slash = module.lastIndexOf(QLatin1Char('/'));
            return slash >= 0 ? module.mid(slash + 1) : (module.isEmpty() ? fallback : module);
        }
        return fallback;
    }

    return fallback;
}

void ProjectService::processManifest(const QString &manifestPath, const QString &projectPath, QJsonObject &project, const QString &manifestName) {
    project["id"] = projectPath + QLatin1Char('/') + manifestName;
    project["project_path"] = projectPath;
    project["manifest"] = manifestName;
    project["name"] = readManifestName(manifestPath, manifestName, QFileInfo(projectPath).fileName());

    if (manifestName == "package.json" || manifestName == "composer.json") {
        QFile f(manifestPath);
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (!doc.isNull() && doc.isObject())
                project["description"] = doc.object().value("description").toString();
        }
    }

    QJsonArray cmdArr;
    for (const auto &cmd : detectCommands(manifestPath, projectPath)) {
        cmdArr.append(cmd);
    }
    project["commands"] = cmdArr;
}

QList<QJsonObject> ProjectService::detectCommands(const QString &manifestPath, const QString &projectPath) {
    QList<QJsonObject> commands;
    QString manifestName = QFileInfo(manifestPath).fileName();

    if (manifestName == "package.json") {
        QFile f(manifestPath);
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject scripts = doc.object().value("scripts").toObject();
                for (auto it = scripts.begin(); it != scripts.end(); ++it) {
                    QJsonObject cmd{
                        {"id", "npm:" + it.key()},
                        {"category", "npm"},
                        {"label", it.key()},
                        {"command", "npm"},
                        {"args", QJsonArray::fromStringList({"run", it.key()})}
                    };
                    QStringList visited;
                    const QJsonObject det = detectProgram(
                        QStringLiteral("npm"),
                        resolveNpmScript(projectPath, scriptTextOf(it.value()), 0, visited));
                    if (!det.isEmpty()) {
                        cmd.insert(QStringLiteral("detectedProgram"),
                                   det.value(QStringLiteral("program")));
                        cmd.insert(QStringLiteral("detectedPort"),
                                   det.value(QStringLiteral("port")));
                    }
                    commands.append(cmd);
                }
            }
        }
    } else if (manifestName == "composer.json") {
        QFile f(manifestPath);
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject scripts = doc.object().value("scripts").toObject();
                for (auto it = scripts.begin(); it != scripts.end(); ++it) {
                    // String and array values are runnable via `composer <name>`;
                    // nested objects (e.g. Symfony Flex "auto-scripts") are config, skip them
                    if (!it.value().isString() && !it.value().isArray())
                        continue;
                    QJsonObject cmd{
                        {"id", "composer:" + it.key()},
                        {"category", "composer"},
                        {"label", it.key()},
                        {"command", "composer"},
                        {"args", QJsonArray::fromStringList({it.key()})}
                    };
                    const QJsonObject det = detectProgram(
                        QStringLiteral("composer"), scriptTextOf(it.value()));
                    if (!det.isEmpty()) {
                        cmd.insert(QStringLiteral("detectedProgram"),
                                   det.value(QStringLiteral("program")));
                        cmd.insert(QStringLiteral("detectedPort"),
                                   det.value(QStringLiteral("port")));
                    }
                    commands.append(cmd);
                }
            }
        }
    } else if (manifestName == "Cargo.toml") {
        commands.append({
            {"id", "cargo:build"},
            {"category", "cargo"},
            {"label", "build"},
            {"command", "cargo"},
            {"args", QJsonArray::fromStringList({"build"})}
        });
        commands.append({
            {"id", "cargo:check"},
            {"category", "cargo"},
            {"label", "check"},
            {"command", "cargo"},
            {"args", QJsonArray::fromStringList({"check"})}
        });
        commands.append({
            {"id", "cargo:test"},
            {"category", "cargo"},
            {"label", "test"},
            {"command", "cargo"},
            {"args", QJsonArray::fromStringList({"test"})}
        });
        commands.append({
            {"id", "cargo:run"},
            {"category", "cargo"},
            {"label", "run"},
            {"command", "cargo"},
            {"args", QJsonArray::fromStringList({"run"})}
        });
    } else if (manifestName == "go.mod") {
        commands.append({
            {"id", "go:test"},
            {"category", "go"},
            {"label", "test"},
            {"command", "go"},
            {"args", QJsonArray::fromStringList({"test", "./..."})}
        });
        commands.append({
            {"id", "go:build"},
            {"category", "go"},
            {"label", "build"},
            {"command", "go"},
            {"args", QJsonArray::fromStringList({"build"})}
        });
    } else if (manifestName == "Gemfile") {
        commands.append({
            {"id", "bundler:test"},
            {"category", "ruby"},
            {"label", "test"},
            {"command", "bundle"},
            {"args", QJsonArray::fromStringList({"exec", "rspec"})}
        });
        commands.append({
            {"id", "bundler:run"},
            {"category", "ruby"},
            {"label", "server"},
            {"command", "bundle"},
            {"args", QJsonArray::fromStringList({"exec", "rails", "server"})}
        });
    }

    return commands;
}

void ProjectService::selectProject(const QString &projectId) {
    logMessage("info", "select", "Requested: " + projectId
               + " (known projects: " + QString::number(m_projects.size()) + ")");
    bool matched = false;
    for (const auto &project : m_projects) {
        const QString composite = project.value(QStringLiteral("project_path")).toString()
            + QLatin1Char('/') + project.value(QStringLiteral("manifest")).toString();
        if (project["id"] == projectId || composite == projectId) {
            m_activeProject = project;
            emit activeProjectChanged();
            matched = true;
            break;
        }
    }
    if (matched)
        logMessage("info", "select", "Active: " + m_activeProject.value(QStringLiteral("name")).toString());
    else
        logMessage("warn", "select", "No match for: " + projectId);

    m_activeProjectCommands.clear();
    QJsonArray cmds = m_activeProject.value("commands").toArray();
    for (const auto &v : cmds) {
        m_activeProjectCommands.append(v.toObject());
    }
    emit activeProjectCommandsChanged();
    emit projectSelected();
}

void ProjectService::setActiveProject(const QJsonObject &project) {
    if (m_activeProject != project) {
        m_activeProject = project;
        emit activeProjectChanged();
    }
}

QList<QJsonObject> ProjectService::activeProjectCommands() const {
    return m_activeProjectCommands;
}

void ProjectService::runCommand(const QString &command, const QStringList &args, const QString &workingDir) {
    QProcess *process = new QProcess();
    process->setWorkingDirectory(workingDir);
    process->start(command, args);

    if (process->waitForStarted(5000)) {
        int pid = process->processId();
        int mem = 0;
        emit commandStarted(pid, mem);
        logMessage("info", "command", "Started: " + command + " " + args.join(" ") + " (PID: " + QString::number(pid) + ")");

        QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                         [this, pid, command, process]() {
                             emit commandStopped(pid);
                             logMessage("info", "command", "Stopped: " + command);
                             process->deleteLater();
                         });

        QObject::connect(process, &QProcess::errorOccurred,
                         [this, pid, process](QProcess::ProcessError error) {
                             emit commandFailed(pid, process->errorString());
                         });

        QObject::connect(process, &QProcess::readyReadStandardOutput,
                         [this, process]() {
                             QByteArray data = process->readAllStandardOutput();
                             logMessage("debug", "stdout", QString::fromUtf8(data));
                         });

        QObject::connect(process, &QProcess::readyReadStandardError,
                         [this, process]() {
                             QByteArray data = process->readAllStandardError();
                             logMessage("warn", "stderr", QString::fromUtf8(data));
                         });
    } else {
        emit commandFailed(0, "Process failed to start");
    }
}

void ProjectService::stopCommand(int pid) {
    emit commandStopped(pid);
    logMessage("info", "command", "Stopping PID: " + QString::number(pid));
}
