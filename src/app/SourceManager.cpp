#include "SourceManager.h"

#include "Database.h"
#include "M3uParser.h"
#include "NetworkFactory.h"

#include <QFile>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtConcurrent/QtConcurrentRun>
#include <utility>

using namespace iptv;

namespace {

constexpr int kTransferTimeoutMs = 60000;

QString formatSize(qint64 bytes)
{
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(qMax<qint64>(1, bytes / 1024));
    return QStringLiteral("%1 MB").arg(double(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
}

QString friendlyNetworkError(QNetworkReply *reply)
{
    switch (reply->error()) {
    case QNetworkReply::HostNotFoundError:
        return SourceManager::tr("Server not found. Check the address.");
    case QNetworkReply::ConnectionRefusedError:
        return SourceManager::tr("The server refused the connection. Check the address and port.");
    case QNetworkReply::TimeoutError:
    case QNetworkReply::OperationCanceledError: // transfer timeout
        return SourceManager::tr("The server took too long to respond.");
    case QNetworkReply::SslHandshakeFailedError:
        return SourceManager::tr("Secure connection failed. Try http:// instead of https://.");
    case QNetworkReply::AuthenticationRequiredError:
    case QNetworkReply::ContentAccessDenied:
        return SourceManager::tr("Access denied by the server. Check your login.");
    case QNetworkReply::ContentNotFoundError:
        return SourceManager::tr("Not found on the server (404). Check the address.");
    case QNetworkReply::RemoteHostClosedError:
        return SourceManager::tr("The server closed the connection. Your provider may be "
                                 "blocking this app or your IP address for a while (for example "
                                 "after many requests), or your internet provider may block it.");
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::TemporaryNetworkFailureError:
        return SourceManager::tr("The server can't be reached from this network (%1). Check "
                                 "your internet connection, VPN or DNS settings.")
                .arg(reply->errorString());
    default:
        return reply->errorString();
    }
}

bool isLocalUrl(const QUrl &url)
{
    return url.isLocalFile() || url.scheme() == QLatin1String("content")
            || url.scheme() == QLatin1String("qrc");
}

QString localPath(const QUrl &url)
{
    // Android content:// URIs are opened by QFile as-is.
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

} // namespace

SourceManager::SourceManager(QObject *parent)
    : QObject(parent), m_nam(NetworkFactory::createManager(this))
{
    connect(&m_watcher, &QFutureWatcher<WorkResult>::finished, this, [this] {
        const WorkResult result = m_watcher.result();
        finish(result.ok, result.error);
    });
    connect(DataNotifier::instance(), &DataNotifier::sourcesChanged, this,
            &SourceManager::sourcesChanged);
}

SourceManager::~SourceManager()
{
    if (m_reply)
        m_reply->abort();
    m_watcher.waitForFinished();
}

bool SourceManager::hasSources() const
{
    return !SourceStore(Database::connection()).sources().isEmpty();
}

// ---------------------------------------------------------------------------
// Public API

void SourceManager::addXtreamSource(const QString &name, const QString &server,
                                    const QString &username, const QString &password,
                                    const QString &format)
{
    if (!begin(-1, true))
        return;

    const QUrl serverUrl = XtreamApi::normalizeServer(server);
    if (!serverUrl.isValid())
        return finish(false, tr("Enter a valid server address, for example http://example.com:8080"));
    if (username.trimmed().isEmpty() || password.isEmpty())
        return finish(false, tr("Enter your username and password."));

    m_source = {};
    m_source.type = QStringLiteral("xtream");
    m_source.name = name.trimmed().isEmpty() ? serverUrl.host() : name.trimmed();
    m_source.url = serverUrl.toString();
    m_source.username = username.trimmed();
    m_source.password = password;
    m_source.outputFormat = format == QLatin1String("m3u8") ? format : QStringLiteral("ts");

    const XtreamCredentials creds{ serverUrl, m_source.username, m_source.password };
    download(XtreamApi::apiUrl(creds), tr("Signing in…"), [this](const QByteArray &data) {
        XtreamAccount account;
        QString error;
        if (!XtreamApi::parseAccount(data, &account, &error))
            return finish(false, error);

        m_source.status = account.status;
        m_source.expiresAt = account.expiresAt;
        m_source.maxConnections = account.maxConnections;
        // Use a format the provider allows.
        const QStringList allowed = account.allowedFormats;
        if (!allowed.isEmpty() && !allowed.contains(m_source.outputFormat)) {
            if (allowed.contains(QLatin1String("ts")))
                m_source.outputFormat = QStringLiteral("ts");
            else if (allowed.contains(QLatin1String("m3u8")))
                m_source.outputFormat = QStringLiteral("m3u8");
        }

        SourceStore store(Database::connection());
        const int id = store.addSource(m_source);
        if (id < 0)
            return finish(false, store.lastError());
        m_source.id = id;
        m_activeSourceId = id;
        emit busyChanged();
        syncXtream();
    });
}

void SourceManager::addM3uSource(const QString &name, const QString &url, const QString &epgUrl)
{
    if (!begin(-1, true))
        return;

    const QString text = url.trimmed();
    const QUrl playlistUrl = text.contains(QLatin1String("://")) ? QUrl(text)
                                                                 : QUrl::fromUserInput(text);
    const bool remote = playlistUrl.scheme() == QLatin1String("http")
            || playlistUrl.scheme() == QLatin1String("https");
    if (text.isEmpty() || !playlistUrl.isValid() || (!remote && !isLocalUrl(playlistUrl)))
        return finish(false, tr("Enter a playlist URL (http://…) or choose a file."));

    m_source = {};
    m_source.type = QStringLiteral("m3u");
    m_source.url = playlistUrl.toString();
    m_source.epgUrl = epgUrl.trimmed();
    m_source.name = name.trimmed();
    if (m_source.name.isEmpty()) {
        m_source.name = remote ? playlistUrl.host() : playlistUrl.fileName();
        if (m_source.name.isEmpty())
            m_source.name = tr("Playlist");
    }

    SourceStore store(Database::connection());
    const int id = store.addSource(m_source);
    if (id < 0)
        return finish(false, store.lastError());
    m_source.id = id;
    m_activeSourceId = id;
    emit busyChanged();
    syncM3u();
}

void SourceManager::refresh(int sourceId)
{
    if (!begin(sourceId, false))
        return;

    const auto source = SourceStore(Database::connection()).source(sourceId);
    if (!source)
        return finish(false, tr("This source no longer exists."));
    m_source = *source;

    if (!m_source.isXtream())
        return syncM3u();

    // Update account details (expiry, status) first, then the content.
    const XtreamCredentials creds{ QUrl(m_source.url), m_source.username, m_source.password };
    download(XtreamApi::apiUrl(creds), tr("Signing in…"), [this](const QByteArray &data) {
        XtreamAccount account;
        QString error;
        if (!XtreamApi::parseAccount(data, &account, &error))
            return finish(false, error);
        m_source.status = account.status;
        m_source.expiresAt = account.expiresAt;
        m_source.maxConnections = account.maxConnections;
        SourceStore(Database::connection()).updateSource(m_source);
        syncXtream();
    });
}

bool SourceManager::refreshIfStale(int sourceId, int maxAgeHours)
{
    if (m_busy)
        return false;
    const auto source = SourceStore(Database::connection()).source(sourceId);
    if (!source)
        return false;
    const QDateTime limit = QDateTime::currentDateTime().addSecs(-qint64(maxAgeHours) * 3600);
    if (source->lastSync.isValid() && source->lastSync > limit)
        return false;
    refresh(sourceId);
    return true;
}

void SourceManager::removeSource(int sourceId)
{
    if (m_busy && m_activeSourceId == sourceId)
        cancel();
    SourceStore(Database::connection()).removeSource(sourceId);
    emit DataNotifier::instance()->sourcesChanged();
    emit DataNotifier::instance()->contentChanged(sourceId);
}

void SourceManager::setEpgShift(int sourceId, int minutes)
{
    SourceStore(Database::connection()).setEpgShift(sourceId, minutes);
    emit DataNotifier::instance()->sourcesChanged();
    emit DataNotifier::instance()->epgChanged(sourceId);
}

void SourceManager::cancel()
{
    if (!m_busy)
        return;
    m_cancelled = true;
    if (m_reply)
        m_reply->abort(); // finish() is called from the reply's finished handler
}

QVariantMap SourceManager::source(int sourceId) const
{
    const auto s = SourceStore(Database::connection()).source(sourceId);
    if (!s)
        return {};
    return {
        { QStringLiteral("id"), s->id },
        { QStringLiteral("name"), s->name },
        { QStringLiteral("type"), s->type },
        { QStringLiteral("url"), s->url },
        { QStringLiteral("username"), s->username },
        { QStringLiteral("outputFormat"), s->outputFormat },
        { QStringLiteral("epgUrl"), s->epgUrl },
        { QStringLiteral("status"), s->status },
        { QStringLiteral("expiresAt"), s->expiresAt },
        { QStringLiteral("maxConnections"), s->maxConnections },
        { QStringLiteral("liveCount"), s->liveCount },
        { QStringLiteral("movieCount"), s->movieCount },
        { QStringLiteral("seriesCount"), s->seriesCount },
        { QStringLiteral("epgShiftMinutes"), s->epgShiftMinutes },
        { QStringLiteral("expiresAtText"), s->expiresAt.isValid()
                  ? QLocale().toString(s->expiresAt.toLocalTime().date(), QLocale::ShortFormat)
                  : QString() },
        { QStringLiteral("epgCount"), s->epgCount },
        { QStringLiteral("epgLastSync"), s->epgLastSync },
        { QStringLiteral("lastSync"), s->lastSync },
    };
}

bool SourceManager::sourceExists(int sourceId) const
{
    return sourceId > 0 && SourceStore(Database::connection()).source(sourceId).has_value();
}

QVariantMap SourceManager::parseCredentialsUrl(const QString &url) const
{
    XtreamCredentials creds;
    if (!XtreamApi::parseCredentialsUrl(url, &creds))
        return {};
    return { { QStringLiteral("server"), creds.server.toString() },
             { QStringLiteral("username"), creds.username },
             { QStringLiteral("password"), creds.password } };
}

// ---------------------------------------------------------------------------
// Steps

bool SourceManager::begin(int sourceId, bool isNew)
{
    if (m_busy)
        return false;
    m_busy = true;
    m_isNew = isNew;
    m_cancelled = false;
    m_activeSourceId = sourceId;
    m_payloads.clear();
    emit busyChanged();
    setStatus(tr("Connecting…"));
    return true;
}

void SourceManager::finish(bool ok, const QString &error)
{
    const int id = m_activeSourceId;
    const bool isNew = m_isNew;
    QString message = error;
    if (!ok && m_cancelled)
        message = tr("Cancelled.");

    if (!ok && isNew && id > 0)
        SourceStore(Database::connection()).removeSource(id);

    m_busy = false;
    m_isNew = false;
    m_activeSourceId = -1;
    m_reply = nullptr;
    m_payloads.clear();
    setStatus({}, -1);
    emit busyChanged();

    if (ok)
        emit DataNotifier::instance()->contentChanged(id);
    emit DataNotifier::instance()->sourcesChanged();
    emit finished(ok || !isNew ? id : -1, ok, message, isNew);
}

void SourceManager::setStatus(const QString &text, qreal progress)
{
    m_statusText = text;
    m_progress = progress;
    emit statusChanged();
}

void SourceManager::download(const QUrl &url, const QString &label, const DataHandler &onData,
                             DownloadFailure onFailure)
{
    QNetworkRequest request(url);
    request.setTransferTimeout(kTransferTimeoutMs);

    QNetworkReply *reply = m_nam->get(request);
    m_reply = reply;
    setStatus(label);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, label](qint64 received, qint64 total) {
                if (received < 256 * 1024)
                    return;
                setStatus(QStringLiteral("%1 %2").arg(label, formatSize(received)),
                          total > 0 ? qreal(received) / qreal(total) : -1);
            });

    connect(reply, &QNetworkReply::finished, this, [this, reply, onData, onFailure] {
        reply->deleteLater();
        if (m_reply == reply)
            m_reply = nullptr;
        emit statusChanged(); // canCancel

        if (m_cancelled)
            return finish(false);
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool failed = reply->error() != QNetworkReply::NoError || status >= 400;
        if (failed && onFailure == DownloadFailure::Skip) {
            qWarning("Skipping optional request %s: %s", qPrintable(reply->url().path()),
                     qPrintable(reply->errorString()));
            return onData(QByteArray());
        }
        if (reply->error() != QNetworkReply::NoError)
            return finish(false, friendlyNetworkError(reply));
        if (status >= 400)
            return finish(false, tr("The server returned an error (HTTP %1).").arg(status));
        onData(reply->readAll());
    });
}

void SourceManager::syncXtream()
{
    m_payloads.clear();
    runXtreamStep(0);
}

namespace {

struct XtreamStep
{
    const char *action;
    const char *label;
    bool required; // movies and series are optional: some providers don't offer them
};

const XtreamStep kXtreamSteps[] = {
    { "get_live_categories", QT_TRANSLATE_NOOP("SourceManager", "Loading categories…"), true },
    { "get_live_streams", QT_TRANSLATE_NOOP("SourceManager", "Loading channels…"), true },
    { "get_vod_categories", QT_TRANSLATE_NOOP("SourceManager", "Loading movie categories…"), false },
    { "get_vod_streams", QT_TRANSLATE_NOOP("SourceManager", "Loading movies…"), false },
    { "get_series_categories", QT_TRANSLATE_NOOP("SourceManager", "Loading series categories…"), false },
    { "get_series", QT_TRANSLATE_NOOP("SourceManager", "Loading series…"), false },
};
constexpr int kXtreamStepCount = int(sizeof(kXtreamSteps) / sizeof(kXtreamSteps[0]));

} // namespace

void SourceManager::runXtreamStep(int index)
{
    if (index >= kXtreamStepCount) {
        saveXtream();
        return;
    }
    const XtreamStep &step = kXtreamSteps[index];
    const XtreamCredentials creds{ QUrl(m_source.url), m_source.username, m_source.password };
    const QString action = QString::fromLatin1(step.action);
    download(XtreamApi::apiUrl(creds, action), tr(step.label),
             [this, index, action](const QByteArray &data) {
                 m_payloads.insert(action, data);
                 runXtreamStep(index + 1);
             },
             step.required ? DownloadFailure::Abort : DownloadFailure::Skip);
}

void SourceManager::saveXtream()
{
    const int id = m_source.id;
    const QHash<QString, QByteArray> payloads = std::exchange(m_payloads, {});
    runWorker([id, payloads] {
        WorkResult result;
        SourceContent content;
        QString error;
        const auto get = [&payloads](const char *action) {
            return payloads.value(QString::fromLatin1(action));
        };

        if (!XtreamApi::parseCategories(get("get_live_categories"), ContentKind::Live,
                                        &content.categories, &result.error)
            || !XtreamApi::parseLiveStreams(get("get_live_streams"), &content.channels,
                                            &result.error))
            return result;

        // Optional parts: ignore what is missing or malformed.
        QList<CategoryData> categories;
        QList<ChannelData> items;
        if (XtreamApi::parseCategories(get("get_vod_categories"), ContentKind::Movie, &categories, &error))
            content.categories += categories;
        if (XtreamApi::parseMovies(get("get_vod_streams"), &items, &error))
            content.channels += items;
        if (XtreamApi::parseCategories(get("get_series_categories"), ContentKind::Series, &categories, &error))
            content.categories += categories;
        if (XtreamApi::parseSeries(get("get_series"), &items, &error))
            content.channels += items;

        if (content.channels.isEmpty()) {
            result.error = tr("The provider returned no channels, movies or series.");
            return result;
        }
        SourceStore store(Database::connection());
        result.ok = store.replaceContent(id, content);
        result.error = store.lastError();
        result.liveCount = int(content.channels.size());
        return result;
    });
}

void SourceManager::syncM3u()
{
    const int id = m_source.id;
    const QUrl url(m_source.url);

    auto parseAndSave = [id](const QByteArray &data) {
        WorkResult result;
        if (!M3uParser::looksLikeM3u(data)) {
            result.error = data.trimmed().isEmpty()
                    ? tr("The playlist is empty.")
                    : tr("This doesn't look like an M3U playlist. Check the URL.");
            return result;
        }
        const SourceContent content = M3uParser::parse(data);
        if (content.channels.isEmpty()) {
            result.error = tr("The playlist contains no channels.");
            return result;
        }
        SourceStore store(Database::connection());
        result.ok = store.replaceContent(id, content);
        result.error = store.lastError();
        return result;
    };

    if (isLocalUrl(url)) {
        const QString path = localPath(url);
        setStatus(tr("Reading playlist…"));
        runWorker([path, parseAndSave] {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                WorkResult result;
                result.error = tr("Could not open the file: %1").arg(file.errorString());
                return result;
            }
            return parseAndSave(file.readAll());
        });
        return;
    }

    download(url, tr("Downloading playlist…"), [this, parseAndSave](const QByteArray &data) {
        runWorker([data, parseAndSave] { return parseAndSave(data); });
    });
}

void SourceManager::runWorker(std::function<WorkResult()> work)
{
    setStatus(tr("Saving channels…"));
    m_watcher.setFuture(QtConcurrent::run([work] {
        WorkResult result = work();
        Database::closeThreadConnection();
        return result;
    }));
}
