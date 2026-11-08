#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>

namespace iptv {

// Free, legal TV channels from the iptv-org project
// (https://github.com/iptv-org/iptv): playlists of channels that are free
// to air, by country and by category.
namespace FreeTv {

struct Choice
{
    QString code;  // country code ("FR") or category id ("news")
    QString name;
    QString flag;  // countries: emoji flag
};

QUrl countriesUrl();
QUrl categoriesUrl();

// Every channel, grouped by category.
QUrl allChannelsUrl();
QUrl countryUrl(const QString &countryCode);
QUrl categoryUrl(const QString &categoryId);

// Parse the iptv-org API lists, sorted by name. Categories without public
// channels (adult ones) are left out.
QList<Choice> parseCountries(const QByteArray &json);
QList<Choice> parseCategories(const QByteArray &json);

} // namespace FreeTv
} // namespace iptv
