#include "player/PlayerController.h"

#include "platform/ScreenWakeLock.h"
#include "player/QtMultimediaBackend.h"

#include <QGuiApplication>
#include <QTimer>

PlayerController::PlayerController(QObject *parent)
    : PlayerController(new QtMultimediaBackend, parent)
{
}

PlayerController::PlayerController(PlayerBackend *backend, QObject *parent)
    : QObject(parent), m_backend(backend)
{
    m_backend->setParent(this);
    init();
}

void PlayerController::init()
{
    connect(m_backend, &PlayerBackend::stateChanged, this, &PlayerController::onBackendStateChanged);
    connect(m_backend, &PlayerBackend::bufferProgressChanged, this,
            &PlayerController::bufferProgressChanged);
    connect(m_backend, &PlayerBackend::positionChanged, this, &PlayerController::positionChanged);
    connect(m_backend, &PlayerBackend::durationChanged, this, &PlayerController::durationChanged);
    connect(m_backend, &PlayerBackend::seekableChanged, this, &PlayerController::seekableChanged);
    connect(m_backend, &PlayerBackend::streamInfoChanged, this,
            &PlayerController::streamInfoChanged);
    connect(m_backend, &PlayerBackend::errorOccurred, this, &PlayerController::onBackendError);
    connect(m_backend, &PlayerBackend::tracksChanged, this, &PlayerController::tracksChanged);
    connect(m_backend, &PlayerBackend::endOfMedia, this, &PlayerController::onEndOfMedia);
    connect(m_backend, &PlayerBackend::seekableChanged, this, &PlayerController::applyPendingSeek);
    connect(m_backend, &PlayerBackend::durationChanged, this, &PlayerController::applyPendingSeek);

    m_watchdog = new QTimer(this);
    m_watchdog->setInterval(1000);
    connect(m_watchdog, &QTimer::timeout, this, &PlayerController::checkStall);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &PlayerController::reconnectNow);

    if (qGuiApp) {
        connect(qGuiApp, &QGuiApplication::applicationStateChanged, this,
                &PlayerController::onApplicationStateChanged);
    }
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
    if (m_reconnecting)
        return true;
    const auto s = m_backend->state();
    return s == PlayerBackend::State::Loading || s == PlayerBackend::State::Buffering;
}

bool PlayerController::hasFailed() const
{
    if (m_reconnecting)
        return false;
    return m_gaveUp || state() == PlayerBackend::State::Error;
}

void PlayerController::setReconnectTiming(int stallTimeoutMs, int stableAfterMs,
                                          const QList<int> &delaysMs)
{
    m_stallTimeoutMs = stallTimeoutMs;
    m_stableAfterMs = stableAfterMs;
    m_reconnectDelaysMs = delaysMs;
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
    cancelReconnect();
    m_reconnectAttempts = 0;
    m_gaveUp = false;
    m_hasPlayed = false;
    m_resumePosition = startPositionMs;
    if (m_source != url) {
        m_source = url;
        emit sourceChanged();
    }
    startSource(startPositionMs);
    emit stateChanged();
}

void PlayerController::startSource(qint64 startPositionMs)
{
    m_pendingSeek = startPositionMs;
    m_positionMoves = false;
    m_lastPosition = -1;
    m_playingSince.invalidate();
    m_sinceProgress.restart();
    setErrorString({});
    m_backend->stop();
    if (m_source.isEmpty()) {
        m_watchdog->stop();
        return;
    }
    m_backend->setSource(m_source);
    m_backend->setVolume(m_volume);
    m_backend->setMuted(m_muted);
    m_backend->play();
    m_watchdog->start();
}

void PlayerController::onBackendStateChanged()
{
    const auto s = m_backend->state();
    if (s == PlayerBackend::State::Playing) {
        setErrorString({});
        m_hasPlayed = true;
        m_gaveUp = false;
        if (!m_playingSince.isValid())
            m_playingSince.start();
        m_sinceProgress.restart();
        if (m_reconnecting) {
            m_reconnecting = false;
            m_reconnectTimer->stop();
        }
    } else if (s != PlayerBackend::State::Buffering) {
        m_playingSince.invalidate();
    }
    updateWakeLock();
    applyPendingSeek();
    emit stateChanged();
}

void PlayerController::onBackendError(const QString &message)
{
    setErrorString(message);
    // Only streams that already played are reconnected here. When a stream
    // can't start at all, the page shows "Channel unavailable" right away.
    if (m_hasPlayed && !m_gaveUp)
        scheduleReconnect();
}

void PlayerController::onEndOfMedia()
{
    if (m_hasPlayed && !m_gaveUp) {
        // A live stream never really ends: the connection dropped.
        const bool live = !m_vod && m_backend->duration() <= 0;
        // A movie that stops long before its end lost the connection too.
        const qint64 duration = m_backend->duration();
        const bool cutShort = m_vod && duration > 0 && m_resumePosition > 0
                              && m_resumePosition < duration - 60000;
        if (live || cutShort) {
            scheduleReconnect();
            return;
        }
    }
    emit ended();
}

void PlayerController::checkStall()
{
    // While waiting to reconnect there is nothing to watch; a reconnect attempt
    // that hangs is watched like any other start.
    if (!m_hasPlayed || m_gaveUp || m_reconnectTimer->isActive())
        return;

    const auto s = m_backend->state();
    const qint64 pos = m_backend->position();
    if (pos != m_lastPosition) {
        if (m_lastPosition >= 0 && pos > m_lastPosition)
            m_positionMoves = true;
        m_lastPosition = pos;
        if (pos > 0 && m_vod)
            m_resumePosition = pos;
        m_sinceProgress.restart();
    }

    switch (s) {
    case PlayerBackend::State::Loading:
    case PlayerBackend::State::Buffering:
        // Waiting for data; too long means the stream is stuck.
        break;
    case PlayerBackend::State::Playing:
        if (m_playingSince.isValid() && m_playingSince.elapsed() >= m_stableAfterMs)
            m_reconnectAttempts = 0; // playing well again
        // A frozen picture: only detectable when the position normally moves.
        if (!m_positionMoves)
            return;
        break;
    default:
        // Paused or stopped by the user, or an error (handled elsewhere).
        m_sinceProgress.restart();
        return;
    }

    if (m_sinceProgress.elapsed() >= m_stallTimeoutMs)
        scheduleReconnect();
}

void PlayerController::scheduleReconnect()
{
    if (m_reconnectTimer->isActive())
        return;
    if (m_reconnectAttempts >= m_reconnectDelaysMs.size()) {
        // Give up: the page shows the error and its own retry options.
        m_reconnecting = false;
        m_gaveUp = true;
        m_watchdog->stop();
        m_backend->stop();
        if (m_errorString.isEmpty())
            setErrorString(tr("Lost the connection to the stream."));
        updateWakeLock();
        emit stateChanged();
        return;
    }
    const int delay = m_reconnectDelaysMs.at(m_reconnectAttempts);
    ++m_reconnectAttempts;
    m_reconnecting = true;
    m_backend->stop();
    m_reconnectTimer->start(delay);
    updateWakeLock();
    emit stateChanged();
}

void PlayerController::reconnectNow()
{
    // Movies continue where they were; live streams start at "now".
    const qint64 position = m_vod ? m_resumePosition : 0;
    m_backend->setSource({});
    startSource(position);
}

void PlayerController::cancelReconnect()
{
    m_reconnectTimer->stop();
    if (m_reconnecting) {
        m_reconnecting = false;
        emit stateChanged();
    }
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
    cancelReconnect();
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
    cancelReconnect();
    m_watchdog->stop();
    m_backend->stop();
}

void PlayerController::retry()
{
    const QUrl url = m_source;
    // Movies continue where they were.
    const qint64 position = m_vod ? qMax(m_backend->position(), m_resumePosition) : 0;
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
