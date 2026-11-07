#pragma once

#include <QIODevice>
#include <memory>

namespace iptv {

// Read-only QIODevice that decompresses gzip (or zlib) data from another
// device on the fly, so large .xml.gz guides never have to fit in memory.
class GzipDevice : public QIODevice
{
public:
    explicit GzipDevice(QIODevice *source, QObject *parent = nullptr);
    ~GzipDevice() override;

    bool open(OpenMode mode) override;
    void close() override;
    bool isSequential() const override { return true; }
    bool atEnd() const override;

    // True if the device's next bytes start with the gzip magic number.
    static bool isGzip(QIODevice *device);

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace iptv
