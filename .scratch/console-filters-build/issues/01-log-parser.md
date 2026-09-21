# 01: Парсер логов (C++)

**What to build:** двухслойный парсер строки лога в структуру (timestamp-метка слоя 1, target, level, channel, message), покрытый юнит-тестами на строках из корпуса. Парсер — общий для Console history и MCP-поиска; UI и MCP строятся поверх него.

**Blocked by:** None (can start immediately).

**Status:** done (2026-09-16: src/logparser.h/.cpp + test_logparser, 19/19 PASS, lint чисто, trun-app собирается)

- [ ] Слой 1: префикс trun `[HH:mm:ss.zzz] label: body` разбирается всегда; level слоя 1 — stdout/stderr/system/error
- [ ] Слой 2: ordered probe chain из решения «Форматы логов» (JSON → Docker-ts → Monolog a/b/c → uvicorn → gunicorn → logrus → tracing/env_logger → python → syslog → access), все regex только после strip SGR
- [ ] Нераспознанная строка возвращает parsed=false и сырой текст (участвует только в substring-поиске)
- [ ] Словари level/channel нигде не хардкодятся сверх проверки «похоже на уровень»
- [ ] Юнит-тесты на выборке из `../console-filters/corpus-sample.log` (все 8 каналов + редкие уровни + жирный SQL); без новых third_party-зависимостей (QRegularExpression + QJsonDocument)
