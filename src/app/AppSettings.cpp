#include "app/AppSettings.h"

#include "NetworkFactory.h"

namespace {
constexpr int MaxRecentUrls = 10;
const auto RecentUrlsKey = QStringLiteral("player/recentUrls");
const auto VolumeKey = QStringLiteral("player/volume");
const auto UserAgentKey = QStringLiteral("network/userAgent"); // read by NetworkFactory
const auto LastSourceKey = QStringLiteral("browse/lastSourceId");
const auto LastChannelSourceKey = QStringLiteral("browse/lastChannelSourceId");
const auto LastChannelExtIdKey = QStringLiteral("browse/lastChannelExtId");
const auto LastChannelNameKey = QStringLiteral("browse/lastChannelName");
} // namespace

AppSettings::AppSettings(QObject *parent) : QObject(parent)
{
    m_recentUrls = m_settings.value(RecentUrlsKey).toStringList();
    m_volume = qBound<qreal>(0.0, m_settings.value(VolumeKey, 1.0).toReal(), 1.0);
}

QString AppSettings::appVersion() const
{
    return QStringLiteral(APP_VERSION);
}

QUrl AppSettings::normalizeUrl(const QString &input) const
{
    const QString text = input.trimmed();
    if (text.isEmpty() || text.contains(QLatin1Char(' ')))
        return {};
    const QUrl url = QUrl::fromUserInput(text);
    if (!url.isValid() || url.scheme().isEmpty()
        || (!url.isLocalFile() && url.host().isEmpty()))
        return {};
    return url;
}

void AppSettings::addRecentUrl(const QString &url)
{
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty())
        return;
    m_recentUrls.removeAll(trimmed);
    m_recentUrls.prepend(trimmed);
    while (m_recentUrls.size() > MaxRecentUrls)
        m_recentUrls.removeLast();
    saveRecentUrls();
}

void AppSettings::removeRecentUrl(const QString &url)
{
    if (m_recentUrls.removeAll(url) > 0)
        saveRecentUrls();
}

void AppSettings::clearRecentUrls()
{
    if (m_recentUrls.isEmpty())
        return;
    m_recentUrls.clear();
    saveRecentUrls();
}

void AppSettings::setVolume(qreal volume)
{
    volume = qBound<qreal>(0.0, volume, 1.0);
    if (qFuzzyCompare(volume, m_volume))
        return;
    m_volume = volume;
    m_settings.setValue(VolumeKey, volume);
    emit volumeChanged();
}

void AppSettings::saveRecentUrls()
{
    m_settings.setValue(RecentUrlsKey, m_recentUrls);
    emit recentUrlsChanged();
}

int AppSettings::lastSourceId() const
{
    return m_settings.value(LastSourceKey, -1).toInt();
}

void AppSettings::setLastSourceId(int id)
{
    if (id == lastSourceId())
        return;
    m_settings.setValue(LastSourceKey, id);
    emit lastSourceIdChanged();
}

int AppSettings::lastChannelSourceId() const
{
    return m_settings.value(LastChannelSourceKey, -1).toInt();
}

QString AppSettings::lastChannelExtId() const
{
    return m_settings.value(LastChannelExtIdKey).toString();
}

QString AppSettings::lastChannelName() const
{
    return m_settings.value(LastChannelNameKey).toString();
}

void AppSettings::setLastChannel(int sourceId, const QString &extId, const QString &name)
{
    m_settings.setValue(LastChannelSourceKey, sourceId);
    m_settings.setValue(LastChannelExtIdKey, extId);
    m_settings.setValue(LastChannelNameKey, name);
    emit lastChannelChanged();
}

QString AppSettings::defaultUserAgent() const
{
    return NetworkFactory::defaultUserAgent();
}

QString AppSettings::userAgent() const
{
    return m_settings.value(UserAgentKey).toString();
}

void AppSettings::setUserAgent(const QString &userAgent)
{
    if (userAgent.trimmed() == this->userAgent())
        return;
    m_settings.setValue(UserAgentKey, userAgent.trimmed());
    emit userAgentChanged();
}
