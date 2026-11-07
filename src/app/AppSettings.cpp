#include "app/AppSettings.h"

namespace {
constexpr int MaxRecentUrls = 10;
const auto RecentUrlsKey = QStringLiteral("player/recentUrls");
const auto VolumeKey = QStringLiteral("player/volume");
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
