#include "XtreamApi.h"

#include "Catchup.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <algorithm>
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

// Image fields are sometimes a string and sometimes a list of strings.
QString firstImage(const QJsonValue &v)
{
    if (v.isArray()) {
        for (const QJsonValue &item : v.toArray()) {
            const QString url = str(item);
            if (!url.isEmpty())
                return url;
        }
        return {};
    }
    return str(v);
}

// "01:45:00" or "105 min" -> seconds
int parseDuration(const QJsonObject &info)
{
    const int secs = str(info.value(QStringLiteral("duration_secs"))).toInt();
    if (secs > 0)
        return secs;
    const QString text = str(info.value(QStringLiteral("duration")));
    const QStringList parts = text.split(QLatin1Char(':'));
    if (parts.size() == 3)
        return parts.at(0).toInt() * 3600 + parts.at(1).toInt() * 60 + parts.at(2).toInt();
    const int minutes = text.section(QLatin1Char(' '), 0, 0).toInt();
    return minutes > 0 ? minutes * 60 : 0;
}

QString yearFrom(const QString &date)
{
    static const QRegularExpression re(QStringLiteral("((?:19|20)\\d{2})"));
    const auto m = re.match(date);
    return m.hasMatch() ? m.captured(1) : QString();
}

QString ratingText(const QJsonValue &v)
{
    const QString r = str(v);
    bool ok = false;
    const double value = r.toDouble(&ok);
    if (!ok || value <= 0)
        return {};
    return QString::number(value, 'f', value == qint64(value) ? 0 : 1);
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

QString XtreamApi::imageUrl(const QString &value, bool backdrop)
{
    const QString v = value.trimmed();
    if (v.isEmpty())
        return {};
    if (v.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)
        || v.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)) {
        // Some providers link to an IMDb web page instead of an image.
        const QUrl url(v);
        if (url.host().endsWith(QLatin1String("imdb.com"), Qt::CaseInsensitive)
            && url.path().contains(QLatin1String("/title/")))
            return {};
        return v;
    }
    if (v.startsWith(QLatin1String("//")))
        return QStringLiteral("https:") + v;
    // A bare image file name: TMDB's image server.
    static const QRegularExpression tmdb(QStringLiteral("^/?[A-Za-z0-9_-]+\\.(jpg|jpeg|png|webp)$"),
                                         QRegularExpression::CaseInsensitiveOption);
    if (tmdb.match(v).hasMatch()) {
        const QString file = v.startsWith(QLatin1Char('/')) ? v : QLatin1Char('/') + v;
        return QStringLiteral("https://image.tmdb.org/t/p/") + (backdrop ? QStringLiteral("w1280")
                                                                         : QStringLiteral("w500"))
                + file;
    }
    return {};
}

QUrl XtreamApi::movieUrl(const XtreamCredentials &creds, const QString &streamId,
                         const QString &extension)
{
    const QString ext = extension.isEmpty() ? QStringLiteral("mp4") : extension;
    return QUrl(creds.server.toString(QUrl::FullyEncoded) + QStringLiteral("/movie/")
                        + encodePathPart(creds.username) + QLatin1Char('/')
                        + encodePathPart(creds.password) + QLatin1Char('/')
                        + encodePathPart(streamId) + QLatin1Char('.') + ext,
                QUrl::StrictMode);
}

QUrl XtreamApi::episodeUrl(const XtreamCredentials &creds, const QString &episodeId,
                           const QString &extension)
{
    const QString ext = extension.isEmpty() ? QStringLiteral("mp4") : extension;
    return QUrl(creds.server.toString(QUrl::FullyEncoded) + QStringLiteral("/series/")
                        + encodePathPart(creds.username) + QLatin1Char('/')
                        + encodePathPart(creds.password) + QLatin1Char('/')
                        + encodePathPart(episodeId) + QLatin1Char('.') + ext,
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

    const QJsonObject server = doc.object().value(QStringLiteral("server_info")).toObject();
    account.serverTimezone = QString::fromUtf8(
            Catchup::serverZone(str(server.value(QStringLiteral("timezone"))),
                                str(server.value(QStringLiteral("timestamp_now"))).toLongLong(),
                                str(server.value(QStringLiteral("time_now"))))
                    .id());

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
        c.logo = imageUrl(str(o.value(QStringLiteral("stream_icon"))));
        c.epgId = str(o.value(QStringLiteral("epg_channel_id")));
        c.number = toInt(o.value(QStringLiteral("num")));
        if (c.number <= 0)
            c.number = position;
        // Catch-up: "tv_archive": 1 and the number of days kept
        if (toInt(o.value(QStringLiteral("tv_archive"))) == 1) {
            c.archiveDays = toInt(o.value(QStringLiteral("tv_archive_duration")));
            if (c.archiveDays <= 0)
                c.archiveDays = 1;
        }

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

namespace iptv {

namespace {

bool parseArray(const QByteArray &json, QJsonArray *out, QString *error, const char *what)
{
    QJsonDocument doc;
    if (!parseJson(json, &doc, error))
        return false;
    if (doc.isObject() && doc.object().isEmpty()) {
        *out = {}; // some servers return {} instead of [] when empty
        return true;
    }
    if (!doc.isArray()) {
        if (error)
            *error = tr(what);
        return false;
    }
    *out = doc.array();
    return true;
}

QString categoryOf(const QJsonObject &o)
{
    QString id = str(o.value(QStringLiteral("category_id")));
    if (id.isEmpty()) {
        const QJsonArray ids = o.value(QStringLiteral("category_ids")).toArray();
        if (!ids.isEmpty())
            id = str(ids.first());
    }
    return id;
}

} // namespace

bool XtreamApi::parseMovies(const QByteArray &json, QList<ChannelData> *out, QString *error)
{
    QJsonArray array;
    if (!parseArray(json, &array, error, "Unexpected movie list from the server."))
        return false;
    QList<ChannelData> result;
    result.reserve(array.size());
    int position = 0;
    for (const QJsonValue &v : std::as_const(array)) {
        const QJsonObject o = v.toObject();
        ++position;
        ChannelData c;
        c.kind = ContentKind::Movie;
        c.extId = str(o.value(QStringLiteral("stream_id")));
        if (c.extId.isEmpty())
            continue;
        c.name = str(o.value(QStringLiteral("name")));
        if (c.name.isEmpty())
            c.name = tr("Movie %1").arg(c.extId);
        c.logo = imageUrl(str(o.value(QStringLiteral("stream_icon"))));
        c.extension = str(o.value(QStringLiteral("container_extension")));
        c.rating = ratingText(o.value(QStringLiteral("rating")));
        c.year = str(o.value(QStringLiteral("year")));
        if (c.year.isEmpty())
            c.year = yearFrom(str(o.value(QStringLiteral("releaseDate"))));
        if (c.year.isEmpty())
            c.year = yearFrom(c.name);
        c.plot = str(o.value(QStringLiteral("plot")));
        c.genre = str(o.value(QStringLiteral("genre")));
        c.number = toInt(o.value(QStringLiteral("num")));
        if (c.number <= 0)
            c.number = position;
        c.categoryExtId = categoryOf(o);
        result.append(std::move(c));
    }
    if (out)
        *out = std::move(result);
    return true;
}

bool XtreamApi::parseSeries(const QByteArray &json, QList<ChannelData> *out, QString *error)
{
    QJsonArray array;
    if (!parseArray(json, &array, error, "Unexpected series list from the server."))
        return false;
    QList<ChannelData> result;
    result.reserve(array.size());
    int position = 0;
    for (const QJsonValue &v : std::as_const(array)) {
        const QJsonObject o = v.toObject();
        ++position;
        ChannelData c;
        c.kind = ContentKind::Series;
        c.extId = str(o.value(QStringLiteral("series_id")));
        if (c.extId.isEmpty())
            continue;
        c.name = str(o.value(QStringLiteral("name")));
        if (c.name.isEmpty())
            c.name = tr("Series %1").arg(c.extId);
        c.logo = imageUrl(str(o.value(QStringLiteral("cover"))));
        c.rating = ratingText(o.value(QStringLiteral("rating")));
        QString date = str(o.value(QStringLiteral("releaseDate")));
        if (date.isEmpty())
            date = str(o.value(QStringLiteral("release_date")));
        c.year = yearFrom(date);
        c.plot = str(o.value(QStringLiteral("plot")));
        c.genre = str(o.value(QStringLiteral("genre")));
        c.number = toInt(o.value(QStringLiteral("num")));
        if (c.number <= 0)
            c.number = position;
        c.categoryExtId = categoryOf(o);
        result.append(std::move(c));
    }
    if (out)
        *out = std::move(result);
    return true;
}

namespace {

VodDetails detailsFrom(const QJsonObject &info)
{
    VodDetails d;
    d.plot = str(info.value(QStringLiteral("plot")));
    if (d.plot.isEmpty())
        d.plot = str(info.value(QStringLiteral("description")));
    d.cast = str(info.value(QStringLiteral("cast")));
    if (d.cast.isEmpty())
        d.cast = str(info.value(QStringLiteral("actors")));
    d.director = str(info.value(QStringLiteral("director")));
    d.genre = str(info.value(QStringLiteral("genre")));
    d.releaseDate = str(info.value(QStringLiteral("releasedate")));
    if (d.releaseDate.isEmpty())
        d.releaseDate = str(info.value(QStringLiteral("releaseDate")));
    if (d.releaseDate.isEmpty())
        d.releaseDate = str(info.value(QStringLiteral("release_date")));
    d.rating = ratingText(info.value(QStringLiteral("rating")));
    d.poster = str(info.value(QStringLiteral("movie_image")));
    if (d.poster.isEmpty())
        d.poster = str(info.value(QStringLiteral("cover")));
    if (d.poster.isEmpty())
        d.poster = str(info.value(QStringLiteral("cover_big")));
    d.poster = XtreamApi::imageUrl(d.poster);
    d.backdrop = XtreamApi::imageUrl(firstImage(info.value(QStringLiteral("backdrop_path"))), true);
    d.durationSecs = parseDuration(info);
    return d;
}

} // namespace

bool XtreamApi::parseMovieInfo(const QByteArray &json, VodDetails *out, QString *error)
{
    QJsonDocument doc;
    if (!parseJson(json, &doc, error))
        return false;
    const QJsonObject root = doc.object();
    VodDetails d = detailsFrom(root.value(QStringLiteral("info")).toObject());
    d.extension = str(root.value(QStringLiteral("movie_data")).toObject()
                              .value(QStringLiteral("container_extension")));
    if (out)
        *out = d;
    return true;
}

bool XtreamApi::parseSeriesInfo(const QByteArray &json, const QString &seriesId,
                                VodDetails *details, QList<EpisodeData> *episodes, QString *error)
{
    QJsonDocument doc;
    if (!parseJson(json, &doc, error))
        return false;
    const QJsonObject root = doc.object();
    if (details)
        *details = detailsFrom(root.value(QStringLiteral("info")).toObject());

    // "episodes" is {"1": [...], "2": [...]} on most servers, [[...], [...]] on some.
    QList<QJsonArray> seasons;
    const QJsonValue eps = root.value(QStringLiteral("episodes"));
    if (eps.isObject()) {
        const QJsonObject obj = eps.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            seasons.append(it.value().toArray());
    } else if (eps.isArray()) {
        for (const QJsonValue &season : eps.toArray())
            seasons.append(season.toArray());
    }

    QList<EpisodeData> result;
    for (const QJsonArray &season : std::as_const(seasons)) {
        for (const QJsonValue &v : season) {
            const QJsonObject o = v.toObject();
            EpisodeData e;
            e.seriesExtId = seriesId;
            e.extId = str(o.value(QStringLiteral("id")));
            if (e.extId.isEmpty())
                continue;
            e.season = qMax(1, toInt(o.value(QStringLiteral("season"))));
            e.episode = toInt(o.value(QStringLiteral("episode_num")));
            e.title = str(o.value(QStringLiteral("title")));
            e.extension = str(o.value(QStringLiteral("container_extension")));
            const QJsonObject info = o.value(QStringLiteral("info")).toObject();
            e.plot = str(info.value(QStringLiteral("plot")));
            e.image = imageUrl(str(info.value(QStringLiteral("movie_image"))), true);
            e.durationSecs = parseDuration(info);
            result.append(std::move(e));
        }
    }
    std::stable_sort(result.begin(), result.end(), [](const EpisodeData &a, const EpisodeData &b) {
        return a.season != b.season ? a.season < b.season : a.episode < b.episode;
    });
    if (episodes)
        *episodes = std::move(result);
    return true;
}

} // namespace iptv
