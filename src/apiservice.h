#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class Settings;

// API client storage slice: saved requests + environments + active
// environment id.
// Networking (Send) lives on the second-page map; this class never touches
// the network and does small synchronous Settings/Keychain I/O only.
// Loading is lazy: nothing is read until the first ensureLoaded() call from
// the API page, so the service costs nothing while the page stays closed.
class ApiService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList requests READ requests NOTIFY requestsChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QVariantList environments READ environments NOTIFY environmentsChanged)
    Q_PROPERTY(QString activeEnvId READ activeEnvId NOTIFY activeEnvIdChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)

public:
    explicit ApiService(Settings *settings, QObject *parent = nullptr);

    QVariantList requests() const;
    // Send history ("api/history", cap 50). Read-only here: the writer is the
    // request page (out of scope), this map only reads and clears it.
    QVariantList history() const;
    QVariantList environments() const;
    QString activeEnvId() const;
    bool loaded() const { return m_loaded; }

    // Loads (and seeds) on first call. Idempotent.
    Q_INVOKABLE void ensureLoaded();
    // Persists the full env list; secrets arrive separately and are written
    // to Keychain, never to Settings. secrets keys are "<envId>/<varName>":
    // absent = keep existing, non-empty = store, empty = delete.
    // Env/vars missing from envs (deleted or unflagged) lose their secret.
    Q_INVOKABLE bool saveEnvironments(const QVariantList &envs,
                                      const QVariantMap &secrets);
    Q_INVOKABLE bool setActiveEnv(const QString &id);
    Q_INVOKABLE bool clearHistory();

    // T6 — model-only entries: the request page does not exist yet, so both
    // actions append a saved request and return its id ("" plus errorMessage()
    // on failure) with nothing else to open. Empty url = the design's
    // "{{baseUrl}}/" placeholder for a brand-new request.
    Q_INVOKABLE QString newUntitled(const QString &url = QString());
    // Parses a pasted curl command into a request: -X/--request, -H/--header,
    // -d/--data, -u/--user, --url plus the boolean flags people paste along.
    // {{var}} placeholders are kept verbatim — resolution happens at send time.
    Q_INVOKABLE QString importCurl(const QString &command);

signals:
    void requestsChanged();
    void historyChanged();
    void environmentsChanged();
    void activeEnvIdChanged();
    void errorMessage(const QString &message);
    void loadedChanged();

private:
    void load();
    void persist();
    void loadRequests();
    void persistRequests();
    void loadHistory();
    void persistHistory();
    QString addRequest(const QVariantMap &request);
    static QVariantMap makeRequest(const QString &name, const QString &method,
                                   const QString &url);
    static QString keychainService(const QString &envId);
    static QString secretKey(const QString &envId, const QString &varName);
    bool keychainHas(const QString &envId, const QString &varName) const;
    bool keychainSet(const QString &envId, const QString &varName,
                     const QString &value) const;
    bool keychainDelete(const QString &envId, const QString &varName) const;
    static int indexOf(const QVariantList &envs, const QString &id);

    Settings *m_settings = nullptr;
    QVariantList m_requests;
    QVariantList m_history;
    QVariantList m_envs;
    QString m_activeEnvId;
    bool m_loaded = false;
};
