#pragma once

#include <QAbstractItemModel>
#include <QList>
#include <QSet>

// ============================================================
// QmlTreeItem — node in the sidebar tree (folder or manifests entry)
//
// The tree shows folders only: per-manifest project nodes are gone.
// A folder holding manifests directly navigates to its folder page;
// a hybrid folder (manifests plus subfolders) grows one ManifestsEntry
// child that navigates instead. Per-manifest data (names, commands)
// lives in ProjectService, keyed by "<path>/<manifest>".
// ============================================================
class QmlTreeItem {
    friend class QmlTreeModel;
public:
    enum Type { Folder, ManifestsEntry };

    explicit QmlTreeItem(Type type, QmlTreeItem* parent = nullptr);
    ~QmlTreeItem();

    QmlTreeItem* child(int row);
    int childCount() const;
    int row() const;
    QmlTreeItem* parent() const;

    void appendChild(QmlTreeItem* child);

    // === Folder & ManifestsEntry ===
    Type type() const { return m_type; }
    QString name() const { return m_name; }
    QString path() const { return m_path; }

    // === Folder: manifests found directly in it ===
    bool hasManifests() const { return !m_manifests.isEmpty(); }

    // === ManifestsEntry: the single manifest name, or empty for several ===
    QString manifest() const { return m_manifest; }

private:
    Type m_type;
    QString m_name;
    QString m_path;
    QSet<QString> m_manifests; // folders only
    QString m_manifest; // entries only
    QmlTreeItem* m_parent;
    QList<QmlTreeItem*> m_children;
};

// ============================================================
// QmlTreeModel — folder tree for the sidebar TreeView
// Exposes:
//   - item_type   — "folder" or "manifests"
//   - folderName  — display name (git repo name, else folder name)
//   - folderPath  — absolute folder path
//   - hasManifests — the folder holds manifests directly
//   - manifest    — single manifest name, or "" for several/none.
//                   Folders expose their own manifests (leaf icon);
//                   entries expose the folder's (entry icon).
// ============================================================
class QmlTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Roles {
        ItemTypeRole = Qt::UserRole + 1,
        FolderNameRole,
        FolderPathRole,
        HasManifestsRole,
        ManifestRole,
    };
    Q_ENUM(Roles)

    explicit QmlTreeModel(QObject *parent = nullptr);
    ~QmlTreeModel() override;

    void setRootPath(const QString &rootPath);
    // Multiple workspace roots: one top-level node per root.
    void setRootPaths(const QStringList &rootPaths);
    // Records one manifest in a folder; maintains the entry child.
    Q_INVOKABLE void addProjectManifest(const QString &projectPath,
                                        const QString &manifest);
    Q_INVOKABLE void clear();

    // Folder label: git repo name when the folder is a repo, else its name.
    static QString folderDisplayName(const QString &absoluteFolderPath);

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

    // Keeps the ManifestsEntry child in sync: present at row 0 exactly
    // when the folder holds manifests and has subfolder children.
    void syncManifestsEntry(QmlTreeItem *folder);
};
