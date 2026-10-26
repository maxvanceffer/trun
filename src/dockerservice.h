#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>

// Docker engine status (via colima or any Docker Desktop/OrbStack daemon)
// plus container/image lists and per-container log fetching.
// All blocking docker/colima calls run off the GUI thread;
// the UI binds to engine/containers/images and busy. `scanning` marks a full
// rescan only (Rescan spinner); `controllingId` names the single action in
// flight so its own control shows the loader.
class DockerService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap engine READ engine NOTIFY engineChanged)
    Q_PROPERTY(QVariantList containers READ containers NOTIFY containersChanged)
    Q_PROPERTY(QVariantList images READ images NOTIFY imagesChanged)
    Q_PROPERTY(QVariantMap disk READ disk NOTIFY diskChanged)
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY statsChanged)
    // Something is in flight — refresh() or one action. Gates every control
    // that would start a second operation.
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // Full rescan only (refresh()): the page's Rescan spinner follows this and
    // nothing else. Actions never raise it.
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    // What is in flight ("engine", "prune", a container or image id,
    // "group:<project>:<action>"), so exactly one control shows a per-action
    // loader.
    Q_PROPERTY(QString controllingId READ controllingId NOTIFY controllingIdChanged)

public:
    explicit DockerService(QObject *parent = nullptr);

    QVariantMap engine() const { return m_engine; }
    QVariantList containers() const { return m_containers; }
    QVariantList images() const { return m_images; }
    QVariantMap disk() const { return m_disk; }
    QVariantMap stats() const { return m_stats; }
    bool busy() const { return m_busy; }
    bool scanning() const { return m_scanning; }
    QString controllingId() const { return m_controllingId; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool startContainer(const QString &name);
    Q_INVOKABLE bool stopContainer(const QString &name);
    Q_INVOKABLE bool restartContainer(const QString &name);
    Q_INVOKABLE bool removeContainer(const QString &name);
    // Group action over compose-project containers (no compose CLI needed:
    // `docker start/stop/restart` accept several names at once). project names
    // the group so only its row controls show the loader ("group:<project>").
    Q_INVOKABLE bool controlGroup(const QStringList &names, const QString &action,
                                  const QString &project);
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
    void scanningChanged();
    void controllingIdChanged();
    void logsReady(const QString &name, const QString &logs);
    void logsError(const QString &message);
    void errorMessage(const QString &message);

private:
    // Claim the service for one off-thread operation, publishing `target` as
    // controllingId. False when another operation already holds it.
    bool beginOp(const QString &target);
    // Release the claim and clear controllingId.
    void endOp();
    // One action off the GUI thread, followed by a full re-read of engine,
    // containers, images and disk. `target` names the control that shows the
    // per-action loader. Never raises `scanning`: that belongs to refresh().
    bool control(const QStringList &args, const QString &target);
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
    bool m_scanning = false;
    QString m_controllingId;
    std::atomic<bool> m_statsBusy{false};
    std::atomic<bool> m_logsBusy{false};
};
