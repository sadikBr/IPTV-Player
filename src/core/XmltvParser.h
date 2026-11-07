#pragma once

#include <QIODevice>
#include <QString>
#include <QStringList>
#include <functional>

namespace iptv {

struct EpgChannel
{
    QString id;
    QStringList names; // <display-name> entries
};

struct EpgProgram
{
    QString channelId;
    qint64 start = 0; // seconds since epoch, UTC
    qint64 stop = 0;
    QString title;
    QString subTitle;
    QString description;
};

// Streaming XMLTV parser: reads <channel> and <programme> elements one by
// one and hands them to callbacks, so files of any size use little memory.
class XmltvParser
{
public:
    using ChannelHandler = std::function<void(const EpgChannel &)>;
    // Return false to stop parsing (e.g. on a database error).
    using ProgramHandler = std::function<bool(const EpgProgram &)>;

    static bool parse(QIODevice *device, const ChannelHandler &onChannel,
                      const ProgramHandler &onProgram, QString *error);

    // "20261107180000 +0100" (offset optional, UTC if missing) -> epoch seconds, or -1.
    static qint64 parseTime(QStringView text);
};

} // namespace iptv
