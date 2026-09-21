#pragma once

#include <QObject>
#include <QSettings>
#include <QVariant>
#include <QJsonArray>
#include <QString>

class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool showGitBranch READ showGitBranch WRITE setShowGitBranch NOTIFY showGitBranchChanged)
    Q_PROPERTY(bool gitBranchTopOnly READ gitBranchTopOnly WRITE setGitBranchTopOnly NOTIFY gitBranchTopOnlyChanged)

public:
    explicit Settings(QObject *parent = nullptr);
    explicit Settings(const QString &filePath, QObject *parent = nullptr);

    Q_INVOKABLE QVariant get(const QString &key, const QVariant &defaultValue = QVariant()) const;
    Q_INVOKABLE void set(const QString &key, const QVariant &value);
    Q_INVOKABLE bool has(const QString &key) const;
    Q_INVOKABLE void remove(const QString &key);

    Q_INVOKABLE QString rootFolder() const;
    Q_INVOKABLE void setRootFolder(const QString &folder);
    // All workspace roots (multi-root). The legacy single rootFolder()
    // stays as the primary root for backward compatibility.
    QStringList rootFolders() const;
    void setRootFolders(const QStringList &folders);
    QJsonArray projects() const;
    void setProjects(const QJsonArray &projects);
    int cacheVersion() const;
    void setCacheVersion(int version);
    Q_INVOKABLE void clearWorkspace();
    Q_INVOKABLE bool hasWorkspace() const;
    bool showGitBranch() const;
    void setShowGitBranch(bool show);
    bool gitBranchTopOnly() const;
    void setGitBranchTopOnly(bool topOnly);

signals:
    void workspaceChanged();
    void showGitBranchChanged();
    void gitBranchTopOnlyChanged();

private:
    QSettings m_settings;
};
