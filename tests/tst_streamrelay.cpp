#include "StreamRelay.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>

// A tiny origin server: answers a few paths and remembers the headers it got.
class Origin : public QObject
{
public:
    Origin()
    {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] { serve(socket); });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        server.listen(QHostAddress::LocalHost, 0);
    }

    QString base() const { return QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()); }

    QTcpServer server;
    QList<QByteArray> paths;
    QHash<QByteArray, QByteArray> lastHeaders; // lower-case name -> value

private:
    void serve(QTcpSocket *socket)
    {
        QByteArray &buffer = m_buffers[socket];
        buffer += socket->readAll();
        const qsizetype end = buffer.indexOf("\r\n\r\n");
        if (end < 0)
            return;
        const QList<QByteArray> lines = buffer.left(end).split('\n');
        buffer.clear();
        const QByteArray path = lines.value(0).split(' ').value(1);
        paths << path;
        lastHeaders.clear();
        for (qsizetype i = 1; i < lines.size(); ++i) {
            const QByteArray line = lines.at(i).trimmed();
            const qsizetype colon = line.indexOf(':');
            if (colon > 0)
                lastHeaders.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
        }

        QByteArray status = "200 OK";
        QByteArray type = "video/mp2t";
        QByteArray body;
        QByteArray extra;
        if (path == "/live/index.m3u8") {
            type = "application/vnd.apple.mpegurl";
            body = "#EXTM3U\n#EXT-X-KEY:METHOD=AES-128,URI=\"key.bin\"\n#EXTINF:6,\nseg1.ts\n"
                   "#EXTINF:6,\n" + base().toUtf8() + "/other/seg2.ts?token=abc\n";
        } else if (path == "/redirect") {
            status = "302 Found";
            extra = "Location: " + base().toUtf8() + "/live/index.m3u8\r\n";
        } else if (path == "/movie.mp4" && lastHeaders.contains("range")) {
            status = "206 Partial Content";
            body = "6789";
            extra = "Content-Range: bytes 6-9/10\r\n";
        } else {
            body = "data for " + path;
        }
        socket->write("HTTP/1.1 " + status + "\r\nContent-Type: " + type + "\r\nContent-Length: "
                      + QByteArray::number(body.size()) + "\r\n" + extra
                      + "Connection: close\r\n\r\n" + body);
        socket->disconnectFromHost();
    }

    QHash<QTcpSocket *, QByteArray> m_buffers;
};

class TestStreamRelay : public QObject
{
    Q_OBJECT

private:
    struct Result
    {
        int status = 0;
        QByteArray body;
        QByteArray contentRange;
    };

    Result fetch(const QUrl &url, const QByteArray &range = {})
    {
        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::ManualRedirectPolicy);
        if (!range.isEmpty())
            request.setRawHeader("Range", range);
        QNetworkReply *reply = m_network.get(request);
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        loop.exec();
        Result result;
        result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.body = reply->readAll();
        result.contentRange = reply->rawHeader("Content-Range");
        reply->deleteLater();
        return result;
    }

    QNetworkAccessManager m_network;
    const StreamRelay::Headers m_headers{ QStringLiteral("Special Agent/1.0"),
                                          QStringLiteral("https://example.org/") };

private slots:
    void withoutHeadersUnchanged()
    {
        StreamRelay relay;
        const QUrl url(QStringLiteral("http://example.com/live.ts"));
        QCOMPARE(relay.wrap(url, {}), url);
        QCOMPARE(relay.port(), quint16(0)); // not even started
        // Not http(s): nothing to relay
        const QUrl rtmp(QStringLiteral("rtmp://example.com/live"));
        QCOMPARE(relay.wrap(rtmp, m_headers), rtmp);
    }

    void sendsTheHeaders()
    {
        Origin origin;
        StreamRelay relay;
        const QUrl wrapped = relay.wrap(QUrl(origin.base() + QStringLiteral("/live/1.ts")), m_headers);
        QCOMPARE(wrapped.host(), QStringLiteral("127.0.0.1"));
        QVERIFY(wrapped.path().endsWith(QLatin1String("/1.ts")));
        const Result result = fetch(wrapped);
        QCOMPARE(result.status, 200);
        QCOMPARE(result.body, QByteArray("data for /live/1.ts"));
        QCOMPARE(origin.lastHeaders.value("user-agent"), QByteArray("Special Agent/1.0"));
        QCOMPARE(origin.lastHeaders.value("referer"), QByteArray("https://example.org/"));
    }

    void passesRanges()
    {
        Origin origin;
        StreamRelay relay;
        const QUrl wrapped = relay.wrap(QUrl(origin.base() + QStringLiteral("/movie.mp4")), m_headers);
        const Result result = fetch(wrapped, "bytes=6-");
        QCOMPARE(result.status, 206);
        QCOMPARE(result.body, QByteArray("6789"));
        QCOMPARE(result.contentRange, QByteArray("bytes 6-9/10"));
        QCOMPARE(origin.lastHeaders.value("range"), QByteArray("bytes=6-"));
    }

    void rewritesPlaylists()
    {
        Origin origin;
        StreamRelay relay;
        const QUrl wrapped = relay.wrap(QUrl(origin.base() + QStringLiteral("/live/index.m3u8")),
                                        m_headers);
        const Result result = fetch(wrapped);
        QCOMPARE(result.status, 200);
        const QList<QByteArray> lines = result.body.split('\n');
        // Every address now points at the relay...
        QByteArray keyLine, seg1, seg2;
        for (const QByteArray &line : lines) {
            if (line.startsWith("#EXT-X-KEY"))
                keyLine = line;
            else if (line.endsWith("seg1.ts"))
                seg1 = line;
            else if (line.contains("seg2.ts"))
                seg2 = line;
        }
        const QByteArray relayBase = "http://127.0.0.1:" + QByteArray::number(relay.port()) + '/';
        QVERIFY2(keyLine.contains("URI=\"" + relayBase), keyLine.constData());
        QVERIFY2(seg1.startsWith(relayBase), seg1.constData());
        QVERIFY2(seg2.startsWith(relayBase), seg2.constData());

        // ...and fetching them goes to the right place, with the headers.
        Result segment = fetch(QUrl(QString::fromUtf8(seg1)));
        QCOMPARE(segment.body, QByteArray("data for /live/seg1.ts"));
        QCOMPARE(origin.lastHeaders.value("user-agent"), QByteArray("Special Agent/1.0"));
        segment = fetch(QUrl(QString::fromUtf8(seg2)));
        QCOMPARE(segment.body, QByteArray("data for /other/seg2.ts?token=abc"));
        const qsizetype start = keyLine.indexOf("URI=\"") + 5;
        segment = fetch(QUrl(QString::fromUtf8(keyLine.mid(start, keyLine.indexOf('"', start) - start))));
        QCOMPARE(segment.body, QByteArray("data for /live/key.bin"));
    }

    void followsRedirects()
    {
        // Relative segment addresses are resolved against where the playlist really is.
        Origin origin;
        StreamRelay relay;
        const QUrl wrapped = relay.wrap(QUrl(origin.base() + QStringLiteral("/redirect")), m_headers);
        const Result result = fetch(wrapped);
        QCOMPARE(result.status, 200);
        QByteArray seg1;
        for (const QByteArray &line : result.body.split('\n')) {
            if (line.endsWith("seg1.ts"))
                seg1 = line;
        }
        QVERIFY(!seg1.isEmpty());
        QCOMPARE(fetch(QUrl(QString::fromUtf8(seg1))).body, QByteArray("data for /live/seg1.ts"));
    }

    void refusesUnknownSessions()
    {
        Origin origin;
        StreamRelay relay;
        QUrl wrapped = relay.wrap(QUrl(origin.base() + QStringLiteral("/live/1.ts")), m_headers);
        QString path = wrapped.path();
        path.replace(1, path.indexOf(QLatin1Char('/'), 1) - 1, QStringLiteral("nosuchtoken"));
        wrapped.setPath(path);
        QCOMPARE(fetch(wrapped).status, 404);
        QVERIFY(origin.paths.isEmpty());
    }
};

QTEST_MAIN(TestStreamRelay)
#include "tst_streamrelay.moc"
