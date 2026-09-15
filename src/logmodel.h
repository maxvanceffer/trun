#pragma once

#include <QObject>
#include <QAbstractListModel>
#include <QString>
#include <QDateTime>

struct LogEntry {
    QString timestamp;
    QString level;
    QString target;
    QString message;
};

class LogModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        TimestampRole = Qt::UserRole + 1,
        LevelRole,
        TargetRole,
        MessageRole
    };

    explicit LogModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = {}) const override {
        if (parent.isValid()) return 0;
        return static_cast<int>(m_entries.size());
    }

    QHash<int, QByteArray> roleNames() const override {
        QHash<int, QByteArray> roles;
        roles[TimestampRole] = "timestamp";
        roles[LevelRole]     = "level";
        roles[TargetRole]    = "target";
        roles[MessageRole]   = "message";
        return roles;
    }

    Q_INVOKABLE void add(const QString &level, const QString &target, const QString &message) {
        // Cap history at 1000 entries, dropping the oldest first
        while (m_entries.size() >= 1000) {
            beginRemoveRows(QModelIndex(), 0, 0);
            m_entries.removeFirst();
            endRemoveRows();
        }
        int row = m_entries.size();
        beginInsertRows(QModelIndex(), row, row);
        m_entries.append({
            QDateTime::currentDateTime().toString("HH:mm:ss.zzz"),
            level, target, message
        });
        endInsertRows();
    }

    Q_INVOKABLE void clear() {
        beginResetModel();
        m_entries.clear();
        endResetModel();
    }

    // Random access for the single-text console (DetailPage rebuilds and
    // appends HTML without going through ListView delegates).
    Q_INVOKABLE int count() const { return static_cast<int>(m_entries.size()); }

    Q_INVOKABLE QVariantMap get(int row) const {
        if (row < 0 || row >= m_entries.size())
            return {};
        const auto &e = m_entries[row];
        return {
            {QStringLiteral("timestamp"), e.timestamp},
            {QStringLiteral("level"), e.level},
            {QStringLiteral("target"), e.target},
            {QStringLiteral("message"), e.message},
        };
    }

    QVariant data(const QModelIndex &index, int role) const override {
        if (!index.isValid() || index.row() >= m_entries.size())
            return {};
        const auto &entry = m_entries[index.row()];
        switch (role) {
        case TimestampRole: return entry.timestamp;
        case LevelRole:     return entry.level;
        case TargetRole:    return entry.target;
        case MessageRole:   return entry.message;
        default: return {};
        }
    }

private:
    QList<LogEntry> m_entries;
};
