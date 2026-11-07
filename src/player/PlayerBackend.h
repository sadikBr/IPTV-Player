#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

// Abstract video player. The UI only talks to PlayerController, which owns one
// backend. This lets us add other engines (for example libmpv on desktop)
// without touching the UI.
class PlayerBackend : public QObject
{
    Q_OBJECT

public:
    enum class State { Stopped, Loading, Buffering, Playing, Paused, Error };
    Q_ENUM(State)

    using QObject::QObject;
    ~PlayerBackend() override = default;

    virtual QString name() const = 0;

    // QML VideoOutput item (or another sink the backend understands).
    virtual void setVideoOutput(QObject *output) = 0;

    virtual void setSource(const QUrl &url) = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;

    virtual void setVolume(qreal volume) = 0; // 0.0 .. 1.0
    virtual void setMuted(bool muted) = 0;

    virtual State state() const = 0;
    virtual qreal bufferProgress() const = 0; // 0.0 .. 1.0
    virtual qint64 position() const = 0;      // ms
    virtual qint64 duration() const = 0;      // ms, 0 for live streams
    virtual bool seekable() const = 0;
    virtual void seek(qint64 positionMs) = 0;

    // Human readable technical details (codecs, resolution, ...).
    virtual QString streamInfo() const = 0;

signals:
    void stateChanged();
    void bufferProgressChanged();
    void positionChanged();
    void durationChanged();
    void seekableChanged();
    void streamInfoChanged();
    void errorOccurred(const QString &message);
};
