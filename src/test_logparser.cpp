#include <QTest>
#include "logparser.h"

// Строки — из корпуса (.scratch/console-filters/corpus-sample.log):
// файловый dev.log + консольный serve-вывод + npm/vite/next.
class TestLogParser : public QObject {
    Q_OBJECT

private slots:
    void testLayer1ConsoleCustom();
    void testLayer1Securi();
    void testLayer1ServerGet();
    void testMonologFileLine();
    void testRequestCritical();
    void testSymfonyConsoleB();
    void testUvicorn();
    void testGunicornError();
    void testZapJson();
    void testDockerWrapped();
    void testApacheCombined();
    void testApacheError();
    void testNginxCombined();
    void testNpmWarnLevelOnly();
    void testViteAndNextUnparsed();
    void testSgrStripped();
    void testEmptyUnparsed();
    void testApplyFilterAnd();
    void testApplyFilterHidesUnparsed();
};

void TestLogParser::testLayer1ConsoleCustom()
{
    const ParsedLogLine e = LogParser::parse(
        QStringLiteral("[14:13:29.692] serve: [Application] Sep 16 11:13:15 |DEBUG | DOCTRI Executing statement: SELECT COUNT(*) FROM campaign t0"));
    QVERIFY(e.ok);
    QCOMPARE(e.target, QStringLiteral("serve"));
    QCOMPARE(e.level, QStringLiteral("DEBUG"));
    QCOMPARE(e.channel, QStringLiteral("DOCTRI"));
    QVERIFY(e.message.contains(QStringLiteral("SELECT COUNT(*)")));
}

void TestLogParser::testLayer1Securi()
{
    const ParsedLogLine e = LogParser::parse(
        QStringLiteral("[14:13:29.832] serve: [Application] Sep 16 11:13:15 |DEBUG | SECURI Stored the security token in the session."));
    QVERIFY(e.ok);
    QCOMPARE(e.level, QStringLiteral("DEBUG"));
    QCOMPARE(e.channel, QStringLiteral("SECURI"));
}

void TestLogParser::testLayer1ServerGet()
{
    const ParsedLogLine e = LogParser::parse(
        QStringLiteral("[14:13:29.926] serve: [Web Server ] Sep 16 14:13:15 |INFO | SERVER GET (200) /api/onboarding/status ip=\"127.0.0.1\""));
    QVERIFY(e.ok);
    QCOMPARE(e.level, QStringLiteral("INFO"));
    QCOMPARE(e.channel, QStringLiteral("SERVER"));
}

void TestLogParser::testMonologFileLine()
{
    const ParsedLogLine e = LogParser::parse(
        QStringLiteral("[2026-08-13T08:29:43.123853+00:00] doctrine.DEBUG: Executing statement: SELECT 1 {\"sql\":\"SELECT 1\"} []"));
    QVERIFY(e.ok);
    QVERIFY(e.target.isEmpty()); // файловый лог: слоя 1 нет
    QCOMPARE(e.level, QStringLiteral("DEBUG"));
    QCOMPARE(e.channel, QStringLiteral("doctrine"));
}

void TestLogParser::testRequestCritical()
{
    const ParsedLogLine e = LogParser::parse(
        QStringLiteral("[2026-08-13T08:13:37.233997+00:00] request.CRITICAL: Uncaught PHP Exception Connection refused []"));
    QVERIFY(e.ok);
    QCOMPARE(e.level, QStringLiteral("CRITICAL"));
    QCOMPARE(e.channel, QStringLiteral("request"));
}

void TestLogParser::testSymfonyConsoleB()
{
    const ParsedLogLine e = LogParser::parse(QStringLiteral("11:13:15 DEBUG     [app] Hello world"));
    QVERIFY(e.ok);
    QCOMPARE(e.level, QStringLiteral("DEBUG"));
    QCOMPARE(e.channel, QStringLiteral("app"));
    QCOMPARE(e.message, QStringLiteral("Hello world"));
}

void TestLogParser::testUvicorn()
{
    const ParsedLogLine e = LogParser::parse(QStringLiteral("INFO:     Started server process [1234]"));
    QVERIFY(e.ok);
    QCOMPARE(e.level, QStringLiteral("INFO"));
    QVERIFY(e.message.contains(QStringLiteral("Started server")));
}

void TestLogParser::testGunicornError()
{
    const ParsedLogLine e = LogParser::parse(
        QStringLiteral("[2025-09-16 11:13:15 +0000] [1234] [INFO] Starting gunicorn 23.0.0"));
    QVERIFY(e.ok);
    QCOMPARE(e.level, QStringLiteral("INFO"));
}

void TestLogParser::testZapJson()
{
    const ParsedLogLine e = LogParser::parse(
        QStringLiteral("{\"level\":\"info\",\"ts\":1726487595.123,\"caller\":\"srv/main.go:42\",\"msg\":\"listening\"}"));
    QVERIFY(e.ok);
    QCOMPARE(e.level, QStringLiteral("INFO"));
    QCOMPARE(e.message, QStringLiteral("listening"));
}

void TestLogParser::testDockerWrapped()
{
    const ParsedLogLine e = LogParser::parse(
        QStringLiteral("2025-09-16T11:13:15.123456789Z [2026-09-10T12:00:00+00:00] app.DEBUG: hello"));
    QVERIFY(e.ok);
    QCOMPARE(e.level, QStringLiteral("DEBUG"));
    QCOMPARE(e.channel, QStringLiteral("app"));
    QVERIFY(!e.layerTimestamp.isEmpty());
}

void TestLogParser::testApacheCombined()
{
    const ParsedLogLine e = LogParser::parse(
        QStringLiteral("127.0.0.1 - frank [10/Oct/2000:13:55:36 -0700] \"GET /a.gif HTTP/1.0\" 500 2326"));
    QVERIFY(e.ok);
    QCOMPARE(e.level, QStringLiteral("ERROR")); // эвристика по статусу
}

void TestLogParser::testApacheError()
{
    const ParsedLogLine e = LogParser::parse(
        QStringLiteral("[Fri Sep 09 10:42:29.902022 2011] [core:error] [pid 35708] [client 72.15.99.187] AH00124: boom"));
    QVERIFY(e.ok);
    QCOMPARE(e.level, QStringLiteral("ERROR"));
    QCOMPARE(e.channel, QStringLiteral("core"));
}

void TestLogParser::testNginxCombined()
{
    const ParsedLogLine e = LogParser::parse(
        QStringLiteral("127.0.0.1 - - [10/Oct/2000:13:55:36 -0700] \"GET / HTTP/1.1\" 200 1234 \"-\" \"curl\""));
    QVERIFY(e.ok);
    QCOMPARE(e.level, QStringLiteral("INFO"));
}

void TestLogParser::testNpmWarnLevelOnly()
{
    const ParsedLogLine e = LogParser::parse(
        QStringLiteral("npm warn deprecated querystring@0.2.1: the querystring API is deprecated"));
    QVERIFY(e.ok);
    QCOMPARE(e.level, QStringLiteral("WARNING")); // только уровень, остальное — substring
    QVERIFY(e.channel.isEmpty());
}

void TestLogParser::testViteAndNextUnparsed()
{
    const ParsedLogLine vite = LogParser::parse(QStringLiteral("[vite] ready in 123 ms"));
    QVERIFY(!vite.ok);
    const ParsedLogLine next = LogParser::parse(QString::fromUtf8("\xe2\x9c\x93 Compiled / in 1.2s"));
    QVERIFY(!next.ok);
}

void TestLogParser::testSgrStripped()
{
    const ParsedLogLine e = LogParser::parse(
        QStringLiteral("[14:13:29.692] serve: \x1b[32m[Application] Sep 16 11:13:15 |DEBUG | DOCTRI hi\x1b[0m"));
    QVERIFY(e.ok);
    QCOMPARE(e.channel, QStringLiteral("DOCTRI"));
    QVERIFY(!e.message.contains(QChar(0x1B)));
}

void TestLogParser::testEmptyUnparsed()
{
    QVERIFY(!LogParser::parse(QString()).ok);
    QVERIFY(!LogParser::parse(QStringLiteral("   ")).ok);
}

void TestLogParser::testApplyFilterAnd()
{
    const QString raw =
        QStringLiteral("[2026-09-10T10:01:00+00:00] app.ERROR: boom failed");
    const ParsedLogLine e = LogParser::parse(raw);
    QVERIFY(e.ok);
    QCOMPARE(LogParser::applyFilter(e, raw,
                                    {QStringLiteral("boom"), {QStringLiteral("ERROR")},
                                     {QStringLiteral("app")}}),
             LogFilterHit::Shown);
    QCOMPARE(LogParser::applyFilter(e, raw,
                                    {QStringLiteral("boom"), {QStringLiteral("INFO")}, {}}),
             LogFilterHit::Hidden);
    QCOMPARE(LogParser::applyFilter(e, raw,
                                    {QStringLiteral("zzz"), {}, {}}),
             LogFilterHit::Hidden);
    QCOMPARE(LogParser::applyFilter(e, raw, {}), LogFilterHit::Shown);
}

void TestLogParser::testApplyFilterHidesUnparsed()
{
    const ParsedLogLine e = LogParser::parse(QStringLiteral("[vite] ready in 123 ms"));
    QVERIFY(!e.ok);
    QCOMPARE(LogParser::applyFilter(e, QStringLiteral("[vite] ready"),
                                    {QStringLiteral("ready"), {}, {}}),
             LogFilterHit::Shown);
    QCOMPARE(LogParser::applyFilter(e, QStringLiteral("[vite] ready"),
                                    {{}, {QStringLiteral("ERROR")}, {}}),
             LogFilterHit::HiddenUnparsed);
}

QTEST_MAIN(TestLogParser)
#include "test_logparser.moc"
