#pragma once

#include "Types.h"

#include <QByteArray>
#include <QDateTime>
#include <QStringList>
#include <QUrl>

namespace iptv {

struct XtreamCredentials
{
    QUrl server; // scheme://host:port[/prefix], no trailing slash
    QString username;
    QString password;
};

struct XtreamAccount
{
    bool authenticated = false;
    QString status;           // "Active", "Expired", "Banned", ...
    QDateTime expiresAt;      // invalid if unlimited
    int maxConnections = 0;
    int activeConnections = 0;
    QStringList allowedFormats; // e.g. "ts", "m3u8"
    QString message;
};

// Xtream Codes "player_api.php" helpers. Pure functions: no networking here,
// so everything can be unit tested with recorded responses.
class XtreamApi
{
public:
    // "host:8080", "http://host:8080/", "http://host/player_api.php" -> "http://host:8080".
    // Returns an invalid URL if the input can't be used.
    static QUrl normalizeServer(const QString &input);

    // Extracts server, username and password from a provider M3U link such as
    // http://host:port/get.php?username=U&password=P&type=m3u_plus. Returns false if
    // the URL doesn't contain credentials.
    static bool parseCredentialsUrl(const QString &url, XtreamCredentials *out);

    static QUrl apiUrl(const XtreamCredentials &creds, const QString &action = {});
    static QUrl liveStreamUrl(const XtreamCredentials &creds, const QString &streamId,
                              const QString &format);

    static bool parseAccount(const QByteArray &json, XtreamAccount *out, QString *error);
    static bool parseCategories(const QByteArray &json, ContentKind kind,
                                QList<CategoryData> *out, QString *error);
    static bool parseLiveStreams(const QByteArray &json, QList<ChannelData> *out, QString *error);
};

} // namespace iptv
