#include "settings.h"
#include <QJsonDocument>

Settings::Settings(QObject *parent)
    : QObject(parent)
{
}

Settings::Settings(const QString &filePath, QObject *parent)
    : QObject(parent)
    , m_settings(filePath, QSettings::IniFormat)
{
}

QVariant Settings::get(const QString &key, const QVariant &defaultValue) const
{
    return m_settings.value(key, defaultValue);
}

void Settings::set(const QString &key, const QVariant &value)
{
    m_settings.setValue(key, value);
    m_settings.sync();
}

bool Settings::has(const QString &key) const
{
    return m_settings.contains(key);
}

void Settings::remove(const QString &key)
{
    m_settings.remove(key);
    m_settings.sync();
}

QString Settings::rootFolder() const
{
    return m_settings.value(QStringLiteral("rootFolder")).toString();
}

void Settings::setRootFolder(const QString &folder)
{
    m_settings.setValue(QStringLiteral("rootFolder"), folder);
    m_settings.sync();
    emit workspaceChanged();
}

QStringList Settings::rootFolders() const
{
    return m_settings.value(QStringLiteral("rootFolders")).toStringList();
}

void Settings::setRootFolders(const QStringList &folders)
{
    m_settings.setValue(QStringLiteral("rootFolders"), folders);
    m_settings.sync();
    emit workspaceChanged();
}

QJsonArray Settings::projects() const
{
    const QByteArray raw = m_settings.value(QStringLiteral("projects")).toByteArray();
    if (raw.isEmpty())
        return {};
    return QJsonDocument::fromJson(raw).array();
}

void Settings::setProjects(const QJsonArray &projects)
{
    m_settings.setValue(QStringLiteral("projects"),
                        QJsonDocument(projects).toJson(QJsonDocument::Compact));
    m_settings.sync();
    emit workspaceChanged();
}

int Settings::cacheVersion() const
{
    return m_settings.value(QStringLiteral("cacheVersion"), 0).toInt();
}

void Settings::setCacheVersion(int version)
{
    m_settings.setValue(QStringLiteral("cacheVersion"), version);
    m_settings.sync();
}

void Settings::clearWorkspace()
{
    m_settings.remove(QStringLiteral("rootFolder"));
    m_settings.remove(QStringLiteral("rootFolders"));
    m_settings.remove(QStringLiteral("projects"));
    m_settings.remove(QStringLiteral("configuredFolder"));
    m_settings.sync();
    emit workspaceChanged();
}

bool Settings::hasWorkspace() const
{
    return !rootFolder().isEmpty() && !projects().isEmpty();
}
