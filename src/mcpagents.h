#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

// Reads/writes MCP entries ("trun" record) in AI agents' configs:
// Claude Code (~/.claude.json), Codex (~/.codex/config.toml),
// opencode (~/.config/opencode/opencode.json), Junie (~/.junie/mcp/mcp.json).
// Every write backs the file up first; unparseable JSON is left untouched.
class McpAgentManager : public QObject {
    Q_OBJECT

public:
    explicit McpAgentManager(QObject *parent = nullptr);

    // [{id, name, configPath, found, installed, enabled, canToggle}]
    Q_INVOKABLE QVariantList scanAgents();
    Q_INVOKABLE bool installAgent(const QString &id);
    // For agents without an enabled flag, false removes the entry.
    Q_INVOKABLE bool setAgentEnabled(const QString &id, bool enabled);
    Q_INVOKABLE QString lastError() const { return m_error; }

    void setHomeForTesting(const QString &home) { m_home = home; }

private:
    struct Agent {
        QString id;
        QString name;
        QString relPath; // under home
        bool canToggle = false;
    };

    QString agentBinary() const;
    QString configPath(const Agent &a) const;
    bool backupFile(const QString &path);
    bool readJson(const QString &path, QJsonObject &out);
    bool writeJson(const QString &path, const QJsonObject &doc);

    QVariantMap scanClaude() const;
    QVariantMap scanCodex() const;
    QVariantMap scanOpencode() const;
    QVariantMap scanJunie() const;

    bool installClaude();
    bool installCodex();
    bool installOpencode();
    bool installJunie();
    bool removeJsonEntry(const QString &path, const QString &topKey);
    bool setCodexEnabled(bool enabled);
    bool setOpencodeEnabled(bool enabled);

    static QString tomlQuote(const QString &s);

    QString m_home;
    QString m_error;
};
