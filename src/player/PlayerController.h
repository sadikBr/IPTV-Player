#pragma once

#include "player/PlayerBackend.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// QML-facing player. Owns a PlayerBackend and adds app-level behaviour:
// keeping the screen awake, stopping/resuming when the app goes to the
// background, and simple error handling.
class QTimer;

class PlayerController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QObject *videoOutput READ videoOutput WRITE setVideoOutput NOTIFY videoOutputChanged)
    Q_PROPERTY(QUrl source READ source NOTIFY sourceChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY stateChanged)
    Q_PROPERTY(bool failed READ hasFailed NOTIFY stateChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ isMuted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(qreal bufferProgress READ bufferProgress NOTIFY bufferProgressChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool seekable READ isSeekable NOTIFY seekableChanged)
    Q_PROPERTY(bool live READ isLive NOTIFY durationChanged)
    Q_PROPERTY(QString streamInfo READ streamInfo NOTIFY streamInfoChanged)
    // Movies/episodes: pausing really pauses, and the stream is never treated as live.
    Q_PROPERTY(bool vod READ isVod WRITE setVod NOTIFY vodChanged)
    Q_PROPERTY(QStringList audioTracks READ audioTracks NOTIFY tracksChanged)
    Q_PROPERTY(QStringList subtitleTracks READ subtitleTracks NOTIFY tracksChanged)
    Q_PROPERTY(int activeAudioTrack READ activeAudioTrack WRITE setActiveAudioTrack NOTIFY tracksChanged)
    Q_PROPERTY(int activeSubtitleTrack READ activeSubtitleTrack WRITE setActiveSubtitleTrack NOTIFY tracksChanged)
    // A stream that was playing dropped or stalled and is being reconnected.
    Q_PROPERTY(bool reconnecting READ isReconnecting NOTIFY stateChanged)
    Q_PROPERTY(int reconnectAttempt READ reconnectAttempt NOTIFY stateChanged)
    Q_PROPERTY(int maxReconnectAttempts READ maxReconnectAttempts CONSTANT)

public:
    explicit PlayerController(QObject *parent = nullptr);
    // Takes ownership of the backend (used by tests).
    explicit PlayerController(PlayerBackend *backend, QObject *parent = nullptr);
    ~PlayerController() override;

    QObject *videoOutput() const { return m_videoOutput; }
    void setVideoOutput(QObject *output);

    QUrl source() const { return m_source; }
    PlayerBackend::State state() const { return m_backend->state(); }
    bool isPlaying() const;
    bool isBusy() const;
    bool hasFailed() const;
    QString errorString() const { return m_errorString; }

    qreal volume() const { return m_volume; }
    void setVolume(qreal volume);
    bool isMuted() const { return m_muted; }
    void setMuted(bool muted);

    qreal bufferProgress() const { return m_backend->bufferProgress(); }
    qint64 position() const { return m_backend->position(); }
    qint64 duration() const { return m_backend->duration(); }
    bool isSeekable() const { return m_backend->seekable(); }
    bool isLive() const { return !m_vod && m_backend->duration() <= 0; }
    QString streamInfo() const { return m_backend->streamInfo(); }

    bool isVod() const { return m_vod; }
    void setVod(bool vod);
    QStringList audioTracks() const { return m_backend->audioTracks(); }
    QStringList subtitleTracks() const { return m_backend->subtitleTracks(); }
    int activeAudioTrack() const { return m_backend->activeAudioTrack(); }
    void setActiveAudioTrack(int index) { m_backend->setActiveAudioTrack(index); }
    int activeSubtitleTrack() const { return m_backend->activeSubtitleTrack(); }
    void setActiveSubtitleTrack(int index) { m_backend->setActiveSubtitleTrack(index); }

    bool isReconnecting() const { return m_reconnecting; }
    int reconnectAttempt() const { return m_reconnectAttempts; }
    int maxReconnectAttempts() const { return int(m_reconnectDelaysMs.size()); }

    // Tuning (tests shorten these).
    void setReconnectTiming(int stallTimeoutMs, int stableAfterMs, const QList<int> &delaysMs);

public slots:
    // startPositionMs > 0 resumes there once the media can seek.
    void open(const QUrl &url, qint64 startPositionMs = 0);
    void play();
    void pause();
    void togglePause();
    void stop();
    void retry();
    void seek(qint64 positionMs);
    void seekRelative(qint64 deltaMs);

signals:
    void videoOutputChanged();
    void sourceChanged();
    void stateChanged();
    void errorStringChanged();
    void volumeChanged();
    void mutedChanged();
    void bufferProgressChanged();
    void positionChanged();
    void durationChanged();
    void seekableChanged();
    void streamInfoChanged();
    void vodChanged();
    void tracksChanged();
    void ended();

private:
    void init();
    void startSource(qint64 startPositionMs);
    void onBackendStateChanged();
    void onBackendError(const QString &message);
    void onEndOfMedia();
    void checkStall();
    // Drop or stall after the stream played: try again after a short delay.
    void scheduleReconnect();
    void reconnectNow();
    void cancelReconnect();
    void setErrorString(const QString &error);
    void onApplicationStateChanged(Qt::ApplicationState state);
    void updateWakeLock();
    void applyPendingSeek();

    PlayerBackend *m_backend = nullptr;
    QPointer<QObject> m_videoOutput;
    QUrl m_source;
    QString m_errorString;
    qreal m_volume = 1.0;
    bool m_muted = false;
    bool m_resumeOnActivate = false;
    bool m_vod = false;
    qint64 m_pendingSeek = 0;

    // Reconnect
    QTimer *m_watchdog = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    QElapsedTimer m_sinceProgress;   // last time playback moved on
    QElapsedTimer m_playingSince;    // playing without trouble since
    bool m_hasPlayed = false;        // reached Playing since open()
    bool m_positionMoves = false;    // the backend reports a moving position
    bool m_reconnecting = false;
    bool m_gaveUp = false;
    int m_reconnectAttempts = 0;
    qint64 m_lastPosition = -1;
    qint64 m_resumePosition = 0;     // movies: continue here after a reconnect
    int m_stallTimeoutMs = 15000;
    int m_stableAfterMs = 30000;
    QList<int> m_reconnectDelaysMs { 1000, 2000, 4000, 8000, 15000 };
};
