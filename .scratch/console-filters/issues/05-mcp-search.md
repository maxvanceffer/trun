Type: grilling
Status: resolved
Blocked by: 01, 03

## Question

Какой MCP API позволяет ИИ осмысленно искать в логах, не тратя токены — например «дай ошибку DOCTRINE за последние 10 минут»?

Разобрать с человеком (grilling + domain-modeling): расширять ли `read_log`/`search_logs` (сейчас: tail + monologMatch по файлам var/log|storage/logs|logs|log) structured-полями (level, channel/target, since/until, query, maxResults, формат выдачи lines vs objects, обрезка длинных SQL до N символов, счётчики без выдачи тел); лимиты/капы токенов; единый парсер для Console history и файловых логов или два; fallback для непарсящихся строк. Цель — решение о сигнатуре инструментов и формате ответа, не реализация.

Контекст из закрытой семантики (03): фильтры UI — текст-substring-CI + level + channel + время-пресеты (5/10/30/all) с AND; в UI обрезка ~500 с expand, в MCP — полные строки; жёсткое требование — MCP отдаёт ИИ накопленные списки level/channel (иначе «дай ошибку DOCTRINE» не сформулировать); паузы нет.

## Answer

Grilling, 1 быстрый раунд (2026-09-16; Q2 делегирован агенту). Решение:

1. Сигнатура: оба инструмента — `read_log` (Console history) и `search_logs` (файлы) — получают одинаковые `query` (substring CI) + `level` + `channel` + `sinceMinutes` (5/10/30/all) с AND-логикой; `tail`/`maxResults` как сейчас. Отдельного инструмента под словари нет.
2. Выдача: plain-text строки `[ts] target: message`, полные, без обрезки; JSON-объекты не вводим (ключи в каждой строке — лишний вес токенов; решение агента, человек согласился). Шапка каждого ответа: `observedLevels`, `observedChannels` (накопленные из выдачи/буфера), `matched`/`total`, флаг `truncated`.
3. Лимиты: нынешние капы без изменений (`maxResults` default 100 / max 500, `tail` ≤ 2000, файловые — tail 5MB); вместо слепого листания ИИ сужает фильтры по счётчикам. Байтового капа нет.
4. Парсер единый для консоли и файлов — ordered probe chain из решения по форматам (01); fallback — substring; нераспознанные строки участвуют только в `query` (консистентно с семантикой 03).
5. Мотивирующий кейс ложится прямо: «ошибку DOCTRINE за 10 минут» → `channel=DOCTRI/doctrine, level=ERROR (опц.), sinceMinutes=10` → строки + `matched/total`, дальше сужение.

## Comments
