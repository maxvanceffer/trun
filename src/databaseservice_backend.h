#pragma once

#include <QVariantList>
#include <QVariantMap>

// Platform-specific database service discovery and control.
// Implemented in databaseservice_macos.cpp / databaseservice_linux.cpp.
//
// A service map looks like:
//   id        unique key, e.g. "brew:postgresql@16"
//   engine    "postgresql" | "mysql" | "mongodb"
//   name      display name, e.g. "PostgreSQL"
//   version   e.g. "16" (may be empty)
//   port      default/known TCP port (0 when unknown)
//   running   bool
//   source    "homebrew" | "postgresapp" | "system"
//   formula   Homebrew formula when source == "homebrew"
//   controllable  bool (can be started/stopped by the app)
QVariantList detectDatabaseServices();

// Start/stop a service previously returned by detectDatabaseServices().
bool controlDatabaseService(const QVariantMap &service, bool start);

// TCP listeners owned by a service's engine process; each entry:
//   port  int      addr  string ("127.0.0.1", "[::1]", "*", ...)
//   pid   string    proc  string (process comm name)
// Empty when the service is not running or lsof found nothing.
QVariantList serviceListeningPorts(const QVariantMap &service);

// Per-database detail for a running service with a reachable connection:
//   name  string    tables  string ("142", "—" when not applicable)
//   size  string ("1.4 GB")    conns  string ("6 / 100", "—" when unknown)
// `error` is set (non-fatal to the caller) when the listing is partial or
// empty because of an auth failure; check `authError` to tell that apart
// from "engine has no such concept" (e.g. Redis) which returns {} cleanly.
QVariantList serviceDatabases(const QVariantMap &service, QString &error, bool &authError);

// Stored connection config for a service (host/port/user/database/sslmode/
// authMethod), without the password. Empty map when nothing is saved, in
// which case sensible defaults (127.0.0.1, the service's default port,
// system user peer auth) apply.
QVariantMap connectionConfig(const QVariantMap &service);

// Save connection config. `password` overwrites the stored secret; pass a
// null QString (not just empty) to keep whatever is already in the
// Keychain. Linux: config is kept in QSettings, password storage is a
// no-op (returns false) until a Secret Service backend is wired up.
bool saveConnectionConfig(const QVariantMap &service, const QVariantMap &config,
                          const QString &password);

// Remove any stored config + Keychain secret for the service.
bool forgetConnectionConfig(const QVariantMap &service);

// Try connecting with the saved (or default) credentials. `detail` gets a
// short human status ("server 16.15 · 18 ms") on success; `error` and
// `authError` are set on failure.
bool testConnection(const QVariantMap &service, QString &detail, QString &error,
                    bool &authError);

// Run a SQL/eval statement against `database` (empty = engine default) using
// the service's stored/default credentials. Result map:
//   rows        QVariantList of QVariantList<QString>
//   columns     QStringList
//   error       QString (empty on success)
//   authError   bool
//   elapsedMs   qint64
QVariantMap runDatabaseQuery(const QVariantMap &service, const QString &database,
                             const QString &sql);
