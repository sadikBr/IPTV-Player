#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace iptv {

// An internet radio station (from radio-browser.info, or a saved favorite).
struct RadioStation
{
    QString uuid;
    QString name;
    QString url;      // stream URL (url_resolved when the API gives one)
    QString favicon;
    QStringList tags;
    QString country;
    QString countryCode;
    QString codec;    // "MP3", "AAC", ...
    int bitrate = 0;  // kbit/s
    int votes = 0;
};

// The free radio-browser.info directory (about 50,000 stations).
// https://api.radio-browser.info: several mirror servers, JSON API.
namespace RadioBrowser {

// Server list (DNS-free fallback list if the lookup fails).
QUrl serversUrl();
QStringList fallbackServers();
QStringList parseServers(const QByteArray &json);

// Queries. Broken stations are left out; results sorted by votes.
QUrl topVotedUrl(const QString &server, int limit);
QUrl trendingUrl(const QString &server, int limit);
QUrl searchUrl(const QString &server, const QString &name, const QString &countryCode,
               const QString &tag, int limit);
// Counting a play helps the directory rank stations (asked for by its terms).
QUrl clickUrl(const QString &server, const QString &uuid);

QList<RadioStation> parseStations(const QByteArray &json);

} // namespace RadioBrowser
} // namespace iptv
