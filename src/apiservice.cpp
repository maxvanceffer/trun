#include "apiservice.h"
#include "settings.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <QUuid>

#ifdef Q_OS_MAC
#include "mackeychain.h"
#endif

namespace {
const QString kEnvsKey = QStringLiteral("api/envs");
const QString kActiveKey = QStringLiteral("api/activeEnv");
const QString kRequestsKey = QStringLiteral("api/requests");
const QString kHistoryKey = QStringLiteral("api/history");

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
    QStringLiteral("insecure"),   QStringLiteral("include"),
    QStringLiteral("verbose"),    QStringLiteral("netrc"),
    QStringLiteral("fail"),       QStringLiteral("no-buffer"),
    QStringLiteral("ftp-pasv"),   QStringLiteral("globoff"),
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
            // Secrets live in Keychain; Settings only records presence.
            const bool hasSaved = secret && keychainHas(envId, name);
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

QVariantMap ApiService::makeRequest(const QString &name, const QString &method,
                                    const QString &url)
{
    return QVariantMap{
        {QStringLiteral("id"), newId()},
        {QStringLiteral("name"), name},
        {QStringLiteral("method"), method},
        {QStringLiteral("url"), url},
        {QStringLiteral("projectId"), QString()},
        {QStringLiteral("headers"), QVariantList{}},
        {QStringLiteral("body"), QString()},
        {QStringLiteral("auth"),
         QVariantMap{{QStringLiteral("type"), QStringLiteral("none")}}},
        {QStringLiteral("lastStatus"), 0},
        {QStringLiteral("lastAt"), QString()},
        {QStringLiteral("updatedAt"),
         QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
    };
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

QString ApiService::newUntitled(const QString &url)
{
    // Design: "+ New request" opens an empty one named "Untitled request";
    // a running-server "Open →" reuses the same draft with that URL pasted in.
    return addRequest(makeRequest(QStringLiteral("Untitled request"),
                                  QStringLiteral("GET"),
                                  url.isEmpty() ? QStringLiteral("{{baseUrl}}/")
                                                : url));
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
            continue; // boolean short flags, possibly combined (-sSL)
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
