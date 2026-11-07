#include "SourceListModel.h"

#include "Database.h"

using namespace iptv;

SourceListModel::SourceListModel(QObject *parent) : QAbstractListModel(parent)
{
    connect(DataNotifier::instance(), &DataNotifier::sourcesChanged, this, &SourceListModel::reload);
    reload();
}

int SourceListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_sources.size());
}

QVariant SourceListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_sources.size())
        return {};
    const SourceRecord &s = m_sources.at(index.row());
    switch (role) {
    case SourceIdRole:
        return s.id;
    case NameRole:
    case Qt::DisplayRole:
        return s.name;
    case TypeRole:
        return s.type;
    case UrlRole:
        return s.url;
    case UsernameRole:
        return s.username;
    case LiveCountRole:
        return s.liveCount;
    case LastSyncRole:
        return s.lastSync;
    case ExpiresAtRole:
        return s.expiresAt;
    case StatusRole:
        return s.status;
    case MaxConnectionsRole:
        return s.maxConnections;
    case EpgLastSyncRole:
        return s.epgLastSync;
    case EpgCountRole:
        return s.epgCount;
    case EpgShiftRole:
        return s.epgShiftMinutes;
    case OutputFormatRole:
        return s.outputFormat;
    case MovieCountRole:
        return s.movieCount;
    case SeriesCountRole:
        return s.seriesCount;
    default:
        return {};
    }
}

QHash<int, QByteArray> SourceListModel::roleNames() const
{
    return {
        { SourceIdRole, "sourceId" },
        { NameRole, "name" },
        { TypeRole, "type" },
        { UrlRole, "url" },
        { UsernameRole, "username" },
        { LiveCountRole, "liveCount" },
        { LastSyncRole, "lastSync" },
        { ExpiresAtRole, "expiresAt" },
        { StatusRole, "status" },
        { MaxConnectionsRole, "maxConnections" },
        { EpgLastSyncRole, "epgLastSync" },
        { EpgCountRole, "epgCount" },
        { EpgShiftRole, "epgShiftMinutes" },
        { MovieCountRole, "movieCount" },
        { SeriesCountRole, "seriesCount" },
        { OutputFormatRole, "outputFormat" },
    };
}

int SourceListModel::sourceIdAt(int row) const
{
    return row >= 0 && row < m_sources.size() ? m_sources.at(row).id : -1;
}

void SourceListModel::reload()
{
    const int oldCount = int(m_sources.size());
    beginResetModel();
    m_sources = SourceStore(Database::connection()).sources();
    endResetModel();
    if (oldCount != m_sources.size())
        emit countChanged();
}
