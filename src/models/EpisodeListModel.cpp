#include "EpisodeListModel.h"

#include "Database.h"

using namespace iptv;

EpisodeListModel::EpisodeListModel(QObject *parent) : QAbstractListModel(parent)
{
    connect(DataNotifier::instance(), &DataNotifier::episodesChanged, this,
            [this](int sourceId, const QString &seriesExtId) {
                if (sourceId == m_sourceId && seriesExtId == m_seriesExtId)
                    reload();
            });
    connect(DataNotifier::instance(), &DataNotifier::progressChanged, this, [this](int sourceId) {
        if (sourceId == m_sourceId)
            reload();
    });
    connect(DataNotifier::instance(), &DataNotifier::contentChanged, this, [this](int sourceId) {
        if (sourceId == m_sourceId)
            reload();
    });
}

void EpisodeListModel::setSourceId(int id)
{
    if (id == m_sourceId)
        return;
    m_sourceId = id;
    emit sourceIdChanged();
    reload();
}

void EpisodeListModel::setSeriesExtId(const QString &id)
{
    if (id == m_seriesExtId)
        return;
    m_seriesExtId = id;
    emit seriesExtIdChanged();
    reload();
}

void EpisodeListModel::setSeason(int season)
{
    if (season == m_season)
        return;
    m_season = season;
    emit seasonChanged();
    reload();
}

int EpisodeListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant EpisodeListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row &row = m_rows.at(index.row());
    const EpisodeData &e = row.data;
    switch (role) {
    case ExtIdRole:
        return e.extId;
    case TitleRole:
    case Qt::DisplayRole:
        return e.title;
    case SeasonRole:
        return e.season;
    case EpisodeRole:
        return e.episode;
    case PlotRole:
        return e.plot;
    case ImageRole:
        return e.image;
    case DurationRole:
        return e.durationSecs;
    case WatchedRole:
        return row.watched;
    default:
        return {};
    }
}

QHash<int, QByteArray> EpisodeListModel::roleNames() const
{
    return {
        { ExtIdRole, "extId" },       { TitleRole, "title" },  { SeasonRole, "season" },
        { EpisodeRole, "episode" },   { PlotRole, "plot" },    { ImageRole, "image" },
        { DurationRole, "durationSecs" }, { WatchedRole, "watched" },
    };
}

QVariantMap EpisodeListModel::get(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return {};
    const EpisodeData &e = m_rows.at(row).data;
    return {
        { QStringLiteral("extId"), e.extId },     { QStringLiteral("title"), e.title },
        { QStringLiteral("season"), e.season },   { QStringLiteral("episode"), e.episode },
        { QStringLiteral("plot"), e.plot },       { QStringLiteral("image"), e.image },
        { QStringLiteral("durationSecs"), e.durationSecs },
        { QStringLiteral("watched"), m_rows.at(row).watched },
    };
}

QUrl EpisodeListModel::streamUrl(int row) const
{
    if (row < 0 || row >= m_rows.size() || m_source.id < 0)
        return {};
    const EpisodeData &e = m_rows.at(row).data;
    return SourceStore::streamUrl(m_source, ContentKind::Episode, e.extId, e.url, e.extension);
}

int EpisodeListModel::indexOfExtId(const QString &extId) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).data.extId == extId)
            return i;
    }
    return -1;
}

int EpisodeListModel::resumeIndex() const
{
    int last = -1;
    for (int i = 0; i < m_rows.size(); ++i) {
        const Row &r = m_rows.at(i);
        if (r.lastWatched.isValid() && (last < 0 || r.lastWatched > m_rows.at(last).lastWatched))
            last = i;
    }
    if (last < 0)
        return m_rows.isEmpty() ? -1 : 0;
    if (!m_rows.at(last).finished)
        return last;
    return last + 1 < m_rows.size() ? last + 1 : last;
}

void EpisodeListModel::reload()
{
    QList<Row> rows;
    QVariantList seasons;
    if (m_sourceId > 0 && !m_seriesExtId.isEmpty()) {
        SourceStore store(Database::connection());
        m_source = store.source(m_sourceId).value_or(SourceRecord{});
        const QList<EpisodeData> all = store.episodes(m_sourceId, m_seriesExtId);
        for (const EpisodeData &e : all) {
            if (!seasons.contains(e.season))
                seasons.append(e.season);
            if (m_season > 0 && e.season != m_season)
                continue;
            Row row;
            row.data = e;
            if (const auto p = store.progress(m_sourceId, ContentKind::Episode, e.extId)) {
                row.finished = p->finished();
                row.watched = row.finished ? 1.0
                                           : double(p->positionMs) / double(qMax<qint64>(1, p->durationMs));
                row.lastWatched = p->updatedAt;
            }
            rows.append(row);
        }
    }

    const bool countChange = rows.size() != m_rows.size();
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    if (countChange)
        emit countChanged();
    if (seasons != m_seasons) {
        m_seasons = seasons;
        emit seasonsChanged();
    }
}
