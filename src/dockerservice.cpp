#include "dockerservice.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QProcess>
#include <QRegularExpression>
#include <QThread>

namespace {

QString runCapture(const QString &program, const QStringList &args, int timeoutMs = 15000)
{
    QProcess p;
    p.start(program, args);
    if (!p.waitForFinished(timeoutMs))
        return {};
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

bool runOk(const QString &program, const QStringList &args, int timeoutMs = 60000)
{
    QProcess p;
    p.start(program, args);
    if (!p.waitForFinished(timeoutMs))
        return false;
    return p.exitCode() == 0;
}

} // namespace

bool DockerService::runDocker(const QStringList &args, QString *output, int timeoutMs)
{
    QProcess p;
    p.start(QStringLiteral("docker"), args);
    if (!p.waitForFinished(timeoutMs)) {
        if (output)
            *output = QStringLiteral("docker timed out");
        return false;
    }
    const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    const bool ok = p.exitCode() == 0;
    if (output) {
        *output = out;
        if (!ok) {
            const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
            if (!err.isEmpty())
                *output = out.isEmpty() ? err : out + u'\n' + err;
        }
    }
    return ok;
}

QVariantMap DockerService::queryEngine()
{
    QVariantMap e;
    const bool available = runOk(QStringLiteral("docker"), {QStringLiteral("info")});
    e[QStringLiteral("available")] = available;
    if (!available) {
        e[QStringLiteral("version")] = QString();
        e[QStringLiteral("context")] = QString();
        e[QStringLiteral("runtime")] = QString();
        e[QStringLiteral("colimaInstalled")] = !runCapture(QStringLiteral("which"), {QStringLiteral("colima")}).isEmpty();
        e[QStringLiteral("colimaRunning")] = false;
        return e;
    }
    const QString version = runCapture(QStringLiteral("docker"), {QStringLiteral("version"), QStringLiteral("--format"), QStringLiteral("{{.Server.Version}}")});
    const QString context = runCapture(QStringLiteral("docker"), {QStringLiteral("context"), QStringLiteral("show")});
    e[QStringLiteral("version")] = version;
    e[QStringLiteral("context")] = context;
    QString runtime = context;
    if (context == QStringLiteral("colima"))
        runtime = QStringLiteral("colima");
    else if (context == QStringLiteral("desktop-linux"))
        runtime = QStringLiteral("Docker Desktop");
    else if (context == QStringLiteral("orbstack"))
        runtime = QStringLiteral("OrbStack");
    e[QStringLiteral("runtime")] = runtime;

    const bool colimaInstalled = !runCapture(QStringLiteral("which"), {QStringLiteral("colima")}).isEmpty();
    e[QStringLiteral("colimaInstalled")] = colimaInstalled;
    // `colima status` always prints "...running..." (even when stopped:
    // "colima is not running"), so parse `colima list` STATUS column.
    bool colimaRunning = false;
    if (colimaInstalled) {
        const QStringList rows = runCapture(QStringLiteral("colima"), {QStringLiteral("list")})
                                     .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (int i = 1; i < rows.size(); ++i) {
            const QStringList cols = rows.at(i).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
            if (cols.size() >= 2 && cols.at(1) == QStringLiteral("Running")) {
                colimaRunning = true;
                break;
            }
        }
    }
    e[QStringLiteral("colimaRunning")] = colimaRunning;
    return e;
}

QVariantList DockerService::queryContainers()
{
    QVariantList out;
    const QString raw = runCapture(QStringLiteral("docker"),
                                   {QStringLiteral("ps"), QStringLiteral("-a"), QStringLiteral("--format"), QStringLiteral("{{json .}}")});
    if (raw.isEmpty())
        return out;
    const QStringList lines = raw.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject())
            continue;
        const QJsonObject o = doc.object();
        const QString state = o.value(QStringLiteral("State")).toString();
        const QString status = o.value(QStringLiteral("Status")).toString();
        const QString labels = o.value(QStringLiteral("Labels")).toString();
        QString project;
        const int pi = labels.indexOf(QStringLiteral("com.docker.compose.project="));
        if (pi >= 0) {
            project = labels.mid(pi + QStringLiteral("com.docker.compose.project=").size());
            project = project.split(QLatin1Char(',')).first();
        }
        QVariantMap c;
        c[QStringLiteral("id")] = o.value(QStringLiteral("ID")).toString();
        c[QStringLiteral("name")] = o.value(QStringLiteral("Names")).toString();
        c[QStringLiteral("image")] = o.value(QStringLiteral("Image")).toString();
        c[QStringLiteral("status")] = status;
        c[QStringLiteral("running")] = state == QStringLiteral("running") || status.startsWith(QStringLiteral("Up"));
        c[QStringLiteral("ports")] = o.value(QStringLiteral("Ports")).toString();
        c[QStringLiteral("project")] = project;
        out << c;
    }
    return out;
}

QVariantList DockerService::queryImages()
{
    QVariantList out;
    const QString raw = runCapture(QStringLiteral("docker"),
                                   {QStringLiteral("images"), QStringLiteral("--format"), QStringLiteral("{{json .}}")});
    if (raw.isEmpty())
        return out;
    const QStringList lines = raw.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject())
            continue;
        const QJsonObject o = doc.object();
        QVariantMap img;
        img[QStringLiteral("id")] = o.value(QStringLiteral("ID")).toString();
        img[QStringLiteral("repository")] = o.value(QStringLiteral("Repository")).toString();
        img[QStringLiteral("tag")] = o.value(QStringLiteral("Tag")).toString();
        img[QStringLiteral("size")] = o.value(QStringLiteral("Size")).toString();
        img[QStringLiteral("created")] = o.value(QStringLiteral("CreatedAt")).toString();
        out << img;
    }
    return out;
}

// `docker system df` per-type usage (Images/Containers/Local Volumes/Build Cache).
QVariantMap DockerService::queryDisk()
{
    QVariantMap out;
    const QString raw = runCapture(QStringLiteral("docker"),
                                   {QStringLiteral("system"), QStringLiteral("df"),
                                    QStringLiteral("--format"), QStringLiteral("{{json .}}")});
    if (raw.isEmpty())
        return out;
    const QStringList lines = raw.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject())
            continue;
        const QJsonObject o = doc.object();
        QVariantMap t;
        t[QStringLiteral("total")] = o.value(QStringLiteral("TotalCount")).toString();
        t[QStringLiteral("active")] = o.value(QStringLiteral("Active")).toString();
        t[QStringLiteral("size")] = o.value(QStringLiteral("Size")).toString();
        t[QStringLiteral("reclaimable")] = o.value(QStringLiteral("Reclaimable")).toString();
        out[o.value(QStringLiteral("Type")).toString()] = t;
    }
    return out;
}

// Live resource usage for running containers, keyed by container name.
QVariantMap DockerService::queryStats()
{
    QVariantMap out;
    const QString raw = runCapture(QStringLiteral("docker"),
                                   {QStringLiteral("stats"), QStringLiteral("--no-stream"),
                                    QStringLiteral("--format"), QStringLiteral("{{json .}}")});
    if (raw.isEmpty())
        return out;
    const QStringList lines = raw.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject())
            continue;
        const QJsonObject o = doc.object();
        const QString name = o.value(QStringLiteral("Name")).toString();
        if (name.isEmpty())
            continue;
        QVariantMap s;
        s[QStringLiteral("cpu")] = o.value(QStringLiteral("CPUPerc")).toString();
        s[QStringLiteral("mem")] = o.value(QStringLiteral("MemUsage")).toString();
        s[QStringLiteral("memPerc")] = o.value(QStringLiteral("MemPerc")).toString();
        s[QStringLiteral("net")] = o.value(QStringLiteral("NetIO")).toString();
        s[QStringLiteral("pids")] = o.value(QStringLiteral("PIDs")).toString();
        out[name] = s;
    }
    return out;
}

QString DockerService::queryLogs(const QString &name, int tail)
{
    // Merged channels: container stdout and stderr both arrive here.
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(QStringLiteral("docker"),
            {QStringLiteral("logs"), QStringLiteral("--tail"), QString::number(tail),
             QStringLiteral("--timestamps"), name});
    if (!p.waitForFinished(15000))
        return QStringLiteral("docker logs timed out");
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

DockerService::DockerService(QObject *parent) : QObject(parent)
{
    // Scan once at startup (off the GUI thread) so the page has data cached.
    refresh();
}

void DockerService::refresh()
{
    if (m_busy)
        return;
    m_busy = true;
    emit busyChanged();

    QThread *thread = QThread::create([this]() {
        const QVariantMap engine = queryEngine();
        const bool up = engine.value(QStringLiteral("available")).toBool();
        const QVariantList containers = up ? queryContainers() : QVariantList{};
        const QVariantList images = up ? queryImages() : QVariantList{};
        const QVariantMap disk = up ? queryDisk() : QVariantMap{};
        QMetaObject::invokeMethod(
            this,
            [this, engine, containers, images, disk]() {
                m_engine = engine;
                m_containers = containers;
                m_images = images;
                m_disk = disk;
                m_busy = false;
                emit busyChanged();
                emit engineChanged();
                emit containersChanged();
                emit imagesChanged();
                emit diskChanged();
            },
            Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

bool DockerService::control(const QStringList &args)
{
    if (m_busy)
        return false;
    m_busy = true;
    emit busyChanged();

    QThread *thread = QThread::create([this, args]() {
        QString error;
        if (!runOk(QStringLiteral("docker"), args))
            error = QStringLiteral("docker ") + args.join(QLatin1Char(' ')) + QStringLiteral(" failed");
        const QVariantMap engine = queryEngine();
        const bool up = engine.value(QStringLiteral("available")).toBool();
        const QVariantList containers = up ? queryContainers() : QVariantList{};
        const QVariantList images = up ? queryImages() : QVariantList{};
        const QVariantMap disk = up ? queryDisk() : QVariantMap{};
        QMetaObject::invokeMethod(
            this,
            [this, engine, containers, images, disk, error]() {
                m_engine = engine;
                m_containers = containers;
                m_images = images;
                m_disk = disk;
                m_busy = false;
                emit busyChanged();
                emit engineChanged();
                emit containersChanged();
                emit imagesChanged();
                emit diskChanged();
                if (!error.isEmpty())
                    emit errorMessage(error);
            },
            Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
    return true;
}

bool DockerService::startContainer(const QString &name)
{
    return control({QStringLiteral("start"), name});
}

bool DockerService::stopContainer(const QString &name)
{
    return control({QStringLiteral("stop"), name});
}

bool DockerService::restartContainer(const QString &name)
{
    return control({QStringLiteral("restart"), name});
}

bool DockerService::removeContainer(const QString &name)
{
    return control({QStringLiteral("rm"), QStringLiteral("-f"), name});
}

bool DockerService::controlGroup(const QStringList &names, const QString &action)
{
    if (names.isEmpty())
        return false;
    if (action != QStringLiteral("start") && action != QStringLiteral("stop")
        && action != QStringLiteral("restart"))
        return false;
    QStringList args{action};
    args << names;
    return control(args);
}

bool DockerService::removeImage(const QString &id)
{
    return control({QStringLiteral("rmi"), id});
}

bool DockerService::prune()
{
    return control({QStringLiteral("system"), QStringLiteral("prune"), QStringLiteral("-f")});
}

bool DockerService::startEngine()
{
    // Colima-backed daemon: `colima start`. For Docker Desktop / OrbStack
    // there is no CLI start the app owns, so report inability via error.
    const bool colimaInstalled = !runCapture(QStringLiteral("which"), {QStringLiteral("colima")}).isEmpty();
    if (!colimaInstalled) {
        emit errorMessage(QStringLiteral("No CLI start for this runtime — launch Docker Desktop / OrbStack manually"));
        return false;
    }
    if (m_busy)
        return false;
    m_busy = true;
    emit busyChanged();
    QThread *thread = QThread::create([this]() {
        QString error;
        if (!runOk(QStringLiteral("colima"), {QStringLiteral("start")}, 120000))
            error = QStringLiteral("colima start failed");
        const QVariantMap engine = queryEngine();
        const bool up = engine.value(QStringLiteral("available")).toBool();
        const QVariantList containers = up ? queryContainers() : QVariantList{};
        const QVariantList images = up ? queryImages() : QVariantList{};
        const QVariantMap disk = up ? queryDisk() : QVariantMap{};
        QMetaObject::invokeMethod(
            this,
            [this, engine, containers, images, disk, error]() {
                m_engine = engine;
                m_containers = containers;
                m_images = images;
                m_disk = disk;
                m_busy = false;
                emit busyChanged();
                emit engineChanged();
                emit containersChanged();
                emit imagesChanged();
                emit diskChanged();
                if (!error.isEmpty())
                    emit errorMessage(error);
            },
            Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
    return true;
}

bool DockerService::stopEngine()
{
    const bool colimaInstalled = !runCapture(QStringLiteral("which"), {QStringLiteral("colima")}).isEmpty();
    if (!colimaInstalled) {
        emit errorMessage(QStringLiteral("No CLI stop for this runtime"));
        return false;
    }
    if (m_busy)
        return false;
    m_busy = true;
    emit busyChanged();
    QThread *thread = QThread::create([this]() {
        QString error;
        if (!runOk(QStringLiteral("colima"), {QStringLiteral("stop")}, 120000))
            error = QStringLiteral("colima stop failed");
        const QVariantMap engine = queryEngine();
        QMetaObject::invokeMethod(
            this,
            [this, engine, error]() {
                m_engine = engine;
                m_containers = QVariantList{};
                m_images = QVariantList{};
                m_disk = QVariantMap{};
                m_stats = QVariantMap{};
                m_busy = false;
                emit busyChanged();
                emit engineChanged();
                emit containersChanged();
                emit imagesChanged();
                emit diskChanged();
                emit statsChanged();
                if (!error.isEmpty())
                    emit errorMessage(error);
            },
            Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
    return true;
}

void DockerService::fetchStats()
{
    QThread *thread = QThread::create([this]() {
        const QVariantMap stats = queryStats();
        QMetaObject::invokeMethod(
            this, [this, stats]() {
                m_stats = stats;
                emit statsChanged();
            },
            Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void DockerService::fetchLogs(const QString &name, int tail)
{
    QThread *thread = QThread::create([this, name, tail]() {
        const QString logs = queryLogs(name, tail);
        QMetaObject::invokeMethod(
            this, [this, name, logs]() { emit logsReady(name, logs); },
            Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}
