#pragma once

#include "player/PlayerBackend.h"

class QMediaPlayer;
class QAudioOutput;

// Player backend built on Qt Multimedia (FFmpeg backend on all platforms).
class QtMultimediaBackend : public PlayerBackend
{
    Q_OBJECT

public:
    explicit QtMultimediaBackend(QObject *parent = nullptr);
    ~QtMultimediaBackend() override;

    QString name() const override { return QStringLiteral("Qt Multimedia"); }

    void setVideoOutput(QObject *output) override;
    void setSource(const QUrl &url) override;
    void play() override;
    void pause() override;
    void stop() override;
    void setVolume(qreal volume) override;
    void setMuted(bool muted) override;

    State state() const override { return m_state; }
    qreal bufferProgress() const override;
    qint64 position() const override;
    qint64 duration() const override;
    bool seekable() const override;
    void seek(qint64 positionMs) override;
    QString streamInfo() const override { return m_streamInfo; }

private:
    void updateState();
    void updateStreamInfo();

    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audio = nullptr;
    State m_state = State::Stopped;
    bool m_hasError = false;
    QString m_streamInfo;
};
