#include "RadioStationModel.h"

using namespace iptv;

int RadioStationModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_stations.size());
}

QVariant RadioStationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_stations.size())
        return {};
    const RadioStation &s = m_stations.at(index.row());
    switch (role) {
    case UuidRole: return s.uuid;
    case NameRole:
    case Qt::DisplayRole: return s.name;
    case UrlRole: return s.url;
    case LogoRole: return s.favicon;
    case TagsRole: return s.tags.mid(0, 3).join(QStringLiteral(", "));
    case CountryRole: return s.country;
    case CountryCodeRole: return s.countryCode;
    case CodecRole: return s.codec;
    case BitrateRole: return s.bitrate;
    case FavoriteRole: return m_favorites.contains(s.uuid);
    case DetailsRole: {
        // "jazz, smooth · US · MP3 128 kbps"
        QStringList parts;
        if (!s.tags.isEmpty())
            parts << s.tags.mid(0, 2).join(QStringLiteral(", "));
        if (!s.countryCode.isEmpty())
            parts << s.countryCode;
        if (!s.codec.isEmpty() || s.bitrate > 0)
            parts << QStringLiteral("%1 %2").arg(s.codec, s.bitrate > 0 ? QStringLiteral("%1 kbps").arg(s.bitrate)
                                                                        : QString()).trimmed();
        return parts.join(QStringLiteral(" · "));
    }
    default: return {};
    }
}

QHash<int, QByteArray> RadioStationModel::roleNames() const
{
    return { { UuidRole, "uuid" }, { NameRole, "name" }, { UrlRole, "url" }, { LogoRole, "logo" },
             { TagsRole, "tags" }, { CountryRole, "country" }, { CountryCodeRole, "countryCode" },
             { CodecRole, "codec" }, { BitrateRole, "bitrate" }, { FavoriteRole, "favorite" },
             { DetailsRole, "details" } };
}

void RadioStationModel::setStations(const QList<RadioStation> &stations, const QSet<QString> &favorites)
{
    beginResetModel();
    m_stations = stations;
    m_favorites = favorites;
    endResetModel();
    emit countChanged();
}

void RadioStationModel::setFavorite(const QString &uuid, bool favorite)
{
    if (favorite)
        m_favorites.insert(uuid);
    else
        m_favorites.remove(uuid);
    for (int i = 0; i < m_stations.size(); ++i) {
        if (m_stations.at(i).uuid == uuid)
            emit dataChanged(index(i), index(i), { FavoriteRole });
    }
}

const RadioStation *RadioStationModel::station(int row) const
{
    return row >= 0 && row < m_stations.size() ? &m_stations.at(row) : nullptr;
}
