#include "EpgManager.h"

#include "Database.h"
#include "EpgStore.h"
#include "NetworkFactory.h"
#include "SourceStore.h"
#include "XtreamApi.h"

#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QtConcurrent/QtConcurrentRun>

using namespace iptv;

namespace {

constexpr int kTransferTimeoutMs = 120000;
constexpr qint64 kKeepPastSecs = 24 * 3600;       // keep 1 day back
constexpr qint64 kKeepFutureSecs = 4 * 24 * 3600; // and 4 days ahead

QUrl guideUrl(const SourceRecord &source)
{
    if (source.isXtream())
        return XtreamApi::epgUrl({ QUrl(source.url), source.username, source.password });
    const QString url = source.epgUrl.trimmed();
    if (url.isEmpty())
        return {};
    return url.contains(QLatin1String("://")) ? QUrl(url) : QUrl::fromUserInput(url);
}

QVariantMap toMap(const ProgramRecord &p, qint64 now)
{
    const qint64 length = qMax<qint64>(1, p.stop - p.start);
    return {
        { QStringLiteral("title"), p.title },
        { QStringLiteral("subTitle"), p.subTitle },
        { QStringLiteral("description"), p.description },
        { QStringLiteral("start"), QDateTime::fromSecsSinceEpoch(p.start) },
        { QStringLiteral("stop"), QDateTime::fromSecsSinceEpoch(p.stop) },
        { QStringLiteral("progress"), qBound(0.0, double(now - p.start) / double(length), 1.0) },
    };
}

QString formatSize(qint64 bytes)
{
    return QStringLiteral("%1 MB").arg(double(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
}

} // namespace

EpgManager::EpgManager(QObject *parent) : QObject(parent), m_nam(NetworkFactory::createManager(this))
{
    connect(&m_watcher, &QFutureWatcher<ImportResult>::finished, this, [this] {
        const ImportResult result = m_watcher.result();
        finish(result.ok, result.error);
    });
}

EpgManager::~EpgManager()
{
    m_queue.clear();
    if (m_reply)
        m_reply->abort();
    m_watcher.waitForFinished();
}

void EpgManager::refresh(int sourceId)
{
    if (sourceId <= 0 || sourceId == m_activeSourceId || m_queue.contains(sourceId))
        return;
    m_queue.append(sourceId);
    if (!isBusy())
        startNext();
}

bool EpgManager::refreshIfStale(int sourceId, int maxAgeHours)
{
    const auto source = SourceStore(Database::connection()).source(sourceId);
    if (!source || !guideUrl(*source).isValid())
        return false;
    const QDateTime limit = QDateTime::currentDateTime().addSecs(-qint64(maxAgeHours) * 3600);
    if (source->epgLastSync.isValid() && source->epgLastSync > limit && source->epgCount > 0)
        return false;
    refresh(sourceId);
    return true;
}

void EpgManager::cancel()
{
    m_queue.clear();
    if (!isBusy())
        return;
    m_cancelled = true;
    if (m_reply)
        m_reply->abort();
}

bool EpgManager::canHaveGuide(int sourceId) const
{
    const auto source = SourceStore(Database::connection()).source(sourceId);
    return source && guideUrl(*source).isValid();
}

bool EpgManager::hasGuide(int sourceId) const
{
    const auto source = SourceStore(Database::connection()).source(sourceId);
    return source && source->epgCount > 0;
}

QVariantMap EpgManager::nowNext(int sourceId, const QString &epgKey) const
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const QList<ProgramRecord> list = EpgStore(Database::connection()).nowNext(sourceId, epgKey, now);
    QVariantMap result;
    int i = 0;
    if (i < list.size() && list.at(i).start <= now)
        result.insert(QStringLiteral("now"), toMap(list.at(i++), now));
    if (i < list.size())
        result.insert(QStringLiteral("next"), toMap(list.at(i), now));
    return result;
}

QVariantList EpgManager::programs(int sourceId, const QString &epgKey, const QDateTime &from,
                                  const QDateTime &to) const
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QVariantList result;
    const auto list = EpgStore(Database::connection())
                              .programs(sourceId, epgKey, from.toSecsSinceEpoch(),
                                        to.toSecsSinceEpoch());
    result.reserve(list.size());
    for (const ProgramRecord &p : list)
        result.append(toMap(p, now));
    return result;
}

// ---------------------------------------------------------------------------

void EpgManager::startNext()
{
    while (!m_queue.isEmpty()) {
        const int sourceId = m_queue.takeFirst();
        const auto source = SourceStore(Database::connection()).source(sourceId);
        if (!source)
            continue;
        const QUrl url = guideUrl(*source);
        if (!url.isValid())
            continue;

        m_activeSourceId = sourceId;
        m_cancelled = false;
        emit busyChanged();
        setStatus(tr("Downloading TV guide…"));

        if (url.isLocalFile() || url.scheme() == QLatin1String("content")) {
            import(url.isLocalFile() ? url.toLocalFile() : url.toString());
            return;
        }

        const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir().mkpath(dir);
        m_file = std::make_unique<QTemporaryFile>(QDir(dir).filePath(QStringLiteral("guide-XXXXXX")));
        if (!m_file->open()) {
            finish(false, tr("Could not create a temporary file for the TV guide."));
            return;
        }

        QNetworkRequest request(url);
        request.setTransferTimeout(kTransferTimeoutMs);
        QNetworkReply *reply = m_nam->get(request);
        m_reply = reply;

        connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
            if (m_file)
                m_file->write(reply->readAll());
        });
        connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
            setStatus(tr("Downloading TV guide… %1").arg(formatSize(received)),
                      total > 0 ? qreal(received) / qreal(total) : -1);
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            reply->deleteLater();
            m_reply = nullptr;
            if (m_cancelled)
                return finish(false, tr("Cancelled."));
            if (reply->error() != QNetworkReply::NoError)
                return finish(false, reply->errorString());
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status >= 400)
                return finish(false, tr("The server returned an error (HTTP %1).").arg(status));
            m_file->write(reply->readAll());
            m_file->flush();
            import(m_file->fileName());
        });
        return;
    }
}

void EpgManager::import(const QString &path)
{
    setStatus(tr("Updating TV guide…"));
    const int sourceId = m_activeSourceId;
    m_watcher.setFuture(QtConcurrent::run([sourceId, path] {
        ImportResult result;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            result.error = file.errorString();
        } else {
            const qint64 now = QDateTime::currentSecsSinceEpoch();
            EpgStore store(Database::connection());
            result.ok = store.importGuide(sourceId, &file, now - kKeepPastSecs,
                                          now + kKeepFutureSecs, &result.count);
            result.error = store.lastError();
        }
        Database::closeThreadConnection();
        return result;
    }));
}

void EpgManager::finish(bool ok, const QString &error)
{
    const int sourceId = m_activeSourceId;
    m_file.reset();
    m_activeSourceId = -1;
    setStatus({});
    emit busyChanged();
    if (ok)
        emit DataNotifier::instance()->epgChanged(sourceId);
    emit DataNotifier::instance()->sourcesChanged();
    emit finished(sourceId, ok, error);
    startNext();
}

void EpgManager::setStatus(const QString &text, qreal progress)
{
    m_statusText = text;
    m_progress = progress;
    emit statusChanged();
}
