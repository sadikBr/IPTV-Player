#include "ChannelListModel.h"

#include "Catchup.h"

#include "CategoryListModel.h"
#include "Database.h"
#include "EpgStore.h"
#include "XtreamApi.h"

#include <QDateTime>

#include <QSqlError>
#include <QSqlQuery>

using namespace iptv;

ChannelListModel::ChannelListModel(QObject *parent) : QAbstractListModel(parent)
{
    connect(DataNotifier::instance(), &DataNotifier::contentChanged, this, [this](int sourceId) {
        if (sourceId == m_sourceId) {
            reloadSource();
            reload();
        }
    });
    connect(DataNotifier::instance(), &DataNotifier::favoritesChanged, this, [this](int sourceId) {
        if (sourceId == m_sourceId && m_categoryId == CategoryListModel::FavoritesCategory)
            reload();
    });
    connect(DataNotifier::instance(), &DataNotifier::progressChanged, this, [this](int sourceId) {
        if (sourceId == m_sourceId && m_kind == QLatin1String("movie"))
            reload();
    });
    connect(DataNotifier::instance(), &DataNotifier::sourcesChanged, this, [this] {
        const int shift = m_source.epgShiftMinutes;
        reloadSource();
        if (shift != m_source.epgShiftMinutes)
            refreshGuide();
    });
    connect(DataNotifier::instance(), &DataNotifier::epgChanged, this, [this](int sourceId) {
        // Channel/guide links may have changed too.
        if (sourceId == m_sourceId)
            reload();
    });

    // Programs change over time: refresh "now" once a minute.
    m_guideTimer.setInterval(60 * 1000);
    connect(&m_guideTimer, &QTimer::timeout, this, &ChannelListModel::refreshGuide);
    m_guideTimer.start();
}

const ChannelListModel::NowNext &ChannelListModel::nowNext(int row) const
{
    auto it = m_guideCache.constFind(row);
    if (it != m_guideCache.constEnd())
        return *it;

    NowNext info;
    const Row &r = m_rows.at(row);
    if (!r.epgKey.isEmpty() && m_kind == QLatin1String("live")) {
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const auto programs = EpgStore(Database::connection())
                                      .nowNext(m_sourceId, r.epgKey, now,
                                               qint64(m_source.epgShiftMinutes) * 60);
        int i = 0;
        if (i < programs.size() && programs.at(i).start <= now) {
            const auto &p = programs.at(i++);
            info.nowTitle = p.title;
            info.nowProgress = qBound(0.0, double(now - p.start) / double(qMax<qint64>(1, p.stop - p.start)), 1.0);
        }
        if (i < programs.size())
            info.nextTitle = programs.at(i).title;
    }
    return *m_guideCache.insert(row, info);
}

void ChannelListModel::refreshGuide()
{
    m_guideCache.clear();
    if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(int(m_rows.size()) - 1),
                         { NowTitleRole, NowProgressRole, NextTitleRole });
}

void ChannelListModel::setSourceId(int id)
{
    if (id == m_sourceId)
        return;
    m_sourceId = id;
    emit sourceIdChanged();
    reloadSource();
    reload();
}

void ChannelListModel::setKind(const QString &kind)
{
    if (kind == m_kind)
        return;
    m_kind = kind;
    emit kindChanged();
    reload();
}

void ChannelListModel::setSortOrder(const QString &order)
{
    if (order == m_sortOrder)
        return;
    m_sortOrder = order;
    emit sortOrderChanged();
    reload();
}

void ChannelListModel::setLimit(int limit)
{
    if (limit == m_limit)
        return;
    m_limit = limit;
    emit limitChanged();
    reload();
}

void ChannelListModel::setCategoryId(int id)
{
    if (id == m_categoryId)
        return;
    m_categoryId = id;
    emit categoryIdChanged();
    reload();
}

void ChannelListModel::setSearchText(const QString &text)
{
    if (text == m_searchText)
        return;
    m_searchText = text;
    emit searchTextChanged();
    reload();
}

int ChannelListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant ChannelListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row &row = m_rows.at(index.row());
    switch (role) {
    case ChannelIdRole:
        return row.id;
    case ExtIdRole:
        return row.extId;
    case NameRole:
    case Qt::DisplayRole:
        return row.name;
    case LogoRole:
        return XtreamApi::imageUrl(row.logo);
    case NumberRole:
        return row.number;
    case FavoriteRole:
        return row.favorite;
    case EpgKeyRole:
        return row.epgKey;
    case NowTitleRole:
        return nowNext(index.row()).nowTitle;
    case NowProgressRole:
        return nowNext(index.row()).nowProgress;
    case NextTitleRole:
        return nowNext(index.row()).nextTitle;
    case YearRole:
        return row.year;
    case RatingRole:
        return row.rating;
    case WatchedRole:
        return row.watched;
    case ArchiveDaysRole:
        return row.archiveDays;
    default:
        return {};
    }
}

QHash<int, QByteArray> ChannelListModel::roleNames() const
{
    return {
        { ChannelIdRole, "channelId" }, { ExtIdRole, "extId" },   { NameRole, "name" },
        { LogoRole, "logo" },           { NumberRole, "number" }, { FavoriteRole, "favorite" },
        { EpgKeyRole, "epgKey" },       { NowTitleRole, "nowTitle" },
        { NowProgressRole, "nowProgress" }, { NextTitleRole, "nextTitle" },
        { YearRole, "year" },           { RatingRole, "rating" },
        { WatchedRole, "watched" },
        { ArchiveDaysRole, "archiveDays" },
    };
}

QVariantMap ChannelListModel::get(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return {};
    const Row &r = m_rows.at(row);
    return {
        { QStringLiteral("channelId"), r.id }, { QStringLiteral("extId"), r.extId },
        { QStringLiteral("name"), r.name },    { QStringLiteral("logo"), r.logo },
        { QStringLiteral("number"), r.number }, { QStringLiteral("favorite"), r.favorite },
        { QStringLiteral("epgKey"), r.epgKey },
        { QStringLiteral("year"), r.year },     { QStringLiteral("rating"), r.rating },
        { QStringLiteral("watched"), r.watched },
        { QStringLiteral("archiveDays"), r.archiveDays },
    };
}

int ChannelListModel::maxArchiveDays() const
{
    int days = 0;
    for (const Row &r : m_rows)
        days = qMax(days, r.archiveDays);
    return days;
}

bool ChannelListModel::canCatchup(int row, const QDateTime &start) const
{
    if (row < 0 || row >= m_rows.size())
        return false;
    return Catchup::isAvailable(m_rows.at(row).archiveDays, start, QDateTime::currentDateTimeUtc());
}

QUrl ChannelListModel::catchupUrl(int row, const QDateTime &start, const QDateTime &end) const
{
    if (!canCatchup(row, start) || m_source.id < 0)
        return {};
    const Row &r = m_rows.at(row);
    return SourceStore::catchupUrl(m_source, r.extId, r.url, r.catchupType, r.catchupSource,
                                   start, end, QDateTime::currentDateTimeUtc());
}

QUrl ChannelListModel::streamUrl(int row) const
{
    if (row < 0 || row >= m_rows.size() || m_source.id < 0)
        return {};
    const Row &r = m_rows.at(row);
    return SourceStore::streamUrl(m_source, kindFromString(m_kind), r.extId, r.url, r.extension);
}

int ChannelListModel::indexOfExtId(const QString &extId) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).extId == extId)
            return i;
    }
    return -1;
}

int ChannelListModel::indexOfNumber(int number) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).number == number)
            return i;
    }
    return -1;
}

void ChannelListModel::toggleFavorite(int row)
{
    if (row < 0 || row >= m_rows.size())
        return;
    Row &r = m_rows[row];
    SourceStore store(Database::connection());
    if (!store.setFavorite(m_sourceId, kindFromString(m_kind), r.extId, !r.favorite))
        return;
    r.favorite = !r.favorite;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, { FavoriteRole });
    emit DataNotifier::instance()->favoritesChanged(m_sourceId);
}

void ChannelListModel::reloadSource()
{
    const auto source = m_sourceId > 0 ? SourceStore(Database::connection()).source(m_sourceId)
                                       : std::nullopt;
    m_source = source.value_or(SourceRecord{});
}

void ChannelListModel::reload()
{
    QList<Row> rows;
    if (m_sourceId > 0) {
        QString sql = QStringLiteral(
                "SELECT c.id, c.ext_id, c.name, c.logo, c.url, c.number, c.epg_key, "
                "(f.ext_id IS NOT NULL), c.extension, c.year, c.rating, "
                "COALESCE(CAST(wp.position_ms AS REAL) / NULLIF(wp.duration_ms, 0), 0), "
                "c.archive_days, c.catchup_type, c.catchup_source "
                "FROM channels c "
                "LEFT JOIN favorites f ON f.source_id = c.source_id AND f.kind = c.kind "
                "AND f.ext_id = c.ext_id "
                "LEFT JOIN watch_progress wp ON wp.source_id = c.source_id AND wp.kind = c.kind "
                "AND wp.ext_id = c.ext_id "
                "WHERE c.source_id = :source AND c.kind = :kind");

        const QString search = SourceStore::searchKey(m_searchText.trimmed());
        if (!search.isEmpty()) {
            sql += QStringLiteral(" AND c.search_name LIKE :search ESCAPE '\\'");
            sql += QStringLiteral(" AND ") + SourceStore::notInHiddenCategory();
        } else if (m_categoryId == CategoryListModel::AllCategories) {
            sql += QStringLiteral(" AND ") + SourceStore::notInHiddenCategory();
        } else if (m_categoryId == CategoryListModel::FavoritesCategory) {
            sql += QStringLiteral(" AND f.ext_id IS NOT NULL");
        } else if (m_categoryId > 0) {
            sql += QStringLiteral(" AND c.category_id = :category");
        }
        sql += m_sortOrder == QLatin1String("newest") ? QStringLiteral(" ORDER BY c.position DESC")
                                                      : QStringLiteral(" ORDER BY c.position");
        if (m_limit > 0)
            sql += QStringLiteral(" LIMIT %1").arg(m_limit);

        QSqlQuery q(Database::connection());
        q.setForwardOnly(true);
        q.prepare(sql);
        q.bindValue(QStringLiteral(":source"), m_sourceId);
        q.bindValue(QStringLiteral(":kind"), m_kind);
        if (!search.isEmpty()) {
            QString escaped = search;
            escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\"))
                    .replace(QLatin1Char('%'), QLatin1String("\\%"))
                    .replace(QLatin1Char('_'), QLatin1String("\\_"));
            q.bindValue(QStringLiteral(":search"), QLatin1Char('%') + escaped + QLatin1Char('%'));
        } else if (m_categoryId > 0) {
            q.bindValue(QStringLiteral(":category"), m_categoryId);
        }

        if (q.exec()) {
            while (q.next()) {
                rows.append({ q.value(0).toInt(), q.value(1).toString(), q.value(2).toString(),
                              q.value(3).toString(), q.value(4).toString(), q.value(6).toString(),
                              q.value(5).toInt(), q.value(7).toBool(), q.value(8).toString(),
                              q.value(9).toString(), q.value(10).toString(),
                              qBound(0.0, q.value(11).toDouble(), 1.0), q.value(12).toInt(),
                              q.value(13).toString(), q.value(14).toString() });
            }
        } else {
            qWarning("Channel query failed: %s", qPrintable(q.lastError().text()));
        }
    }

    const bool countChange = rows.size() != m_rows.size();
    beginResetModel();
    m_rows = std::move(rows);
    m_guideCache.clear();
    endResetModel();
    Q_UNUSED(countChange)
    emit countChanged(); // also updates maxArchiveDays
}
