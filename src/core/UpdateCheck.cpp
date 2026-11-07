#include "UpdateCheck.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>

namespace iptv::Updates {

namespace {

QList<int> versionParts(QString v)
{
    v = v.trimmed();
    if (v.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        v.remove(0, 1);
    const int cut = v.indexOf(QRegularExpression(QStringLiteral("[-+]")));
    if (cut >= 0)
        v.truncate(cut);
    QList<int> parts;
    for (const QString &p : v.split(QLatin1Char('.')))
        parts << p.toInt();
    while (parts.size() > 1 && parts.last() == 0)
        parts.removeLast();
    return parts;
}

} // namespace

int compareVersions(const QString &a, const QString &b)
{
    const QList<int> pa = versionParts(a);
    const QList<int> pb = versionParts(b);
    for (int i = 0; i < qMax(pa.size(), pb.size()); ++i) {
        const int x = i < pa.size() ? pa.at(i) : 0;
        const int y = i < pb.size() ? pb.at(i) : 0;
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

ReleaseInfo parseRelease(const QByteArray &json, const QString &assetName)
{
    ReleaseInfo info;
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    QString tag = o.value(QLatin1String("tag_name")).toString().trimmed();
    if (tag.isEmpty() || o.value(QLatin1String("draft")).toBool()
        || o.value(QLatin1String("prerelease")).toBool())
        return info;
    if (tag.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        tag.remove(0, 1);
    info.version = tag;
    info.notes = o.value(QLatin1String("body")).toString();
    info.pageUrl = QUrl(o.value(QLatin1String("html_url")).toString());

    if (!assetName.isEmpty()) {
        const QJsonArray assets = o.value(QLatin1String("assets")).toArray();
        for (const QJsonValue &v : assets) {
            const QJsonObject a = v.toObject();
            if (a.value(QLatin1String("name")).toString() != assetName)
                continue;
            info.assetName = assetName;
            info.assetUrl = QUrl(a.value(QLatin1String("browser_download_url")).toString());
            info.assetSize = qint64(a.value(QLatin1String("size")).toDouble());
            const QString digest = a.value(QLatin1String("digest")).toString();
            if (digest.startsWith(QLatin1String("sha256:")))
                info.sha256 = digest.mid(7).toLower().toLatin1();
            break;
        }
    }
    info.valid = true;
    return info;
}

} // namespace iptv::Updates
