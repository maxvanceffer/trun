#include <QTest>
#include <QSignalSpy>
#include <QDir>
#include <QJsonDocument>
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QFileInfo>
#include <algorithm>
#include "projectservice.h"
#include "commandexecutor.h"
#include "projectlistmodel.h"
#include "settings.h"

class TestProjectService : public QObject {
    Q_OBJECT

private:
    QString m_fixtureDir;

private slots:
    void initTestCase();
    void testScanFixtures();
    void testScanPersonizely();
    void testProjectFields();
    void testCommands();
    void testSelectProject();
    void testProjectCount();
    void testCachePersistAndRestore();
    void testCacheDropsMissingProjects();
    void testCacheClearsWhenRootGone();
    void testCacheVersionMismatchRescans();
    void testSelectProjectByCompositeId();
    void testRestoreThenSelect();
    void testComposerScripts();
    void testManifestNameFromPackageJson();
    void testSameFolderMultipleManifests();
    void testTreeGroupsManifestsUnderOneFolder();
    void testRootFolderWithOnlyManifestStaysVisible();
    void testSelectFolderGroupsManifests();
    void testFolderCrumbs();
    void testFolderDisplayNameGitFallback();
    void testAddCustomCommandAttachesToProject();
    void testAddCustomCommandPseudoProject();
    void testAddCustomCommandUniqueIds();
    void testAddCustomCommandRejectsEmpty();
    void testCustomCommandsSurviveRescanAndStripCache();
    void testPinsPersistAndToggle();
    void testPinsPrunedOnScan();
    void testRunCommandEffective();
    void testRecentCommandsRecordedOnStart();
    void testSameCommandIdAcrossProjectsTrackedSeparately();
    void testStaticProgramDetection();
    void testNpmChainAndWrapperDetection();
    void testRescanDropsStaleSelection();
};

void TestProjectService::initTestCase() {
    // Use stable fixture directory (pre-created in repo, never deleted)
    QByteArray env = qgetenv("TRUN_FIXTURES");
    if (!env.isEmpty()) {
        m_fixtureDir = QString(env) + "/test/fixtures/trun-root-test";
    } else {
        m_fixtureDir = QStringLiteral("/Users/maxxxtraxxx/projects/trun/test/fixtures/trun-root-test");
    }
    qDebug() << "Fixture directory:" << m_fixtureDir;
}

void TestProjectService::testScanFixtures() {
    ProjectListModel model;
    ProjectService service(&model);
    
    // Verify fixtures exist before scanning
    QVERIFY2(QDir(m_fixtureDir).exists(), "Fixture directory missing");
    QVERIFY2(QFile::exists(m_fixtureDir + "/package.json"), "Root package.json missing");
    QVERIFY2(QFile::exists(m_fixtureDir + "/sublib/package.json"), "Sublib package.json missing");
    QVERIFY2(QFile::exists(m_fixtureDir + "/rustlib/Cargo.toml"), "Cargo.toml missing");
    QVERIFY2(QDir(m_fixtureDir + "/node_modules").exists(), "node_modules dir missing");

    service.scanFolder(m_fixtureDir);

    QList<QJsonObject> projects = service.projects();
    qDebug() << "Fixture scan found:" << projects.size() << "projects";
    for (const auto &p : projects) {
        qDebug() << "  -" << p.value("name").toString() << "(" << p.value("manifest").toString() << ")";
    }

    // Should find exactly 3: sublib, rustlib, root. node_modules must be skipped.
    QVERIFY2(projects.size() == 3,
             ("Expected exactly 3 projects, got " + QString::number(projects.size())).toStdString().c_str());
}

void TestProjectService::testScanPersonizely() {
    ProjectListModel model;
    ProjectService service(&model);
    
    QString personizelyPath = "/Users/maxxxtraxxx/projects/personizely";
    if (!QDir(personizelyPath).exists()) {
        QSKIP("personizely project not found, skipping test");
    }

    // Convert URL-style path to local path (as FolderDialog does)
    QString rootPath = QUrl("file://" + personizelyPath).toLocalFile();
    qDebug() << "Scanning:" << rootPath << "exists:" << QDir(rootPath).exists();

    service.scanFolder(rootPath);

    QList<QJsonObject> projects = service.projects();
    qDebug() << "Found projects:" << projects.size();

    // Should find at least 5 projects (personizely has 5 package.json)
    QVERIFY2(projects.size() >= 5,
             ("Expected >= 5 projects, got " + QString::number(projects.size())).toStdString().c_str());

    // Print all found project names for debugging
    for (const auto &proj : projects) {
        qDebug() << "  -" << proj.value("name").toString()
                 << "(" << proj.value("manifest").toString() << ")";
    }
}

void TestProjectService::testProjectFields() {
    ProjectListModel model;
    ProjectService service(&model);
    
    service.scanFolder(m_fixtureDir);

    QList<QJsonObject> projects = service.projects();
    QVERIFY2(projects.size() >= 2,
             ("Expected >= 2 projects in fixture dir, got " + QString::number(projects.size())).toStdString().c_str());

    // Check each project has required fields
    for (const auto &proj : projects) {
        QVERIFY2(proj.contains("id"), "Project missing 'id' field");
        QVERIFY2(proj.contains("project_path"), "Project missing 'project_path' field");
        QVERIFY2(proj.contains("name"), "Project missing 'name' field");
        QVERIFY2(proj.contains("manifest"), "Project missing 'manifest' field");
        QVERIFY2(proj.contains("commands"), "Project missing 'commands' field");

        QVERIFY2(!proj["id"].toString().isEmpty(), "Project 'id' is empty");
        QVERIFY2(!proj["project_path"].toString().isEmpty(), "Project 'project_path' is empty");
        QVERIFY2(!proj["name"].toString().isEmpty(), "Project 'name' is empty");
    }
}

void TestProjectService::testCommands() {
    ProjectListModel model;
    ProjectService service(&model);
    
    service.scanFolder(m_fixtureDir);

    QList<QJsonObject> projects = service.projects();
    qDebug() << "testCommands: found" << projects.size() << "projects";
    for (const auto &p : projects) {
        qDebug() << "  -" << p.value("name").toString();
    }

    // Find the root project (should have build, test, dev scripts)
    QJsonObject rootProject;
    for (const auto &proj : projects) {
        if (proj.value("name").toString() == "trun-root-test") {
            rootProject = proj;
            break;
        }
    }

    QVERIFY2(!rootProject.isEmpty(), "Root project not found");

    QJsonArray cmds = rootProject.value("commands").toArray();
    QVERIFY2(cmds.size() >= 3,
             ("Root project should have >= 3 commands, got " + QString::number(cmds.size())).toStdString().c_str());

    // Check command structure
    for (const auto &v : cmds) {
        QJsonObject cmd = v.toObject();
        QVERIFY2(cmd.contains("id"), "Command missing 'id'");
        QVERIFY2(cmd.contains("label"), "Command missing 'label'");
        QVERIFY2(cmd.contains("command"), "Command missing 'command'");
        QVERIFY2(cmd.contains("args"), "Command missing 'args'");
    }
}

void TestProjectService::testSelectProject() {
    ProjectListModel model;
    ProjectService service(&model);
    
    service.scanFolder(m_fixtureDir);

    QList<QJsonObject> projects = service.projects();
    QVERIFY2(projects.size() > 0, "No projects found");

    // Select the first project
    service.selectProject(projects.first()["id"].toString());

    QVERIFY2(!service.activeProject().isEmpty(), "Active project is empty after selection");
    QVERIFY2(!service.activeProjectCommands().isEmpty(), "Active project commands is empty");

    qDebug() << "Selected project:" << service.activeProject()["name"].toString();
    qDebug() << "Commands count:" << service.activeProjectCommands().size();
}

void TestProjectService::testProjectCount() {
    ProjectListModel model;
    ProjectService service(&model);

    QCOMPARE(service.projectCount(), 0);
    service.scanFolder(m_fixtureDir);
    QVERIFY(service.projectCount() > 0);
    QCOMPARE(service.projectCount(), service.projects().size());
}

static bool writeManifest(const QString &path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    return f.write("{}") > 0;
}

void TestProjectService::testCachePersistAndRestore() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(writeManifest(tmp.path() + "/a/package.json"));
    QVERIFY(writeManifest(tmp.path() + "/b/package.json"));

    Settings settings(tmp.path() + "/trun.ini");
    ProjectListModel model;
    ProjectService service(&model);
    service.setSettings(&settings);
    service.scanFolder(tmp.path());
    QCOMPARE(service.projectCount(), 2);
    QVERIFY(settings.hasWorkspace());

    ProjectListModel model2;
    ProjectService restored(&model2);
    restored.setSettings(&settings);
    QVERIFY(restored.restoreFromCache());
    QCOMPARE(restored.projectCount(), 2);
}

void TestProjectService::testCacheDropsMissingProjects() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(writeManifest(tmp.path() + "/a/package.json"));
    QVERIFY(writeManifest(tmp.path() + "/b/package.json"));

    Settings settings(tmp.path() + "/trun.ini");
    ProjectListModel model;
    ProjectService service(&model);
    service.setSettings(&settings);
    service.scanFolder(tmp.path());
    QCOMPARE(service.projectCount(), 2);

    QVERIFY(QFile::remove(tmp.path() + "/b/package.json"));

    ProjectListModel model2;
    ProjectService restored(&model2);
    restored.setSettings(&settings);
    QVERIFY(restored.restoreFromCache());
    QCOMPARE(restored.projectCount(), 1);
    QCOMPARE(restored.projects().first()["name"].toString(), QString("a"));
}

void TestProjectService::testCacheClearsWhenRootGone() {
    QTemporaryDir configDir;
    QVERIFY(configDir.isValid());
    Settings settings(configDir.path() + "/trun.ini");

    QString root;
    {
        QTemporaryDir workspace;
        QVERIFY(workspace.isValid());
        workspace.setAutoRemove(false);
        root = workspace.path();
        QVERIFY(writeManifest(root + "/package.json"));

        ProjectListModel model;
        ProjectService service(&model);
        service.setSettings(&settings);
        service.scanFolder(root);
        QCOMPARE(service.projectCount(), 1);
    }
    QDir(root).removeRecursively();

    ProjectListModel model2;
    ProjectService restored(&model2);
    restored.setSettings(&settings);
    QVERIFY(!restored.restoreFromCache());
    QCOMPARE(restored.projectCount(), 0);
    QVERIFY(!settings.hasWorkspace());
}

static bool writeJson(const QString &path, const QByteArray &json)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    return f.write(json) == json.size();
}

static QModelIndex findChildByName(QAbstractItemModel *model, const QModelIndex &parent, const QString &name)
{
    for (int i = 0; i < model->rowCount(parent); ++i) {
        const QModelIndex idx = model->index(i, 0, parent);
        if (model->data(idx, Qt::DisplayRole).toString() == name)
            return idx;
    }
    return {};
}

void TestProjectService::testCacheVersionMismatchRescans() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(writeJson(tmp.path() + "/a/package.json", R"({"name":"a"})"));

    Settings settings(tmp.path() + "/trun.ini");
    ProjectListModel model;
    ProjectService service(&model);
    service.setSettings(&settings);
    service.scanFolder(tmp.path());
    QCOMPARE(service.projectCount(), 1);

    // Simulate an old cache: drop the version, add a project on disk
    settings.set("cacheVersion", 0);
    QVERIFY(writeJson(tmp.path() + "/b/package.json", R"({"name":"b"})"));

    ProjectListModel model2;
    ProjectService restarted(&model2);
    restarted.setSettings(&settings);
    QVERIFY(restarted.restoreFromCache());
    QCOMPARE(restarted.projectCount(), 2);
}

void TestProjectService::testSelectProjectByCompositeId() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(writeJson(tmp.path() + "/backend/package.json", R"({"name":"acme-js","scripts":{"build":"tsc"}})"));
    QVERIFY(writeJson(tmp.path() + "/backend/composer.json", R"({"name":"acme/php"})"));

    ProjectListModel model;
    ProjectService service(&model);
    service.scanFolder(tmp.path());
    QCOMPARE(service.projectCount(), 2);

    service.selectProject(tmp.path() + "/backend/composer.json");
    QCOMPARE(service.activeProject()["manifest"].toString(), QString("composer.json"));
    QCOMPARE(service.activeProject()["name"].toString(), QString("acme/php"));

    service.selectProject(tmp.path() + "/backend/package.json");
    QCOMPARE(service.activeProject()["manifest"].toString(), QString("package.json"));
    QCOMPARE(service.activeProject()["name"].toString(), QString("acme-js"));
    QVERIFY(!service.activeProjectCommands().isEmpty());
}

void TestProjectService::testRestoreThenSelect() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(writeJson(tmp.path() + "/backend/package.json",
                      R"({"name":"acme-js","scripts":{"build":"tsc"}})"));
    QVERIFY(writeJson(tmp.path() + "/backend/composer.json", R"({"name":"acme/php"})"));

    Settings settings(tmp.path() + "/trun.ini");

    ProjectListModel model;
    ProjectService service(&model);
    service.setSettings(&settings);
    service.scanFolder(tmp.path());
    QCOMPARE(service.projectCount(), 2);

    // Fresh instance, like an app restart: restore from cache only
    ProjectListModel model2;
    ProjectService restarted(&model2);
    restarted.setSettings(&settings);
    QVERIFY(restarted.restoreFromCache());
    QCOMPARE(restarted.projectCount(), 2);

    // Select using the id the tree view actually exposes (path + "/" + manifest)
    const QString treeId = tmp.path() + "/backend/package.json";
    restarted.selectProject(treeId);
    QCOMPARE(restarted.activeProject()["manifest"].toString(), QString("package.json"));
    QCOMPARE(restarted.activeProject()["name"].toString(), QString("acme-js"));
    QVERIFY(!restarted.activeProjectCommands().isEmpty());
}

void TestProjectService::testComposerScripts() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(writeJson(tmp.path() + "/backend/composer.json", R"({
        "name": "acme/php",
        "scripts": {
            "serve": "symfony serve",
            "phpstan": "vendor/bin/phpstan analyse",
            "post-install-cmd": ["@auto-scripts"],
            "auto-scripts": {"cache:clear": "symfony-cmd"}
        }
    })"));

    ProjectListModel model;
    ProjectService service(&model);
    service.scanFolder(tmp.path());
    QCOMPARE(service.projectCount(), 1);

    const QJsonArray cmds = service.projects().first()["commands"].toArray();
    QStringList labels;
    for (const auto &v : cmds)
        labels << v.toObject()["label"].toString();

    QVERIFY(labels.contains("serve"));
    QVERIFY(labels.contains("phpstan"));
    QVERIFY(labels.contains("post-install-cmd"));
    QVERIFY(!labels.contains("auto-scripts"));

    const auto it = std::find_if(cmds.begin(), cmds.end(), [](const QJsonValue &v) {
        return v.toObject()["label"].toString() == "serve";
    });
    QVERIFY(it != cmds.end());
    const QJsonObject serve = it->toObject();
    QCOMPARE(serve["command"].toString(), QString("composer"));
    QCOMPARE(serve["args"].toArray().first().toString(), QString("serve"));
}

void TestProjectService::testManifestNameFromPackageJson() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(writeJson(tmp.path() + "/package.json", R"({"name":"from-manifest"})"));

    ProjectListModel model;
    ProjectService service(&model);
    service.scanFolder(tmp.path());
    QCOMPARE(service.projectCount(), 1);
    QCOMPARE(service.projects().first()["name"].toString(), QString("from-manifest"));
}

void TestProjectService::testSameFolderMultipleManifests() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(writeJson(tmp.path() + "/backend/package.json", R"({"name":"acme-js"})"));
    QVERIFY(writeJson(tmp.path() + "/backend/composer.json", R"({"name":"acme/php"})"));

    ProjectListModel model;
    ProjectService service(&model);
    service.scanFolder(tmp.path());
    QCOMPARE(service.projectCount(), 2);

    QStringList names;
    QStringList manifests;
    for (const auto &p : service.projects()) {
        names << p["name"].toString();
        manifests << p["manifest"].toString();
        QCOMPARE(p["project_path"].toString(), tmp.path() + "/backend");
        QVERIFY(p["id"].toString().endsWith("/" + p["manifest"].toString()));
    }
    QVERIFY(names.contains("acme-js"));
    QVERIFY(names.contains("acme/php"));
    QVERIFY(manifests.contains("package.json"));
    QVERIFY(manifests.contains("composer.json"));
}

void TestProjectService::testTreeGroupsManifestsUnderOneFolder() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(writeJson(tmp.path() + "/backend/package.json", R"({"name":"acme-js"})"));
    QVERIFY(writeJson(tmp.path() + "/backend/composer.json", R"({"name":"acme/php"})"));
    QVERIFY(writeJson(tmp.path() + "/editor/package.json", R"({"name":"acme-editor"})"));

    ProjectListModel model;
    ProjectService service(&model);
    service.scanFolder(tmp.path());

    QAbstractItemModel *tree = service.treeModel();
    QCOMPARE(tree->rowCount(), 1); // single visible root node

    const QModelIndex rootIdx = tree->index(0, 0);
    QVERIFY(rootIdx.isValid());
    QCOMPARE(tree->data(rootIdx, QmlTreeModel::ItemTypeRole).toString(), QString("folder"));
    QCOMPARE(tree->rowCount(rootIdx), 2);

    const QModelIndex backend = findChildByName(tree, rootIdx, "backend");
    QVERIFY(backend.isValid());
    QCOMPARE(tree->data(backend, QmlTreeModel::ItemTypeRole).toString(), QString("folder"));
    QCOMPARE(tree->data(backend, QmlTreeModel::HasManifestsRole).toBool(), true);
    // Leaf folders with manifests have no children: no project rows, no entry
    QCOMPARE(tree->rowCount(backend), 0);

    const QModelIndex editor = findChildByName(tree, rootIdx, "editor");
    QVERIFY(editor.isValid());
    QCOMPARE(tree->data(editor, QmlTreeModel::HasManifestsRole).toBool(), true);
    QCOMPARE(tree->rowCount(editor), 0);
}

void TestProjectService::testRootFolderWithOnlyManifestStaysVisible() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(writeJson(tmp.path() + "/package.json", R"({"name":"solo"})"));

    ProjectListModel model;
    ProjectService service(&model);
    service.scanFolder(tmp.path());

    // The root itself holds the only manifest: visible root row plus
    // one manifests entry beneath it
    QAbstractItemModel *tree = service.treeModel();
    QCOMPARE(tree->rowCount(), 1);
    const QModelIndex rootIdx = tree->index(0, 0);
    QVERIFY(rootIdx.isValid());
    QCOMPARE(tree->data(rootIdx, QmlTreeModel::ItemTypeRole).toString(), QString("folder"));
    QCOMPARE(tree->data(rootIdx, QmlTreeModel::HasManifestsRole).toBool(), true);
    QCOMPARE(tree->rowCount(rootIdx), 1);

    const QModelIndex entryIdx = tree->index(0, 0, rootIdx);
    QVERIFY(entryIdx.isValid());
    QCOMPARE(tree->data(entryIdx, QmlTreeModel::ItemTypeRole).toString(), QString("manifests"));
    QCOMPARE(tree->data(entryIdx, QmlTreeModel::ManifestRole).toString(), QString("package.json"));

    service.selectFolder(tmp.path());
    QCOMPARE(service.activeFolderProjects().size(), 1);
    QCOMPARE(service.folderCrumbs(tmp.path()).size(), 1);
}

void TestProjectService::testSelectFolderGroupsManifests() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(writeJson(tmp.path() + "/backend/package.json", R"({"name":"acme-js"})"));
    QVERIFY(writeJson(tmp.path() + "/backend/composer.json", R"({"name":"acme/php"})"));
    QVERIFY(writeJson(tmp.path() + "/editor/package.json", R"({"name":"acme-editor"})"));

    ProjectListModel model;
    ProjectService service(&model);
    service.scanFolder(tmp.path());

    service.selectFolder(tmp.path() + "/backend");
    QCOMPARE(service.activeFolder().value("path").toString(),
             QDir::cleanPath(tmp.path() + "/backend"));
    QCOMPARE(service.activeFolder().value("name").toString(), QString("backend"));
    QCOMPARE(service.activeFolderProjects().size(), 2);
    QSet<QString> manifests;
    for (const auto &p : service.activeFolderProjects())
        manifests.insert(p.value("manifest").toString());
    QCOMPARE(manifests, QSet<QString>({"package.json", "composer.json"}));

    service.selectFolder(tmp.path() + "/editor");
    QCOMPARE(service.activeFolderProjects().size(), 1);

    // Unknown and empty paths are ignored
    service.selectFolder(tmp.path() + "/ghost");
    QCOMPARE(service.activeFolder().value("path").toString(),
             QDir::cleanPath(tmp.path() + "/editor"));
    service.selectFolder(QString());
    QCOMPARE(service.activeFolder().value("path").toString(),
             QDir::cleanPath(tmp.path() + "/editor"));

    // Rescan keeps the folder selection with refreshed projects
    service.scanFolder(tmp.path());
    QCOMPARE(service.activeFolder().value("path").toString(),
             QDir::cleanPath(tmp.path() + "/editor"));
    QCOMPARE(service.activeFolderProjects().size(), 1);
}

void TestProjectService::testFolderCrumbs() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(writeJson(tmp.path() + "/personizely/backend/package.json", R"({"name":"be"})"));

    ProjectListModel model;
    ProjectService service(&model);
    service.scanFolder(tmp.path());

    const QString backend = QDir::cleanPath(tmp.path() + "/personizely/backend");
    const QVariantList crumbs = service.folderCrumbs(backend);
    QCOMPARE(crumbs.size(), 2);
    QCOMPARE(crumbs.at(0).toMap().value("name").toString(), QString("personizely"));
    QCOMPARE(crumbs.at(0).toMap().value("path").toString(),
             QDir::cleanPath(tmp.path() + "/personizely"));
    QCOMPARE(crumbs.at(1).toMap().value("name").toString(), QString("backend"));
    QCOMPARE(crumbs.at(1).toMap().value("path").toString(), backend);

    // The root itself is a single crumb
    const QVariantList rootCrumbs = service.folderCrumbs(tmp.path());
    QCOMPARE(rootCrumbs.size(), 1);
    QCOMPARE(rootCrumbs.at(0).toMap().value("path").toString(), QDir::cleanPath(tmp.path()));

    // Outside known roots: single fallback segment
    const QVariantList outside = service.folderCrumbs(QStringLiteral("/nope/nada"));
    QCOMPARE(outside.size(), 1);
    QCOMPARE(outside.at(0).toMap().value("path").toString(), QString("/nope/nada"));

    QVERIFY(service.folderCrumbs(QString()).isEmpty());
}

void TestProjectService::testFolderDisplayNameGitFallback() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    ProjectListModel model;
    ProjectService service(&model);
    QCOMPARE(service.folderDisplayName(tmp.path() + "/plain"), QString("plain"));

    QVERIFY(QDir(tmp.path()).mkpath("repo/.git"));
    QFile config(tmp.path() + "/repo/.git/config");
    QVERIFY(config.open(QIODevice::WriteOnly));
    config.write("[remote \"origin\"]\n\turl = git@github.com:acme/widget.git\n");
    config.close();
    QCOMPARE(service.folderDisplayName(tmp.path() + "/repo"), QString("widget"));
}

void TestProjectService::testAddCustomCommandAttachesToProject()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir(tmp.path()).mkpath(QStringLiteral("app")));
    QFile manifest(tmp.path() + QStringLiteral("/app/package.json"));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    manifest.write(R"({"name":"app","scripts":{"dev":"vite"}})");
    manifest.close();

    Settings settings(tmp.path() + QStringLiteral("/trun.ini"));
    ProjectListModel model;
    ProjectService service(&model);
    service.setSettings(&settings);
    service.scanFolder(tmp.path());
    QCOMPARE(service.projectCount(), 1);
    const QString projectId = service.projects().first().value(QStringLiteral("id")).toString();

    const QString result = service.addCustomCommand(
        QStringLiteral("my tool"), tmp.path() + QStringLiteral("/app"),
        QStringLiteral("make"), QStringLiteral("lint --fix"), QStringLiteral(""),
        QStringLiteral("FOO=bar"), false, 5173);
    QCOMPARE(result, projectId + QStringLiteral("|custom:my-tool"));
    QCOMPARE(service.projectCount(), 1);

    const QJsonArray cmds = service.projects().first().value(QStringLiteral("commands")).toArray();
    QJsonObject custom;
    for (const QJsonValue &v : cmds) {
        if (v.toObject().value(QStringLiteral("id")).toString() == QStringLiteral("custom:my-tool"))
            custom = v.toObject();
    }
    QVERIFY(!custom.isEmpty());
    QCOMPARE(custom.value(QStringLiteral("command")).toString(), QStringLiteral("make"));
    QCOMPARE(custom.value(QStringLiteral("args")).toArray().size(), 2);

    // Stored run configuration is picked up by Run/Detail/MCP
    const QString raw = settings.get(
        QStringLiteral("runConfig/%1/custom:my-tool").arg(projectId)).toString();
    QVERIFY(raw.contains(QStringLiteral("FOO=bar")));
    QVERIFY(raw.contains(QStringLiteral("5173")));

    service.selectProject(projectId);
    bool found = false;
    for (const QJsonObject &c : service.activeProjectCommands()) {
        if (c.value(QStringLiteral("id")).toString() == QStringLiteral("custom:my-tool"))
            found = true;
    }
    QVERIFY(found);
}

void TestProjectService::testAddCustomCommandPseudoProject()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir(tmp.path()).mkpath(QStringLiteral("plain")));

    Settings settings(tmp.path() + QStringLiteral("/trun.ini"));
    ProjectListModel model;
    ProjectService service(&model);
    service.setSettings(&settings);
    service.scanFolder(tmp.path());
    QCOMPARE(service.projectCount(), 0);

    const QString folder = tmp.path() + QStringLiteral("/plain");
    const QString result = service.addCustomCommand(
        QStringLiteral("hello"), folder, QStringLiteral("/bin/echo"),
        QStringLiteral("hi"), QStringLiteral(""), QStringLiteral(""), false);
    QCOMPARE(result, folder + QStringLiteral("/custom|custom:hello"));
    QCOMPARE(service.projectCount(), 1);
    QCOMPARE(service.projects().first().value(QStringLiteral("manifest")).toString(),
             QStringLiteral("custom"));

    service.selectProject(folder + QStringLiteral("/custom"));
    QCOMPARE(service.activeProjectCommands().size(), 1);
}

void TestProjectService::testAddCustomCommandUniqueIds()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir(tmp.path()).mkpath(QStringLiteral("app")));
    QFile manifest(tmp.path() + QStringLiteral("/app/package.json"));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    manifest.write(R"({"name":"app","scripts":{}})");
    manifest.close();

    Settings settings(tmp.path() + QStringLiteral("/trun.ini"));
    ProjectListModel model;
    ProjectService service(&model);
    service.setSettings(&settings);
    service.scanFolder(tmp.path());

    const QString folder = tmp.path() + QStringLiteral("/app");
    const QString first = service.addCustomCommand(
        QStringLiteral("tool"), folder, QStringLiteral("make"),
        QStringLiteral(""), QStringLiteral(""), QStringLiteral(""), false);
    const QString second = service.addCustomCommand(
        QStringLiteral("tool"), folder, QStringLiteral("make"),
        QStringLiteral(""), QStringLiteral(""), QStringLiteral(""), false);
    QVERIFY(first.endsWith(QStringLiteral("|custom:tool")));
    QVERIFY(second.endsWith(QStringLiteral("|custom:tool-2")));
}

void TestProjectService::testAddCustomCommandRejectsEmpty()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir(tmp.path()).mkpath(QStringLiteral("app")));

    ProjectListModel model;
    ProjectService service(&model);
    const QString folder = tmp.path() + QStringLiteral("/app");
    QVERIFY(service.addCustomCommand(QStringLiteral(""), folder, QStringLiteral("make"),
                                     QStringLiteral(""), QStringLiteral(""),
                                     QStringLiteral(""), false).isEmpty());
    QVERIFY(service.addCustomCommand(QStringLiteral("tool"), folder, QStringLiteral("  "),
                                     QStringLiteral(""), QStringLiteral(""),
                                     QStringLiteral(""), false).isEmpty());
    QVERIFY(service.addCustomCommand(QStringLiteral("tool"), tmp.path() + QStringLiteral("/gone"),
                                     QStringLiteral("make"), QStringLiteral(""),
                                     QStringLiteral(""), QStringLiteral(""), false).isEmpty());
    QCOMPARE(service.projectCount(), 0);
}

void TestProjectService::testCustomCommandsSurviveRescanAndStripCache()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir(tmp.path()).mkpath(QStringLiteral("app")));
    QFile manifest(tmp.path() + QStringLiteral("/app/package.json"));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    manifest.write(R"({"name":"app","scripts":{"dev":"vite"}})");
    manifest.close();

    Settings settings(tmp.path() + QStringLiteral("/trun.ini"));
    ProjectListModel model;
    ProjectService service(&model);
    service.setSettings(&settings);
    service.scanFolder(tmp.path());
    service.addCustomCommand(QStringLiteral("tool"), tmp.path() + QStringLiteral("/app"),
                             QStringLiteral("make"), QStringLiteral(""), QStringLiteral(""),
                             QStringLiteral(""), false);

    // Rescan keeps the custom command merged
    service.scanFolder(tmp.path());
    bool found = false;
    for (const QJsonValue &v :
         service.projects().first().value(QStringLiteral("commands")).toArray()) {
        if (v.toObject().value(QStringLiteral("id")).toString() == QStringLiteral("custom:tool"))
            found = true;
    }
    QVERIFY(found);

    // ...but the scan cache itself stays custom-free
    bool leaked = false;
    for (const QJsonValue &p : settings.projects()) {
        const QJsonObject proj = p.toObject();
        if (proj.value(QStringLiteral("manifest")).toString() == QStringLiteral("custom"))
            leaked = true;
        for (const QJsonValue &v : proj.value(QStringLiteral("commands")).toArray()) {
            if (v.toObject().value(QStringLiteral("id")).toString().startsWith(
                    QStringLiteral("custom:")))
                leaked = true;
        }
    }
    QVERIFY(!leaked);

    // Fresh service restores customs from their own key
    ProjectListModel model2;
    ProjectService restored(&model2);
    restored.setSettings(&settings);
    QVERIFY(restored.restoreFromCache());
    bool refound = false;
    for (const QJsonValue &v :
         restored.projects().first().value(QStringLiteral("commands")).toArray()) {
        if (v.toObject().value(QStringLiteral("id")).toString() == QStringLiteral("custom:tool"))
            refound = true;
    }
    QVERIFY(refound);
}

void TestProjectService::testPinsPersistAndToggle()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    Settings settings(tmp.path() + QStringLiteral("/trun.ini"));
    ProjectListModel model;
    ProjectService service(&model);
    service.setSettings(&settings);

    QSignalSpy spy(&service, &ProjectService::pinnedChanged);
    service.setPinned(QStringLiteral("p1"), QStringLiteral("npm:dev"), true);
    QCOMPARE(spy.size(), 1);
    QVERIFY(service.isPinned(QStringLiteral("p1"), QStringLiteral("npm:dev")));
    QVERIFY(!service.isPinned(QStringLiteral("p1"), QStringLiteral("npm:build")));
    QCOMPARE(service.pinnedCommands().size(), 1);

    // No-op toggles don't emit
    service.setPinned(QStringLiteral("p1"), QStringLiteral("npm:dev"), true);
    QCOMPARE(spy.size(), 1);

    service.setPinned(QStringLiteral("p1"), QStringLiteral("npm:dev"), false);
    QCOMPARE(spy.size(), 2);
    QVERIFY(!service.isPinned(QStringLiteral("p1"), QStringLiteral("npm:dev")));
    QVERIFY(service.pinnedCommands().isEmpty());

    // Persisted across instances sharing the settings file
    service.setPinned(QStringLiteral("p1"), QStringLiteral("npm:dev"), true);
    ProjectListModel model2;
    ProjectService restored(&model2);
    restored.setSettings(&settings);
    QVERIFY(restored.isPinned(QStringLiteral("p1"), QStringLiteral("npm:dev")));
}

void TestProjectService::testPinsPrunedOnScan()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir(tmp.path()).mkpath(QStringLiteral("app")));
    QFile manifest(tmp.path() + QStringLiteral("/app/package.json"));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    manifest.write(R"({"name":"app","scripts":{"dev":"vite"}})");
    manifest.close();

    Settings settings(tmp.path() + QStringLiteral("/trun.ini"));
    ProjectListModel model;
    ProjectService service(&model);
    service.setSettings(&settings);
    service.setPinned(QStringLiteral("/gone/package.json"), QStringLiteral("npm:dev"), true);
    service.scanFolder(tmp.path());
    QVERIFY(!service.isPinned(QStringLiteral("/gone/package.json"), QStringLiteral("npm:dev")));

    const QString projectId = service.projects().first().value(QStringLiteral("id")).toString();
    service.setPinned(projectId, QStringLiteral("npm:dev"), true);
    service.scanFolder(tmp.path());
    QVERIFY(service.isPinned(projectId, QStringLiteral("npm:dev")));
}

void TestProjectService::testRunCommandEffective()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir(tmp.path()).mkpath(QStringLiteral("app")));

    Settings settings(tmp.path() + QStringLiteral("/trun.ini"));
    ProjectListModel model;
    ProjectService service(&model);
    service.setSettings(&settings);
    CommandExecutor executor;
    service.setExecutor(&executor);
    service.scanFolder(tmp.path());

    const QString folder = tmp.path() + QStringLiteral("/app");
    const QString result = service.addCustomCommand(
        QStringLiteral("echo"), folder, QStringLiteral("/bin/echo"),
        QStringLiteral("hello-effective"), QStringLiteral(""), QStringLiteral(""), false);
    QVERIFY(!result.isEmpty());
    const QString projectId = result.split(u'|').first();
    const QString commandId = result.split(u'|').last();

    QSignalSpy finishedSpy(&executor, &CommandExecutor::finished);
    QVERIFY(service.runCommandEffective(projectId, commandId));
    const QString runKey = projectId + u'|' + commandId;
    QVERIFY2(finishedSpy.wait(5000), "run finished never arrived");
    QVERIFY(!executor.runningCommandIds().contains(runKey));

    // Unknown refs fail without executor calls
    QVERIFY(!service.runCommandEffective(projectId, QStringLiteral("npm:nope")));
    QVERIFY(!service.runCommandEffective(QStringLiteral("/nope"), commandId));

    // No executor wired: safe failure
    ProjectListModel model2;
    ProjectService noexec(&model2);
    noexec.setSettings(&settings);
    QVERIFY(!noexec.runCommandEffective(projectId, commandId));
}

void TestProjectService::testRecentCommandsRecordedOnStart()
{
    // Commands launched outside runCommandEffective (e.g. DetailPage's
    // direct runWithEnv) must still land in the recent list.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    Settings settings(tmp.path() + QStringLiteral("/trun.ini"));
    ProjectListModel model;
    ProjectService service(&model);
    service.setSettings(&settings);
    CommandExecutor executor;
    service.setExecutor(&executor);

    const QString runKey = QStringLiteral("/tmp/proj/package.json|echo:hi");
    QSignalSpy startedSpy(&executor, &CommandExecutor::started);
    QSignalSpy finishedSpy(&executor, &CommandExecutor::finished);
    QVERIFY(executor.run(QStringLiteral("/bin/echo"), {QStringLiteral("hi")},
                         tmp.path(), QStringLiteral("Echo"), runKey) > 0);
    QVERIFY2(startedSpy.wait(5000), "started never arrived");
    QTRY_COMPARE(service.recentCommands().size(), 1);
    const QVariantMap recent = service.recentCommands().first().toMap();
    QCOMPARE(recent.value("projectId").toString(), QStringLiteral("/tmp/proj/package.json"));
    QCOMPARE(recent.value("commandId").toString(), QStringLiteral("echo:hi"));
    QCOMPARE(recent.value("label").toString(), QStringLiteral("Echo"));

    service.clearRecentCommands();
    QCOMPARE(service.recentCommands().size(), 0);
    QVERIFY2(finishedSpy.wait(5000), "finished never arrived");
}

void TestProjectService::testSameCommandIdAcrossProjectsTrackedSeparately()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir(tmp.path()).mkpath(QStringLiteral("one")));
    QVERIFY(QDir(tmp.path()).mkpath(QStringLiteral("two")));

    Settings settings(tmp.path() + QStringLiteral("/trun.ini"));
    ProjectListModel model;
    ProjectService service(&model);
    service.setSettings(&settings);
    CommandExecutor executor;
    service.setExecutor(&executor);
    service.scanFolder(tmp.path());

    // Same bare id in two folders: independent PIDs and states
    const QString r1 = service.addCustomCommand(
        QStringLiteral("work"), tmp.path() + QStringLiteral("/one"), QStringLiteral("/bin/sleep"),
        QStringLiteral("30"), QStringLiteral(""), QStringLiteral(""), false);
    const QString r2 = service.addCustomCommand(
        QStringLiteral("work"), tmp.path() + QStringLiteral("/two"), QStringLiteral("/bin/sleep"),
        QStringLiteral("30"), QStringLiteral(""), QStringLiteral(""), false);
    QVERIFY(r1.endsWith(QStringLiteral("|custom:work")));
    QVERIFY(r2.endsWith(QStringLiteral("|custom:work")));
    QVERIFY(r1 != r2);

    QSignalSpy startedSpy(&executor, &CommandExecutor::started);
    QVERIFY(service.runCommandEffective(tmp.path() + QStringLiteral("/one"),
                                        QStringLiteral("custom:work")));
    QVERIFY(startedSpy.wait(5000));
    const int pidOne = startedSpy.first().at(1).toInt();
    QVERIFY(pidOne > 0);

    const QStringList running = executor.runningCommandIds();
    QCOMPARE(running.size(), 1);
    QVERIFY(running.first().endsWith(QStringLiteral("|custom:work")));
    QCOMPARE(executor.pidForCommand(running.first()), pidOne);

    const QString otherKey = r2.split(u'|').first() + QStringLiteral("|custom:work");
    QVERIFY(otherKey != running.first());
    QCOMPARE(executor.pidForCommand(otherKey), 0);
    QCOMPARE(executor.memoryForCommand(otherKey), -1);

    QSignalSpy finishedSpy(&executor, &CommandExecutor::finished);
    executor.stopCommand(running.first());
    QVERIFY(finishedSpy.wait(5000));
    QVERIFY(!executor.runningCommandIds().contains(running.first()));
    executor.killAll();
}

void TestProjectService::testStaticProgramDetection()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir(tmp.path()).mkpath(QStringLiteral("web")));
    QFile manifest(tmp.path() + QStringLiteral("/web/package.json"));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    manifest.write(R"({"name":"web","scripts":{)"
                  R"("serve":"vite --port 3000",)"
                  R"("dev":"next dev",)"
                  R"("lint":"eslint .",)"
                  R"("start":"php -S 127.0.0.1:9000 -t public")"
                  R"(}})");
    manifest.close();
    QVERIFY(QDir(tmp.path()).mkpath(QStringLiteral("api")));
    QFile composer(tmp.path() + QStringLiteral("/api/composer.json"));
    QVERIFY(composer.open(QIODevice::WriteOnly));
    composer.write(R"({"name":"acme/api","scripts":{"serve":"symfony serve"}})");
    composer.close();

    ProjectListModel model;
    ProjectService service(&model);
    service.scanFolder(tmp.path());
    QCOMPARE(service.projectCount(), 2);

    auto byId = [&](const QString &id) {
        for (const QJsonObject &p : service.projects()) {
            for (const QJsonValue &v : p.value(QStringLiteral("commands")).toArray()) {
                if (v.toObject().value(QStringLiteral("id")).toString() == id)
                    return v.toObject();
            }
        }
        return QJsonObject();
    };
    const QJsonObject vite = byId(QStringLiteral("npm:serve"));
    QCOMPARE(vite.value(QStringLiteral("detectedProgram")).toString(), QStringLiteral("vite"));
    QCOMPARE(vite.value(QStringLiteral("detectedPort")).toInt(), 3000);

    const QJsonObject next = byId(QStringLiteral("npm:dev"));
    QCOMPARE(next.value(QStringLiteral("detectedProgram")).toString(), QStringLiteral("next"));
    QCOMPARE(next.value(QStringLiteral("detectedPort")).toInt(), 3000);

    const QJsonObject lint = byId(QStringLiteral("npm:lint"));
    QVERIFY(!lint.contains(QStringLiteral("detectedProgram")));

    const QJsonObject php = byId(QStringLiteral("npm:start"));
    QCOMPARE(php.value(QStringLiteral("detectedProgram")).toString(), QStringLiteral("php"));
    QCOMPARE(php.value(QStringLiteral("detectedPort")).toInt(), 9000);

    const QJsonObject symfony = byId(QStringLiteral("composer:serve"));
    QCOMPARE(symfony.value(QStringLiteral("detectedProgram")).toString(),
             QStringLiteral("symfony"));
    QCOMPARE(symfony.value(QStringLiteral("detectedPort")).toInt(), 8000);
}

void TestProjectService::testNpmChainAndWrapperDetection()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir(tmp.path()).mkpath(QStringLiteral("backend")));
    QVERIFY(QDir(tmp.path()).mkpath(QStringLiteral("frontend")));
    QVERIFY(writeJson(tmp.path() + "/backend/package.json", R"({
        "name": "acme-backend",
        "scripts": {
            "serve": "npm --prefix ../frontend run serve",
            "loop-a": "npm run loop-b",
            "loop-b": "npm run loop-a",
            "wrapped": "cross-env NODE_ENV=production vite --port 3001"
        }
    })"));
    QVERIFY(writeJson(tmp.path() + "/frontend/package.json", R"({
        "name": "acme-frontend",
        "scripts": {"serve": "vite"}
    })"));

    ProjectListModel model;
    ProjectService service(&model);
    service.scanFolder(tmp.path());
    QCOMPARE(service.projectCount(), 2);

    auto byLabel = [&](const QString &projectName, const QString &label) {
        for (const QJsonObject &p : service.projects()) {
            if (p.value(QStringLiteral("name")).toString() != projectName)
                continue;
            for (const QJsonValue &v : p.value(QStringLiteral("commands")).toArray()) {
                if (v.toObject().value(QStringLiteral("label")).toString() == label)
                    return v.toObject();
            }
        }
        return QJsonObject();
    };
    // Proxied script resolves to the real program behind the chain
    const QJsonObject serve = byLabel(QStringLiteral("acme-backend"), QStringLiteral("serve"));
    QVERIFY(!serve.isEmpty());
    QCOMPARE(serve.value(QStringLiteral("detectedProgram")).toString(),
             QStringLiteral("vite"));
    QCOMPARE(serve.value(QStringLiteral("detectedPort")).toInt(), 5173);
    // Env wrappers are stripped before detection
    const QJsonObject wrapped = byLabel(QStringLiteral("acme-backend"), QStringLiteral("wrapped"));
    QCOMPARE(wrapped.value(QStringLiteral("detectedProgram")).toString(),
             QStringLiteral("vite"));
    QCOMPARE(wrapped.value(QStringLiteral("detectedPort")).toInt(), 3001);
    // Reference cycles terminate (no hang) and stay runnable
    const QJsonObject loop = byLabel(QStringLiteral("acme-backend"), QStringLiteral("loop-a"));
    QVERIFY(!loop.isEmpty());
    QCOMPARE(loop.value(QStringLiteral("command")).toString(), QStringLiteral("npm"));
}

void TestProjectService::testRescanDropsStaleSelection()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir(tmp.path()).mkpath(QStringLiteral("app")));
    const QString manifestPath = tmp.path() + QStringLiteral("/app/package.json");
    QFile manifest(manifestPath);
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    manifest.write(R"({"name":"app","scripts":{"dev":"vite"}})");
    manifest.close();

    ProjectListModel model;
    ProjectService service(&model);
    service.scanFolder(tmp.path());
    QCOMPARE(service.projectCount(), 1);
    const QString projectId = service.projects().first().value(QStringLiteral("id")).toString();
    service.selectProject(projectId);
    QCOMPARE(service.activeProjectCommands().size(), 1);

    // Rescan without changes keeps the selection (refreshed)
    service.scanFolder(tmp.path());
    QCOMPARE(service.activeProject().value(QStringLiteral("id")).toString(), projectId);
    QCOMPARE(service.activeProjectCommands().size(), 1);

    // Manifest gone: rescan drops the selection
    QVERIFY(QFile::remove(manifestPath));
    service.scanFolder(tmp.path());
    QCOMPARE(service.projectCount(), 0);
    QVERIFY(service.activeProject().isEmpty());
    QVERIFY(service.activeProjectCommands().isEmpty());
}

QTEST_MAIN(TestProjectService)
#include "test_projectservice.moc"
