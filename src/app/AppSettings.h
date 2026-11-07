#pragma once

#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// Persistent user preferences, available in QML as the `AppSettings` singleton.
class AppSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QStringList recentUrls READ recentUrls NOTIFY recentUrlsChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)

public:
    explicit AppSettings(QObject *parent = nullptr);

    QString appVersion() const;

    QStringList recentUrls() const { return m_recentUrls; }
    Q_INVOKABLE void addRecentUrl(const QString &url);
    Q_INVOKABLE void removeRecentUrl(const QString &url);
    Q_INVOKABLE void clearRecentUrls();

    // Turns user input ("example.com/live.m3u8", " http://… ") into a URL.
    // Returns an empty URL if the input is not usable.
    Q_INVOKABLE QUrl normalizeUrl(const QString &input) const;

    qreal volume() const { return m_volume; }
    void setVolume(qreal volume);

signals:
    void recentUrlsChanged();
    void volumeChanged();

private:
    void saveRecentUrls();

    QSettings m_settings;
    QStringList m_recentUrls;
    qreal m_volume = 1.0;
};
