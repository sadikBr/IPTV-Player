#include "VodManager.h"

#include "Database.h"
#include "NetworkFactory.h"
#include "SourceStore.h"
#include "XtreamApi.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

using namespace iptv;

namespace {

constexpr qint64 kDetailsMaxAgeSecs = 7 * 24 * 3600;

QString loadingKey(int sourceId, const QString &extId)
{
    return QString::number(sourceId) + QLatin1Char('|') + extId;
}

ContentKind progressKind(const QString &kind)
{
    return kind == QLatin1String("episode") ? ContentKind::Episode : ContentKind::Movie;
}

} // namespace

VodManager::VodManager(QObject *parent) : QObject(parent), m_nam(new QNetworkAccessManager(this)) { }

QVariantMap VodManager::describe(int sourceId, int kindValue, const QString &extId) const
{
    const auto kind = ContentKind(kindValue);
    SourceStore store(Database::connection());
    const auto item = store.item(sourceId, kind, extId);
    if (!item)
        return {};
    const auto details = store.details(sourceId, kind, extId).value_or(VodDetails{});

    auto pick = [](const QString &a, const QString &b) { return a.isEmpty() ? b : a; };
    QString year = item->year;
    if (year.isEmpty() && details.releaseDate.size() >= 4)
        year = details.releaseDate.left(4);

    return {
        { QStringLiteral("extId"), item->extId },
        { QStringLiteral("name"), item->name },
        { QStringLiteral("poster"), pick(item->logo, details.poster) },
        { QStringLiteral("backdrop"), details.backdrop },
        { QStringLiteral("year"), year },
        { QStringLiteral("rating"), pick(details.rating, item->rating) },
        { QStringLiteral("genre"), pick(details.genre, item->genre) },
        { QStringLiteral("plot"), pick(details.plot, item->plot) },
        { QStringLiteral("cast"), details.cast },
        { QStringLiteral("director"), details.director },
        { QStringLiteral("releaseDate"), details.releaseDate },
        { QStringLiteral("durationSecs"), details.durationSecs },
        { QStringLiteral("favorite"), store.isFavorite(sourceId, kind, extId) },
    };
}

QVariantMap VodManager::movie(int sourceId, const QString &extId) const
{
    return describe(sourceId, int(ContentKind::Movie), extId);
}

QVariantMap VodManager::series(int sourceId, const QString &extId) const
{
    return describe(sourceId, int(ContentKind::Series), extId);
}

bool VodManager::isLoading(int sourceId, const QString &extId) const
{
    return m_loading.contains(loadingKey(sourceId, extId));
}

void VodManager::fetchMovie(int sourceId, const QString &extId)
{
    SourceStore store(Database::connection());
    const auto source = store.source(sourceId);
    QDateTime fetched;
    const bool cached = store.details(sourceId, ContentKind::Movie, extId, &fetched).has_value()
            && fetched.secsTo(QDateTime::currentDateTime()) < kDetailsMaxAgeSecs;
    const QString key = loadingKey(sourceId, extId);
    if (!source || !source->isXtream() || cached || m_loading.contains(key)) {
        emit detailsLoaded(sourceId, extId, true, {});
        return;
    }

    const XtreamCredentials creds{ QUrl(source->url), source->username, source->password };
    QUrl url = XtreamApi::apiUrl(creds, QStringLiteral("get_vod_info"));
    QUrlQuery query(url);
    query.addQueryItem(QStringLiteral("vod_id"), extId);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, NetworkFactory::userAgent());
    request.setTransferTimeout(30000);
    QNetworkReply *reply = m_nam->get(request);
    m_loading.insert(key);
    emit loadingChanged();

    connect(reply, &QNetworkReply::finished, this, [this, reply, sourceId, extId, key] {
        reply->deleteLater();
        m_loading.remove(key);
        emit loadingChanged();
        if (reply->error() != QNetworkReply::NoError) {
            emit detailsLoaded(sourceId, extId, false, reply->errorString());
            return;
        }
        VodDetails details;
        QString error;
        if (!XtreamApi::parseMovieInfo(reply->readAll(), &details, &error)) {
            emit detailsLoaded(sourceId, extId, false, error);
            return;
        }
        SourceStore(Database::connection()).saveDetails(sourceId, ContentKind::Movie, extId, details);
        emit DataNotifier::instance()->detailsChanged(sourceId, extId);
        emit detailsLoaded(sourceId, extId, true, {});
    });
}

void VodManager::fetchSeries(int sourceId, const QString &extId, bool force)
{
    SourceStore store(Database::connection());
    const auto source = store.source(sourceId);
    QDateTime fetched;
    const bool cached = store.details(sourceId, ContentKind::Series, extId, &fetched).has_value()
            && fetched.secsTo(QDateTime::currentDateTime()) < 24 * 3600
            && !store.episodes(sourceId, extId).isEmpty();
    const QString key = loadingKey(sourceId, extId);
    // M3U series bring their episodes with the playlist.
    if (!source || !source->isXtream() || (cached && !force) || m_loading.contains(key)) {
        if (!m_loading.contains(key))
            emit seriesLoaded(sourceId, extId, true, {});
        return;
    }

    const XtreamCredentials creds{ QUrl(source->url), source->username, source->password };
    QUrl url = XtreamApi::apiUrl(creds, QStringLiteral("get_series_info"));
    QUrlQuery query(url);
    query.addQueryItem(QStringLiteral("series_id"), extId);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, NetworkFactory::userAgent());
    request.setTransferTimeout(30000);
    QNetworkReply *reply = m_nam->get(request);
    m_loading.insert(key);
    emit loadingChanged();

    connect(reply, &QNetworkReply::finished, this, [this, reply, sourceId, extId, key] {
        reply->deleteLater();
        m_loading.remove(key);
        emit loadingChanged();
        if (reply->error() != QNetworkReply::NoError) {
            emit seriesLoaded(sourceId, extId, false, reply->errorString());
            return;
        }
        VodDetails details;
        QList<EpisodeData> episodes;
        QString error;
        if (!XtreamApi::parseSeriesInfo(reply->readAll(), extId, &details, &episodes, &error)) {
            emit seriesLoaded(sourceId, extId, false, error);
            return;
        }
        SourceStore store(Database::connection());
        store.saveDetails(sourceId, ContentKind::Series, extId, details);
        store.replaceEpisodes(sourceId, extId, episodes);
        emit DataNotifier::instance()->detailsChanged(sourceId, extId);
        emit DataNotifier::instance()->episodesChanged(sourceId, extId);
        emit seriesLoaded(sourceId, extId, true, {});
    });
}

QUrl VodManager::movieUrl(int sourceId, const QString &extId) const
{
    SourceStore store(Database::connection());
    const auto source = store.source(sourceId);
    const auto item = store.item(sourceId, ContentKind::Movie, extId);
    if (!source || !item)
        return {};
    QString extension = item->extension;
    if (const auto details = store.details(sourceId, ContentKind::Movie, extId);
        details && !details->extension.isEmpty())
        extension = details->extension;
    return SourceStore::streamUrl(*source, ContentKind::Movie, extId, item->url, extension);
}

double VodManager::resumePosition(int sourceId, const QString &kind, const QString &extId) const
{
    const auto p = SourceStore(Database::connection()).progress(sourceId, progressKind(kind), extId);
    if (!p || p->finished() || p->positionMs < 30000)
        return 0;
    return double(p->positionMs);
}

void VodManager::saveProgress(int sourceId, const QString &kind, const QString &extId,
                              double positionMs, double durationMs, const QVariantMap &info)
{
    if (sourceId <= 0 || extId.isEmpty() || durationMs <= 0)
        return;
    WatchProgress p;
    p.sourceId = sourceId;
    p.kind = progressKind(kind);
    p.extId = extId;
    p.positionMs = qint64(positionMs);
    p.durationMs = qint64(durationMs);
    p.title = info.value(QStringLiteral("title")).toString();
    p.image = info.value(QStringLiteral("image")).toString();
    p.seriesExtId = info.value(QStringLiteral("seriesExtId")).toString();
    p.season = info.value(QStringLiteral("season")).toInt();
    p.episode = info.value(QStringLiteral("episode")).toInt();
    SourceStore(Database::connection()).saveProgress(p);
    emit DataNotifier::instance()->progressChanged(sourceId);
}

void VodManager::clearProgress(int sourceId, const QString &kind, const QString &extId)
{
    SourceStore(Database::connection()).removeProgress(sourceId, progressKind(kind), extId);
    emit DataNotifier::instance()->progressChanged(sourceId);
}

bool VodManager::isFavorite(int sourceId, const QString &kind, const QString &extId) const
{
    return SourceStore(Database::connection()).isFavorite(sourceId, kindFromString(kind), extId);
}

void VodManager::setFavorite(int sourceId, const QString &kind, const QString &extId, bool favorite)
{
    SourceStore(Database::connection()).setFavorite(sourceId, kindFromString(kind), extId, favorite);
    emit DataNotifier::instance()->favoritesChanged(sourceId);
}
