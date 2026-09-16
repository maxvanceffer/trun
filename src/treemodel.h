#pragma once

#include <QAbstractItemModel>
#include <QList>
#include <QJsonObject>

// ============================================================
// QmlTreeItem — node in the tree (folder or project)
// ============================================================
class QmlTreeItem {
    friend class QmlTreeModel;
public:
    enum Type { Folder, Project };

    explicit QmlTreeItem(Type type, QmlTreeItem* parent = nullptr);
    ~QmlTreeItem();

    QmlTreeItem* child(int row);
    int childCount() const;
    int row() const;
    QmlTreeItem* parent() const;

    void appendChild(QmlTreeItem* child);

    // === Folder ===
    Type type() const { return m_type; }
    QString name() const { return m_name; }
    QString path() const { return m_path; }

    // === Project ===
    QString manifest() const { return m_manifest; }
    QString description() const { return m_description; }
    QVariant commands() const { return m_commands; }

private:
    Type m_type;
    QString m_name;
    QString m_path;
    QString m_manifest;
    QString m_description;
    QVariant m_commands;
    QmlTreeItem* m_parent;
    QList<QmlTreeItem*> m_children;
};

// ============================================================
// QmlTreeModel — hierarchical model for TreeView
// Exposes:
//   - treeModel (registered QML type, same API as old ProjectListModel)
//   - folderName  — for folders (name) / projects (project folder name)
//   - folderPath  — for folders (path) / projects (project folder path)
//   - name        — project name (only for projects)
//   - project_path — project path (only for projects)
//   - manifest    — manifest name (only for projects)
//   - description — project description (only for projects)
//   - commands    — commands array (only for projects)
// ============================================================
class QmlTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Roles {
        ItemTypeRole = Qt::UserRole + 1,
        FolderNameRole,
        FolderPathRole,
        NameRole,
        ProjectIdRole,
        DescriptionRole,
        ManifestRole,
        CommandsRole
    };
    Q_ENUM(Roles)

    explicit QmlTreeModel(QObject *parent = nullptr);
    ~QmlTreeModel() override;

    void setRootPath(const QString &rootPath);
    // Multiple workspace roots: one top-level node per root.
    void setRootPaths(const QStringList &rootPaths);
    Q_INVOKABLE void addProject(
        const QString &projectPath,
        const QString &name,
        const QString &description,
        const QString &manifest,
        const QVariant &commands
    );
    Q_INVOKABLE void clear();

    // QAbstractItemModel
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QModelIndex indexForItem(QmlTreeItem *item) const;
    QmlTreeItem* getItem(const QModelIndex &index) const;
    QmlTreeItem* rootItem;
    QString m_rootPath; // primary root (first of m_rootPaths), kept for compat
    QStringList m_rootPaths;

    // Visible top-level nodes, one per workspace root.
    QList<QmlTreeItem*> m_rootFolders;

    QmlTreeItem* ensureFolder(const QString &absoluteFolderPath);

    // Folder label: git repo name when the folder is a repo, else its name.
    static QString folderDisplayName(const QString &absoluteFolderPath);
};
