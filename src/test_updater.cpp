#include <QTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "updater.h"

class TestUpdater : public QObject {
    Q_OBJECT

private slots:
    void testVersionFromTag();
    void testIsNewerVersion();
    void testPickMacZipUrl();
    void testParseReleaseNewer();
    void testParseReleaseUpToDate();
    void testParseReleaseErrors();
};

void TestUpdater::testVersionFromTag()
{
    QCOMPARE(Updater::versionFromTag(QStringLiteral("v0.2.0")),
             QVersionNumber(0, 2, 0));
    QCOMPARE(Updater::versionFromTag(QStringLiteral("0.1.0")),
             QVersionNumber(0, 1, 0));
    QCOMPARE(Updater::versionFromTag(QStringLiteral("V10.0.1")),
             QVersionNumber(10, 0, 1));
    QVERIFY(Updater::versionFromTag(QStringLiteral("")).isNull());
    QVERIFY(Updater::versionFromTag(QStringLiteral("latest")).isNull());
    QVERIFY(Updater::versionFromTag(QStringLiteral("v")).isNull());
}

void TestUpdater::testIsNewerVersion()
{
    QVERIFY(Updater::isNewerVersion(QStringLiteral("v0.2.0"), QStringLiteral("0.1.0")));
    QVERIFY(!Updater::isNewerVersion(QStringLiteral("v0.1.0"), QStringLiteral("0.1.0")));
    QVERIFY(!Updater::isNewerVersion(QStringLiteral("v0.1.0"), QStringLiteral("0.2.0")));
    QVERIFY(Updater::isNewerVersion(QStringLiteral("v1.0.0"), QStringLiteral("0.9.9")));
    QVERIFY(!Updater::isNewerVersion(QStringLiteral("garbage"), QStringLiteral("0.1.0")));
    QVERIFY(!Updater::isNewerVersion(QStringLiteral("v0.2.0"), QStringLiteral("garbage")));
}

static QJsonArray assetsWith(const QStringList &names)
{
    QJsonArray assets;
    for (const QString &name : names) {
        QJsonObject a;
        a.insert(QStringLiteral("name"), name);
        a.insert(QStringLiteral("browser_download_url"),
                 QStringLiteral("https://github.com/x/y/releases/download/v0.2.0/") + name);
        assets.append(a);
    }
    return assets;
}

void TestUpdater::testPickMacZipUrl()
{
    const QUrl url = Updater::pickMacZipUrl(
        assetsWith({QStringLiteral("trun-0.2.0-macos-arm64.dmg"),
                    QStringLiteral("trun-0.2.0-macos-arm64.zip"),
                    QStringLiteral("checksums.txt")}));
    QVERIFY(url.isValid());
    QVERIFY(url.toString().endsWith(QStringLiteral("-macos-arm64.zip")));

    QVERIFY(!Updater::pickMacZipUrl(
                    assetsWith({QStringLiteral("trun-0.2.0-macos-arm64.dmg")}))
                 .isValid());
    QVERIFY(!Updater::pickMacZipUrl(QJsonArray()).isValid());
}

static QJsonDocument releaseDoc(const QString &tag, const QJsonArray &assets)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("tag_name"), tag);
    obj.insert(QStringLiteral("body"), QStringLiteral("### Highlights\n- faster\n"));
    obj.insert(QStringLiteral("html_url"),
               QStringLiteral("https://github.com/maxvanceffer/trun/releases/tag/") + tag);
    obj.insert(QStringLiteral("assets"), assets);
    return QJsonDocument(obj);
}

void TestUpdater::testParseReleaseNewer()
{
    const Updater::ReleaseInfo info = Updater::parseReleasePayload(
        releaseDoc(QStringLiteral("v0.2.0"),
                   assetsWith({QStringLiteral("trun-0.2.0-macos-arm64.zip")})),
        QStringLiteral("0.1.0"));
    QVERIFY(info.ok);
    QVERIFY(info.newer);
    QCOMPARE(info.latestVersion, QStringLiteral("0.2.0"));
    QVERIFY(info.notes.contains(QStringLiteral("faster")));
    QVERIFY(info.zipUrl.isValid());
    QVERIFY(info.pageUrl.isValid());
}

void TestUpdater::testParseReleaseUpToDate()
{
    const Updater::ReleaseInfo info = Updater::parseReleasePayload(
        releaseDoc(QStringLiteral("v0.1.0"),
                   assetsWith({QStringLiteral("trun-0.1.0-macos-arm64.zip")})),
        QStringLiteral("0.1.0"));
    QVERIFY(info.ok);
    QVERIFY(!info.newer);
    QCOMPARE(info.latestVersion, QStringLiteral("0.1.0"));
}

void TestUpdater::testParseReleaseErrors()
{
    // GitHub error payload (e.g. rate limit).
    QJsonObject err;
    err.insert(QStringLiteral("message"), QStringLiteral("API rate limit exceeded"));
    Updater::ReleaseInfo info =
        Updater::parseReleasePayload(QJsonDocument(err), QStringLiteral("0.1.0"));
    QVERIFY(!info.ok);
    QVERIFY(!info.error.isEmpty());

    // Missing tag.
    QJsonObject noTag;
    noTag.insert(QStringLiteral("body"), QStringLiteral("notes"));
    info = Updater::parseReleasePayload(QJsonDocument(noTag), QStringLiteral("0.1.0"));
    QVERIFY(!info.ok);

    // Not an object at all.
    info = Updater::parseReleasePayload(QJsonDocument(QJsonArray()),
                                        QStringLiteral("0.1.0"));
    QVERIFY(!info.ok);

    // Newer release without a macOS zip: still newer, browser fallback.
    info = Updater::parseReleasePayload(
        releaseDoc(QStringLiteral("v0.2.0"),
                   assetsWith({QStringLiteral("trun-0.2.0-macos-arm64.dmg")})),
        QStringLiteral("0.1.0"));
    QVERIFY(info.ok);
    QVERIFY(info.newer);
    QVERIFY(!info.zipUrl.isValid());
    QVERIFY(info.pageUrl.isValid());
}

QTEST_MAIN(TestUpdater)
#include "test_updater.moc"
