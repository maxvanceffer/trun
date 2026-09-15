#include "databaseservice.h"

#include "databaseservice_backend.h"

#include <QMetaObject>
#include <QThread>
#include <utility>

DatabaseService::DatabaseService(QObject *parent) : QObject(parent)
{
    // Scan once at startup (off the GUI thread) so the page has data cached.
    refresh();
}

void DatabaseService::refresh()
{
    if (m_busy)
        return;
    m_busy = true;
    emit busyChanged();

    QThread *thread = QThread::create([this]() {
        const QVariantList result = detectDatabaseServices();
        QMetaObject::invokeMethod(
            this,
            [this, result]() {
                m_services = result;
                m_busy = false;
                emit busyChanged();
                emit servicesChanged();
            },
            Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

bool DatabaseService::control(const QString &id, bool start)
{
    for (const QVariant &v : std::as_const(m_services)) {
        const QVariantMap svc = v.toMap();
        if (svc.value(QStringLiteral("id")).toString() != id)
            continue;
        if (m_busy)
            return false;

        m_busy = true;
        emit busyChanged();

        QThread *thread = QThread::create([this, svc, start]() {
            controlDatabaseService(svc, start);
            const QVariantList result = detectDatabaseServices();
            QMetaObject::invokeMethod(
                this,
                [this, result]() {
                    m_services = result;
                    m_busy = false;
                    emit busyChanged();
                    emit servicesChanged();
                },
                Qt::QueuedConnection);
        });
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        thread->start();
        return true;
    }
    return false;
}

bool DatabaseService::startService(const QString &id)
{
    return control(id, true);
}

bool DatabaseService::stopService(const QString &id)
{
    return control(id, false);
}
