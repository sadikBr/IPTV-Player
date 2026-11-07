#include "GzipDevice.h"

#include <QByteArray>
#include <limits>
#include <zlib.h>

namespace iptv {

struct GzipDevice::Private
{
    QIODevice *source = nullptr;
    z_stream stream{};
    QByteArray input;
    bool initialized = false;
    bool finished = false;
};

GzipDevice::GzipDevice(QIODevice *source, QObject *parent)
    : QIODevice(parent), d(std::make_unique<Private>())
{
    d->source = source;
    d->input.resize(64 * 1024);
}

GzipDevice::~GzipDevice()
{
    close();
}

bool GzipDevice::isGzip(QIODevice *device)
{
    const QByteArray magic = device->peek(2);
    return magic.size() == 2 && uchar(magic.at(0)) == 0x1f && uchar(magic.at(1)) == 0x8b;
}

bool GzipDevice::open(OpenMode mode)
{
    if ((mode & QIODevice::WriteOnly) || !d->source || !d->source->isReadable())
        return false;
    d->stream = {};
    // 15 + 32: maximum window, detect gzip or zlib header automatically.
    if (inflateInit2(&d->stream, 15 + 32) != Z_OK)
        return false;
    d->initialized = true;
    d->finished = false;
    return QIODevice::open(mode);
}

void GzipDevice::close()
{
    if (d->initialized) {
        inflateEnd(&d->stream);
        d->initialized = false;
    }
    QIODevice::close();
}

bool GzipDevice::atEnd() const
{
    return d->finished && QIODevice::atEnd();
}

qint64 GzipDevice::readData(char *data, qint64 maxSize)
{
    if (d->finished || maxSize <= 0)
        return d->finished ? -1 : 0;

    d->stream.next_out = reinterpret_cast<Bytef *>(data);
    d->stream.avail_out = uInt(qMin<qint64>(maxSize, std::numeric_limits<uInt>::max()));

    while (d->stream.avail_out > 0) {
        if (d->stream.avail_in == 0) {
            const qint64 n = d->source->read(d->input.data(), d->input.size());
            if (n <= 0) {
                // Truncated input: return what we have.
                d->finished = true;
                break;
            }
            d->stream.next_in = reinterpret_cast<Bytef *>(d->input.data());
            d->stream.avail_in = uInt(n);
        }

        const int ret = inflate(&d->stream, Z_NO_FLUSH);
        if (ret == Z_STREAM_END) {
            // Concatenated gzip members are allowed; continue if more input follows.
            if (d->stream.avail_in == 0 && d->source->atEnd()) {
                d->finished = true;
                break;
            }
            inflateReset(&d->stream);
        } else if (ret != Z_OK && ret != Z_BUF_ERROR) {
            setErrorString(QStringLiteral("Decompression failed: %1")
                                   .arg(QString::fromLatin1(d->stream.msg ? d->stream.msg : "")));
            d->finished = true;
            break;
        }
    }

    const qint64 produced = maxSize - d->stream.avail_out;
    if (produced == 0 && d->finished)
        return -1;
    return produced;
}

} // namespace iptv
