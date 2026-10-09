#include "frameworkconsole.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMetaObject>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QThread>

#ifndef Q_OS_WINDOWS
#include <unistd.h>
#endif

namespace {

// GUI apps (Finder launch) get a minimal PATH while tools live in
// Homebrew, /usr/local, ~/.local. Enrich it once so `php` resolves
// the same way as in CommandExecutor.
QString enrichedPath()
{
#ifdef Q_OS_WINDOWS
    return {};
#else
    static QString cached;
    static bool done = false;
    if (done)
        return cached;
    done = true;
    QStringList seen =
        QProcessEnvironment::systemEnvironment().value(QStringLiteral("PATH"))
            .split(QLatin1Char(':'), Qt::SkipEmptyParts);
    auto addMissing = [&](const QString &p) {
        if (!p.isEmpty() && !seen.contains(p))
            seen << p;
    };
    for (const char *shell : {"/bin/zsh", "/bin/bash"}) {
        if (!QFileInfo::exists(QString::fromUtf8(shell)))
            continue;
        QProcess p;
        p.start(QString::fromUtf8(shell), {"-l", "-c", "printf %s \"$PATH\""});
        if (!p.waitForFinished(2000))
            continue;
        const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        if (!out.isEmpty()) {
            for (const QString &dir : out.split(QLatin1Char(':'), Qt::SkipEmptyParts))
                addMissing(dir);
            break;
        }
    }
    addMissing(QStringLiteral("/opt/homebrew/bin"));
    addMissing(QStringLiteral("/opt/homebrew/sbin"));
    addMissing(QStringLiteral("/usr/local/bin"));
    const QString home = QDir::homePath();
    if (!home.isEmpty()) {
        addMissing(home + QStringLiteral("/.local/bin"));
        addMissing(home + QStringLiteral("/.cargo/bin"));
    }
    return seen.join(QLatin1Char(':'));
#endif
}

QString strOf(const QJsonValue &v)
{
    return v.isString() ? v.toString() : QString();
}

} // namespace

const QList<FrameworkConsole::FrameworkDef> &FrameworkConsole::registry()
{
    static const QList<FrameworkDef> defs = {
        {"Symfony", "symfony/framework-bundle", "bin/console", "list --format=json", {}, "src/Command"},
        {"Laravel", "laravel/framework", "artisan", "list --format=json", {}, "app/Console/Commands"},
    };
    return defs;
}

FrameworkConsole::FrameworkConsole(QObject *parent) : QObject(parent) {}

FrameworkConsole::~FrameworkConsole()
{
    // Iterate a snapshot of the keys: waitForFinished() pumps the event
    // loop, so the finished handler below runs and erases its
    // m_processes entry while we iterate — holding a QMap iterator
    // across that would be invalidated (crash). Entries removed in the
    // meantime are skipped by the contains() re-check; the process
    // object itself stays alive until deleteLater() is delivered.
    const QList<QString> keys = m_processes.keys();
    for (const QString &key : keys) {
        if (!m_processes.contains(key))
            continue;
        QProcess *process = m_processes.value(key);
        if (process && process->state() != QProcess::NotRunning) {
            process->kill();
            process->waitForFinished(2000);
        }
    }
}

QString FrameworkConsole::detectFramework(const QString &projectPath,
                                         const QJsonObject &composerDoc) const
{
    const QJsonObject require = composerDoc.value("require").toObject();
    for (const FrameworkDef &def : registry()) {
        if (require.contains(def.requireMarker) &&
            QFile(projectPath + QStringLiteral("/") + def.consoleBinary).exists())
            return def.name;
    }
    return {};
}

FrameworkConsole::FrameworkDef FrameworkConsole::findDef(const QString &name) const
{
    for (const FrameworkDef &def : registry())
        if (def.name == name)
            return def;
    return {};
}

QStringList FrameworkConsole::frameworkNames()
{
    QStringList names;
    for (const FrameworkDef &def : registry())
        names << def.name;
    return names;
}

QString FrameworkConsole::commandDirFor(const QString &framework) const
{
    for (const FrameworkDef &def : registry())
        if (def.name == framework)
            return def.commandDir;
    return {};
}

bool FrameworkConsole::discoveryInProgress(const QString &projectPath) const
{
    QProcess *process = m_processes.value(projectPath, nullptr);
    return process && process->state() != QProcess::NotRunning;
}

QVariantList FrameworkConsole::parseListing(const QString &framework,
                                            const QString &binary,
                                            const QByteArray &raw)
{
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject())
        return {};
    const QJsonArray commands = doc.object().value("commands").toArray();
    QVariantList records;
    for (const QJsonValue &entry : commands) {
        const QJsonObject cmd = entry.toObject();
        const QString name = cmd.value("name").toString();
        if (name.isEmpty())
            continue;
        const QJsonObject definition = cmd.value("definition").toObject();
        const QJsonObject args = definition.value("arguments").toObject();
        const QJsonObject options = definition.value("options").toObject();

        // Positional arguments, declaration order. Only those with a
        // default value enter the assembled default invocation;
        // defaults are not passed otherwise.
        QStringList defaultArgs;
        QVariantList meta;
        for (auto it = args.begin(); it != args.end(); ++it) {
            const QJsonObject arg = it.value().toObject();
            QVariantMap m{
                {"name", it.key()},
                {"kind", QStringLiteral("positional")},
                {"required", arg.value("is_required").toBool() || arg.value("required").toBool()},
                {"description", arg.value("description").toString()},
            };
            const QJsonValue def = arg.value("default");
            if (!def.isNull()) {
                m.insert("default", def.toString());
                if (!def.toString().isEmpty())
                    defaultArgs << def.toString();
            }
            if (arg.value("is_array").toBool())
                m.insert("multiple", true);
            meta << m;
        }
        // Options, declaration order. Negatable twins ("no-<name>" entries
        // present in the JSON) are not listed as their own meta entry;
        // the flag is marked on the base option.
        for (auto it = options.begin(); it != options.end(); ++it) {
            const QString key = it.key();
            if (key.startsWith(QLatin1String("no-")) && options.contains(key.mid(3)))
                continue;
            const QJsonObject opt = it.value().toObject();
            QVariantMap m{
                {"name", key},
                {"kind", QStringLiteral("option")},
                {"required", false},
                {"valueRequired", opt.value("is_value_required").toBool()
                                     || opt.value("requires_value").toBool()},
                // VALUE_OPTIONAL ([=VALUE]) accepts a value without
                // requiring one: like VALUE_REQUIRED it renders as an
                // input, not a flag chip. Only VALUE_NONE is a flag.
                {"acceptValue", opt.value("accept_value").toBool()},
                {"negatable", options.contains(QLatin1String("no-") + key)},
                {"hidden", opt.value("is_hidden").toBool() || opt.value("hidden").toBool()},
                {"description", opt.value("description").toString()},
            };
            const QJsonValue def = opt.value("default");
            if (!def.isNull())
                m.insert("default", def.toString());
            if (opt.value("is_multiple").toBool())
                m.insert("multiple", true);
            const QString shortcut = strOf(opt.value("shortcut"));
            if (!shortcut.isEmpty())
                m.insert("shortcut", shortcut);
            meta << m;
        }

        // Assembled default invocation: command name first, then
        // positional defaults (declaration order), options omitted
        // (defaults are not passed), --env/-n always appended at the end.
        QStringList invocation;
        invocation << name;
        invocation += defaultArgs;
        invocation << QStringLiteral("--env");
        invocation << QStringLiteral("-n");

        QJsonObject record{
            {"id", framework + QLatin1Char(':') + name},
            {"category", framework},
            {"label", name},
            {"command", binary},
            {"description", cmd.value("description").toString()},
            {"hidden", cmd.value("hidden").toBool()},
            {"args", QJsonArray::fromStringList(invocation)},
            {"argsMeta", QJsonArray::fromVariantList(meta)},
            {"detectedProgram", binary},
        };
        records << record;
    }
    return records;
}

void FrameworkConsole::startDiscovery(const QString &projectPath)
{
    if (discoveryInProgress(projectPath))
        return; // discovery already in flight for this project

    QFile composer(projectPath + QStringLiteral("/composer.json"));
    if (!composer.open(QIODevice::ReadOnly)) {
        emit discoveryFailed(projectPath, QStringLiteral("composer.json not found"));
        return;
    }
    const QJsonDocument composerDoc = QJsonDocument::fromJson(composer.readAll());
    const QString framework = detectFramework(projectPath, composerDoc.object());
    if (framework.isEmpty()) {
        emit discoveryFailed(projectPath, QStringLiteral("framework not detected"));
        return;
    }
    const FrameworkDef def = findDef(framework);
    const QString binary = def.consoleBinary;

    QProcess *process = new QProcess(this);
    m_processes.insert(projectPath, process);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString path = enrichedPath();
    if (!path.isEmpty())
        env.insert(QStringLiteral("PATH"), path);
    process->setProcessEnvironment(env);
    process->setWorkingDirectory(projectPath);

    QStringList args{binary};
    args += def.listCommand.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    args += def.extraDefaultArgs;

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, projectPath, framework, binary, process](int exitCode, QProcess::ExitStatus) {
        if (exitCode != 0) {
            QString reason;
            if (exitCode == 127)
                reason = QStringLiteral("php not found in PATH");
            else if (exitCode == 255)
                reason = QStringLiteral("vendor missing or fatal error (exit 255)");
            else
                reason = QStringLiteral("console exited with code %1").arg(exitCode);
            m_processes.remove(projectPath);
            process->deleteLater();
            emit discoveryFailed(projectPath, reason);
            return;
        }
        // Heavy JSON parsing runs off the GUI thread. The finished signal
        // fires exactly once per process, so the worker below fires exactly
        // once. The map entry and the process stay alive until the queued
        // cleanup below runs: discoveryInProgress keeps returning true
        // during the worker's lifetime, so a second startDiscovery for
        // the same project is ignored until this one completes.
        const QByteArray output = process->readAllStandardOutput();
        QThread *thread = QThread::create([this, projectPath, framework, binary, process, output]() {
            const QVariantList records = parseListing(framework, binary, output);
            QMetaObject::invokeMethod(
                this,
                [this, projectPath, process, records]() {
                    m_processes.remove(projectPath);
                    process->deleteLater();
                    if (records.isEmpty())
                        emit discoveryFailed(projectPath,
                                           QStringLiteral("console produced no parseable JSON"));
                    else
                        emit discoveryFinished(projectPath, records);
                },
                Qt::QueuedConnection);
        });
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        thread->start();
    });
    connect(process, &QProcess::errorOccurred, this, [this, projectPath, process](QProcess::ProcessError error) {
        QString reason = process->errorString();
        if (error == QProcess::ProcessError::FailedToStart)
            reason += QStringLiteral(" (php not found)");
        m_processes.remove(projectPath);
        process->deleteLater();
        emit discoveryFailed(projectPath, reason);
    });

    process->start(QStringLiteral("php"), args);
}

QString FrameworkConsole::readFrameworkVersion(const QString &projectPath) const
{
    QFile composer(projectPath + QStringLiteral("/composer.json"));
    if (!composer.open(QIODevice::ReadOnly))
        return {};
    const QJsonDocument composerDoc = QJsonDocument::fromJson(composer.readAll());
    if (!composerDoc.isObject())
        return {};
    const QJsonObject require = composerDoc.object().value("require").toObject();
    QString marker;
    for (const FrameworkDef &def : registry()) {
        if (require.contains(def.requireMarker)) {
            marker = def.requireMarker;
            break;
        }
    }
    if (marker.isEmpty())
        return {};
    QFile lock(projectPath + QStringLiteral("/composer.lock"));
    if (!lock.open(QIODevice::ReadOnly))
        return {};
    const QJsonDocument lockDoc = QJsonDocument::fromJson(lock.readAll());
    if (!lockDoc.isObject())
        return {};
    const QJsonArray packages = lockDoc.object().value("packages").toArray();
    for (const QJsonValue &p : packages) {
        const QJsonObject pkg = p.toObject();
        if (pkg.value("name").toString() == marker)
            return pkg.value("version").toString();
    }
    return {};
}
