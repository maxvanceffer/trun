#pragma once

#include <QString>
#include <QStringList>

// Одна разобранная строка лога. покрывает и Console history (со слоем 1),
// и файловые логи (без слоя 1).
struct ParsedLogLine {
    bool ok = false;           // false = формат не распознан (только substring-поиск)
    QString layerTimestamp;    // слой 1: [HH:mm:ss.zzz] или Docker-ts; пусто, если нет
    QString target;            // слой 1: label команды; пусто для файловых логов
    QString level;             // нормализованный UPPER (WARN -> WARNING)
    QString channel;           // канал слоя 2; пусто, если формат его не даёт
    QString message;           // тело: целиком для текстовых форматов, msg-поле для JSON
};

// Результат применения фильтров к одной строке (AND; пустой фильтр выключен).
// Нераспознанная строка участвует только в query: под level/channel она
// HiddenUnparsed (UI показывает счётчик, MCP молча пропускает).
enum class LogFilterHit { Shown, Hidden, HiddenUnparsed };

struct LogFilterQuery {
    QString query;          // substring, case-insensitive
    QStringList levels;     // точные совпадения (нормализуются)
    QStringList channels;   // точные совпадения, case-insensitive
};

// Двухслойный парсер. Порядок проб слоя 2 зафиксирован исследованием
// (.scratch/console-filters/research-log-formats.md): JSON -> Docker-ts ->
// Monolog(a/b/c) -> uvicorn -> gunicorn-error -> logrus -> tracing/env_logger ->
// python -> syslog -> access -> fallback. Все regex слоя 2 — после strip SGR.
// Словари не хардкодятся: известна лишь проверка «похоже на уровень».
class LogParser {
public:
    static ParsedLogLine parse(const QString &raw);
    static LogFilterHit applyFilter(const ParsedLogLine &p, const QString &raw,
                                    const LogFilterQuery &f);
    static QString stripSgr(const QString &s);
    static QString normalizeLevel(const QString &level);
    static bool isLevelName(const QString &level);
};
