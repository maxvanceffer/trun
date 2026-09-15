#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "mcpserver.h"
#include "projectservice.h"
#include "commandexecutor.h"
#include "projectlistmodel.h"
#include "settings.h"

class TestMcpServer : public QObject {
    Q_OBJECT

private slots:
    void testInitialize();
    void testToolsList();
    void testUnknownMethodAndTool();
    void testProjectsAndCommands();
    void testReadLogEmpty();
    void testSearchLogsMonolog();
    void testRunUnknown();

private:
    struct Fixture {
        QTemporaryDir home;
        Settings *settings = nullptr;
        ProjectListModel *models = nullptr;
        ProjectService *service = nullptr;
        CommandExecutor *executor = nullptr;
        McpServer *server = nullptr;
    };
    std::unique_ptr<Fixture> makeFixture();
    QJsonObject call(Fixture &fx, const QString &tool, const QJsonObject &args);
};

std::unique_ptr<TestMcpServer::Fixture> TestMcpServer::makeFixture()
{
    auto fx = std::make_unique<Fixture>();
    if (!fx->home.isValid())
        return nullptr;
    fx->settings = new Settings(fx->home.path() + QStringLiteral("/settings.ini"), this);
    fx->models = new ProjectListModel(this);
    fx->service = new ProjectService(fx->models, this);
    fx->service->setSettings(fx->settings);
    fx->executor = new CommandExecutor(this);
    fx->server = new McpServer(fx->service, fx->executor, fx->settings, this);
    return fx;
}

QJsonObject TestMcpServer::call(Fixture &fx, const QString &tool, const QJsonObject &args)
{
    const QJsonObject reply = fx.server->handleMessage(
        QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("id"), 1},
                    {QStringLiteral("method"), QStringLiteral("tools/call")},
                    {QStringLiteral("params"),
                     QJsonObject{{QStringLiteral("name"), tool},
                                 {QStringLiteral("arguments"), args}}}});
    const QJsonObject result = reply.value(QStringLiteral("result")).toObject();
    const QJsonArray content = result.value(QStringLiteral("content")).toArray();
    return QJsonObject{{QStringLiteral("text"),
                        content.first().toObject().value(QStringLiteral("text")).toString()},
                       {QStringLiteral("isError"), result.value(QStringLiteral("isError"))}};
}

void TestMcpServer::testInitialize()
{
    auto fx = makeFixture();
    QVERIFY(fx);
    const QJsonObject reply = fx->server->handleMessage(
        QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("id"), 1},
                    {QStringLiteral("method"), QStringLiteral("initialize")},
                    {QStringLiteral("params"), QJsonObject()}});
    const QJsonObject result = reply.value(QStringLiteral("result")).toObject();
    QVERIFY(!result.value(QStringLiteral("protocolVersion")).toString().isEmpty());
    QCOMPARE(result.value(QStringLiteral("serverInfo"))
                 .toObject()
                 .value(QStringLiteral("name"))
                 .toString(),
             QStringLiteral("trun"));
}

void TestMcpServer::testToolsList()
{
    auto fx = makeFixture();
    QVERIFY(fx);
    const QJsonObject reply = fx->server->handleMessage(
        QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("id"), 1},
                    {QStringLiteral("method"), QStringLiteral("tools/list")},
                    {QStringLiteral("params"), QJsonObject()}});
    const QJsonArray tools = reply.value(QStringLiteral("result"))
                                 .toObject()
                                 .value(QStringLiteral("tools"))
                                 .toArray();
    QCOMPARE(tools.size(), 14);
    QSet<QString> names;
    for (const QJsonValue &t : tools)
        names << t.toObject().value(QStringLiteral("name")).toString();
    for (const char *n : {"list_projects", "list_commands", "status", "run", "stop",
                          "read_log", "search_logs", "docker_status", "docker_ps",
                          "docker_images", "docker_logs", "docker_control", "docker_stats",
                          "docker_prune"})
        QVERIFY2(names.contains(QString::fromUtf8(n)), n);
}

void TestMcpServer::testUnknownMethodAndTool()
{
    auto fx = makeFixture();
    QVERIFY(fx);
    const QJsonObject badMethod = fx->server->handleMessage(
        QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("id"), 1},
                    {QStringLiteral("method"), QStringLiteral("nope")}});
    QCOMPARE(badMethod.value(QStringLiteral("error"))
                 .toObject()
                 .value(QStringLiteral("code"))
                 .toInt(),
             -32601);

    const QJsonObject badTool = call(*fx, QStringLiteral("nope"), {});
    QVERIFY(badTool.value(QStringLiteral("isError")).toBool());
}

void TestMcpServer::testProjectsAndCommands()
{
    auto fx = makeFixture();
    QVERIFY(fx);
    QVERIFY(QDir(fx->home.path()).mkpath(QStringLiteral("app")));
    QFile manifest(fx->home.path() + QStringLiteral("/app/package.json"));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    manifest.write(R"({"name":"probe","scripts":{"dev":"vite","build":"tsc"}})");
    manifest.close();
    fx->service->scanFolder(fx->home.path());
    QVERIFY(fx->service->projectCount() > 0);

    const QString projectId = fx->service->projects().first().value(QStringLiteral("id")).toString();
    const QJsonObject projects = call(*fx, QStringLiteral("list_projects"), {});
    QVERIFY(!projects.value(QStringLiteral("isError")).toBool());
    QVERIFY(projects.value(QStringLiteral("text")).toString().contains(QStringLiteral("probe")));

    const QJsonObject commands = call(
        *fx, QStringLiteral("list_commands"),
        QJsonObject{{QStringLiteral("project"), projectId}});
    QVERIFY(!commands.value(QStringLiteral("isError")).toBool());
    QVERIFY(commands.value(QStringLiteral("text")).toString().contains(QStringLiteral("npm:dev")));

    const QJsonObject missing =
        call(*fx, QStringLiteral("list_commands"),
             QJsonObject{{QStringLiteral("project"), QStringLiteral("/nope")}});
    QVERIFY(missing.value(QStringLiteral("isError")).toBool());
}

void TestMcpServer::testReadLogEmpty()
{
    auto fx = makeFixture();
    QVERIFY(fx);
    const QJsonObject reply =
        call(*fx, QStringLiteral("read_log"),
             QJsonObject{{QStringLiteral("commandId"), QStringLiteral("npm:dev")}});
    QVERIFY(!reply.value(QStringLiteral("isError")).toBool());
    QVERIFY(reply.value(QStringLiteral("text")).toString().isEmpty());
}

void TestMcpServer::testSearchLogsMonolog()
{
    auto fx = makeFixture();
    QVERIFY(fx);
    QVERIFY(QDir(fx->home.path()).mkpath(QStringLiteral("app/var/log")));
    QFile manifest(fx->home.path() + QStringLiteral("/app/package.json"));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    manifest.write(R"({"name":"logapp","scripts":{"dev":"vite"}})");
    manifest.close();
    QFile log(fx->home.path() + QStringLiteral("/app/var/log/app.log"));
    QVERIFY(log.open(QIODevice::WriteOnly));
    log.write("[2026-09-10T10:00:00+00:00] app.INFO: all good\n"
              "[2026-09-10T10:01:00+00:00] app.ERROR: boom failed\n"
              "[2026-09-10T10:02:00+00:00] app.DEBUG: got userErrors in response\n"
              "[2026-09-11T10:00:00+00:00] app.ERROR: later issue\n"
              "plain line without format\n");
    log.close();
    fx->service->scanFolder(fx->home.path());
    const QString projectId = fx->service->projects().first().value(QStringLiteral("id")).toString();

    const QJsonObject byLevel =
        call(*fx, QStringLiteral("search_logs"),
             QJsonObject{{QStringLiteral("project"), projectId},
                         {QStringLiteral("level"), QStringLiteral("ERROR")}});
    const QString levelText = byLevel.value(QStringLiteral("text")).toString();
    QVERIFY(levelText.contains(QStringLiteral("boom failed")));
    QVERIFY(!levelText.contains(QStringLiteral("all good")));
    // "userErrors" inside a DEBUG line must not match level=ERROR
    QVERIFY(!levelText.contains(QStringLiteral("userErrors")));

    const QJsonObject byDate =
        call(*fx, QStringLiteral("search_logs"),
             QJsonObject{{QStringLiteral("project"), projectId},
                         {QStringLiteral("date"), QStringLiteral("2026-09-11")}});
    QVERIFY(byDate.value(QStringLiteral("text")).toString().contains(QStringLiteral("later issue")));
    QVERIFY(!byDate.value(QStringLiteral("text")).toString().contains(QStringLiteral("boom failed")));

    // Plain-text fallback matches non-monolog lines too
    const QJsonObject byQuery =
        call(*fx, QStringLiteral("search_logs"),
             QJsonObject{{QStringLiteral("project"), projectId},
                         {QStringLiteral("query"), QStringLiteral("plain line")}});
    QVERIFY(byQuery.value(QStringLiteral("text")).toString().contains(QStringLiteral("plain line")));
}

void TestMcpServer::testRunUnknown()
{
    auto fx = makeFixture();
    QVERIFY(fx);
    const QJsonObject reply =
        call(*fx, QStringLiteral("run"),
             QJsonObject{{QStringLiteral("project"), QStringLiteral("/nope")},
                         {QStringLiteral("command"), QStringLiteral("npm:dev")}});
    QVERIFY(reply.value(QStringLiteral("isError")).toBool());
}

QTEST_MAIN(TestMcpServer)
#include "test_mcpserver.moc"
