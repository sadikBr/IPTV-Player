#pragma once

#include "Types.h"

#include <QDateTime>
#include <QList>
#include <QSqlDatabase>
#include <QString>
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
    QString epgUrl;
    QString status;
    QDateTime expiresAt;
    int maxConnections = 0;
    int liveCount = 0;
    QDateTime lastSync;
    QDateTime createdAt;

    bool isXtream() const { return type == QLatin1String("xtream"); }
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

    // Replaces all categories and channels of a source in one transaction and
    // updates its live channel count. Favorites are kept (they use ext_id).
    bool replaceContent(int sourceId, const SourceContent &content);

    bool setFavorite(int sourceId, ContentKind kind, const QString &extId, bool favorite);
    bool isFavorite(int sourceId, ContentKind kind, const QString &extId) const;

    // URL to play: the stored one for M3U, built from credentials for Xtream.
    static QUrl streamUrl(const SourceRecord &source, ContentKind kind, const QString &extId,
                          const QString &storedUrl);

    // Lower-cased, accent-free text used for searching.
    static QString searchKey(const QString &text);

    QString lastError() const { return m_lastError; }

private:
    QSqlDatabase m_db;
    mutable QString m_lastError;
};

} // namespace iptv
