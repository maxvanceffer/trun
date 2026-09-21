.pragma library
// PROTOTYPE (throwaway): mock Console history для макета фильтров.
// ageMin — «минут назад» (мок времени прибытия строки, слой 1).
// Длинные SQL собраны кодом, чтобы не таскать килобайты литералов.
function longConsoleSql() {
    var cols = [];
    for (var i = 1; i <= 90; ++i) cols.push("t0.col_" + i + " AS col_" + i);
    return "[14:13:29.701] serve: [Application] Sep 16 11:13:15 |DEBUG | DOCTRI Executing statement: SELECT "
        + cols.join(", ")
        + " FROM widget t0 WHERE t0.active = ? AND ((t0.website_id = CAST('4000000' AS SIGNED))) (parameters: array{\"1\":1})";
}
function longFileSql() {
    var cols = [];
    for (var i = 1; i <= 70; ++i) cols.push("t0.f_" + i + " AS f_" + i);
    return "[2026-08-13T08:29:43.123853+00:00] doctrine.DEBUG: Executing statement: SELECT "
        + cols.join(", ")
        + " FROM agency t0 WHERE t0.domain = ? LIMIT 1 {\"sql\":\"SELECT ...\"} []";
}
function entries() {
    return [
        { ageMin: 2, raw: "[14:13:29.926] serve: [Web Server ] Sep 16 14:13:15 |INFO | SERVER GET (200) /api/onboarding/status ip=\"127.0.0.1\"" },
        { ageMin: 3, raw: "[14:13:29.692] serve: [Application] Sep 16 11:13:15 |DEBUG | DOCTRI Executing statement: SELECT COUNT(*) FROM campaign t0 WHERE t0.type IN (?, ?, ?, ?, ?, ?) AND t0.active = ? (parameters: array{\"1\":\"experiment\",\"7\":1})" },
        { ageMin: 4, raw: "[vite] ready in 123 ms" },
        { ageMin: 5, raw: longConsoleSql() },
        { ageMin: 6, raw: "[2026-08-13T08:29:50.951098+00:00] security.DEBUG: Read existing security token from the session. {\"key\":\"_security_main\"} []" },
        { ageMin: 8, raw: "[14:13:29.832] serve: [Application] Sep 16 11:13:15 |DEBUG | SECURI Stored the security token in the session. key=\"_security_main\"" },
        { ageMin: 9, raw: "✓ Compiled / in 1.2s" },
        { ageMin: 12, raw: longFileSql() },
        { ageMin: 15, raw: "npm warn deprecated querystring@0.2.1: the querystring API is deprecated" },
        { ageMin: 25, raw: "[2026-08-13T08:13:37.233997+00:00] request.CRITICAL: Uncaught PHP Exception Doctrine\\DBAL\\Exception\\ConnectionException: \"An exception occurred in the driver: SQLSTATE[HY000] [2002] Connection refused\" []" },
        { ageMin: 40, raw: "[2026-08-13T08:13:37.089478+00:00] php.INFO: User Deprecated: Method Comparator::__construct() is considered internal []" },
        { ageMin: 70, raw: "[2026-08-13T08:30:01.000000+00:00] messenger.INFO: Received message App\\Message\\SyncAgencies []" }
    ];
}
