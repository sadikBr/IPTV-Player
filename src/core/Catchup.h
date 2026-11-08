#pragma once

#include <QDateTime>
#include <QString>
#include <QTimeZone>
#include <QUrl>

namespace iptv {

// Catch-up TV: watching programs that already aired, from the provider's
// archive. Pure functions so every URL scheme can be unit tested.
namespace Catchup {

// Whether a program can be watched from the archive: it has started, and it
// started less than archiveDays ago.
bool isAvailable(int archiveDays, const QDateTime &start, const QDateTime &now);

// The time zone of an Xtream server, from player_api.php "server_info":
// its IANA name when known, otherwise the offset between "time_now" (server
// local time) and "timestamp_now" (UTC), otherwise UTC.
QTimeZone serverZone(const QString &timezoneName, qint64 timestampNow, const QString &timeNow);

// Xtream archive URL:
//   {server}/timeshift/{user}/{pass}/{minutes}/{YYYY-MM-DD:HH-MM}/{id}.ts
// The start time is written in the server's local time.
QUrl xtreamUrl(const QUrl &server, const QString &username, const QString &password,
               const QString &streamId, const QDateTime &start, const QDateTime &end,
               const QTimeZone &serverZone);

// M3U catch-up (#EXTINF catchup="..." catchup-source="...").
//   type "default"   catchup-source is the whole URL (or is appended when it
//                    starts with ? or &)
//   type "append"    catchup-source is appended to the stream URL
//   type "shift"     ?utc={start}&lutc={now} is appended
//   type "flussonic" / "fs"  Flussonic archive URLs
//   type "xc"        an Xtream style stream URL (/user/pass/id) on the same server
// catchup-source may use {utc} {start} {lutc} {now} {utcend} {end} {duration}
// {duration:N} {offset} {Y} {m} {d} {H} {M} {S} (also written ${...}).
// Returns an empty URL when the type isn't supported.
QUrl m3uUrl(const QString &streamUrl, const QString &type, const QString &source,
            const QDateTime &start, const QDateTime &end, const QDateTime &now);

} // namespace Catchup
} // namespace iptv
