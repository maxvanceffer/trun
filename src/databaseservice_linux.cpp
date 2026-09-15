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
