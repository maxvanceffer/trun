#include "commandexecutor.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>

#ifndef Q_OS_WINDOWS
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#ifdef Q_OS_MACOS
#include <sys/sysctl.h>
#endif

namespace {

bool pidAliveWithCommand(int pid, const QString &command)
{
#ifdef Q_OS_WINDOWS
    Q_UNUSED(pid);
    Q_UNUSED(command);
    return false;
#else
    if (pid <= 0 || ::kill(static_cast<pid_t>(pid), 0) != 0)
        return false;
    // Guard against PID reuse: the live command line must mention our executable
    QProcess ps;
    ps.start("ps", {"-p", QString::number(pid), "-o", "command="});
    if (!ps.waitForFinished(3000))
        return true; // ps unavailable: trust kill()
    const QString out = QString::fromUtf8(ps.readAllStandardOutput());
    const QString base = QFileInfo(command).fileName();
    return !base.isEmpty() && out.contains(base);
#endif
}

} // namespace

CommandExecutor::CommandExecutor(QObject *parent) : QObject(parent) {}

int CommandExecutor::run(const QString &command, const QStringList &args,
                         const QString &workingDir, const QString &label,
                         const QString &commandId)
{
    return runWithEnv(command, args, workingDir, label, commandId, {});
}

int CommandExecutor::runWithEnv(const QString &command, const QStringList &args,
                                const QString &workingDir, const QString &label,
                                const QString &commandId, const QStringList &envPairs)
{
    if (command.isEmpty())
        return 0;

    const int id = m_nextId++;

    auto *process = new QProcess(this);
    if (!workingDir.isEmpty())
        process->setWorkingDirectory(workingDir);
    if (!envPairs.isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        for (const QString &pair : envPairs) {
            const int eq = pair.indexOf(QLatin1Char('='));
            if (eq > 0)
                env.insert(pair.left(eq).trimmed(), pair.mid(eq + 1).trimmed());
        }
        process->setProcessEnvironment(env);
    }

    m_processes.insert(id, Proc{process, {}, {}, label.isEmpty() ? command : label, commandId,
                               QDateTime::currentDateTime()});
    emit runningCommandsChanged();

    connect(process, &QProcess::started, this, [this, id]() {
        auto it = m_processes.find(id);
        if (it == m_processes.end())
            return;
        emit started(id, static_cast<int>(it->process->processId()), it->label,
                     it->process->program() + QLatin1Char(' ') + it->process->arguments().join(QLatin1Char(' ')),
                     it->commandId);
    });

    connect(process, &QProcess::readyReadStandardOutput, this, [this, id]() {
        flushReady(id, false);
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, id]() {
        flushReady(id, true);
    });

    connect(process, &QProcess::errorOccurred, this, [this, id](QProcess::ProcessError) {
        auto it = m_processes.find(id);
        if (it == m_processes.end())
            return;
        if (m_killRequested.contains(id))
            return; // intentional stop: finished() will report it
        finishWithFailure(id, it->process->errorString());
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, id](int exitCode, QProcess::ExitStatus) {
        auto it = m_processes.find(id);
        if (it == m_processes.end())
            return;
        flushReady(id, false);
        flushReady(id, true);
        const QString label = it->label;
        const QString commandId = it->commandId;
        cleanup(id);
        emit finished(id, label, exitCode, commandId);
        emit runningCommandsChanged();
    });

    process->start(command, args);
    return id;
}

void CommandExecutor::stop(int id)
{
    auto it = m_processes.find(id);
    if (it == m_processes.end())
        return;
    // finished() will follow from kill() and do the cleanup + notify
    m_killRequested.insert(id);
    it->process->kill();
}

bool CommandExecutor::isRunning(int id) const
{
    return m_processes.contains(id);
}

int CommandExecutor::runningCount() const
{
    return m_processes.size() + m_external.size();
}

void CommandExecutor::killAll()
{
    const QList<int> ids = m_processes.keys();
    for (int id : ids)
        stop(id);
    const QList<int> extIds = m_external.keys();
    for (int id : extIds)
        stopCommand(m_external.value(id).commandId);
}

void CommandExecutor::stopCommand(const QString &commandId)
{
    if (commandId.isEmpty())
        return;
    const QList<int> ids = m_processes.keys();
    for (int id : ids) {
        if (m_processes.value(id).commandId == commandId)
            stop(id);
    }
#ifndef Q_OS_WINDOWS
    QList<int> gone;
    for (auto it = m_external.begin(); it != m_external.end(); ++it) {
        if (it->commandId != commandId)
            continue;
        ::kill(static_cast<pid_t>(it->pid), SIGKILL);
        emit finished(it.key(), it->label, -1, it->commandId);
        gone << it.key();
    }
    for (int id : gone)
        m_external.erase(m_external.find(id));
    if (!gone.isEmpty())
        emit runningCommandsChanged();
#endif
}

QStringList CommandExecutor::runningCommandIds() const
{
    QSet<QString> ids;
    for (const auto &proc : m_processes) {
        if (!proc.commandId.isEmpty())
            ids.insert(proc.commandId);
    }
    for (const auto &ext : m_external) {
        if (!ext.commandId.isEmpty())
            ids.insert(ext.commandId);
    }
    return ids.values();
}

QVariantList CommandExecutor::runningProcesses() const
{
    QVariantList out;
    for (const auto &proc : m_processes) {
        const int pid = static_cast<int>(proc.process->processId());
        if (pid <= 0)
            continue; // not started yet
        out << QVariantMap{
            {"pid", pid},
            {"command", proc.process->program()},
            {"args", proc.process->arguments()},
            {"workingDir", proc.process->workingDirectory()},
            {"label", proc.label},
            {"commandId", proc.commandId},
            {"startedAt", proc.startTime.toString(Qt::ISODateWithMs)},
        };
    }
    for (const auto &ext : m_external) {
        out << QVariantMap{
            {"pid", ext.pid},
            {"command", ext.command},
            {"args", ext.args},
            {"workingDir", ext.workingDir},
            {"label", ext.label},
            {"commandId", ext.commandId},
            {"startedAt", ext.startTime.toString(Qt::ISODateWithMs)},
        };
    }
    return out;
}

bool CommandExecutor::adoptExternal(int pid, const QString &command, const QStringList &args,
                                    const QString &workingDir, const QString &label,
                                    const QString &commandId,
                                    const QString &startedAtIso)
{
    if (commandId.isEmpty() || !pidAliveWithCommand(pid, command))
        return false;
    QDateTime startTime = QDateTime::fromString(startedAtIso, Qt::ISODateWithMs);
    if (!startTime.isValid())
        startTime = QDateTime::currentDateTime();
    const int id = m_nextId++;
    m_external.insert(id, ExternalProc{pid, command, args, workingDir,
                                       label.isEmpty() ? command : label, commandId,
                                       startTime});
    emit runningCommandsChanged();
    return true;
}

bool CommandExecutor::revealFolder(const QString &path)
{
    QDir dir(path);
    if (path.isEmpty() || !dir.exists())
        return false;
    // No QtGui dependency: delegate to the OS file manager via QProcess (Core only)
#if defined(Q_OS_MACOS)
    return QProcess::startDetached("open", {dir.absolutePath()});
#elif defined(Q_OS_WINDOWS)
    return QProcess::startDetached("explorer", {QDir::toNativeSeparators(dir.absolutePath())});
#else
    return QProcess::startDetached("xdg-open", {dir.absolutePath()});
#endif
}

bool CommandExecutor::portBusy(int port)
{
#ifndef Q_OS_WINDOWS
    if (port <= 0 || port > 65535)
        return false;
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return false;
    struct timeval timeout { 1, 0 };
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    // A connect() interrupted by SIGCHLD fails with EINTR; retrying it on
    // BSD yields EISCONN/EALREADY — all three mean the port is busy.
    int rc;
    do {
        rc = ::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    } while (rc != 0 && errno == EINTR);
    const bool busy = rc == 0 || errno == EISCONN || errno == EALREADY;
    ::close(fd);
    return busy;
#else
    Q_UNUSED(port);
    return false;
#endif
}

int CommandExecutor::pidOnPort(int port)
{
#ifndef Q_OS_WINDOWS
    if (port <= 0 || port > 65535)
        return 0;
    QProcess lsof;
    lsof.start("lsof", {"-ti", QStringLiteral(":") + QString::number(port)});
    if (!lsof.waitForFinished(3000))
        return 0;
    return QString::fromUtf8(lsof.readAllStandardOutput()).split(u'\n').value(0).toInt();
#else
    Q_UNUSED(port);
    return 0;
#endif
}

bool CommandExecutor::killExternal(int pid)
{
#ifndef Q_OS_WINDOWS
    if (pid <= 1 || pid == static_cast<int>(::getpid()))
        return false;
    return ::kill(static_cast<pid_t>(pid), SIGTERM) == 0;
#else
    Q_UNUSED(pid);
    return false;
#endif
}

int CommandExecutor::pidForCommand(const QString &commandId) const
{
    if (commandId.isEmpty())
        return 0;
    for (const auto &proc : m_processes) {
        if (proc.commandId != commandId)
            continue;
        const int pid = static_cast<int>(proc.process->processId());
        if (pid > 0)
            return pid;
    }
    for (const auto &ext : m_external) {
        if (ext.commandId == commandId && ext.pid > 0)
            return ext.pid;
    }
    return 0;
}

qlonglong CommandExecutor::memoryForCommand(const QString &commandId)
{
    const int pid = pidForCommand(commandId);
    if (pid <= 0)
        return -1;
    refreshStatCache();
    const auto it = m_statCache.find(pid);
    return it == m_statCache.end() ? -1 : it->rssKb;
}

double CommandExecutor::cpuForCommand(const QString &commandId)
{
    const int pid = pidForCommand(commandId);
    if (pid <= 0)
        return -1.0;
    refreshStatCache();
    const auto it = m_statCache.find(pid);
    return it == m_statCache.end() ? -1.0 : it->cpuPercent;
}

qlonglong CommandExecutor::elapsedForCommand(const QString &commandId) const
{
    if (commandId.isEmpty())
        return -1;
    for (const auto &proc : m_processes) {
        if (proc.commandId == commandId && proc.startTime.isValid())
            return proc.startTime.secsTo(QDateTime::currentDateTime());
    }
    for (const auto &ext : m_external) {
        if (ext.commandId == commandId && ext.startTime.isValid())
            return ext.startTime.secsTo(QDateTime::currentDateTime());
    }
    return -1;
}

qlonglong CommandExecutor::totalMemoryKb() const
{
#ifdef Q_OS_MACOS
    uint64_t bytes = 0;
    size_t size = sizeof(bytes);
    if (::sysctlbyname("hw.memsize", &bytes, &size, nullptr, 0) != 0)
        return -1;
    return static_cast<qlonglong>(bytes / 1024);
#elif defined(Q_OS_LINUX)
    QFile meminfo("/proc/meminfo");
    if (!meminfo.open(QIODevice::ReadOnly))
        return -1;
    while (!meminfo.atEnd()) {
        const QString line = QString::fromUtf8(meminfo.readLine());
        if (!line.startsWith(QLatin1String("MemTotal:")))
            continue;
        bool ok = false;
        const qlonglong kb = line.split(QLatin1Char(':')).value(1).simplified().split(QLatin1Char(' ')).value(0).toLongLong(&ok);
        return ok ? kb : -1;
    }
    return -1;
#else
    return -1;
#endif
}

void CommandExecutor::refreshStatCache()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_statCacheTime < 2000)
        return;
    m_statCacheTime = now;
    m_statCache.clear();
#ifdef Q_OS_WINDOWS
    return;
#else
    QStringList pids;
    for (const auto &proc : m_processes) {
        const int pid = static_cast<int>(proc.process->processId());
        if (pid > 0)
            pids << QString::number(pid);
    }
    for (const auto &ext : m_external) {
        if (ext.pid > 0)
            pids << QString::number(ext.pid);
    }
    if (pids.isEmpty())
        return;
    QProcess ps;
    ps.start("ps", QStringList{"-o", "pid=,rss=,pcpu="} << "-p" << pids.join(QLatin1Char(',')));
    if (!ps.waitForFinished(3000))
        return;
    const QStringList lines = QString::fromUtf8(ps.readAllStandardOutput()).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QStringList parts = line.simplified().split(QLatin1Char(' '));
        if (parts.size() == 3) {
            StatEntry entry;
            entry.rssKb = parts.at(1).toLongLong();
            entry.cpuPercent = parts.at(2).toDouble();
            m_statCache.insert(parts.at(0).toInt(), entry);
        }
    }
#endif
}

void CommandExecutor::detachAll()
{
    for (auto it = m_processes.begin(); it != m_processes.end(); ++it)
        it->process->setParent(nullptr);
    m_processes.clear();
    m_external.clear();
    m_killRequested.clear();
    emit runningCommandsChanged();
}

void CommandExecutor::flushReady(int id, bool isError)
{
    auto it = m_processes.find(id);
    if (it == m_processes.end())
        return;

    QByteArray *remainder = isError ? &it->errRemainder : &it->outRemainder;
    QByteArray chunk = isError ? it->process->readAllStandardError()
                               : it->process->readAllStandardOutput();
    if (chunk.isEmpty())
        return;

    *remainder += chunk;

    int nl = -1;
    while ((nl = remainder->indexOf('\n')) >= 0) {
        QByteArray rawLine = remainder->left(nl);
        remainder->remove(0, nl + 1);
        QString line = QString::fromUtf8(rawLine).remove(QLatin1Char('\r'));
        emit outputReceived(id, it->label, isError, line, it->commandId);
    }
}

void CommandExecutor::finishWithFailure(int id, const QString &error)
{
    auto it = m_processes.find(id);
    if (it == m_processes.end())
        return;
    const QString label = it->label;
    const QString commandId = it->commandId;
    cleanup(id);
    emit failed(id, label, error, commandId);
    emit runningCommandsChanged();
}

void CommandExecutor::cleanup(int id)
{
    auto it = m_processes.find(id);
    if (it == m_processes.end())
        return;
    it->process->deleteLater();
    m_processes.erase(it);
    m_killRequested.remove(id);
}
