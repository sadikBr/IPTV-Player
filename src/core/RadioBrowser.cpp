#include "RadioBrowser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

namespace iptv::RadioBrowser {

namespace {

QUrl apiUrl(const QString &server, const QString &path, const QUrlQuery &query = {})
{
    QUrl url;
    url.setScheme(QStringLiteral("https"));
    url.setHost(server);
    url.setPath(path);
    url.setQuery(query);
    return url;
}

QUrlQuery listQuery(int limit)
{
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("hidebroken"), QStringLiteral("true"));
    q.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    return q;
}

} // namespace

QUrl serversUrl()
{
    return QUrl(QStringLiteral("https://all.api.radio-browser.info/json/servers"));
}

QStringList fallbackServers()
{
    return { QStringLiteral("de1.api.radio-browser.info"), QStringLiteral("nl1.api.radio-browser.info"),
             QStringLiteral("at1.api.radio-browser.info") };
}

QStringList parseServers(const QByteArray &json)
{
    QStringList result;
    for (const QJsonValue &v : QJsonDocument::fromJson(json).array()) {
        const QString name = v.toObject().value(QLatin1String("name")).toString();
        if (name.endsWith(QLatin1String(".api.radio-browser.info")) && !result.contains(name))
            result << name;
    }
    return result;
}

QUrl topVotedUrl(const QString &server, int limit)
{
    return apiUrl(server, QStringLiteral("/json/stations/topvote"), listQuery(limit));
}

QUrl trendingUrl(const QString &server, int limit)
{
    return apiUrl(server, QStringLiteral("/json/stations/topclick"), listQuery(limit));
}

QUrl searchUrl(const QString &server, const QString &name, const QString &countryCode,
               const QString &tag, int limit)
{
    QUrlQuery q = listQuery(limit);
    if (!name.trimmed().isEmpty())
        q.addQueryItem(QStringLiteral("name"), name.trimmed());
    if (!countryCode.isEmpty())
        q.addQueryItem(QStringLiteral("countrycode"), countryCode);
    if (!tag.isEmpty()) {
        q.addQueryItem(QStringLiteral("tag"), tag);
        q.addQueryItem(QStringLiteral("tagExact"), QStringLiteral("true"));
    }
    q.addQueryItem(QStringLiteral("order"), QStringLiteral("votes"));
    q.addQueryItem(QStringLiteral("reverse"), QStringLiteral("true"));
    return apiUrl(server, QStringLiteral("/json/stations/search"), q);
}

QUrl clickUrl(const QString &server, const QString &uuid)
{
    return apiUrl(server, QStringLiteral("/json/url/") + uuid);
}

QList<RadioStation> parseStations(const QByteArray &json)
{
    QList<RadioStation> result;
    for (const QJsonValue &v : QJsonDocument::fromJson(json).array()) {
        const QJsonObject o = v.toObject();
        RadioStation s;
        s.uuid = o.value(QLatin1String("stationuuid")).toString();
        s.name = o.value(QLatin1String("name")).toString().trimmed();
        s.url = o.value(QLatin1String("url_resolved")).toString();
        if (s.url.isEmpty())
            s.url = o.value(QLatin1String("url")).toString();
        if (s.uuid.isEmpty() || s.name.isEmpty() || s.url.isEmpty())
            continue;
        s.favicon = o.value(QLatin1String("favicon")).toString();
        for (const QString &tag : o.value(QLatin1String("tags")).toString().split(QLatin1Char(','))) {
            const QString t = tag.trimmed();
            if (!t.isEmpty())
                s.tags << t;
        }
        s.country = o.value(QLatin1String("country")).toString();
        s.countryCode = o.value(QLatin1String("countrycode")).toString();
        s.codec = o.value(QLatin1String("codec")).toString();
        s.bitrate = o.value(QLatin1String("bitrate")).toInt();
        s.votes = o.value(QLatin1String("votes")).toInt();
        result.append(s);
    }
    return result;
}

} // namespace iptv::RadioBrowser
