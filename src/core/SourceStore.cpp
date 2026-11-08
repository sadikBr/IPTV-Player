#include "SourceStore.h"

#include "Catchup.h"

#include <atomic>

#include "EpgStore.h"
#include "XtreamApi.h"

#include <QHash>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace iptv {

namespace {

// Null QStrings would be bound as SQL NULL; the schema uses '' instead.
QVariant text(const QString &value)
{
    return value.isNull() ? QVariant(QString(QLatin1String(""))) : QVariant(value);
}

qint64 toEpoch(const QDateTime &dt)
{
    return dt.isValid() ? dt.toSecsSinceEpoch() : 0;
}

QDateTime fromEpoch(qint64 secs)
{
    return secs > 0 ? QDateTime::fromSecsSinceEpoch(secs) : QDateTime();
}

const QString kSourceColumns = QStringLiteral(
        "id, name, type, url, username, password, output_format, epg_url, status, expires_at, "
        "max_connections, live_count, last_sync, created_at, epg_last_sync, epg_count, "
        "movie_count, series_count, epg_shift_minutes, server_timezone");

SourceRecord readSource(const QSqlQuery &q)
{
    SourceRecord s;
    s.id = q.value(0).toInt();
    s.name = q.value(1).toString();
    s.type = q.value(2).toString();
    s.url = q.value(3).toString();
    s.username = q.value(4).toString();
    s.password = q.value(5).toString();
    s.outputFormat = q.value(6).toString();
    s.epgUrl = q.value(7).toString();
    s.status = q.value(8).toString();
    s.expiresAt = fromEpoch(q.value(9).toLongLong());
    s.maxConnections = q.value(10).toInt();
    s.liveCount = q.value(11).toInt();
    s.lastSync = fromEpoch(q.value(12).toLongLong());
    s.createdAt = fromEpoch(q.value(13).toLongLong());
    s.epgLastSync = fromEpoch(q.value(14).toLongLong());
    s.epgCount = q.value(15).toInt();
    s.movieCount = q.value(16).toInt();
    s.seriesCount = q.value(17).toInt();
    s.epgShiftMinutes = q.value(18).toInt();
    s.serverTimezone = q.value(19).toString();
    return s;
}

void bindSource(QSqlQuery &q, const SourceRecord &s)
{
    q.bindValue(QStringLiteral(":name"), text(s.name));
    q.bindValue(QStringLiteral(":type"), text(s.type));
    q.bindValue(QStringLiteral(":url"), text(s.url));
    q.bindValue(QStringLiteral(":username"), text(s.username));
    q.bindValue(QStringLiteral(":password"), text(s.password));
    q.bindValue(QStringLiteral(":output_format"), text(s.outputFormat));
    q.bindValue(QStringLiteral(":epg_url"), text(s.epgUrl));
    q.bindValue(QStringLiteral(":status"), text(s.status));
    q.bindValue(QStringLiteral(":expires_at"), toEpoch(s.expiresAt));
    q.bindValue(QStringLiteral(":max_connections"), s.maxConnections);
    q.bindValue(QStringLiteral(":last_sync"), toEpoch(s.lastSync));
    q.bindValue(QStringLiteral(":server_timezone"), text(s.serverTimezone));
}

} // namespace

SourceStore::SourceStore(QSqlDatabase db) : m_db(std::move(db)) { }

QList<SourceRecord> SourceStore::sources() const
{
    QList<SourceRecord> result;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT %1 FROM sources ORDER BY created_at, id").arg(kSourceColumns))) {
        m_lastError = q.lastError().text();
        return result;
    }
    while (q.next())
        result.append(readSource(q));
    return result;
}

std::optional<SourceRecord> SourceStore::source(int id) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT %1 FROM sources WHERE id = ?").arg(kSourceColumns));
    q.addBindValue(id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return std::nullopt;
    }
    if (!q.next())
        return std::nullopt;
    return readSource(q);
}

int SourceStore::addSource(const SourceRecord &source)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
            "INSERT INTO sources (name, type, url, username, password, output_format, epg_url, "
            "status, expires_at, max_connections, last_sync, created_at, server_timezone) "
            "VALUES (:name, :type, :url, :username, :password, :output_format, :epg_url, "
            ":status, :expires_at, :max_connections, :last_sync, :created_at, :server_timezone)"));
    bindSource(q, source);
    q.bindValue(QStringLiteral(":created_at"),
                toEpoch(source.createdAt.isValid() ? source.createdAt
                                                   : QDateTime::currentDateTimeUtc()));
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool SourceStore::updateSource(const SourceRecord &source)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
            "UPDATE sources SET name = :name, type = :type, url = :url, username = :username, "
            "password = :password, output_format = :output_format, epg_url = :epg_url, "
            "status = :status, expires_at = :expires_at, max_connections = :max_connections, "
            "last_sync = :last_sync, server_timezone = :server_timezone WHERE id = :id"));
    bindSource(q, source);
    q.bindValue(QStringLiteral(":id"), source.id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool SourceStore::setEpgShift(int id, int minutes)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE sources SET epg_shift_minutes = ? WHERE id = ?"));
    q.addBindValue(minutes);
    q.addBindValue(id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool SourceStore::setOutputFormat(int id, const QString &format)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE sources SET output_format = ? WHERE id = ?"));
    q.addBindValue(format == QLatin1String("m3u8") ? QStringLiteral("m3u8") : QStringLiteral("ts"));
    q.addBindValue(id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool SourceStore::removeSource(int id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM sources WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool SourceStore::replaceContent(int sourceId, const SourceContent &content)
{
    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        return false;
    }

    auto fail = [this](const QSqlQuery &q) {
        m_lastError = q.lastError().text();
        m_db.rollback();
        return false;
    };

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM channels WHERE source_id = ?"));
    q.addBindValue(sourceId);
    if (!q.exec())
        return fail(q);
    q.prepare(QStringLiteral("DELETE FROM categories WHERE source_id = ?"));
    q.addBindValue(sourceId);
    if (!q.exec())
        return fail(q);

    // Categories: map "kind|ext_id" -> row id
    QHash<QString, qint64> categoryIds;
    q.prepare(QStringLiteral("INSERT INTO categories (source_id, kind, ext_id, name, position) "
                             "VALUES (?, ?, ?, ?, ?)"));
    int position = 0;
    for (const CategoryData &c : content.categories) {
        const QString key = kindToString(c.kind) + QLatin1Char('|') + c.extId;
        if (categoryIds.contains(key))
            continue;
        q.addBindValue(sourceId);
        q.addBindValue(kindToString(c.kind));
        q.addBindValue(text(c.extId));
        q.addBindValue(text(c.name));
        q.addBindValue(position++);
        if (!q.exec())
            return fail(q);
        categoryIds.insert(key, q.lastInsertId().toLongLong());
    }

    q.prepare(QStringLiteral(
            "INSERT INTO channels (source_id, category_id, kind, ext_id, name, search_name, logo, "
            "epg_id, url, number, position, extension, rating, year, plot, genre, archive_days, "
            "catchup_type, catchup_source) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    position = 0;
    int liveCount = 0;
    int movieCount = 0;
    int seriesCount = 0;
    for (const ChannelData &c : content.channels) {
        const QString kind = kindToString(c.kind);
        const auto category = categoryIds.constFind(kind + QLatin1Char('|') + c.categoryExtId);
        q.addBindValue(sourceId);
        q.addBindValue(category != categoryIds.constEnd() ? QVariant(*category) : QVariant());
        q.addBindValue(kind);
        q.addBindValue(text(c.extId));
        q.addBindValue(text(c.name));
        q.addBindValue(searchKey(c.name));
        q.addBindValue(text(c.logo));
        q.addBindValue(text(c.epgId));
        q.addBindValue(text(c.url));
        q.addBindValue(c.number);
        q.addBindValue(position++);
        q.addBindValue(text(c.extension));
        q.addBindValue(text(c.rating));
        q.addBindValue(text(c.year));
        q.addBindValue(text(c.plot));
        q.addBindValue(text(c.genre));
        q.addBindValue(c.archiveDays);
        q.addBindValue(text(c.catchupType));
        q.addBindValue(text(c.catchupSource));
        if (!q.exec())
            return fail(q);
        if (c.kind == ContentKind::Live)
            ++liveCount;
        else if (c.kind == ContentKind::Movie)
            ++movieCount;
        else if (c.kind == ContentKind::Series)
            ++seriesCount;
    }

    // Episodes: M3U playlists bring all of them. Xtream loads them per series
    // later, so its cached episodes are kept unless their series disappeared.
    if (!content.episodes.isEmpty()) {
        q.prepare(QStringLiteral("DELETE FROM episodes WHERE source_id = ?"));
        q.addBindValue(sourceId);
        if (!q.exec())
            return fail(q);
        q.prepare(QStringLiteral(
                "INSERT INTO episodes (source_id, series_ext_id, ext_id, season, episode, title, "
                "plot, image, duration, extension, url, position) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
        position = 0;
        for (const EpisodeData &e : content.episodes) {
            q.addBindValue(sourceId);
            q.addBindValue(text(e.seriesExtId));
            q.addBindValue(text(e.extId));
            q.addBindValue(e.season);
            q.addBindValue(e.episode);
            q.addBindValue(text(e.title));
            q.addBindValue(text(e.plot));
            q.addBindValue(text(e.image));
            q.addBindValue(e.durationSecs);
            q.addBindValue(text(e.extension));
            q.addBindValue(text(e.url));
            q.addBindValue(position++);
            if (!q.exec())
                return fail(q);
        }
    } else {
        q.prepare(QStringLiteral(
                "DELETE FROM episodes WHERE source_id = ? AND series_ext_id NOT IN "
                "(SELECT ext_id FROM channels WHERE source_id = ? AND kind = 'series')"));
        q.addBindValue(sourceId);
        q.addBindValue(sourceId);
        if (!q.exec())
            return fail(q);
    }

    q.prepare(QStringLiteral("UPDATE sources SET live_count = ?, movie_count = ?, "
                             "series_count = ?, last_sync = ?, "
                             "epg_url = CASE WHEN epg_url = '' THEN ? ELSE epg_url END "
                             "WHERE id = ?"));
    q.addBindValue(liveCount);
    q.addBindValue(movieCount);
    q.addBindValue(seriesCount);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    q.addBindValue(text(content.epgUrl));
    q.addBindValue(sourceId);
    if (!q.exec())
        return fail(q);

    // Re-attach the existing TV guide to the new channel rows.
    EpgStore epg(m_db);
    if (!epg.linkChannels(sourceId)) {
        m_lastError = epg.lastError();
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}

bool SourceStore::setCategoriesHidden(int sourceId, ContentKind kind, const QStringList &extIds,
                                      bool hidden)
{
    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(hidden ? QStringLiteral("INSERT OR IGNORE INTO hidden_categories (source_id, kind, ext_id) "
                                      "VALUES (?, ?, ?)")
                     : QStringLiteral("DELETE FROM hidden_categories WHERE source_id = ? AND kind = ? "
                                      "AND ext_id = ?"));
    for (const QString &extId : extIds) {
        q.addBindValue(sourceId);
        q.addBindValue(kindToString(kind));
        q.addBindValue(text(extId));
        if (!q.exec()) {
            m_lastError = q.lastError().text();
            m_db.rollback();
            return false;
        }
    }
    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}

QString SourceStore::cleanEpisodeTitle(const QString &title)
{
    static const QRegularExpression marker(
            QStringLiteral(R"re(\b[Ss]\d{1,2}\s*[Ee]\d{1,4}\b)re"));
    const auto m = marker.match(title);
    if (!m.hasMatch())
        return title.trimmed();
    QString rest = title.mid(m.capturedEnd()).trimmed();
    static const QRegularExpression leading(QStringLiteral(R"re(^[\s\-\x{2013}\x{2014}:|.]+)re"));
    rest.remove(leading);
    return rest.trimmed();
}

QString SourceStore::categoryGroup(const QString &name)
{
    static const QRegularExpression bracket(QStringLiteral(R"re(^\s*[\[(]\s*([^\])]{1,12}?)\s*[\])])re"));
    static const QRegularExpression prefix(QStringLiteral(R"re(^\s*([A-Za-z]{2,4})\s*[|:\x{2013}\x{2014}-]\s*\S)re"));
    auto m = bracket.match(name);
    if (!m.hasMatch())
        m = prefix.match(name);
    return m.hasMatch() ? m.captured(1).trimmed().toUpper() : QString();
}

namespace {
std::atomic<bool> g_parentalLocked { false };
}

void SourceStore::setParentalLocked(bool locked)
{
    g_parentalLocked = locked;
}

bool SourceStore::parentalLocked()
{
    return g_parentalLocked;
}

QString SourceStore::notInLockedCategory(const QString &alias)
{
    if (!g_parentalLocked)
        return QStringLiteral("1 = 1");
    return QStringLiteral("NOT EXISTS (SELECT 1 FROM categories lc JOIN locked_categories lk "
                          "ON lk.source_id = lc.source_id AND lk.kind = lc.kind AND lk.ext_id = lc.ext_id "
                          "WHERE lc.id = %1.category_id)").arg(alias);
}

QString SourceStore::progressNotLocked(const QString &alias)
{
    if (!g_parentalLocked)
        return QStringLiteral("1 = 1");
    return QStringLiteral(
            "NOT EXISTS (SELECT 1 FROM channels lch JOIN categories lc ON lc.id = lch.category_id "
            "JOIN locked_categories lk ON lk.source_id = lc.source_id AND lk.kind = lc.kind "
            "AND lk.ext_id = lc.ext_id WHERE lch.source_id = %1.source_id AND "
            "((%1.kind = 'movie' AND lch.kind = 'movie' AND lch.ext_id = %1.ext_id) OR "
            "(%1.kind = 'episode' AND lch.kind = 'series' AND lch.ext_id = %1.series_ext_id)))")
            .arg(alias);
}

QString SourceStore::notInHiddenCategory(const QString &alias)
{
    return QStringLiteral("NOT EXISTS (SELECT 1 FROM categories hc JOIN hidden_categories hh "
                          "ON hh.source_id = hc.source_id AND hh.kind = hc.kind AND hh.ext_id = hc.ext_id "
                          "WHERE hc.id = %1.category_id) AND %2").arg(alias, notInLockedCategory(alias));
}

QString SourceStore::categoryNotHidden(const QString &alias)
{
    QString sql = QStringLiteral("NOT EXISTS (SELECT 1 FROM hidden_categories hh WHERE hh.source_id = %1.source_id "
                                 "AND hh.kind = %1.kind AND hh.ext_id = %1.ext_id)").arg(alias);
    if (g_parentalLocked) {
        sql += QStringLiteral(" AND NOT EXISTS (SELECT 1 FROM locked_categories lk WHERE "
                              "lk.source_id = %1.source_id AND lk.kind = %1.kind "
                              "AND lk.ext_id = %1.ext_id)").arg(alias);
    }
    return sql;
}

// ORDER BY for a sort order (see the sortOrder property).
QString SourceStore::orderBy(const QString &order)
{
    if (order == QLatin1String("newest"))
        return QStringLiteral("c.position DESC");
    if (order == QLatin1String("name"))
        return QStringLiteral("c.search_name, c.position");
    if (order == QLatin1String("name-desc"))
        return QStringLiteral("c.search_name DESC, c.position");
    // Titles without a rating or year go last.
    if (order == QLatin1String("rating"))
        return QStringLiteral("(c.rating = '' OR CAST(c.rating AS REAL) <= 0), "
                              "CAST(c.rating AS REAL) DESC, c.search_name");
    if (order == QLatin1String("year"))
        return QStringLiteral("(CAST(c.year AS INTEGER) <= 0), CAST(c.year AS INTEGER) DESC, "
                              "c.search_name");
    if (order == QLatin1String("number"))
        return QStringLiteral("(c.number <= 0), c.number, c.position");
    return QStringLiteral("c.position");
}

bool SourceStore::setCategoryLocked(int sourceId, ContentKind kind, const QString &extId, bool locked)
{
    QSqlQuery q(m_db);
    q.prepare(locked ? QStringLiteral("INSERT OR IGNORE INTO locked_categories (source_id, kind, ext_id) "
                                      "VALUES (?, ?, ?)")
                     : QStringLiteral("DELETE FROM locked_categories WHERE source_id = ? AND kind = ? "
                                      "AND ext_id = ?"));
    q.addBindValue(sourceId);
    q.addBindValue(kindToString(kind));
    q.addBindValue(text(extId));
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

int SourceStore::lockedCategoryCount(int sourceId) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM locked_categories WHERE source_id = ?"));
    q.addBindValue(sourceId);
    if (!q.exec() || !q.next())
        return 0;
    return q.value(0).toInt();
}

bool SourceStore::setFavorite(int sourceId, ContentKind kind, const QString &extId, bool favorite)
{
    QSqlQuery q(m_db);
    if (favorite) {
        q.prepare(QStringLiteral("INSERT OR IGNORE INTO favorites (source_id, kind, ext_id, added_at) "
                                 "VALUES (?, ?, ?, ?)"));
        q.addBindValue(sourceId);
        q.addBindValue(kindToString(kind));
        q.addBindValue(text(extId));
        q.addBindValue(QDateTime::currentSecsSinceEpoch());
    } else {
        q.prepare(QStringLiteral("DELETE FROM favorites WHERE source_id = ? AND kind = ? AND ext_id = ?"));
        q.addBindValue(sourceId);
        q.addBindValue(kindToString(kind));
        q.addBindValue(text(extId));
    }
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool SourceStore::isFavorite(int sourceId, ContentKind kind, const QString &extId) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT 1 FROM favorites WHERE source_id = ? AND kind = ? AND ext_id = ?"));
    q.addBindValue(sourceId);
    q.addBindValue(kindToString(kind));
    q.addBindValue(text(extId));
    return q.exec() && q.next();
}

QUrl SourceStore::streamUrl(const SourceRecord &source, ContentKind kind, const QString &extId,
                            const QString &storedUrl, const QString &extension)
{
    if (!source.isXtream() || !storedUrl.isEmpty())
        return QUrl(storedUrl);
    const XtreamCredentials creds{ QUrl(source.url), source.username, source.password };
    switch (kind) {
    case ContentKind::Movie:
        return XtreamApi::movieUrl(creds, extId, extension);
    case ContentKind::Episode:
        return XtreamApi::episodeUrl(creds, extId, extension);
    default:
        return XtreamApi::liveStreamUrl(creds, extId, source.outputFormat);
    }
}

QUrl SourceStore::catchupUrl(const SourceRecord &source, const QString &extId,
                             const QString &storedUrl, const QString &catchupType,
                             const QString &catchupSource, const QDateTime &start,
                             const QDateTime &end, const QDateTime &now)
{
    if (source.isXtream() && storedUrl.isEmpty()) {
        const QTimeZone zone = source.serverTimezone.isEmpty()
                ? QTimeZone::utc()
                : QTimeZone(source.serverTimezone.toUtf8());
        return Catchup::xtreamUrl(QUrl(source.url), source.username, source.password, extId,
                                  start, end, zone.isValid() ? zone : QTimeZone::utc());
    }
    return Catchup::m3uUrl(storedUrl, catchupType, catchupSource, start, end, now);
}

std::optional<ChannelData> SourceStore::item(int sourceId, ContentKind kind,
                                            const QString &extId) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
            "SELECT c.ext_id, c.name, c.logo, c.epg_id, COALESCE(cat.ext_id, ''), c.url, c.number, "
            "c.extension, c.rating, c.year, c.plot, c.genre, c.archive_days, c.catchup_type, "
            "c.catchup_source FROM channels c "
            "LEFT JOIN categories cat ON cat.id = c.category_id "
            "WHERE c.source_id = ? AND c.kind = ? AND c.ext_id = ? LIMIT 1"));
    q.addBindValue(sourceId);
    q.addBindValue(kindToString(kind));
    q.addBindValue(extId);
    if (!q.exec() || !q.next())
        return std::nullopt;
    ChannelData c;
    c.kind = kind;
    c.extId = q.value(0).toString();
    c.name = q.value(1).toString();
    c.logo = q.value(2).toString();
    c.epgId = q.value(3).toString();
    c.categoryExtId = q.value(4).toString();
    c.url = q.value(5).toString();
    c.number = q.value(6).toInt();
    c.extension = q.value(7).toString();
    c.rating = q.value(8).toString();
    c.year = q.value(9).toString();
    c.plot = q.value(10).toString();
    c.genre = q.value(11).toString();
    c.archiveDays = q.value(12).toInt();
    c.catchupType = q.value(13).toString();
    c.catchupSource = q.value(14).toString();
    return c;
}

QList<EpisodeData> SourceStore::episodes(int sourceId, const QString &seriesExtId) const
{
    QList<EpisodeData> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
            "SELECT series_ext_id, ext_id, season, episode, title, plot, image, duration, "
            "extension, url FROM episodes WHERE source_id = ? AND series_ext_id = ? "
            "ORDER BY season, episode, position"));
    q.addBindValue(sourceId);
    q.addBindValue(seriesExtId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return result;
    }
    while (q.next()) {
        EpisodeData e;
        e.seriesExtId = q.value(0).toString();
        e.extId = q.value(1).toString();
        e.season = q.value(2).toInt();
        e.episode = q.value(3).toInt();
        e.title = q.value(4).toString();
        e.plot = q.value(5).toString();
        e.image = q.value(6).toString();
        e.durationSecs = q.value(7).toInt();
        e.extension = q.value(8).toString();
        e.url = q.value(9).toString();
        result.append(e);
    }
    return result;
}

bool SourceStore::replaceEpisodes(int sourceId, const QString &seriesExtId,
                                  const QList<EpisodeData> &episodes)
{
    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        return false;
    }
    QSqlQuery q(m_db);
    auto fail = [&] {
        m_lastError = q.lastError().text();
        m_db.rollback();
        return false;
    };
    q.prepare(QStringLiteral("DELETE FROM episodes WHERE source_id = ? AND series_ext_id = ?"));
    q.addBindValue(sourceId);
    q.addBindValue(seriesExtId);
    if (!q.exec())
        return fail();
    q.prepare(QStringLiteral(
            "INSERT INTO episodes (source_id, series_ext_id, ext_id, season, episode, title, "
            "plot, image, duration, extension, url, position) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    int position = 0;
    for (const EpisodeData &e : episodes) {
        q.addBindValue(sourceId);
        q.addBindValue(seriesExtId);
        q.addBindValue(text(e.extId));
        q.addBindValue(e.season);
        q.addBindValue(e.episode);
        q.addBindValue(text(e.title));
        q.addBindValue(text(e.plot));
        q.addBindValue(text(e.image));
        q.addBindValue(e.durationSecs);
        q.addBindValue(text(e.extension));
        q.addBindValue(text(e.url));
        q.addBindValue(position++);
        if (!q.exec())
            return fail();
    }
    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}

bool SourceStore::saveDetails(int sourceId, ContentKind kind, const QString &extId,
                              const VodDetails &d)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO vod_details (source_id, kind, ext_id, plot, cast_names, "
            "director, genre, release_date, rating, poster, backdrop, duration, extension, "
            "fetched_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(sourceId);
    q.addBindValue(kindToString(kind));
    q.addBindValue(text(extId));
    q.addBindValue(text(d.plot));
    q.addBindValue(text(d.cast));
    q.addBindValue(text(d.director));
    q.addBindValue(text(d.genre));
    q.addBindValue(text(d.releaseDate));
    q.addBindValue(text(d.rating));
    q.addBindValue(text(d.poster));
    q.addBindValue(text(d.backdrop));
    q.addBindValue(d.durationSecs);
    q.addBindValue(text(d.extension));
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

std::optional<VodDetails> SourceStore::details(int sourceId, ContentKind kind,
                                               const QString &extId, QDateTime *fetchedAt) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
            "SELECT plot, cast_names, director, genre, release_date, rating, poster, backdrop, "
            "duration, extension, fetched_at FROM vod_details "
            "WHERE source_id = ? AND kind = ? AND ext_id = ?"));
    q.addBindValue(sourceId);
    q.addBindValue(kindToString(kind));
    q.addBindValue(extId);
    if (!q.exec() || !q.next())
        return std::nullopt;
    VodDetails d;
    d.plot = q.value(0).toString();
    d.cast = q.value(1).toString();
    d.director = q.value(2).toString();
    d.genre = q.value(3).toString();
    d.releaseDate = q.value(4).toString();
    d.rating = q.value(5).toString();
    d.poster = q.value(6).toString();
    d.backdrop = q.value(7).toString();
    d.durationSecs = q.value(8).toInt();
    d.extension = q.value(9).toString();
    if (fetchedAt)
        *fetchedAt = fromEpoch(q.value(10).toLongLong());
    return d;
}

namespace {

const QString kProgressColumns = QStringLiteral(
        "source_id, kind, ext_id, position_ms, duration_ms, updated_at, title, image, "
        "series_ext_id, season, episode");

WatchProgress readProgress(const QSqlQuery &q)
{
    WatchProgress p;
    p.sourceId = q.value(0).toInt();
    p.kind = kindFromString(q.value(1).toString());
    p.extId = q.value(2).toString();
    p.positionMs = q.value(3).toLongLong();
    p.durationMs = q.value(4).toLongLong();
    p.updatedAt = fromEpoch(q.value(5).toLongLong());
    p.title = q.value(6).toString();
    p.image = q.value(7).toString();
    p.seriesExtId = q.value(8).toString();
    p.season = q.value(9).toInt();
    p.episode = q.value(10).toInt();
    return p;
}

} // namespace

bool SourceStore::saveProgress(const WatchProgress &p)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO watch_progress (%1) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)").arg(kProgressColumns));
    q.addBindValue(p.sourceId);
    q.addBindValue(kindToString(p.kind));
    q.addBindValue(text(p.extId));
    q.addBindValue(p.positionMs);
    q.addBindValue(p.durationMs);
    q.addBindValue(toEpoch(p.updatedAt.isValid() ? p.updatedAt : QDateTime::currentDateTimeUtc()));
    q.addBindValue(text(p.title));
    q.addBindValue(text(p.image));
    q.addBindValue(text(p.seriesExtId));
    q.addBindValue(p.season);
    q.addBindValue(p.episode);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

std::optional<WatchProgress> SourceStore::progress(int sourceId, ContentKind kind,
                                                   const QString &extId) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT %1 FROM watch_progress WHERE source_id = ? AND kind = ? "
                             "AND ext_id = ?").arg(kProgressColumns));
    q.addBindValue(sourceId);
    q.addBindValue(kindToString(kind));
    q.addBindValue(extId);
    if (!q.exec() || !q.next())
        return std::nullopt;
    return readProgress(q);
}

QList<WatchProgress> SourceStore::recentProgress(int limit) const
{
    QList<WatchProgress> result;
    QSqlQuery q(m_db);
    // Unfinished only; for a series only its most recent episode.
    q.prepare(QStringLiteral(
            "SELECT %1 FROM watch_progress p WHERE duration_ms > 0 "
            "AND position_ms < duration_ms * 0.95 AND position_ms > 0 "
            "AND (kind = 'movie' OR updated_at = (SELECT MAX(updated_at) FROM watch_progress p2 "
            "     WHERE p2.source_id = p.source_id AND p2.series_ext_id = p.series_ext_id)) "
            "AND %2 ORDER BY updated_at DESC LIMIT ?").arg(kProgressColumns, progressNotLocked(QStringLiteral("p"))));
    q.addBindValue(limit);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return result;
    }
    while (q.next())
        result.append(readProgress(q));
    return result;
}

bool SourceStore::removeProgress(int sourceId, ContentKind kind, const QString &extId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM watch_progress WHERE source_id = ? AND kind = ? AND ext_id = ?"));
    q.addBindValue(sourceId);
    q.addBindValue(kindToString(kind));
    q.addBindValue(extId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

QString SourceStore::searchKey(const QString &text)
{
    // Decompose accents (é -> e + ´) and drop the combining marks.
    const QString decomposed = text.normalized(QString::NormalizationForm_KD).toCaseFolded();
    QString result;
    result.reserve(decomposed.size());
    for (const QChar c : decomposed) {
        if (c.category() != QChar::Mark_NonSpacing)
            result.append(c);
    }
    return result;
}

} // namespace iptv
