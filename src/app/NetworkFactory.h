#pragma once

#include <QQmlNetworkAccessManagerFactory>

// Gives QML (channel logos) a network manager with a disk cache, so logos
// load instantly after the first time and survive restarts.
class NetworkFactory : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager *create(QObject *parent) override;

    static QString userAgent();
};
