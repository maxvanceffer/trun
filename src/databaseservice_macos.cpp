#include "databaseservice_backend.h"
#include "mackeychain.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryFile>
#include <algorithm>
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

QString findCli(const QString &engine, const QString &version, const QString &formula)
{
    QStringList candidates;
    if (engine == QLatin1String("postgresql")) {
        if (!version.isEmpty()) {
            candidates << QStringLiteral("/opt/homebrew/opt/postgresql@") + version + QStringLiteral("/bin/psql");
            candidates << QStringLiteral("/Library/PostgreSQL/") + version + QStringLiteral("/bin/psql");
            candidates << QStringLiteral("/Applications/Postgres.app/Contents/Versions/") + version + QStringLiteral("/bin/psql");
        }
        candidates << QStringLiteral("/opt/homebrew/bin/psql");
        candidates << QStringLiteral("/usr/local/bin/psql");
        candidates << QStandardPaths::findExecutable(QStringLiteral("psql"));
    } else if (engine == QLatin1String("mysql")) {
        if (formula.startsWith(QLatin1String("mysql@"))) {
            candidates << QStringLiteral("/opt/homebrew/opt/") + formula + QStringLiteral("/bin/mysql");
        }
        candidates << QStringLiteral("/opt/homebrew/bin/mysql");
        candidates << QStringLiteral("/usr/local/mysql/bin/mysql");
        candidates << QStandardPaths::findExecutable(QStringLiteral("mysql"));
    } else if (engine == QLatin1String("mongodb")) {
        candidates << QStringLiteral("/opt/homebrew/opt/mongodb-community/bin/mongosh");
        candidates << QStringLiteral("/opt/homebrew/bin/mongosh");
        candidates << QStandardPaths::findExecutable(QStringLiteral("mongosh"));
    } else if (engine == QLatin1String("redis")) {
        candidates << QStringLiteral("/opt/homebrew/bin/redis-cli");
        candidates << QStringLiteral("/usr/local/bin/redis-cli");
        candidates << QStandardPaths::findExecutable(QStringLiteral("redis-cli"));
    }
    for (const QString &c : std::as_const(candidates)) {
        if (!c.isEmpty() && QFileInfo::exists(c))
            return c;
    }
    return {};
}

// "3-04:12:33" / "04:12:33" / "12:33" (macOS ps etime) -> "3d 04h" / "04h 12m" / "12m".
QString formatUptime(const QString &etime)
{
    static const QRegularExpression re(
        QStringLiteral("^(?:(\\d+)-)?(\\d{1,2}):(\\d{2})(?::(\\d{2}))?$"));
    const auto m = re.match(etime.trimmed());
    if (!m.hasMatch())
        return {};
    const bool hasSeconds = m.hasMatch() && !m.captured(4).isEmpty();
    const int days = m.captured(1).toInt();
    const int h = hasSeconds ? m.captured(2).toInt() : 0;
    const int mo = hasSeconds ? m.captured(3).toInt() : m.captured(2).toInt();
    if (days > 0)
        return QStringLiteral("%1d %2h").arg(days).arg(h, 2, 10, QLatin1Char('0'));
    if (hasSeconds)
        return QStringLiteral("%1h %2m").arg(h, 2, 10, QLatin1Char('0')).arg(mo, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1m").arg(mo);
}

// Per-engine process metrics: short process name -> {pcpu, rssKb, uptime}.
// -e: bare `ps` is scoped to the caller's session and never sees launchd-
// daemonized background processes (postgres/mysqld/etc. would be invisible
// without it). ucomm (not comm): comm can print the full binary path on
// macOS for some processes; ucomm is the short "accounting name" that
// matches engineProcess()'s bare names ("postgres", "mysqld", ...).
QVariantMap processMetrics()
{
    QVariantMap m;
    QHash<QString, int> bestPid; // lowest PID kept per name
    const QString out = runProcess(QStringLiteral("ps"),
        {QStringLiteral("-eo"), QStringLiteral("pid=,ucomm=,pcpu=,rss=,etime=")});
    const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    static const QRegularExpression re(
        QStringLiteral("(\\d+)\\s+(\\S+)\\s+([\\d.]+)\\s+(\\d+)\\s+(\\S+)"));
    for (const QString &line : std::as_const(lines)) {
        auto mm = re.match(line);
        if (!mm.hasMatch())
            continue;
        const int pid = mm.captured(1).toInt();
        const QString name = mm.captured(2);
        // A daemon's forked workers (postgres's checkpointer/walwriter/...)
        // keep the parent's ucomm after fork(); only execve() changes it.
        // Keep the lowest-PID entry per name — the postmaster/parent was
        // forked first, so it reliably has the lowest PID in the family.
        if (bestPid.contains(name) && bestPid.value(name) <= pid)
            continue;
        bestPid.insert(name, pid);
        m.insert(name,
                QVariantMap{{QStringLiteral("pcpu"), mm.captured(3).toDouble()},
                            {QStringLiteral("rssKb"), mm.captured(4).toInt()},
                            {QStringLiteral("uptime"), formatUptime(mm.captured(5))}});
    }
    return m;
}

QString engineProcess(const QVariantMap &s)
{
    const QString e = s.value(QStringLiteral("engine")).toString();
    if (e == QLatin1String("postgresql"))
        return QStringLiteral("postgres");
    if (e == QLatin1String("mysql"))
        return QStringLiteral("mysqld");
    if (e == QLatin1String("mongodb"))
        return QStringLiteral("mongod");
    if (e == QLatin1String("redis"))
        return QStringLiteral("redis-server");
    return {};
}

bool versionLess(const QString &a, const QString &b)
{
    auto parts = [](const QString &v) {
        QList<int> p;
        for (const QString &s : v.split(QLatin1Char('.'), Qt::SkipEmptyParts))
            p << s.toInt();
        return p;
    };
    const QList<int> pa = parts(a);
    const QList<int> pb = parts(b);
    const int n = qMax(pa.size(), pb.size());
    for (int i = 0; i < n; ++i) {
        const int x = i < pa.size() ? pa.at(i) : 0;
        const int y = i < pb.size() ? pb.at(i) : 0;
        if (x != y)
            return x < y;
    }
    return false;
}

// ---------- Process capture (stdout + stderr + exit code + optional env) --

int runProcessCapture(const QString &program, const QStringList &args,
                      const QProcessEnvironment &env, int timeoutMs,
                      QString &out, QString &err)
{
    QProcess proc;
    if (!env.isEmpty())
        proc.setProcessEnvironment(env);
    proc.start(program, args);
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(1000);
        err = QStringLiteral("timed out");
        return -1;
    }
    out = QString::fromUtf8(proc.readAllStandardOutput());
    err = QString::fromUtf8(proc.readAllStandardError());
    return proc.exitCode();
}

// ---------- Connection config (QSettings) + Keychain (via `security`) ----

QString settingsGroup(const QVariantMap &service)
{
    return QStringLiteral("DatabaseConnections/")
        + service.value(QStringLiteral("id")).toString();
}

// "trun.db.postgresql@16" — matches the label shown in the Connection dialog.
QString keychainLabel(const QVariantMap &service)
{
    const QString engine = service.value(QStringLiteral("engine")).toString();
    const QString version = service.value(QStringLiteral("version")).toString();
    QString label = QStringLiteral("trun.db.") + engine;
    if (!version.isEmpty())
        label += QLatin1Char('@') + version;
    return label;
}

QString defaultAccount()
{
    const QString user = qEnvironmentVariable("USER");
    return user.isEmpty() ? QStringLiteral("postgres") : user;
}

// Non-secret fields only; no password.
QVariantMap rawConnectionConfig(const QVariantMap &service)
{
    QSettings settings;
    settings.beginGroup(settingsGroup(service));
    QVariantMap cfg;
    for (const char *key : {"host", "user", "database", "sslmode", "authMethod"}) {
        if (settings.contains(QLatin1String(key)))
            cfg.insert(QLatin1String(key), settings.value(QLatin1String(key)));
    }
    settings.endGroup();
    return cfg;
}

// Native Security-framework access (mackeychain.mm) — no `security` CLI, so
// the password never touches argv or a /dev/tty prompt.
bool keychainFindPassword(const QVariantMap &service, const QString &account, QString &password)
{
    return macKeychainFindPassword(keychainLabel(service), account, password);
}

bool keychainSavePassword(const QVariantMap &service, const QString &account, const QString &password)
{
    return macKeychainSetPassword(keychainLabel(service), account, password);
}

bool keychainDeletePassword(const QVariantMap &service, const QString &account)
{
    return macKeychainDeletePassword(keychainLabel(service), account);
}

// Resolved, ready-to-use connection parameters: saved config (if any) with
// sensible per-engine defaults, plus the Keychain password when a user was
// configured. Nothing configured -> host 127.0.0.1, no user/password, i.e.
// identical to the app's pre-credentials behavior (peer/no-auth).
struct ConnInfo {
    QString host = QStringLiteral("127.0.0.1");
    int port = 0;
    QString user;
    QString password;
    bool hasPassword = false;
    QString database;
    QString sslmode;
};

ConnInfo resolveConnection(const QVariantMap &service)
{
    ConnInfo c;
    c.port = service.value(QStringLiteral("port")).toInt();
    const QVariantMap cfg = rawConnectionConfig(service);
    const QString host = cfg.value(QStringLiteral("host")).toString();
    if (!host.isEmpty())
        c.host = host;
    c.user = cfg.value(QStringLiteral("user")).toString();
    c.database = cfg.value(QStringLiteral("database")).toString();
    c.sslmode = cfg.value(QStringLiteral("sslmode")).toString();
    if (!c.user.isEmpty() && keychainFindPassword(service, c.user, c.password))
        c.hasPassword = true;
    return c;
}

QString defaultDatabaseFor(const QString &engine)
{
    if (engine == QLatin1String("postgresql"))
        return QStringLiteral("postgres");
    if (engine == QLatin1String("mysql"))
        return QStringLiteral("mysql");
    if (engine == QLatin1String("mongodb"))
        return QStringLiteral("test");
    return {};
}

// ---------- Per-engine argument/env builders ------------------------------

QStringList postgresBaseArgs(const ConnInfo &c)
{
    QStringList a;
    a << QStringLiteral("-h") << c.host << QStringLiteral("-p") << QString::number(c.port);
    if (!c.user.isEmpty())
        a << QStringLiteral("-U") << c.user;
    return a;
}

QProcessEnvironment postgresEnv(const ConnInfo &c)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (c.hasPassword)
        env.insert(QStringLiteral("PGPASSWORD"), c.password);
    if (!c.sslmode.isEmpty())
        env.insert(QStringLiteral("PGSSLMODE"), c.sslmode);
    return env;
}

// MySQL has no password env var; a --defaults-extra-file keeps the secret
// out of argv (visible to any local user via `ps`) at the cost of a
// short-lived 0600 temp file, deleted by the QTemporaryFile destructor.
QStringList mysqlBaseArgs(const ConnInfo &c, QTemporaryFile &optFile)
{
    QStringList a;
    if (c.hasPassword) {
        optFile.setFileTemplate(QDir::tempPath() + QStringLiteral("/trun-mysql-XXXXXX.cnf"));
        if (optFile.open()) {
            optFile.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
            QTextStream ts(&optFile);
            ts << "[client]\npassword=" << c.password << "\n";
            optFile.flush();
            a << (QStringLiteral("--defaults-extra-file=") + optFile.fileName());
        }
    }
    a << QStringLiteral("-h") << c.host << QStringLiteral("-P") << QString::number(c.port);
    if (!c.user.isEmpty())
        a << QStringLiteral("-u") << c.user;
    return a;
}

// Host/port/database only — no credentials on argv. mongosh has neither a
// PGPASSWORD-style env var nor a stdin-fed prompt, so `-u`/`-p` would be the
// only way to authenticate from argv; auth instead runs from the script
// file mongoScriptFile() below writes (0600, deleted when it goes out of
// scope).
QStringList mongoBaseArgs(const ConnInfo &c, const QString &database)
{
    QStringList a;
    a << QStringLiteral("--host") << c.host << QStringLiteral("--port") << QString::number(c.port);
    if (!database.isEmpty())
        a << database;
    return a;
}

QString jsStringLiteral(const QString &s)
{
    QString out = s;
    out.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    out.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    out.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    return QLatin1Char('"') + out + QLatin1Char('"');
}

// Writes `body` to a 0600 temp .js file, prefixed with a db.auth() call when
// credentials are configured; returns the file path to pass to mongosh as a
// positional script argument instead of `--eval`.
QString mongoScriptFile(QTemporaryFile &file, const ConnInfo &c, const QString &body)
{
    file.setFileTemplate(QDir::tempPath() + QStringLiteral("/trun-mongo-XXXXXX.js"));
    if (!file.open())
        return {};
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    QTextStream ts(&file);
    if (!c.user.isEmpty()) {
        const QString authDb = c.database.isEmpty() ? QStringLiteral("admin") : c.database;
        ts << "db.getSiblingDB(" << jsStringLiteral(authDb) << ").auth("
           << jsStringLiteral(c.user) << ", " << jsStringLiteral(c.password) << ");\n";
    }
    ts << body << "\n";
    file.flush();
    return file.fileName();
}

QStringList redisBaseArgs(const ConnInfo &c)
{
    QStringList a;
    a << QStringLiteral("-h") << c.host << QStringLiteral("-p") << QString::number(c.port);
    if (c.hasPassword)
        a << QStringLiteral("--no-auth-warning");
    return a;
}

// redis-cli has no --pass-from-file; REDISCLI_AUTH is its documented way to
// keep the secret off argv.
QProcessEnvironment redisEnv(const ConnInfo &c)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (c.hasPassword)
        env.insert(QStringLiteral("REDISCLI_AUTH"), c.password);
    return env;
}

// Heuristic auth-failure classification per engine's error text.
bool classifyAuthError(const QString &engine, const QString &combined)
{
    if (engine == QLatin1String("postgresql"))
        return combined.contains(QLatin1String("password authentication failed"))
            || combined.contains(QLatin1String("role \""))
            || combined.contains(QLatin1String("no password supplied"));
    if (engine == QLatin1String("mysql"))
        return combined.contains(QLatin1String("Access denied for user"));
    if (engine == QLatin1String("mongodb"))
        return combined.contains(QLatin1String("Authentication failed"))
            || combined.contains(QLatin1String("AuthenticationFailed"));
    if (engine == QLatin1String("redis"))
        return combined.contains(QLatin1String("NOAUTH"))
            || combined.contains(QLatin1String("WRONGPASS"));
    return false;
}

// Splits `sep`-delimited output into a header row + data rows, dropping a
// trailing psql "(N rows)" footer when present.
void parseDelimited(const QString &out, QChar sep, QStringList &columns, QVariantList &rows)
{
    QStringList lines = out.split(QLatin1Char('\n'));
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
        lines.removeLast();
    if (lines.isEmpty())
        return;
    static const QRegularExpression footerRe(QStringLiteral("^\\(\\d+ rows?\\)$"));
    if (footerRe.match(lines.last().trimmed()).hasMatch())
        lines.removeLast();
    if (lines.isEmpty())
        return;
    columns = lines.takeFirst().split(sep);
    for (const QString &line : std::as_const(lines)) {
        QVariantList cells;
        for (const QString &cell : line.split(sep))
            cells << cell;
        rows << QVariant(cells);
    }
}

// ---------- Listening ports for a service's engine process ----------------

QVariantList listeningPortsForProcess(const QString &procName)
{
    QVariantList out;
    if (procName.isEmpty())
        return out;
    const QString rawOut = runProcess(QStringLiteral("lsof"),
                                      {QStringLiteral("-a"), QStringLiteral("-c"), procName,
                                       QStringLiteral("-nP"), QStringLiteral("-iTCP"),
                                       QStringLiteral("-sTCP:LISTEN")});
    static const QRegularExpression re(
        QStringLiteral("^(\\S+)\\s+(\\d+)\\s+.*\\s(\\S+):(\\d+)\\s*\\(LISTEN\\)\\s*$"));
    for (const QString &line : rawOut.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        if (line.startsWith(QLatin1String("COMMAND")))
            continue;
        const auto m = re.match(line);
        if (!m.hasMatch())
            continue;
        out << QVariantMap{
            {QStringLiteral("proc"), m.captured(1)},
            {QStringLiteral("pid"), m.captured(2)},
            {QStringLiteral("addr"), m.captured(3)},
            {QStringLiteral("port"), m.captured(4).toInt()},
        };
    }
    return out;
}

// ---------- Per-database detail --------------------------------------------

QVariantList postgresDatabases(const QVariantMap &service, const ConnInfo &conn,
                               QString &error, bool &authError)
{
    const QString cli = findCli(QStringLiteral("postgresql"),
                                service.value(QStringLiteral("version")).toString(),
                                service.value(QStringLiteral("formula")).toString());
    if (cli.isEmpty()) {
        error = QStringLiteral("psql not found");
        return {};
    }
    const QString adminDb = conn.database.isEmpty() ? QStringLiteral("postgres") : conn.database;
    const QProcessEnvironment env = postgresEnv(conn);

    QString out, err;
    QStringList listArgs = postgresBaseArgs(conn);
    listArgs << QStringLiteral("-d") << adminDb << QStringLiteral("-A") << QStringLiteral("-F")
             << QStringLiteral("\t") << QStringLiteral("-c")
             << QStringLiteral(
                    "SELECT d.datname, pg_size_pretty(pg_database_size(d.datname)), "
                    "(SELECT count(*) FROM pg_stat_activity a WHERE a.datname = d.datname) "
                    "FROM pg_database d WHERE datname NOT IN ('template0','template1') "
                    "ORDER BY d.datname");
    const int code = runProcessCapture(cli, listArgs, env, 8000, out, err);
    if (code != 0) {
        authError = classifyAuthError(QStringLiteral("postgresql"), out + err);
        error = err.trimmed().split(QLatin1Char('\n')).value(0);
        return {};
    }

    QString maxOut, maxErr;
    QStringList maxArgs = postgresBaseArgs(conn);
    maxArgs << QStringLiteral("-d") << adminDb << QStringLiteral("-t") << QStringLiteral("-A")
            << QStringLiteral("-c") << QStringLiteral("SHOW max_connections");
    runProcessCapture(cli, maxArgs, env, 5000, maxOut, maxErr);
    const QString maxConns = maxOut.trimmed();

    QStringList columns;
    QVariantList rawRows;
    parseDelimited(out, QLatin1Char('\t'), columns, rawRows);

    QVariantList result;
    for (const QVariant &rv : std::as_const(rawRows)) {
        const QVariantList cells = rv.toList();
        if (cells.size() < 3)
            continue;
        const QString name = cells.at(0).toString();
        QString tables = QStringLiteral("—");
        QString tOut, tErr;
        QStringList tArgs = postgresBaseArgs(conn);
        tArgs << QStringLiteral("-d") << name << QStringLiteral("-t") << QStringLiteral("-A")
              << QStringLiteral("-c")
              << QStringLiteral("SELECT count(*) FROM information_schema.tables "
                                "WHERE table_schema NOT IN ('pg_catalog','information_schema')");
        if (runProcessCapture(cli, tArgs, env, 5000, tOut, tErr) == 0 && !tOut.trimmed().isEmpty())
            tables = tOut.trimmed();

        result << QVariantMap{
            {QStringLiteral("name"), name},
            {QStringLiteral("size"), cells.at(1).toString()},
            {QStringLiteral("tables"), tables},
            {QStringLiteral("conns"),
             maxConns.isEmpty() ? cells.at(2).toString()
                                : cells.at(2).toString() + QStringLiteral(" / ") + maxConns},
        };
    }
    return result;
}

QVariantList mysqlDatabases(const QVariantMap &service, const ConnInfo &conn,
                            QString &error, bool &authError)
{
    const QString cli = findCli(QStringLiteral("mysql"),
                                service.value(QStringLiteral("version")).toString(),
                                service.value(QStringLiteral("formula")).toString());
    if (cli.isEmpty()) {
        error = QStringLiteral("mysql client not found");
        return {};
    }
    QTemporaryFile optFile;
    QStringList args = mysqlBaseArgs(conn, optFile);
    // LEFT JOIN from schemata, not a bare GROUP BY over information_schema.tables:
    // a database with zero tables (e.g. a freshly created "test") contributes no
    // rows to a tables-only aggregate and would silently vanish from the list.
    args << QStringLiteral("-N") << QStringLiteral("-B") << QStringLiteral("-e")
         << QStringLiteral(
                "SELECT s.schema_name, "
                "ROUND(COALESCE(SUM(t.data_length+t.index_length), 0)/1024/1024, 1), "
                "COUNT(t.table_name), "
                "(SELECT COUNT(*) FROM information_schema.processlist p "
                " WHERE p.db = s.schema_name) "
                "FROM information_schema.schemata s "
                "LEFT JOIN information_schema.tables t ON t.table_schema = s.schema_name "
                "WHERE s.schema_name NOT IN "
                "('mysql','information_schema','performance_schema','sys') "
                "GROUP BY s.schema_name");
    QString out, err;
    const int code = runProcessCapture(cli, args, QProcessEnvironment(), 8000, out, err);
    if (code != 0) {
        authError = classifyAuthError(QStringLiteral("mysql"), out + err);
        error = err.trimmed().split(QLatin1Char('\n')).value(0);
        return {};
    }
    QVariantList result;
    for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QStringList cells = line.split(QLatin1Char('\t'));
        if (cells.size() < 4)
            continue;
        result << QVariantMap{
            {QStringLiteral("name"), cells.at(0)},
            {QStringLiteral("size"), cells.at(1) + QStringLiteral(" MB")},
            {QStringLiteral("tables"), cells.at(2)},
            {QStringLiteral("conns"), cells.at(3)},
        };
    }
    return result;
}

QVariantList mongoDatabases(const QVariantMap &service, const ConnInfo &conn,
                            QString &error, bool &authError)
{
    const QString cli = findCli(QStringLiteral("mongodb"),
                                service.value(QStringLiteral("version")).toString(),
                                service.value(QStringLiteral("formula")).toString());
    if (cli.isEmpty()) {
        error = QStringLiteral("mongosh not found");
        return {};
    }
    QTemporaryFile scriptFile;
    QStringList args = mongoBaseArgs(conn, QString());
    args << QStringLiteral("--quiet")
         << mongoScriptFile(scriptFile, conn, QStringLiteral(
                "db.adminCommand('listDatabases').databases.forEach(d => "
                "print(d.name+'\\t'+d.sizeOnDisk+'\\t'+"
                "db.getSiblingDB(d.name).getCollectionInfos().length))"));
    QString out, err;
    const int code = runProcessCapture(cli, args, QProcessEnvironment(), 10000, out, err);
    if (code != 0) {
        authError = classifyAuthError(QStringLiteral("mongodb"), out + err);
        error = err.trimmed().split(QLatin1Char('\n')).value(0);
        return {};
    }
    QVariantList result;
    for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QStringList cells = line.split(QLatin1Char('\t'));
        if (cells.size() < 3)
            continue;
        const qint64 bytes = cells.at(1).toLongLong();
        const QString size = bytes >= (1024 * 1024)
            ? QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB")
            : QString::number(bytes / 1024.0, 'f', 0) + QStringLiteral(" KB");
        result << QVariantMap{
            {QStringLiteral("name"), cells.at(0)},
            {QStringLiteral("size"), size},
            {QStringLiteral("tables"), cells.at(2)},
            {QStringLiteral("conns"), QStringLiteral("—")},
        };
    }
    return result;
}

// List-view summary count ("N databases"/"N keys"). Uses the same saved
// credentials as the detail-view functions above — unlike the pre-credentials
// version this replaces, an auth failure here no longer silently reports 0.
int databaseCount(const QVariantMap &s, const ConnInfo &conn)
{
    const QString engine = s.value(QStringLiteral("engine")).toString();
    const QString version = s.value(QStringLiteral("version")).toString();
    const QString formula = s.value(QStringLiteral("formula")).toString();
    const QString cli = findCli(engine, version, formula);
    if (cli.isEmpty())
        return 0;

    QString out, err;
    const QString db = conn.database.isEmpty() ? defaultDatabaseFor(engine) : conn.database;

    if (engine == QLatin1String("postgresql")) {
        QStringList args = postgresBaseArgs(conn);
        args << QStringLiteral("-d") << db << QStringLiteral("-t") << QStringLiteral("-A")
             << QStringLiteral("-c")
             << QStringLiteral("SELECT count(*) FROM pg_database WHERE datname NOT IN ('template0','template1')");
        if (runProcessCapture(cli, args, postgresEnv(conn), 8000, out, err) != 0)
            return 0;
        return out.trimmed().toInt();
    }
    if (engine == QLatin1String("mysql")) {
        // Same system-schema exclusion as mysqlDatabases(), so this summary
        // count matches what the detail view's Databases table shows.
        QTemporaryFile optFile;
        QStringList args = mysqlBaseArgs(conn, optFile);
        args << QStringLiteral("-N") << QStringLiteral("-e")
             << QStringLiteral(
                    "SELECT COUNT(*) FROM information_schema.schemata "
                    "WHERE schema_name NOT IN "
                    "('mysql','information_schema','performance_schema','sys')");
        if (runProcessCapture(cli, args, QProcessEnvironment(), 8000, out, err) != 0)
            return 0;
        return out.trimmed().toInt();
    }
    if (engine == QLatin1String("mongodb")) {
        QTemporaryFile scriptFile;
        QStringList args = mongoBaseArgs(conn, QString());
        args << QStringLiteral("--quiet")
             << mongoScriptFile(scriptFile, conn,
                    QStringLiteral("print(db.adminCommand('listDatabases').databases.length)"));
        if (runProcessCapture(cli, args, QProcessEnvironment(), 8000, out, err) != 0)
            return 0;
        return out.trimmed().toInt();
    }
    if (engine == QLatin1String("redis")) {
        QStringList args = redisBaseArgs(conn);
        args << QStringLiteral("DBSIZE");
        if (runProcessCapture(cli, args, redisEnv(conn), 8000, out, err) != 0)
            return 0;
        return out.trimmed().toInt();
    }
    return 0;
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
            // Trust brew's own per-plist status exclusively (no port-listening
            // OR-fallback here): two installs of the same engine (e.g. an
            // unversioned "mysql" formula alongside "mysql@8.4") get the same
            // hardcoded default port from classifyFormula(), so falling back to
            // "is anything listening on that port" would mark BOTH as running
            // the instant either one binds it.
            entry.running = status == QLatin1String("started");
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

    // Attach per-service metrics (CPU/RSS) and, for running services, a
    // database/keys count; then sort running-first, engine, version (desc).
    const QVariantMap metrics = processMetrics();
    QVariantList sorted;
    for (const QVariant &v : std::as_const(out)) {
        QVariantMap s = v.toMap();
        const QVariantMap pm = metrics.value(engineProcess(s)).toMap();
        s.insert(QStringLiteral("pcpu"), pm.value(QStringLiteral("pcpu"), 0.0));
        s.insert(QStringLiteral("rss"), pm.value(QStringLiteral("rssKb"), 0).toInt() / 1024.0);
        s.insert(QStringLiteral("uptime"), pm.value(QStringLiteral("uptime"), QString()));
        if (s.value(QStringLiteral("running")).toBool())
            s.insert(QStringLiteral("dbCount"), databaseCount(s, resolveConnection(s)));
        sorted << s;
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const QVariant &a, const QVariant &b) {
                  const QVariantMap x = a.toMap();
                  const QVariantMap y = b.toMap();
                  const bool rx = x.value(QStringLiteral("running")).toBool();
                  const bool ry = y.value(QStringLiteral("running")).toBool();
                  if (rx != ry)
                      return rx;
                  const QString ex = x.value(QStringLiteral("engine")).toString();
                  const QString ey = y.value(QStringLiteral("engine")).toString();
                  if (ex != ey)
                      return ex < ey;
                  // version descending
                  return versionLess(y.value(QStringLiteral("version")).toString(),
                                    x.value(QStringLiteral("version")).toString());
              });
    return sorted;
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

QVariantList serviceListeningPorts(const QVariantMap &service)
{
    if (!service.value(QStringLiteral("running")).toBool())
        return {};
    return listeningPortsForProcess(engineProcess(service));
}

QVariantList serviceDatabases(const QVariantMap &service, QString &error, bool &authError)
{
    error.clear();
    authError = false;
    if (!service.value(QStringLiteral("running")).toBool())
        return {};
    const QString engine = service.value(QStringLiteral("engine")).toString();
    const ConnInfo conn = resolveConnection(service);
    if (engine == QLatin1String("postgresql"))
        return postgresDatabases(service, conn, error, authError);
    if (engine == QLatin1String("mysql"))
        return mysqlDatabases(service, conn, error, authError);
    if (engine == QLatin1String("mongodb"))
        return mongoDatabases(service, conn, error, authError);
    return {}; // Redis: no per-database table/size/conn concept.
}

QVariantMap connectionConfig(const QVariantMap &service)
{
    QVariantMap cfg = rawConnectionConfig(service);
    const QString user = cfg.value(QStringLiteral("user")).toString();
    const QString account = user.isEmpty() ? defaultAccount() : user;
    QString dummy;
    cfg[QStringLiteral("hasSavedPassword")] = !user.isEmpty()
        && keychainFindPassword(service, account, dummy);
    return cfg;
}

bool saveConnectionConfig(const QVariantMap &service, const QVariantMap &config,
                          const QString &password)
{
    QSettings settings;
    settings.beginGroup(settingsGroup(service));
    for (auto it = config.constBegin(); it != config.constEnd(); ++it)
        settings.setValue(it.key(), it.value());
    settings.endGroup();
    settings.sync();

    if (password.isNull())
        return true; // keep whatever secret (if any) is already stored
    const QString user = config.value(QStringLiteral("user")).toString();
    const QString account = user.isEmpty() ? defaultAccount() : user;
    return keychainSavePassword(service, account, password);
}

bool forgetConnectionConfig(const QVariantMap &service)
{
    const QVariantMap cfg = rawConnectionConfig(service);
    const QString user = cfg.value(QStringLiteral("user")).toString();
    const QString account = user.isEmpty() ? defaultAccount() : user;

    QSettings settings;
    settings.beginGroup(settingsGroup(service));
    settings.remove(QString());
    settings.endGroup();
    settings.sync();

    return keychainDeletePassword(service, account);
}

bool testConnection(const QVariantMap &service, QString &detail, QString &error, bool &authError)
{
    detail.clear();
    error.clear();
    authError = false;
    const QString engine = service.value(QStringLiteral("engine")).toString();
    const QString version = service.value(QStringLiteral("version")).toString();
    const QString formula = service.value(QStringLiteral("formula")).toString();
    const QString cli = findCli(engine, version, formula);
    if (cli.isEmpty()) {
        error = QStringLiteral("client not found");
        return false;
    }
    const ConnInfo conn = resolveConnection(service);
    const QString db = conn.database.isEmpty() ? defaultDatabaseFor(engine) : conn.database;

    QElapsedTimer timer;
    timer.start();
    QString out, err;
    int code = -1;

    if (engine == QLatin1String("postgresql")) {
        QStringList args = postgresBaseArgs(conn);
        args << QStringLiteral("-d") << db << QStringLiteral("-t") << QStringLiteral("-A")
             << QStringLiteral("-c") << QStringLiteral("SHOW server_version");
        code = runProcessCapture(cli, args, postgresEnv(conn), 8000, out, err);
    } else if (engine == QLatin1String("mysql")) {
        QTemporaryFile optFile;
        QStringList args = mysqlBaseArgs(conn, optFile);
        args << QStringLiteral("-N") << QStringLiteral("-B") << QStringLiteral("-e")
             << QStringLiteral("SELECT VERSION()");
        code = runProcessCapture(cli, args, QProcessEnvironment(), 8000, out, err);
    } else if (engine == QLatin1String("mongodb")) {
        QTemporaryFile scriptFile;
        QStringList args = mongoBaseArgs(conn, db);
        args << QStringLiteral("--quiet")
             << mongoScriptFile(scriptFile, conn, QStringLiteral("print(db.version())"));
        code = runProcessCapture(cli, args, QProcessEnvironment(), 8000, out, err);
    } else if (engine == QLatin1String("redis")) {
        QStringList args = redisBaseArgs(conn);
        args << QStringLiteral("PING");
        code = runProcessCapture(cli, args, redisEnv(conn), 8000, out, err);
    } else {
        error = QStringLiteral("unsupported engine");
        return false;
    }

    const qint64 ms = timer.elapsed();
    if (code != 0) {
        authError = classifyAuthError(engine, out + err);
        error = err.trimmed().split(QLatin1Char('\n')).value(0);
        if (error.isEmpty())
            error = QStringLiteral("connection failed");
        return false;
    }

    const QString as = conn.user.isEmpty() ? defaultAccount() : conn.user;
    detail = QStringLiteral("as %1 · server %2 · %3 ms")
                 .arg(as, out.trimmed().split(QLatin1Char('\n')).value(0), QString::number(ms));
    return true;
}

QVariantMap runDatabaseQuery(const QVariantMap &service, const QString &database,
                             const QString &sql)
{
    QVariantMap result{
        {QStringLiteral("rows"), QVariantList()},
        {QStringLiteral("columns"), QStringList()},
        {QStringLiteral("error"), QString()},
        {QStringLiteral("authError"), false},
        {QStringLiteral("elapsedMs"), qint64(0)},
    };

    const QString engine = service.value(QStringLiteral("engine")).toString();
    const QString version = service.value(QStringLiteral("version")).toString();
    const QString formula = service.value(QStringLiteral("formula")).toString();
    const QString cli = findCli(engine, version, formula);
    if (cli.isEmpty()) {
        result[QStringLiteral("error")] = QStringLiteral("client not found");
        return result;
    }

    const ConnInfo conn = resolveConnection(service);
    const QString db = !database.isEmpty() ? database
        : (!conn.database.isEmpty() ? conn.database : defaultDatabaseFor(engine));

    QElapsedTimer timer;
    timer.start();
    QString out, err;
    int code = -1;
    QStringList columns;
    QVariantList rows;

    if (engine == QLatin1String("postgresql")) {
        QStringList args = postgresBaseArgs(conn);
        args << QStringLiteral("-d") << db << QStringLiteral("-A") << QStringLiteral("-F")
             << QStringLiteral("\t") << QStringLiteral("-c") << sql;
        code = runProcessCapture(cli, args, postgresEnv(conn), 15000, out, err);
        if (code == 0)
            parseDelimited(out, QLatin1Char('\t'), columns, rows);
    } else if (engine == QLatin1String("mysql")) {
        QTemporaryFile optFile;
        QStringList args = mysqlBaseArgs(conn, optFile);
        args << QStringLiteral("-D") << db << QStringLiteral("-B") << QStringLiteral("-e") << sql;
        code = runProcessCapture(cli, args, QProcessEnvironment(), 15000, out, err);
        if (code == 0)
            parseDelimited(out, QLatin1Char('\t'), columns, rows);
    } else if (engine == QLatin1String("mongodb")) {
        QTemporaryFile scriptFile;
        QStringList args = mongoBaseArgs(conn, db);
        args << QStringLiteral("--quiet") << mongoScriptFile(scriptFile, conn, sql);
        code = runProcessCapture(cli, args, QProcessEnvironment(), 15000, out, err);
        if (code == 0) {
            // No tabular shape for arbitrary eval output: one "result" column,
            // one row per non-empty output line.
            columns << QStringLiteral("result");
            for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
                rows << QVariant(QVariantList{line});
        }
    } else if (engine == QLatin1String("redis")) {
        QStringList args = redisBaseArgs(conn);
        for (const QString &tok : sql.split(QLatin1Char(' '), Qt::SkipEmptyParts))
            args << tok;
        code = runProcessCapture(cli, args, redisEnv(conn), 15000, out, err);
        if (code == 0) {
            columns << QStringLiteral("result");
            for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
                rows << QVariant(QVariantList{line});
        }
    } else {
        result[QStringLiteral("error")] = QStringLiteral("unsupported engine");
        return result;
    }

    result[QStringLiteral("elapsedMs")] = timer.elapsed();
    if (code != 0) {
        result[QStringLiteral("authError")] = classifyAuthError(engine, out + err);
        QString firstLine = err.trimmed().split(QLatin1Char('\n')).value(0);
        if (firstLine.isEmpty())
            firstLine = out.trimmed().split(QLatin1Char('\n')).value(0);
        result[QStringLiteral("error")] =
            firstLine.isEmpty() ? QStringLiteral("query failed") : firstLine;
        return result;
    }

    result[QStringLiteral("columns")] = columns;
    result[QStringLiteral("rows")] = rows;
    return result;
}
