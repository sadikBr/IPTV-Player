#pragma once

#include "player/PlayerBackend.h"

#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// QML-facing player. Owns a PlayerBackend and adds app-level behaviour:
// keeping the screen awake, stopping/resuming when the app goes to the
// background, and simple error handling.
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

public:
    explicit PlayerController(QObject *parent = nullptr);
    ~PlayerController() override;

    QObject *videoOutput() const { return m_videoOutput; }
    void setVideoOutput(QObject *output);

    QUrl source() const { return m_source; }
    PlayerBackend::State state() const { return m_backend->state(); }
    bool isPlaying() const;
    bool isBusy() const;
    bool hasFailed() const { return state() == PlayerBackend::State::Error; }
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
};
