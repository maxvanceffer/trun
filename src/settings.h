#pragma once

#include <QObject>
#include <QSettings>
#include <QVariant>
#include <QJsonArray>
#include <QString>

class Settings : public QObject {
    Q_OBJECT

public:
    explicit Settings(QObject *parent = nullptr);
    explicit Settings(const QString &filePath, QObject *parent = nullptr);

    Q_INVOKABLE QVariant get(const QString &key, const QVariant &defaultValue = QVariant()) const;
    Q_INVOKABLE void set(const QString &key, const QVariant &value);
    Q_INVOKABLE bool has(const QString &key) const;
    Q_INVOKABLE void remove(const QString &key);

    Q_INVOKABLE QString rootFolder() const;
    Q_INVOKABLE void setRootFolder(const QString &folder);
    QJsonArray projects() const;
    void setProjects(const QJsonArray &projects);
    int cacheVersion() const;
    void setCacheVersion(int version);
    Q_INVOKABLE void clearWorkspace();
    Q_INVOKABLE bool hasWorkspace() const;

signals:
    void workspaceChanged();

private:
    QSettings m_settings;
};
