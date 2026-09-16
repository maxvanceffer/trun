#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "projectservice.h"
#include "projectlistmodel.h"
#include "settings.h"
#include "treemodel.h"

// Multi-root workspace: scanning a second folder appends its projects
// instead of replacing the first folder's; rescanning one root drops
// only that root's stale projects.
class TestRoots : public QObject {
    Q_OBJECT

private slots:
    void testScanSecondRootMerges();
    void testRescanRootDropsOnlyItsStale();
    void testPersistRestoreMultipleRoots();
    void testLegacySingleRootMigrates();
    void testIsFolderKnown();
    void testTreeHasBothRoots();

private:
    void writeManifest(const QString &dir, const QString &name, const QByteArray &content);
    QByteArray npmManifest(const QByteArray &scripts);
};

void TestRoots::writeManifest(const QString &dir, const QString &name,
                              const QByteArray &content)
{
    QVERIFY(QDir().mkpath(dir));
    QFile f(dir + QLatin1Char('/') + name);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(content);
    f.close();
}

QByteArray TestRoots::npmManifest(const QByteArray &scripts)
{
    return "{\"name\": \"demo\", \"scripts\": {" + scripts + "}}";
}

void TestRoots::testScanSecondRootMerges()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    writeManifest(tmp.path() + "/a", "package.json",
                  npmManifest("\"dev\": \"vite\", \"build\": \"vite build\""));
    writeManifest(tmp.path() + "/b", "composer.json",
                  "{\"name\": \"demo/b\", \"scripts\": {\"serve\": \"php -S localhost:8000\"}}");

    ProjectListModel model;
    ProjectService service(&model);
    service.scanFolder(tmp.path() + "/a");
    QCOMPARE(service.projects().size(), 1);
    service.scanFolder(tmp.path() + "/b");
    QCOMPARE(service.projects().size(), 2);
    QCOMPARE(service.rootPaths(),
             QStringList({QDir::cleanPath(tmp.path() + "/a"),
                          QDir::cleanPath(tmp.path() + "/b")}));

    QSet<QString> manifests;
    for (const QJsonObject &p : service.projects())
        manifests.insert(p.value(QStringLiteral("manifest")).toString());
    QVERIFY(manifests.contains(QStringLiteral("package.json")));
    QVERIFY(manifests.contains(QStringLiteral("composer.json")));
}

void TestRoots::testRescanRootDropsOnlyItsStale()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    writeManifest(tmp.path() + "/a", "package.json",
                  npmManifest("\"dev\": \"vite\""));
    writeManifest(tmp.path() + "/b", "package.json",
                  npmManifest("\"serve\": \"node server.js\""));

    ProjectListModel model;
    ProjectService service(&model);
    service.scanFolder(tmp.path() + "/a");
    service.scanFolder(tmp.path() + "/b");
    QCOMPARE(service.projects().size(), 2);

    // Root A loses its manifest: rescanning A must drop A's project only.
    QVERIFY(QFile::remove(tmp.path() + "/a/package.json"));
    service.scanFolder(tmp.path() + "/a");
    QCOMPARE(service.projects().size(), 1);
    QCOMPARE(service.projects().first().value(QStringLiteral("project_path")).toString(),
             QDir::cleanPath(tmp.path() + "/b"));
}

void TestRoots::testPersistRestoreMultipleRoots()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QTemporaryDir config;
    QVERIFY(config.isValid());
    writeManifest(tmp.path() + "/a", "package.json",
                  npmManifest("\"dev\": \"vite\""));
    writeManifest(tmp.path() + "/b", "package.json",
                  npmManifest("\"serve\": \"node server.js\""));

    Settings settings(config.path() + "/trun.ini");
    {
        ProjectListModel model;
        ProjectService service(&model);
        service.setSettings(&settings);
        service.scanFolder(tmp.path() + "/a");
        service.scanFolder(tmp.path() + "/b");
        QCOMPARE(service.projects().size(), 2);
        QCOMPARE(settings.rootFolders().size(), 2);
    }
    {
        ProjectListModel model;
        ProjectService restarted(&model);
        restarted.setSettings(&settings);
        QVERIFY(restarted.restoreFromCache());
        QCOMPARE(restarted.projects().size(), 2);
        QCOMPARE(restarted.rootPaths().size(), 2);
    }
}

void TestRoots::testLegacySingleRootMigrates()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QTemporaryDir config;
    QVERIFY(config.isValid());
    writeManifest(tmp.path() + "/a", "package.json",
                  npmManifest("\"dev\": \"vite\""));

    Settings settings(config.path() + "/trun.ini");
    {
        ProjectListModel model;
        ProjectService service(&model);
        service.setSettings(&settings);
        service.scanFolder(tmp.path() + "/a");
        // Simulate a pre-multi-root cache: only the legacy keys exist.
        settings.remove(QStringLiteral("rootFolders"));
    }
    {
        ProjectListModel model;
        ProjectService restarted(&model);
        restarted.setSettings(&settings);
        QVERIFY(restarted.restoreFromCache());
        QCOMPARE(restarted.projects().size(), 1);
        QCOMPARE(restarted.rootPaths(),
                 QStringList({QDir::cleanPath(tmp.path() + "/a")}));
    }
}

void TestRoots::testIsFolderKnown()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    writeManifest(tmp.path() + "/a", "package.json",
                  npmManifest("\"dev\": \"vite\""));

    ProjectListModel model;
    ProjectService service(&model);
    QVERIFY(!service.isFolderKnown(tmp.path() + "/a"));
    service.scanFolder(tmp.path() + "/a");

    QVERIFY(service.isFolderKnown(tmp.path() + "/a"));
    QVERIFY(service.isFolderKnown(tmp.path() + "/a/sub")); // nested inside
    QVERIFY(service.isFolderKnown(tmp.path())); // parent of a known root
    QVERIFY(!service.isFolderKnown(tmp.path() + "/elsewhere"));
    QVERIFY(!service.isFolderKnown(QString()));
}

void TestRoots::testTreeHasBothRoots()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    writeManifest(tmp.path() + "/a", "package.json",
                  npmManifest("\"dev\": \"vite\""));
    writeManifest(tmp.path() + "/b", "package.json",
                  npmManifest("\"serve\": \"node server.js\""));

    ProjectListModel model;
    ProjectService service(&model);
    service.scanFolder(tmp.path() + "/a");
    service.scanFolder(tmp.path() + "/b");

    QmlTreeModel *tree = service.treeModel();
    QCOMPARE(tree->rowCount(QModelIndex()), 2);
    QSet<QString> topPaths;
    for (int r = 0; r < 2; ++r) {
        topPaths.insert(tree->data(tree->index(r, 0, QModelIndex()),
                                   QmlTreeModel::FolderPathRole).toString());
    }
    QVERIFY(topPaths.contains(QDir::cleanPath(tmp.path() + "/a")));
    QVERIFY(topPaths.contains(QDir::cleanPath(tmp.path() + "/b")));
}

QTEST_MAIN(TestRoots)
#include "test_roots.moc"
