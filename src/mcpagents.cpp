#include "mcpagents.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
const char *kEntryName = "trun";
}

McpAgentManager::McpAgentManager(QObject *parent)
    : QObject(parent), m_home(QDir::homePath())
{
}

QString McpAgentManager::agentBinary() const
{
    return QCoreApplication::applicationFilePath();
}

QString McpAgentManager::configPath(const Agent &a) const
{
    return QDir::cleanPath(m_home + u'/' + a.relPath);
}

bool McpAgentManager::backupFile(const QString &path)
{
    QString backup =
        path + QStringLiteral(".bak-trun-")
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"));
    int attempt = 0;
    while (QFile::exists(backup)) // two writes within the same second
        backup = path + QStringLiteral(".bak-trun-")
            + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"))
            + QStringLiteral("-%1").arg(++attempt);
    if (!QFile::copy(path, backup)) {
        m_error = QStringLiteral("backup failed: %1").arg(path);
        return false;
    }
    return true;
}

bool McpAgentManager::readJson(const QString &path, QJsonObject &out)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("cannot read: %1").arg(path);
        return false;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        m_error = QStringLiteral("invalid JSON, left untouched: %1 (%2)")
                      .arg(path, err.errorString());
        return false;
    }
    out = doc.object();
    return true;
}

bool McpAgentManager::writeJson(const QString &path, const QJsonObject &doc)
{
    if (QFile::exists(path) && !backupFile(path))
        return false;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_error = QStringLiteral("cannot write: %1").arg(path);
        return false;
    }
    f.write(QJsonDocument(doc).toJson(QJsonDocument::Indented));
    return true;
}

QVariantList McpAgentManager::scanAgents()
{
    return {scanClaude(), scanCodex(), scanOpencode(), scanJunie()};
}

QVariantMap McpAgentManager::scanClaude() const
{
    const QString path = QDir::cleanPath(m_home + QStringLiteral("/.claude.json"));
    const bool found = QDir(m_home + QStringLiteral("/.claude")).exists()
        || QFile::exists(path);
    bool installed = false;
    if (QFile::exists(path)) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonObject doc = QJsonDocument::fromJson(f.readAll()).object();
            installed = doc.value(QStringLiteral("mcpServers")).toObject().contains(
                QString::fromUtf8(kEntryName));
        }
    }
    return QVariantMap{{QStringLiteral("id"), QStringLiteral("claude")},
                       {QStringLiteral("name"), QStringLiteral("Claude Code")},
                       {QStringLiteral("configPath"), path},
                       {QStringLiteral("found"), found},
                       {QStringLiteral("installed"), installed},
                       {QStringLiteral("enabled"), installed},
                       {QStringLiteral("canToggle"), false}};
}

QVariantMap McpAgentManager::scanCodex() const
{
    const QString path = QDir::cleanPath(m_home + QStringLiteral("/.codex/config.toml"));
    const bool found = QDir(m_home + QStringLiteral("/.codex")).exists()
        || QFile::exists(path);
    bool installed = false;
    bool enabled = false;
    if (QFile::exists(path)) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            bool inBlock = false;
            enabled = true;
            for (const QByteArray &raw : f.readAll().split('\n')) {
                const QString line = QString::fromUtf8(raw).trimmed();
                if (line.startsWith(u'[')) {
                    inBlock = (line == QStringLiteral("[mcp_servers.trun]")
                               || line == QStringLiteral("[mcp_servers.\"trun\"]"));
                    continue;
                }
                if (!inBlock)
                    continue;
                installed = true;
                if (line.startsWith(QStringLiteral("enabled"))) {
                    const QString val = line.split(u'=').value(1).trimmed();
                    if (val == QStringLiteral("false"))
                        enabled = false;
                }
            }
        }
    }
    return QVariantMap{{QStringLiteral("id"), QStringLiteral("codex")},
                       {QStringLiteral("name"), QStringLiteral("Codex")},
                       {QStringLiteral("configPath"), path},
                       {QStringLiteral("found"), found},
                       {QStringLiteral("installed"), installed},
                       {QStringLiteral("enabled"), installed && enabled},
                       {QStringLiteral("canToggle"), true}};
}

QVariantMap McpAgentManager::scanOpencode() const
{
    const QString path =
        QDir::cleanPath(m_home + QStringLiteral("/.config/opencode/opencode.json"));
    const bool found = QDir(m_home + QStringLiteral("/.config/opencode")).exists()
        || QFile::exists(path);
    bool installed = false;
    bool enabled = false;
    if (QFile::exists(path)) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonObject trun = QJsonDocument::fromJson(f.readAll())
                                         .object()
                                         .value(QStringLiteral("mcp"))
                                         .toObject()
                                         .value(QString::fromUtf8(kEntryName))
                                         .toObject();
            installed = !trun.isEmpty();
            enabled = installed && trun.value(QStringLiteral("enabled")).toBool(true);
        }
    }
    return QVariantMap{{QStringLiteral("id"), QStringLiteral("opencode")},
                       {QStringLiteral("name"), QStringLiteral("opencode")},
                       {QStringLiteral("configPath"), path},
                       {QStringLiteral("found"), found},
                       {QStringLiteral("installed"), installed},
                       {QStringLiteral("enabled"), enabled},
                       {QStringLiteral("canToggle"), true}};
}

QVariantMap McpAgentManager::scanJunie() const
{
    const QString path = QDir::cleanPath(m_home + QStringLiteral("/.junie/mcp/mcp.json"));
    const bool found = QDir(m_home + QStringLiteral("/.junie")).exists()
        || QFile::exists(path);
    bool installed = false;
    if (QFile::exists(path)) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonObject doc = QJsonDocument::fromJson(f.readAll()).object();
            installed = doc.value(QStringLiteral("mcpServers")).toObject().contains(
                QString::fromUtf8(kEntryName));
        }
    }
    return QVariantMap{{QStringLiteral("id"), QStringLiteral("junie")},
                       {QStringLiteral("name"), QStringLiteral("Junie")},
                       {QStringLiteral("configPath"), path},
                       {QStringLiteral("found"), found},
                       {QStringLiteral("installed"), installed},
                       {QStringLiteral("enabled"), installed},
                       {QStringLiteral("canToggle"), false}};
}

bool McpAgentManager::installAgent(const QString &id)
{
    if (id == QStringLiteral("claude"))
        return installClaude();
    if (id == QStringLiteral("codex"))
        return installCodex();
    if (id == QStringLiteral("opencode"))
        return installOpencode();
    if (id == QStringLiteral("junie"))
        return installJunie();
    m_error = QStringLiteral("unknown agent: %1").arg(id);
    return false;
}

bool McpAgentManager::setAgentEnabled(const QString &id, bool enabled)
{
    if (id == QStringLiteral("codex"))
        return setCodexEnabled(enabled);
    if (id == QStringLiteral("opencode"))
        return setOpencodeEnabled(enabled);
    // No enabled flag: disabling removes the entry, enabling installs it.
    if (!enabled) {
        if (id == QStringLiteral("claude"))
            return removeJsonEntry(
                QDir::cleanPath(m_home + QStringLiteral("/.claude.json")),
                QStringLiteral("mcpServers"));
        if (id == QStringLiteral("junie"))
            return removeJsonEntry(
                QDir::cleanPath(m_home + QStringLiteral("/.junie/mcp/mcp.json")),
                QStringLiteral("mcpServers"));
    }
    return installAgent(id);
}

bool McpAgentManager::installClaude()
{
    const QString path = QDir::cleanPath(m_home + QStringLiteral("/.claude.json"));
    QJsonObject doc;
    if (QFile::exists(path) && !readJson(path, doc))
        return false;
    QJsonObject servers = doc.value(QStringLiteral("mcpServers")).toObject();
    servers.insert(QString::fromUtf8(kEntryName),
                   QJsonObject{{QStringLiteral("type"), QStringLiteral("stdio")},
                               {QStringLiteral("command"), agentBinary()},
                               {QStringLiteral("args"), QJsonArray{QStringLiteral("--mcp")}},
                               {QStringLiteral("env"), QJsonObject()}});
    doc.insert(QStringLiteral("mcpServers"), servers);
    return writeJson(path, doc);
}

bool McpAgentManager::installJunie()
{
    const QString path = QDir::cleanPath(m_home + QStringLiteral("/.junie/mcp/mcp.json"));
    QJsonObject doc;
    if (QFile::exists(path) && !readJson(path, doc))
        return false;
    QJsonObject servers = doc.value(QStringLiteral("mcpServers")).toObject();
    servers.insert(QString::fromUtf8(kEntryName),
                   QJsonObject{{QStringLiteral("command"), agentBinary()},
                               {QStringLiteral("args"), QJsonArray{QStringLiteral("--mcp")}},
                               {QStringLiteral("env"), QJsonObject()}});
    doc.insert(QStringLiteral("mcpServers"), servers);
    return writeJson(path, doc);
}

bool McpAgentManager::installOpencode()
{
    const QString path =
        QDir::cleanPath(m_home + QStringLiteral("/.config/opencode/opencode.json"));
    QJsonObject doc;
    if (QFile::exists(path) && !readJson(path, doc))
        return false;
    QJsonObject mcp = doc.value(QStringLiteral("mcp")).toObject();
    QJsonObject trun = mcp.value(QString::fromUtf8(kEntryName)).toObject();
    trun.insert(QStringLiteral("type"), QStringLiteral("local"));
    trun.insert(QStringLiteral("command"),
                QJsonArray{agentBinary(), QStringLiteral("--mcp")});
    if (!trun.contains(QStringLiteral("enabled")))
        trun.insert(QStringLiteral("enabled"), true);
    mcp.insert(QString::fromUtf8(kEntryName), trun);
    doc.insert(QStringLiteral("mcp"), mcp);
    return writeJson(path, doc);
}

bool McpAgentManager::removeJsonEntry(const QString &path, const QString &topKey)
{
    QJsonObject doc;
    if (!readJson(path, doc))
        return false;
    QJsonObject top = doc.value(topKey).toObject();
    if (!top.contains(QString::fromUtf8(kEntryName)))
        return true; // already gone
    top.remove(QString::fromUtf8(kEntryName));
    doc.insert(topKey, top);
    return writeJson(path, doc);
}

bool McpAgentManager::setOpencodeEnabled(bool enabled)
{
    const QString path =
        QDir::cleanPath(m_home + QStringLiteral("/.config/opencode/opencode.json"));
    QJsonObject doc;
    if (!readJson(path, doc))
        return false;
    QJsonObject mcp = doc.value(QStringLiteral("mcp")).toObject();
    QJsonObject trun = mcp.value(QString::fromUtf8(kEntryName)).toObject();
    if (trun.isEmpty()) {
        if (!enabled)
            return true; // nothing to disable
        return installOpencode();
    }
    trun.insert(QStringLiteral("enabled"), enabled);
    mcp.insert(QString::fromUtf8(kEntryName), trun);
    doc.insert(QStringLiteral("mcp"), mcp);
    return writeJson(path, doc);
}

QString McpAgentManager::tomlQuote(const QString &s)
{
    QString out = s;
    out.replace(u'\\', QStringLiteral("\\\\"));
    out.replace(u'"', QStringLiteral("\\\""));
    return u'"' + out + u'"';
}

bool McpAgentManager::installCodex()
{
    const QString path = QDir::cleanPath(m_home + QStringLiteral("/.codex/config.toml"));
    QString content;
    if (QFile::exists(path)) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            m_error = QStringLiteral("cannot read: %1").arg(path);
            return false;
        }
        content = QString::fromUtf8(f.readAll());
        if (content.contains(QStringLiteral("[mcp_servers.trun")))
            return true; // already installed
        if (!backupFile(path))
            return false;
    }
    if (!content.isEmpty() && !content.endsWith(u'\n'))
        content += u'\n';
    content += QStringLiteral("\n[mcp_servers.trun]\ncommand = %1\nargs = [\"--mcp\"]\n")
                   .arg(tomlQuote(agentBinary()));
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_error = QStringLiteral("cannot write: %1").arg(path);
        return false;
    }
    f.write(content.toUtf8());
    return true;
}

bool McpAgentManager::setCodexEnabled(bool enabled)
{
    const QString path = QDir::cleanPath(m_home + QStringLiteral("/.codex/config.toml"));
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (!enabled)
            return true; // nothing to disable
        return installCodex();
    }
    const QStringList lines = QString::fromUtf8(f.readAll()).split(u'\n');
    f.close();

    QStringList out;
    bool inBlock = false;
    bool blockSeen = false;
    bool enabledWritten = false;
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(u'[')) {
            if (inBlock && !enabled && !enabledWritten) {
                out << QStringLiteral("enabled = false");
                enabledWritten = true;
            }
            inBlock = (trimmed == QStringLiteral("[mcp_servers.trun]")
                       || trimmed == QStringLiteral("[mcp_servers.\"trun\"]"));
            blockSeen = blockSeen || inBlock;
            out << line;
            continue;
        }
        if (inBlock && trimmed.startsWith(QStringLiteral("enabled"))) {
            if (enabled)
                continue; // drop the line: default is enabled
            out << QStringLiteral("enabled = false");
            enabledWritten = true;
            continue;
        }
        out << line;
    }
    if (inBlock && !enabled && !enabledWritten)
        out << QStringLiteral("enabled = false");
    if (!blockSeen) {
        if (!enabled)
            return true; // nothing to disable
        return installCodex();
    }
    if (!backupFile(path))
        return false;
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_error = QStringLiteral("cannot write: %1").arg(path);
        return false;
    }
    f.write(out.join(u'\n').toUtf8());
    return true;
}
