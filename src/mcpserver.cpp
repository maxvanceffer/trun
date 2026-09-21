#include "mcpserver.h"
#include "projectservice.h"
#include "commandexecutor.h"
#include "dockerservice.h"
#include "settings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLocale>
#include <QRegularExpression>
#include <QSocketNotifier>
#include <QTextStream>
#include <unistd.h>

namespace {
constexpr const char *kProtocolVersion = "2024-11-05";
const QStringList kLogDirs = {QStringLiteral("var/log"), QStringLiteral("storage/logs"),
                              QStringLiteral("logs"), QStringLiteral("log")};

QJsonValue variantToJson(const QVariant &v)
{
    switch (v.typeId()) {
    case QMetaType::QVariantMap: {
        QJsonObject o;
        const QVariantMap m = v.toMap();
        for (auto it = m.begin(); it != m.end(); ++it)
            o.insert(it.key(), variantToJson(it.value()));
        return o;
    }
    case QMetaType::QVariantList:
    case QMetaType::QStringList: {
        QJsonArray a;
        for (const QVariant &e : v.toList())
            a << variantToJson(e);
        return a;
    }
    case QMetaType::Bool:
        return QJsonValue(v.toBool());
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Double:
        return QJsonValue(v.toDouble());
    default:
        return QJsonValue(v.toString());
    }
}
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
                static const QRegularExpression listenRe(
                    QStringLiteral("listening on\\s+(?:https?://)?(?:localhost|127\\.0\\.0\\.1|"
                                   "0\\.0\\.0\\.0|\\[::1\\]):(\\d+)"),
                    QRegularExpression::CaseInsensitiveOption);
                static const QRegularExpression listenPortRe(
                    QStringLiteral("listening on\\s+[a-z ]*port\\s+(\\d+)"),
                    QRegularExpression::CaseInsensitiveOption);
                int port = 0;
                if (const auto urlMatch = urlRe.match(line); urlMatch.hasMatch())
                    port = urlMatch.captured(1).toInt();
                else if (const auto listenMatch = listenRe.match(line); listenMatch.hasMatch())
                    port = listenMatch.captured(1).toInt();
                else if (const auto portMatch = listenPortRe.match(line); portMatch.hasMatch())
                    port = portMatch.captured(1).toInt();
                if (port > 0)
                    m_runtimePorts.insert(commandId, port);
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
    const auto jsonValueText = [](const QJsonValue &v) {
        if (v.isArray())
            return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
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
        const QString query = args.value(QStringLiteral("query")).toString();
        const QString level = args.value(QStringLiteral("level")).toString();
        const QString channel = args.value(QStringLiteral("channel")).toString();
        const int sinceMin = args.value(QStringLiteral("sinceMinutes")).toInt(-1);
        const QList<LogLine> buf = m_logs.value(commandId);
        if (buf.isEmpty())
            return toolResult(id, {});
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const LogFilterQuery filter = {query, level.isEmpty() ? QStringList() : QStringList{level},
                                       channel.isEmpty() ? QStringList() : QStringList{channel}};
        QSet<QString> levels;
        QSet<QString> channels;
        QStringList matched;
        for (const LogLine &l : buf) {
            const ParsedLogLine p = LogParser::parse(l.message);
            observe(p, levels, channels);
            if (sinceMin >= 0 && nowMs - l.timestampMs > qint64(sinceMin) * 60000)
                continue;
            if (LogParser::applyFilter(p, l.message, filter) != LogFilterHit::Shown)
                continue;
            matched << QStringLiteral("[%1] %2: %3")
                           .arg(QDateTime::fromMSecsSinceEpoch(l.timestampMs).toString(Qt::ISODate),
                                l.target, l.message);
        }
        const bool truncated = matched.size() > tail;
        const int matchedTotal = matched.size();
        while (matched.size() > tail)
            matched.removeFirst();
        QStringList out{resultHeader(matchedTotal, buf.size(), sorted(levels),
                                     sorted(channels), truncated)};
        out.append(matched);
        return toolResult(id, out.join(u'\n'));
    }

    if (name == QStringLiteral("search_logs")) {
        const QJsonObject project = findProject(args.value(QStringLiteral("project")).toString());
        if (project.isEmpty())
            return toolResult(id, QStringLiteral("unknown project"), true);
        const QString query = args.value(QStringLiteral("query")).toString();
        const QString level = args.value(QStringLiteral("level")).toString();
        const QString channel = args.value(QStringLiteral("channel")).toString();
        const QString date = args.value(QStringLiteral("date")).toString();
        const int sinceMin = args.value(QStringLiteral("sinceMinutes")).toInt(-1);
        int maxResults = args.value(QStringLiteral("maxResults")).toInt(100);
        maxResults = qBound(1, maxResults, 500);
        const QString base = project.value(QStringLiteral("project_path")).toString();

        const LogFilterQuery filter = {query, level.isEmpty() ? QStringList() : QStringList{level},
                                       channel.isEmpty() ? QStringList() : QStringList{channel}};
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        QSet<QString> levels;
        QSet<QString> channels;
        QStringList hits;
        int scanned = 0;
        bool truncated = false;
        for (const QString &sub : kLogDirs) {
            const QDir dir(base + u'/' + sub);
            if (!dir.exists() || truncated)
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
                    ++scanned;
                    const ParsedLogLine p = LogParser::parse(line);
                    observe(p, levels, channels);
                    if (!date.isEmpty()) {
                        // Legacy prefix: текст первой [...] скобки сырой строки.
                        QString lineDate;
                        if (line.startsWith(u'[')) {
                            const int end = line.indexOf(u']');
                            if (end > 0)
                                lineDate = line.mid(1, end - 1);
                        }
                        if (!lineDate.startsWith(date))
                            continue;
                    }
                    if (sinceMin >= 0) {
                        const QDateTime dt = parsedTimestamp(p);
                        if (!dt.isValid() || dt.msecsTo(QDateTime::fromMSecsSinceEpoch(nowMs))
                                > qint64(sinceMin) * 60000)
                            continue;
                    }
                    if (LogParser::applyFilter(p, line, filter) != LogFilterHit::Shown)
                        continue;
                    hits << QStringLiteral("%1:%2: %3")
                                   .arg(dir.dirName() + u'/' + fi.fileName())
                                   .arg(lineNo)
                                   .arg(line.left(500));
                    if (hits.size() >= maxResults) {
                        truncated = true;
                        break;
                    }
                }
                if (truncated)
                    break;
            }
        }
        QStringList out{resultHeader(hits.size(), scanned, sorted(levels),
                                     sorted(channels), truncated)};
        out.append(hits);
        return toolResult(id, out.join(u'\n'));
    }

    if (name == QStringLiteral("docker_status")) {
        const QVariantMap engine = DockerService::queryEngine();
        QJsonObject out = variantToJson(engine).toObject();
        if (engine.value(QStringLiteral("available")).toBool())
            out.insert(QStringLiteral("disk"),
                       variantToJson(DockerService::queryDisk()));
        return toolResult(id, jsonValueText(out));
    }

    if (name == QStringLiteral("docker_ps")) {
        const QVariantMap engine = DockerService::queryEngine();
        QJsonArray out;
        if (engine.value(QStringLiteral("available")).toBool()) {
            for (const QVariant &v : DockerService::queryContainers())
                out << variantToJson(v);
        }
        return toolResult(id, jsonText(out));
    }

    if (name == QStringLiteral("docker_images")) {
        const QVariantMap engine = DockerService::queryEngine();
        QJsonArray out;
        if (engine.value(QStringLiteral("available")).toBool()) {
            for (const QVariant &v : DockerService::queryImages())
                out << variantToJson(v);
        }
        return toolResult(id, jsonText(out));
    }

    if (name == QStringLiteral("docker_logs")) {
        const QString container = args.value(QStringLiteral("name")).toString();
        if (container.isEmpty())
            return toolResult(id, QStringLiteral("missing container name"), true);
        int tail = args.value(QStringLiteral("tail")).toInt(100);
        tail = qBound(1, tail, 2000);
        bool ok = false;
        const QString logs = DockerService::queryLogs(container, tail, &ok);
        if (!ok)
            return toolResult(id, logs, true);
        return toolResult(id, logs);
    }

    if (name == QStringLiteral("docker_control")) {
        const QString container = args.value(QStringLiteral("name")).toString();
        const QString action = args.value(QStringLiteral("action")).toString();
        if (container.isEmpty() || action.isEmpty())
            return toolResult(id, QStringLiteral("need name and action"), true);
        QStringList dockerArgs;
        QString past;
        if (action == QStringLiteral("start")) {
            dockerArgs = {QStringLiteral("start"), container};
            past = QStringLiteral("started");
        } else if (action == QStringLiteral("stop")) {
            dockerArgs = {QStringLiteral("stop"), container};
            past = QStringLiteral("stopped");
        } else if (action == QStringLiteral("restart")) {
            dockerArgs = {QStringLiteral("restart"), container};
            past = QStringLiteral("restarted");
        } else if (action == QStringLiteral("remove")) {
            dockerArgs = {QStringLiteral("rm"), QStringLiteral("-f"), container};
            past = QStringLiteral("removed");
        } else {
            return toolResult(id, QStringLiteral("unknown action, use start|stop|restart|remove"),
                              true);
        }
        QString output;
        if (!DockerService::runDocker(dockerArgs, &output))
            return toolResult(id, output.isEmpty() ? action + QStringLiteral(" failed") : output,
                              true);
        return toolResult(id, QStringLiteral("%1 %2").arg(past, container));
    }

    if (name == QStringLiteral("docker_stats")) {
        const QVariantMap engine = DockerService::queryEngine();
        QJsonArray out;
        if (engine.value(QStringLiteral("available")).toBool()) {
            const QVariantMap stats = DockerService::queryStats();
            for (auto it = stats.begin(); it != stats.end(); ++it) {
                QJsonObject s = variantToJson(it.value()).toObject();
                s.insert(QStringLiteral("name"), it.key());
                out << s;
            }
        }
        return toolResult(id, jsonText(out));
    }

    if (name == QStringLiteral("docker_prune")) {
        QString output;
        if (!DockerService::runDocker(
                {QStringLiteral("system"), QStringLiteral("prune"), QStringLiteral("-f")},
                &output, 120000))
            return toolResult(id, output.isEmpty() ? QStringLiteral("prune failed") : output,
                              true);
        return toolResult(id, output);
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
        tool("read_log", "Last console lines of a command, with structured filters "
                            "(query/level/channel/sinceMinutes, AND). Reply starts with a header: "
                            "matched/total counts, observed levels/channels, truncated flag",
             {{"commandId", str("Bare id or projectId|commandId")},
              {"tail",
               QJsonObject{{"type", "integer"}, {"description", "Line count (default 100)"}}},
              {"query", str("Substring to match (case-insensitive)")},
              {"level", str("Exact level, e.g. ERROR")},
              {"channel", str("Exact channel, e.g. doctrine")},
              {"sinceMinutes",
               QJsonObject{{"type", "integer"},
                           {"description", "Only lines from the last N minutes"}}}},
             {"commandId"}),
        tool("search_logs",
             "Search project log files with structured filters (query/level/channel/"
             "sinceMinutes, AND; date is a legacy prefix filter). Reply starts with a header: "
             "matched/scanned counts, observed levels/channels, truncated flag. "
             "Unparsed lines match query only",
             {{"project", str("Project id or path")},
              {"query", str("Substring to match")},
              {"level", str("Log level, e.g. ERROR")},
              {"channel", str("Channel, e.g. doctrine")},
              {"date", str("Date prefix, e.g. 2026-09-10")},
              {"sinceMinutes",
               QJsonObject{{"type", "integer"},
                           {"description", "Only lines from the last N minutes"}}},
              {"maxResults",
               QJsonObject{{"type", "integer"}, {"description", "Cap (default 100)"}}}},
             {"project"}),
        tool("docker_status", "Docker engine status with disk usage (colima/Desktop/OrbStack)",
             {}),
        tool("docker_ps", "List containers: name, image, running, status, ports, project", {}),
        tool("docker_images", "List images: repository, tag, id, size", {}),
        tool("docker_logs", "Tail a container's logs",
             {{"name", str("Container name")},
              {"tail",
               QJsonObject{{"type", "integer"}, {"description", "Line count (default 100)"}}}},
             {"name"}),
        tool("docker_control", "Start, stop, restart or force-remove a container",
             {{"name", str("Container name")},
              {"action",
               QJsonObject{{"type", "string"},
                           {"enum", QJsonArray{"start", "stop", "restart", "remove"}},
                           {"description", "Control action"}}}},
             {"name", "action"}),
        tool("docker_stats", "Live CPU/RAM of running containers", {}),
        tool("docker_prune",
             "Remove stopped containers and dangling images (docker system prune -f)", {}),
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
    // SGR colors are preserved for the QML console; MCP consumers get plain
    // text (OSC escapes are already stripped upstream).
    static const QRegularExpression sgr(QStringLiteral("\x1b\\[[0-9;]*m"));
    QString clean = message;
    clean.remove(sgr);
    QList<LogLine> &buf = m_logs[commandId];
    buf << LogLine{QDateTime::currentMSecsSinceEpoch(), level, target, clean};
    while (buf.size() > kMaxLogLines)
        buf.removeFirst();
}

QString McpServer::resultHeader(int matched, int total, const QStringList &levels,
                                const QStringList &channels, bool truncated)
{
    QStringList head{QStringLiteral("# matched %1 of %2, truncated: %3")
                         .arg(matched)
                         .arg(total)
                         .arg(truncated ? QStringLiteral("yes") : QStringLiteral("no"))};
    if (!levels.isEmpty())
        head << QStringLiteral("# levels: ") + levels.join(QStringLiteral(", "));
    if (!channels.isEmpty())
        head << QStringLiteral("# channels: ") + channels.join(QStringLiteral(", "));
    return head.join(u'\n');
}

QDateTime McpServer::parsedTimestamp(const ParsedLogLine &p)
{
    QDateTime dt = QDateTime::fromString(p.layerTimestamp, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(p.layerTimestamp, Qt::ISODate);
    if (!dt.isValid() && !p.layerTimestamp.isEmpty()) {
        // CLF "10/Oct/2000:13:55:36 -0700" (access-логи).
        static const QLocale c(QLocale::C);
        dt = c.toDateTime(p.layerTimestamp, QStringLiteral("dd/MMM/yyyy:HH:mm:ss t"));
    }
    return dt;
}

void McpServer::observe(const ParsedLogLine &p, QSet<QString> &levels, QSet<QString> &channels)
{
    if (!p.ok)
        return;
    if (!p.level.isEmpty())
        levels.insert(p.level);
    if (!p.channel.isEmpty())
        channels.insert(p.channel);
}

QStringList McpServer::sorted(const QSet<QString> &s)
{
    QStringList out(s.begin(), s.end());
    out.sort();
    return out;
}
