#pragma once

#include <QObject>
#include <QString>

// Host machine metrics for the Dashboard: host name, CPU load and memory
// usage. Values are -1 when unavailable (unsupported platform).
class SystemStats : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString hostName READ hostName CONSTANT)
    Q_PROPERTY(QString platform READ platform CONSTANT)
    // 0..1, or -1 unknown
    Q_PROPERTY(qreal cpuUsage READ cpuUsage NOTIFY statsChanged)
    Q_PROPERTY(qreal memoryUsage READ memoryUsage NOTIFY statsChanged)
    Q_PROPERTY(qlonglong memoryTotalKb READ memoryTotalKb NOTIFY statsChanged)
    Q_PROPERTY(qlonglong memoryUsedKb READ memoryUsedKb NOTIFY statsChanged)

public:
    explicit SystemStats(QObject *parent = nullptr);

    QString hostName() const { return m_hostName; }
    QString platform() const { return m_platform; }
    qreal cpuUsage() const { return m_cpuUsage; }
    qreal memoryUsage() const { return m_memoryUsage; }
    qlonglong memoryTotalKb() const { return m_memoryTotalKb; }
    qlonglong memoryUsedKb() const { return m_memoryUsedKb; }

    Q_INVOKABLE void refresh();

signals:
    void statsChanged();

private:
    void sampleCpu();
    void sampleMemory();

    QString m_hostName;
    QString m_platform;
    qreal m_cpuUsage = -1.0;
    qreal m_memoryUsage = -1.0;
    qlonglong m_memoryTotalKb = -1;
    qlonglong m_memoryUsedKb = -1;

    // Previous CPU tick counters, for delta-based load estimation.
    quint64 m_prevCpuTotal = 0;
    quint64 m_prevCpuIdle = 0;
};
