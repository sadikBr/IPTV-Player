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

    bool havePending = false;
    Attributes pending;
    QString pendingGroup;

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
                havePending = true;
            } else if (line.startsWith(QLatin1String("#EXTGRP:"), Qt::CaseInsensitive)) {
                pendingGroup = line.mid(8).trimmed();
            } else if (line.startsWith(QLatin1String("#EXTM3U"), Qt::CaseInsensitive)) {
                if (content.epgUrl.isEmpty())
                    content.epgUrl = headerEpgUrl(line);
            }
            // Other directives (#EXTVLCOPT, #KODIPROP, ...) are ignored for now.
            continue;
        }

        // A URL line.
        ChannelData channel;
        channel.url = line;
        channel.extId = line;
        channel.kind = classifyUrl(line);

        QString group;
        if (havePending) {
            const auto &v = pending.values;
            channel.name = pending.title;
            if (channel.name.isEmpty())
                channel.name = v.value(QStringLiteral("tvg-name"));
            channel.logo = v.value(QStringLiteral("tvg-logo"));
            channel.epgId = v.value(QStringLiteral("tvg-id"));
            group = v.value(QStringLiteral("group-title"));
            if (group.isEmpty())
                group = pendingGroup;
            bool ok = false;
            const int chno = v.value(QStringLiteral("tvg-chno")).toInt(&ok);
            if (ok && chno > 0)
                channel.number = chno;
        }
        if (channel.name.isEmpty()) {
            const QUrl url(line);
            channel.name = url.fileName().isEmpty() ? line : url.fileName();
        }

        int &running = numberByKind[int(channel.kind)];
        ++running;
        if (channel.number <= 0)
            channel.number = running;

        if (!group.isEmpty()) {
            const QString key = kindToString(channel.kind) + QLatin1Char('|') + group;
            if (!categoryIndex.contains(key)) {
                categoryIndex.insert(key, int(content.categories.size()));
                content.categories.append({ group, group, channel.kind });
            }
            channel.categoryExtId = group;
        }

        content.channels.append(std::move(channel));
        havePending = false;
        pending = {};
        pendingGroup.clear();
    }

    return content;
}

} // namespace iptv
