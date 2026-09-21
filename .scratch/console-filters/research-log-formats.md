# Research: лог-форматы со стабильным выводом (тикет 01-log-formats)

Вопрос тикета: какие лог-форматы со стабильным структурированным выводом реально
встречаются в проектах trun (кроме monolog/Symfony) и какие поля из них можно
надёжно парсить в `(timestamp, level, channel/target, message)`?

Метод: только первичные источники — исходники библиотек/рантаймов и официальная
документация. Каждый пункт ссылается на источник. Примеры — из тех же источников
(не выдуманы).

Ключевой архитектурный факт (свой код trun, проверено чтением исходников):
парсинг всегда двухслойный.

- **Слой 1 — префикс trun, стабилен на 100 %.** `CommandExecutor` отдаёт
  `outputReceived(id, label, isError, line, commandId)` (`src/commandexecutor.cpp:595`),
  QML складывает `commandLog.add(isError ? "stderr" : "stdout", label, line)`
  (`qml/Dashboard.qml:493-494`), MCP — `appendLog(commandId, stdout|stderr|system|error,
  label, line)` (`src/mcpserver.cpp:63-67`). `LogModel::add` штампует
  `HH:mm:ss.zzz` локально (`src/logmodel.h:52`), рендер —
  `[ts] target: message` (`qml/ConsoleFormat.js:69-73`).
  Грамматика слоя 1:
  `^\[(?P<ts>\d{2}:\d{2}:\d{2}\.\d{3})\] (?P<target>[^:]+): (?P<body>.*)$`,
  множество `level` закрыто: `{stdout, stderr, system, error}`.
- **Слой 2 — тело `body`.** Его формат принадлежит дочернему процессу.
  Ниже — разбор по форматам: что стабильно, что парсится, что — только substring.

Важно про ANSI: `ConsoleFormat.ansiToHtml` красит SGR на отображении, а
`McpServer::appendLog` чистит SGR для MCP (`src/mcpserver.cpp:615-617`), но сырое
сообщение в `LogModel` цвета содержит. **Любой regex слоя 2 применять только после
strip SGR** (`\x1b\[[0-9;]*m`), иначе цветные префиксы (`[vite]`, уровни Symfony)
не сматчатся.

---

## 1. Monolog LineFormatter — файловые логи (стабилен)

Источник: `Seldaek/monolog`, `src/Monolog/Formatter/LineFormatter.php`,
константа `SIMPLE_FORMAT`, и `doc/01-usage.md`.

- Дефолт: `[%datetime%] %channel%.%level_name%: %message% %context% %extra%\n`
- Дефолтная дата: `Y-m-d\TH:i:sP` (ISO8601 с таймзоной).
- Уровни — закрытое множество PSR-3: `DEBUG, INFO, NOTICE, WARNING, ERROR,
  CRITICAL, ALERT, EMERGENCY`. Канал — произвольная строка (словарь заранее
  неизвестен, выводится из потока — как и требует тикет).
- Пример (из документации): `[2026-09-10T12:00:00+00:00] app.DEBUG: message …`
- Regex-кандидат:
  `^\[(?P<ts>[^\]]+)\] (?P<channel>[^.]+)\.(?P<level>[A-Z]+): (?P<msg>.*)$`,
  затем валидация `level` по словарю, `ts` — проба ISO8601.
- Это же уже парсит `McpServer::monologMatch` (`src/mcpserver.cpp:624-656`)
  для файловых логов — переиспользовать логику.
- Стабильность: **да** (формат дефолта зафиксирован кодом; кастомизация через
  `setFormatter` возможна, но в Symfony-скелетах дефолт не меняют).
- Рекомендация: **парсить**.

## 2. Symfony ConsoleFormatter — то, что реально видит trun от `serve` (стабилен)

Источник: `symfony/symfony`, `src/Symfony/Bridge/Monolog/Handler/ConsoleHandler.php`
+ `src/Symfony/Bridge/Monolog/Formatter/ConsoleFormatter.php` (прочитаны целиком).

- `ConsoleHandler` пишет в консоль по маппингу verbosity→level
  (`VERBOSITY_NORMAL→WARNING, -v→NOTICE, -vv→INFO, -vvv→DEBUG`), ошибки — в stderr.
- Форматтер по умолчанию: `SIMPLE_FORMAT =
  "%datetime% %start_tag%%level_name%%end_tag% <comment>[%channel%]</> %message%%context%%extra%\n"`,
  `SIMPLE_DATE = 'H:i:s'`, `level_name_format = '%-9s'` (уровень дополнен пробелами
  до 9 символов), цвета по `LEVEL_COLOR_MAP` (ANSI-теги Symfony, в сыром потоке —
  SGR-последовательности).
- Пример после strip SGR: `11:13:15 DEBUG     [app] Hello {name}` →
  `ts=11:13:15, level=DEBUG, channel=app`.
- Regex-кандидат (по strip-строке):
  `^(?P<ts>\d{2}:\d{2}:\d{2}) (?P<level>[A-Z]+)\s+\[(?P<channel>[^\]]+)\] (?P<msg>.*)$`
- Внимание — строка из постановки тикета
  `serve: [Application] Sep 16 11:13:15 |DEBUG | DOCTRI…` это **не** стоковый
  формат: ни `LineFormatter`, ни `ConsoleFormatter` не дают `[Chan] Mon DD HH:MM:SS
  |LEVEL|`. Это кастомный форматтер проекта (распространённый рецепт
  `"[%channel%] %datetime% |%level_name%| %message%"` с датой `M d H:i:s`).
  Значит парсер обязан держать **все три варианта monolog-тела** упорядоченными
  альтернативами: (a) `LineFormatter`, (b) `ConsoleFormatter`, (c) кастомный
  `[channel] Mon DD HH:MM:SS |LEVEL| msg`:
  `^\[(?P<channel>[^\]]+)\] (?P<ts>[A-Z][a-z]{2}\s+\d{1,2} \d{2}:\d{2}:\d{2}) \|(?P<level>[A-Z]+)\s*\| (?P<msg>.*)$`
- Стабильность: **да** для (a)/(b) — зафиксировано кодом Symfony/Monolog;
  (c) — полустабилен (конвенция, встречается в дикой природе).
- Рекомендация: **парсить**, порядок проб: (b) → (a) → (c) → fallback.

## 3. npm CLI (полустабилен: уровень — да, остальное — нет)

Источник: `npm/cli`, `lib/utils/display.js` (прочитан целиком).

- Каждая лог-строка пишется в stderr с префиксом `[heading, level, title]`:
  буквально `npm <level> [<title>] <message>`. Уровни — закрытое множество:
  `error, warn, notice, http, info, verbose, silly` (+ `timing`, дедупликация
  `notice`). Таймстампа в строке **нет**.
- Lifecycle-баннеры `npm run <script>` (`@npmcli/run-script`, наблюдаемое
  поведение, спеки формата нет): `> pkg@1.0.0 script` / `> command`.
- Пример: `npm warn deprecated querystring@0.2.1: …`.
- Regex-кандидат: `^npm (?P<level>error|warn|notice|http|info|verbose|silly|timing)\b\s*(?P<msg>.*)$`.
- Стабильность: **частичная** — префикс и словарь уровней стабильны (код),
  содержание `msg` и баннеры — ad-hoc.
- Рекомендация: **парсить только level** (`stderr→npm-уровень`), остальное —
  substring. Баннеры `> …` — только substring/якорь начала секции.

## 4. Vite dev server (нестабилен, кроме префикса)

Источник: `vitejs/vite`, `packages/vite/src/node/logger.ts` (прочитан целиком);
CLI-опция `--logLevel info|warn|error|silent` (`vite.dev/guide/cli`).

- `createLogger`: сообщение проходит как есть (`return msg`), и только при
  `options.timestamp` добавляется `dim(time) + [vite] + env + msg`.
  Префикс по умолчанию `[vite]`, цвета через picocolors (в трубе при не-TTY —
  без цвета). Дедупликатор `(xN)`, `clearScreen` — служебный мусор в потоке.
- Сами тексты (`ready in …`, `vite v… ready in …`, `➜ Local: http://…`,
  ошибки transform, HMR-апдейты) грамматики не имеют — «что захотели, то вывели».
- Regex-кандидат (только префикс): `^(?:(?P<ts>\d{1,2}:\d{2}:\d{2}\s?[AP]M|[0-2]?\d:\d{2}:\d{2}) )?(?:\[vite\] )(?P<msg>.*)$`
  (`Intl.DateTimeFormat` с hour/minute/second — формат зависит от локали!).
- Стабильность: **нет** (время локальное и опциональное, тексты свободные).
- Рекомендация: **только substring** + опциональный срез `[vite]`-префикса;
  level брать из слоя 1 (`stdout/stderr`).

## 5. Next.js dev server (нестабилен, глиф-префикс — слабый сигнал)

Источник: `vercel/next.js`, `packages/next/src/server/lib/start-server.ts`
(прочитан; использование `Log.event / Log.warn / Log.error` зафиксировано кодом).

- Стартовые/рантайм-строки идут через `build/output/log` (`Log.event('Ready in …')`,
  `Log.warn('Port … is in use …')`, `Log.error('Failed to handle request …')`,
  `logStartInfo` печатает `▲ Next.js x.y.z`, `- Local: …`, `- Network: …`).
  Уровень кодируется глифом-префиксом (`▲ ✓ ○ ⚠ ⨯ …`, точная таблица — в
  `packages/next/src/build/output/log.ts`), дальше свободный текст:
  `○ Compiling /page …`, `✓ Compiled /page in 123ms`, `GET / 200 in 45ms`,
  ошибки компиляции — многострочные блоки.
- Стабильность: **нет** (глифы — деталь реализации, тексты свободные,
  access-строки `METHOD path status in Xms` — конвенция без спеки).
- Рекомендация: **только substring**; как слабый эвристический маппинг
  глиф→severity — только после сверки с корпусом (тикет 02-log-corpus), иначе нет.

## 6. Docker (транспорт стабилен, тело — вложенный формат)

Источники: `docs.docker.com` (`json-file` driver, `docker container logs`,
секция `--timestamps` → RFC3339Nano с дополнением нулями);
`moby/moby` `daemon/logger/jsonfilelog/jsonfilelog.go`; свой код trun.

- Факт про trun: `DockerService::queryLogs` вызывает
  `docker logs --tail N --timestamps <name>` с merged-каналами
  (`src/dockerservice.cpp:237-251`). Значит каждая строка, которую увидит парсер:
  `<RFC3339Nano-время> <сырое тело>`, без признака stdout/stderr (каналы склеены).
- Пример: `2025-09-16T11:13:15.123456789Z [2026-…] app.DEBUG: …` (таймстамп демона +
  вложенный monolog).
- `{"log","stream","time"}` json-file драйвера trun **не видит** — это серверный
  формат демона, не клиентский вывод.
- Префикса `container | ` здесь нет — его добавляет только `docker compose logs`,
  а не `docker logs`.
- Regex-кандидат (слой-обёртка):
  `^(?P<ts>\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?Z) (?P<msg>.*)$`
  (в реальности всегда `Z`/наносекунды; зону допускать `[Z+-…]`).
- Стабильность: **да** для обёртки; тело — рекурсия в парсер слоя 2
  (часто JSON-логи приложений → если тело `{…}`, парсить как JSON-объект
  с полями `level/msg/message/ts/time/logger/name` best-effort).
- Рекомендация: **парсить обёртку (timestamp), тело — рекурсивно**.

## 7. Python logging / stdlib (полустабилен)

Источник: `python/cpython`, `Lib/logging/__init__.py` (прочитан).

- Голый `Formatter()` без `fmt` → строка равна ровно `%(message)s` (уровня/времени
  **нет**). `logging.basicConfig()` ставит `BASIC_FORMAT =
  "%(levelname)s:%(name)s:%(message)s"`, пример: `ERROR:root:Some error`.
- Уровни закрыты: `DEBUG, INFO, WARNING, ERROR, CRITICAL`. Имя логгера — свободная
  строка (аналог channel). `%(asctime)s` по умолчанию `Y-m-d H:i:s,mmm`
  (`default_time_format`/`default_msec_format`), но появляется только если формат
  задан явно. На практике каждый проект задаёт свой `format=` → дикая природа.
- Regex-кандидат (только basicConfig-вариант):
  `^(?P<level>DEBUG|INFO|WARNING|ERROR|CRITICAL):(?P<channel>[^:]+):(?P<msg>.*)$`.
- Стабильность: **частичная** (два дефолта зафиксированы кодом, остальное —
  произвольные `format=`).
- Рекомендация: **парсить оппортунистически** (проба regex → fallback substring).

## 8. Gunicorn (дефолты стабильны, но конфигурируемы)

Источник: `benoitc/gunicorn`, `gunicorn/glogging.py` (прочитан) + `docs/.../settings.md`.

- Error-лог: `error_fmt = "%(asctime)s [%(process)d] [%(levelname)s] %(message)s"`,
  `datefmt = "[%Y-%m-%d %H:%M:%S %z]"`.
  Пример: `[2025-09-16 11:13:15 +0000] [1234] [INFO] Starting gunicorn 23.0.0`.
- Access-лог дефолт: `'%(h)s %(l)s %(u)s %(t)s "%(r)s" %(s)s %(b)s "%(f)s" "%(a)s"'`
  (Apache-совместимый; `%(t)s` = `[10/Oct/2000:13:55:36 -0700]`).
- Regex-кандидаты: error —
  `^\[(?P<ts>[^\]]+)\] \[(?P<pid>\d+)\] \[(?P<level>[A-Z]+)\] (?P<msg>.*)$`;
  access — тот же, что для Apache combined (§14).
- Стабильность: **да для дефолтов** (код), но `access_log_format`/`logconfig`
  на практике меняют → держать fallback.
- Рекомендация: **парсить оба дефолта**, иначе substring.

## 9. Uvicorn (стабилен при дефолтном LOGGING_CONFIG)

Источники: `encode/uvicorn`, `uvicorn/config.py` (`LOGGING_CONFIG`, прочитан) +
`uvicorn/logging.py` (`ColourizedFormatter`, прочитан).

- Default-логгер: `fmt = "%(levelprefix)s %(message)s"`, где `levelprefix =
  levelname + ":" + " "*(8-len(levelname))` (код `formatMessage`).
  **Таймстампа в строке нет вообще.**
  Пример: `INFO:     Started server process [1234]` (`INFO`+`:`+5 пробелов).
- Access-логгер (stdout!): `fmt =
  '%(levelprefix)s %(client_addr)s - "%(request_line)s" %(status_code)s'`.
  Пример: `INFO:     127.0.0.1:52344 - "GET / HTTP/1.1" 200 OK`.
- Уровни: стандартные logging + кастомный `TRACE=5`. Стартовые строки
  (`Uvicorn running on http://…`) идут через тот же default-форматтер.
- Regex-кандидаты:
  default — `^(?P<level>TRACE|DEBUG|INFO|WARNING|ERROR|CRITICAL):\s+(?P<msg>.*)$`;
  access — `^(?P<level>[A-Z]+):\s+(?P<client>\S+) - "(?P<method>\S+) (?P<path>\S+) HTTP/(?P<ver>[0-9.]+)" (?P<status>\d{3})(?: (?P<phrase>.*))?$`.
- Стабильность: **да** (пока не переопределён `--log-config`; переопределение —
  штатная практика → fallback обязателен).
- Рекомендация: **парсить**; timestamp = время прибытия строки (слой 1).

## 10. Go: logrus (текст — полустабилен, JSON — стабилен)

Источник: `sirupsen/logrus` README + `pkg.go.dev/github.com/sirupsen/logrus`
(цитаты дословные из документации пакета).

- `TextFormatter` **без TTY** (а в трубе trun TTY нет — это наш случай):
  выход совместим с logfmt:
  `time="2015-03-26T01:27:38-04:00" level=warning msg="…" animal=walrus number=122`.
  Порядок полей: `time, level, msg`, дальше пользовательские `k=v`.
- С TTY (интерактивный запуск, не наш случай, но строка может прийти из
  `docker logs`): цвета + **уровни урезаются до 4 символов** (`DEBU/INFO/WARN/ERRO/
  FATA/PANI`, отключается `DisableLevelTruncation`), паддинг опционален
  (`PadLevelText`). Этот режим для строгого парсинга **непригоден**.
- `JSONFormatter` (частый выбор для продакшена): стабильные ключи
  `{"level":"warning","msg":"…","time":"…",…custom}`.
- Regex-кандидат (logfmt, порядок фиксирован кодом, но допускать перестановку):
  `(?:^|\s)time="(?P<ts>[^"]+)"(?:\s|$).*?(?:^|\s)level=(?P<level>\w+)(?:\s|$).*?(?:^|\s)msg="(?P<msg>(?:[^"\\]|\\.)*)"`.
  Проще и надёжнее: искать три `k=v`-токена независимо, `msg` — со скобками кавычек.
- Стабильность: **да без TTY / JSON**; TTY-режим — нет.
- Рекомендация: **парсить logfmt и JSON**, TTY-вариант — substring.

## 11. Go: zap (JSON стабилен, console — полустабилен)

Источник: `uber-go/zap`, `config.go` (`NewProductionEncoderConfig`,
`NewDevelopmentEncoderConfig`, прочитаны) + `zapcore/encoder_test.go`.

- Production (JSON): ключи `level (lowercase), ts (epoch float), logger?, caller?,
  msg, stacktrace?`. Пример:
  `{"level":"info","ts":1726487595.123,"caller":"srv/main.go:42","msg":"listening"}`.
  Форматы времени/уровня настраиваются (`EncodeTime/EncodeLevel`), но дефолты —
  самые распространённые в дикой природе.
- Development (console, tab-separated, порядок фиксирован кодом):
  `T L N C M`: `2025-09-16T11:13:15.123+0300\tINFO\tmyapp\tsrv/main.go:42\tlistening`
  (`CapitalLevelEncoder`, ISO8601). Любые ключи можно переименовать/опустить через
  `EncoderConfig` (тесты это демонстрируют) — поэтому позиционный парсинг хрупок.
- Regex-кандидаты: JSON — `json.loads` + проверка `msg`+`level`;
  console — `^(?P<ts>\S+)\t(?P<level>[A-Z]+)\t(?:(?P<logger>[^\t]*)\t)?(?P<caller>[^\t]*)\t(?P<msg>.*)$`
  с валидацией уровня.
- Стабильность: **JSON — да; console — частично**.
- Рекомендация: **JSON парсить всегда; console — оппортунистически**.

## 12. Rust: tracing_subscriber fmt (полустабилен)

Источник: `docs.rs/tracing-subscriber`, страницы `fmt`, `fmt::format::Format`,
`fmt::format::Full` (примеры вывода дословно оттуда).

- Full-дефолт: `<RFC3339-микросекунды> <LEVEL> [span{поля}: ]target: <поля события / message>`.
  Дословный пример из доков:
  `2022-02-15T18:40:14.289898Z  INFO shaving_yaks{yaks=3}:shave{yak=1}: fmt::yak_shave: hello! I'm gonna shave a yak excitement="yay!"`
  (уровень визуально в колонке шириной 5: ` INFO`, ` WARN`, `TRACE`, `DEBUG`, `ERROR`).
- Всё настраивается: `with_target/with_level/with_thread_ids/without_time`,
  ANSI-цвета, `Compact` (span-поля в конец), `Pretty` (многострочный!),
  `Json` (`{"timestamp":…,"level":"INFO","target":"mycrate","fields":{…}}`
  — стабилен, парсить как JSON §11).
- Regex-кандидат (Full/Compact, ANSI уже снят):
  `^(?P<ts>\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?Z?)\s+(?P<level>TRACE|DEBUG|INFO|WARN|ERROR)\s+(?:(?P<span>[A-Za-z0-9_:]+(?:\{[^}]*\}(?:::[A-Za-z0-9_:]+(?:\{[^}]*\})?)*)?:))?(?P<target>[A-Za-z0-9_:]+): (?P<msg>.*)$`.
  На практике достаточно жадной пробы справа: последнее `target: ` перед сообщением.
- Стабильность: **частичная** (дефолт узнаваем, но однострочные опции меняют всё;
  `Pretty` вообще многострочный).
- Рекомендация: **парсить оппортунистически** (ts+level+target), `Pretty` и
  нестандарт — substring; JSON-вариант — как JSON.

## 13. Rust: env_logger (полустабилен + явный дисклеймер автора)

Источник: `docs.rs/env_logger` (разделы Example, Tweaking/Stability, дословно).

- Дефолт: `[2017-11-09T02:12:24Z ERROR main] this is printed by default`
  (`[ts LEVEL target] msg`, уровни lowercase-приём, UPPERCASE-печать:
  `error|warn|info|debug|trace`; пишет в **stderr**).
- Авторский дисклеймер (дословно): «The default format won't optimise for
  long-term stability, and explicitly makes no guarantees about the stability of
  its output across major, minor or patch version bumps during `0.x`. If you want
  to capture or interpret the output of `env_logger` programmatically then you
  should use a custom format.»
- Regex-кандидат: `^\[(?P<ts>[^\s\]]+) (?P<level>ERROR|WARN|INFO|DEBUG|TRACE) (?P<target>[^\]]+)\] (?P<msg>.*)$`.
- Стабильность: **частичная с официальной оговоркой**.
- Рекомендация: **парсить оппортунистически**, держать fallback; в своих
  Rust-проектах рекомендовать кастомный формат/JSON.

## 14. Nginx access (стабилен) + Apache access/error (стабильны)

Источники: `nginx.org/en/docs/http/ngx_http_log_module.html` (дефолт `combined`
дословно); `httpd.apache.org/docs/2.4/logs.html` + `mod_log_config` (CLF/Combined/
ErrorLogFormat дословно).

- Nginx `combined` (дефолт `access_log logs/access.log combined`):
  `$remote_addr - $remote_user [$time_local] "$request" $status $body_bytes_sent "$http_referer" "$http_user_agent"`,
  где `$time_local` = `10/Oct/2000:13:55:36 -0700`.
- Apache CLF: `%h %l %u %t "%r" %>s %b`, пример:
  `127.0.0.1 - frank [10/Oct/2000:13:55:36 -0700] "GET /apache_pb.gif HTTP/1.0" 200 2326`.
  Combined = CLF + `"%{Referer}i" "%{User-agent}i"`.
- Apache error (дефолт `ErrorLogFormat`):
  `[Fri Sep 09 10:42:29.902022 2011] [core:error] [pid 35708:tid 4328636416] [client 72.15.99.187] AH00124: …`
  → дата, `module:severity`, pid/tid, client, код+текст. Уровни: `emerg/alert/crit/
  error/warn/notice/info/debug/trace1-8` (+ per-module `LogLevel`).
- Regex-кандидаты:
  combined — `^(?P<ip>\S+) \S+ (?P<user>\S+) \[(?P<ts>[^\]]+)\] "(?P<method>\S+) (?P<path>\S+)(?: \S+)?" (?P<status>\d{3}) (?P<size>\S+)(?: "(?P<referer>[^"]*)" "(?P<ua>[^"]*)")?.*$`;
  apache-error —
  `^\[(?P<ts>[^\]]+)\] \[(?P<module>[^:]+):(?P<level>[a-z0-9]+)\](?: \[pid [^\]]+\])?(?: \[client [^\]]+\])? (?P<msg>.*)$`.
- Стабильность: **да** (дефолты зафиксированы документацией; `log_format`/
  `LogFormat` кастомизируемы → fallback).
- Рекомендация: **парсить**; `target = "nginx-access"/"apache-access"` (или имя
  команды), `level` для access-логов отсутствует как таковой → хранить `status`
  в message и/или маппить `status>=500→error, >=400→warn, иначе info`
  (эвристика, явно помеченная).

## 15. systemd journal / journalctl (стабилен при явном `-o`)

Источник: man `journalctl(1)` (Arch manual pages, прочитан; опции `-o/--output`,
фильтры `-p/--priority`, `-u`, `-g`, `-o json*`, `cat`, `with-unit`).

- `short` (дефолт) ≈ классический syslog: `Mar 30 12:00:00 host ident[pid]: msg`
  (имена месяцев — локаль!). `short-iso` — RFC3339-таймстамп (стабильнее).
  Цвета приоритетов — только на TTY.
- `-o json`: структурированные поля `MESSAGE, PRIORITY (0-7 syslog), SYSLOG_IDENTIFIER,
  _SYSTEMD_UNIT, _PID, __REALTIME_TIMESTAMP…` — **машиночитаемо и стабильно**.
- `-o cat`: только сообщение (метаданных нет — парсить нечего).
- Уровни: syslog 0-7 (`emerg…debug`), фильтр `-p` работает по ним же.
- Для trun напрямую малоприменимо (команды — обычные дочерние процессы, не юниты),
  но если появится ingestion через `journalctl -u … -o json` — парсить JSON.
- Рекомендация: **парсить только `-o json`**; `short` — оппортунистически
  (ts+host+ident), `cat` — substring.

## 16. Syslog wire format (стабилен, встретится редко)

Источники: RFC5424 §6 (PRI/VERSION/timestamp/hostname/app/procid/msgid/structured-data),
RFC3164 §4 (3164-формат). Знание спеки, цитирование по памяти разделов — сверить
при реализации.

- RFC5424: `<165>1 2003-10-11T22:14:15.003Z host app procid msgid [sd] msg`;
  `PRI = facility*8+severity`, severity 0-7 — готовый `level`.
- RFC3164: `<34>Oct 11 22:14:15 host tag[pid]: msg`.
- В консоли trun сырой syslog почти не встретится (разве что проброс через
  `logger(1)`/`syslog:` драйвер nginx) — держать как дешёвую пробу в конце цепочки.
- Рекомендация: **парсить оппортунистически** (два regex), приоритет низкий.

---

## Сводная таблица

| Формат | Пример | Поля (ts / level / target / msg) | Стабильность | Рекомендация |
|---|---|---|---|---|
| trun слой 1 (свой код) | `[14:13:29.692] serve: …` | ts ✓ / stdout·stderr·system·error ✓ / label ✓ / body ✓ | **да** | парсить всегда |
| Monolog LineFormatter | `[2026-09-10T12:00:00+00:00] app.DEBUG: …` | ts ✓ / словарь 8 ✓ / channel ✓ / msg ✓ | **да** | парсить |
| Symfony ConsoleFormatter | `11:13:15 DEBUG     [app] …` | H:i:s ✓ / словарь 8 ✓ / channel ✓ / msg ✓ | **да** | парсить |
| Monolog-кастом `[ch] Mon DD … \|LVL\|` | `[Application] Sep 16 11:13:15 \|DEBUG\| …` | ts ✓ / level ✓ / channel ✓ / msg ✓ | частичная | парсить 3-й альтернативой |
| npm CLI | `npm warn deprecated …` | — / 7 уровней ✓ / — / ✓ | частичная | парсить только level |
| Vite dev | `[vite] ready in 123 ms` / `➜ Local: …` | (локальное ts, опц.) / — / — / ✓ | **нет** | только substring |
| Next.js dev | `▲ Next.js 15.0.0` / `✓ Compiled / in 1.2s` | — / глиф≈severity / — / ✓ | **нет** | только substring |
| Docker `--timestamps` (как вызывает trun) | `2025-09-16T11:13:15.12Z <тело>` | RFC3339Nano ✓ / — / — / тело→рекурсия | **да (обёртка)** | strip ts + рекурсия в слой 2 |
| Python logging basicConfig | `ERROR:root:Some error` | — / 5 уровней ✓ / logger ✓ / ✓ | частичная | оппортунистически |
| Gunicorn error / access | `[2025-…] [1234] [INFO] …` / CLF-строка | ts ✓ / level ✓ / — / ✓ | **да (дефолты)** | парсить дефолты |
| Uvicorn default / access | `INFO:     Started …` / `INFO:     127.0.0.1:… - "GET / …" 200 OK` | — / level ✓ / — / ✓ (+client/status для access) | **да (дефолт)** | парсить; ts=arrival |
| logrus text (без TTY) / JSON | `time="…" level=warning msg="…"` | ts ✓ / level ✓ / — / ✓ (+k=v) | **да без TTY / да JSON** | парсить; TTY→substring |
| zap JSON / console | `{"level":"info","ts":…,"msg":…}` / `ts\tINFO\t…\tmsg` | ts ✓ / level ✓ / logger ✓ / ✓ | **да / частичная** | JSON всегда; console оппортунистически |
| tracing_subscriber Full/Json | `2022-…Z  INFO span{…}: target: msg` | ts ✓ / level ✓ / target ✓ / ✓ | частичная / **да (Json)** | оппортунистически; Json как JSON |
| env_logger | `[2017-11-09T02:12:24Z ERROR main] …` | ts ✓ / level ✓ / target ✓ / ✓ | частичная (дисклеймер автора!) | оппортунистически |
| Nginx/Apache access | combined/CLF-строка | ts ✓ / status→severity-эвристика / — / ✓ | **да** | парсить |
| Apache error | `[…] [core:error] [pid …] …` | ts ✓ / module:severity ✓ / module ✓ / ✓ | **да** | парсить |
| journalctl `-o json` / short | `{"MESSAGE":…,"PRIORITY":"6",…}` | всё ✓ / оппортунистически | **да (json)** / частичная | json парсить; short оппортунистически |
| Syslog RFC5424/3164 | `<165>1 2003-… host app …` | ts ✓ / severity 0-7 ✓ / app ✓ / ✓ | **да** | оппортунистически, низкий приоритет |

Общая стратегия для дизайна (не решение, а вывод исследования):

1. Порядок проб слоя 2: JSON-объект → Docker-ts-strip (если строка из
   `docker_logs`) → Monolog(a/b/c) → uvicorn → gunicorn-error →
   logrus-logfmt → tracing/env_logger/generic `[ts LEVEL target]` →
   python-basic → syslog → access-форматы → fallback substring.
2. Словари уровней/каналов нигде заранее не фиксировать (тикет требует вывод из
   потока) — только проверка «похоже на уровень» по объединённому множеству.
3. Multiline (трейсы, `Pretty`, ошибки компиляции Next/Vite) — отдельная тема,
   здесь только зафиксировано; схлопывание решает тикет 03-filter-semantics на
   основе корпуса из 02-log-corpus.
