#pragma once

#include "Types.h"

#include <QDateTime>
#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <optional>

namespace iptv {

struct SourceRecord
{
    int id = -1;
    QString name;
    QString type; // "xtream" | "m3u"
    QString url;  // Xtream server, or the M3U playlist URL
    QString username;
    QString password;
    QString outputFormat = QStringLiteral("ts"); // Xtream: "ts" or "m3u8"
    QString serverTimezone; // Xtream: time zone id of the server (catch-up links)
    QString epgUrl;
    QString status;
    QDateTime expiresAt;
    int maxConnections = 0;
    int liveCount = 0;
    QDateTime lastSync;
    QDateTime createdAt;
    QDateTime epgLastSync;
    int epgCount = 0;
    int movieCount = 0;
    int seriesCount = 0;
    int epgShiftMinutes = 0; // added to all guide times (fixes providers with wrong time zones)

    bool isXtream() const { return type == QLatin1String("xtream"); }
};

// Where the user stopped watching a movie or episode.
struct WatchProgress
{
    int sourceId = -1;
    ContentKind kind = ContentKind::Movie; // Movie or Episode
    QString extId;
    qint64 positionMs = 0;
    qint64 durationMs = 0;
    QDateTime updatedAt;
    QString title;
    QString image;
    QString seriesExtId; // episodes only
    int season = 0;
    int episode = 0;

    bool finished() const { return durationMs > 0 && positionMs >= durationMs * 0.95; }
};

// Reads and writes sources and their content. Each instance uses the
// connection it was given, so it can run on any thread.
class SourceStore
{
public:
    explicit SourceStore(QSqlDatabase db);

    QList<SourceRecord> sources() const;
    std::optional<SourceRecord> source(int id) const;

    int addSource(const SourceRecord &source); // returns the new id, or -1
    bool updateSource(const SourceRecord &source);
    bool removeSource(int id);
    bool setEpgShift(int id, int minutes);
    // Xtream live streams: "ts" or "m3u8" (HLS)
    bool setOutputFormat(int id, const QString &format);

    // Replaces all categories and channels of a source in one transaction and
    // updates its live channel count. Favorites are kept (they use ext_id).
    bool replaceContent(int sourceId, const SourceContent &content);

    // One channel, movie or series by its provider id.
    std::optional<ChannelData> item(int sourceId, ContentKind kind, const QString &extId) const;

    // Episodes of a series, in season/episode order.
    QList<EpisodeData> episodes(int sourceId, const QString &seriesExtId) const;
    // Replaces the cached episodes of one series (Xtream loads them on demand).
    bool replaceEpisodes(int sourceId, const QString &seriesExtId, const QList<EpisodeData> &episodes);

    // Cached details of a movie or series (Xtream get_vod_info / get_series_info).
    bool saveDetails(int sourceId, ContentKind kind, const QString &extId, const VodDetails &details);
    std::optional<VodDetails> details(int sourceId, ContentKind kind, const QString &extId,
                                      QDateTime *fetchedAt = nullptr) const;

    bool saveProgress(const WatchProgress &progress);
    std::optional<WatchProgress> progress(int sourceId, ContentKind kind, const QString &extId) const;
    // Unfinished movies and episodes, most recent first (all sources).
    QList<WatchProgress> recentProgress(int limit) const;
    bool removeProgress(int sourceId, ContentKind kind, const QString &extId);

    // Hidden categories: left out of category lists, "All" and search.
    bool setCategoriesHidden(int sourceId, ContentKind kind, const QStringList &extIds, bool hidden);
    // Episode title without the series name and SxxEyy prefix providers often
    // repeat: "EN| Show [4K] - S01E02 - The Title" -> "The Title". Empty if
    // nothing is left.
    static QString cleanEpisodeTitle(const QString &title);

    // Language / region group from a category name: "[EN] KIDS" -> "EN",
    // "UK | News" -> "UK", "FR: Sport" -> "FR". Empty when there is none.
    static QString categoryGroup(const QString &name);

    // SQL condition: true when channel row `alias` is not in a hidden category.
    static QString notInHiddenCategory(const QString &alias = QStringLiteral("c"));
    // Parental control: while locked, content of locked categories is left
    // out everywhere (notInHiddenCategory / categoryNotHidden include it).
    // ORDER BY for channels (alias c) in a sort order: "default", "newest",
    // "name", "name-desc", "rating", "year", "number".
    static QString orderBy(const QString &order);
    static void setParentalLocked(bool locked);
    static bool parentalLocked();
    // "1 = 1" when unlocked. For channels (alias.category_id) ...
    static QString notInLockedCategory(const QString &alias = QStringLiteral("c"));
    // ... and for watch_progress rows (movies, and episodes by their series).
    static QString progressNotLocked(const QString &alias = QStringLiteral("p"));
    bool setCategoryLocked(int sourceId, ContentKind kind, const QString &extId, bool locked);
    int lockedCategoryCount(int sourceId) const;
    // SQL condition: true when category row `alias` is not hidden.
    static QString categoryNotHidden(const QString &alias = QStringLiteral("cat"));

    bool setFavorite(int sourceId, ContentKind kind, const QString &extId, bool favorite);
    bool isFavorite(int sourceId, ContentKind kind, const QString &extId) const;

    // URL to play: the stored one for M3U, built from credentials for Xtream.
    static QUrl streamUrl(const SourceRecord &source, ContentKind kind, const QString &extId,
                          const QString &storedUrl, const QString &extension = {});
    // Catch-up: the archive URL of a live channel for a program that started
    // at start and ends at end. Empty if the channel has no archive.
    static QUrl catchupUrl(const SourceRecord &source, const QString &extId,
                           const QString &storedUrl, const QString &catchupType,
                           const QString &catchupSource, const QDateTime &start,
                           const QDateTime &end, const QDateTime &now);

    // Lower-cased, accent-free text used for searching.
    static QString searchKey(const QString &text);

    QString lastError() const { return m_lastError; }

private:
    QSqlDatabase m_db;
    mutable QString m_lastError;
};

} // namespace iptv
