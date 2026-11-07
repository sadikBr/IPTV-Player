#include "SourceManager.h"

#include "Database.h"
#include "M3uParser.h"
#include "NetworkFactory.h"

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtConcurrent/QtConcurrentRun>

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
    : QObject(parent), m_nam(new QNetworkAccessManager(this))
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
    m_content = {};
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
    m_content = {};
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

void SourceManager::download(const QUrl &url, const QString &label, const DataHandler &onData)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, NetworkFactory::userAgent());
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

    connect(reply, &QNetworkReply::finished, this, [this, reply, onData] {
        reply->deleteLater();
        if (m_reply == reply)
            m_reply = nullptr;
        emit statusChanged(); // canCancel

        if (m_cancelled)
            return finish(false);
        if (reply->error() != QNetworkReply::NoError)
            return finish(false, friendlyNetworkError(reply));
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 400)
            return finish(false, tr("The server returned an error (HTTP %1).").arg(status));
        onData(reply->readAll());
    });
}

void SourceManager::syncXtream()
{
    const XtreamCredentials creds{ QUrl(m_source.url), m_source.username, m_source.password };

    download(XtreamApi::apiUrl(creds, QStringLiteral("get_live_categories")),
             tr("Loading categories…"), [this, creds](const QByteArray &data) {
                 QString error;
                 if (!XtreamApi::parseCategories(data, ContentKind::Live, &m_content.categories,
                                                 &error))
                     return finish(false, error);

                 download(XtreamApi::apiUrl(creds, QStringLiteral("get_live_streams")),
                          tr("Loading channels…"), [this](const QByteArray &streams) {
                              const int id = m_source.id;
                              const QList<CategoryData> categories = m_content.categories;
                              runWorker([id, categories, streams] {
                                  WorkResult result;
                                  SourceContent content;
                                  content.categories = categories;
                                  if (!XtreamApi::parseLiveStreams(streams, &content.channels,
                                                                   &result.error))
                                      return result;
                                  if (content.channels.isEmpty()) {
                                      result.error = tr("The provider returned no live channels.");
                                      return result;
                                  }
                                  SourceStore store(Database::connection());
                                  result.ok = store.replaceContent(id, content);
                                  result.error = store.lastError();
                                  result.liveCount = int(content.channels.size());
                                  return result;
                              });
                          });
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
