#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QProcess>
#include <QMap>
#include <QSet>
#include <QDateTime>

class CommandExecutor : public QObject {
    Q_OBJECT

public:
    explicit CommandExecutor(QObject *parent = nullptr);

    // Starts a process, returns an internal id (0 on immediate failure).
    // Never blocks: completion arrives via finished()/failed().
    // commandId identifies the UI command (e.g. "npm:build") for tracking.
    Q_INVOKABLE int run(const QString &command, const QStringList &args,
                        const QString &workingDir, const QString &label,
                        const QString &commandId);
    // Same as run() but merges envPairs ("KEY=VALUE") over the system env.
    Q_INVOKABLE int runWithEnv(const QString &command, const QStringList &args,
                               const QString &workingDir, const QString &label,
                               const QString &commandId, const QStringList &envPairs);
    Q_INVOKABLE void stop(int id);
    Q_INVOKABLE bool isRunning(int id) const;
    Q_INVOKABLE int runningCount() const;
    Q_INVOKABLE void killAll();
    // Stop everything launched for one UI command (live runs and adopted ones).
    Q_INVOKABLE void stopCommand(const QString &commandId);
    // Command descriptor ids (e.g. "npm:build") with something running,
    // including processes adopted from a previous session.
    Q_PROPERTY(QStringList runningCommandIds READ runningCommandIds NOTIFY runningCommandsChanged)
    QStringList runningCommandIds() const;
    // Snapshot of live runs for persisting across restarts.
    Q_INVOKABLE QVariantList runningProcesses() const;
    // OS pid behind a UI command, or 0 when nothing runs for it.
    Q_INVOKABLE int pidForCommand(const QString &commandId) const;
    // Reveal a folder in the system file manager. False for empty/missing paths.
    Q_INVOKABLE bool revealFolder(const QString &path);
    // Local TCP port checks (POSIX connect to 127.0.0.1; stubs on Windows).
    Q_INVOKABLE bool portBusy(int port);
    // PID listening on the port via lsof, or 0 when unknown.
    Q_INVOKABLE int pidOnPort(int port);
    // SIGTERM a foreign pid. Never self/init. False on failure.
    Q_INVOKABLE bool killExternal(int pid);
    // Resident memory in KB for a UI command, or -1 when unknown.
    // Backed by a throttled `ps` cache shared across all queries.
    Q_INVOKABLE qlonglong memoryForCommand(const QString &commandId);
    // CPU usage in percent (htop-style, may exceed 100 on multicore), or -1.
    Q_INVOKABLE double cpuForCommand(const QString &commandId);
    // Elapsed seconds since the (re)start, or -1 when not running.
    Q_INVOKABLE qlonglong elapsedForCommand(const QString &commandId) const;
    // Total system memory in KB, or -1 when unknown.
    Q_INVOKABLE qlonglong totalMemoryKb() const;

    // Adopt an already-running OS process (e.g. left by a previous session).
    // Returns false when the pid is dead or runs something else.
    // startedAtIso (Qt::ISODate) restores the original launch time;
    // empty means "adopted just now".
    Q_INVOKABLE bool adoptExternal(int pid, const QString &command, const QStringList &args,
                                   const QString &workingDir, const QString &label,
                                   const QString &commandId,
                                   const QString &startedAtIso = QString());
    // Detach all processes so they survive app exit (used for "leave running").
    // The QProcess wrappers are intentionally leaked: destroying them is safe,
    // but leaking guarantees no teardown touches the live children.
    Q_INVOKABLE void detachAll();

signals:
    void started(int id, int pid, const QString &label, const QString &cmdline,
                 const QString &commandId);
    void outputReceived(int id, const QString &label, bool isError, const QString &line,
                        const QString &commandId);
    void finished(int id, const QString &label, int exitCode, const QString &commandId);
    void failed(int id, const QString &label, const QString &error, const QString &commandId);
    void runningCommandsChanged();

private:
    struct Proc {
        QProcess *process = nullptr;
        QByteArray outRemainder;
        QByteArray errRemainder;
        QString label;
        QString commandId;
        QDateTime startTime;
    };
    struct ExternalProc {
        int pid = 0;
        QString command;
        QStringList args;
        QString workingDir;
        QString label;
        QString commandId;
        QDateTime startTime;
    };
    struct StatEntry {
        qlonglong rssKb = -1;
        double cpuPercent = -1.0;
    };

    void refreshStatCache();
    void flushReady(int id, bool isError);
    void finishWithFailure(int id, const QString &error);
    void cleanup(int id);

    QMap<int, Proc> m_processes;
    QMap<int, ExternalProc> m_external;
    QSet<int> m_killRequested;
    int m_nextId = 1;
    QMap<int, StatEntry> m_statCache;
    qint64 m_statCacheTime = 0;
};
