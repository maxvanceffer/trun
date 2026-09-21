#include <QTest>
#include "logmodel.h"

class TestLogModel : public QObject {
    Q_OBJECT

private slots:
    void testAddCounts();
    void testLevelFilter();
    void testChannelFilter();
    void testQueryIncludesUnparsed();
    void testResetRestores();
    void testObserved();
    void testVisibleMapping();
    void testCapKeepsMappingValid();
};

static const char *kDebug =
    "[2026-08-13T08:29:43.123853+00:00] doctrine.DEBUG: Executing statement: SELECT 1 []";
static const char *kCritical =
    "[2026-08-13T08:13:37.233997+00:00] request.CRITICAL: Uncaught PHP Exception boom []";
static const char *kVite = "[vite] ready in 123 ms";

void TestLogModel::testAddCounts()
{
    LogModel m;
    m.add(QStringLiteral("stdout"), QStringLiteral("serve"), QString::fromUtf8(kDebug));
    m.add(QStringLiteral("stdout"), QStringLiteral("serve"), QString::fromUtf8(kVite));
    QCOMPARE(m.count(), 2);
    QCOMPARE(m.shownCount(), 2);
    QCOMPARE(m.totalCount(), 2);
    QCOMPARE(m.hiddenUnparsedCount(), 0);
}

void TestLogModel::testLevelFilter()
{
    LogModel m;
    m.add(QStringLiteral("stdout"), QStringLiteral("serve"), QString::fromUtf8(kDebug));
    m.add(QStringLiteral("stdout"), QStringLiteral("serve"), QString::fromUtf8(kCritical));
    m.add(QStringLiteral("stdout"), QStringLiteral("serve"), QString::fromUtf8(kVite));
    m.setFilters({}, {QStringLiteral("CRITICAL")}, {}, -1);
    QCOMPARE(m.shownCount(), 1);
    QCOMPARE(m.hiddenUnparsedCount(), 1); // vite скрыт как нераспознанный
    QVERIFY(m.get(0).value(QStringLiteral("message")).toString().contains(QStringLiteral("boom")));
}

void TestLogModel::testChannelFilter()
{
    LogModel m;
    m.add(QStringLiteral("stdout"), QStringLiteral("serve"), QString::fromUtf8(kDebug));
    m.add(QStringLiteral("stdout"), QStringLiteral("serve"), QString::fromUtf8(kCritical));
    m.setFilters({}, {}, {QStringLiteral("doctrine")}, -1);
    QCOMPARE(m.shownCount(), 1);
    QVERIFY(m.get(0).value(QStringLiteral("message")).toString().contains(QStringLiteral("SELECT 1")));
}

void TestLogModel::testQueryIncludesUnparsed()
{
    LogModel m;
    m.add(QStringLiteral("stdout"), QStringLiteral("serve"), QString::fromUtf8(kDebug));
    m.add(QStringLiteral("stdout"), QStringLiteral("serve"), QString::fromUtf8(kVite));
    m.setFilters(QStringLiteral("VITE"), {}, {}, -1); // case-insensitive
    QCOMPARE(m.shownCount(), 1);
    QCOMPARE(m.hiddenUnparsedCount(), 0);
}

void TestLogModel::testResetRestores()
{
    LogModel m;
    m.add(QStringLiteral("stdout"), QStringLiteral("serve"), QString::fromUtf8(kDebug));
    m.add(QStringLiteral("stdout"), QStringLiteral("serve"), QString::fromUtf8(kVite));
    m.setFilters(QStringLiteral("zzz-no-match"), {}, {}, -1);
    QCOMPARE(m.shownCount(), 0);
    m.resetFilters();
    QCOMPARE(m.shownCount(), 2);
    QCOMPARE(m.hiddenUnparsedCount(), 0);
}

void TestLogModel::testObserved()
{
    LogModel m;
    m.add(QStringLiteral("stdout"), QStringLiteral("serve"), QString::fromUtf8(kDebug));
    m.add(QStringLiteral("stdout"), QStringLiteral("serve"), QString::fromUtf8(kCritical));
    m.add(QStringLiteral("stdout"), QStringLiteral("serve"), QString::fromUtf8(kVite));
    QVERIFY(m.observedLevels().contains(QStringLiteral("DEBUG")));
    QVERIFY(m.observedLevels().contains(QStringLiteral("CRITICAL")));
    QVERIFY(m.observedChannels().contains(QStringLiteral("doctrine")));
    QVERIFY(m.observedChannels().contains(QStringLiteral("request")));
}

void TestLogModel::testVisibleMapping()
{
    LogModel m;
    m.add(QStringLiteral("stdout"), QStringLiteral("serve"), QString::fromUtf8(kDebug));
    m.add(QStringLiteral("stdout"), QStringLiteral("serve"), QString::fromUtf8(kCritical));
    m.setFilters({}, {QStringLiteral("CRITICAL")}, {}, -1);
    QCOMPARE(m.count(), 1);
    QCOMPARE(m.rowCount(), 1);
    QVERIFY(m.data(m.index(0, 0), LogModel::MessageRole).toString().contains(QStringLiteral("boom")));
    QVERIFY(m.get(1).isEmpty()); // вне диапазона — пусто
}

void TestLogModel::testCapKeepsMappingValid()
{
    LogModel m;
    for (int i = 0; i < 1005; ++i)
        m.add(QStringLiteral("stdout"), QStringLiteral("serve"),
              QStringLiteral("[2026-08-13T08:29:43.123853+00:00] doctrine.DEBUG: line %1 []").arg(i));
    QCOMPARE(m.totalCount(), 1000);
    QCOMPARE(m.count(), 1000);
    QVERIFY(m.get(0).value(QStringLiteral("message")).toString().contains(QStringLiteral("line 5")));
}

QTEST_MAIN(TestLogModel)
#include "test_logmodel.moc"
