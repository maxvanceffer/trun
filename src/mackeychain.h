#pragma once

#include <QString>

// Generic-password Keychain access via the Security framework directly
// (SecItemAdd/SecItemCopyMatching/SecItemDelete), not the `security` CLI.
// The CLI's write path (`add-generic-password -w <password>`) has no way to
// take the secret off argv — `-w` with no value falls back to a /dev/tty
// prompt, which a QProcess without a controlling terminal can't answer, so
// the password would have to sit in argv (visible to any local user via
// `ps`). The native API takes it as in-memory bytes instead.
bool macKeychainSetPassword(const QString &service, const QString &account, const QString &password);
bool macKeychainFindPassword(const QString &service, const QString &account, QString &password);
bool macKeychainDeletePassword(const QString &service, const QString &account);
