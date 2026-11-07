#include "ChannelListModel.h"

#include "CategoryListModel.h"
#include "Database.h"

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
    connect(DataNotifier::instance(), &DataNotifier::sourcesChanged, this,
            &ChannelListModel::reloadSource);
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
        return row.logo;
    case NumberRole:
        return row.number;
    case FavoriteRole:
        return row.favorite;
    default:
        return {};
    }
}

QHash<int, QByteArray> ChannelListModel::roleNames() const
{
    return {
        { ChannelIdRole, "channelId" }, { ExtIdRole, "extId" },   { NameRole, "name" },
        { LogoRole, "logo" },           { NumberRole, "number" }, { FavoriteRole, "favorite" },
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
    };
}

QUrl ChannelListModel::streamUrl(int row) const
{
    if (row < 0 || row >= m_rows.size() || m_source.id < 0)
        return {};
    const Row &r = m_rows.at(row);
    return SourceStore::streamUrl(m_source, ContentKind::Live, r.extId, r.url);
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
    if (!store.setFavorite(m_sourceId, ContentKind::Live, r.extId, !r.favorite))
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
                "SELECT c.id, c.ext_id, c.name, c.logo, c.url, c.number, "
                "(f.ext_id IS NOT NULL) FROM channels c "
                "LEFT JOIN favorites f ON f.source_id = c.source_id AND f.kind = c.kind "
                "AND f.ext_id = c.ext_id "
                "WHERE c.source_id = :source AND c.kind = 'live'");

        const QString search = SourceStore::searchKey(m_searchText.trimmed());
        if (!search.isEmpty()) {
            sql += QStringLiteral(" AND c.search_name LIKE :search ESCAPE '\\'");
        } else if (m_categoryId == CategoryListModel::FavoritesCategory) {
            sql += QStringLiteral(" AND f.ext_id IS NOT NULL");
        } else if (m_categoryId > 0) {
            sql += QStringLiteral(" AND c.category_id = :category");
        }
        sql += QStringLiteral(" ORDER BY c.position");

        QSqlQuery q(Database::connection());
        q.setForwardOnly(true);
        q.prepare(sql);
        q.bindValue(QStringLiteral(":source"), m_sourceId);
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
                              q.value(3).toString(), q.value(4).toString(), q.value(5).toInt(),
                              q.value(6).toBool() });
            }
        } else {
            qWarning("Channel query failed: %s", qPrintable(q.lastError().text()));
        }
    }

    const bool countChange = rows.size() != m_rows.size();
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    if (countChange)
        emit countChanged();
}
