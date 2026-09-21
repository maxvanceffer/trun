Type: research
Status: resolved
Blocked by:

## Question

Какие лог-форматы со стабильным структурированным выводом реально встречаются в проектах trun (кроме monolog/Symfony) и какие поля из них можно надёжно парсить в (timestamp, level, channel/target, message)?

Контекст: пример monolog-строки `serve: [Application] Sep 16 11:13:15 |DEBUG | DOCTRI ...` — дата `[14:13:29.692]`, канал `[Application]`, уровень `|DEBUG`, подканал `| DOCTRI`. Неизвестен заранее полный словарь каналов/уровней — парсер должен выводить их из потока. Вопрос пользователя: какие ещё популярные форматы стабильны (а не «что захотели, то вывели»), а какие — нет (и тогда для них фильтров нет, только plain-text).

Ожидаемый ответ: таблица формат → пример → поля (regex/грамматика) → стабильность (да/нет) → рекомендация: парсить / только substring. Покрыть минимум: monolog/Symfony (два слоя: trun-префикс `[time] target:` + monolog-тело), npm/vite/next/dev-серверы, Docker/colon-логи, Python logging/gunicorn/uvicorn, Go logrus/zap, Rust tracing/env_logger, Nginx/Apache access, systemd/journal. Факты — только по документации/реальным семплам, не выдумывать.

## Answer

Парсинг двухслойный: слой 1 — префикс trun `[HH:mm:ss.zzz] label: body` (стабилен 100%, level закрыт: stdout/stderr/system/error); слой 2 — тело дочернего процесса. Любой regex слоя 2 — только после strip SGR.

Парсить: Monolog LineFormatter, Symfony ConsoleFormatter, monolog-кастом `[channel] Mon DD HH:MM:SS |LEVEL|` (строка из постановки — именно он, 3-й альтернативой), Uvicorn default/access (ts=arrival), Gunicorn error/access-дефолты, logrus logfmt+JSON, zap JSON, Nginx/Apache access+Apache error, Docker `--timestamps`-обёртка (strip + рекурсия), journal `-o json`.

Оппортунистически (проба → fallback): Python basicConfig, tracing Full/Compact, env_logger (дисклеймер автора), zap console, syslog.

Только substring: Vite dev, Next.js dev (глиф→severity только после сверки с корпусом), npm — только level, TTY-варианты.

Порядок проб слоя 2: JSON → Docker-ts-strip → Monolog(a/b/c) → uvicorn → gunicorn-error → logrus-logfmt → tracing/env_logger → python-basic → syslog → access → fallback. Словари каналов/уровней не фиксировать, выводить из потока.

Полные findings (16 форматов, примеры, regex-кандидаты, источники): `../research-log-formats.md`.

## Comments
