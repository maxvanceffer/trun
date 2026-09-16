#include "treemodel.h"
#include <algorithm>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>

namespace {

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
      m_name(), m_path(), m_manifest(), m_description(), m_commands()
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
        rootItem->appendChild(node);
        m_rootFolders.append(node);
        endInsertRows();
    }
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
            parent->appendChild(child);
            endInsertRows();
        }
        parent = child;
    }
    return parent;
}

void QmlTreeModel::clear()
{
    beginResetModel();
    delete rootItem;
    rootItem = new QmlTreeItem(QmlTreeItem::Folder);
    m_rootFolders.clear();
    endResetModel();
}

void QmlTreeModel::addProject(
    const QString &projectPath,
    const QString &name,
    const QString &description,
    const QString &manifest,
    const QVariant &commands)
{
    QmlTreeItem *parent = ensureFolder(projectPath);

    for (int i = 0; i < parent->childCount(); ++i) {
        auto *child = parent->child(i);
        if (child->type() == QmlTreeItem::Project
            && child->path() == projectPath
            && child->manifest() == manifest) {
            return;
        }
    }

    const QModelIndex parentIdx = indexForItem(parent);
    const int row = parent->childCount();
    beginInsertRows(parentIdx, row, row);
    auto *item = new QmlTreeItem(QmlTreeItem::Project, parent);
    item->m_name = name;
    item->m_path = projectPath;
    item->m_manifest = manifest;
    item->m_description = description;
    item->m_commands = commands;
    parent->appendChild(item);
    endInsertRows();
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
    if (parentItem->type() == QmlTreeItem::Project)
        return 0;
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
            return item->type() == QmlTreeItem::Folder ? "folder" : "project";

        case FolderNameRole:
            return item->name();

        case FolderPathRole:
            return item->path();

        case NameRole:
            if (item->type() == QmlTreeItem::Project)
                return item->name();
            return {};

        case ProjectIdRole:
            if (item->type() == QmlTreeItem::Project)
                return item->path() + QLatin1Char('/') + item->manifest();
            return {};

        case DescriptionRole:
            if (item->type() == QmlTreeItem::Project)
                return item->description();
            return {};

        case ManifestRole:
            if (item->type() == QmlTreeItem::Project)
                return item->manifest();
            return {};

        case CommandsRole:
            if (item->type() == QmlTreeItem::Project)
                return item->commands();
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
    roles[NameRole] = "name";
    roles[ProjectIdRole] = "project_id";
    roles[DescriptionRole] = "description";
    roles[ManifestRole] = "manifest";
    roles[CommandsRole] = "commands";
    return roles;
}
