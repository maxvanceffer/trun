#include "treemodel.h"
#include <algorithm>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>

namespace {

QString branchForFolder(const QString &path, bool searchParents)
{
    QDir folder(path);
    if (!folder.exists())
        return {};
    do {
        const QString marker = folder.filePath(QStringLiteral(".git"));
        const QFileInfo info(marker);
        if (!info.exists())
            continue;
        QString gitDir = marker;
        if (info.isFile()) {
            QFile file(marker);
            if (!file.open(QIODevice::ReadOnly))
                return {};
            const QByteArray line = file.readLine(8192).trimmed();
            if (!line.startsWith("gitdir: "))
                return {};
            gitDir = folder.absoluteFilePath(QString::fromUtf8(line.mid(8)).trimmed());
        } else if (!info.isDir()) {
            return {};
        }
        QFile head(QDir(gitDir).filePath(QStringLiteral("HEAD")));
        if (!head.open(QIODevice::ReadOnly))
            return {};
        const QString value = QString::fromUtf8(head.readLine(8192)).trimmed();
        const QString prefix = QStringLiteral("ref: refs/heads/");
        if (value.startsWith(prefix))
            return value.mid(prefix.size());
        static const QRegularExpression hash(QStringLiteral("^(?:[0-9a-fA-F]{40}|[0-9a-fA-F]{64})$"));
        return hash.match(value).hasMatch() ? value.left(7) : QString();
    } while (searchParents && folder.cdUp());
    return {};
}

// Repo name from <dir>/.git/config's origin URL, empty when unavailable.
QString gitRepoName(const QString &dir)
{
    QFile config(dir + QStringLiteral("/.git/config"));
    if (!config.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream in(&config);
    bool inOrigin = false;
    QString url;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inOrigin = line.contains(QStringLiteral("remote \"origin\""));
            continue;
        }
        if (inOrigin && line.startsWith(QLatin1String("url"))) {
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq >= 0) {
                url = line.mid(eq + 1).trimmed();
                break;
            }
        }
    }
    if (url.isEmpty())
        return {};

    QString name = url;
    while (name.endsWith(QLatin1Char('/')))
        name.chop(1);
    if (name.endsWith(QLatin1String(".git")))
        name.chop(4);
    const int sep = std::max(name.lastIndexOf(QLatin1Char('/')),
                             name.lastIndexOf(QLatin1Char(':')));
    if (sep >= 0)
        name = name.mid(sep + 1);
    return name;
}

} // namespace


// ============================================================
// QmlTreeItem
// ============================================================

QmlTreeItem::QmlTreeItem(QmlTreeItem::Type type, QmlTreeItem *parent)
    : m_type(type), m_parent(parent),
      m_name(), m_path(), m_manifests(), m_manifest()
{
}

QmlTreeItem::~QmlTreeItem()
{
    qDeleteAll(m_children);
}

QmlTreeItem *QmlTreeItem::child(int row)
{
    if (row < 0 || row >= m_children.size())
        return nullptr;
    return m_children[row];
}

int QmlTreeItem::childCount() const
{
    return m_children.size();
}

int QmlTreeItem::row() const
{
    if (m_parent)
        return m_parent->m_children.indexOf(const_cast<QmlTreeItem*>(this));
    return 0;
}

QmlTreeItem *QmlTreeItem::parent() const
{
    return m_parent;
}

void QmlTreeItem::appendChild(QmlTreeItem *child)
{
    m_children.append(child);
    child->m_parent = this;
}

// ============================================================
// QmlTreeModel
// ============================================================

QmlTreeModel::QmlTreeModel(QObject *parent)
    : QAbstractItemModel(parent), rootItem(new QmlTreeItem(QmlTreeItem::Folder))
{
    m_gitRefreshTimer.setInterval(2000);
    connect(&m_gitRefreshTimer, &QTimer::timeout, this, &QmlTreeModel::refreshGitBranches);
}

void QmlTreeModel::refreshGitBranches()
{
    bool changed = false;
    QList<QmlTreeItem *> pending = m_rootFolders;
    while (!pending.isEmpty()) {
        QmlTreeItem *item = pending.takeLast();
        if (item->type() != QmlTreeItem::Folder)
            continue;
        pending.append(item->m_children);
        const QString branch = branchForFolder(item->path(), item->parent() == rootItem);
        if (branch == item->m_gitBranch)
            continue;
        item->m_gitBranch = branch;
        changed = true;
        const QModelIndex idx = indexForItem(item);
        emit dataChanged(idx, idx, {GitBranchRole});
    }
    if (changed) {
        ++m_gitBranchesVersion;
        emit gitBranchesChanged();
    }
}

QString QmlTreeModel::gitBranchForPath(const QString &path) const
{
    QList<QmlTreeItem *> pending = m_rootFolders;
    while (!pending.isEmpty()) {
        QmlTreeItem *item = pending.takeLast();
        if (item->type() != QmlTreeItem::Folder)
            continue;
        if (item->path() == path)
            return item->m_gitBranch;
        pending.append(item->m_children);
    }
    return {};
}

QmlTreeModel::~QmlTreeModel()
{
    delete rootItem;
}

QmlTreeItem *QmlTreeModel::getItem(const QModelIndex &idx) const
{
    if (idx.isValid())
        return static_cast<QmlTreeItem*>(idx.internalPointer());
    return rootItem;
}

void QmlTreeModel::setRootPath(const QString &rootPath)
{
    setRootPaths(QStringList{QDir::cleanPath(rootPath)});
}

void QmlTreeModel::setRootPaths(const QStringList &rootPaths)
{
    m_rootPaths.clear();
    for (const QString &p : rootPaths) {
        const QString clean = QDir::cleanPath(p);
        if (!clean.isEmpty() && clean != QLatin1String(".")
            && !m_rootPaths.contains(clean))
            m_rootPaths.append(clean);
    }
    m_rootPath = m_rootPaths.value(0);

    beginResetModel();
    delete rootItem;
    rootItem = new QmlTreeItem(QmlTreeItem::Folder);
    m_rootFolders.clear();
    endResetModel();

    for (const QString &root : m_rootPaths) {
        const int row = rootItem->childCount();
        beginInsertRows(QModelIndex(), row, row);
        QmlTreeItem *node = new QmlTreeItem(QmlTreeItem::Folder, rootItem);
        node->m_name = folderDisplayName(root);
        node->m_path = root;
        node->m_gitBranch = branchForFolder(root, true);
        rootItem->appendChild(node);
        m_rootFolders.append(node);
        endInsertRows();
    }
    if (m_rootFolders.isEmpty())
        m_gitRefreshTimer.stop();
    else
        m_gitRefreshTimer.start();
}

QString QmlTreeModel::folderDisplayName(const QString &absoluteFolderPath)
{
    const QString base = QFileInfo(absoluteFolderPath).fileName();
    if (QFileInfo::exists(absoluteFolderPath + QStringLiteral("/.git"))) {
        const QString repo = gitRepoName(absoluteFolderPath);
        if (!repo.isEmpty())
            return repo;
    }
    return base;
}

QModelIndex QmlTreeModel::indexForItem(QmlTreeItem *item) const
{
    if (!item || item == rootItem)
        return {};
    return createIndex(item->row(), 0, item);
}

QmlTreeItem *QmlTreeModel::ensureFolder(const QString &absoluteFolderPath)
{
    const QString abs = QDir::cleanPath(absoluteFolderPath);

    // Route to the deepest workspace root containing the path.
    QmlTreeItem *baseParent = nullptr;
    QString root;
    int bestLength = -1;
    for (QmlTreeItem *node : m_rootFolders) {
        const QString rp = QDir::cleanPath(node->path());
        if ((abs == rp || abs.startsWith(rp + QLatin1Char('/')))
            && rp.size() > bestLength) {
            bestLength = rp.size();
            baseParent = node;
            root = rp;
        }
    }
    if (!baseParent) {
        // Outside every known root (e.g. a custom-command folder):
        // hang it directly off the invisible root, like the legacy
        // no-root behavior.
        baseParent = rootItem;
        root = abs;
    }

    // A project that *is* the root lives directly under the root node.
    if (abs == root)
        return baseParent;

    QString rel = QDir(root).relativeFilePath(abs);
    if (rel == QLatin1String(".") || rel.isEmpty())
        return baseParent;

    const QStringList parts = rel.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QmlTreeItem *parent = baseParent;

    for (int i = 0; i < parts.size(); ++i) {
        const QString wantAbs = QDir::cleanPath(root + QLatin1Char('/') + parts.mid(0, i + 1).join(QLatin1Char('/')));

        QmlTreeItem *child = nullptr;
        for (QmlTreeItem *c : parent->m_children) {
            if (c->type() == QmlTreeItem::Folder && QDir::cleanPath(c->path()) == wantAbs) {
                child = c;
                break;
            }
        }
        if (!child) {
            const QModelIndex parentIdx = indexForItem(parent);
            const int row = parent->childCount();
            beginInsertRows(parentIdx, row, row);
            child = new QmlTreeItem(QmlTreeItem::Folder, parent);
            child->m_name = folderDisplayName(wantAbs);
            child->m_path = wantAbs;
            child->m_gitBranch = branchForFolder(wantAbs, false);
            parent->appendChild(child);
            endInsertRows();
            // A leaf folder with manifests just became hybrid:
            // it grows the manifests entry at row 0.
            syncManifestsEntry(parent);
        }
        parent = child;
    }
    return parent;
}

void QmlTreeModel::clear()
{
    m_gitRefreshTimer.stop();
    beginResetModel();
    delete rootItem;
    rootItem = new QmlTreeItem(QmlTreeItem::Folder);
    m_rootFolders.clear();
    endResetModel();
}

void QmlTreeModel::syncManifestsEntry(QmlTreeItem *folder)
{
    if (!folder || folder == rootItem || folder->type() != QmlTreeItem::Folder)
        return;
    QmlTreeItem *entry = nullptr;
    int folderChildren = 0;
    for (QmlTreeItem *c : folder->m_children) {
        if (c->type() == QmlTreeItem::Folder)
            ++folderChildren;
        else if (c->type() == QmlTreeItem::ManifestsEntry)
            entry = c;
    }
    // Top-level roots always expose their manifests through an entry,
    // even without subfolders: the root row itself stays a container.
    const bool wantEntry = folder->hasManifests()
        && (folderChildren > 0 || folder->parent() == rootItem);
    if (wantEntry && !entry) {
        const QModelIndex parentIdx = indexForItem(folder);
        beginInsertRows(parentIdx, 0, 0);
        auto *item = new QmlTreeItem(QmlTreeItem::ManifestsEntry, folder);
        item->m_name = folder->m_name;
        item->m_path = folder->m_path;
        folder->m_children.prepend(item);
        entry = item;
        endInsertRows();
    } else if (!wantEntry && entry) {
        const QModelIndex parentIdx = indexForItem(folder);
        const int row = entry->row();
        beginRemoveRows(parentIdx, row, row);
        folder->m_children.removeAt(row);
        delete entry;
        entry = nullptr;
    }
    if (entry) {
        const QString single = folder->m_manifests.size() == 1
            ? folder->m_manifests.values().value(0) : QString();
        if (entry->m_manifest != single) {
            entry->m_manifest = single;
            const QModelIndex idx = indexForItem(entry);
            emit dataChanged(idx, idx, {ManifestRole});
        }
    }
}

void QmlTreeModel::addProjectManifest(const QString &projectPath,
                                      const QString &manifest)
{
    if (manifest.isEmpty())
        return;
    QmlTreeItem *parent = ensureFolder(projectPath);
    if (parent == rootItem || parent->m_manifests.contains(manifest))
        return;
    parent->m_manifests.insert(manifest);
    syncManifestsEntry(parent);
}

QModelIndex QmlTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return {};

    QmlTreeItem *parentItem = getItem(parent);
    QmlTreeItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    return {};
}

QModelIndex QmlTreeModel::parent(const QModelIndex &idx) const
{
    if (!idx.isValid())
        return {};

    QmlTreeItem *childItem = getItem(idx);
    QmlTreeItem *parentItem = childItem->parent();

    if (parentItem == rootItem || !parentItem)
        return {};

    return createIndex(parentItem->row(), 0, parentItem);
}

int QmlTreeModel::rowCount(const QModelIndex &parent) const
{
    QmlTreeItem *parentItem = getItem(parent);
    if (parentItem->type() != QmlTreeItem::Folder)
        return 0; // manifests entries are leaves
    return parentItem->childCount();
}

int QmlTreeModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 1;
}

QVariant QmlTreeModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid())
        return {};

    QmlTreeItem *item = getItem(idx);

    switch (role) {
        case Qt::DisplayRole:
            return item->name();

        case ItemTypeRole:
            return item->type() == QmlTreeItem::Folder ? "folder" : "manifests";

        case FolderNameRole:
            return item->name();

        case FolderPathRole:
            return item->path();

        case GitBranchRole:
            return item->type() == QmlTreeItem::Folder ? item->m_gitBranch : QString();

        case HasManifestsRole:
            return item->type() == QmlTreeItem::Folder
                ? item->hasManifests() : true;

        case ManifestRole:
            // Folders and entries: the single manifest name, or "" for
            // several (the stack icon). Folders without manifests: "".
            if (item->type() == QmlTreeItem::ManifestsEntry)
                return item->manifest();
            if (item->type() == QmlTreeItem::Folder && item->hasManifests())
                return item->m_manifests.size() == 1
                    ? item->m_manifests.values().value(0) : QString();
            return {};

        default:
            return {};
    }
}

QHash<int, QByteArray> QmlTreeModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Qt::DisplayRole] = "display";
    roles[ItemTypeRole] = "item_type";
    roles[FolderNameRole] = "folderName";
    roles[FolderPathRole] = "folderPath";
    roles[HasManifestsRole] = "hasManifests";
    roles[ManifestRole] = "manifest";
    roles[GitBranchRole] = "gitBranch";
    return roles;
}
