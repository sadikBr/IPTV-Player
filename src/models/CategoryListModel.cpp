#include "CategoryListModel.h"

#include "Database.h"

#include <QSqlQuery>

using namespace iptv;

CategoryListModel::CategoryListModel(QObject *parent) : QAbstractListModel(parent)
{
    auto reloadIfMine = [this](int sourceId) {
        if (sourceId == m_sourceId)
            reload();
    };
    connect(DataNotifier::instance(), &DataNotifier::contentChanged, this, reloadIfMine);
    connect(DataNotifier::instance(), &DataNotifier::favoritesChanged, this, reloadIfMine);
}

void CategoryListModel::setSourceId(int id)
{
    if (id == m_sourceId)
        return;
    m_sourceId = id;
    emit sourceIdChanged();
    reload();
}

int CategoryListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant CategoryListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row &row = m_rows.at(index.row());
    switch (role) {
    case CategoryIdRole:
        return row.id;
    case NameRole:
    case Qt::DisplayRole:
        return row.name;
    case CountRole:
        return row.count;
    default:
        return {};
    }
}

QHash<int, QByteArray> CategoryListModel::roleNames() const
{
    return { { CategoryIdRole, "categoryId" }, { NameRole, "name" }, { CountRole, "count" } };
}

int CategoryListModel::indexOf(int categoryId) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).id == categoryId)
            return i;
    }
    return -1;
}

QString CategoryListModel::nameOf(int categoryId) const
{
    const int i = indexOf(categoryId);
    return i >= 0 ? m_rows.at(i).name : QString();
}

void CategoryListModel::reload()
{
    QList<Row> rows;
    if (m_sourceId > 0) {
        QSqlDatabase db = Database::connection();
        QSqlQuery q(db);

        int total = 0;
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM channels WHERE source_id = ? AND kind = 'live'"));
        q.addBindValue(m_sourceId);
        if (q.exec() && q.next())
            total = q.value(0).toInt();

        int favorites = 0;
        q.prepare(QStringLiteral(
                "SELECT COUNT(*) FROM channels c JOIN favorites f ON f.source_id = c.source_id "
                "AND f.kind = c.kind AND f.ext_id = c.ext_id "
                "WHERE c.source_id = ? AND c.kind = 'live'"));
        q.addBindValue(m_sourceId);
        if (q.exec() && q.next())
            favorites = q.value(0).toInt();

        rows.append({ AllCategories, tr("All channels"), total });
        rows.append({ FavoritesCategory, tr("Favorites"), favorites });

        // Empty categories are hidden.
        q.prepare(QStringLiteral(
                "SELECT cat.id, cat.name, COUNT(c.id) FROM categories cat "
                "JOIN channels c ON c.category_id = cat.id "
                "WHERE cat.source_id = ? AND cat.kind = 'live' "
                "GROUP BY cat.id ORDER BY cat.position"));
        q.addBindValue(m_sourceId);
        if (q.exec()) {
            while (q.next())
                rows.append({ q.value(0).toInt(), q.value(1).toString(), q.value(2).toInt() });
        }
    }

    const bool countChange = rows.size() != m_rows.size();
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    if (countChange)
        emit countChanged();
}
