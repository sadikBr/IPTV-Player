#include "M3uParser.h"

#include <QHash>
#include <QRegularExpression>
#include <QUrl>

namespace iptv {

namespace {

struct Attributes
{
    QHash<QString, QString> values;
    QString title;
};

// Splits `-1 tvg-id="a" group-title="b, c",Title` into attributes and title.
Attributes parseExtInf(const QString &line)
{
    Attributes result;

    // Find the comma that separates attributes from the title, ignoring
    // commas inside quoted attribute values.
    bool inQuotes = false;
    int titleComma = -1;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (c == QLatin1Char('"'))
            inQuotes = !inQuotes;
        else if (c == QLatin1Char(',') && !inQuotes) {
            titleComma = i;
            break;
        }
    }

    const QString attrPart = titleComma >= 0 ? line.left(titleComma) : line;
    result.title = titleComma >= 0 ? line.mid(titleComma + 1).trimmed() : QString();

    static const QRegularExpression attrRe(
            QStringLiteral(R"re(([A-Za-z0-9_\-]+)\s*=\s*(?:"([^"]*)"|([^\s"]+)))re"));
    auto it = attrRe.globalMatch(attrPart);
    while (it.hasNext()) {
        const auto m = it.next();
        // Group 2 is a quoted value, group 3 an unquoted one.
        const QString value = m.capturedStart(3) >= 0 ? m.captured(3) : m.captured(2);
        result.values.insert(m.captured(1).toLower(), value.trimmed());
    }
    return result;
}

ContentKind classifyUrl(const QString &url)
{
    const QString path = QUrl(url).path().toLower();
    if (path.contains(QLatin1String("/movie/")) || path.contains(QLatin1String("/movies/")))
        return ContentKind::Movie;
    if (path.contains(QLatin1String("/series/")))
        return ContentKind::Series;
    return ContentKind::Live;
}

struct EpisodeName
{
    QString show;
    int season = 0; // 0: not found in the name
    int episode = 0;
    QString title;
};

// "Show Name S01 E02 Title" / "Show.Name.S1E2" -> show, season, episode
EpisodeName parseEpisodeName(const QString &name)
{
    static const QRegularExpression re(
            QStringLiteral(R"re(^(.*?)[\s._\-:|]*\b[Ss](\d{1,2})[\s._\-]*[Ee](\d{1,4})\b[\s._\-:|]*(.*)$)re"));
    const auto m = re.match(name);
    if (!m.hasMatch() || m.captured(1).trimmed().isEmpty())
        return { name.trimmed(), 0, 0, QString() };
    QString show = m.captured(1);
    show.replace(QLatin1Char('.'), QLatin1Char(' '));
    return { show.trimmed(), m.captured(2).toInt(), m.captured(3).toInt(), m.captured(4).trimmed() };
}

// "Movie Title (2024)" -> "2024"
QString yearFromName(const QString &name)
{
    static const QRegularExpression re(QStringLiteral(R"re([\(\[]((?:19|20)\d{2})[\)\]])re"));
    const auto m = re.match(name);
    return m.hasMatch() ? m.captured(1) : QString();
}

QString headerEpgUrl(const QString &line)
{
    const Attributes attrs = parseExtInf(line.mid(7)); // after "#EXTM3U"
    for (const char *key : { "url-tvg", "x-tvg-url", "tvg-url" }) {
        const QString value = attrs.values.value(QLatin1String(key));
        if (!value.isEmpty())
            return value.section(QLatin1Char(','), 0, 0).trimmed(); // may list several
    }
    return {};
}

} // namespace

bool M3uParser::looksLikeM3u(const QByteArray &data)
{
    QByteArray head = data.left(4096);
    if (head.startsWith("\xEF\xBB\xBF"))
        head.remove(0, 3);
    head = head.trimmed();
    return head.startsWith("#EXTM3U") || head.contains("#EXTINF");
}

SourceContent M3uParser::parse(const QByteArray &data)
{
    SourceContent content;

    QHash<QString, int> categoryIndex; // "kind|name" -> index in content.categories
    QHash<int, int> numberByKind;      // running channel number per kind
    QHash<QString, int> seriesIndex;   // series ext id -> index in content.channels
    QHash<QString, int> episodeCount;  // series ext id -> episodes without SxxEyy

    bool havePending = false;
    Attributes pending;
    QString pendingGroup;
    // #EXTVLCOPT:http-user-agent=... / http-referrer=... before the URL
    QString pendingUserAgent;
    QString pendingReferrer;
    // Catch-up defaults for the whole playlist (#EXTM3U catchup="..." ...)
    QString defaultCatchupType;
    QString defaultCatchupSource;
    int defaultCatchupDays = 0;

    qsizetype pos = 0;
    if (data.startsWith("\xEF\xBB\xBF"))
        pos = 3;

    while (pos < data.size()) {
        qsizetype end = data.indexOf('\n', pos);
        if (end < 0)
            end = data.size();
        QString line = QString::fromUtf8(data.constData() + pos, end - pos).trimmed();
        pos = end + 1;

        if (line.isEmpty())
            continue;

        if (line.startsWith(QLatin1Char('#'))) {
            if (line.startsWith(QLatin1String("#EXTINF:"), Qt::CaseInsensitive)) {
                pending = parseExtInf(line.mid(8));
                pendingGroup.clear();
                pendingUserAgent.clear();
                pendingReferrer.clear();
                havePending = true;
            } else if (line.startsWith(QLatin1String("#EXTVLCOPT:"), Qt::CaseInsensitive)) {
                const QString option = line.mid(11).trimmed();
                const qsizetype eq = option.indexOf(QLatin1Char('='));
                if (eq > 0) {
                    const QString key = option.left(eq).trimmed().toLower();
                    const QString value = option.mid(eq + 1).trimmed();
                    if (key == QLatin1String("http-user-agent"))
                        pendingUserAgent = value;
                    else if (key == QLatin1String("http-referrer") || key == QLatin1String("http-referer"))
                        pendingReferrer = value;
                }
            } else if (line.startsWith(QLatin1String("#EXTGRP:"), Qt::CaseInsensitive)) {
                pendingGroup = line.mid(8).trimmed();
            } else if (line.startsWith(QLatin1String("#EXTM3U"), Qt::CaseInsensitive)) {
                if (content.epgUrl.isEmpty())
                    content.epgUrl = headerEpgUrl(line);
                const Attributes header = parseExtInf(line.mid(7));
                defaultCatchupType = header.values.value(QStringLiteral("catchup"));
                defaultCatchupSource = header.values.value(QStringLiteral("catchup-source"));
                defaultCatchupDays = header.values.value(QStringLiteral("catchup-days")).toInt();
            }
            // Other directives (#KODIPROP, ...) are ignored for now.
            continue;
        }

        // A URL line, maybe with headers after a "|" (Kodi style):
        // http://host/stream.m3u8|User-Agent=Foo&Referer=https%3A%2F%2Fsite%2F
        ChannelData channel;
        channel.extId = line;
        QString url = line;
        const qsizetype pipe = line.indexOf(QLatin1Char('|'));
        if (pipe > 0) {
            url = line.left(pipe).trimmed();
            const QStringList pairs = line.mid(pipe + 1).split(QLatin1Char('&'), Qt::SkipEmptyParts);
            for (const QString &pair : pairs) {
                const qsizetype eq = pair.indexOf(QLatin1Char('='));
                if (eq <= 0)
                    continue;
                const QString key = pair.left(eq).trimmed().toLower();
                const QString value = QUrl::fromPercentEncoding(pair.mid(eq + 1).trimmed().toUtf8());
                if (key == QLatin1String("user-agent"))
                    channel.httpUserAgent = value;
                else if (key == QLatin1String("referer") || key == QLatin1String("referrer"))
                    channel.httpReferrer = value;
            }
        }
        channel.url = url;
        channel.kind = classifyUrl(url);
        if (channel.httpUserAgent.isEmpty())
            channel.httpUserAgent = pendingUserAgent;
        if (channel.httpReferrer.isEmpty())
            channel.httpReferrer = pendingReferrer;
        pendingUserAgent.clear();
        pendingReferrer.clear();

        QString group;
        if (havePending) {
            const auto &v = pending.values;
            channel.name = pending.title;
            if (channel.name.isEmpty())
                channel.name = v.value(QStringLiteral("tvg-name"));
            channel.logo = v.value(QStringLiteral("tvg-logo"));
            if (channel.httpUserAgent.isEmpty())
                channel.httpUserAgent = v.value(QStringLiteral("http-user-agent"),
                                                v.value(QStringLiteral("user-agent")));
            if (channel.httpReferrer.isEmpty()) {
                for (const char *key : { "http-referrer", "http-referer", "referrer", "referer" }) {
                    channel.httpReferrer = v.value(QLatin1String(key));
                    if (!channel.httpReferrer.isEmpty())
                        break;
                }
            }
            channel.epgId = v.value(QStringLiteral("tvg-id"));
            group = v.value(QStringLiteral("group-title"));
            if (group.isEmpty())
                group = pendingGroup;
            // "Animation;Kids" (iptv-org lists several categories): the first one.
            static const QRegularExpression multiGroup(QStringLiteral("^[\\w&]+(?:;[\\w&]+)+$"));
            if (multiGroup.match(group).hasMatch())
                group = group.section(QLatin1Char(';'), 0, 0);
            bool ok = false;
            const int chno = v.value(QStringLiteral("tvg-chno")).toInt(&ok);
            if (ok && chno > 0)
                channel.number = chno;

            // Catch-up: catchup="default|append|shift|flussonic|xc",
            // catchup-days="7", catchup-source="..." (older lists: timeshift="7")
            if (channel.kind == ContentKind::Live) {
                channel.catchupType = v.value(QStringLiteral("catchup"), defaultCatchupType);
                if (channel.catchupType.isEmpty())
                    channel.catchupType = v.value(QStringLiteral("catchup-type"));
                channel.catchupSource = v.value(QStringLiteral("catchup-source"), defaultCatchupSource);
                int days = v.value(QStringLiteral("catchup-days")).toInt();
                if (days <= 0)
                    days = v.value(QStringLiteral("timeshift")).toInt();
                if (days <= 0)
                    days = v.value(QStringLiteral("tvg-rec")).toInt();
                if (days <= 0)
                    days = defaultCatchupDays;
                // "shift" and "timeshift" lists often give the days only.
                if (days > 0 && channel.catchupType.isEmpty())
                    channel.catchupType = channel.catchupSource.isEmpty() ? QStringLiteral("shift")
                                                                          : QStringLiteral("default");
                if (!channel.catchupType.isEmpty() && days <= 0)
                    days = 1;
                if (channel.catchupType.compare(QLatin1String("vod"), Qt::CaseInsensitive) == 0)
                    days = 0;
                channel.archiveDays = days;
                if (days <= 0) {
                    channel.catchupType.clear();
                    channel.catchupSource.clear();
                }
            }
        }
        if (channel.name.isEmpty()) {
            const QUrl parsed(url);
            channel.name = parsed.fileName().isEmpty() ? url : parsed.fileName();
        }

        if (!group.isEmpty()) {
            const QString key = kindToString(channel.kind) + QLatin1Char('|') + group;
            if (!categoryIndex.contains(key)) {
                categoryIndex.insert(key, int(content.categories.size()));
                content.categories.append({ group, group, channel.kind });
            }
            channel.categoryExtId = group;
        }

        if (channel.kind == ContentKind::Series) {
            // Each entry is an episode; group them into one series item per show.
            const EpisodeName parsed = parseEpisodeName(channel.name);
            const QString seriesId = QStringLiteral("m3u-series:")
                    + parsed.show.toCaseFolded() + QLatin1Char('|') + group;
            if (!seriesIndex.contains(seriesId)) {
                ChannelData series;
                series.kind = ContentKind::Series;
                series.extId = seriesId;
                series.name = parsed.show;
                series.logo = channel.logo;
                series.categoryExtId = channel.categoryExtId;
                series.number = ++numberByKind[int(ContentKind::Series)];
                seriesIndex.insert(seriesId, int(content.channels.size()));
                content.channels.append(series);
            }
            EpisodeData episode;
            episode.seriesExtId = seriesId;
            episode.extId = channel.url;
            episode.url = channel.url;
            episode.image = channel.logo;
            episode.season = parsed.season > 0 ? parsed.season : 1;
            episode.episode = parsed.season > 0 ? parsed.episode : ++episodeCount[seriesId];
            episode.title = parsed.title.isEmpty() ? channel.name : parsed.title;
            content.episodes.append(std::move(episode));
        } else {
            int &running = numberByKind[int(channel.kind)];
            ++running;
            if (channel.number <= 0)
                channel.number = running;
            if (channel.kind == ContentKind::Movie)
                channel.year = yearFromName(channel.name);
            content.channels.append(std::move(channel));
        }
        havePending = false;
        pending = {};
        pendingGroup.clear();
    }

    return content;
}

} // namespace iptv
