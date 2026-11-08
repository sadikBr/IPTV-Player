#include "Catchup.h"

#include <QRegularExpression>

namespace iptv::Catchup {

namespace {

QString pathPart(const QString &value)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(value));
}

// Replaces {name}, ${name} and {name:N} placeholders.
QString fillTemplate(QString text, const QDateTime &start, const QDateTime &end,
                     const QDateTime &now)
{
    const qint64 s = start.toSecsSinceEpoch();
    const qint64 e = end.toSecsSinceEpoch();
    const qint64 n = now.toSecsSinceEpoch();
    const qint64 duration = qMax<qint64>(0, e - s);
    const QDateTime utcStart = start.toUTC();

    static const QRegularExpression re(QStringLiteral(R"(\$?\{([a-zA-Z]+)(?::(\d+))?\})"));
    QString result;
    qsizetype last = 0;
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        result += text.mid(last, m.capturedStart() - last);
        last = m.capturedEnd();
        const QString name = m.captured(1);
        const qint64 divisor = m.captured(2).isEmpty() ? 1 : qMax<qint64>(1, m.captured(2).toLongLong());
        QString value;
        if (name == QLatin1String("utc") || name == QLatin1String("start")
            || name == QLatin1String("timestamp"))
            value = QString::number(s);
        else if (name == QLatin1String("lutc") || name == QLatin1String("now"))
            value = QString::number(n);
        else if (name == QLatin1String("utcend") || name == QLatin1String("end"))
            value = QString::number(e);
        else if (name == QLatin1String("duration"))
            value = QString::number(duration / divisor);
        else if (name == QLatin1String("offset"))
            value = QString::number((n - s) / divisor);
        else if (name == QLatin1String("Y"))
            value = utcStart.toString(QStringLiteral("yyyy"));
        else if (name == QLatin1String("m"))
            value = utcStart.toString(QStringLiteral("MM"));
        else if (name == QLatin1String("d"))
            value = utcStart.toString(QStringLiteral("dd"));
        else if (name == QLatin1String("H"))
            value = utcStart.toString(QStringLiteral("HH"));
        else if (name == QLatin1String("M"))
            value = utcStart.toString(QStringLiteral("mm"));
        else if (name == QLatin1String("S"))
            value = utcStart.toString(QStringLiteral("ss"));
        else
            value = m.captured(0); // unknown: leave as is
        result += value;
    }
    result += text.mid(last);
    return result;
}

QString appendQuery(const QString &url, QString query)
{
    if (query.startsWith(QLatin1Char('?')) || query.startsWith(QLatin1Char('&')))
        query.remove(0, 1);
    return url + (url.contains(QLatin1Char('?')) ? QLatin1Char('&') : QLatin1Char('?')) + query;
}

} // namespace

bool isAvailable(int archiveDays, const QDateTime &start, const QDateTime &now)
{
    if (archiveDays <= 0 || !start.isValid() || start > now)
        return false;
    return start.secsTo(now) <= qint64(archiveDays) * 86400;
}

QTimeZone serverZone(const QString &timezoneName, qint64 timestampNow, const QString &timeNow)
{
    if (!timezoneName.isEmpty()) {
        const QTimeZone zone(timezoneName.toUtf8());
        if (zone.isValid())
            return zone;
    }
    if (timestampNow > 0 && !timeNow.isEmpty()) {
        QDateTime local = QDateTime::fromString(timeNow, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        if (local.isValid()) {
            local.setTimeZone(QTimeZone::utc());
            // Round to a quarter of an hour: the two values are read a moment apart.
            const qint64 offset = local.toSecsSinceEpoch() - timestampNow;
            const int rounded = int(qRound(offset / 900.0) * 900);
            return QTimeZone(rounded);
        }
    }
    return QTimeZone::utc();
}

QUrl xtreamUrl(const QUrl &server, const QString &username, const QString &password,
               const QString &streamId, const QDateTime &start, const QDateTime &end,
               const QTimeZone &serverZone)
{
    // A few extra minutes so the end of the program isn't cut off.
    const int minutes = int((start.secsTo(end) + 59) / 60) + 5;
    const QString when = start.toTimeZone(serverZone).toString(QStringLiteral("yyyy-MM-dd:HH-mm"));
    QString base = server.toString(QUrl::FullyEncoded);
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    return QUrl(base + QStringLiteral("/timeshift/") + pathPart(username) + QLatin1Char('/')
                        + pathPart(password) + QLatin1Char('/') + QString::number(minutes)
                        + QLatin1Char('/') + when + QLatin1Char('/') + pathPart(streamId)
                        + QStringLiteral(".ts"),
                QUrl::StrictMode);
}

QUrl m3uUrl(const QString &streamUrl, const QString &type, const QString &source,
            const QDateTime &start, const QDateTime &end, const QDateTime &now)
{
    const QString t = type.trimmed().toLower();
    const QString filled = fillTemplate(source.trimmed(), start, end, now);

    if (t.isEmpty() || t == QLatin1String("default")) {
        if (filled.isEmpty())
            return {};
        if (filled.startsWith(QLatin1Char('?')) || filled.startsWith(QLatin1Char('&')))
            return QUrl(appendQuery(streamUrl, filled));
        return QUrl(filled);
    }
    if (t == QLatin1String("append")) {
        if (filled.isEmpty())
            return {};
        if (filled.startsWith(QLatin1Char('?')) || filled.startsWith(QLatin1Char('&')))
            return QUrl(appendQuery(streamUrl, filled));
        return QUrl(streamUrl + filled);
    }
    if (t == QLatin1String("shift") || t == QLatin1String("timeshift")) {
        return QUrl(appendQuery(streamUrl, fillTemplate(QStringLiteral("utc={utc}&lutc={lutc}"),
                                                         start, end, now)));
    }
    if (t == QLatin1String("flussonic") || t == QLatin1String("flussonic-hls")
        || t == QLatin1String("flussonic-ts") || t == QLatin1String("fs")) {
        // http://host/channel/index.m3u8?token=x -> http://host/channel/index-{start}-{duration}.m3u8?token=x
        // http://host/channel/mpegts?token=x    -> http://host/channel/timeshift_abs-{start}.ts?token=x
        QUrl url(streamUrl);
        QString path = url.path();
        const int slash = int(path.lastIndexOf(QLatin1Char('/')));
        const QString file = path.mid(slash + 1);
        const QString dir = path.left(slash + 1);
        const qint64 s = start.toSecsSinceEpoch();
        const qint64 duration = qMax<qint64>(0, start.secsTo(end));
        if (file.endsWith(QLatin1String(".m3u8"))) {
            const QString name = file.left(file.size() - 5);
            path = dir + name + QLatin1Char('-') + QString::number(s) + QLatin1Char('-')
                    + QString::number(duration) + QStringLiteral(".m3u8");
        } else if (file == QLatin1String("mpegts") || file.endsWith(QLatin1String(".ts"))) {
            path = dir + QStringLiteral("timeshift_abs-") + QString::number(s) + QStringLiteral(".ts");
        } else {
            return {};
        }
        url.setPath(path);
        return url;
    }
    if (t == QLatin1String("xc")) {
        // http://host:port/[live/]user/pass/id[.ext]
        const QUrl url(streamUrl);
        QStringList parts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (!parts.isEmpty() && parts.first() == QLatin1String("live"))
            parts.removeFirst();
        if (parts.size() < 3)
            return {};
        QString id = parts.at(2);
        id = id.section(QLatin1Char('.'), 0, 0);
        QUrl server = url;
        server.setPath(QString());
        server.setQuery(QString());
        return xtreamUrl(server, parts.at(0), parts.at(1), id, start, end, QTimeZone::utc());
    }
    return {};
}

} // namespace iptv::Catchup
