#include "player/PlayerController.h"

#include "platform/ScreenWakeLock.h"
#include "player/QtMultimediaBackend.h"

#include <QGuiApplication>

PlayerController::PlayerController(QObject *parent)
    : QObject(parent), m_backend(new QtMultimediaBackend(this))
{
    connect(m_backend, &PlayerBackend::stateChanged, this, [this] {
        if (m_backend->state() == PlayerBackend::State::Playing)
            setErrorString({});
        updateWakeLock();
        applyPendingSeek();
        emit stateChanged();
    });
    connect(m_backend, &PlayerBackend::bufferProgressChanged, this,
            &PlayerController::bufferProgressChanged);
    connect(m_backend, &PlayerBackend::positionChanged, this, &PlayerController::positionChanged);
    connect(m_backend, &PlayerBackend::durationChanged, this, &PlayerController::durationChanged);
    connect(m_backend, &PlayerBackend::seekableChanged, this, &PlayerController::seekableChanged);
    connect(m_backend, &PlayerBackend::streamInfoChanged, this,
            &PlayerController::streamInfoChanged);
    connect(m_backend, &PlayerBackend::errorOccurred, this, &PlayerController::setErrorString);
    connect(m_backend, &PlayerBackend::tracksChanged, this, &PlayerController::tracksChanged);
    connect(m_backend, &PlayerBackend::endOfMedia, this, &PlayerController::ended);
    connect(m_backend, &PlayerBackend::seekableChanged, this, &PlayerController::applyPendingSeek);
    connect(m_backend, &PlayerBackend::durationChanged, this, &PlayerController::applyPendingSeek);

    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this,
            &PlayerController::onApplicationStateChanged);
}

PlayerController::~PlayerController()
{
    m_backend->stop();
    ScreenWakeLock::setActive(false);
}

void PlayerController::setVideoOutput(QObject *output)
{
    if (m_videoOutput == output)
        return;
    m_videoOutput = output;
    m_backend->setVideoOutput(output);
    emit videoOutputChanged();
}

bool PlayerController::isPlaying() const
{
    return m_backend->state() == PlayerBackend::State::Playing;
}

bool PlayerController::isBusy() const
{
    const auto s = m_backend->state();
    return s == PlayerBackend::State::Loading || s == PlayerBackend::State::Buffering;
}

void PlayerController::setVolume(qreal volume)
{
    volume = qBound<qreal>(0.0, volume, 1.0);
    if (qFuzzyCompare(volume, m_volume))
        return;
    m_volume = volume;
    m_backend->setVolume(volume);
    emit volumeChanged();
}

void PlayerController::setMuted(bool muted)
{
    if (muted == m_muted)
        return;
    m_muted = muted;
    m_backend->setMuted(muted);
    emit mutedChanged();
}

void PlayerController::setVod(bool vod)
{
    if (vod == m_vod)
        return;
    m_vod = vod;
    emit vodChanged();
    emit durationChanged(); // "live" depends on it
}

void PlayerController::applyPendingSeek()
{
    if (m_pendingSeek <= 0 || !m_backend->seekable() || m_backend->duration() <= 0)
        return;
    const qint64 target = qMin(m_pendingSeek, m_backend->duration() - 5000);
    m_pendingSeek = 0;
    if (target > 0)
        m_backend->seek(target);
}

void PlayerController::open(const QUrl &url, qint64 startPositionMs)
{
    m_pendingSeek = startPositionMs;
    setErrorString({});
    m_backend->stop();
    if (m_source != url) {
        m_source = url;
        emit sourceChanged();
    }
    if (url.isEmpty())
        return;
    m_backend->setSource(url);
    m_backend->setVolume(m_volume);
    m_backend->setMuted(m_muted);
    m_backend->play();
}

void PlayerController::play()
{
    if (m_backend->state() == PlayerBackend::State::Error) {
        retry();
        return;
    }
    m_backend->play();
}

void PlayerController::pause()
{
    // Pausing a live stream makes the server buffer pile up and the stream
    // usually drops. Stop instead; play() will reconnect.
    if (isLive())
        m_backend->stop();
    else
        m_backend->pause();
}

void PlayerController::togglePause()
{
    const auto s = m_backend->state();
    if (s == PlayerBackend::State::Playing || isBusy())
        pause();
    else if (isLive() && s == PlayerBackend::State::Stopped)
        retry();
    else
        play();
}

void PlayerController::stop()
{
    m_backend->stop();
}

void PlayerController::retry()
{
    const QUrl url = m_source;
    // Movies continue where they were.
    const qint64 position = m_vod ? m_backend->position() : 0;
    m_backend->setSource({});
    open(url, position);
}

void PlayerController::seek(qint64 positionMs)
{
    if (isSeekable())
        m_backend->seek(qBound<qint64>(0, positionMs, duration()));
}

void PlayerController::seekRelative(qint64 deltaMs)
{
    seek(position() + deltaMs);
}

void PlayerController::setErrorString(const QString &error)
{
    if (error == m_errorString)
        return;
    m_errorString = error;
    emit errorStringChanged();
}

void PlayerController::onApplicationStateChanged(Qt::ApplicationState state)
{
    // Only react to the app really going to the background (mobile). On
    // desktop the window merely losing focus is ApplicationInactive.
    if (state == Qt::ApplicationSuspended || state == Qt::ApplicationHidden) {
        const auto s = m_backend->state();
        m_resumeOnActivate = s == PlayerBackend::State::Playing || isBusy();
        if (m_resumeOnActivate)
            pause();
    } else if (state == Qt::ApplicationActive && m_resumeOnActivate) {
        m_resumeOnActivate = false;
        if (isLive())
            retry();
        else
            play();
    }
}

void PlayerController::updateWakeLock()
{
    const auto s = m_backend->state();
    ScreenWakeLock::setActive(s == PlayerBackend::State::Playing || isBusy());
}
