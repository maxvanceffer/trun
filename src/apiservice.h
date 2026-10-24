#pragma once

#include <QMutex>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class QNetworkReply;
class Settings;

// API client storage + networking slice: saved requests + environments +
// active environment id, the request-page storage surface (R2: drafts, Save,
// send-status stamping, history writer) and the Send slice (R6): a real HTTP
// call run off the GUI thread (R1 design), with secret resolution on the GUI
// thread so Keychain values never travel through QML.
// Loading is lazy: nothing is read until the first ensureLoaded() call from
// the API page, so the service costs nothing while the page stays closed.
class ApiService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList requests READ requests NOTIFY requestsChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QVariantList environments READ environments NOTIFY environmentsChanged)
    Q_PROPERTY(QString activeEnvId READ activeEnvId NOTIFY activeEnvIdChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(bool sending READ sending NOTIFY sendingChanged)

public:
    explicit ApiService(Settings *settings, QObject *parent = nullptr);

    QVariantList requests() const;
    // Send history ("api/history", cap 50). Read/write: the reader + clear +
    // the request-page writer appendHistory() all live here.
    QVariantList history() const;
    QVariantList environments() const;
    QString activeEnvId() const;
    bool loaded() const { return m_loaded; }
    bool sending() const { return m_sending; }

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

    // ─── Request page (R2: storage slice) ─────────────────────────────────
    // A fresh, unsaved request ("Untitled request", GET, {{baseUrl}}/ — or the
    // pasted url for a running-server "Open →"). id stays "" until the first
    // saveRequest(); nothing is persisted here.
    Q_INVOKABLE QVariantMap makeDraft(const QString &url = QString()) const;
    // Create (empty id → assigned here, appended) or update (existing id →
    // replaced in place, position kept, no duplicate) a request, bump
    // updatedAt, persist. Returns the id ("" plus errorMessage() for an
    // incomplete map or an unknown non-empty id) — the request page binds its
    // working map to the persisted row on first save.
    Q_INVOKABLE QString saveRequest(const QVariantMap &request);
    // The list row's status/time after a send: stamps lastStatus/lastAt and
    // updatedAt on the stored request. Drafts (id "") and unknown ids are
    // no-ops (false) — they only write history, never requests.
    Q_INVOKABLE bool updateRequestStatus(const QString &id, int status);
    // Prepend an entry {method, url (resolved), status} to history, stamp
    // `at` when absent, trim to cap 50 (oldest at the end), persist.
    Q_INVOKABLE void appendHistory(const QVariantMap &entry);
    // Save-dialog name suggestion "METHOD /path" (the design's save()).
    Q_INVOKABLE QString suggestName(const QString &method, const QString &url) const;

    // Parses a pasted curl command into a request: -X/--request, -H/--header,
    // -d/--data, -u/--user, --url plus the boolean flags people paste along.
    // {{var}} placeholders are kept verbatim — resolution happens at send time.
    Q_INVOKABLE QString importCurl(const QString &command);

    // ─── Request page (R6: network slice) ─────────────────────────────────
    // Resolves the spec on the GUI thread ({{var}} in the URL from the active
    // environment — secret values from Keychain, never from QML — enabled
    // params as a query, headers, Content-Type, Authorization), snapshots it
    // and runs the call in a throwaway QThread (local QNAM + QEventLoop).
    // Returns false while one send is in flight (one Send at a time).
    Q_INVOKABLE bool send(const QVariantMap &spec);
    // Aborts the in-flight reply via a queued abort(); the worker still
    // delivers its sendFinished, with canceled = true.
    Q_INVOKABLE void cancel();
    // Sends text to the system clipboard (the response Copy buttons; QML has
    // no clipboard of its own).
    Q_INVOKABLE bool copyToClipboard(const QString &text);

signals:
    void requestsChanged();
    void historyChanged();
    void environmentsChanged();
    void activeEnvIdChanged();
    void errorMessage(const QString &message);
    void loadedChanged();
    void sendingChanged();
    // Result of one Send: { ok, canceled, status, reason, headers:[{name,
    // value}], body, size, ms, error, errorCode, host }. ok = a reply came
    // back with a status (4xx/5xx are a response, not an error — Qt maps
    // them onto NetworkError, and the classification keys off the status);
    // only a transport failure reports error.
    void sendFinished(const QVariantMap &result);

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
    QString keychainGet(const QString &envId, const QString &varName) const;
    static int indexOf(const QVariantList &envs, const QString &id);
    // GUI-thread resolution of a send spec into a concrete HTTP request.
    QVariantMap resolveRequest(const QVariantMap &spec) const;
    // GUI-thread completion of a worker run.
    void finishSend(const QVariantMap &result);

    Settings *m_settings = nullptr;
    QVariantList m_requests;
    QVariantList m_history;
    QVariantList m_envs;
    QString m_activeEnvId;
    bool m_loaded = false;

    // Send slice. m_reply/m_cancelRequested cross the worker boundary, so
    // they are guarded by a mutex (the worker publishes the reply before it
    // blocks in the event loop and clears it before deleting).
    bool m_sending = false;
    QNetworkReply *m_reply = nullptr;
    bool m_cancelRequested = false;
    QMutex m_sendMutex;
};
