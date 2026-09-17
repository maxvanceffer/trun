#include "logparser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {

const QStringList kSeverities = {QStringLiteral("EMERGENCY"), QStringLiteral("ALERT"),
                                 QStringLiteral("CRITICAL"), QStringLiteral("ERROR"),
                                 QStringLiteral("WARNING"), QStringLiteral("NOTICE"),
                                 QStringLiteral("INFO"), QStringLiteral("DEBUG")};

QString severityToLevel(int sev)
{
    if (sev < 0 || sev > 7)
        return {};
    return kSeverities.at(sev);
}

QString levelFromHttpStatus(int status)
{
    if (status >= 500)
        return QStringLiteral("ERROR");
    if (status >= 400)
        return QStringLiteral("WARNING");
    return QStringLiteral("INFO");
}

// JSON-ветка: zap/logrus-JSON/tracing-Json и любой объект с msg+level.
ParsedLogLine tryJson(const QString &line, bool &attempted)
{
    attempted = false;
    if (!line.startsWith(u'{'))
        return {};
    QJsonParseError err = {};
    const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return {};
    attempted = true;
    const QJsonObject o = doc.object();
    const QString msg = o.value(QStringLiteral("msg")).toString().isEmpty()
        ? o.value(QStringLiteral("message")).toString()
        : o.value(QStringLiteral("msg")).toString();
    const QString level = LogParser::normalizeLevel(o.value(QStringLiteral("level")).toString());
    if (msg.isEmpty() || !LogParser::isLevelName(level))
        return {};
    ParsedLogLine out;
    out.ok = true;
    for (const char *k : {"logger", "name", "target", "app"}) {
        const QString v = o.value(QString::fromUtf8(k)).toString();
        if (!v.isEmpty()) {
            out.channel = v;
            break;
        }
    }
    const QJsonValue ts = o.value(QStringLiteral("ts"));
    if (!ts.isUndefined())
        out.layerTimestamp = ts.isDouble() ? QString::number(ts.toDouble(), 'f', 3)
                                           : ts.toString();
    else
        out.layerTimestamp = o.value(QStringLiteral("time")).toString().isEmpty()
            ? o.value(QStringLiteral("timestamp")).toString()
            : o.value(QStringLiteral("time")).toString();
    out.level = level;
    out.message = msg;
    return out;
}

} // namespace

QString LogParser::stripSgr(const QString &s)
{
    static const QRegularExpression sgr(QStringLiteral("\x1b\\[[0-9;]*m"));
    QString out = s;
    out.remove(sgr);
    return out;
}

QString LogParser::normalizeLevel(const QString &level)
{
    const QString u = level.toUpper();
    if (u == QStringLiteral("WARN"))
        return QStringLiteral("WARNING");
    if (u == QStringLiteral("DEBU"))
        return QStringLiteral("DEBUG");
    if (u == QStringLiteral("ERRO"))
        return QStringLiteral("ERROR");
    if (u == QStringLiteral("FATA") || u == QStringLiteral("FATAL") || u == QStringLiteral("PANI"))
        return QStringLiteral("CRITICAL");
    return u;
}

bool LogParser::isLevelName(const QString &level)
{
    const QString u = normalizeLevel(level);
    return u == QStringLiteral("TRACE") || kSeverities.contains(u);
}

LogFilterHit LogParser::applyFilter(const ParsedLogLine &p, const QString &raw,
                                    const LogFilterQuery &f)
{
    if (!f.query.isEmpty() && !raw.contains(f.query, Qt::CaseInsensitive))
        return LogFilterHit::Hidden;
    if (!p.ok)
        return (!f.levels.isEmpty() || !f.channels.isEmpty()) ? LogFilterHit::HiddenUnparsed
                                                              : LogFilterHit::Shown;
    if (!f.levels.isEmpty()) {
        bool any = false;
        for (const QString &lv : f.levels) {
            if (p.level == normalizeLevel(lv)) {
                any = true;
                break;
            }
        }
        if (!any)
            return LogFilterHit::Hidden;
    }
    if (!f.channels.isEmpty()) {
        bool any = false;
        for (const QString &ch : f.channels) {
            if (p.channel.compare(ch, Qt::CaseInsensitive) == 0) {
                any = true;
                break;
            }
        }
        if (!any)
            return LogFilterHit::Hidden;
    }
    return LogFilterHit::Shown;
}

ParsedLogLine LogParser::parse(const QString &raw)
{
    ParsedLogLine out;
    const QString line = stripSgr(raw).trimmed();
    if (line.isEmpty())
        return out;

    // 1. JSON-объект.
    bool jsonAttempted = false;
    ParsedLogLine asJson = tryJson(line, jsonAttempted);
    if (jsonAttempted)
        return asJson; // ok либо честный fallback, дальше не идём

    // 2. Docker-обёртка --timestamps: RFC3339Nano + тело, каналы склеены.
    {
        static const QRegularExpression docker(
            QStringLiteral("^(\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}(?:\\.\\d+)?(?:Z|[+-]\\d{2}:?\\d{2}?))\\s+([\\s\\S]*)$"));
        const auto m = docker.match(line);
        if (m.hasMatch()) {
            ParsedLogLine inner = parse(m.captured(2)); // рекурсия, глубина 1
            if (inner.ok && inner.layerTimestamp.isEmpty())
                inner.layerTimestamp = m.captured(1);
            return inner;
        }
    }

    // 3a. Monolog LineFormatter на всей строке (файловые логи, слоя 1 нет).
    {
        static const QRegularExpression monologA(
            QStringLiteral("^\\[([^\\]]+)\\]\\s*([^.\\[]+)\\.([A-Za-z]+):\\s*([\\s\\S]*)$"));
        const auto m = monologA.match(line);
        if (m.hasMatch() && isLevelName(m.captured(3))) {
            out.ok = true;
            out.layerTimestamp = m.captured(1);
            out.level = normalizeLevel(m.captured(3));
            out.channel = m.captured(2).trimmed();
            out.message = m.captured(4);
            return out;
        }
    }

    // 4. Слой 1 trun: [HH:mm:ss.zzz] label: body.
    // target без :[] — иначе apache-error "[..] [core:error] .." режется неверно.
    QString target;
    QString body = line;
    {
        static const QRegularExpression layer1(
            QStringLiteral("^\\[([^\\]]+)\\]\\s*([^\\]:\\[]+):\\s*([\\s\\S]*)$"));
        const auto m = layer1.match(line);
        if (m.hasMatch()) {
            out.layerTimestamp = m.captured(1);
            target = m.captured(2).trimmed();
            body = m.captured(3);
        }
    }
    out.target = target;

    // 3b. Symfony ConsoleFormatter тела.
    {
        static const QRegularExpression consoleB(
            QStringLiteral("^(\\d{2}:\\d{2}:\\d{2})\\s+([A-Za-z]+)\\s+\\[([^\\]]+)\\]\\s*([\\s\\S]*)$"));
        const auto m = consoleB.match(body);
        if (m.hasMatch() && isLevelName(m.captured(2))) {
            out.ok = true;
            out.level = normalizeLevel(m.captured(2));
            out.channel = m.captured(3);
            out.message = m.captured(4);
            return out;
        }
    }

    // 3c. Кастом [channel] Mon DD HH:MM:SS |LEVEL| [SUB] msg.
    {
        static const QRegularExpression customC(
            QStringLiteral("^\\[([^\\]]+)\\]\\s*[A-Z][a-z]{2}\\s+\\d{1,2}\\s+\\d{2}:\\d{2}:\\d{2}\\s*\\|([A-Za-z]+)\\s*\\|\\s*(?:([A-Za-z0-9_]+)\\s+)?([\\s\\S]*)$"));
        const auto m = customC.match(body);
        if (m.hasMatch() && isLevelName(m.captured(2))) {
            out.ok = true;
            out.level = normalizeLevel(m.captured(2));
            out.channel = m.captured(3).isEmpty() ? m.captured(1).trimmed() : m.captured(3);
            out.message = m.captured(4);
            return out;
        }
    }

    // Uvicorn default/access: "INFO:     ...".
    {
        static const QRegularExpression uvicorn(QStringLiteral("^([A-Za-z]+):\\s+([\\s\\S]*)$"));
        const auto m = uvicorn.match(body);
        if (m.hasMatch() && isLevelName(m.captured(1))) {
            out.ok = true;
            out.level = normalizeLevel(m.captured(1));
            out.message = body;
            return out;
        }
    }

    // Gunicorn error: "[date] [pid] [LEVEL] msg".
    {
        static const QRegularExpression gunicorn(
            QStringLiteral("^\\[([^\\]]+)\\]\\s*\\[(\\d+)\\]\\s*\\[([A-Za-z]+)\\]\\s*([\\s\\S]*)$"));
        const auto m = gunicorn.match(body);
        if (m.hasMatch() && isLevelName(m.captured(3))) {
            out.ok = true;
            out.level = normalizeLevel(m.captured(3));
            out.message = body;
            return out;
        }
    }

    // logrus logfmt без TTY: time=".." level=.. msg="..".
    {
        static const QRegularExpression lTime(QStringLiteral("(?:^|\\s)time=\"([^\"]+)\""));
        static const QRegularExpression lLevel(QStringLiteral("(?:^|\\s)level=([A-Za-z]+)"));
        static const QRegularExpression lMsg(QStringLiteral("(?:^|\\s)msg=\"((?:[^\"\\\\]|\\\\.)*)\""));
        const auto t = lTime.match(body);
        const auto l = lLevel.match(body);
        const auto g = lMsg.match(body);
        if (t.hasMatch() && l.hasMatch() && g.hasMatch() && isLevelName(l.captured(1))) {
            out.ok = true;
            out.layerTimestamp = t.captured(1);
            out.level = normalizeLevel(l.captured(1));
            out.message = body;
            return out;
        }
    }

    // tracing Full/Compact + env_logger: "TS LEVEL [span:]target: msg".
    {
        static const QRegularExpression tracing(
            QStringLiteral("^(\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}(?:\\.\\d+)?Z?)\\s+(TRACE|DEBUG|INFO|WARN|ERROR)\\s+([\\s\\S]*)$"));
        const auto m = tracing.match(body);
        if (m.hasMatch()) {
            const QString rest = m.captured(3);
            const int sep = rest.lastIndexOf(QStringLiteral(": "));
            if (sep > 0) {
                out.ok = true;
                out.layerTimestamp = m.captured(1);
                out.level = normalizeLevel(m.captured(2));
                out.channel = rest.left(sep);
                out.message = body;
                return out;
            }
        }
        static const QRegularExpression envLogger(
            QStringLiteral("^\\[([^\\s\\]]+)\\s+(ERROR|WARN|INFO|DEBUG|TRACE)\\s+([^\\]]+)\\]\\s*([\\s\\S]*)$"));
        const auto e = envLogger.match(body);
        if (e.hasMatch()) {
            out.ok = true;
            out.layerTimestamp = e.captured(1);
            out.level = normalizeLevel(e.captured(2));
            out.channel = e.captured(3);
            out.message = body;
            return out;
        }
    }

    // Python basicConfig: "LEVEL:name:msg".
    {
        static const QRegularExpression py(
            QStringLiteral("^(DEBUG|INFO|WARNING|ERROR|CRITICAL):([^:]+):([\\s\\S]*)$"));
        const auto m = py.match(body);
        if (m.hasMatch()) {
            out.ok = true;
            out.level = m.captured(1);
            out.channel = m.captured(2);
            out.message = body;
            return out;
        }
    }

    // Syslog RFC5424 / RFC3164.
    {
        static const QRegularExpression rfc5424(
            QStringLiteral("^<(\\d{1,3})>(\\d)\\s+(\\S+)\\s+(\\S+)\\s+(\\S+)\\s+(\\S+)\\s+(\\S+)?\\s*([\\s\\S]*)$"));
        const auto m = rfc5424.match(body);
        if (m.hasMatch()) {
            const QString level = severityToLevel(m.captured(1).toInt() & 7);
            if (!level.isEmpty()) {
                out.ok = true;
                out.level = level;
                out.channel = m.captured(4);
                out.message = m.captured(8).isEmpty() ? body : m.captured(8);
                return out;
            }
        }
        static const QRegularExpression rfc3164(
            QStringLiteral("^<(\\d{1,3})>[A-Z][a-z]{2}\\s+\\d{1,2}\\s+\\d{2}:\\d{2}:\\d{2}\\s+(\\S+)\\s+([^:\\[]+)(?:\\[\\d+\\])?:\\s*([\\s\\S]*)$"));
        const auto s = rfc3164.match(body);
        if (s.hasMatch()) {
            const QString level = severityToLevel(s.captured(1).toInt() & 7);
            if (!level.isEmpty()) {
                out.ok = true;
                out.level = level;
                out.channel = s.captured(3);
                out.message = s.captured(4);
                return out;
            }
        }
    }

    // Access-логи: combined/CLF. Уровня нет — эвристика по статусу.
    {
        static const QRegularExpression access(
            QStringLiteral("^(\\S+)\\s+\\S+\\s+\\S+\\s+\\[([^\\]]+)\\]\\s+\"([A-Z]+)\\s+(\\S+)(?:\\s+\\S+\")?\"?\\s+(\\d{3})\\s+(\\S+)([\\s\\S]*)$"));
        const auto m = access.match(body);
        if (m.hasMatch()) {
            out.ok = true;
            out.layerTimestamp = m.captured(2);
            out.level = levelFromHttpStatus(m.captured(5).toInt());
            out.message = body;
            return out;
        }
    }

    // Apache error: "[date] [module:severity] ...".
    {
        static const QRegularExpression apacheErr(
            QStringLiteral("^\\[([^\\]]+)\\]\\s*\\[([^:\\]]+):([a-z0-9]+)\\]([\\s\\S]*)$"));
        const auto m = apacheErr.match(body);
        if (m.hasMatch()) {
            static const QHash<QString, QString> sev = {
                {QStringLiteral("emerg"), QStringLiteral("EMERGENCY")},
                {QStringLiteral("alert"), QStringLiteral("ALERT")},
                {QStringLiteral("crit"), QStringLiteral("CRITICAL")},
                {QStringLiteral("error"), QStringLiteral("ERROR")},
                {QStringLiteral("warn"), QStringLiteral("WARNING")},
                {QStringLiteral("notice"), QStringLiteral("NOTICE")},
                {QStringLiteral("info"), QStringLiteral("INFO")},
                {QStringLiteral("debug"), QStringLiteral("DEBUG")},
            };
            QString level = sev.value(m.captured(3).toLower());
            if (level.isEmpty() && m.captured(3).toLower().startsWith(QStringLiteral("trace")))
                level = QStringLiteral("TRACE");
            if (!level.isEmpty()) {
                out.ok = true;
                out.layerTimestamp = m.captured(1);
                out.level = level;
                out.channel = m.captured(2);
                out.message = m.captured(4).trimmed().isEmpty() ? body : m.captured(4).trimmed();
                return out;
            }
        }
    }

    // npm CLI: "npm <level> ...".
    {
        static const QRegularExpression npm(
            QStringLiteral("^npm\\s+(error|warn|notice|http|info|verbose|silly|timing)\\b\\s*([\\s\\S]*)$"),
            QRegularExpression::CaseInsensitiveOption);
        const auto m = npm.match(line);
        if (m.hasMatch()) {
            out.ok = true;
            out.level = normalizeLevel(m.captured(1));
            out.message = line;
            return out;
        }
    }

    return out; // fallback: ok=false, сырая строка для substring-поиска
}
