#include "NetworkFactory.h"

#include <QDir>
#include <QMutex>
#include <QNetworkReply>
#include <QSet>
#include <QNetworkDiskCache>
#include <QNetworkRequest>
#include <QSettings>
#include <QStandardPaths>

namespace {

const auto kUserAgentKey = QStringLiteral("network/userAgent");

// Image hosts that don't exist (dead links in provider data). Requests to them
// are answered with an empty image instead of being retried for every poster.
// QML loads images on its own thread, hence the mutex.
QMutex g_deadHostsMutex;
QSet<QString> g_deadHosts;

const QUrl kEmptyImage(QStringLiteral(
        "data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7"));

// Adds the app's User-Agent to every request that doesn't set one.
class AppNetworkAccessManager : public QNetworkAccessManager
{
public:
    AppNetworkAccessManager(QObject *parent, bool skipDeadHosts)
        : QNetworkAccessManager(parent), m_skipDeadHosts(skipDeadHosts)
    {
    }

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &original,
                                 QIODevice *outgoingData) override
    {
        QNetworkRequest request(original);
        const QString host = request.url().host();
        if (m_skipDeadHosts && !host.isEmpty()) {
            QMutexLocker lock(&g_deadHostsMutex);
            if (g_deadHosts.contains(host)) {
                request.setUrl(kEmptyImage);
                lock.unlock();
                return QNetworkAccessManager::createRequest(op, request, outgoingData);
            }
        }
        if (!request.hasRawHeader("User-Agent"))
            request.setHeader(QNetworkRequest::UserAgentHeader, NetworkFactory::userAgent());

        QNetworkReply *reply = QNetworkAccessManager::createRequest(op, request, outgoingData);
        if (m_skipDeadHosts) {
            QObject::connect(reply, &QNetworkReply::errorOccurred, reply,
                             [host](QNetworkReply::NetworkError error) {
                                 if (error == QNetworkReply::HostNotFoundError) {
                                     QMutexLocker lock(&g_deadHostsMutex);
                                     g_deadHosts.insert(host);
                                 }
                             });
        }
        return reply;
    }

private:
    bool m_skipDeadHosts = false;
};

} // namespace

QNetworkAccessManager *NetworkFactory::create(QObject *parent)
{
    auto *nam = new AppNetworkAccessManager(parent, true);
    auto *cache = new QNetworkDiskCache(nam);
    cache->setCacheDirectory(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
                                     .filePath(QStringLiteral("http")));
    cache->setMaximumCacheSize(200 * 1024 * 1024);
    nam->setCache(cache);
    return nam;
}

QNetworkAccessManager *NetworkFactory::createManager(QObject *parent)
{
    return new AppNetworkAccessManager(parent, false);
}

QString NetworkFactory::defaultUserAgent()
{
    return QStringLiteral("VLC/3.0.20 LibVLC/3.0.20");
}

QString NetworkFactory::userAgent()
{
    const QString custom = QSettings().value(kUserAgentKey).toString().trimmed();
    return custom.isEmpty() ? defaultUserAgent() : custom;
}
