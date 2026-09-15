#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QList>
#include <QTextStream>

class ProjectService;
class CommandExecutor;
class Settings;

// MCP server over stdio (newline-delimited JSON-RPC 2.0).
// Exposes trun projects, commands, run controls and logs to AI agents.
// `handleMessage` is pure dispatch so it can be unit-tested without stdio.
class McpServer : public QObject {
    Q_OBJECT

public:
    explicit McpServer(ProjectService *projects, CommandExecutor *executor,
                       Settings *settings, QObject *parent = nullptr);

    // One JSON-RPC request object in, zero or one response object out
    // (null object = notification, no reply).
    QJsonObject handleMessage(const QJsonObject &msg);

    // Starts the stdio transport (reads stdin, writes stdout).
    void startTransport();

    QString lastError() const { return m_error; }

private:
    struct LogLine {
        qint64 timestampMs = 0;
        QString level;
        QString target;
        QString message;
    };

    QJsonObject toolResult(const QJsonValue &id, const QString &text, bool isError = false);
    QJsonObject protocolError(const QJsonValue &id, int code, const QString &message);
    QJsonArray toolDefinitions() const;
    QJsonObject callTool(const QJsonValue &id, const QString &name, const QJsonObject &args);

    QJsonObject findProject(const QString &ref) const;
    QJsonObject findCommand(const QJsonObject &project, const QString &commandRef) const;
    // Bare ids ("npm:serve") predate project scoping: exact full-key match
    // wins, one "|bare" suffix match resolves, several are ambiguous.
    QString resolveCommandKey(const QString &ref, const QStringList &known,
                              bool &ambiguous) const;
    void appendLog(const QString &commandId, const QString &level,
                   const QString &target, const QString &message);

    static QJsonObject monologMatch(const QString &line, const QString &date,
                                    const QString &level, const QString &query);

    void onStdinActivated();

    ProjectService *m_projects;
    CommandExecutor *m_executor;
    Settings *m_settings;
    QString m_error;
    QMap<QString, QList<LogLine>> m_logs;
    QMap<QString, int> m_runtimePorts; // run key -> port sniffed from output URLs
    static constexpr int kMaxLogLines = 2000;
};
