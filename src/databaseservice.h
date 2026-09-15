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

public:
    explicit DatabaseService(QObject *parent = nullptr);

    QVariantList services() const { return m_services; }
    bool busy() const { return m_busy; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool startService(const QString &id);
    Q_INVOKABLE bool stopService(const QString &id);

signals:
    void servicesChanged();
    void busyChanged();

private:
    bool control(const QString &id, bool start);
    QVariantList m_services;
    bool m_busy = false;
};
