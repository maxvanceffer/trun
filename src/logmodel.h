#pragma once

#include <QObject>
#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QDateTime>

#include "logparser.h"

struct LogEntry {
    QString timestamp;
    QString level;   // слой 1: stdout/stderr/system/error
    QString target;  // слой 1: label команды
    QString message; // сырая строка
    QString level2;  // распознанный уровень слоя 2 ("" если нет)
    QString channel; // распознанный канал слоя 2 ("" если нет)
    bool parsed = false;
    qint64 arrivedMs = 0;
};

// Console history выбранного Command с фильтрами (решение: движок на C++/модель).
// count()/get()/data() работают по видимым строкам; observed* — по всем,
// чтобы чипсы не исчезали при активной фильтрации.
class LogModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int shownCount READ shownCount NOTIFY countsChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countsChanged)
    Q_PROPERTY(int hiddenUnparsedCount READ hiddenUnparsedCount NOTIFY countsChanged)

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
        return m_visible.size();
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
        // Cap history at 1000 entries, dropping the oldest first.
        // Trim идёт ДО append, чтобы visible-индексы не протухали.
        while (m_entries.size() >= 1000) {
            const int visPos = m_visible.indexOf(0);
            if (visPos >= 0) {
                beginRemoveRows(QModelIndex(), visPos, visPos);
                m_visible.removeAt(visPos);
                endRemoveRows();
            }
            m_entries.removeFirst();
            for (int &v : m_visible)
                --v;
        }
        LogEntry e;
        e.timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
        e.level = level;
        e.target = target;
        e.message = message;
        e.arrivedMs = QDateTime::currentMSecsSinceEpoch();
        const ParsedLogLine p = LogParser::parse(message);
        e.parsed = p.ok;
        e.level2 = p.level;
        e.channel = p.channel;
        m_entries.append(e);
        const int srcRow = m_entries.size() - 1;
        bool hiddenUnparsed = false;
        if (matchesFilter(m_entries.at(srcRow), hiddenUnparsed)) {
            const int visRow = m_visible.size();
            beginInsertRows(QModelIndex(), visRow, visRow);
            m_visible.append(srcRow);
            endInsertRows();
        } else if (hiddenUnparsed) {
            ++m_hiddenUnparsed;
        }
        emit countsChanged();
    }

    Q_INVOKABLE void clear() {
        beginResetModel();
        m_entries.clear();
        m_visible.clear();
        m_hiddenUnparsed = 0;
        endResetModel();
        emit countsChanged();
    }

    // Random access for the single-text console (DetailPage rebuilds and
    // appends HTML without going through ListView delegates).
    // Индексы — по видимым строкам.
    Q_INVOKABLE int count() const { return m_visible.size(); }

    Q_INVOKABLE QVariantMap get(int row) const {
        if (row < 0 || row >= m_visible.size())
            return {};
        const auto &e = m_entries.at(m_visible.at(row));
        return {
            {QStringLiteral("timestamp"), e.timestamp},
            {QStringLiteral("level"), e.level},
            {QStringLiteral("target"), e.target},
            {QStringLiteral("message"), e.message},
        };
    }

    // Фильтры (AND; пустой фильтр выключен; sinceMin < 0 = всё время).
    Q_INVOKABLE void setFilters(const QString &query, const QStringList &levels,
                                const QStringList &channels, int sinceMin) {
        m_query = query;
        m_levels = levels;
        m_channels = channels;
        m_sinceMin = sinceMin;
        beginResetModel();
        rebuildVisible();
        endResetModel();
        emit countsChanged();
    }

    Q_INVOKABLE void resetFilters() { setFilters({}, {}, {}, -1); }

    // Наблюдаемые значения по всем строкам (для чипсов + MCP-шапки).
    Q_INVOKABLE QStringList observedLevels() const {
        QStringList out;
        for (const auto &e : m_entries) {
            if (e.parsed && !e.level2.isEmpty() && !out.contains(e.level2))
                out << e.level2;
        }
        out.sort();
        return out;
    }

    Q_INVOKABLE QStringList observedChannels() const {
        QStringList out;
        for (const auto &e : m_entries) {
            if (e.parsed && !e.channel.isEmpty() && !out.contains(e.channel))
                out << e.channel;
        }
        out.sort();
        return out;
    }

    int shownCount() const { return m_visible.size(); }
    int totalCount() const { return static_cast<int>(m_entries.size()); }
    int hiddenUnparsedCount() const { return m_hiddenUnparsed; }

    QVariant data(const QModelIndex &index, int role) const override {
        if (!index.isValid() || index.row() >= m_visible.size())
            return {};
        const auto &entry = m_entries.at(m_visible.at(index.row()));
        switch (role) {
        case TimestampRole: return entry.timestamp;
        case LevelRole:     return entry.level;
        case TargetRole:    return entry.target;
        case MessageRole:   return entry.message;
        default: return {};
        }
    }

signals:
    void countsChanged();

private:
    bool matchesFilter(const LogEntry &e, bool &hiddenUnparsed) const {
        hiddenUnparsed = false;
        if (m_sinceMin >= 0
            && QDateTime::currentMSecsSinceEpoch() - e.arrivedMs > qint64(m_sinceMin) * 60000)
            return false;
        ParsedLogLine p;
        p.ok = e.parsed;
        p.level = e.level2;
        p.channel = e.channel;
        const LogFilterHit hit = LogParser::applyFilter(
            p, e.message, {m_query, m_levels, m_channels});
        if (hit == LogFilterHit::HiddenUnparsed)
            hiddenUnparsed = true;
        return hit == LogFilterHit::Shown;
    }

    void rebuildVisible() {
        m_visible.clear();
        m_hiddenUnparsed = 0;
        for (int i = 0; i < m_entries.size(); ++i) {
            bool hiddenUnparsed = false;
            if (matchesFilter(m_entries.at(i), hiddenUnparsed))
                m_visible.append(i);
            else if (hiddenUnparsed)
                ++m_hiddenUnparsed;
        }
    }

    QList<LogEntry> m_entries;
    QList<int> m_visible; // source-индексы видимых строк
    int m_hiddenUnparsed = 0;
    QString m_query;
    QStringList m_levels;
    QStringList m_channels;
    int m_sinceMin = -1;
};
