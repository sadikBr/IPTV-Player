#include "EpgStore.h"

#include "GzipDevice.h"
#include "SourceStore.h"
#include "XmltvParser.h"

#include <QDateTime>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace iptv {

namespace {

constexpr int kBatchSize = 2000;

QVariant text(const QString &value)
{
    return value.isNull() ? QVariant(QString(QLatin1String(""))) : QVariant(value);
}

QList<ProgramRecord> readPrograms(QSqlQuery &q, qint64 shiftSecs)
{
    QList<ProgramRecord> result;
    while (q.next()) {
        result.append({ q.value(0).toLongLong() + shiftSecs, q.value(1).toLongLong() + shiftSecs,
                        q.value(2).toString(), q.value(3).toString(), q.value(4).toString() });
    }
    return result;
}

} // namespace

EpgStore::EpgStore(QSqlDatabase db) : m_db(std::move(db)) { }

QString EpgStore::channelKey(const QString &id)
{
    // ASCII-only lower-casing, to match SQLite's lower().
    QString key = id.trimmed();
    for (QChar &c : key) {
        if (c.unicode() < 128)
            c = c.toLower();
    }
    return key;
}

bool EpgStore::importGuide(int sourceId, QIODevice *device, qint64 windowStart, qint64 windowEnd,
                           int *programCount, qint64 nowOverride)
{
    QIODevice *input = device;
    std::unique_ptr<GzipDevice> gzip;
    if (GzipDevice::isGzip(device)) {
        gzip = std::make_unique<GzipDevice>(device);
        if (!gzip->open(QIODevice::ReadOnly)) {
            m_lastError = QStringLiteral("Could not decompress the guide.");
            return false;
        }
        input = gzip.get();
    }

    // Check it is XMLTV before throwing away the current guide.
    const QByteArray head = input->peek(64 * 1024);
    if (!head.contains("<tv")) {
        m_lastError = head.trimmed().isEmpty()
                ? QStringLiteral("The TV guide is empty.")
                : QStringLiteral("This doesn't look like an XMLTV guide.");
        return false;
    }

    QSqlQuery q(m_db);
    auto failed = [this](const QSqlQuery &query) {
        m_lastError = query.lastError().text();
        m_db.rollback();
        return false;
    };

    // Start fresh.
    m_db.transaction();
    q.prepare(QStringLiteral("DELETE FROM epg_programs WHERE source_id = ?"));
    q.addBindValue(sourceId);
    if (!q.exec())
        return failed(q);
    q.prepare(QStringLiteral("DELETE FROM epg_channels WHERE source_id = ?"));
    q.addBindValue(sourceId);
    if (!q.exec())
        return failed(q);
    if (!m_db.commit())
        return failed(q);

    // Only the guide of channels that are in the lists (and not hidden): a
    // provider's guide often covers thousands of channels more. Each wanted
    // channel keeps one day of past programs, or its catch-up archive.
    // Without any channels yet (lists not loaded) everything is kept.
    QHash<QString, int> wantedIds;   // guide id -> archive days
    QHash<QString, int> wantedNames; // channel name (search key) -> archive days
    {
        QSqlQuery w(m_db);
        w.prepare(QStringLiteral("SELECT c.epg_id, c.search_name, c.archive_days FROM channels c "
                                 "WHERE c.source_id = ? AND c.kind = 'live' AND ")
                  + SourceStore::notInHiddenCategory());
        w.addBindValue(sourceId);
        if (w.exec()) {
            while (w.next()) {
                const int days = w.value(2).toInt();
                const QString id = channelKey(w.value(0).toString());
                if (!id.isEmpty())
                    wantedIds[id] = qMax(wantedIds.value(id), days);
                const QString name = w.value(1).toString();
                if (!name.isEmpty())
                    wantedNames[name] = qMax(wantedNames.value(name), days);
            }
        }
    }
    const bool filterChannels = !wantedIds.isEmpty() || !wantedNames.isEmpty();
    QHash<QString, int> relevant = wantedIds; // guide channel key -> archive days
    const qint64 now = nowOverride >= 0 ? nowOverride : QDateTime::currentSecsSinceEpoch();
    const auto pastLimit = [&](int archiveDays) {
        return qMax(windowStart, now - qint64(qMax(1, archiveDays)) * 86400);
    };

    QSqlQuery insertProgram(m_db);
    insertProgram.prepare(QStringLiteral(
            "INSERT INTO epg_programs (source_id, channel_key, start, stop, title, sub_title, "
            "description) VALUES (?, ?, ?, ?, ?, ?, ?)"));
    QSqlQuery insertChannel(m_db);
    insertChannel.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO epg_channels (source_id, channel_key, name_key) VALUES (?, ?, ?)"));

    int count = 0;
    int inBatch = 0;
    bool dbOk = true;
    QSet<QString> programKeys;

    auto beginBatch = [&] {
        if (inBatch == 0)
            m_db.transaction();
    };
    auto endBatch = [&](bool force) {
        if (inBatch > 0 && (force || inBatch >= kBatchSize)) {
            if (!m_db.commit()) {
                m_lastError = m_db.lastError().text();
                dbOk = false;
            }
            inBatch = 0;
        }
    };

    auto onChannel = [&](const EpgChannel &channel) {
        if (!dbOk)
            return;
        const QString key = channelKey(channel.id);
        if (filterChannels) {
            // Wanted by its id, or by one of its names.
            int days = -1;
            if (wantedIds.contains(key))
                days = wantedIds.value(key);
            for (const QString &name : channel.names) {
                const auto it = wantedNames.constFind(SourceStore::searchKey(name));
                if (it != wantedNames.constEnd())
                    days = qMax(days, *it);
            }
            if (days < 0)
                return;
            relevant[key] = days;
        }
        beginBatch();
        QStringList names = channel.names;
        names.prepend(QString()); // always register the id itself
        for (const QString &name : names) {
            insertChannel.addBindValue(sourceId);
            insertChannel.addBindValue(key);
            insertChannel.addBindValue(text(SourceStore::searchKey(name)));
            if (!insertChannel.exec()) {
                m_lastError = insertChannel.lastError().text();
                dbOk = false;
                return;
            }
            ++inBatch;
        }
        endBatch(false);
    };

    auto onProgram = [&](const EpgProgram &p) {
        if (!dbOk)
            return false;
        if (p.stop <= windowStart || p.start >= windowEnd)
            return true;
        const QString key = channelKey(p.channelId);
        if (filterChannels) {
            const auto it = relevant.constFind(key);
            if (it == relevant.constEnd() || p.stop <= pastLimit(*it))
                return true;
        }
        programKeys.insert(key);
        beginBatch();
        insertProgram.addBindValue(sourceId);
        insertProgram.addBindValue(key);
        insertProgram.addBindValue(p.start);
        insertProgram.addBindValue(p.stop);
        insertProgram.addBindValue(text(p.title));
        insertProgram.addBindValue(text(p.subTitle));
        insertProgram.addBindValue(text(p.description));
        if (!insertProgram.exec()) {
            m_lastError = insertProgram.lastError().text();
            dbOk = false;
            return false;
        }
        ++count;
        ++inBatch;
        endBatch(false);
        return true;
    };

    QString parseError;
    const bool parsed = XmltvParser::parse(input, onChannel, onProgram, &parseError);
    endBatch(true);
    if (!dbOk) {
        m_db.rollback();
        return false;
    }
    if (!parsed) {
        m_lastError = parseError;
        return false;
    }

    // Some guides have programmes without a matching <channel> element.
    m_db.transaction();
    for (const QString &key : std::as_const(programKeys)) {
        insertChannel.addBindValue(sourceId);
        insertChannel.addBindValue(key);
        insertChannel.addBindValue(QString(QLatin1String("")));
        if (!insertChannel.exec())
            return failed(insertChannel);
    }
    if (!linkChannels(sourceId))
        return false;
    q.prepare(QStringLiteral("UPDATE sources SET epg_last_sync = ?, epg_count = ? WHERE id = ?"));
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    q.addBindValue(count);
    q.addBindValue(sourceId);
    if (!q.exec())
        return failed(q);
    if (!m_db.commit())
        return failed(q);

    if (programCount)
        *programCount = count;
    return true;
}

bool EpgStore::linkChannels(int sourceId)
{
    // Each statement with the number of times it uses the source id.
    // Only live channels have a guide; films and series are left alone.
    const QList<QPair<QString, int>> statements = {
        { QStringLiteral("UPDATE channels SET epg_key = '' WHERE source_id = ? AND epg_key != ''"),
          1 },
        // 1. By guide id (tvg-id / epg_channel_id)
        { QStringLiteral("UPDATE channels SET epg_key = lower(trim(epg_id)) WHERE source_id = ? "
                         "AND kind = 'live' AND epg_id != '' AND EXISTS (SELECT 1 FROM "
                         "epg_channels ec WHERE ec.source_id = ? "
                         "AND ec.channel_key = lower(trim(channels.epg_id)))"),
          2 },
        // 2. By name, for channels without a usable id. The test for an empty name is
        // on the channel so the lookup uses the (source_id, name_key) index.
        { QStringLiteral("UPDATE channels SET epg_key = COALESCE((SELECT ec.channel_key "
                         "FROM epg_channels ec INDEXED BY idx_epg_channels_name WHERE ec.source_id = ? "
                         "AND ec.name_key = channels.search_name LIMIT 1), '') "
                         "WHERE source_id = ? AND kind = 'live' AND epg_key = '' "
                         "AND search_name != ''"),
          2 },
    };
    QSqlQuery q(m_db);
    for (const auto &[sql, binds] : statements) {
        q.prepare(sql);
        for (int i = 0; i < binds; ++i)
            q.addBindValue(sourceId);
        if (!q.exec()) {
            m_lastError = q.lastError().text();
            return false;
        }
    }
    return true;
}

QList<ProgramRecord> EpgStore::nowNext(int sourceId, const QString &epgKey, qint64 now,
                                       qint64 shiftSecs) const
{
    if (epgKey.isEmpty())
        return {};
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
            "SELECT start, stop, title, sub_title, description FROM epg_programs "
            "WHERE source_id = ? AND channel_key = ? AND stop > ? ORDER BY start LIMIT 2"));
    q.addBindValue(sourceId);
    q.addBindValue(epgKey);
    q.addBindValue(now - shiftSecs);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return {};
    }
    return readPrograms(q, shiftSecs);
}

QList<ProgramRecord> EpgStore::programs(int sourceId, const QString &epgKey, qint64 from,
                                        qint64 to, qint64 shiftSecs) const
{
    if (epgKey.isEmpty())
        return {};
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
            "SELECT start, stop, title, sub_title, description FROM epg_programs "
            "WHERE source_id = ? AND channel_key = ? AND stop > ? AND start < ? ORDER BY start"));
    q.addBindValue(sourceId);
    q.addBindValue(epgKey);
    q.addBindValue(from - shiftSecs);
    q.addBindValue(to - shiftSecs);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return {};
    }
    return readPrograms(q, shiftSecs);
}

bool EpgStore::prune(int sourceId, qint64 before)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM epg_programs WHERE source_id = ? AND stop < ?"));
    q.addBindValue(sourceId);
    q.addBindValue(before);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

} // namespace iptv
