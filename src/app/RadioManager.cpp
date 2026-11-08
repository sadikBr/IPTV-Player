#include "RadioManager.h"

#include "Database.h"
#include "RadioStore.h"

#include <QCoreApplication>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>

using namespace iptv;

namespace {

constexpr int kLimit = 120;

QNetworkRequest apiRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    // The directory asks clients to identify themselves.
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("IPTVPlayer/%1").arg(QCoreApplication::applicationVersion()));
    request.setTransferTimeout(20000);
    return request;
}

} // namespace

RadioManager::RadioManager(QObject *parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this))
{
}

QString RadioManager::localCountry() const
{
    const QString name = QLocale::system().name(); // "fr_FR"
    return name.size() >= 5 ? name.mid(3, 2).toUpper() : QString();
}

QStringList RadioManager::genres() const
{
    return { QStringLiteral("pop"), QStringLiteral("rock"), QStringLiteral("news"),
             QStringLiteral("jazz"), QStringLiteral("classical"), QStringLiteral("dance"),
             QStringLiteral("hits"), QStringLiteral("talk"), QStringLiteral("chillout"),
             QStringLiteral("electronic"), QStringLiteral("hiphop"), QStringLiteral("80s"),
             QStringLiteral("oldies"), QStringLiteral("country"), QStringLiteral("sports"),
             QStringLiteral("arabic"), QStringLiteral("latin"), QStringLiteral("reggae") };
}

int RadioManager::favoriteCount() const
{
    return int(RadioStore(Database::connection()).favorites().size());
}

QSet<QString> RadioManager::favoriteIds() const
{
    QSet<QString> ids;
    for (const RadioStation &s : RadioStore(Database::connection()).favorites())
        ids.insert(s.uuid);
    return ids;
}

QVariantMap RadioManager::current() const
{
    if (m_current.uuid.isEmpty())
        return {};
    QStringList details;
    if (!m_current.tags.isEmpty())
        details << m_current.tags.mid(0, 2).join(QStringLiteral(", "));
    if (!m_current.country.isEmpty())
        details << m_current.country;
    return { { QStringLiteral("uuid"), m_current.uuid }, { QStringLiteral("name"), m_current.name },
             { QStringLiteral("url"), m_current.url }, { QStringLiteral("logo"), m_current.favicon },
             { QStringLiteral("details"), details.join(QStringLiteral(" · ")) },
             { QStringLiteral("favorite"), RadioStore(Database::connection()).isFavorite(m_current.uuid) } };
}

void RadioManager::setMode(const QString &mode, const QString &value)
{
    m_mode = mode;
    m_modeValue = value;
    m_error.clear();
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->abort();
    }
    emit stateChanged();
}

void RadioManager::showLocal(const QList<RadioStation> &stations)
{
    m_model.setStations(stations, favoriteIds());
    emit stateChanged();
}

void RadioManager::showFavorites()
{
    setMode(QStringLiteral("favorites"), {});
    showLocal(RadioStore(Database::connection()).favorites());
}

void RadioManager::showRecent()
{
    setMode(QStringLiteral("recent"), {});
    showLocal(RadioStore(Database::connection()).recent(60));
}

void RadioManager::showTop()
{
    setMode(QStringLiteral("top"), {});
    withServer([this](const QString &server) { query(RadioBrowser::topVotedUrl(server, kLimit)); });
}

void RadioManager::showTrending()
{
    setMode(QStringLiteral("trending"), {});
    withServer([this](const QString &server) { query(RadioBrowser::trendingUrl(server, kLimit)); });
}

void RadioManager::showCountry(const QString &code)
{
    setMode(QStringLiteral("country"), code);
    withServer([this, code](const QString &server) {
        query(RadioBrowser::searchUrl(server, {}, code, {}, kLimit));
    });
}

void RadioManager::showGenre(const QString &tag)
{
    setMode(QStringLiteral("genre"), tag);
    withServer([this, tag](const QString &server) {
        query(RadioBrowser::searchUrl(server, {}, {}, tag, kLimit));
    });
}

void RadioManager::search(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        showTop();
        return;
    }
    setMode(QStringLiteral("search"), text.trimmed());
    withServer([this, text](const QString &server) {
        query(RadioBrowser::searchUrl(server, text, {}, {}, kLimit));
    });
}

void RadioManager::reload()
{
    const QString mode = m_mode;
    const QString value = m_modeValue;
    if (mode == QLatin1String("favorites")) showFavorites();
    else if (mode == QLatin1String("recent")) showRecent();
    else if (mode == QLatin1String("trending")) showTrending();
    else if (mode == QLatin1String("country")) showCountry(value);
    else if (mode == QLatin1String("genre")) showGenre(value);
    else if (mode == QLatin1String("search")) search(value);
    else showTop();
}

void RadioManager::withServer(std::function<void(const QString &)> then)
{
    if (!m_server.isEmpty()) {
        then(m_server);
        return;
    }
    m_waiting.append(std::move(then));
    if (m_resolvingServer)
        return;
    // Pick one of the mirrors at random, as the directory asks.
    m_resolvingServer = true;
    QNetworkReply *reply = m_network->get(apiRequest(RadioBrowser::serversUrl()));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_resolvingServer = false;
        QStringList servers = RadioBrowser::parseServers(reply->readAll());
        if (servers.isEmpty())
            servers = RadioBrowser::fallbackServers();
        m_server = servers.at(QRandomGenerator::global()->bounded(int(servers.size())));
        const auto waiting = std::exchange(m_waiting, {});
        for (const auto &then : waiting)
            then(m_server);
    });
}

void RadioManager::query(const QUrl &url)
{
    const QString mode = m_mode;
    m_reply = m_network->get(apiRequest(url));
    QNetworkReply *reply = m_reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, mode] {
        reply->deleteLater();
        if (reply != m_reply)
            return; // replaced by a newer query
        m_reply = nullptr;
        if (reply->error() != QNetworkReply::NoError) {
            m_error = tr("Couldn't reach the radio directory: %1").arg(reply->errorString());
            m_server.clear(); // try another mirror next time
            m_model.setStations({}, {});
        } else {
            m_model.setStations(RadioBrowser::parseStations(reply->readAll()), favoriteIds());
        }
        emit stateChanged();
    });
    emit stateChanged();
}

void RadioManager::searchStations(const QString &text)
{
    if (m_searchReply)
        m_searchReply->abort();
    if (text.trimmed().size() < 2) {
        m_searchModel.setStations({}, {});
        return;
    }
    withServer([this, text](const QString &server) {
        m_searchReply = m_network->get(apiRequest(RadioBrowser::searchUrl(server, text, {}, {}, 20)));
        QNetworkReply *reply = m_searchReply;
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError)
                m_searchModel.setStations(RadioBrowser::parseStations(reply->readAll()), favoriteIds());
            else if (reply->error() != QNetworkReply::OperationCanceledError)
                m_searchModel.setStations({}, {});
        });
    });
}

void RadioManager::playSearchResult(int row)
{
    const RadioStation *station = m_searchModel.station(row);
    if (!station)
        return;
    m_current = *station;
    RadioStore(Database::connection()).markPlayed(m_current);
    emit currentChanged();
    emit playRequested(QUrl(m_current.url), m_current.name);
}

void RadioManager::play(int row)
{
    const RadioStation *station = m_model.station(row);
    if (!station)
        return;
    m_current = *station;
    RadioStore(Database::connection()).markPlayed(m_current);
    emit currentChanged();
    emit playRequested(QUrl(m_current.url), m_current.name);

    // Count the play in the directory (fire and forget).
    if (!m_server.isEmpty()) {
        QNetworkReply *click = m_network->get(apiRequest(RadioBrowser::clickUrl(m_server, m_current.uuid)));
        connect(click, &QNetworkReply::finished, click, &QObject::deleteLater);
    }
}

void RadioManager::setFavorite(const RadioStation &station, bool favorite)
{
    RadioStore(Database::connection()).setFavorite(station, favorite);
    m_model.setFavorite(station.uuid, favorite);
    emit favoritesChanged();
    if (station.uuid == m_current.uuid)
        emit currentChanged();
    if (m_mode == QLatin1String("favorites"))
        showLocal(RadioStore(Database::connection()).favorites());
}

void RadioManager::toggleFavorite(int row)
{
    const RadioStation *station = m_model.station(row);
    if (station)
        setFavorite(*station, !RadioStore(Database::connection()).isFavorite(station->uuid));
}

void RadioManager::toggleCurrentFavorite()
{
    if (!m_current.uuid.isEmpty())
        setFavorite(m_current, !RadioStore(Database::connection()).isFavorite(m_current.uuid));
}
