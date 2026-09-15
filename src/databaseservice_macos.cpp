#include "databaseservice_backend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>
#include <utility>

namespace {

QString runProcess(const QString &program, const QStringList &args, int timeoutMs = 5000)
{
    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForFinished(timeoutMs))
        return {};
    return QString::fromUtf8(proc.readAllStandardOutput());
}

QString findBrew()
{
    for (const QString &p : {QStringLiteral("/opt/homebrew/bin/brew"),
                             QStringLiteral("/usr/local/bin/brew")}) {
        if (QFileInfo::exists(p))
            return p;
    }
    return QStandardPaths::findExecutable(QStringLiteral("brew"));
}

// TCP ports currently in LISTEN state (via lsof).
QSet<int> listeningPorts()
{
    QSet<int> ports;
    const QString out = runProcess(QStringLiteral("lsof"),
                                   {QStringLiteral("-nP"), QStringLiteral("-iTCP"),
                                    QStringLiteral("-sTCP:LISTEN")});
    static const QRegularExpression re(QStringLiteral(":(\\d+)\\s*\\(LISTEN\\)"));
    auto it = re.globalMatch(out);
    while (it.hasNext())
        ports.insert(it.next().captured(1).toInt());
    return ports;
}

QString versionFromFormula(const QString &formula)
{
    const int at = formula.indexOf(QLatin1Char('@'));
    return at >= 0 ? formula.mid(at + 1) : QString();
}

// Maps a tracked Homebrew formula to (engine, display name, default port).
bool classifyFormula(const QString &formula, QString &engine, QString &name, int &port,
                     QString &version)
{
    if (formula.startsWith(QLatin1String("postgresql"))) {
        engine = QStringLiteral("postgresql");
        name = QStringLiteral("PostgreSQL");
        port = 5432;
        version = versionFromFormula(formula);
        return true;
    }
    if (formula == QLatin1String("mysql") || formula.startsWith(QLatin1String("mysql@"))) {
        engine = QStringLiteral("mysql");
        name = QStringLiteral("MySQL");
        port = 3306;
        version = versionFromFormula(formula);
        return true;
    }
    if (formula.startsWith(QLatin1String("mongodb-community"))) {
        engine = QStringLiteral("mongodb");
        name = QStringLiteral("MongoDB");
        port = 27017;
        version = versionFromFormula(formula);
        return true;
    }
    if (formula == QLatin1String("redis") || formula.startsWith(QLatin1String("redis@"))) {
        engine = QStringLiteral("redis");
        name = QStringLiteral("Redis");
        port = 6379;
        version = versionFromFormula(formula);
        return true;
    }
    return false;
}

} // namespace

QVariantList detectDatabaseServices()
{
    QVariantList out;
    const QSet<int> listening = listeningPorts();
    bool haveBrewPostgres = false;
    bool haveBrewMysql = false;
    bool haveBrewMongo = false;

    const QString brew = findBrew();
    if (!brew.isEmpty()) {
        struct BrewEntry {
            QString formula;
            QString engine;
            QString name;
            int port = 0;
            bool running = false;
        };
        QList<BrewEntry> entries;
        QStringList formulas;

        const QString outText = runProcess(brew, {QStringLiteral("services"),
                                                  QStringLiteral("list")});
        const QStringList lines = outText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        static const QRegularExpression ws(QStringLiteral("\\s+"));
        for (int i = 0; i < lines.size(); ++i) {
            const QStringList parts = lines.at(i).trimmed().split(ws, Qt::SkipEmptyParts);
            if (parts.size() < 2)
                continue;
            const QString formula = parts.at(0);
            if (formula == QLatin1String("Name"))
                continue; // header
            const QString status = parts.at(1);

            BrewEntry entry;
            entry.formula = formula;
            QString fallbackVersion;
            if (!classifyFormula(formula, entry.engine, entry.name, entry.port,
                                 fallbackVersion))
                continue;
            entry.running =
                status == QLatin1String("started")
                || (entry.port > 0 && listening.contains(entry.port));
            entries.append(entry);
            formulas.append(formula);

            if (entry.engine == QLatin1String("postgresql"))
                haveBrewPostgres = true;
            else if (entry.engine == QLatin1String("mysql"))
                haveBrewMysql = true;
            else if (entry.engine == QLatin1String("mongodb"))
                haveBrewMongo = true;
        }

        // Real versions via `brew list --versions` (formula -> version).
        QHash<QString, QString> versions;
        if (!formulas.isEmpty()) {
            const QString vout = runProcess(brew, QStringList{QStringLiteral("list"),
                                                              QStringLiteral("--versions")}
                                                     + formulas,
                                            10000);
            const QStringList vlines = vout.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            static const QRegularExpression rev(QStringLiteral("_\\d+$"));
            for (const QString &line : vlines) {
                const QStringList p = line.trimmed().split(ws, Qt::SkipEmptyParts);
                if (p.size() < 2)
                    continue;
                QString v = p.at(1);
                v.remove(rev); // drop Homebrew revision suffix (_3, _1, ...)
                versions.insert(p.at(0), v);
            }
        }

        for (const BrewEntry &e : std::as_const(entries)) {
            out << QVariantMap{
                {QStringLiteral("id"), QStringLiteral("brew:") + e.formula},
                {QStringLiteral("engine"), e.engine},
                {QStringLiteral("name"), e.name},
                {QStringLiteral("version"), versions.value(e.formula, versionFromFormula(e.formula))},
                {QStringLiteral("port"), e.port},
                {QStringLiteral("running"), e.running},
                {QStringLiteral("source"), QStringLiteral("homebrew")},
                {QStringLiteral("formula"), e.formula},
                {QStringLiteral("controllable"), true},
            };
        }
    }

    // Postgres.app (only when Homebrew has no PostgreSQL, to avoid noise).
    if (!haveBrewPostgres) {
        const QDir versions(QStringLiteral("/Applications/Postgres.app/Contents/Versions"));
        for (const QString &v : versions.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (!QFileInfo::exists(versions.filePath(v) + QStringLiteral("/bin/postgres")))
                continue;
            out << QVariantMap{
                {QStringLiteral("id"), QStringLiteral("postgresapp:") + v},
                {QStringLiteral("engine"), QStringLiteral("postgresql")},
                {QStringLiteral("name"), QStringLiteral("PostgreSQL")},
                {QStringLiteral("version"), v},
                {QStringLiteral("port"), 5432},
                {QStringLiteral("running"), listening.contains(5432)},
                {QStringLiteral("source"), QStringLiteral("postgresapp")},
                {QStringLiteral("formula"), QString()},
                {QStringLiteral("controllable"), false},
            };
        }
    }

    // Official MySQL package.
    if (!haveBrewMysql && QFileInfo::exists(QStringLiteral("/usr/local/mysql/bin/mysqld"))) {
        QString version;
        QFile vf(QStringLiteral("/usr/local/mysql/VERSION"));
        if (vf.open(QIODevice::ReadOnly))
            version = QString::fromUtf8(vf.readLine()).trimmed();
        out << QVariantMap{
            {QStringLiteral("id"), QStringLiteral("system:mysql")},
            {QStringLiteral("engine"), QStringLiteral("mysql")},
            {QStringLiteral("name"), QStringLiteral("MySQL")},
            {QStringLiteral("version"), version},
            {QStringLiteral("port"), 3306},
            {QStringLiteral("running"), listening.contains(3306)},
            {QStringLiteral("source"), QStringLiteral("system")},
            {QStringLiteral("formula"), QString()},
            {QStringLiteral("controllable"), false},
        };
    }

    // EnterpriseDB / official PostgreSQL package.
    if (!haveBrewPostgres) {
        const QDir pgRoot(QStringLiteral("/Library/PostgreSQL"));
        for (const QString &v : pgRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (!QFileInfo::exists(pgRoot.filePath(v) + QStringLiteral("/bin/postgres")))
                continue;
            out << QVariantMap{
                {QStringLiteral("id"), QStringLiteral("system:postgresql:") + v},
                {QStringLiteral("engine"), QStringLiteral("postgresql")},
                {QStringLiteral("name"), QStringLiteral("PostgreSQL")},
                {QStringLiteral("version"), v},
                {QStringLiteral("port"), 5432},
                {QStringLiteral("running"), listening.contains(5432)},
                {QStringLiteral("source"), QStringLiteral("system")},
                {QStringLiteral("formula"), QString()},
                {QStringLiteral("controllable"), false},
            };
        }
    }

    Q_UNUSED(haveBrewMongo)
    return out;
}

bool controlDatabaseService(const QVariantMap &service, bool start)
{
    if (service.value(QStringLiteral("source")).toString() != QLatin1String("homebrew"))
        return false;
    const QString formula = service.value(QStringLiteral("formula")).toString();
    if (formula.isEmpty())
        return false;
    const QString brew = findBrew();
    if (brew.isEmpty())
        return false;
    return !runProcess(brew, {QStringLiteral("services"),
                              start ? QStringLiteral("start") : QStringLiteral("stop"),
                              formula},
                       15000)
                .isEmpty();
}
