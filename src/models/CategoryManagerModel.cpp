#include "CategoryManagerModel.h"

#include "Database.h"
#include "SourceStore.h"

#include <QHash>
#include <QSqlQuery>
#include <QVariantMap>
#include <algorithm>

using namespace iptv;

namespace {
const QString kOther = QStringLiteral("Other");
}

CategoryManagerModel::CategoryManagerModel(QObject *parent) : QAbstractListModel(parent)
{
    connect(DataNotifier::instance(), &DataNotifier::contentChanged, this, [this](int sourceId) {
        if (sourceId == m_sourceId && !m_applying)
            reload();
    });
}

void CategoryManagerModel::setSourceId(int id)
{
    if (id == m_sourceId)
        return;
    m_sourceId = id;
    emit sourceIdChanged();
    reload();
}

void CategoryManagerModel::setKind(const QString &kind)
{
    if (kind == m_kind)
        return;
    m_kind = kind;
    emit kindChanged();
    m_groupFilter.clear();
    emit groupFilterChanged();
    reload();
}

void CategoryManagerModel::setFilterText(const QString &text)
{
    if (text == m_filterText)
        return;
    m_filterText = text;
    emit filterTextChanged();
    applyFilter();
}

void CategoryManagerModel::setGroupFilter(const QString &group)
{
    if (group == m_groupFilter)
        return;
    m_groupFilter = group;
    emit groupFilterChanged();
    applyFilter();
}

int CategoryManagerModel::hiddenCount() const
{
    return int(std::count_if(m_all.cbegin(), m_all.cend(), [](const Category &c) { return c.hidden; }));
}

int CategoryManagerModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant CategoryManagerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Category &c = m_all.at(m_rows.at(index.row()));
    switch (role) {
    case NameRole:
    case Qt::DisplayRole:
        return c.name;
    case CountRole:
        return c.count;
    case HiddenRole:
        return c.hidden;
    case LockedRole:
        return c.locked;
    case GroupRole:
        return c.group.isEmpty() ? kOther : c.group;
    default:
        return {};
    }
}

QHash<int, QByteArray> CategoryManagerModel::roleNames() const
{
    return { { NameRole, "name" }, { CountRole, "count" }, { HiddenRole, "hidden" }, { LockedRole, "locked" },
             { GroupRole, "group" } };
}

void CategoryManagerModel::setLocked(int row, bool locked)
{
    if (row < 0 || row >= m_rows.size() || SourceStore::parentalLocked())
        return;
    Category &c = m_all[m_rows.at(row)];
    SourceStore(Database::connection()).setCategoryLocked(m_sourceId, kindFromString(m_kind),
                                                          c.extId, locked);
    c.locked = locked;
    emit dataChanged(index(row), index(row), { LockedRole });
    emit DataNotifier::instance()->contentChanged(m_sourceId);
}

void CategoryManagerModel::setHidden(int row, bool hidden)
{
    if (row < 0 || row >= m_rows.size())
        return;
    apply({ m_rows.at(row) }, hidden);
}

void CategoryManagerModel::setGroupHidden(const QString &group, bool hidden)
{
    QList<int> indexes;
    for (int i = 0; i < m_all.size(); ++i) {
        const QString g = m_all.at(i).group.isEmpty() ? kOther : m_all.at(i).group;
        if (g == group)
            indexes.append(i);
    }
    apply(indexes, hidden);
}

void CategoryManagerModel::setListedHidden(bool hidden)
{
    apply(m_rows, hidden);
}

void CategoryManagerModel::apply(const QList<int> &indexes, bool hidden)
{
    QStringList extIds;
    for (int i : indexes) {
        if (m_all.at(i).hidden != hidden)
            extIds.append(m_all.at(i).extId);
    }
    if (extIds.isEmpty())
        return;

    SourceStore store(Database::connection());
    if (!store.setCategoriesHidden(m_sourceId, kindFromString(m_kind), extIds, hidden))
        return;
    for (int i : indexes)
        m_all[i].hidden = hidden;
    if (!hidden && m_kind == QLatin1String("live"))
        m_liveShownAgain = true;

    if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(int(m_rows.size()) - 1), { HiddenRole });
    updateStats();

    // Let the other lists refresh, without reloading this one (keeps scrolling).
    m_applying = true;
    emit DataNotifier::instance()->contentChanged(m_sourceId);
    m_applying = false;
}

void CategoryManagerModel::reload()
{
    QList<Category> all;
    if (m_sourceId > 0) {
        QSqlQuery q(Database::connection());
        q.prepare(QStringLiteral(
                "SELECT cat.ext_id, cat.name, COUNT(c.id), "
                "EXISTS (SELECT 1 FROM hidden_categories h WHERE h.source_id = cat.source_id "
                "AND h.kind = cat.kind AND h.ext_id = cat.ext_id), "
                "EXISTS (SELECT 1 FROM locked_categories l WHERE l.source_id = cat.source_id "
                "AND l.kind = cat.kind AND l.ext_id = cat.ext_id) "
                "FROM categories cat LEFT JOIN channels c ON c.category_id = cat.id "
                "WHERE cat.source_id = ? AND cat.kind = ? GROUP BY cat.id ORDER BY cat.position"));
        q.addBindValue(m_sourceId);
        q.addBindValue(m_kind);
        if (q.exec()) {
            while (q.next()) {
                Category c;
                c.extId = q.value(0).toString();
                c.name = q.value(1).toString();
                c.count = q.value(2).toInt();
                c.hidden = q.value(3).toBool();
                c.locked = q.value(4).toBool();
                // While locked, locked categories don't even show their names.
                if (c.locked && SourceStore::parentalLocked())
                    continue;
                c.searchKey = SourceStore::searchKey(c.name);
                c.group = SourceStore::categoryGroup(c.name);
                if (c.count > 0 || c.hidden)
                    all.append(c);
            }
        }
    }
    m_all = std::move(all);
    applyFilter();
    updateStats();
}

void CategoryManagerModel::applyFilter()
{
    const QString key = SourceStore::searchKey(m_filterText.trimmed());
    QList<int> rows;
    for (int i = 0; i < m_all.size(); ++i) {
        const Category &c = m_all.at(i);
        const QString group = c.group.isEmpty() ? kOther : c.group;
        if (!m_groupFilter.isEmpty() && group != m_groupFilter)
            continue;
        if (!key.isEmpty() && !c.searchKey.contains(key))
            continue;
        rows.append(i);
    }
    const bool countChange = rows.size() != m_rows.size();
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    if (countChange)
        emit countChanged();
}

void CategoryManagerModel::updateStats()
{
    struct Stat { int total = 0; int hidden = 0; };
    QHash<QString, Stat> stats;
    for (const Category &c : std::as_const(m_all)) {
        Stat &s = stats[c.group.isEmpty() ? kOther : c.group];
        ++s.total;
        if (c.hidden)
            ++s.hidden;
    }
    QList<QString> names = stats.keys();
    std::sort(names.begin(), names.end(), [&stats](const QString &a, const QString &b) {
        if (a == kOther || b == kOther)
            return b == kOther && a != kOther; // "Other" last
        return stats.value(a).total != stats.value(b).total ? stats.value(a).total > stats.value(b).total
                                                            : a < b;
    });
    QVariantList groups;
    // Only worth showing when there are real groups
    if (names.size() > 1 || (names.size() == 1 && names.first() != kOther)) {
        for (const QString &n : std::as_const(names)) {
            groups.append(QVariantMap{ { QStringLiteral("name"), n },
                                       { QStringLiteral("total"), stats.value(n).total },
                                       { QStringLiteral("hidden"), stats.value(n).hidden } });
        }
    }
    m_groups = groups;
    emit statsChanged();
}
