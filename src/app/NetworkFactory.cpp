#include "NetworkFactory.h"

#include <QCoreApplication>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QStandardPaths>

QNetworkAccessManager *NetworkFactory::create(QObject *parent)
{
    auto *nam = new QNetworkAccessManager(parent);
    auto *cache = new QNetworkDiskCache(nam);
    cache->setCacheDirectory(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
                                     .filePath(QStringLiteral("http")));
    cache->setMaximumCacheSize(200 * 1024 * 1024);
    nam->setCache(cache);
    return nam;
}

QString NetworkFactory::userAgent()
{
    return QStringLiteral("IPTVPlayer/%1").arg(QCoreApplication::applicationVersion());
}
