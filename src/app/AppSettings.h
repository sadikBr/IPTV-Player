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
    // Sent to providers with every request; empty = default (VLC).
    Q_PROPERTY(QString userAgent READ userAgent WRITE setUserAgent NOTIFY userAgentChanged)
    Q_PROPERTY(QString defaultUserAgent READ defaultUserAgent CONSTANT)
    // Live TV layout: "grid" or "list"
    Q_PROPERTY(QString liveViewMode READ liveViewMode WRITE setLiveViewMode NOTIFY liveViewModeChanged)
    // Playback (Settings → Playback)
    Q_PROPERTY(bool autoReconnect READ autoReconnect WRITE setAutoReconnect NOTIFY playbackChanged)
    Q_PROPERTY(bool lowLatency READ lowLatency WRITE setLowLatency NOTIFY playbackChanged)
    Q_PROPERTY(bool lowLatencySupported READ lowLatencySupported CONSTANT)
    // Takes effect after a restart.
    Q_PROPERTY(bool hardwareDecoding READ hardwareDecoding WRITE setHardwareDecoding NOTIFY playbackChanged)
    Q_PROPERTY(int lastSourceId READ lastSourceId WRITE setLastSourceId NOTIFY lastSourceIdChanged)
    Q_PROPERTY(int lastChannelSourceId READ lastChannelSourceId NOTIFY lastChannelChanged)
    Q_PROPERTY(QString lastChannelExtId READ lastChannelExtId NOTIFY lastChannelChanged)
    Q_PROPERTY(QString lastChannelName READ lastChannelName NOTIFY lastChannelChanged)

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

    // Sort order of a section ("live", "movie", "series"), see ChannelListModel.
    Q_INVOKABLE QString sortOrder(const QString &kind) const;
    Q_INVOKABLE void setSortOrder(const QString &kind, const QString &order);

    QString liveViewMode() const;
    void setLiveViewMode(const QString &mode);

    bool autoReconnect() const;
    void setAutoReconnect(bool on);
    bool lowLatency() const;
    void setLowLatency(bool on);
    bool lowLatencySupported() const;
    bool hardwareDecoding() const;
    void setHardwareDecoding(bool on);
    // Called once at start-up, before any video plays.
    static void applyHardwareDecodingSetting();

    QString userAgent() const;
    QString defaultUserAgent() const;
    void setUserAgent(const QString &userAgent);

    // The source shown last, reopened on start-up.
    int lastSourceId() const;
    void setLastSourceId(int id);

    // The channel watched last ("Continue watching").
    int lastChannelSourceId() const;
    QString lastChannelExtId() const;
    QString lastChannelName() const;
    Q_INVOKABLE void setLastChannel(int sourceId, const QString &extId, const QString &name);

signals:
    void recentUrlsChanged();
    void volumeChanged();
    void userAgentChanged();
    void liveViewModeChanged();
    void playbackChanged();
    void lastSourceIdChanged();
    void lastChannelChanged();

private:
    void saveRecentUrls();

    QSettings m_settings;
    QStringList m_recentUrls;
    qreal m_volume = 1.0;
};
