#include "Types.h"

namespace iptv {

QString kindToString(ContentKind kind)
{
    switch (kind) {
    case ContentKind::Movie:
        return QStringLiteral("movie");
    case ContentKind::Series:
        return QStringLiteral("series");
    case ContentKind::Live:
        break;
    }
    return QStringLiteral("live");
}

ContentKind kindFromString(const QString &kind)
{
    if (kind == QLatin1String("movie"))
        return ContentKind::Movie;
    if (kind == QLatin1String("series"))
        return ContentKind::Series;
    return ContentKind::Live;
}

} // namespace iptv
