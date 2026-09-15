#include "systemstats.h"

#include <QSysInfo>
#include <QTimer>

#ifdef Q_OS_MACOS
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/vm_statistics.h>
#include <sys/sysctl.h>
#endif

#ifdef Q_OS_LINUX
#include <QFile>
#endif

namespace {

// Reads total/used memory in KB. Either may stay -1 when unknown.
void readMemoryKb(qlonglong &totalKb, qlonglong &usedKb)
{
#ifdef Q_OS_MACOS
    uint64_t bytes = 0;
    size_t size = sizeof(bytes);
    if (::sysctlbyname("hw.memsize", &bytes, &size, nullptr, 0) == 0)
        totalKb = static_cast<qlonglong>(bytes / 1024);

    vm_statistics64_data_t vm{};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                          reinterpret_cast<host_info64_t>(&vm), &count) == KERN_SUCCESS) {
        vm_size_t pageSize = 0;
        host_page_size(mach_host_self(), &pageSize);
        const quint64 usedPages =
            vm.active_count + vm.wire_count + vm.compressor_page_count;
        usedKb = static_cast<qlonglong>(usedPages) * pageSize / 1024;
    }
#elif defined(Q_OS_LINUX)
    QFile meminfo(QStringLiteral("/proc/meminfo"));
    if (!meminfo.open(QIODevice::ReadOnly))
        return;
    qlonglong availableKb = -1;
    while (!meminfo.atEnd()) {
        const QString line = QString::fromUtf8(meminfo.readLine());
        if (line.startsWith(QLatin1String("MemTotal:")))
            totalKb = line.split(QLatin1Char(':')).value(1).simplified()
                          .split(QLatin1Char(' ')).value(0).toLongLong();
        else if (line.startsWith(QLatin1String("MemAvailable:")))
            availableKb = line.split(QLatin1Char(':')).value(1).simplified()
                              .split(QLatin1Char(' ')).value(0).toLongLong();
    }
    if (totalKb > 0 && availableKb >= 0)
        usedKb = totalKb - availableKb;
#else
    Q_UNUSED(totalKb)
    Q_UNUSED(usedKb)
#endif
}

// Returns cumulative CPU ticks and idle ticks, or false when unavailable.
bool readCpuTicks(quint64 &total, quint64 &idle)
{
#ifdef Q_OS_MACOS
    host_cpu_load_info_data_t info{};
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                        reinterpret_cast<host_info_t>(&info), &count) != KERN_SUCCESS)
        return false;
    const quint64 user = info.cpu_ticks[CPU_STATE_USER];
    const quint64 system = info.cpu_ticks[CPU_STATE_SYSTEM];
    const quint64 nice = info.cpu_ticks[CPU_STATE_NICE];
    idle = info.cpu_ticks[CPU_STATE_IDLE];
    total = user + system + nice + idle;
    return true;
#elif defined(Q_OS_LINUX)
    QFile stat(QStringLiteral("/proc/stat"));
    if (!stat.open(QIODevice::ReadOnly))
        return false;
    const QStringList parts =
        QString::fromUtf8(stat.readLine()).simplified().split(QLatin1Char(' '));
    if (parts.size() < 6)
        return false;
    const quint64 user = parts.value(1).toULongLong();
    const quint64 nice = parts.value(2).toULongLong();
    const quint64 system = parts.value(3).toULongLong();
    const quint64 idleTicks = parts.value(4).toULongLong();
    const quint64 iowait = parts.value(5).toULongLong();
    idle = idleTicks + iowait;
    total = user + nice + system + idle;
    return true;
#else
    Q_UNUSED(total)
    Q_UNUSED(idle)
    return false;
#endif
}

} // namespace

SystemStats::SystemStats(QObject *parent) : QObject(parent)
{
    m_hostName = QSysInfo::machineHostName();
    m_platform = QSysInfo::prettyProductName();

    // First sample only establishes the CPU baseline.
    sampleCpu();
    sampleMemory();

    auto *timer = new QTimer(this);
    timer->setInterval(2000);
    connect(timer, &QTimer::timeout, this, &SystemStats::refresh);
    timer->start();
}

void SystemStats::refresh()
{
    sampleCpu();
    sampleMemory();
    emit statsChanged();
}

void SystemStats::sampleCpu()
{
    quint64 total = 0;
    quint64 idle = 0;
    if (!readCpuTicks(total, idle))
        return;

    if (m_prevCpuTotal != 0 && total > m_prevCpuTotal) {
        const quint64 dTotal = total - m_prevCpuTotal;
        const quint64 dIdle = idle - m_prevCpuIdle;
        const qreal busy = static_cast<qreal>(dTotal - dIdle) / static_cast<qreal>(dTotal);
        m_cpuUsage = qBound(0.0, busy, 1.0);
    }
    m_prevCpuTotal = total;
    m_prevCpuIdle = idle;
}

void SystemStats::sampleMemory()
{
    qlonglong totalKb = -1;
    qlonglong usedKb = -1;
    readMemoryKb(totalKb, usedKb);
    m_memoryTotalKb = totalKb;
    m_memoryUsedKb = usedKb;
    if (totalKb > 0 && usedKb >= 0)
        m_memoryUsage = qBound(0.0, static_cast<qreal>(usedKb) / static_cast<qreal>(totalKb), 1.0);
    else
        m_memoryUsage = -1.0;
}
