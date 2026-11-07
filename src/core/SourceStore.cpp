#include "SourceStore.h"

#include "XtreamApi.h"

#include <QHash>
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
        "max_connections, live_count, last_sync, created_at");

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
            "status, expires_at, max_connections, last_sync, created_at) VALUES (:name, :type, "
            ":url, :username, :password, :output_format, :epg_url, :status, :expires_at, "
            ":max_connections, :last_sync, :created_at)"));
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
            "last_sync = :last_sync WHERE id = :id"));
    bindSource(q, source);
    q.bindValue(QStringLiteral(":id"), source.id);
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
            "epg_id, url, number, position) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    position = 0;
    int liveCount = 0;
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
        if (!q.exec())
            return fail(q);
        if (c.kind == ContentKind::Live)
            ++liveCount;
    }

    q.prepare(QStringLiteral("UPDATE sources SET live_count = ?, last_sync = ?, "
                             "epg_url = CASE WHEN epg_url = '' THEN ? ELSE epg_url END "
                             "WHERE id = ?"));
    q.addBindValue(liveCount);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    q.addBindValue(text(content.epgUrl));
    q.addBindValue(sourceId);
    if (!q.exec())
        return fail(q);

    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
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
                            const QString &storedUrl)
{
    if (!source.isXtream() || kind != ContentKind::Live)
        return QUrl(storedUrl);
    const XtreamCredentials creds{ QUrl(source.url), source.username, source.password };
    return XtreamApi::liveStreamUrl(creds, extId, source.outputFormat);
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
