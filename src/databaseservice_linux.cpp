#include "databaseservice_backend.h"

// Linux backend is not implemented yet: no services are reported and no
// control actions succeed. Fill in with detection via systemd / package
// managers when Linux support is added.

QVariantList detectDatabaseServices()
{
    return {};
}

bool controlDatabaseService(const QVariantMap &service, bool start)
{
    Q_UNUSED(service)
    Q_UNUSED(start)
    return false;
}

QVariantList serviceListeningPorts(const QVariantMap &service)
{
    Q_UNUSED(service)
    return {};
}

QVariantList serviceDatabases(const QVariantMap &service, QString &error, bool &authError)
{
    Q_UNUSED(service)
    error = QStringLiteral("not implemented on this platform");
    authError = false;
    return {};
}

QVariantMap connectionConfig(const QVariantMap &service)
{
    Q_UNUSED(service)
    return {};
}

bool saveConnectionConfig(const QVariantMap &service, const QVariantMap &config,
                          const QString &password)
{
    Q_UNUSED(service)
    Q_UNUSED(config)
    Q_UNUSED(password)
    return false;
}

bool forgetConnectionConfig(const QVariantMap &service)
{
    Q_UNUSED(service)
    return false;
}

bool testConnection(const QVariantMap &service, QString &detail, QString &error, bool &authError)
{
    Q_UNUSED(service)
    detail.clear();
    error = QStringLiteral("not implemented on this platform");
    authError = false;
    return false;
}

QVariantMap runDatabaseQuery(const QVariantMap &service, const QString &database,
                             const QString &sql)
{
    Q_UNUSED(service)
    Q_UNUSED(database)
    Q_UNUSED(sql)
    return QVariantMap{
        {QStringLiteral("rows"), QVariantList()},
        {QStringLiteral("columns"), QStringList()},
        {QStringLiteral("error"), QStringLiteral("not implemented on this platform")},
        {QStringLiteral("authError"), false},
        {QStringLiteral("elapsedMs"), qint64(0)},
    };
}
