# 03: MCP-поиск со структурой

**What to build:** ИИ через MCP осмысленно ищет в логах без перерасхода токенов: `read_log` (Console history) и `search_logs` (файлы) принимают `query` (substring CI) + `level` + `channel` + `sinceMinutes` с AND-логикой. Каждый ответ — plain-text полные строки с шапкой: наблюдаемые `observedLevels`/`observedChannels`, `matched`/`total`, флаг `truncated`. Кейс «ошибка DOCTRINE за последние 10 минут» работает одним вызовом с последующим сужением по счётчикам.

**Blocked by:** 01 (Парсер логов).

**Status:** done (2026-09-16: query/level/channel/sinceMinutes в read_log+search_logs, шапка matched/total/levels/channels/truncated, общий LogFilterHit-движок; test_mcpserver 11/11, test_logparser 21/21; легаси date-сохранён, monologMatch удалён)

- [ ] Одинаковые параметры и AND-семантика в обоих инструментах; капы как сейчас (maxResults 100/500, tail ≤ 2000), без байтового капа
- [ ] Единый probe chain из слайса 01 для консоли и файлов; fallback — substring; нераспознанные участвуют только в `query`
- [ ] JSON-объекты в выдаче не вводим; обрезки строк нет (полные строки сразу)
- [ ] Юнит-тесты на `handleMessage` (прецедент — существующие тесты McpServer)
