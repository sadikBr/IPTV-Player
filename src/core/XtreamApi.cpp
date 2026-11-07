#include "XtreamApi.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>
#include <QUrlQuery>

namespace iptv {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("XtreamApi", text);
}

// Xtream servers are inconsistent: the same field can be a number, a string,
// null or missing.
QString str(const QJsonValue &v)
{
    switch (v.type()) {
    case QJsonValue::String:
        return v.toString().trimmed();
    case QJsonValue::Double: {
        const double d = v.toDouble();
        return d == qint64(d) ? QString::number(qint64(d)) : QString::number(d);
    }
    case QJsonValue::Bool:
        return v.toBool() ? QStringLiteral("1") : QStringLiteral("0");
    default:
        return {};
    }
}

int toInt(const QJsonValue &v)
{
    return str(v).toInt();
}

bool parseJson(const QByteArray &json, QJsonDocument *doc, QString *error)
{
    QJsonParseError parseError;
    *doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || doc->isNull()) {
        if (error) {
            *error = json.trimmed().isEmpty()
                    ? tr("The server returned an empty response.")
                    : tr("The server did not return valid data. Check the server address.");
        }
        return false;
    }
    return true;
}

QString encodePathPart(const QString &part)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(part));
}

} // namespace

QUrl XtreamApi::normalizeServer(const QString &input)
{
    const QString text = input.trimmed();
    if (text.isEmpty())
        return {};

    QUrl url = QUrl::fromUserInput(text);
    if (!url.isValid() || url.host().isEmpty())
        return {};
    if (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))
        return {};

    // Drop "/player_api.php", "/get.php", query and trailing slashes.
    QString path = url.path();
    if (path.endsWith(QLatin1String(".php"), Qt::CaseInsensitive))
        path = path.left(path.lastIndexOf(QLatin1Char('/')));
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);

    url.setPath(path);
    url.setQuery(QString());
    url.setFragment(QString());
    url.setUserInfo(QString());
    return url;
}

bool XtreamApi::parseCredentialsUrl(const QString &input, XtreamCredentials *out)
{
    const QUrl url = QUrl::fromUserInput(input.trimmed());
    const QUrlQuery query(url);
    const QString username = query.queryItemValue(QStringLiteral("username"), QUrl::FullyDecoded);
    const QString password = query.queryItemValue(QStringLiteral("password"), QUrl::FullyDecoded);
    const QUrl server = normalizeServer(input);
    if (username.isEmpty() || password.isEmpty() || !server.isValid())
        return false;
    if (out)
        *out = { server, username, password };
    return true;
}

QUrl XtreamApi::apiUrl(const XtreamCredentials &creds, const QString &action)
{
    QUrl url = creds.server;
    url.setPath(creds.server.path() + QStringLiteral("/player_api.php"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("username"), QString::fromLatin1(QUrl::toPercentEncoding(creds.username)));
    query.addQueryItem(QStringLiteral("password"), QString::fromLatin1(QUrl::toPercentEncoding(creds.password)));
    if (!action.isEmpty())
        query.addQueryItem(QStringLiteral("action"), action);
    url.setQuery(query);
    return url;
}

QUrl XtreamApi::epgUrl(const XtreamCredentials &creds)
{
    QUrl url = apiUrl(creds);
    url.setPath(creds.server.path() + QStringLiteral("/xmltv.php"));
    return url;
}

QUrl XtreamApi::liveStreamUrl(const XtreamCredentials &creds, const QString &streamId,
                              const QString &format)
{
    const QString ext = format.isEmpty() ? QStringLiteral("ts") : format;
    const QString base = creds.server.toString(QUrl::FullyEncoded);
    return QUrl(base + QStringLiteral("/live/") + encodePathPart(creds.username) + QLatin1Char('/')
                        + encodePathPart(creds.password) + QLatin1Char('/')
                        + encodePathPart(streamId) + QLatin1Char('.') + ext,
                QUrl::StrictMode);
}

bool XtreamApi::parseAccount(const QByteArray &json, XtreamAccount *out, QString *error)
{
    QJsonDocument doc;
    if (!parseJson(json, &doc, error))
        return false;

    // Wrong credentials: some servers return [] or {"user_info":{"auth":0}}.
    const QJsonObject user = doc.object().value(QStringLiteral("user_info")).toObject();
    XtreamAccount account;
    account.authenticated = toInt(user.value(QStringLiteral("auth"))) == 1;
    account.status = str(user.value(QStringLiteral("status")));
    account.message = str(user.value(QStringLiteral("message")));
    account.maxConnections = toInt(user.value(QStringLiteral("max_connections")));
    account.activeConnections = toInt(user.value(QStringLiteral("active_cons")));

    const qint64 exp = str(user.value(QStringLiteral("exp_date"))).toLongLong();
    if (exp > 0)
        account.expiresAt = QDateTime::fromSecsSinceEpoch(exp, QTimeZone::utc());

    for (const QJsonValue &f : user.value(QStringLiteral("allowed_output_formats")).toArray())
        account.allowedFormats << str(f);

    if (!account.authenticated) {
        if (error)
            *error = tr("Login failed. Check the username and password.");
        if (out)
            *out = account;
        return false;
    }
    if (out)
        *out = account;
    return true;
}

bool XtreamApi::parseCategories(const QByteArray &json, ContentKind kind,
                                QList<CategoryData> *out, QString *error)
{
    QJsonDocument doc;
    if (!parseJson(json, &doc, error))
        return false;
    if (!doc.isArray()) {
        if (error)
            *error = tr("Unexpected category list from the server.");
        return false;
    }

    QList<CategoryData> result;
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        CategoryData c;
        c.extId = str(o.value(QStringLiteral("category_id")));
        c.name = str(o.value(QStringLiteral("category_name")));
        c.kind = kind;
        if (c.extId.isEmpty())
            continue;
        if (c.name.isEmpty())
            c.name = tr("Category %1").arg(c.extId);
        result.append(c);
    }
    if (out)
        *out = std::move(result);
    return true;
}

bool XtreamApi::parseLiveStreams(const QByteArray &json, QList<ChannelData> *out, QString *error)
{
    QJsonDocument doc;
    if (!parseJson(json, &doc, error))
        return false;
    if (!doc.isArray()) {
        if (error)
            *error = tr("Unexpected channel list from the server.");
        return false;
    }

    QList<ChannelData> result;
    const QJsonArray array = doc.array();
    result.reserve(array.size());
    int position = 0;
    for (const QJsonValue &v : array) {
        const QJsonObject o = v.toObject();
        ++position;
        ChannelData c;
        c.extId = str(o.value(QStringLiteral("stream_id")));
        if (c.extId.isEmpty())
            continue;
        c.kind = ContentKind::Live;
        c.name = str(o.value(QStringLiteral("name")));
        if (c.name.isEmpty())
            c.name = tr("Channel %1").arg(c.extId);
        c.logo = str(o.value(QStringLiteral("stream_icon")));
        c.epgId = str(o.value(QStringLiteral("epg_channel_id")));
        c.number = toInt(o.value(QStringLiteral("num")));
        if (c.number <= 0)
            c.number = position;

        c.categoryExtId = str(o.value(QStringLiteral("category_id")));
        if (c.categoryExtId.isEmpty()) {
            const QJsonArray ids = o.value(QStringLiteral("category_ids")).toArray();
            if (!ids.isEmpty())
                c.categoryExtId = str(ids.first());
        }
        result.append(std::move(c));
    }
    if (out)
        *out = std::move(result);
    return true;
}

} // namespace iptv
