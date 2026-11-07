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

QList<ProgramRecord> readPrograms(QSqlQuery &q)
{
    QList<ProgramRecord> result;
    while (q.next()) {
        result.append({ q.value(0).toLongLong(), q.value(1).toLongLong(), q.value(2).toString(),
                        q.value(3).toString(), q.value(4).toString() });
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
                           int *programCount)
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
    const QList<QPair<QString, int>> statements = {
        { QStringLiteral("UPDATE channels SET epg_key = '' WHERE source_id = ?"), 1 },
        // 1. By guide id (tvg-id / epg_channel_id)
        { QStringLiteral("UPDATE channels SET epg_key = lower(trim(epg_id)) WHERE source_id = ? "
                         "AND epg_id != '' AND EXISTS (SELECT 1 FROM epg_channels ec "
                         "WHERE ec.source_id = ? AND ec.channel_key = lower(trim(channels.epg_id)))"),
          2 },
        // 2. By name, for channels without a usable id
        { QStringLiteral("UPDATE channels SET epg_key = COALESCE((SELECT ec.channel_key "
                         "FROM epg_channels ec WHERE ec.source_id = ? AND ec.name_key != '' "
                         "AND ec.name_key = channels.search_name LIMIT 1), '') "
                         "WHERE source_id = ? AND epg_key = ''"),
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

QList<ProgramRecord> EpgStore::nowNext(int sourceId, const QString &epgKey, qint64 now) const
{
    if (epgKey.isEmpty())
        return {};
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
            "SELECT start, stop, title, sub_title, description FROM epg_programs "
            "WHERE source_id = ? AND channel_key = ? AND stop > ? ORDER BY start LIMIT 2"));
    q.addBindValue(sourceId);
    q.addBindValue(epgKey);
    q.addBindValue(now);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return {};
    }
    return readPrograms(q);
}

QList<ProgramRecord> EpgStore::programs(int sourceId, const QString &epgKey, qint64 from,
                                        qint64 to) const
{
    if (epgKey.isEmpty())
        return {};
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
            "SELECT start, stop, title, sub_title, description FROM epg_programs "
            "WHERE source_id = ? AND channel_key = ? AND stop > ? AND start < ? ORDER BY start"));
    q.addBindValue(sourceId);
    q.addBindValue(epgKey);
    q.addBindValue(from);
    q.addBindValue(to);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return {};
    }
    return readPrograms(q);
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
