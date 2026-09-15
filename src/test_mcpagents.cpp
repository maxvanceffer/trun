#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include "mcpagents.h"

class TestMcpAgents : public QObject {
    Q_OBJECT

private slots:
    void testScanEmptyHome();
    void testClaudeInstallAndRemove();
    void testClaudeRefusesInvalidJson();
    void testCodexInstallAndToggle();
    void testOpencodeInstallAndToggle();
    void testJunieInstallAndRemove();
    void testBackupCreated();
    void testUnknownAgent();

private:
    QVariantMap byId(const QVariantList &agents, const QString &id);
};

QVariantMap TestMcpAgents::byId(const QVariantList &agents, const QString &id)
{
    for (const QVariant &v : agents) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("id")).toString() == id)
            return m;
    }
    return {};
}

static std::unique_ptr<McpAgentManager> managerWithHome(const QString &home)
{
    auto m = std::make_unique<McpAgentManager>();
    m->setHomeForTesting(home);
    return m;
}

void TestMcpAgents::testScanEmptyHome()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    auto m = managerWithHome(home.path());

    const QVariantList agents = m->scanAgents();
    QCOMPARE(agents.size(), 4);
    for (const QVariant &v : agents) {
        const QVariantMap a = v.toMap();
        QVERIFY(!a.value(QStringLiteral("found")).toBool());
        QVERIFY(!a.value(QStringLiteral("installed")).toBool());
        QVERIFY(!a.value(QStringLiteral("enabled")).toBool());
    }
}

void TestMcpAgents::testClaudeInstallAndRemove()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    QVERIFY(QDir(home.path()).mkpath(QStringLiteral(".claude")));
    auto m = managerWithHome(home.path());

    QVERIFY(byId(m->scanAgents(), QStringLiteral("claude"))
                .value(QStringLiteral("found"))
                .toBool());
    QVERIFY(m->installAgent(QStringLiteral("claude")));

    QVariantMap a = byId(m->scanAgents(), QStringLiteral("claude"));
    QVERIFY(a.value(QStringLiteral("installed")).toBool());
    QVERIFY(a.value(QStringLiteral("enabled")).toBool());

    QFile f(home.path() + QStringLiteral("/.claude.json"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QJsonObject trun = QJsonDocument::fromJson(f.readAll())
                                 .object()
                                 .value(QStringLiteral("mcpServers"))
                                 .toObject()
                                 .value(QStringLiteral("trun"))
                                 .toObject();
    QVERIFY(!trun.value(QStringLiteral("command")).toString().isEmpty());
    QCOMPARE(trun.value(QStringLiteral("args")).toArray().first().toString(),
             QStringLiteral("--mcp"));

    // Disable on a flag-less agent removes the entry
    QVERIFY(m->setAgentEnabled(QStringLiteral("claude"), false));
    QVERIFY(!byId(m->scanAgents(), QStringLiteral("claude"))
                 .value(QStringLiteral("installed"))
                 .toBool());
}

void TestMcpAgents::testClaudeRefusesInvalidJson()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    QFile f(home.path() + QStringLiteral("/.claude.json"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("{invalid json");
    f.close();

    auto m = managerWithHome(home.path());
    QVERIFY(!m->installAgent(QStringLiteral("claude")));
    QVERIFY(!m->lastError().isEmpty());

    // File untouched
    QFile check(home.path() + QStringLiteral("/.claude.json"));
    QVERIFY(check.open(QIODevice::ReadOnly));
    QCOMPARE(check.readAll(), QByteArray("{invalid json"));
}

void TestMcpAgents::testCodexInstallAndToggle()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    QVERIFY(QDir(home.path()).mkpath(QStringLiteral(".codex")));
    {
        QFile f(home.path() + QStringLiteral("/.codex/config.toml"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[mcp_servers.other]\ncommand = \"other\"\n");
        f.close();
    }
    auto m = managerWithHome(home.path());

    QVERIFY(m->installAgent(QStringLiteral("codex")));
    QVariantMap a = byId(m->scanAgents(), QStringLiteral("codex"));
    QVERIFY(a.value(QStringLiteral("installed")).toBool());
    QVERIFY(a.value(QStringLiteral("enabled")).toBool());

    QVERIFY(m->setAgentEnabled(QStringLiteral("codex"), false));
    a = byId(m->scanAgents(), QStringLiteral("codex"));
    QVERIFY(a.value(QStringLiteral("installed")).toBool());
    QVERIFY(!a.value(QStringLiteral("enabled")).toBool());

    QFile f(home.path() + QStringLiteral("/.codex/config.toml"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString content = QString::fromUtf8(f.readAll());
    QVERIFY(content.contains(QStringLiteral("[mcp_servers.other]")));
    QVERIFY(content.contains(QStringLiteral("enabled = false")));

    QVERIFY(m->setAgentEnabled(QStringLiteral("codex"), true));
    QVERIFY(byId(m->scanAgents(), QStringLiteral("codex"))
                .value(QStringLiteral("enabled"))
                .toBool());
}

void TestMcpAgents::testOpencodeInstallAndToggle()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    auto m = managerWithHome(home.path());

    QVERIFY(m->installAgent(QStringLiteral("opencode")));
    QVariantMap a = byId(m->scanAgents(), QStringLiteral("opencode"));
    QVERIFY(a.value(QStringLiteral("found")).toBool());
    QVERIFY(a.value(QStringLiteral("installed")).toBool());
    QVERIFY(a.value(QStringLiteral("enabled")).toBool());

    QVERIFY(m->setAgentEnabled(QStringLiteral("opencode"), false));
    QVERIFY(!byId(m->scanAgents(), QStringLiteral("opencode"))
                 .value(QStringLiteral("enabled"))
                 .toBool());

    QFile f(home.path() + QStringLiteral("/.config/opencode/opencode.json"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QJsonObject trun = QJsonDocument::fromJson(f.readAll())
                                 .object()
                                 .value(QStringLiteral("mcp"))
                                 .toObject()
                                 .value(QStringLiteral("trun"))
                                 .toObject();
    QCOMPARE(trun.value(QStringLiteral("type")).toString(), QStringLiteral("local"));
    QVERIFY(!trun.value(QStringLiteral("enabled")).toBool(true));
}

void TestMcpAgents::testJunieInstallAndRemove()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    QVERIFY(QDir(home.path()).mkpath(QStringLiteral(".junie")));
    auto m = managerWithHome(home.path());

    QVERIFY(m->installAgent(QStringLiteral("junie")));
    QVERIFY(byId(m->scanAgents(), QStringLiteral("junie"))
                .value(QStringLiteral("installed"))
                .toBool());
    QVERIFY(QFile::exists(home.path() + QStringLiteral("/.junie/mcp/mcp.json")));

    QVERIFY(m->setAgentEnabled(QStringLiteral("junie"), false));
    QVERIFY(!byId(m->scanAgents(), QStringLiteral("junie"))
                 .value(QStringLiteral("installed"))
                 .toBool());
}

void TestMcpAgents::testBackupCreated()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    QVERIFY(QDir(home.path()).mkpath(QStringLiteral(".claude")));
    {
        QFile f(home.path() + QStringLiteral("/.claude.json"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(QJsonObject{{QStringLiteral("mcpServers"), QJsonObject()}})
                    .toJson());
        f.close();
    }
    auto m = managerWithHome(home.path());
    QVERIFY(m->installAgent(QStringLiteral("claude")));

    const QStringList backups = QDir(home.path()).entryList(
        QStringList{QStringLiteral(".claude.json.bak-trun-*")}, QDir::Files | QDir::Hidden);
    QCOMPARE(backups.size(), 1);
}

void TestMcpAgents::testUnknownAgent()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    auto m = managerWithHome(home.path());
    QVERIFY(!m->installAgent(QStringLiteral("nope")));
    QVERIFY(!m->lastError().isEmpty());
}

QTEST_MAIN(TestMcpAgents)
#include "test_mcpagents.moc"
