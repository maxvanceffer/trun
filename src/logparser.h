#pragma once

#include <QString>
#include <QStringList>

// One parsed log line. Covers both console history (with layer 1) and
// file logs (without layer 1).
struct ParsedLogLine {
    bool ok = false;           // false = format unrecognized (substring search only)
    QString layerTimestamp;    // layer 1: [HH:mm:ss.zzz] or Docker-ts; empty when absent
    QString target;            // layer 1: command label; empty for file logs
    QString level;             // normalized UPPER (WARN -> WARNING)
    QString channel;           // layer-2 channel; empty when the format gives none
    QString message;           // body: whole line for text formats, msg field for JSON
};

// The result of applying filters to one line (AND; an empty filter is off).
// An unrecognized line takes part in query only: for level/channel purposes
// it is HiddenUnparsed (the UI shows a counter, MCP skips it silently).
enum class LogFilterHit { Shown, Hidden, HiddenUnparsed };

struct LogFilterQuery {
    QString query;          // substring, case-insensitive
    QStringList levels;     // exact matches (normalized)
    QStringList channels;   // exact matches, case-insensitive
};

// Two-layer parser. The layer-2 probe order is fixed by research
// (.scratch/console-filters/research-log-formats.md): JSON -> Docker-ts ->
// Monolog(a/b/c) -> uvicorn -> gunicorn-error -> logrus -> tracing/env_logger ->
// python -> syslog -> access -> fallback. All layer-2 regexes run after SGR
// stripping. Dictionaries are not hardcoded: only the "looks like a level"
// check is known.
class LogParser {
public:
    static ParsedLogLine parse(const QString &raw);
    static LogFilterHit applyFilter(const ParsedLogLine &p, const QString &raw,
                                    const LogFilterQuery &f);
    static QString stripSgr(const QString &s);
    static QString normalizeLevel(const QString &level);
    static bool isLevelName(const QString &level);
};
