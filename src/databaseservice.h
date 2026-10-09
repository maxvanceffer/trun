#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

// Discovers locally installed database engines (PostgreSQL, MySQL, MongoDB)
// and lets the UI start/stop the ones the app can control.
class DatabaseService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList services READ services NOTIFY servicesChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // True only during a full refresh() (initial scan or manual Rescan) — gates
    // the list view's full-page "Scanning" splash. Distinct from `busy`, which
    // also covers a single start/stop and would otherwise blank the whole list
    // for that too.
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    // Id of the service currently being started/stopped (empty when none), so
    // just that row can show a busy indicator instead of the whole list.
    Q_PROPERTY(QString controllingId READ controllingId NOTIFY controllingIdChanged)

public:
    explicit DatabaseService(QObject *parent = nullptr);

    QVariantList services() const { return m_services; }
    bool busy() const { return m_busy; }
    bool scanning() const { return m_scanning; }
    QString controllingId() const { return m_controllingId; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool startService(const QString &id);
    Q_INVOKABLE bool stopService(const QString &id);
    Q_INVOKABLE bool runQuery(const QString &id, const QString &database, const QString &sql);
    Q_INVOKABLE bool copyToClipboard(const QString &text);
    Q_INVOKABLE bool exportToFile(const QString &path, const QVariantList &rows);

    // Detail page: listening ports + per-database detail + saved connection
    // config, fetched together in one background pass.
    Q_INVOKABLE bool fetchDetail(const QString &id);
    // Save (password null = keep existing secret), forget, or test the
    // service's connection config. All run off the GUI thread.
    Q_INVOKABLE bool saveConnection(const QString &id, const QVariantMap &config,
                                    const QString &password);
    Q_INVOKABLE bool forgetConnection(const QString &id);
    Q_INVOKABLE bool testServiceConnection(const QString &id);

signals:
    void servicesChanged();
    void busyChanged();
    void scanningChanged();
    void controllingIdChanged();
    void queryResult(const QVariantList &rows, const QStringList &columns,
                     const QString &error, bool authError, qint64 elapsedMs);
    void detailReady(const QString &id, const QVariantMap &detail);
    void connectionTestResult(const QString &id, bool success, const QString &detail,
                              const QString &error, bool authError);
    void connectionSaved(const QString &id);
    void connectionForgotten(const QString &id);

private:
    bool control(const QString &id, bool start);
    QVariantMap serviceById(const QString &id) const;
    QVariantList m_services;
    bool m_busy = false;
    bool m_scanning = false;
    QString m_controllingId;
};
