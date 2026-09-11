#include "mcpserver.h"
#include "projectservice.h"
#include "commandexecutor.h"
#include "settings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSocketNotifier>
#include <QTextStream>
#include <unistd.h>

namespace {
constexpr const char *kProtocolVersion = "2024-11-05";
const QStringList kLogDirs = {QStringLiteral("var/log"), QStringLiteral("storage/logs"),
                              QStringLiteral("logs"), QStringLiteral("log")};
} // namespace

McpServer::McpServer(ProjectService *projects, CommandExecutor *executor,
                     Settings *settings, QObject *parent)
    : QObject(parent), m_projects(projects), m_executor(executor), m_settings(settings)
{
    connect(m_executor, &CommandExecutor::started, this,
            [this](int, int pid, const QString &label, const QString &cmdline,
                   const QString &commandId) {
                appendLog(commandId, QStringLiteral("system"), label,
                          QStringLiteral("started: %1 (PID %2)").arg(cmdline).arg(pid));
            });
    connect(m_executor, &CommandExecutor::outputReceived, this,
            [this](int, const QString &label, bool isError, const QString &line,
                   const QString &commandId) {
                appendLog(commandId, isError ? QStringLiteral("stderr") : QStringLiteral("stdout"),
                          label, line);
                static const QRegularExpression urlRe(
                    QStringLiteral("https?://(?:localhost|127\\.0\\.0\\.1|\\[::1\\]):(\\d+)"),
                    QRegularExpression::CaseInsensitiveOption);
                const auto urlMatch = urlRe.match(line);
                if (urlMatch.hasMatch())
                    m_runtimePorts.insert(commandId, urlMatch.captured(1).toInt());
                if (line.contains(QStringLiteral("EADDRINUSE"))
                    || line.contains(QStringLiteral("Address already in use"),
                                     Qt::CaseInsensitive))
                    appendLog(commandId, QStringLiteral("system"), label,
                              QStringLiteral("Port looks busy — set the command's Port "
                                             "in trun Run configuration"));
                if (line.contains(QStringLiteral("already running"), Qt::CaseInsensitive))
                    appendLog(commandId, QStringLiteral("system"), label,
                              QStringLiteral("Something is already running outside trun — "
                                             "stop it there, or set Port in Run configuration"));
            });
    connect(m_executor, &CommandExecutor::finished, this,
            [this](int, const QString &label, int exitCode, const QString &commandId) {
                appendLog(commandId, QStringLiteral("system"), label,
                          exitCode < 0 ? QStringLiteral("stopped")
                                       : QStringLiteral("done, exit code %1").arg(exitCode));
            });
    connect(m_executor, &CommandExecutor::failed, this,
            [this](int, const QString &label, const QString &error, const QString &commandId) {
                appendLog(commandId, QStringLiteral("error"), label,
                          QStringLiteral("failed: %1").arg(error));
            });
}

void McpServer::startTransport()
{
    auto *notifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &McpServer::onStdinActivated);
}

void McpServer::onStdinActivated()
{
    static QTextStream in(stdin, QIODevice::ReadOnly);
    static QTextStream out(stdout, QIODevice::WriteOnly);
    QString line;
    while (in.readLineInto(&line)) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        const QJsonObject msg = QJsonDocument::fromJson(line.toUtf8()).object();
        if (msg.isEmpty()) {
            m_error = QStringLiteral("invalid JSON-RPC message");
            continue;
        }
        const QJsonObject reply = handleMessage(msg);
        if (!reply.isEmpty()) {
            out << QString::fromUtf8(QJsonDocument(reply).toJson(QJsonDocument::Compact)) << '\n';
            out.flush();
        }
    }
    if (in.atEnd())
        QCoreApplication::quit(); // pipe closed: no more requests coming
}

QJsonObject McpServer::handleMessage(const QJsonObject &msg)
{
    if (msg.value(QStringLiteral("jsonrpc")).toString() != QStringLiteral("2.0"))
        return protocolError(msg.value(QStringLiteral("id")), -32600,
                             QStringLiteral("only JSON-RPC 2.0 is supported"));

    const QJsonValue id = msg.value(QStringLiteral("id"));
    const QString method = msg.value(QStringLiteral("method")).toString();
    const QJsonObject params = msg.value(QStringLiteral("params")).toObject();

    if (method == QStringLiteral("initialize")) {
        QJsonObject result{
            {QStringLiteral("protocolVersion"), kProtocolVersion},
            {QStringLiteral("capabilities"),
             QJsonObject{{QStringLiteral("tools"), QJsonObject()}}},
            {QStringLiteral("serverInfo"),
             QJsonObject{{QStringLiteral("name"), QStringLiteral("trun")},
                         {QStringLiteral("version"),
                          QCoreApplication::applicationVersion()}}},
        };
        return QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                           {QStringLiteral("id"), id},
                           {QStringLiteral("result"), result}};
    }
    if (method == QStringLiteral("ping"))
        return QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                           {QStringLiteral("id"), id},
                           {QStringLiteral("result"), QJsonObject()}};
    if (method == QStringLiteral("tools/list")) {
        return QJsonObject{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), id},
            {QStringLiteral("result"),
             QJsonObject{{QStringLiteral("tools"), toolDefinitions()}}}};
    }
    if (method == QStringLiteral("tools/call")) {
        const QString name = params.value(QStringLiteral("name")).toString();
        return callTool(id, name, params.value(QStringLiteral("arguments")).toObject());
    }
    if (method.startsWith(QStringLiteral("notifications/")))
        return {}; // no reply
    return protocolError(id, -32601, QStringLiteral("unknown method: %1").arg(method));
}

QJsonObject McpServer::toolResult(const QJsonValue &id, const QString &text, bool isError)
{
    QJsonObject content{{QStringLiteral("type"), QStringLiteral("text")},
                        {QStringLiteral("text"), text}};
    return QJsonObject{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), id},
        {QStringLiteral("result"),
         QJsonObject{{QStringLiteral("content"), QJsonArray{content}},
                     {QStringLiteral("isError"), isError}}}};
}

QJsonObject McpServer::protocolError(const QJsonValue &id, int code, const QString &message)
{
    if (id.isUndefined())
        return {};
    return QJsonObject{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), id},
        {QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), code},
                                              {QStringLiteral("message"), message}}}};
}

QJsonObject McpServer::callTool(const QJsonValue &id, const QString &name,
                                const QJsonObject &args)
{
    const auto jsonText = [](const QJsonArray &arr) {
        return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    };

    if (name == QStringLiteral("list_projects")) {
        QJsonArray out;
        for (const QJsonObject &p : m_projects->projects()) {
            out << QJsonObject{
                {QStringLiteral("id"), p.value(QStringLiteral("id")).toString()},
                {QStringLiteral("name"), p.value(QStringLiteral("name")).toString()},
                {QStringLiteral("path"), p.value(QStringLiteral("project_path")).toString()},
                {QStringLiteral("manifest"), p.value(QStringLiteral("manifest")).toString()}};
        }
        return toolResult(id, jsonText(out));
    }

    if (name == QStringLiteral("list_commands")) {
        const QJsonObject project = findProject(args.value(QStringLiteral("project")).toString());
        if (project.isEmpty())
            return toolResult(id, QStringLiteral("unknown project"), true);
        QJsonArray out;
        for (const QJsonValue &v : project.value(QStringLiteral("commands")).toArray()) {
            const QJsonObject c = v.toObject();
            out << QJsonObject{
                {QStringLiteral("id"), c.value(QStringLiteral("id")).toString()},
                {QStringLiteral("label"), c.value(QStringLiteral("label")).toString()},
                {QStringLiteral("command"), c.value(QStringLiteral("command")).toString()},
                {QStringLiteral("args"),
                 QJsonArray::fromStringList(
                     [&] {
                         QStringList l;
                         for (const QJsonValue &a : c.value(QStringLiteral("args")).toArray())
                             l << a.toString();
                         return l;
                     }())}};
        }
        return toolResult(id, jsonText(out));
    }

    if (name == QStringLiteral("status")) {
        const QString filter = args.value(QStringLiteral("commandId")).toString();
        bool ambiguous = false;
        const QString key =
            filter.isEmpty() ? QString() : resolveCommandKey(filter, m_executor->runningCommandIds(), ambiguous);
        if (ambiguous)
            return toolResult(id, QStringLiteral("ambiguous command id, use projectId|commandId"),
                              true);
        QJsonArray out;
        for (const QString &cmdId : m_executor->runningCommandIds()) {
            if (!key.isEmpty() && cmdId != key)
                continue;
            out << QJsonObject{
                {QStringLiteral("commandId"), cmdId},
                {QStringLiteral("pid"), m_executor->pidForCommand(cmdId)},
                {QStringLiteral("memoryKb"), m_executor->memoryForCommand(cmdId)},
                {QStringLiteral("cpuPercent"), m_executor->cpuForCommand(cmdId)},
                {QStringLiteral("elapsedSec"), m_executor->elapsedForCommand(cmdId)},
                {QStringLiteral("detectedPort"), m_runtimePorts.value(cmdId, 0)}};
        }
        return toolResult(id, jsonText(out));
    }

    if (name == QStringLiteral("run")) {
        const QJsonObject project = findProject(args.value(QStringLiteral("project")).toString());
        if (project.isEmpty())
            return toolResult(id, QStringLiteral("unknown project"), true);
        const QString commandRef = args.value(QStringLiteral("command")).toString();
        const QJsonObject cmd = findCommand(project, commandRef);
        if (cmd.isEmpty())
            return toolResult(id, QStringLiteral("unknown command"), true);
        const QString projectId = project.value(QStringLiteral("id")).toString();
        const QString commandId = cmd.value(QStringLiteral("id")).toString();

        QString executable = cmd.value(QStringLiteral("command")).toString();
        QStringList cmdArgs;
        for (const QJsonValue &a : cmd.value(QStringLiteral("args")).toArray())
            cmdArgs << a.toString();
        QString workdir = project.value(QStringLiteral("project_path")).toString();
        QString label = cmd.value(QStringLiteral("label")).toString();
        QStringList env;
        bool allowMultiple = false;
        m_projects->resolveRunConfig(projectId, commandId, executable, cmdArgs, workdir, env,
                                     allowMultiple);

        if (args.contains(QStringLiteral("args"))) {
            cmdArgs.clear();
            for (const QJsonValue &a : args.value(QStringLiteral("args")).toArray())
                cmdArgs << a.toString();
        }
        if (args.contains(QStringLiteral("workdir")))
            workdir = args.value(QStringLiteral("workdir")).toString();
        const QJsonObject envObj = args.value(QStringLiteral("env")).toObject();
        for (auto it = envObj.begin(); it != envObj.end(); ++it)
            env << it.key() + u'=' + it.value().toString();

        const int runId = m_projects->runResolved(
            projectId + u'|' + commandId, executable, cmdArgs, workdir,
            label.isEmpty() ? executable : label, env, allowMultiple);
        if (runId <= 0)
            return toolResult(id, QStringLiteral("failed to start"), true);
        return toolResult(id, QStringLiteral("started %1 (run #%2)")
                                    .arg(commandId)
                                    .arg(runId));
    }

    if (name == QStringLiteral("stop")) {
        const QString ref = args.value(QStringLiteral("commandId")).toString();
        bool ambiguous = false;
        const QString key =
            resolveCommandKey(ref, m_executor->runningCommandIds(), ambiguous);
        if (ambiguous)
            return toolResult(id, QStringLiteral("ambiguous command id, use projectId|commandId"),
                              true);
        m_executor->stopCommand(key);
        return toolResult(id, QStringLiteral("stopped %1").arg(ref));
    }

    if (name == QStringLiteral("read_log")) {
        const QString ref = args.value(QStringLiteral("commandId")).toString();
        bool ambiguous = false;
        const QString commandId = resolveCommandKey(ref, m_logs.keys(), ambiguous);
        if (ambiguous)
            return toolResult(id, QStringLiteral("ambiguous command id, use projectId|commandId"),
                              true);
        int tail = args.value(QStringLiteral("tail")).toInt(100);
        tail = qBound(1, tail, kMaxLogLines);
        const QList<LogLine> buf = m_logs.value(commandId);
        QStringList lines;
        for (int i = qMax(0, buf.size() - tail); i < buf.size(); ++i) {
            const LogLine &l = buf.at(i);
            lines << QStringLiteral("[%1] %2: %3")
                         .arg(QDateTime::fromMSecsSinceEpoch(l.timestampMs).toString(Qt::ISODate),
                              l.target, l.message);
        }
        return toolResult(id, lines.join(u'\n'));
    }

    if (name == QStringLiteral("search_logs")) {
        const QJsonObject project = findProject(args.value(QStringLiteral("project")).toString());
        if (project.isEmpty())
            return toolResult(id, QStringLiteral("unknown project"), true);
        const QString query = args.value(QStringLiteral("query")).toString();
        const QString level = args.value(QStringLiteral("level")).toString();
        const QString date = args.value(QStringLiteral("date")).toString();
        int maxResults = args.value(QStringLiteral("maxResults")).toInt(100);
        maxResults = qBound(1, maxResults, 500);
        const QString base = project.value(QStringLiteral("project_path")).toString();

        QStringList hits;
        for (const QString &sub : kLogDirs) {
            const QDir dir(base + u'/' + sub);
            if (!dir.exists())
                continue;
            const QFileInfoList files =
                dir.entryInfoList(QStringList{QStringLiteral("*.log")}, QDir::Files,
                                  QDir::Name);
            for (const QFileInfo &fi : files) {
                QFile f(fi.absoluteFilePath());
                if (!f.open(QIODevice::ReadOnly))
                    continue;
                QByteArray data = f.readAll();
                if (data.size() > 10 * 1024 * 1024)
                    data = data.right(5 * 1024 * 1024); // tail of huge logs
                int lineNo = 0;
                for (const QByteArray &raw : data.split('\n')) {
                    ++lineNo;
                    const QString line = QString::fromUtf8(raw).trimmed();
                    if (line.isEmpty())
                        continue;
                    if (!monologMatch(line, date, level, query).isEmpty()) {
                        hits << QStringLiteral("%1:%2: %3")
                                      .arg(dir.dirName() + u'/' + fi.fileName())
                                      .arg(lineNo)
                                      .arg(line.left(500));
                        if (hits.size() >= maxResults)
                            return toolResult(id, hits.join(u'\n'));
                    }
                }
            }
        }
        return toolResult(id, hits.join(u'\n'));
    }

    return toolResult(id, QStringLiteral("unknown tool: %1").arg(name), true);
}

QJsonArray McpServer::toolDefinitions() const
{
    auto tool = [](const char *name, const char *desc, const QJsonObject &props,
                   const QStringList &required = {}) {
        return QJsonObject{
            {QStringLiteral("name"), QString::fromUtf8(name)},
            {QStringLiteral("description"), QString::fromUtf8(desc)},
            {QStringLiteral("inputSchema"),
             QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                         {QStringLiteral("properties"), props},
                         {QStringLiteral("required"),
                          QJsonArray::fromStringList(required)}}}};
    };
    auto str = [](const char *desc) {
        return QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                           {QStringLiteral("description"), QString::fromUtf8(desc)}};
    };
    return QJsonArray{
        tool("list_projects", "List scanned projects", {}),
        tool("list_commands", "List runnable commands of a project",
             {{"project", str("Project id or path")}}, {"project"}),
        tool("status", "Running runs with pid, memory, cpu and uptime",
             {{"commandId", str("Optional filter: bare id or projectId|commandId")}}),
        tool("run", "Start a command using its stored run configuration",
             {{"project", str("Project id or path")},
              {"command", str("Command id, e.g. npm:dev")},
              {"args", QJsonObject{{"type", "array"},
                                   {"items", QJsonObject{{"type", "string"}}},
                                   {"description", "Override arguments"}}},
              {"env", QJsonObject{{"type", "object"},
                                  {"description", "Extra KEY: VALUE pairs"}}},
              {"workdir", str("Override working directory")}},
             {"project", "command"}),
        tool("stop", "Stop all runs of a command",
             {{"commandId", str("Bare id or projectId|commandId")}}, {"commandId"}),
        tool("read_log", "Last console lines of a command",
             {{"commandId", str("Bare id or projectId|commandId")},
              {"tail",
               QJsonObject{{"type", "integer"}, {"description", "Line count (default 100)"}}}},
             {"commandId"}),
        tool("search_logs",
             "Search project log files (monolog-aware: date, level) with plain-text fallback",
             {{"project", str("Project id or path")},
              {"query", str("Substring to match")},
              {"level", str("Log level, e.g. ERROR")},
              {"date", str("Date prefix, e.g. 2026-09-10")},
              {"maxResults",
               QJsonObject{{"type", "integer"}, {"description", "Cap (default 100)"}}}},
             {"project"}),
    };
}

QJsonObject McpServer::findProject(const QString &ref) const
{
    for (const QJsonObject &p : m_projects->projects()) {
        if (p.value(QStringLiteral("id")).toString() == ref
            || p.value(QStringLiteral("project_path")).toString() == ref)
            return p;
    }
    return {};
}

QJsonObject McpServer::findCommand(const QJsonObject &project, const QString &commandRef) const
{
    const QJsonArray cmds = project.value(QStringLiteral("commands")).toArray();
    for (const QJsonValue &v : cmds) {
        const QJsonObject c = v.toObject();
        if (c.value(QStringLiteral("id")).toString() == commandRef
            || c.value(QStringLiteral("label")).toString() == commandRef)
            return c;
    }
    return {};
}

QString McpServer::resolveCommandKey(const QString &ref, const QStringList &known,
                                     bool &ambiguous) const
{
    ambiguous = false;
    if (known.contains(ref))
        return ref;
    const QString suffix = u'|' + ref;
    QStringList hits;
    for (const QString &k : known) {
        if (k.endsWith(suffix))
            hits << k;
    }
    if (hits.size() > 1)
        ambiguous = true;
    return hits.size() == 1 ? hits.first() : ref;
}

void McpServer::appendLog(const QString &commandId, const QString &level,
                          const QString &target, const QString &message)
{
    if (commandId.isEmpty())
        return;
    QList<LogLine> &buf = m_logs[commandId];
    buf << LogLine{QDateTime::currentMSecsSinceEpoch(), level, target, message};
    while (buf.size() > kMaxLogLines)
        buf.removeFirst();
}

QJsonObject McpServer::monologMatch(const QString &line, const QString &date,
                                    const QString &level, const QString &query)
{
    // Monolog: "[2026-09-10T12:00:00+00:00] channel.LEVEL: message"
    QString lineLevel;
    QString lineDate;
    if (line.startsWith(u'[')) {
        const int end = line.indexOf(u']');
        if (end > 0) {
            lineDate = line.mid(1, end - 1);
            const int dot = line.indexOf(u'.', end);
            const int colon = line.indexOf(u':', end);
            if (dot > 0 && colon > dot)
                lineLevel = line.mid(dot + 1, colon - dot - 1);
        }
    }
    if (!date.isEmpty() && !lineDate.startsWith(date))
        return {};
    if (!level.isEmpty()) {
        if (!lineLevel.isEmpty()) {
            // Monolog line: exact level match only ("userErrors" must not
            // match level=ERROR)
            if (lineLevel.compare(level, Qt::CaseInsensitive) != 0)
                return {};
        } else if (!line.contains(level, Qt::CaseInsensitive)) {
            // Plain line: substring fallback
            return {};
        }
    }
    if (!query.isEmpty() && !line.contains(query, Qt::CaseInsensitive))
        return {};
    return QJsonObject{{QStringLiteral("text"), line}};
}
