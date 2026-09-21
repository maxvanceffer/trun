## Destination

Готовый spec фильтров Console (UI + парсинг + MCP-поиск), по которому можно строить без новых решений: какие лог-форматы парсим в структурированные поля, как выглядят и ведут себя фильтры в DetailPage, какой MCP API даёт ИИ осмысленный поиск («ошибка DOCTRINE за 10 минут») без перерасхода токенов.

## Notes

Домен: trun — `Console history` per `Command` (см. CONTEXT.md), DetailPage Console (single-text TextEdit + ConsoleFormat.js), LogModel (timestamp/level/target/message, cap 1000), McpServer (m_logs cap 2000, `read_log`/`search_logs`, monologMatch уже есть для файловых логов).
Скиллы каждой сессии: `qt-qml` для QML, `qt-cpp-review` перед C++-решениями, плюс тип тикета (`research` / `prototype` / `grilling` / `domain-modeling`).
Предпочтения: решения, не deliverables (plan, don't do); ресурсы QML только через `qrc:/`; термины строго из CONTEXT.md (Command, Run, Console history, Detail page, MCP server); ссылаться на тикеты по имени, не по номеру.

## Decisions so far

- [Форматы логов](issues/01-log-formats.md): двухслойный парсинг (trun-префикс всегда + тело), парсить monolog×3/uvicorn/gunicorn/logrus/zap-JSON/nginx/apache/docker-обёртку, остальное — substring; детали в research-log-formats.md.
- [Корпус логов](issues/02-log-corpus.md): артефакт corpus-sample.log (95 строк: 8 каналов dev.log + редкие уровни + жирный SQL 28k + 7 строк serve-консоли); файловые логи строго однострочные, DEBUG ~85% потока.
- [Семантика фильтров](issues/03-filter-semantics.md): 4 фильтра с AND (текст-substring-CI, level, channel, время-пресеты), hide + «X из Y», нераспознанные только в тексте, без паузы, обрезка ~500 с expand (в MCP — полные), session-only; MCP обязан отдавать накопленные level/channel.
- [MCP-поиск](issues/05-mcp-search.md): read_log + search_logs с query/level/channel/sinceMinutes (AND); выдача — plain-text полные строки + шапка (observedLevels/Channels, matched/total, truncated), без JSON-объектов; капы как сейчас, единый probe chain, fallback substring.
- [Прототип панели фильтров](issues/04-filter-bar-prototype.md): вердикт — вариант A «Тулбар»; артефакт prototype/FilterBarPrototype.qml (qmllint/offscreen чисто); fold-in: Theme-палитра, движок на C++/модель, JS только референс.

## Not yet specified

- Персистентность фильтров per Command (Settings/runConfig vs session-only) — станет резкой после семантики фильтров.
- Multiline-записи (stack traces, SQL на несколько строк) — схлопывать ли в одну запись при парсинге.
- Производительность: фильтрация 1000–2000 строк в QML-модели vs C++ proxy, виртуализация, подсветка совпадений.
- Поведение ANSI/SGR при фильтрации и в MCP-выдаче (чистить всегда, как сейчас в appendLog, или сохранять).
- Оценка сложности/ёмкости: имеет ли смысл вообще при непарсящихся логах (fallback = только plain-text поиск).

## Out of scope

<!-- пока пусто -->
