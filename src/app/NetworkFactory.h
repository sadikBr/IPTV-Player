#pragma once

#include <QNetworkAccessManager>
#include <QQmlNetworkAccessManagerFactory>

// Network access for the whole app.
//
// Every request carries the same User-Agent. Many IPTV providers only answer
// clients that look like a known player and reset the connection otherwise,
// so the default identifies as VLC. It can be changed with the
// "network/userAgent" setting (AppSettings.userAgent).
class NetworkFactory : public QQmlNetworkAccessManagerFactory
{
public:
    // Used by QML (channel logos, posters); has a disk cache.
    QNetworkAccessManager *create(QObject *parent) override;

    // Used by the provider, guide and movie/series managers.
    static QNetworkAccessManager *createManager(QObject *parent);

    static QString userAgent();
    static QString defaultUserAgent();
};
