#include "player/QtMultimediaBackend.h"

#include <QAudioOutput>
#include <QMediaFormat>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QSize>
#include <QStringList>

namespace {

QMediaMetaData activeTrack(const QList<QMediaMetaData> &tracks, int index)
{
    if (index >= 0 && index < tracks.size())
        return tracks.at(index);
    return tracks.isEmpty() ? QMediaMetaData() : tracks.first();
}

QString formatBitRate(const QVariant &value)
{
    const int bps = value.toInt();
    if (bps <= 0)
        return {};
    return QStringLiteral("%1 kb/s").arg(bps / 1000);
}

} // namespace

QtMultimediaBackend::QtMultimediaBackend(QObject *parent)
    : PlayerBackend(parent), m_player(new QMediaPlayer(this)), m_audio(new QAudioOutput(this))
{
    m_player->setAudioOutput(m_audio);

    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &QtMultimediaBackend::updateState);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &QtMultimediaBackend::updateState);
    connect(m_player, &QMediaPlayer::bufferProgressChanged, this,
            &PlayerBackend::bufferProgressChanged);
    connect(m_player, &QMediaPlayer::positionChanged, this, &PlayerBackend::positionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &PlayerBackend::durationChanged);
    connect(m_player, &QMediaPlayer::seekableChanged, this, &PlayerBackend::seekableChanged);
    connect(m_player, &QMediaPlayer::metaDataChanged, this, &QtMultimediaBackend::updateStreamInfo);
    connect(m_player, &QMediaPlayer::tracksChanged, this, &QtMultimediaBackend::updateStreamInfo);

    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error error, const QString &message) {
                if (error == QMediaPlayer::NoError)
                    return;
                m_hasError = true;
                updateState();
                emit errorOccurred(message.isEmpty() ? tr("Unable to play this stream.")
                                                     : message);
            });
}

QtMultimediaBackend::~QtMultimediaBackend()
{
    m_player->stop();
}

void QtMultimediaBackend::setVideoOutput(QObject *output)
{
    m_player->setVideoOutput(output);
}

void QtMultimediaBackend::setSource(const QUrl &url)
{
    m_hasError = false;
    m_player->setSource(url);
    updateState();
    updateStreamInfo();
}

void QtMultimediaBackend::play()
{
    m_hasError = false;
    m_player->play();
    updateState();
}

void QtMultimediaBackend::pause()
{
    m_player->pause();
}

void QtMultimediaBackend::stop()
{
    m_player->stop();
}

void QtMultimediaBackend::setVolume(qreal volume)
{
    m_audio->setVolume(float(volume));
}

void QtMultimediaBackend::setMuted(bool muted)
{
    m_audio->setMuted(muted);
}

qreal QtMultimediaBackend::bufferProgress() const
{
    return m_player->bufferProgress();
}

qint64 QtMultimediaBackend::position() const
{
    return m_player->position();
}

qint64 QtMultimediaBackend::duration() const
{
    return m_player->duration();
}

bool QtMultimediaBackend::seekable() const
{
    return m_player->isSeekable();
}

void QtMultimediaBackend::seek(qint64 positionMs)
{
    m_player->setPosition(positionMs);
}

void QtMultimediaBackend::updateState()
{
    State next = State::Stopped;
    if (m_hasError) {
        next = State::Error;
    } else {
        switch (m_player->playbackState()) {
        case QMediaPlayer::StoppedState:
            next = State::Stopped;
            break;
        case QMediaPlayer::PausedState:
            next = State::Paused;
            break;
        case QMediaPlayer::PlayingState:
            switch (m_player->mediaStatus()) {
            case QMediaPlayer::LoadingMedia:
            case QMediaPlayer::NoMedia:
                next = State::Loading;
                break;
            case QMediaPlayer::BufferingMedia:
            case QMediaPlayer::StalledMedia:
                next = State::Buffering;
                break;
            default:
                next = State::Playing;
                break;
            }
            break;
        }
    }

    if (next != m_state) {
        m_state = next;
        emit stateChanged();
    }
}

void QtMultimediaBackend::updateStreamInfo()
{
    QStringList lines;

    const QMediaMetaData video =
            activeTrack(m_player->videoTracks(), m_player->activeVideoTrack());
    const QMediaMetaData audio =
            activeTrack(m_player->audioTracks(), m_player->activeAudioTrack());
    const QMediaMetaData global = m_player->metaData();

    auto value = [&](const QMediaMetaData &track, QMediaMetaData::Key key) {
        const QVariant v = track.value(key);
        return v.isValid() ? v : global.value(key);
    };

    // Video
    QStringList videoParts;
    const QVariant vCodec = value(video, QMediaMetaData::VideoCodec);
    if (vCodec.isValid()) {
        const auto codec = vCodec.value<QMediaFormat::VideoCodec>();
        if (codec != QMediaFormat::VideoCodec::Unspecified)
            videoParts << QMediaFormat::videoCodecName(codec);
    }
    const QSize resolution = value(video, QMediaMetaData::Resolution).toSize();
    if (resolution.isValid())
        videoParts << QStringLiteral("%1×%2").arg(resolution.width()).arg(resolution.height());
    const qreal fps = value(video, QMediaMetaData::VideoFrameRate).toReal();
    if (fps > 0)
        videoParts << QStringLiteral("%1 fps").arg(fps, 0, 'f', fps == int(fps) ? 0 : 2);
    const QString vRate = formatBitRate(value(video, QMediaMetaData::VideoBitRate));
    if (!vRate.isEmpty())
        videoParts << vRate;
    if (!videoParts.isEmpty())
        lines << tr("Video: %1").arg(videoParts.join(QStringLiteral(" · ")));

    // Audio
    QStringList audioParts;
    const QVariant aCodec = value(audio, QMediaMetaData::AudioCodec);
    if (aCodec.isValid()) {
        const auto codec = aCodec.value<QMediaFormat::AudioCodec>();
        if (codec != QMediaFormat::AudioCodec::Unspecified)
            audioParts << QMediaFormat::audioCodecName(codec);
    }
    const QString aRate = formatBitRate(value(audio, QMediaMetaData::AudioBitRate));
    if (!aRate.isEmpty())
        audioParts << aRate;
    const QString language = audio.stringValue(QMediaMetaData::Language);
    if (!language.isEmpty())
        audioParts << language;
    if (!audioParts.isEmpty())
        lines << tr("Audio: %1").arg(audioParts.join(QStringLiteral(" · ")));

    const QString source = m_player->source().toString();
    if (!source.isEmpty()) {
        const int dot = source.lastIndexOf(QLatin1Char('.'));
        const int slash = source.lastIndexOf(QLatin1Char('/'));
        if (dot > slash && dot >= 0)
            lines << tr("Container: %1").arg(source.mid(dot + 1).section(QLatin1Char('?'), 0, 0));
    }

    if (m_player->videoTracks().size() > 1 || m_player->audioTracks().size() > 1) {
        lines << tr("Tracks: %1 video, %2 audio")
                         .arg(m_player->videoTracks().size())
                         .arg(m_player->audioTracks().size());
    }

    lines << tr("Engine: %1").arg(name());

    const QString info = lines.join(QLatin1Char('\n'));
    if (info != m_streamInfo) {
        m_streamInfo = info;
        emit streamInfoChanged();
    }
}
