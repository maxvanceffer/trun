#include "databaseservice.h"

#include "databaseservice_backend.h"

#include <QMetaObject>
#include <QThread>
#include <utility>
#include <QGuiApplication>
#include <QClipboard>
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QUrl>

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
    m_scanning = true;
    emit busyChanged();
    emit scanningChanged();

    QThread *thread = QThread::create([this]() {
        const QVariantList result = detectDatabaseServices();
        QMetaObject::invokeMethod(
            this,
            [this, result]() {
                m_services = result;
                m_busy = false;
                m_scanning = false;
                emit busyChanged();
                emit scanningChanged();
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
        m_controllingId = id;
        emit busyChanged();
        emit controllingIdChanged();

        QThread *thread = QThread::create([this, svc, start]() {
            controlDatabaseService(svc, start);
            const QVariantList result = detectDatabaseServices();
            QMetaObject::invokeMethod(
                this,
                [this, result]() {
                    m_services = result;
                    m_busy = false;
                    m_controllingId.clear();
                    emit busyChanged();
                    emit controllingIdChanged();
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

QVariantMap DatabaseService::serviceById(const QString &id) const
{
    for (const QVariant &v : std::as_const(m_services)) {
        const QVariantMap s = v.toMap();
        if (s.value(QStringLiteral("id")).toString() == id)
            return s;
    }
    return {};
}

bool DatabaseService::runQuery(const QString &id, const QString &database, const QString &sql)
{
    const QVariantMap svc = serviceById(id);
    if (svc.isEmpty() || m_busy)
        return false;

    m_busy = true;
    emit busyChanged();

    QThread *thread = QThread::create([this, svc, database, sql]() {
        const QVariantMap result = runDatabaseQuery(svc, database, sql);
        QMetaObject::invokeMethod(
            this,
            [this, result]() {
                m_busy = false;
                emit busyChanged();
                emit queryResult(result.value(QStringLiteral("rows")).toList(),
                                 result.value(QStringLiteral("columns")).toStringList(),
                                 result.value(QStringLiteral("error")).toString(),
                                 result.value(QStringLiteral("authError")).toBool(),
                                 result.value(QStringLiteral("elapsedMs")).toLongLong());
            },
            Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
    return true;
}

bool DatabaseService::fetchDetail(const QString &id)
{
    const QVariantMap svc = serviceById(id);
    if (svc.isEmpty())
        return false;

    QThread *thread = QThread::create([this, id, svc]() {
        const QVariantList ports = serviceListeningPorts(svc);
        QString dbError;
        bool dbAuthError = false;
        const QVariantList databases = serviceDatabases(svc, dbError, dbAuthError);
        const QVariantMap connection = connectionConfig(svc);
        const QVariantMap detail{
            {QStringLiteral("ports"), ports},
            {QStringLiteral("databases"), databases},
            {QStringLiteral("databasesError"), dbError},
            {QStringLiteral("databasesAuthError"), dbAuthError},
            {QStringLiteral("connection"), connection},
        };
        QMetaObject::invokeMethod(
            this, [this, id, detail]() { emit detailReady(id, detail); }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
    return true;
}

bool DatabaseService::saveConnection(const QString &id, const QVariantMap &config,
                                     const QString &password)
{
    const QVariantMap svc = serviceById(id);
    if (svc.isEmpty())
        return false;

    QThread *thread = QThread::create([this, id, svc, config, password]() {
        saveConnectionConfig(svc, config, password);
        QMetaObject::invokeMethod(
            this, [this, id]() { emit connectionSaved(id); }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
    return true;
}

bool DatabaseService::forgetConnection(const QString &id)
{
    const QVariantMap svc = serviceById(id);
    if (svc.isEmpty())
        return false;

    QThread *thread = QThread::create([this, id, svc]() {
        forgetConnectionConfig(svc);
        QMetaObject::invokeMethod(
            this, [this, id]() { emit connectionForgotten(id); }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
    return true;
}

bool DatabaseService::testServiceConnection(const QString &id)
{
    const QVariantMap svc = serviceById(id);
    if (svc.isEmpty())
        return false;

    QThread *thread = QThread::create([this, id, svc]() {
        QString detail, error;
        bool authError = false;
        const bool ok = testConnection(svc, detail, error, authError);
        QMetaObject::invokeMethod(
            this,
            [this, id, ok, detail, error, authError]() {
                emit connectionTestResult(id, ok, detail, error, authError);
            },
            Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
    return true;
}

bool DatabaseService::copyToClipboard(const QString &text)
{
    if (QGuiApplication::clipboard())
        QGuiApplication::clipboard()->setText(text);
    return true;
}

bool DatabaseService::exportToFile(const QString &path, const QVariantList &rows)
{
    // FileDialog passes a file:// URL; QFile needs a local path.
    QString local = path;
    if (local.startsWith(QLatin1String("file:")))
        local = QUrl(local).toLocalFile();
    QFile f(local);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    QTextStream out(&f);
    for (const QVariant &r : std::as_const(rows)) {
        QVariantList cells = r.toList();
        QStringList parts;
        for (const QVariant &c : std::as_const(cells))
            parts << c.toString();
        out << parts.join(QLatin1Char(',')) << QChar(QLatin1Char('\n'));
    }
    out.flush();
    return true;
}
