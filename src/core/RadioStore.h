#pragma once

#include "RadioBrowser.h"

#include <QSqlDatabase>

namespace iptv {

// Favorite and recently played radio stations (kept so they work even when
// the directory can't be reached).
class RadioStore
{
public:
    explicit RadioStore(QSqlDatabase db);

    bool setFavorite(const RadioStation &station, bool favorite);
    bool isFavorite(const QString &uuid) const;
    bool markPlayed(const RadioStation &station);
    QList<RadioStation> favorites() const;
    QList<RadioStation> recent(int limit) const;

private:
    bool save(const RadioStation &station);

    QSqlDatabase m_db;
};

} // namespace iptv
