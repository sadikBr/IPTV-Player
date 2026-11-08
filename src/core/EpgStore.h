#pragma once

#include <QIODevice>
#include <QList>
#include <QSqlDatabase>
#include <QString>

namespace iptv {

struct ProgramRecord
{
    qint64 start = 0; // seconds since epoch, UTC
    qint64 stop = 0;
    QString title;
    QString subTitle;
    QString description;
};

// TV guide storage: imports XMLTV data and answers "what's on" queries.
class EpgStore
{
public:
    explicit EpgStore(QSqlDatabase db);

    // Replaces the guide of a source with the XMLTV data in `device` (plain or
    // gzip). Only programs overlapping [windowStart, windowEnd] are kept.
    // Writes in small batches so other writers are never blocked for long.
    // now: the current time (tests pass a fixed one); -1 = the clock.
    bool importGuide(int sourceId, QIODevice *device, qint64 windowStart, qint64 windowEnd,
                     int *programCount = nullptr, qint64 now = -1);

    // Matches channels to guide entries (by guide id, then by name) and stores
    // the result in channels.epg_key. Called after a guide or channel update.
    bool linkChannels(int sourceId);

    // The program on air at `now` (if any) followed by the next one.
    // shiftSecs is added to the guide's times (per-provider correction).
    QList<ProgramRecord> nowNext(int sourceId, const QString &epgKey, qint64 now,
                                 qint64 shiftSecs = 0) const;
    QList<ProgramRecord> programs(int sourceId, const QString &epgKey, qint64 from, qint64 to,
                                  qint64 shiftSecs = 0) const;

    // Removes programs that ended before `before`.
    bool prune(int sourceId, qint64 before);

    // Guide ids are matched case-insensitively.
    static QString channelKey(const QString &id);

    QString lastError() const { return m_lastError; }

private:
    QSqlDatabase m_db;
    mutable QString m_lastError;
};

} // namespace iptv
