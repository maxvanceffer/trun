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
