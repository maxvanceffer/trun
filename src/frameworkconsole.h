#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <QProcess>
#include <QMap>

// Framework console layer (Symfony/Laravel).
// Data-driven registry: framework detection from composer.json + console
// binary gate, background discovery of commands via
// `php <binary> list --format=json`. Nothing blocks; completion arrives
// through the signals.
class FrameworkConsole : public QObject {
    Q_OBJECT

public:
    // Registry entry: framework name, require marker in composer.json,
    // console binary gate, discovery list command, extra default args
    // appended to the discovery invocation, and the directory holding
    // the console command definitions (relative to the project root).
    struct FrameworkDef {
        QString name;
        QString requireMarker;
        QString consoleBinary;
        QString listCommand;
        QStringList extraDefaultArgs;
        QString commandDir;
    };

    explicit FrameworkConsole(QObject *parent = nullptr);
    ~FrameworkConsole();

    // Detected framework name, empty string when not detected:
    // composer.json "require" must contain the marker AND the console
    // binary must exist in the project root.
    Q_INVOKABLE QString detectFramework(const QString &projectPath,
                                        const QJsonObject &composerDoc) const;
    // Structured per-command records parsed from a
    // `list --format=json` payload. Empty when the payload is not JSON.
    static QVariantList parseListing(const QString &framework,
                                     const QString &binary,
                                     const QByteArray &raw);
    // Runs `php <binary> list --format=json` in the background
    // (working dir = project root). Never blocks; completion arrives
    // via discoveryFinished/discoveryFailed. One in-flight discovery per
    // project; a call while one is running is ignored.
    Q_INVOKABLE void startDiscovery(const QString &projectPath);
    // True when a discovery for this project is currently running.
    Q_INVOKABLE bool discoveryInProgress(const QString &projectPath) const;
    // Console command definition directory for a framework, relative to
    // the project root (for cache invalidation); empty when unknown.
    Q_INVOKABLE QString commandDirFor(const QString &framework) const;
    // Installed version of the detected framework from composer.lock
    // (packages[].name/version); empty when absent.
    Q_INVOKABLE QString readFrameworkVersion(const QString &projectPath) const;
    // All known framework names from the registry.
    static QStringList frameworkNames();

signals:
    void discoveryFinished(const QString &projectPath, const QVariantList &commands);
    void discoveryFailed(const QString &projectPath, const QString &reason);

private:
    static const QList<FrameworkDef> &registry();
    FrameworkDef findDef(const QString &name) const;

    QMap<QString, QProcess *> m_processes; // in-flight discoveries, by project path
};
