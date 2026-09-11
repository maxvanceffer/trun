#include "projectlistmodel.h"
#include <QJsonArray>

void ProjectListModel::setProjects(const QList<QJsonObject> &projects) {
    if (projects == m_projects) return;
    
    beginResetModel();
    m_projects = projects;
    endResetModel();
    
    emit projectsChanged();
}

int ProjectListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return m_projects.size();
}

QVariant ProjectListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_projects.size()) return QVariant();
    
    const QJsonObject &proj = m_projects[index.row()];
    
    switch (role) {
        case IdRole: return proj.value("id").toString();
        case ProjectPathRole: return proj.value("project_path").toString();
        case NameRole: return proj.value("name").toString();
        case DescriptionRole: return proj.value("description").toString();
        case ManifestRole: return proj.value("manifest").toString();
        case CommandsRole: return proj.value("commands");
        default: return QVariant();
    }
}

QHash<int, QByteArray> ProjectListModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[ProjectPathRole] = "project_path";
    roles[NameRole] = "name";
    roles[DescriptionRole] = "description";
    roles[ManifestRole] = "manifest";
    roles[CommandsRole] = "commands";
    return roles;
}
