#pragma once

#include <QByteArray>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QUrl>

class QThread;

// Plays streams that need particular HTTP headers (a User-Agent or Referer
// given in the playlist). The video player can't send headers of its own, so
// such streams are played through this small local relay instead: the player
// asks http://127.0.0.1:<port>/..., the relay fetches the real address with
// the headers and passes the data on. HLS playlists are rewritten so their
// segments, keys and variant playlists come through the relay too.
//
// The relay listens on the loopback interface only and runs on a thread of
// its own.
class StreamRelay : public QObject
{
    Q_OBJECT

public:
    struct Headers
    {
        QString userAgent;
        QString referrer;
        bool isEmpty() const { return userAgent.isEmpty() && referrer.isEmpty(); }
    };

    // The app-wide relay (started on first use).
    static StreamRelay *instance();

    explicit StreamRelay(QObject *parent = nullptr);
    ~StreamRelay() override;

    // The address to play `url` with `headers`; `url` itself when no headers are
    // needed or the relay couldn't start.
    QUrl wrap(const QUrl &url, const Headers &headers);

    // For tests: the port, or 0 when not listening.
    quint16 port() const;

    // Rewrites the addresses in an HLS playlist (fetched from `base`) to relay
    // addresses for `token`. Public for tests.
    QByteArray rewritePlaylist(const QByteArray &playlist, const QUrl &base,
                               const QString &token) const;
    QUrl relayUrl(const QString &token, const QUrl &target) const;

private:
    friend class StreamRelayWorker;
    friend class RelayConnection;
    bool ensureStarted();
    Headers headersFor(const QString &token) const;

    mutable QMutex m_mutex;
    QHash<QString, Headers> m_sessions; // token -> headers
    QHash<QString, QString> m_tokens;   // "agent\nreferrer" -> token
    QThread *m_thread = nullptr;
    QObject *m_worker = nullptr;
    quint16 m_port = 0;
    bool m_startFailed = false;
};
