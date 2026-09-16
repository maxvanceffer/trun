#include "updater.h"
#include "settings.h"
#include "commandexecutor.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QDateTime>

namespace {
constexpr int CheckTimeoutMs = 30000;
constexpr int DownloadTimeoutMs = 300000;
constexpr qint64 AutoCheckIntervalSecs = 24 * 3600;

QString releasesApiUrl()
{
    return QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest")
        .arg(QLatin1String(Updater::RepoOwner), QLatin1String(Updater::RepoName));
}

void armTimeout(QNetworkReply *reply, int msecs)
{
    QTimer::singleShot(msecs, reply, [reply]() {
        if (reply->isRunning())
            reply->abort();
    });
}
} // namespace

Updater::Updater(Settings *settings, CommandExecutor *executor, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_executor(executor)
    , m_nam(new QNetworkAccessManager(this))
{
}

QString Updater::currentVersion() const
{
    return QCoreApplication::applicationVersion();
}

bool Updater::canInstall() const
{
#ifdef Q_OS_MACOS
    QDir bundleDir(QCoreApplication::applicationDirPath());
    if (!bundleDir.cdUp() || !bundleDir.cdUp())
        return false;
    // Contents/MacOS -> trun.app; only auto-replace released installs.
    if (!bundleDir.dirName().endsWith(QStringLiteral(".app"), Qt::CaseInsensitive))
        return false;
    return bundleDir.absolutePath().startsWith(QStringLiteral("/Applications/"));
#else
    return false;
#endif
}

void Updater::setState(const QString &state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void Updater::setError(const QString &error)
{
    m_error = error;
    m_updateAvailable = false;
    setState(QStringLiteral("error"));
}

void Updater::checkForUpdates()
{
    if (checking() || downloading())
        return;
    setState(QStringLiteral("checking"));
    m_error.clear();

    QNetworkRequest req{QUrl{releasesApiUrl()}};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("trun-updater"));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_checkReply = m_nam->get(req);
    armTimeout(m_checkReply, CheckTimeoutMs);
    connect(m_checkReply, &QNetworkReply::finished, this, &Updater::onCheckFinished);
}

void Updater::onCheckFinished()
{
    QNetworkReply *reply = m_checkReply;
    m_checkReply = nullptr;
    if (!reply)
        return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 404) {
            setError(tr("No releases published on GitHub yet."));
        } else if (reply->error() == QNetworkReply::OperationCanceledError) {
            setError(tr("Update check timed out. Check your connection and retry."));
        } else {
            setError(tr("Update check failed: %1").arg(reply->errorString()));
        }
        return;
    }

    const ReleaseInfo info = parseReleasePayload(
        QJsonDocument::fromJson(reply->readAll()), currentVersion());
    if (!info.ok) {
        setError(info.error);
        return;
    }
    m_latestVersion = info.latestVersion;
    m_notes = info.notes;
    m_zipUrl = info.zipUrl;
    m_pageUrl = info.pageUrl.isValid() ? info.pageUrl : m_pageUrl;
    m_updateAvailable = info.newer;
    setState(info.newer ? QStringLiteral("available") : QStringLiteral("upToDate"));
}

void Updater::downloadUpdate()
{
    if (downloading() || m_state == QStringLiteral("ready"))
        return;
    if (m_latestVersion.isEmpty() || !m_zipUrl.isValid())
        return;

    QDir().mkpath(updateWorkDir());
    m_downloadPath = updateWorkDir() + QStringLiteral("/trun-%1-macos-arm64.zip")
                         .arg(m_latestVersion);
    QFile::remove(m_downloadPath);
    m_downloadFile = new QFile(m_downloadPath, this);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        setError(tr("Cannot write update file: %1").arg(m_downloadFile->errorString()));
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
        return;
    }

    setState(QStringLiteral("downloading"));
    m_progress = 0.0;
    m_cancelled = false;
    emit downloadProgressChanged();

    QNetworkRequest req(m_zipUrl);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("trun-updater"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_downloadReply = m_nam->get(req);
    armTimeout(m_downloadReply, DownloadTimeoutMs);
    connect(m_downloadReply, &QNetworkReply::readyRead, this, &Updater::onDownloadReadyRead);
    connect(m_downloadReply, &QNetworkReply::downloadProgress,
            this, &Updater::onDownloadProgress);
    connect(m_downloadReply, &QNetworkReply::finished, this, &Updater::onDownloadFinished);
}

void Updater::onDownloadReadyRead()
{
    if (m_downloadReply && m_downloadFile)
        m_downloadFile->write(m_downloadReply->readAll());
}

void Updater::onDownloadProgress(qint64 received, qint64 total)
{
    if (total > 0) {
        m_progress = static_cast<double>(received) / static_cast<double>(total);
        emit downloadProgressChanged();
    }
}

void Updater::onDownloadFinished()
{
    QNetworkReply *reply = m_downloadReply;
    m_downloadReply = nullptr;
    if (!reply)
        return;
    reply->deleteLater();
    if (m_downloadFile) {
        m_downloadFile->write(reply->readAll());
        m_downloadFile->close();
    }

    if (reply->error() != QNetworkReply::NoError) {
        const bool wasCancelled = m_cancelled;
        m_cancelled = false;
        cleanupDownloadFile();
        if (wasCancelled) {
            m_updateAvailable = true;
            setState(QStringLiteral("available"));
        } else if (reply->error() == QNetworkReply::OperationCanceledError) {
            setError(tr("Download timed out. Retry when the connection is stable."));
        } else {
            setError(tr("Download failed: %1").arg(reply->errorString()));
        }
        return;
    }
    if (!QFileInfo(m_downloadPath).exists() || QFileInfo(m_downloadPath).size() == 0) {
        cleanupDownloadFile();
        setError(tr("Downloaded file is empty. Retry the download."));
        return;
    }
    m_progress = 1.0;
    emit downloadProgressChanged();
    setState(QStringLiteral("ready"));
}

void Updater::cancelDownload()
{
    if (m_downloadReply && m_downloadReply->isRunning()) {
        m_cancelled = true;
        // finished() fires after abort; the cancelled path is handled there.
        m_downloadReply->abort();
    }
}

void Updater::cleanupDownloadFile()
{
    if (m_downloadFile) {
        m_downloadFile->close();
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
    }
    if (!m_downloadPath.isEmpty())
        QFile::remove(m_downloadPath);
}

void Updater::installAndRestart()
{
#ifdef Q_OS_MACOS
    if (m_state != QStringLiteral("ready") || !canInstall()
        || !QFileInfo(m_downloadPath).exists()) {
        return;
    }
    setState(QStringLiteral("installing"));

    const QString workDir = updateWorkDir() + QStringLiteral("/install");
    QDir().mkpath(workDir);
    // Clear a stale extraction, but never the running bundle.
    QDir(workDir).removeRecursively();
    QDir().mkpath(workDir);

    m_extractProcess = new QProcess(this);
    connect(m_extractProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Updater::onExtractFinished);
    m_extractProcess->start(QStringLiteral("/usr/bin/ditto"),
                            {QStringLiteral("-x"), QStringLiteral("-k"),
                             m_downloadPath, workDir});
#else
    openReleasesPage();
#endif
}

void Updater::onExtractFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QProcess *proc = m_extractProcess;
    m_extractProcess = nullptr;
    if (proc)
        proc->deleteLater();

    const QString workDir = updateWorkDir() + QStringLiteral("/install");
    const QString bundleSrc = workDir + QStringLiteral("/trun.app");
    if (exitStatus != QProcess::NormalExit || exitCode != 0
        || !QDir(bundleSrc).exists()) {
        setError(tr("Could not unpack the update. Try downloading it manually."));
        return;
    }

    QDir bundleDir(QCoreApplication::applicationDirPath());
    bundleDir.cdUp();
    bundleDir.cdUp();
    const QString bundleDst = bundleDir.absolutePath();
    const QString scriptPath = updateWorkDir() + QStringLiteral("/install.sh");
    if (!writeInstallScript(scriptPath, bundleSrc, bundleDst)) {
        setError(tr("Could not prepare the installer. Try downloading manually."));
        return;
    }

    // Same "leave running" semantics as a manual quit: live runs survive
    // the restart and are adopted back on startup.
    if (m_settings && m_executor) {
        m_settings->set(QStringLiteral("orphanedProcesses"),
                        m_executor->runningProcesses());
        m_executor->detachAll();
    }
    QProcess::startDetached(QStringLiteral("/bin/bash"),
                            {scriptPath,
                             QString::number(QCoreApplication::applicationPid()),
                             bundleSrc, bundleDst});
    QCoreApplication::quit();
}

bool Updater::writeInstallScript(const QString &scriptPath, const QString &bundleSrc,
                                 const QString &bundleDst) const
{
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    script.write("#!/bin/bash\n"
                 "# trun self-update: waits for the old process, swaps the bundle, relaunches.\n"
                 "PID=\"$1\"\n"
                 "SRC=\"$2\"\n"
                 "DST=\"$3\"\n"
                 "for i in $(seq 1 150); do\n"
                 "  kill -0 \"$PID\" 2>/dev/null || break\n"
                 "  sleep 0.2\n"
                 "done\n"
                 "rm -rf \"$DST\"\n"
                 "mv \"$SRC\" \"$DST\"\n"
                 "open \"$DST\"\n");
    script.close();
    QFile::setPermissions(scriptPath,
                          QFile::permissions(scriptPath) | QFile::ExeOwner);
    return true;
}

void Updater::openReleasesPage()
{
    QDesktopServices::openUrl(m_pageUrl);
}

void Updater::checkOnStartup()
{
    if (!m_settings) {
        checkForUpdates();
        return;
    }
    const QDateTime last =
        QDateTime::fromString(m_settings->get(QStringLiteral("updates/lastCheck")).toString(),
                              Qt::ISODate);
    if (last.isValid()
        && last.secsTo(QDateTime::currentDateTimeUtc()) < AutoCheckIntervalSecs) {
        return;
    }
    m_settings->set(QStringLiteral("updates/lastCheck"),
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    checkForUpdates();
}

QString Updater::updateWorkDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/updates/trun-%1").arg(
            m_latestVersion.isEmpty() ? QStringLiteral("latest") : m_latestVersion);
}

// --- Pure logic (unit-tested) ---

QVersionNumber Updater::versionFromTag(const QString &tag)
{
    QString t = tag.trimmed();
    if (t.startsWith(QLatin1Char('v')) || t.startsWith(QLatin1Char('V')))
        t = t.mid(1);
    return QVersionNumber::fromString(t);
}

bool Updater::isNewerVersion(const QString &latest, const QString &current)
{
    const QVersionNumber l = versionFromTag(latest);
    const QVersionNumber c = versionFromTag(current);
    if (l.isNull() || c.isNull())
        return false;
    return l > c;
}

QUrl Updater::pickMacZipUrl(const QJsonArray &assets)
{
    for (const QJsonValue &v : assets) {
        const QJsonObject a = v.toObject();
        const QString name = a.value(QStringLiteral("name")).toString();
        if (name.endsWith(QStringLiteral("-macos-arm64.zip"))) {
            const QUrl url(a.value(QStringLiteral("browser_download_url")).toString());
            if (url.isValid())
                return url;
        }
    }
    return QUrl();
}

Updater::ReleaseInfo Updater::parseReleasePayload(const QJsonDocument &doc,
                                                  const QString &currentVersion)
{
    ReleaseInfo info;
    if (!doc.isObject()) {
        info.error = tr("Unexpected response from GitHub.");
        return info;
    }
    const QJsonObject obj = doc.object();
    if (obj.contains(QStringLiteral("message"))) {
        info.error = tr("GitHub API: %1")
                         .arg(obj.value(QStringLiteral("message")).toString());
        return info;
    }
    const QString tag = obj.value(QStringLiteral("tag_name")).toString();
    if (versionFromTag(tag).isNull()) {
        info.error = tr("Release has no parseable version tag.");
        return info;
    }
    info.ok = true;
    info.latestVersion = tag.startsWith(QLatin1Char('v')) || tag.startsWith(QLatin1Char('V'))
        ? tag.mid(1)
        : tag;
    info.notes = obj.value(QStringLiteral("body")).toString();
    info.pageUrl = QUrl(obj.value(QStringLiteral("html_url")).toString());
    info.zipUrl = pickMacZipUrl(obj.value(QStringLiteral("assets")).toArray());
    info.newer = isNewerVersion(tag, currentVersion);
    return info;
}
