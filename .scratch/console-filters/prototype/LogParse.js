.pragma library
// PROTOTYPE (throwaway): двухслойный парсер + движок фильтров.
// Порядок проб слоя 2 — из решения тикета «Форматы логов»: (b) Symfony Console,
// (a) Monolog Line, (c) кастом [channel] Mon DD |LEVEL|, затем npm. Всё —
// только после strip SGR. Словари уровней/каналов НЕ фиксированы: чипсы
// вариантов строятся из observed() по факту потока.
function stripSgr(s) {
    return s.replace(/\x1b\[[0-9;]*m/g, "");
}
function normLevel(lv) {
    var u = (lv || "").toUpperCase();
    if (u === "WARN") return "WARNING";
    return u;
}
function isLevel(lv) {
    var u = normLevel(lv);
    return ["DEBUG", "INFO", "NOTICE", "WARNING", "ERROR", "CRITICAL", "ALERT", "EMERGENCY", "TRACE"].indexOf(u) >= 0;
}
function parseLine(raw) {
    var line = stripSgr(raw);
    var target = "", body = line;
    var m1 = line.match(/^\[([^\]]+)\]\s*([^:]+):\s*([\s\S]*)$/);
    if (m1) { target = m1[2].replace(/^\s+|\s+$/g, ""); body = m1[3]; }
    var mb = body.match(/^(\d{2}:\d{2}:\d{2})\s+([A-Za-z]+)\s+\[([^\]]+)\]\s*([\s\S]*)$/);
    if (mb && isLevel(mb[2]))
        return { ok: true, target: target, level: normLevel(mb[2]), channel: mb[3], message: mb[4] };
    var ma = body.match(/^\[([^\]]+)\]\s*([^.]+?)\.([A-Za-z]+):\s*([\s\S]*)$/);
    if (ma && isLevel(ma[3]))
        return { ok: true, target: target, level: normLevel(ma[3]), channel: ma[2], message: ma[4] };
    var mc = body.match(/^\[([^\]]+)\]\s*[A-Z][a-z]{2}\s+\d{1,2}\s+\d{2}:\d{2}:\d{2}\s*\|([A-Za-z]+)\s*\|\s*(?:([A-Za-z]+)\s+)?([\s\S]*)$/);
    if (mc && isLevel(mc[2]))
        return { ok: true, target: target, level: normLevel(mc[2]), channel: mc[3] || mc[1], message: mc[4] };
    var mn = line.match(/^npm\s+(error|warn|notice|http|info|verbose|silly|timing)\b\s*([\s\S]*)$/i);
    if (mn)
        return { ok: true, target: target, level: normLevel(mn[1]), channel: "", message: mn[2] };
    return { ok: false, target: target, level: "", channel: "", message: line };
}
function observed(items) {
    var lv = {}, ch = {}, i;
    for (i = 0; i < items.length; ++i) {
        if (!items[i].p.ok) continue;
        if (items[i].p.level !== "") lv[items[i].p.level] = 1;
        if (items[i].p.channel !== "") ch[items[i].p.channel] = 1;
    }
    return { levels: Object.keys(lv).sort(), channels: Object.keys(ch).sort() };
}
// f = {query, levels:[], channels:[], sinceMin(-1 = всё время)} — AND.
function applyFilters(items, f) {
    var q = (f.query || "").toLowerCase();
    var shown = [], hiddenUnparsed = 0, i, e;
    for (i = 0; i < items.length; ++i) {
        e = items[i];
        if (q !== "" && e.raw.toLowerCase().indexOf(q) < 0) continue;
        if (f.sinceMin >= 0 && e.ageMin > f.sinceMin) continue;
        if (!e.p.ok) {
            if (f.levels.length > 0 || f.channels.length > 0) { hiddenUnparsed++; continue; }
            shown.push(e);
            continue;
        }
        if (f.levels.length > 0 && f.levels.indexOf(e.p.level) < 0) continue;
        if (f.channels.length > 0 && f.channels.indexOf(e.p.channel) < 0) continue;
        shown.push(e);
    }
    return { shown: shown, total: items.length, hiddenUnparsed: hiddenUnparsed };
}
