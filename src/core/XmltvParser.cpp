#include "XmltvParser.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QTimeZone>
#include <QXmlStreamReader>

namespace iptv {

namespace {

int digits(QStringView text, int pos, int count, bool *ok)
{
    int value = 0;
    for (int i = pos; i < pos + count; ++i) {
        const QChar c = text.at(i);
        if (!c.isDigit()) {
            *ok = false;
            return 0;
        }
        value = value * 10 + c.digitValue();
    }
    return value;
}

// Reads the text of the current element, skipping nested markup.
QString elementText(QXmlStreamReader &xml)
{
    return xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
}

} // namespace

qint64 XmltvParser::parseTime(QStringView text)
{
    text = text.trimmed();
    if (text.size() < 12)
        return -1;

    bool ok = true;
    const int year = digits(text, 0, 4, &ok);
    const int month = digits(text, 4, 2, &ok);
    const int day = digits(text, 6, 2, &ok);
    const int hour = digits(text, 8, 2, &ok);
    const int minute = digits(text, 10, 2, &ok);
    int second = 0;
    int pos = 12;
    if (text.size() >= 14 && text.at(12).isDigit()) {
        second = digits(text, 12, 2, &ok);
        pos = 14;
    }
    if (!ok)
        return -1;

    const QDate date(year, month, day);
    const QTime time(hour, minute, second);
    if (!date.isValid() || !time.isValid())
        return -1;
    qint64 secs = QDateTime(date, time, QTimeZone::utc()).toSecsSinceEpoch();

    // Optional offset: " +0100", "-0500", "+01:00"
    const QStringView rest = text.mid(pos).trimmed();
    if (rest.size() >= 5 && (rest.at(0) == QLatin1Char('+') || rest.at(0) == QLatin1Char('-'))) {
        const QString offset = rest.mid(1).toString().remove(QLatin1Char(':'));
        bool offsetOk = true;
        if (offset.size() >= 4) {
            const int oh = digits(offset, 0, 2, &offsetOk);
            const int om = digits(offset, 2, 2, &offsetOk);
            if (offsetOk) {
                const qint64 delta = qint64(oh) * 3600 + qint64(om) * 60;
                secs += rest.at(0) == QLatin1Char('+') ? -delta : delta;
            }
        }
    }
    return secs;
}

bool XmltvParser::parse(QIODevice *device, const ChannelHandler &onChannel,
                        const ProgramHandler &onProgram, QString *error)
{
    QXmlStreamReader xml(device);
    bool sawRoot = false;

    while (!xml.atEnd()) {
        if (xml.readNext() != QXmlStreamReader::StartElement)
            continue;

        const auto name = xml.name();
        if (name == QLatin1String("tv")) {
            sawRoot = true;
        } else if (name == QLatin1String("channel")) {
            EpgChannel channel;
            channel.id = xml.attributes().value(QLatin1String("id")).toString().trimmed();
            while (xml.readNextStartElement()) {
                if (xml.name() == QLatin1String("display-name")) {
                    const QString n = elementText(xml);
                    if (!n.isEmpty())
                        channel.names << n;
                } else {
                    xml.skipCurrentElement();
                }
            }
            if (!channel.id.isEmpty() && onChannel)
                onChannel(channel);
        } else if (name == QLatin1String("programme")) {
            const auto attrs = xml.attributes();
            EpgProgram program;
            program.channelId = attrs.value(QLatin1String("channel")).toString().trimmed();
            program.start = parseTime(attrs.value(QLatin1String("start")));
            program.stop = parseTime(attrs.value(QLatin1String("stop")));

            while (xml.readNextStartElement()) {
                const auto child = xml.name();
                if (child == QLatin1String("title") && program.title.isEmpty())
                    program.title = elementText(xml);
                else if (child == QLatin1String("sub-title") && program.subTitle.isEmpty())
                    program.subTitle = elementText(xml);
                else if (child == QLatin1String("desc") && program.description.isEmpty())
                    program.description = elementText(xml);
                else
                    xml.skipCurrentElement();
            }

            if (program.channelId.isEmpty() || program.start < 0)
                continue;
            if (program.stop <= program.start)
                program.stop = program.start + 30 * 60; // missing stop: assume 30 min
            if (onProgram && !onProgram(program))
                return false;
        }
    }

    if (xml.hasError() && xml.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
        if (error) {
            *error = QCoreApplication::translate("XmltvParser", "Invalid TV guide data: %1")
                             .arg(xml.errorString());
        }
        return false;
    }
    if (!sawRoot) {
        if (error)
            *error = QCoreApplication::translate("XmltvParser",
                                                 "This doesn't look like an XMLTV guide.");
        return false;
    }
    return true;
}

} // namespace iptv
