#include "NetworkFactory.h"

#include <QDir>
#include <QNetworkDiskCache>
#include <QNetworkRequest>
#include <QSettings>
#include <QStandardPaths>

namespace {

const auto kUserAgentKey = QStringLiteral("network/userAgent");

// Adds the app's User-Agent to every request that doesn't set one.
class AppNetworkAccessManager : public QNetworkAccessManager
{
public:
    using QNetworkAccessManager::QNetworkAccessManager;

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &original,
                                 QIODevice *outgoingData) override
    {
        QNetworkRequest request(original);
        if (!request.hasRawHeader("User-Agent"))
            request.setHeader(QNetworkRequest::UserAgentHeader, NetworkFactory::userAgent());
        return QNetworkAccessManager::createRequest(op, request, outgoingData);
    }
};

} // namespace

QNetworkAccessManager *NetworkFactory::create(QObject *parent)
{
    auto *nam = new AppNetworkAccessManager(parent);
    auto *cache = new QNetworkDiskCache(nam);
    cache->setCacheDirectory(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
                                     .filePath(QStringLiteral("http")));
    cache->setMaximumCacheSize(200 * 1024 * 1024);
    nam->setCache(cache);
    return nam;
}

QNetworkAccessManager *NetworkFactory::createManager(QObject *parent)
{
    return new AppNetworkAccessManager(parent);
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
