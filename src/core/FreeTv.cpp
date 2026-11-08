#include "FreeTv.h"

#include <QCollator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace iptv::FreeTv {

namespace {

const QString kBase = QStringLiteral("https://iptv-org.github.io/");

void sortByName(QList<Choice> &list)
{
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(list.begin(), list.end(), [&collator](const Choice &a, const Choice &b) {
        return collator.compare(a.name, b.name) < 0;
    });
}

} // namespace

QUrl countriesUrl() { return QUrl(kBase + QStringLiteral("api/countries.json")); }
QUrl categoriesUrl() { return QUrl(kBase + QStringLiteral("api/categories.json")); }
QUrl allChannelsUrl() { return QUrl(kBase + QStringLiteral("iptv/index.m3u")); }

QUrl countryUrl(const QString &countryCode)
{
    return QUrl(kBase + QStringLiteral("iptv/countries/%1.m3u").arg(countryCode.toLower()));
}

QUrl categoryUrl(const QString &categoryId)
{
    return QUrl(kBase + QStringLiteral("iptv/categories/%1.m3u").arg(categoryId.toLower()));
}

QList<Choice> parseCountries(const QByteArray &json)
{
    QList<Choice> result;
    const QJsonArray array = QJsonDocument::fromJson(json).array();
    for (const QJsonValue &v : array) {
        const QJsonObject o = v.toObject();
        Choice c { o.value(QLatin1String("code")).toString(), o.value(QLatin1String("name")).toString(),
                   o.value(QLatin1String("flag")).toString() };
        if (!c.code.isEmpty() && !c.name.isEmpty())
            result.append(c);
    }
    sortByName(result);
    return result;
}

QList<Choice> parseCategories(const QByteArray &json)
{
    QList<Choice> result;
    const QJsonArray array = QJsonDocument::fromJson(json).array();
    for (const QJsonValue &v : array) {
        const QJsonObject o = v.toObject();
        Choice c { o.value(QLatin1String("id")).toString(), o.value(QLatin1String("name")).toString(), {} };
        // iptv-org publishes no adult channels: that playlist is always empty.
        if (c.code.isEmpty() || c.name.isEmpty() || c.code == QLatin1String("xxx"))
            continue;
        result.append(c);
    }
    sortByName(result);
    return result;
}

} // namespace iptv::FreeTv
