#pragma once

#include <QList>
#include <QString>

namespace iptv {

// What kind of content an entry is. Only Live is shown for now; movies and
// series found in M3U playlists are stored for later milestones.
enum class ContentKind { Live, Movie, Series };

QString kindToString(ContentKind kind);
ContentKind kindFromString(const QString &kind);

struct CategoryData
{
    QString extId; // provider id (Xtream category_id, or the group name for M3U)
    QString name;
    ContentKind kind = ContentKind::Live;
};

struct ChannelData
{
    QString extId; // stable id used for favorites (Xtream stream_id, or the URL for M3U)
    QString name;
    QString logo;
    QString epgId;
    QString categoryExtId; // empty if uncategorized
    QString url;           // M3U only; Xtream URLs are built when playing
    int number = 0;
    ContentKind kind = ContentKind::Live;
};

// Everything a provider returns for one source.
struct SourceContent
{
    QList<CategoryData> categories;
    QList<ChannelData> channels;
    QString epgUrl;
};

} // namespace iptv
