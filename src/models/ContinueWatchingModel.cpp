#include "ContinueWatchingModel.h"

#include "Database.h"

using namespace iptv;

ContinueWatchingModel::ContinueWatchingModel(QObject *parent) : QAbstractListModel(parent)
{
    connect(DataNotifier::instance(), &DataNotifier::progressChanged, this,
            &ContinueWatchingModel::reload);
    connect(DataNotifier::instance(), &DataNotifier::sourcesChanged, this,
            &ContinueWatchingModel::reload);
    reload();
}

int ContinueWatchingModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QVariant ContinueWatchingModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const WatchProgress &p = m_items.at(index.row());
    switch (role) {
    case SourceIdRole:
        return p.sourceId;
    case KindRole:
        return kindToString(p.kind);
    case ExtIdRole:
        return p.extId;
    case TitleRole:
    case Qt::DisplayRole:
        return p.title;
    case SubtitleRole:
        return p.kind == ContentKind::Episode
                ? tr("S%1 · E%2").arg(p.season).arg(p.episode)
                : QString();
    case ImageRole:
        return p.image;
    case WatchedRole:
        return double(p.positionMs) / double(qMax<qint64>(1, p.durationMs));
    case RemainingRole:
        return int((p.durationMs - p.positionMs) / 60000);
    case SeriesExtIdRole:
        return p.seriesExtId;
    default:
        return {};
    }
}

QHash<int, QByteArray> ContinueWatchingModel::roleNames() const
{
    return {
        { SourceIdRole, "sourceId" }, { KindRole, "kind" },       { ExtIdRole, "extId" },
        { TitleRole, "title" },       { SubtitleRole, "subtitle" }, { ImageRole, "image" },
        { WatchedRole, "watched" },   { RemainingRole, "remainingMinutes" },
        { SeriesExtIdRole, "seriesExtId" },
    };
}

QVariantMap ContinueWatchingModel::get(int row) const
{
    QVariantMap map;
    if (row < 0 || row >= m_items.size())
        return map;
    const QModelIndex idx = index(row);
    const auto roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        map.insert(QString::fromLatin1(it.value()), data(idx, it.key()));
    return map;
}

void ContinueWatchingModel::reload()
{
    const int oldCount = int(m_items.size());
    beginResetModel();
    m_items = SourceStore(Database::connection()).recentProgress(20);
    endResetModel();
    if (oldCount != m_items.size())
        emit countChanged();
}
