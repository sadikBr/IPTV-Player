#pragma once

#include <QList>
#include <QString>

namespace iptv {

// What kind of content an entry is. Live channels, movies and series are
// stored as items (the "channels" table); episodes belong to a series.
enum class ContentKind { Live, Movie, Series, Episode };

QString kindToString(ContentKind kind);
ContentKind kindFromString(const QString &kind);

struct CategoryData
{
    QString extId; // provider id (Xtream category_id, or the group name for M3U)
    QString name;
    ContentKind kind = ContentKind::Live;
};

// A live channel, a movie or a series.
struct ChannelData
{
    QString extId; // stable id used for favorites (Xtream stream/series id, or the URL for M3U)
    QString name;
    QString logo;  // channel logo or poster
    QString epgId;
    QString categoryExtId; // empty if uncategorized
    QString url;           // M3U only; Xtream URLs are built when playing
    int number = 0;
    ContentKind kind = ContentKind::Live;

    // Movies and series
    QString extension; // container, e.g. "mkv" (Xtream movies)
    QString rating;    // e.g. "7.4"
    QString year;
    QString plot;
    QString genre;
};

struct EpisodeData
{
    QString seriesExtId;
    QString extId; // Xtream episode id, or the URL for M3U
    int season = 1;
    int episode = 0;
    QString title;
    QString plot;
    QString image;
    int durationSecs = 0;
    QString extension;
    QString url; // M3U only
};

// Extra details shown on a movie or series page.
struct VodDetails
{
    QString plot;
    QString cast;
    QString director;
    QString genre;
    QString releaseDate;
    QString rating;
    QString poster;
    QString backdrop;
    int durationSecs = 0;
    QString extension;
};

// Everything a provider returns for one source.
struct SourceContent
{
    QList<CategoryData> categories;
    QList<ChannelData> channels; // live channels, movies and series
    QList<EpisodeData> episodes; // M3U only; Xtream episodes are loaded per series
    QString epgUrl;
};

} // namespace iptv
