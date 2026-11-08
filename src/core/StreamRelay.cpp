#include "StreamRelay.h"

#include <QCoreApplication>
#include <QHostAddress>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

namespace {

constexpr qint64 kMaxPending = 1024 * 1024;      // bytes waiting for the player
constexpr qint64 kReadBuffer = 512 * 1024;       // bytes buffered from the server
constexpr qint64 kMaxPlaylist = 8 * 1024 * 1024; // larger "playlists" are passed on as-is

bool looksLikePlaylist(const QUrl &url, const QByteArray &contentType)
{
    const QByteArray type = contentType.toLower();
    if (type.contains("mpegurl"))
        return true;
    const QString path = url.path().toLower();
    return path.endsWith(QLatin1String(".m3u8")) || path.endsWith(QLatin1String(".m3u"));
}

QByteArray reasonPhrase(int status)
{
    switch (status) {
    case 200: return "OK";
    case 206: return "Partial Content";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 416: return "Range Not Satisfiable";
    case 502: return "Bad Gateway";
    default: return status >= 400 ? "Error" : "OK";
    }
}

} // namespace

// One request from the player: fetch the real address and pass the answer on.
class RelayConnection : public QObject
{
public:
    RelayConnection(StreamRelay *relay, QNetworkAccessManager *network, QTcpSocket *socket)
        : QObject(socket), m_relay(relay), m_network(network), m_socket(socket)
    {
        connect(socket, &QTcpSocket::readyRead, this, &RelayConnection::readRequest);
        connect(socket, &QTcpSocket::bytesWritten, this, &RelayConnection::pump);
        connect(socket, &QTcpSocket::disconnected, this, [this] {
            if (m_reply)
                m_reply->abort();
            m_socket->deleteLater();
        });
    }

private:
    void readRequest()
    {
        if (m_started)
            return;
        m_request += m_socket->readAll();
        const qsizetype end = m_request.indexOf("\r\n\r\n");
        if (end < 0) {
            if (m_request.size() > 64 * 1024)
                fail(400);
            return;
        }
        m_started = true;

        const QList<QByteArray> lines = m_request.left(end).split('\n');
        const QList<QByteArray> requestLine = lines.value(0).trimmed().split(' ');
        const QByteArray method = requestLine.value(0);
        m_head = method == "HEAD";
        if (method != "GET" && !m_head) {
            fail(405);
            return;
        }
        // /<token>/<base64url target>[/<name>]
        const QList<QByteArray> parts = requestLine.value(1).split('/');
        const QString token = QString::fromLatin1(parts.value(1));
        const QUrl target(QString::fromUtf8(QByteArray::fromBase64(
                parts.value(2), QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)));
        const StreamRelay::Headers headers = m_relay->headersFor(token);
        // Unknown token (every relay session has at least one header): refuse.
        if (headers.isEmpty() || !target.isValid()
            || (target.scheme() != QLatin1String("http") && target.scheme() != QLatin1String("https"))) {
            fail(404);
            return;
        }
        m_token = token;

        QNetworkRequest request(target);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        if (!headers.userAgent.isEmpty())
            request.setRawHeader("User-Agent", headers.userAgent.toUtf8());
        if (!headers.referrer.isEmpty())
            request.setRawHeader("Referer", headers.referrer.toUtf8());
        // Seeking: pass the player's Range on.
        for (qsizetype i = 1; i < lines.size(); ++i) {
            const QByteArray line = lines.at(i).trimmed();
            const qsizetype colon = line.indexOf(':');
            if (colon <= 0)
                continue;
            const QByteArray name = line.left(colon).trimmed().toLower();
            if (name == "range" || name == "icy-metadata")
                request.setRawHeader(line.left(colon).trimmed(), line.mid(colon + 1).trimmed());
        }

        m_reply = m_head ? m_network->head(request) : m_network->get(request);
        m_reply->setParent(this);
        m_reply->setReadBufferSize(kReadBuffer);
        connect(m_reply, &QNetworkReply::metaDataChanged, this, &RelayConnection::gotHeaders);
        connect(m_reply, &QNetworkReply::readyRead, this, &RelayConnection::pump);
        connect(m_reply, &QNetworkReply::finished, this, &RelayConnection::finished);
    }

    void gotHeaders()
    {
        if (m_sentHeaders || !m_reply)
            return;
        // Wait for the final answer, not a redirect.
        const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 300 && status < 400)
            return;
        m_playlist = !m_head && status == 200
                && looksLikePlaylist(m_reply->url(), m_reply->rawHeader("Content-Type"))
                && m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong() <= kMaxPlaylist;
        if (m_playlist)
            return; // sent whole, once rewritten
        QByteArray head = "HTTP/1.1 " + QByteArray::number(status ? status : 200) + ' '
                + reasonPhrase(status) + "\r\n";
        for (const char *name : { "Content-Type", "Content-Length", "Content-Range",
                                  "Accept-Ranges", "icy-metaint", "icy-name" }) {
            if (m_reply->hasRawHeader(name))
                head += QByteArray(name) + ": " + m_reply->rawHeader(name) + "\r\n";
        }
        head += "Connection: close\r\n\r\n";
        m_socket->write(head);
        m_sentHeaders = true;
        pump();
    }

    void pump()
    {
        if (!m_reply || !m_sentHeaders || m_playlist)
            return;
        // Only read more from the server when the player keeps up.
        while (m_reply->bytesAvailable() > 0 && m_socket->bytesToWrite() < kMaxPending)
            m_socket->write(m_reply->read(64 * 1024));
        if (m_done && m_reply->bytesAvailable() == 0 && m_socket->bytesToWrite() == 0)
            m_socket->disconnectFromHost();
    }

    void finished()
    {
        m_done = true;
        if (m_playlist) {
            const QByteArray body = m_relay->rewritePlaylist(m_reply->readAll(), m_reply->url(), m_token);
            m_socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/vnd.apple.mpegurl\r\n"
                            "Content-Length: " + QByteArray::number(body.size())
                            + "\r\nConnection: close\r\n\r\n" + body);
            m_socket->disconnectFromHost();
            return;
        }
        if (!m_sentHeaders) {
            const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            fail(status >= 400 ? status : 502);
            return;
        }
        pump();
    }

    void fail(int status)
    {
        m_socket->write("HTTP/1.1 " + QByteArray::number(status) + ' ' + reasonPhrase(status)
                        + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        m_socket->disconnectFromHost();
    }

    StreamRelay *m_relay;
    QNetworkAccessManager *m_network;
    QTcpSocket *m_socket;
    QPointer<QNetworkReply> m_reply;
    QByteArray m_request;
    QString m_token;
    bool m_started = false;
    bool m_head = false;
    bool m_sentHeaders = false;
    bool m_playlist = false;
    bool m_done = false;
};

// Lives on the relay's thread: the server and the network access.
class StreamRelayWorker : public QObject
{
public:
    explicit StreamRelayWorker(StreamRelay *relay) : m_relay(relay) {}

    quint16 listen()
    {
        m_network = new QNetworkAccessManager(this);
        m_server = new QTcpServer(this);
        connect(m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = m_server->nextPendingConnection())
                new RelayConnection(m_relay, m_network, socket);
        });
        if (!m_server->listen(QHostAddress::LocalHost, 0))
            return 0;
        return m_server->serverPort();
    }

private:
    StreamRelay *m_relay;
    QNetworkAccessManager *m_network = nullptr;
    QTcpServer *m_server = nullptr;
};

StreamRelay *StreamRelay::instance()
{
    // Owned by the application: its thread stops when the app quits.
    static StreamRelay *relay = new StreamRelay(QCoreApplication::instance());
    return relay;
}

StreamRelay::StreamRelay(QObject *parent) : QObject(parent) {}

StreamRelay::~StreamRelay()
{
    if (m_thread) {
        QMetaObject::invokeMethod(m_worker, &QObject::deleteLater);
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
    }
}

bool StreamRelay::ensureStarted()
{
    // Called with m_mutex held.
    if (m_port || m_startFailed)
        return m_port != 0;
    m_thread = new QThread();
    m_thread->setObjectName(QStringLiteral("StreamRelay"));
    auto *worker = new StreamRelayWorker(this);
    worker->moveToThread(m_thread);
    m_worker = worker;
    m_thread->start();
    quint16 port = 0;
    QMetaObject::invokeMethod(worker, [worker, &port] { port = worker->listen(); },
                              Qt::BlockingQueuedConnection);
    m_port = port;
    m_startFailed = port == 0;
    if (m_startFailed)
        qWarning("Stream relay: could not listen on the loopback interface");
    return m_port != 0;
}

quint16 StreamRelay::port() const
{
    QMutexLocker lock(&m_mutex);
    return m_port;
}

StreamRelay::Headers StreamRelay::headersFor(const QString &token) const
{
    QMutexLocker lock(&m_mutex);
    return m_sessions.value(token);
}

QUrl StreamRelay::relayUrl(const QString &token, const QUrl &target) const
{
    const QByteArray encoded = target.toEncoded().toBase64(QByteArray::Base64UrlEncoding
                                                           | QByteArray::OmitTrailingEquals);
    // The original file name last, so the player can tell the format from it.
    QString name = target.fileName();
    if (name.isEmpty())
        name = QStringLiteral("stream");
    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(QStringLiteral("127.0.0.1"));
    url.setPort(m_port);
    url.setPath(QLatin1Char('/') + token + QLatin1Char('/') + QString::fromLatin1(encoded)
                + QLatin1Char('/') + name);
    return url;
}

QUrl StreamRelay::wrap(const QUrl &url, const Headers &headers)
{
    if (headers.isEmpty() || !url.isValid()
        || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https")))
        return url;
    QMutexLocker lock(&m_mutex);
    if (!ensureStarted())
        return url;
    const QString key = headers.userAgent + QLatin1Char('\n') + headers.referrer;
    QString token = m_tokens.value(key);
    if (token.isEmpty()) {
        token = QString::number(QRandomGenerator::system()->generate64(), 36);
        m_tokens.insert(key, token);
        m_sessions.insert(token, headers);
    }
    return relayUrl(token, url);
}

QByteArray StreamRelay::rewritePlaylist(const QByteArray &playlist, const QUrl &base,
                                        const QString &token) const
{
    static const QRegularExpression uriAttribute(QStringLiteral("URI=\"([^\"]*)\""));
    const QString text = QString::fromUtf8(playlist);
    QStringList out;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            out << line;
        } else if (trimmed.startsWith(QLatin1Char('#'))) {
            // Keys, init sections, other renditions: URI="..."
            QString rewritten;
            qsizetype last = 0;
            auto it = uriAttribute.globalMatch(line);
            while (it.hasNext()) {
                const auto match = it.next();
                rewritten += line.mid(last, match.capturedStart(1) - last);
                rewritten += relayUrl(token, base.resolved(QUrl(match.captured(1)))).toString();
                last = match.capturedEnd(1);
            }
            rewritten += line.mid(last);
            out << rewritten;
        } else {
            out << relayUrl(token, base.resolved(QUrl(trimmed))).toString();
        }
    }
    return out.join(QLatin1Char('\n')).toUtf8();
}
