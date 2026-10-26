#include "apiservice.h"
#include "settings.h"

#include <QClipboard>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslSocket>
#include <QStringList>
#include <QThread>
#include <QUrl>
#include <QUuid>

#ifdef Q_OS_MAC
#include "mackeychain.h"
#endif

namespace {
const QString kEnvsKey = QStringLiteral("api/envs");
const QString kActiveKey = QStringLiteral("api/activeEnv");
const QString kRequestsKey = QStringLiteral("api/requests");
const QString kHistoryKey = QStringLiteral("api/history");
const int kHistoryCap = 50;
// R4/R6: the transfer-timeout default from the map.
const int kTransferTimeoutMs = 30000;

// Stored var shape: {name, value, secret, enabled, hasSavedValue}.
// On macOS secret values never reach Settings (value is "").
QVariantMap storedVar(const QString &name, const QString &value, bool secret,
                     bool enabled, bool hasSavedValue)
{
    return QVariantMap{
        {QStringLiteral("name"), name},
        {QStringLiteral("value"), value},
        {QStringLiteral("secret"), secret},
        {QStringLiteral("enabled"), enabled},
        {QStringLiteral("hasSavedValue"), hasSavedValue},
    };
}

QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

// Shell-ish word split: honours single/double quotes and backslash escapes,
// and joins the "\<newline>" continuations multi-line curl commands use.
// "{{baseUrl}}/api" survives untouched — nothing here substitutes variables.
QStringList splitCommand(const QString &command)
{
    QString text = command;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QStringLiteral("\\\n"), QStringLiteral(" "));

    QStringList words;
    QString word;
    bool started = false;
    QChar quote;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (quote.isNull()) {
            if (c.isSpace()) {
                if (started) {
                    words.append(word);
                    word.clear();
                    started = false;
                }
            } else if (c == u'\'' || c == u'"') {
                quote = c;
                started = true; // "" still yields an (empty) word
            } else if (c == u'\\' && i + 1 < text.size()) {
                word.append(text.at(++i));
                started = true;
            } else {
                word.append(c);
                started = true;
            }
            continue;
        }
        if (c == quote) {
            quote = QChar();
        } else if (quote == u'"' && c == u'\\' && i + 1 < text.size()
                   && (text.at(i + 1) == u'"' || text.at(i + 1) == u'\\')) {
            word.append(text.at(++i));
        } else {
            word.append(c);
        }
    }
    if (started)
        words.append(word);
    return words;
}

// Curl flags carrying a value. Booleans are the complement and get dropped.
const QSet<QString> kLongValueFlags = {
    QStringLiteral("url"),           QStringLiteral("request"),
    QStringLiteral("header"),        QStringLiteral("data"),
    QStringLiteral("data-raw"),      QStringLiteral("data-binary"),
    QStringLiteral("data-ascii"),    QStringLiteral("data-urlencode"),
    QStringLiteral("user"),          QStringLiteral("user-agent"),
    QStringLiteral("referer"),       QStringLiteral("cookie"),
    QStringLiteral("output"),        QStringLiteral("form"),
    QStringLiteral("connect-timeout"), QStringLiteral("max-time"),
    QStringLiteral("retry"),         QStringLiteral("proxy"),
    QStringLiteral("cacert"),        QStringLiteral("cert"),
    QStringLiteral("key"),           QStringLiteral("write-out"),
    QStringLiteral("upload-file"),
};
const QSet<QString> kLongBoolFlags = {
    QStringLiteral("compressed"), QStringLiteral("silent"),
    QStringLiteral("show-error"), QStringLiteral("location"),
    QStringLiteral("include"),    QStringLiteral("verbose"),
    QStringLiteral("netrc"),      QStringLiteral("fail"),
    QStringLiteral("no-buffer"),  QStringLiteral("ftp-pasv"),
    QStringLiteral("globoff"),
};
// Short flags that take a value when written together: -XPOST, -H"K: V".
const QString kShortValueFlags = QStringLiteral("XHduoAbFewT");

// One flag/value pair folded into the request. Unknown flags are dropped by
// the caller — nothing here invents fields the request page cannot show yet.
void applyCurlValue(const QString &flag, const QString &value, QString &method,
                    QString &url, QString &body, QString &user,
                    QVariantList &headers, bool &hasBody)
{
    if (flag == QLatin1String("X") || flag == QLatin1String("request")) {
        method = value.trimmed().toUpper();
        return;
    }
    if (flag == QLatin1String("H") || flag == QLatin1String("header")) {
        const QString text = value.trimmed();
        const int colon = text.indexOf(u':');
        if (colon > 0) {
            const QString name = text.left(colon).trimmed();
            if (!name.isEmpty())
                headers.append(QVariantMap{
                    {QStringLiteral("name"), name},
                    {QStringLiteral("value"), text.mid(colon + 1).trimmed()},
                    {QStringLiteral("enabled"), true},
                });
        }
        return;
    }
    if (flag == QLatin1String("d") || flag.startsWith(QLatin1String("data"))) {
        // curl concatenates repeated --data with '&'.
        if (hasBody)
            body += u'&';
        body += value;
        hasBody = true;
        return;
    }
    if (flag == QLatin1String("u") || flag == QLatin1String("user")) {
        user = value;
        return;
    }
    if (flag == QLatin1String("url")) {
        if (url.isEmpty())
            url = value;
        return;
    }
}

// Save-dialog suggestion from the design: "GET /api/campaigns".
QString suggestRequestName(const QString &method, const QString &url)
{
    QString path = url;
    const int scheme = path.indexOf(QStringLiteral("://"));
    if (scheme >= 0) {
        path = path.mid(scheme + 3);
        const int slash = path.indexOf(u'/');
        path = slash >= 0 ? path.mid(slash) : QString();
    } else if (path.startsWith(u'{')) {
        // "{{baseUrl}}/api/campaigns" -> "/api/campaigns": the variable is
        // part of the origin, not of the path we show.
        const int close = path.indexOf(QStringLiteral("}}"));
        if (close >= 0)
            path = path.mid(close + 2);
    }
    if (path.isEmpty())
        path = QStringLiteral("/");
    return method + u' ' + path;
}
} // namespace

ApiService::ApiService(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
}

QVariantList ApiService::requests() const
{
    return m_requests;
}

QVariantList ApiService::history() const
{
    return m_history;
}

QVariantList ApiService::environments() const
{
    return m_envs;
}

QString ApiService::activeEnvId() const
{
    return m_activeEnvId;
}

void ApiService::ensureLoaded()
{
    if (m_loaded)
        return;
    m_loaded = true;
    load();
    loadRequests();
    loadHistory();
    emit loadedChanged();
    emit requestsChanged();
    emit historyChanged();
    emit environmentsChanged();
    emit activeEnvIdChanged();
}

void ApiService::load()
{
    m_envs.clear();
    m_activeEnvId.clear();
    if (!m_settings)
        return;

    if (!m_settings->has(kEnvsKey)) {
        // First run seed (matches the design mockup).
        m_envs = QVariantList{
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("local")},
                {QStringLiteral("name"), QStringLiteral("local")},
                {QStringLiteral("vars"),
                 QVariantList{
                     storedVar(QStringLiteral("baseUrl"),
                               QStringLiteral("http://localhost:8000"),
                               false, true, false),
                 }},
            },
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("staging")},
                {QStringLiteral("name"), QStringLiteral("staging")},
                {QStringLiteral("vars"), QVariantList{}},
            },
        };
        m_activeEnvId = QStringLiteral("local");
        persist();
        return;
    }

    const QByteArray raw = m_settings->get(kEnvsKey).toString().toUtf8();
    const QJsonArray arr = QJsonDocument::fromJson(raw).array();
    for (const QJsonValue &v : arr) {
        const QJsonObject e = v.toObject();
        const QString envId = e.value(QStringLiteral("id")).toString();
        if (envId.isEmpty())
            continue;
        QVariantList vars;
        for (const QJsonValue &w : e.value(QStringLiteral("vars")).toArray()) {
            const QJsonObject o = w.toObject();
            const QString name = o.value(QStringLiteral("name")).toString();
            if (name.isEmpty())
                continue;
            const bool secret = o.value(QStringLiteral("secret")).toBool();
            const bool enabled = o.value(QStringLiteral("enabled")).toBool(true);
#ifdef Q_OS_MAC
            // Secrets live in Keychain; Settings records only their presence
            // (hasSavedValue), so loading never touches Keychain — values are
            // fetched lazily, only when a request actually resolves them.
            const bool hasSaved = secret
                && o.value(QStringLiteral("hasSavedValue")).toBool();
            vars.append(storedVar(name, secret ? QString() : o.value(QStringLiteral("value")).toString(),
                                  secret, enabled, hasSaved));
#else
            // No Keychain on this platform: value stays inline (see T2 spec).
            const QString value = o.value(QStringLiteral("value")).toString();
            vars.append(storedVar(name, value, secret, enabled,
                                  secret && !value.isEmpty()));
#endif
        }
        m_envs.append(QVariantMap{
            {QStringLiteral("id"), envId},
            {QStringLiteral("name"), e.value(QStringLiteral("name")).toString(envId)},
            {QStringLiteral("vars"), vars},
        });
    }
    m_activeEnvId = m_settings->get(kActiveKey, QString()).toString();
    if (!m_envs.isEmpty() && indexOf(m_envs, m_activeEnvId) < 0)
        m_activeEnvId = m_envs.first().toMap().value(QStringLiteral("id")).toString();
}

bool ApiService::saveEnvironments(const QVariantList &envs, const QVariantMap &secrets)
{
    if (!m_settings) {
        emit errorMessage(tr("Settings unavailable"));
        return false;
    }

    // Old secret set, for deleting entries that disappeared.
    QSet<QString> oldSecrets;
    for (const QVariant &e : m_envs) {
        const QVariantMap env = e.toMap();
        const QString envId = env.value(QStringLiteral("id")).toString();
        for (const QVariant &w : env.value(QStringLiteral("vars")).toList()) {
            const QVariantMap var = w.toMap();
            if (var.value(QStringLiteral("secret")).toBool()
                && var.value(QStringLiteral("hasSavedValue")).toBool())
                oldSecrets.insert(secretKey(envId, var.value(QStringLiteral("name")).toString()));
        }
    }

    QVariantList stored;
    QSet<QString> newSecrets;
    for (const QVariant &e : envs) {
        QVariantMap env = e.toMap();
        QString envId = env.value(QStringLiteral("id")).toString();
        if (envId.isEmpty())
            envId = newId();
        const QString name = env.value(QStringLiteral("name"), envId).toString();
        QVariantList vars;
        for (const QVariant &w : env.value(QStringLiteral("vars")).toList()) {
            QVariantMap var = w.toMap();
            const QString varName = var.value(QStringLiteral("name")).toString().trimmed();
            if (varName.isEmpty())
                continue;
            const bool secret = var.value(QStringLiteral("secret")).toBool();
            const bool enabled = var.value(QStringLiteral("enabled"), true).toBool();
            const QString key = secretKey(envId, varName);
            if (secret)
                newSecrets.insert(key);
#ifdef Q_OS_MAC
            if (secret && secrets.contains(key)) {
                const QString v = secrets.value(key).toString();
                if (v.isEmpty())
                    keychainDelete(envId, varName);
                else if (!keychainSet(envId, varName, v)) {
                    emit errorMessage(tr("Could not save secret %1").arg(varName));
                    return false;
                }
            }
            // Absent key = keep: fall back to the stored Keychain presence.
            const bool hasSaved = secret
                && (secrets.contains(key) ? !secrets.value(key).toString().isEmpty()
                                          : keychainHas(envId, varName));
            // Secrets never reach Settings; plain values persist inline.
            const QString inlineValue = secret ? QString()
                : var.value(QStringLiteral("value")).toString();
            vars.append(storedVar(varName, inlineValue, secret, enabled, hasSaved));
#else
            // Linux fallback (T2): secrets stay inline in Settings.
            QString value = var.value(QStringLiteral("value")).toString();
            if (secret && secrets.contains(key))
                value = secrets.value(key).toString();
            vars.append(storedVar(varName, value, secret, enabled,
                                  secret && !value.isEmpty()));
#endif
        }
        stored.append(QVariantMap{
            {QStringLiteral("id"), envId},
            {QStringLiteral("name"), name.isEmpty() ? envId : name},
            {QStringLiteral("vars"), vars},
        });
    }

#ifdef Q_OS_MAC
    for (const QString &key : oldSecrets) {
        if (!newSecrets.contains(key)) {
            const int slash = key.indexOf(u'/');
            keychainDelete(key.left(slash), key.mid(slash + 1));
        }
    }
#else
    Q_UNUSED(oldSecrets);
    Q_UNUSED(newSecrets);
#endif

    m_envs = stored;
    if (indexOf(m_envs, m_activeEnvId) < 0 && !m_envs.isEmpty())
        m_activeEnvId = m_envs.first().toMap().value(QStringLiteral("id")).toString();
    persist();
    emit environmentsChanged();
    emit activeEnvIdChanged();
    return true;
}

bool ApiService::setActiveEnv(const QString &id)
{
    ensureLoaded();
    if (indexOf(m_envs, id) < 0)
        return false;
    if (m_activeEnvId == id)
        return true;
    m_activeEnvId = id;
    persist();
    emit activeEnvIdChanged();
    return true;
}

// ─── Saved requests (T6: model only — the request page is a separate map) ──

void ApiService::loadRequests()
{
    m_requests.clear();
    if (!m_settings || !m_settings->has(kRequestsKey))
        return;
    const QByteArray raw = m_settings->get(kRequestsKey).toString().toUtf8();
    const QJsonArray arr = QJsonDocument::fromJson(raw).array();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("id")).toString().isEmpty())
            continue;
        QVariantMap request = o.toVariantMap();
        if (!request.contains(QStringLiteral("headers")))
            request.insert(QStringLiteral("headers"), QVariantList{});
        if (!request.contains(QStringLiteral("params")))
            request.insert(QStringLiteral("params"), QVariantList{});
        if (!request.contains(QStringLiteral("body")))
            request.insert(QStringLiteral("body"), QString());
        if (!request.contains(QStringLiteral("auth")))
            request.insert(QStringLiteral("auth"),
                           QVariantMap{{QStringLiteral("type"), QStringLiteral("none")}});
        m_requests.append(request);
    }
}

void ApiService::persistRequests()
{
    if (!m_settings)
        return;
    QJsonArray arr;
    for (const QVariant &r : m_requests)
        arr.append(QJsonObject::fromVariantMap(r.toMap()));
    m_settings->set(kRequestsKey,
                    QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

// ─── Send history (read + clear; the writer is the request page) ──────────

void ApiService::loadHistory()
{
    m_history.clear();
    if (!m_settings || !m_settings->has(kHistoryKey))
        return;
    const QByteArray raw = m_settings->get(kHistoryKey).toString().toUtf8();
    const QJsonArray arr = QJsonDocument::fromJson(raw).array();
    for (const QJsonValue &v : arr)
        m_history.append(v.toObject().toVariantMap());
}

void ApiService::persistHistory()
{
    if (!m_settings)
        return;
    QJsonArray arr;
    for (const QVariant &h : m_history)
        arr.append(QJsonObject::fromVariantMap(h.toMap()));
    m_settings->set(kHistoryKey,
                    QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

bool ApiService::clearHistory()
{
    ensureLoaded();
    if (m_history.isEmpty())
        return true;
    m_history.clear();
    persistHistory();
    emit historyChanged();
    return true;
}

// ─── Request page storage (R2) ────────────────────────────────────────────

QString ApiService::saveRequest(const QVariantMap &request)
{
    ensureLoaded();
    if (!m_settings) {
        emit errorMessage(tr("Settings unavailable"));
        return {};
    }
    if (!request.contains(QStringLiteral("method"))
        || !request.contains(QStringLiteral("url"))) {
        emit errorMessage(tr("Cannot save an incomplete request"));
        return {};
    }

    QVariantMap stored = request;
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    stored.insert(QStringLiteral("updatedAt"), now);

    const QString id = stored.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) {
        // Create: assign the id here, append to the collection.
        stored.insert(QStringLiteral("id"), newId());
        m_requests.append(stored);
    } else {
        // Update: replace in place — position kept, no duplicate row.
        const int i = indexOf(m_requests, id);
        if (i < 0) {
            emit errorMessage(tr("Request not found"));
            return {};
        }
        m_requests.replace(i, stored);
    }
    persistRequests();
    emit requestsChanged();
    return stored.value(QStringLiteral("id")).toString();
}

bool ApiService::updateRequestStatus(const QString &id, int status)
{
    ensureLoaded();
    if (id.isEmpty())
        return false;
    const int i = indexOf(m_requests, id);
    if (i < 0)
        return false; // drafts are never stored requests — no error, they just
                      // skip the status stamp and only write history.
    QVariantMap request = m_requests.at(i).toMap();
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    request.insert(QStringLiteral("lastStatus"), status);
    request.insert(QStringLiteral("lastAt"), now);
    request.insert(QStringLiteral("updatedAt"), now);
    m_requests.replace(i, request);
    persistRequests();
    emit requestsChanged();
    return true;
}

void ApiService::appendHistory(const QVariantMap &entry)
{
    ensureLoaded();
    QVariantMap item = entry;
    if (item.value(QStringLiteral("at")).toString().isEmpty())
        item.insert(QStringLiteral("at"),
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    m_history.prepend(item);
    while (m_history.size() > kHistoryCap)
        m_history.removeLast();
    persistHistory();
    emit historyChanged();
}

QString ApiService::suggestName(const QString &method, const QString &url) const
{
    return suggestRequestName(method, url);
}

QVariantMap makeDraftImpl(const QString &url)
{
    // The request page's draft: same shape as a saved request, but id stays
    // empty (unsaved) until the first saveRequest(). url is raw — {{var}} is
    // kept verbatim and resolved at send time.
    return QVariantMap{
        {QStringLiteral("id"), QString()},
        {QStringLiteral("name"), QStringLiteral("Untitled request")},
        {QStringLiteral("method"), QStringLiteral("GET")},
        {QStringLiteral("url"), url.isEmpty() ? QStringLiteral("{{baseUrl}}/")
                                              : url},
        {QStringLiteral("projectId"), QString()},
        {QStringLiteral("headers"), QVariantList{}},
        {QStringLiteral("params"), QVariantList{}},
        {QStringLiteral("body"), QString()},
        {QStringLiteral("auth"),
         QVariantMap{{QStringLiteral("type"), QStringLiteral("none")}}},
        // R8: per-request TLS opt-out (curl -k), off unless the user ticks it.
        {QStringLiteral("insecure"), false},
        {QStringLiteral("lastStatus"), 0},
        {QStringLiteral("lastAt"), QString()},
        {QStringLiteral("updatedAt"),
         QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
    };
}

QVariantMap ApiService::makeDraft(const QString &url) const
{
    return makeDraftImpl(url);
}

QVariantMap ApiService::makeRequest(const QString &name, const QString &method,
                                    const QString &url)
{
    QVariantMap request = makeDraftImpl(url);
    request.insert(QStringLiteral("id"), newId());
    request.insert(QStringLiteral("name"), name);
    request.insert(QStringLiteral("method"), method);
    return request;
}

QString ApiService::addRequest(const QVariantMap &request)
{
    ensureLoaded();
    const QString id = request.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
        return {};
    m_requests.append(request);
    persistRequests();
    emit requestsChanged();
    return id;
}

QString ApiService::importCurl(const QString &command)
{
    const QStringList words = splitCommand(command);
    if (words.isEmpty()) {
        emit errorMessage(tr("Paste a cURL command first"));
        return {};
    }

    QString method;
    QString url;
    QString body;
    QString user;
    QString error;
    QVariantList headers;
    bool hasBody = false;
    bool headMethod = false;
    bool getMethod = false;
    // R8: curl -k / --insecure now has somewhere to go.
    bool insecure = false;

    int i = 0;
    if (words.first() == QLatin1String("curl")
        || words.first().endsWith(QLatin1String("/curl")))
        i = 1;

    for (; i < words.size(); ++i) {
        const QString word = words.at(i);
        if (word == QLatin1String("-")) // stdin marker, never a URL
            continue;

        if (word.startsWith(QLatin1String("--"))) {
            QString name = word.mid(2);
            QString value;
            bool inlineValue = false;
            const int eq = name.indexOf(u'=');
            if (eq >= 0) {
                value = name.mid(eq + 1);
                name = name.left(eq);
                inlineValue = true;
            }
            if (name == QLatin1String("head")) {
                headMethod = true;
                continue;
            }
            if (name == QLatin1String("get")) {
                getMethod = true;
                continue;
            }
            if (name == QLatin1String("insecure")) {
                insecure = true;
                continue;
            }
            if (kLongBoolFlags.contains(name))
                continue;
            if (kLongValueFlags.contains(name)) {
                if (!inlineValue) {
                    if (i + 1 >= words.size()) {
                        error = tr("Missing value for --%1").arg(name);
                        break;
                    }
                    value = words.at(++i);
                }
                applyCurlValue(name, value, method, url, body, user, headers, hasBody);
                continue;
            }
            // Unknown long flag: drop it, and its value when it has one.
            if (!inlineValue && i + 1 < words.size()
                && !words.at(i + 1).startsWith(u'-'))
                ++i;
            continue;
        }

        if (word.size() > 1 && word.startsWith(u'-')) {
            const QChar flag = word.at(1);
            if (flag == u'I') {
                headMethod = true;
                continue;
            }
            if (flag == u'G') {
                getMethod = true;
                continue;
            }
            if (kShortValueFlags.contains(flag)) {
                QString value = word.mid(2);
                if (value.isEmpty()) {
                    if (i + 1 >= words.size()) {
                        error = tr("Missing value for -%1").arg(flag);
                        break;
                    }
                    value = words.at(++i);
                }
                applyCurlValue(QString(flag), value, method, url, body, user,
                               headers, hasBody);
                continue;
            }
            // Boolean short flags, possibly combined (-sSL). -k is the one
            // that carries a request field instead of being dropped.
            if (word.mid(1).contains(u'k'))
                insecure = true;
            continue;
        }

        if (url.isEmpty())
            url = word; // the first bare word is the URL
    }

    if (!error.isEmpty()) {
        emit errorMessage(error);
        return {};
    }
    url = url.trimmed();
    if (url.isEmpty()) {
        emit errorMessage(tr("No URL found in the cURL command"));
        return {};
    }

    QString finalMethod = method;
    if (finalMethod.isEmpty()) {
        if (getMethod)
            finalMethod = QStringLiteral("GET");
        else if (headMethod)
            finalMethod = QStringLiteral("HEAD");
        else
            finalMethod = hasBody ? QStringLiteral("POST") : QStringLiteral("GET");
    }

    QVariantMap auth{{QStringLiteral("type"), QStringLiteral("none")}};
    if (!user.isEmpty()) {
        const int colon = user.indexOf(u':');
        auth = QVariantMap{
            {QStringLiteral("type"), QStringLiteral("basic")},
            {QStringLiteral("user"), colon >= 0 ? user.left(colon) : user},
            {QStringLiteral("password"),
             colon >= 0 ? user.mid(colon + 1) : QString()},
        };
    }

    QVariantMap request =
        makeRequest(suggestRequestName(finalMethod, url), finalMethod, url);
    request.insert(QStringLiteral("headers"), headers);
    request.insert(QStringLiteral("body"), body);
    request.insert(QStringLiteral("auth"), auth);
    // Conditional write: the draft's own `false` stays the single source of
    // the off value.
    if (insecure)
        request.insert(QStringLiteral("insecure"), true);
    return addRequest(request);
}

void ApiService::persist()
{
    if (!m_settings)
        return;
    QJsonArray arr;
    for (const QVariant &e : m_envs) {
        const QVariantMap env = e.toMap();
        QJsonArray vars;
        for (const QVariant &w : env.value(QStringLiteral("vars")).toList()) {
            const QVariantMap var = w.toMap();
            vars.append(QJsonObject{
                {QStringLiteral("name"), var.value(QStringLiteral("name")).toString()},
                {QStringLiteral("value"), var.value(QStringLiteral("value")).toString()},
                {QStringLiteral("secret"), var.value(QStringLiteral("secret")).toBool()},
                {QStringLiteral("enabled"), var.value(QStringLiteral("enabled"), true).toBool()},
                {QStringLiteral("hasSavedValue"),
                 var.value(QStringLiteral("hasSavedValue")).toBool()},
            });
        }
        arr.append(QJsonObject{
            {QStringLiteral("id"), env.value(QStringLiteral("id")).toString()},
            {QStringLiteral("name"), env.value(QStringLiteral("name")).toString()},
            {QStringLiteral("vars"), vars},
        });
    }
    m_settings->set(kEnvsKey,
                    QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    m_settings->set(kActiveKey, m_activeEnvId);
}

QString ApiService::keychainService(const QString &envId)
{
    return QStringLiteral("trun.api.") + envId;
}

QString ApiService::secretKey(const QString &envId, const QString &varName)
{
    return envId + u'/' + varName;
}

bool ApiService::keychainHas(const QString &envId, const QString &varName) const
{
#ifdef Q_OS_MAC
    QString dummy;
    return macKeychainFindPassword(keychainService(envId), varName, dummy);
#else
    Q_UNUSED(envId);
    Q_UNUSED(varName);
    return false;
#endif
}

// The stored secret value (macOS Keychain); "" when absent or off macOS.
// Secret values are read here only — they never reach QML.
QString ApiService::keychainGet(const QString &envId, const QString &varName) const
{
#ifdef Q_OS_MAC
    QString value;
    if (macKeychainFindPassword(keychainService(envId), varName, value))
        return value;
    return QString();
#else
    Q_UNUSED(envId);
    Q_UNUSED(varName);
    return QString();
#endif
}

bool ApiService::keychainSet(const QString &envId, const QString &varName,
                             const QString &value) const
{
#ifdef Q_OS_MAC
    return macKeychainSetPassword(keychainService(envId), varName, value);
#else
    Q_UNUSED(envId);
    Q_UNUSED(varName);
    Q_UNUSED(value);
    return false;
#endif
}

bool ApiService::keychainDelete(const QString &envId, const QString &varName) const
{
#ifdef Q_OS_MAC
    return macKeychainDeletePassword(keychainService(envId), varName);
#else
    Q_UNUSED(envId);
    Q_UNUSED(varName);
    return true;
#endif
}

// Index of the env with the given id, -1 when absent.
int ApiService::indexOf(const QVariantList &envs, const QString &id)
{
    for (int i = 0; i < envs.size(); ++i) {
        if (envs.at(i).toMap().value(QStringLiteral("id")).toString() == id)
            return i;
    }
    return -1;
}

// ─── Send (R6) ───────────────────────────────────────────────────────────

// GUI-thread resolution of a send spec into a concrete HTTP request: {{var}}
// substitution from the active environment in every text field the wire
// carries (URL, enabled param values, enabled header values, Bearer token,
// Basic username/password, body — secret values straight from Keychain, never
// from QML), enabled params appended as a query, Authorization from the auth
// block (an explicit header wins), Content-Type inferred from the body mode,
// and the per-request insecure flag passed through. Names (method, param and
// header names, request name) stay literal — they are identifiers.
// Only the URL line mirrors qml/ApiRequestPage.qml resolvedUrl(); the other
// fields are drafts that resolve here at send time.
QVariantMap ApiService::resolveRequest(const QVariantMap &spec) const
{
    QString url = spec.value(QStringLiteral("url")).toString().trimmed();

    const QString envId = spec.value(QStringLiteral("envId")).toString();
    QVariantList vars;
    const int ei = indexOf(m_envs, envId);
    if (ei >= 0)
        vars = m_envs.at(ei).toMap().value(QStringLiteral("vars")).toList();

    // Substitution: unresolvable keeps the placeholder (name not in the env,
    // unset secret, Keychain miss). Plain literal replacement in
    // variable-list order; resolved lazily per field, so a secret's Keychain
    // entry is read only when the field actually holds its placeholder.
    const auto resolve = [this, &vars, &envId](QString &text) {
        if (text.isEmpty())
            return;
        for (const QVariant &w : vars) {
            const QVariantMap var = w.toMap();
            const QString name =
                var.value(QStringLiteral("name")).toString().trimmed();
            if (name.isEmpty())
                continue;
            const QString placeholder =
                QStringLiteral("{{") + name + QStringLiteral("}}");
            if (!text.contains(placeholder))
                continue;
            QString value;
            if (var.value(QStringLiteral("secret")).toBool()) {
                if (!var.value(QStringLiteral("hasSavedValue")).toBool())
                    continue; // not set — leave {{name}} visible
                value = keychainGet(envId, name);
                if (value.isEmpty())
                    continue; // keychain lost it — keep the placeholder
            } else {
                value = var.value(QStringLiteral("value")).toString();
            }
            text.replace(placeholder, value);
        }
    };
    resolve(url);

    QStringList query;
    const QVariantList params = spec.value(QStringLiteral("params")).toList();
    for (const QVariant &p : params) {
        const QVariantMap pm = p.toMap();
        if (!pm.value(QStringLiteral("enabled"), true).toBool())
            continue;
        const QString name = pm.value(QStringLiteral("name")).toString();
        if (name.isEmpty())
            continue;
        QString value = pm.value(QStringLiteral("value")).toString();
        resolve(value);
        query.append(name + u'=' + value);
    }
    if (!query.isEmpty())
        url += (url.contains(u'?') ? QStringLiteral("&") : QStringLiteral("?"))
               + query.join(u'&');

    QVariantList headers;
    bool hasContentType = false;
    bool hasAuth = false;
    const QVariantList inHeaders = spec.value(QStringLiteral("headers")).toList();
    for (const QVariant &h : inHeaders) {
        const QVariantMap hm = h.toMap();
        if (!hm.value(QStringLiteral("enabled"), true).toBool())
            continue;
        const QString name = hm.value(QStringLiteral("name")).toString().trimmed();
        if (name.isEmpty())
            continue;
        if (name.compare(QStringLiteral("content-type"), Qt::CaseInsensitive) == 0)
            hasContentType = true;
        if (name.compare(QStringLiteral("authorization"), Qt::CaseInsensitive) == 0)
            hasAuth = true;
        QString value = hm.value(QStringLiteral("value")).toString();
        resolve(value);
        headers.append(QVariantMap{
            {QStringLiteral("name"), name},
            {QStringLiteral("value"), value},
        });
    }

    const QVariantMap auth = spec.value(QStringLiteral("auth")).toMap();
    const QString authType = auth.value(QStringLiteral("type")).toString();
    if (!hasAuth && authType == QLatin1String("bearer")) {
        QString token = auth.value(QStringLiteral("token")).toString();
        resolve(token);
        if (!token.isEmpty())
            headers.append(QVariantMap{
                {QStringLiteral("name"), QStringLiteral("Authorization")},
                {QStringLiteral("value"), QStringLiteral("Bearer ") + token},
            });
    } else if (!hasAuth && authType == QLatin1String("basic")) {
        QString user = auth.value(QStringLiteral("username")).toString();
        QString pass = auth.value(QStringLiteral("password")).toString();
        resolve(user);
        resolve(pass);
        const QByteArray pair = (user + u':' + pass).toUtf8().toBase64();
        headers.append(QVariantMap{
            {QStringLiteral("name"), QStringLiteral("Authorization")},
            {QStringLiteral("value"), QStringLiteral("Basic ") + QString::fromLatin1(pair)},
        });
    }

    QString bodyText = spec.value(QStringLiteral("body")).toString();
    resolve(bodyText);
    if (!hasContentType && !bodyText.isEmpty()) {
        const QString mode = spec.value(QStringLiteral("bodyMode")).toString();
        QString contentType;
        if (mode == QLatin1String("json"))
            contentType = QStringLiteral("application/json");
        else if (mode == QLatin1String("form"))
            contentType = QStringLiteral("application/x-www-form-urlencoded");
        if (!contentType.isEmpty())
            headers.append(QVariantMap{
                {QStringLiteral("name"), QStringLiteral("Content-Type")},
                {QStringLiteral("value"), contentType},
            });
    }

    QString method = spec.value(QStringLiteral("method")).toString().trimmed().toUpper();
    if (method.isEmpty())
        method = QStringLiteral("GET");

    QVariantMap out;
    out.insert(QStringLiteral("url"), url);
    out.insert(QStringLiteral("method"), method);
    out.insert(QStringLiteral("headers"), headers);
    out.insert(QStringLiteral("body"), bodyText);
    out.insert(QStringLiteral("host"), QUrl(url).host());
    // R8: per-request TLS opt-out — carried, never inferred.
    out.insert(QStringLiteral("insecure"), spec.value(QStringLiteral("insecure")).toBool());
    return out;
}

bool ApiService::send(const QVariantMap &spec)
{
    ensureLoaded();
    if (m_sending)
        return false; // one send at a time

    const QVariantMap resolved = resolveRequest(spec);
    const QString url = resolved.value(QStringLiteral("url")).toString();
    if (url.isEmpty()) {
        emit errorMessage(tr("Nothing to send — the URL is empty"));
        return false;
    }

    const QString method = resolved.value(QStringLiteral("method")).toString();
    const QVariantList headers = resolved.value(QStringLiteral("headers")).toList();
    const QByteArray body = resolved.value(QStringLiteral("body")).toString().toUtf8();
    const QString host = resolved.value(QStringLiteral("host")).toString();
    // R8: this request opted out of certificate verification (curl -k).
    const bool insecure = resolved.value(QStringLiteral("insecure")).toBool();

    {
        QMutexLocker locker(&m_sendMutex);
        m_reply = nullptr;
        m_cancelRequested = false;
    }
    m_sending = true;
    emit sendingChanged();

    // Throwaway thread + local QNAM + nested event loop (R1 design): the GUI
    // thread never touches the network. The reply is published under the mutex
    // before blocking and cleared before it is deleted, so a queued abort()
    // can never reach a dangling pointer.
    QThread *thread = QThread::create([this, url, method, headers, body, host, insecure]() {
        QNetworkAccessManager nam;
        QNetworkRequest request{QUrl(url)};
        request.setTransferTimeout(kTransferTimeoutMs);

        if (insecure) {
            // curl -k: the handshake stops checking the chain. The manager
            // backstop covers a reply that still reports errors — notably a
            // redirect, where Qt builds a fresh request and drops the
            // per-request SSL configuration.
            QObject::connect(&nam, &QNetworkAccessManager::sslErrors,
                             [](QNetworkReply *r, const QList<QSslError> &) {
                                 r->ignoreSslErrors();
                             });
            QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
            ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
            request.setSslConfiguration(ssl);
        }

        for (const QVariant &h : headers) {
            const QVariantMap hm = h.toMap();
            request.setRawHeader(hm.value(QStringLiteral("name")).toString().toUtf8(),
                                 hm.value(QStringLiteral("value")).toString().toUtf8());
        }

        QElapsedTimer timer;
        timer.start();
        QNetworkReply *reply = nam.sendCustomRequest(request, method.toUtf8(), body);
        {
            QMutexLocker locker(&m_sendMutex);
            m_reply = reply;
        }

        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        const int elapsed = int(timer.elapsed());

        const int errorCode = int(reply->error());
        const QByteArray raw = reply->readAll();

        QVariantList replyHeaders;
        const QList<QNetworkReply::RawHeaderPair> pairs = reply->rawHeaderPairs();
        for (const QNetworkReply::RawHeaderPair &pair : pairs) {
            replyHeaders.append(QVariantMap{
                {QStringLiteral("name"), QString::fromUtf8(pair.first)},
                {QStringLiteral("value"), QString::fromUtf8(pair.second)},
            });
        }

        const QVariant statusAttr =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const int status = statusAttr.isValid() ? statusAttr.toInt() : 0;
        const QString reason =
            reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();

        bool canceled = false;
        {
            QMutexLocker locker(&m_sendMutex);
            canceled = m_cancelRequested;
            m_reply = nullptr;
        }

        // A 4xx/5xx is a response, not an error — and Qt maps HTTP statuses
        // onto NetworkError (401 → AuthenticationRequiredError, 404 →
        // ContentNotFoundError, …), so the classification keys off the
        // status: a reply that carries one is a response whatever errorCode
        // says. Only a transport failure (no status — a refused connection,
        // a deadline the user did not ask for) reaches the error state.
        const bool isResponse = status > 0;
        QString error;
        if (!canceled && !isResponse && errorCode != int(QNetworkReply::NoError)) {
            switch (errorCode) {
            case QNetworkReply::OperationCanceledError:
                error = tr("Request timed out"); // transfer timeout, not a cancel
                break;
            case QNetworkReply::ConnectionRefusedError:
                error = tr("Could not connect to the server");
                break;
            case QNetworkReply::HostNotFoundError:
                error = tr("Host not found");
                break;
            case QNetworkReply::TimeoutError:
                error = tr("Connection timed out");
                break;
            default:
                error = reply->errorString();
                break;
            }
        }

        delete reply;

        QVariantMap result;
        result.insert(QStringLiteral("ok"),
                      !canceled
                          && (isResponse
                              || errorCode == int(QNetworkReply::NoError)));
        result.insert(QStringLiteral("canceled"), canceled);
        result.insert(QStringLiteral("status"), status);
        result.insert(QStringLiteral("reason"), reason);
        result.insert(QStringLiteral("headers"), replyHeaders);
        result.insert(QStringLiteral("body"), QString::fromUtf8(raw));
        result.insert(QStringLiteral("size"), raw.size());
        result.insert(QStringLiteral("ms"), elapsed);
        result.insert(QStringLiteral("error"), error);
        result.insert(QStringLiteral("errorCode"), errorCode);
        result.insert(QStringLiteral("host"), host);

        QMetaObject::invokeMethod(
            this, [this, result]() { finishSend(result); }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
    return true;
}

void ApiService::cancel()
{
    if (!m_sending)
        return;
    QMutexLocker locker(&m_sendMutex);
    m_cancelRequested = true;
    if (m_reply)
        QMetaObject::invokeMethod(m_reply, "abort", Qt::QueuedConnection);
}

void ApiService::finishSend(const QVariantMap &result)
{
    m_sending = false;
    {
        QMutexLocker locker(&m_sendMutex);
        m_reply = nullptr;
        m_cancelRequested = false;
    }
    emit sendingChanged();
    emit sendFinished(result);
}

bool ApiService::copyToClipboard(const QString &text)
{
    if (QGuiApplication::clipboard())
        QGuiApplication::clipboard()->setText(text);
    return true;
}
