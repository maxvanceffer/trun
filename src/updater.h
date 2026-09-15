#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QUrl>
#include <QVersionNumber>
#include <QJsonArray>
#include <QJsonDocument>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;
class Settings;
class CommandExecutor;

// In-app updater backed by GitHub Releases.
//
// Flow: checkForUpdates() → downloadUpdate() → installAndRestart().
// On macOS, when the app runs from /Applications, install swaps the running
// .app bundle via a helper script and relaunches it; running commands are
// preserved with the same "leave running" semantics as a manual quit.
// Dev builds (outside /Applications) fall back to opening the release page.
class Updater : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool checking READ checking NOTIFY stateChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY stateChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY stateChanged)
    Q_PROPERTY(QString downloadUrl READ downloadUrl NOTIFY stateChanged)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY stateChanged)
    Q_PROPERTY(QString releasePageUrl READ releasePageUrl NOTIFY stateChanged)
    Q_PROPERTY(double downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY stateChanged)
    Q_PROPERTY(bool canInstall READ canInstall CONSTANT)

public:
    static constexpr const char *RepoOwner = "maxvanceffer";
    static constexpr const char *RepoName = "trun";

    // Parsed outcome of a /releases/latest payload.
    struct ReleaseInfo {
        bool ok = false;
        bool newer = false;
        QString latestVersion;
        QString notes;
        QUrl zipUrl; // empty when the release has no macOS zip attached
        QUrl pageUrl;
        QString error;
    };

    explicit Updater(Settings *settings, CommandExecutor *executor,
                     QObject *parent = nullptr);

    // idle|checking|upToDate|available|downloading|ready|installing|error
    QString state() const { return m_state; }
    bool checking() const { return m_state == QStringLiteral("checking"); }
    bool downloading() const { return m_state == QStringLiteral("downloading"); }
    bool updateAvailable() const { return m_updateAvailable; }
    QString currentVersion() const;
    QString latestVersion() const { return m_latestVersion; }
    QString downloadUrl() const { return m_zipUrl.toString(); }
    QString releaseNotes() const { return m_notes; }
    QString releasePageUrl() const { return m_pageUrl.toString(); }
    double downloadProgress() const { return m_progress; }
    QString errorString() const { return m_error; }
    bool canInstall() const;

    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void downloadUpdate();
    Q_INVOKABLE void installAndRestart();
    Q_INVOKABLE void openReleasesPage();
    Q_INVOKABLE void cancelDownload();
    // Startup auto-check, throttled to once per 24h via Settings.
    Q_INVOKABLE void checkOnStartup();

    // Pure logic, covered by test_updater.
    static QVersionNumber versionFromTag(const QString &tag);
    static bool isNewerVersion(const QString &latest, const QString &current);
    static QUrl pickMacZipUrl(const QJsonArray &assets);
    static ReleaseInfo parseReleasePayload(const QJsonDocument &doc,
                                           const QString &currentVersion);

signals:
    void stateChanged();
    void downloadProgressChanged();

private slots:
    void onCheckFinished();
    void onDownloadReadyRead();
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();
    void onExtractFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void setState(const QString &state);
    void setError(const QString &error);
    void cleanupDownloadFile();
    QString updateWorkDir() const;
    bool writeInstallScript(const QString &scriptPath, const QString &bundleSrc,
                            const QString &bundleDst) const;

    Settings *m_settings = nullptr;
    CommandExecutor *m_executor = nullptr;
    QNetworkAccessManager *m_nam = nullptr;
    QNetworkReply *m_checkReply = nullptr;
    QNetworkReply *m_downloadReply = nullptr;
    QFile *m_downloadFile = nullptr;
    QProcess *m_extractProcess = nullptr;

    QString m_state = QStringLiteral("idle");
    bool m_updateAvailable = false;
    QString m_latestVersion;
    QString m_notes;
    QUrl m_zipUrl;
    QUrl m_pageUrl = QUrl(QStringLiteral("https://github.com/maxvanceffer/trun/releases"));
    QString m_error;
    double m_progress = 0.0;
    QString m_downloadPath;
    bool m_cancelled = false;
};
