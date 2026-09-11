#pragma once

#include <QAbstractListModel>
#include <QJsonObject>
#include <QList>

class ProjectListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        ProjectPathRole,
        NameRole,
        DescriptionRole,
        ManifestRole,
        CommandsRole
    };

    explicit ProjectListModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    Q_PROPERTY(QList<QJsonObject> projects READ projects WRITE setProjects NOTIFY projectsChanged)

    QList<QJsonObject> projects() const { return m_projects; }
    void setProjects(const QList<QJsonObject> &projects);

    // QAbstractListModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void projectsChanged();

private:
    QList<QJsonObject> m_projects;
};
