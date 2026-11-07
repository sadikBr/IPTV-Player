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
    bool isLive() const { return m_backend->duration() <= 0; }
    QString streamInfo() const { return m_backend->streamInfo(); }

public slots:
    void open(const QUrl &url);
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

private:
    void setErrorString(const QString &error);
    void onApplicationStateChanged(Qt::ApplicationState state);
    void updateWakeLock();

    PlayerBackend *m_backend = nullptr;
    QPointer<QObject> m_videoOutput;
    QUrl m_source;
    QString m_errorString;
    qreal m_volume = 1.0;
    bool m_muted = false;
    bool m_resumeOnActivate = false;
};
