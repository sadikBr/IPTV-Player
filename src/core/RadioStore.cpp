#include "RadioStore.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace iptv {

namespace {

const QString kColumns = QStringLiteral(
        "uuid, name, url, favicon, tags, country, country_code, codec, bitrate");

// Never NULL: the text columns are NOT NULL.
QString text(const QString &value)
{
    return value.isNull() ? QStringLiteral("") : value;
}

RadioStation read(const QSqlQuery &q)
{
    RadioStation s;
    s.uuid = q.value(0).toString();
    s.name = q.value(1).toString();
    s.url = q.value(2).toString();
    s.favicon = q.value(3).toString();
    s.tags = q.value(4).toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
    s.country = q.value(5).toString();
    s.countryCode = q.value(6).toString();
    s.codec = q.value(7).toString();
    s.bitrate = q.value(8).toInt();
    return s;
}

} // namespace

RadioStore::RadioStore(QSqlDatabase db) : m_db(std::move(db)) { }

bool RadioStore::save(const RadioStation &s)
{
    // Insert, or refresh the details of a station we already have.
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
            "INSERT INTO radio_stations (%1) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(uuid) DO UPDATE SET name = excluded.name, url = excluded.url, "
            "favicon = excluded.favicon, tags = excluded.tags, country = excluded.country, "
            "country_code = excluded.country_code, codec = excluded.codec, "
            "bitrate = excluded.bitrate").arg(kColumns));
    q.addBindValue(text(s.uuid));
    q.addBindValue(text(s.name));
    q.addBindValue(text(s.url));
    q.addBindValue(text(s.favicon));
    q.addBindValue(text(s.tags.join(QLatin1Char(','))));
    q.addBindValue(text(s.country));
    q.addBindValue(text(s.countryCode));
    q.addBindValue(text(s.codec));
    q.addBindValue(s.bitrate);
    if (!q.exec()) {
        qWarning("Radio station not saved: %s", qPrintable(q.lastError().text()));
        return false;
    }
    return true;
}

bool RadioStore::setFavorite(const RadioStation &station, bool favorite)
{
    if (!save(station))
        return false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE radio_stations SET favorite = ?, favorited_at = ? WHERE uuid = ?"));
    q.addBindValue(favorite ? 1 : 0);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    q.addBindValue(station.uuid);
    return q.exec();
}

bool RadioStore::isFavorite(const QString &uuid) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT favorite FROM radio_stations WHERE uuid = ?"));
    q.addBindValue(uuid);
    return q.exec() && q.next() && q.value(0).toBool();
}

bool RadioStore::markPlayed(const RadioStation &station)
{
    if (!save(station))
        return false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE radio_stations SET played_at = ? WHERE uuid = ?"));
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    q.addBindValue(station.uuid);
    return q.exec();
}

QList<RadioStation> RadioStore::favorites() const
{
    QList<RadioStation> result;
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral("SELECT %1 FROM radio_stations WHERE favorite = 1 "
                              "ORDER BY favorited_at DESC").arg(kColumns))) {
        while (q.next())
            result.append(read(q));
    }
    return result;
}

QList<RadioStation> RadioStore::recent(int limit) const
{
    QList<RadioStation> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT %1 FROM radio_stations WHERE played_at > 0 "
                             "ORDER BY played_at DESC LIMIT ?").arg(kColumns));
    q.addBindValue(limit);
    if (q.exec()) {
        while (q.next())
            result.append(read(q));
    }
    return result;
}

} // namespace iptv
