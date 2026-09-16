#pragma once

#include <QObject>

class Updater;

// Inserts "Check for Updates…" into the native macOS application menu,
// right above Quit. No-op on other platforms (implemented in macappmenu.mm).
// The item opens the in-app update dialog on openTarget and renames itself
// to "Update to vX…" while a newer release is available.
void installMacAppMenu(QObject *openTarget, Updater *updater);
