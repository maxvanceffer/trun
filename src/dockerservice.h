#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>

// Docker engine status (via colima or any Docker Desktop/OrbStack daemon)
// plus container/image lists and per-container log fetching.
// All blocking docker/colima calls run off the GUI thread;
// the UI binds to engine/containers/images and busy.
class DockerService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap engine READ engine NOTIFY engineChanged)
    Q_PROPERTY(QVariantList containers READ containers NOTIFY containersChanged)
    Q_PROPERTY(QVariantList images READ images NOTIFY imagesChanged)
    Q_PROPERTY(QVariantMap disk READ disk NOTIFY diskChanged)
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY statsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit DockerService(QObject *parent = nullptr);

    QVariantMap engine() const { return m_engine; }
    QVariantList containers() const { return m_containers; }
    QVariantList images() const { return m_images; }
    QVariantMap disk() const { return m_disk; }
    QVariantMap stats() const { return m_stats; }
    bool busy() const { return m_busy; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool startContainer(const QString &name);
    Q_INVOKABLE bool stopContainer(const QString &name);
    Q_INVOKABLE bool restartContainer(const QString &name);
    Q_INVOKABLE bool removeContainer(const QString &name);
    // Group action over compose-project containers (no compose CLI needed:
    // `docker start/stop/restart` accept several names at once).
    Q_INVOKABLE bool controlGroup(const QStringList &names, const QString &action);
    Q_INVOKABLE bool removeImage(const QString &id);
    Q_INVOKABLE bool prune();
    Q_INVOKABLE bool startEngine();
    Q_INVOKABLE bool stopEngine();
    // Tail log lines for one container; result arrives via logsReady.
    Q_INVOKABLE void fetchLogs(const QString &name, int tail = 300);
    // Live CPU/RAM for running containers; result arrives via statsChanged.
    // Never touches busy: safe to poll on a timer.
    Q_INVOKABLE void fetchStats();

    // Synchronous queries for headless consumers (MCP server). Blocking:
    // call off the GUI thread or from the headless --mcp mode.
    // ok (when given) reports transport success: false means the daemon
    // did not answer (timeout), not an empty result.
    static QVariantMap queryEngine();
    static QVariantList queryContainers(bool *ok = nullptr);
    static QVariantList queryImages(bool *ok = nullptr);
    static QVariantMap queryDisk(bool *ok = nullptr);
    static QVariantMap queryStats(bool *ok = nullptr);
    static QString queryLogs(const QString &name, int tail, bool *ok = nullptr);
    // Runs `docker <args>` synchronously; output holds stdout (plus stderr
    // on failure). Timeout in ms.
    static bool runDocker(const QStringList &args, QString *output = nullptr,
                          int timeoutMs = 30000);

signals:
    void engineChanged();
    void containersChanged();
    void imagesChanged();
    void diskChanged();
    void statsChanged();
    void busyChanged();
    void logsReady(const QString &name, const QString &logs);
    void logsError(const QString &message);
    void errorMessage(const QString &message);

private:
    // Quiet ops skip the global busy flag (no rescan spinner): the UI
    // tracks them per item via pendingRemove instead.
    bool control(const QStringList &args, bool quiet = false);
    // One full rescan off the GUI thread. Failed queries keep their
    // previous data (reported via error); only successes overwrite.
    void applyScan(const QVariantMap &engine, const QVariantList &containers,
                   bool containersOk, const QVariantList &images, bool imagesOk,
                   const QVariantMap &disk, bool diskOk, const QString &error);
    QVariantMap m_engine;
    QVariantList m_containers;
    QVariantList m_images;
    QVariantMap m_disk;
    QVariantMap m_stats;
    bool m_busy = false;
    std::atomic<bool> m_statsBusy{false};
    std::atomic<bool> m_logsBusy{false};
};
